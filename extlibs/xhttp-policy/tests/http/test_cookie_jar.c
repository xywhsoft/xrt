#include "../test.h"



/* 测试公共后缀策略识别 com 和 co.uk。 */
static bool testCookieJarPublicSuffix(ptr pContext, xstrview Domain)
{
	(void)pContext;
	return ((Domain.Size == 3u) &&
		(memcmp(Domain.Data, "com", 3u) == 0)) ||
		((Domain.Size == 5u) &&
		 (memcmp(Domain.Data, "co.uk", 5u) == 0));
}



/* 使用固定时间和 HTTP 来源存储字段。 */
static xcookiestorestatus testCookieJarStore(
	xcookiejar* pJar,
	xstrview URL,
	xstrview PartitionKey,
	xstrview Field,
	xtime iNow,
	xcookiereject* pReject
)
{
	xcookiestorecontext Context;

	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_HAS_NOW |
		XCOOKIE_STORE_SAME_SITE;
	Context.URL = URL;
	Context.PartitionKey = PartitionKey;
	Context.Now = iNow;
	return xrtCookieJarStore(pJar, &Context, Field, pReject);
}



/* 验证借用文本等于期望的零结尾文本。 */
static bool testCookieJarText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证配置支持未对齐存储，并在读取或写入前拒绝回绕范围。 */
static void testCookieJarConfigStorage(void)
{
	uint8 Storage[sizeof(xcookiejarconfig) + 2u];
	xcookiejarconfig Config;
	xcookiejar* pJar;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtCookieJarConfigInit(
		(xcookiejarconfig*)(void*)(Storage + 1u)
	);
	memcpy(&Config, Storage + 1u, sizeof(Config));
	testRequire((Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Config.InitialCookies == 16u) &&
		(Config.MaxCookies == 3000u) &&
		(Config.MaxCookiesPerDomain == 180u),
		"cookie jar config init did not support unaligned storage");

	Config.MaxCookies = 32u;
	Config.MaxCookiesPerDomain = 16u;
	memcpy(Storage + 1u, &Config, sizeof(Config));
	pJar = xrtCookieJarCreate(
		(const xcookiejarconfig*)(const void*)(Storage + 1u)
	);
	testRequire(pJar != NULL,
		"cookie jar create did not snapshot unaligned config");
	xrtCookieJarRelease(pJar);

	xrtClearError();
	xrtCookieJarConfigInit((xcookiejarconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"cookie jar config init accepted a wrapping range");
	xrtClearError();
	testRequire(xrtCookieJarCreate(
		(const xcookiejarconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"cookie jar create accepted a wrapping config range");
	xrtClearError();
}



/* 验证基础存储、默认路径、发送顺序、删除和快照所有权。 */
static void testCookieJarLifecycle(void)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;
	xcookiesnapshot* pSnapshot;
	const xcookieinfo* pInfo;
	xcookiereject Reject;
	char Output[128];
	size_t iSize;
	const xtime iNow = INT64_C(1000000000000);

	xrtCookieJarConfigInit(&Config);
	Config.IsPublicSuffix = testCookieJarPublicSuffix;
	pJar = xrtCookieJarCreate(&Config);
	testRequire(pJar != NULL, "cookie jar create failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://api.example.com/account/login"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("narrow=1; Secure; HttpOnly"), iNow, &Reject
	) == XCOOKIE_STORE_STORED, "cookie jar default path store failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://api.example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL(
			"broad=2; Domain=example.com; Path=/; Secure"
		), iNow + 1, &Reject
	) == XCOOKIE_STORE_STORED, "cookie jar domain store failed");
	testRequire(xrtCookieJarCount(pJar) == 2,
		"cookie jar count mismatch");
	testRequire(xrtCookieJarWriteUrl(
		pJar, XRT_STR_LITERAL("https://api.example.com/account/view"),
		Output, sizeof(Output), &iSize
	) && (iSize == strlen("narrow=1; broad=2")) &&
		(memcmp(Output, "narrow=1; broad=2", iSize) == 0),
		"cookie jar path order mismatch");
	testRequire(xrtCookieJarWriteUrl(
		pJar, XRT_STR_LITERAL("http://api.example.com/account/view"),
		Output, sizeof(Output), &iSize
	) && (iSize == 0), "cookie jar sent Secure over HTTP");

	pSnapshot = xrtCookieJarSnapshot(pJar, iNow + 2);
	testRequire((pSnapshot != NULL) &&
		(xrtCookieSnapshotCount(pSnapshot) == 2),
		"cookie jar snapshot failed");
	pInfo = xrtCookieSnapshotAt(pSnapshot, 0);
	testRequire((pInfo != NULL) && testCookieJarText(pInfo->Name, "narrow") &&
		testCookieJarText(pInfo->Path, "/account") &&
		((pInfo->Flags & (XCOOKIE_INFO_HOST_ONLY |
		 XCOOKIE_INFO_SECURE | XCOOKIE_INFO_HTTP_ONLY)) ==
		 (XCOOKIE_INFO_HOST_ONLY | XCOOKIE_INFO_SECURE |
		  XCOOKIE_INFO_HTTP_ONLY)),
		"cookie jar snapshot metadata mismatch");
	xrtCookieJarClear(pJar);
	testRequire((xrtCookieJarCount(pJar) == 0) &&
		testCookieJarText(pInfo->Name, "narrow"),
		"cookie jar snapshot borrowed mutable jar storage");
	xrtCookieSnapshotDestroy(pSnapshot);

	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://api.example.com/"),
		(xstrview){ NULL, 0 }, XRT_STR_LITERAL("sid=1; Path=/"),
		iNow, &Reject
	) == XCOOKIE_STORE_STORED, "cookie jar deletion fixture failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://api.example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=gone; Path=/; Max-Age=0"),
		iNow + 1, &Reject
	) == XCOOKIE_STORE_REMOVED && (xrtCookieJarCount(pJar) == 0),
		"cookie jar Max-Age deletion failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://api.example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=gone; Path=/; Max-Age=-1"),
		iNow + 2, &Reject
	) == XCOOKIE_STORE_IGNORED,
		"cookie jar missing deletion was not ignored");
	xrtCookieJarRelease(pJar);
}



