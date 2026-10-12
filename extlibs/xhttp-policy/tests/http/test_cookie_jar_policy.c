#include "../test.h"



/* 使用完整接收上下文写入 Cookie。 */
static xcookiestorestatus testCookiePolicyStore(
	xcookiejar* pJar,
	xstrview URL,
	xstrview Key,
	xstrview Field,
	xtime iNow,
	uint32 iFlags,
	xcookiereject* pReject
)
{
	xcookiestorecontext Context;

	memset(&Context, 0, sizeof(Context));
	Context.Flags = iFlags | XCOOKIE_STORE_HAS_NOW;
	Context.URL = URL;
	Context.PartitionKey = Key;
	Context.Now = iNow;
	return xrtCookieJarStore(pJar, &Context, Field, pReject);
}



/* 使用完整请求上下文构建字段值。 */
static str testCookiePolicyBuild(
	xcookiejar* pJar,
	xstrview URL,
	xstrview Key,
	xtime iNow,
	uint32 iFlags,
	size_t* pSize
)
{
	xcookierequestcontext Context;

	memset(&Context, 0, sizeof(Context));
	Context.Flags = iFlags | XCOOKIE_REQUEST_HAS_NOW;
	Context.URL = URL;
	Context.PartitionKey = Key;
	Context.Now = iNow;
	return xrtCookieJarBuild(pJar, &Context, pSize);
}



/* 验证输出包含名称并且不误匹配名称前后缀。 */
static bool testCookiePolicyHas(str sText, size_t iSize, cstr sName)
{
	size_t iName = strlen(sName);
	size_t i = 0;

	while ( i < iSize ) {
		size_t iEnd = i;

		while ( (iEnd < iSize) && (sText[iEnd] != ';') ) {
			iEnd++;
		}
		if ( ((iEnd - i) > iName) &&
			(memcmp(sText + i, sName, iName) == 0) &&
			(sText[i + iName] == '=') ) {
			return true;
		}
		i = (iEnd < iSize) ? (iEnd + 2u) : iEnd;
	}
	return false;
}



/* 验证 SameSite、HttpOnly 和分区键都由显式请求上下文控制。 */
static void testCookiePolicyRequestContext(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	str sText;
	size_t iSize;
	const xtime iNow = INT64_C(3000000000000);
	const xstrview URL = XRT_STR_LITERAL("https://example.com/");

	testRequire(pJar != NULL, "cookie policy create failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("strict=1; SameSite=Strict; Secure"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, NULL
	) == XCOOKIE_STORE_STORED, "strict cookie store failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("lax=1; SameSite=Lax; Secure"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, NULL
	) == XCOOKIE_STORE_STORED, "lax cookie store failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("default=1; Secure"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, NULL
	) == XCOOKIE_STORE_STORED, "default cookie store failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("none=1; SameSite=None; Secure; HttpOnly"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, NULL
	) == XCOOKIE_STORE_STORED, "none cookie store failed");

	sText = testCookiePolicyBuild(
		pJar, URL, (xstrview){ NULL, 0 }, iNow + XRT_TIME_SECOND,
		XCOOKIE_REQUEST_TOP_LEVEL | XCOOKIE_REQUEST_SAFE_METHOD |
		XCOOKIE_REQUEST_HTTP_API, &iSize
	);
	testRequire((sText != NULL) && !testCookiePolicyHas(sText, iSize, "strict") &&
		testCookiePolicyHas(sText, iSize, "lax") &&
		testCookiePolicyHas(sText, iSize, "default") &&
		testCookiePolicyHas(sText, iSize, "none"),
		"cookie cross-site safe request policy mismatch");
	xrtFree(sText);

	sText = testCookiePolicyBuild(
		pJar, URL, (xstrview){ NULL, 0 }, iNow + XRT_TIME_MINUTE,
		XCOOKIE_REQUEST_TOP_LEVEL, &iSize
	);
	testRequire((sText != NULL) && testCookiePolicyHas(sText, iSize, "default") &&
		!testCookiePolicyHas(sText, iSize, "lax") &&
		!testCookiePolicyHas(sText, iSize, "none"),
		"cookie Lax-allowing-unsafe or HttpOnly policy mismatch");
	xrtFree(sText);

	sText = testCookiePolicyBuild(
		pJar, URL, (xstrview){ NULL, 0 }, iNow + (3 * XRT_TIME_MINUTE),
		XCOOKIE_REQUEST_TOP_LEVEL | XCOOKIE_REQUEST_HTTP_API, &iSize
	);
	testRequire((sText != NULL) && !testCookiePolicyHas(sText, iSize, "default") &&
		testCookiePolicyHas(sText, iSize, "none"),
		"cookie default compatibility window did not expire");
	xrtFree(sText);
	xrtCookieJarRelease(pJar);
}



/* 验证跨站子资源不能创建非 None Cookie，顶层导航不受此限制。 */
static void testCookiePolicyStoreContext(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiereject Reject;
	const xtime iNow = INT64_C(3500000000000);
	const xstrview URL = XRT_STR_LITERAL("https://example.com/");

	testRequire(pJar != NULL, "cookie store context create failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("default=1"), iNow,
		XCOOKIE_STORE_HTTP_API, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_SAME_SITE),
		"cross-site subresource created a Default cookie");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("strict=1; SameSite=Strict"), iNow + 1,
		XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_TOP_LEVEL, &Reject
	) == XCOOKIE_STORE_STORED,
		"cross-site top-level navigation could not create Strict cookie");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("none=1; SameSite=None; Secure"), iNow + 2,
		XCOOKIE_STORE_HTTP_API, &Reject
	) == XCOOKIE_STORE_STORED,
		"cross-site subresource could not create SameSite=None cookie");
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("lax=1; SameSite=Lax"), iNow + 3,
		XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED,
		"same-site response could not create a Lax cookie");
	xrtCookieJarRelease(pJar);
}