/* 验证同主键覆盖保留创建时间且持久寿命钳制到 400 天。 */
static void testCookieJarReplacement(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiesnapshot* pSnapshot;
	const xcookieinfo* pInfo;
	xtime iCreated;
	const xtime iNow = INT64_C(2000000000000);

	testRequire(pJar != NULL, "cookie jar replacement create failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 }, XRT_STR_LITERAL("sid=old; Path=/"),
		iNow, NULL
	) == XCOOKIE_STORE_STORED, "cookie jar initial replace store failed");
	pSnapshot = xrtCookieJarSnapshot(pJar, iNow);
	pInfo = xrtCookieSnapshotAt(pSnapshot, 0);
	testRequire(pInfo != NULL, "cookie jar initial snapshot failed");
	iCreated = pInfo->Created;
	xrtCookieSnapshotDestroy(pSnapshot);
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=new; Path=/; Max-Age=999999999"),
		iNow + XRT_TIME_SECOND, NULL
	) == XCOOKIE_STORE_STORED, "cookie jar replacement failed");
	pSnapshot = xrtCookieJarSnapshot(pJar, iNow + XRT_TIME_SECOND);
	pInfo = xrtCookieSnapshotAt(pSnapshot, 0);
	testRequire((pInfo != NULL) && testCookieJarText(pInfo->Value, "new") &&
		(pInfo->Created == iCreated) &&
		(pInfo->Expires == (iNow + XRT_TIME_SECOND +
		 XCOOKIE_JAR_MAX_LIFETIME)),
		"cookie jar replacement metadata or lifetime clamp failed");
	xrtCookieSnapshotDestroy(pSnapshot);
	xrtCookieJarRelease(pJar);
}