/* 验证 Partitioned Cookie 具有真实分区键隔离。 */
static void testCookiePolicyPartition(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiereject Reject;
	str sText;
	size_t iSize;
	const xtime iNow = INT64_C(4000000000000);
	const xstrview URL = XRT_STR_LITERAL("https://cdn.example.com/");

	testRequire(pJar != NULL, "partition cookie jar create failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, XRT_STR_LITERAL("site-a"),
		XRT_STR_LITERAL("part=a; Secure; Partitioned"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED, "partitioned cookie store failed");
	testRequire(testCookiePolicyStore(
		pJar, URL, XRT_STR_LITERAL("site-b"),
		XRT_STR_LITERAL("part=b; Secure; Partitioned"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED, "second partitioned cookie store failed");
	testRequire(xrtCookieJarCount(pJar) == 2,
		"partition key was omitted from storage key");
	sText = testCookiePolicyBuild(
		pJar, URL, XRT_STR_LITERAL("site-a"), iNow,
		XCOOKIE_REQUEST_HTTP_API | XCOOKIE_REQUEST_SAME_SITE, &iSize
	);
	testRequire((sText != NULL) && (iSize == 6u) &&
		(memcmp(sText, "part=a", 6u) == 0),
		"partitioned cookie escaped its partition");
	xrtFree(sText);
	testRequire(testCookiePolicyStore(
		pJar, URL, (xstrview){ NULL, 0 },
		XRT_STR_LITERAL("bad=1; Secure; Partitioned"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_PARTITION),
		"partitioned cookie accepted a missing partition key");
	xrtCookieJarRelease(pJar);
}



/* 验证安全前缀、公共后缀和 Secure 覆盖保护。 */
static void testCookiePolicyStorage(void)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;
	xcookiereject Reject;
	const xtime iNow = INT64_C(5000000000000);

	xrtCookieJarConfigInit(&Config);
	pJar = xrtCookieJarCreate(&Config);
	testRequire(pJar != NULL, "cookie storage policy create failed");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://sub.example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=1; Domain=example.com"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_PUBLIC_SUFFIX),
		"cookie domain sharing bypassed missing PSL policy");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=1; Domain=example.com"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED,
		"same-host Domain cookie was not safely downgraded");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("__Host-token=1; Secure; Path=/"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED,
		"valid __Host cookie was rejected");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("__Host-bad=1; Secure"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_PREFIX),
		"invalid __Host cookie was accepted");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 }, XRT_STR_LITERAL("__sEcUrE-nameless"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_PREFIX),
		"nameless __Secure value bypassed prefix policy");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("__Host-nameless; Secure"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_PREFIX),
		"nameless __Host value bypassed explicit Path policy");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("__Host-nameless; Secure; Path=/"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED,
		"valid nameless __Host value was rejected");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("guard=secure; Secure; Path=/"),
		iNow, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_STORED,
		"secure overwrite fixture failed");
	testRequire(testCookiePolicyStore(
		pJar, XRT_STR_LITERAL("http://example.com/"),
		(xstrview){ NULL, 0 }, XRT_STR_LITERAL("guard=plain; Path=/"),
		iNow + 1, XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_SECURE_OVERWRITE),
		"insecure origin overlaid a Secure cookie");
	xrtCookieJarRelease(pJar);
}



/* 验证直接 Set 与输出参数的别名和非法结构边界。 */
static void testCookiePolicyArguments(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiestorecontext Store;
	xcookiestorecontext StoreBefore;
	xcookierequestcontext Request;
	xsetcookie Cookie;
	xcookiereject Reject;
	char Output[32];
	char Before[32];

	testRequire(pJar != NULL, "cookie argument jar create failed");
	memset(&Store, 0, sizeof(Store));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Store.URL = XRT_STR_LITERAL("https://example.com/");
	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = XRT_STR_LITERAL("bad=name");
	Cookie.Value = XRT_STR_LITERAL("value");
	testRequire(xrtCookieJarSet(
		pJar, &Store, &Cookie, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_SYNTAX),
		"cookie direct Set accepted an impossible parsed name");
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.SameSite = (xcookiesamesite)99;
	testRequire(xrtCookieJarSet(
		pJar, &Store, &Cookie, &Reject
	) == XCOOKIE_STORE_REJECTED &&
		(Reject == XCOOKIE_REJECT_SYNTAX),
		"cookie direct Set accepted an invalid enum");
	testRequire(xrtCookieJarStoreUrl(
		pJar, Store.URL, XRT_STR_LITERAL("sid=1"), NULL
	) == XCOOKIE_STORE_STORED, "cookie alias fixture store failed");
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE;
	Request.URL = Store.URL;
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtCookieJarWrite(
		pJar, &Request, Output, sizeof(Output), (size_t*)Output
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"cookie output accepted a size alias");
	StoreBefore = Store;
	testRequire(xrtCookieJarStore(
		pJar, &Store, XRT_STR_LITERAL("sid=2"),
		(xcookiereject*)&Store
	) == XCOOKIE_STORE_ERROR &&
		(memcmp(&Store, &StoreBefore, sizeof(Store)) == 0),
		"cookie Store modified an aliased result");
	xrtCookieJarRelease(pJar);
}



/* 执行 CookieJar 请求与存储策略测试。 */
int main(void)
{
	testCookiePolicyRequestContext();
	testCookiePolicyStoreContext();
	testCookiePolicyPartition();
	testCookiePolicyStorage();
	testCookiePolicyArguments();
	printf("[PASS] cookie_jar_policy\n");
	return 0;
}