/* 验证 HostOnly 位属于主键，覆盖和删除不会串到 Domain Cookie。 */
static void testCookieJarHostOnlyKey(void)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;
	xcookiesnapshot* pSnapshot;
	const xcookieinfo* pFirst;
	const xcookieinfo* pSecond;
	const xtime iNow = INT64_C(2500000000000);

	xrtCookieJarConfigInit(&Config);
	Config.IsPublicSuffix = testCookieJarPublicSuffix;
	pJar = xrtCookieJarCreate(&Config);
	testRequire(pJar != NULL, "cookie HostOnly key create failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 }, XRT_STR_LITERAL("sid=host; Path=/"),
		iNow, NULL
	) == XCOOKIE_STORE_STORED, "cookie HostOnly key fixture failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=domain; Domain=example.com; Path=/"),
		iNow + 1, NULL
	) == XCOOKIE_STORE_STORED && (xrtCookieJarCount(pJar) == 2u),
		"cookie Domain entry replaced the HostOnly entry");
	pSnapshot = xrtCookieJarSnapshot(pJar, iNow + 2);
	pFirst = xrtCookieSnapshotAt(pSnapshot, 0);
	pSecond = xrtCookieSnapshotAt(pSnapshot, 1);
	testRequire((pSnapshot != NULL) && (pFirst != NULL) &&
		(pSecond != NULL) &&
		(((pFirst->Flags ^ pSecond->Flags) &
		 XCOOKIE_INFO_HOST_ONLY) != 0),
		"cookie HostOnly key snapshot did not retain both variants");
	xrtCookieSnapshotDestroy(pSnapshot);
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("sid=gone; Path=/; Max-Age=0"),
		iNow + 3, NULL
	) == XCOOKIE_STORE_REMOVED && (xrtCookieJarCount(pJar) == 1u),
		"cookie HostOnly deletion removed the Domain entry");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL(
			"sid=gone; Domain=example.com; Path=/; Max-Age=0"
		), iNow + 4, NULL
	) == XCOOKIE_STORE_REMOVED && (xrtCookieJarCount(pJar) == 0),
		"cookie Domain deletion missed its exact key");
	xrtCookieJarRelease(pJar);
}



/* 验证容量压力优先淘汰低 Priority 条目。 */
static void testCookieJarEviction(void)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;
	xcookiesnapshot* pSnapshot;
	const xcookieinfo* pFirst;
	const xcookieinfo* pSecond;
	const xtime iNow = INT64_C(6000000000000);

	xrtCookieJarConfigInit(&Config);
	Config.InitialCookies = 2;
	Config.MaxCookies = 2;
	Config.MaxCookiesPerDomain = 2;
	pJar = xrtCookieJarCreate(&Config);
	testRequire(pJar != NULL, "cookie eviction jar create failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("low=1; Priority=Low"), iNow, NULL
	) == XCOOKIE_STORE_STORED, "low priority cookie store failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("high=1; Priority=High"), iNow + 1, NULL
	) == XCOOKIE_STORE_STORED, "high priority cookie store failed");
	testRequire(testCookieJarStore(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("new=1"), iNow + 2, NULL
	) == XCOOKIE_STORE_STORED, "capacity replacement store failed");
	pSnapshot = xrtCookieJarSnapshot(pJar, iNow + 3);
	pFirst = xrtCookieSnapshotAt(pSnapshot, 0);
	pSecond = xrtCookieSnapshotAt(pSnapshot, 1);
	testRequire((pSnapshot != NULL) &&
		(xrtCookieSnapshotCount(pSnapshot) == 2u) &&
		(pFirst != NULL) && (pSecond != NULL) &&
		!testCookieJarText(pFirst->Name, "low") &&
		!testCookieJarText(pSecond->Name, "low"),
		"cookie priority eviction retained the low entry");
	xrtCookieSnapshotDestroy(pSnapshot);
	xrtCookieJarRelease(pJar);
}



/* 执行 CookieJar 生命周期和所有权测试。 */
int main(void)
{
	testCookieJarConfigStorage();
	testCookieJarLifecycle();
	testCookieJarReplacement();
	testCookieJarHostOnlyKey();
	testCookieJarEviction();
	printf("[PASS] cookie_jar\n");
	return 0;
}

