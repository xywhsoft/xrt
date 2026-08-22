#include "../test.h"



/* 按字节比较借用文本。 */
static bool testSetCookieText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证宽松接收解析、重复属性的最后有效值和扩展属性保留。 */
static void testSetCookieParseCompat(void)
{
	xsetcookie Cookie;
	xcookieattribute Attribute;
	size_t iOffset = 0;
	size_t iAttributes = 0;

	testRequire(xrtSetCookieParse(XRT_STR_LITERAL(
		" sid = abc123 ; Path=/old; Path=/; Domain=.Example.COM; "
		"HttpOnly=x; Secure; SameSite=unknown; SameSite=Lax; "
		"Partitioned; Priority=High; SameParty"
	), &Cookie), "set-cookie compatible parse failed");
	testRequire(testSetCookieText(Cookie.Name, "sid") &&
		testSetCookieText(Cookie.Value, "abc123") &&
		testSetCookieText(Cookie.Path, "/") &&
		testSetCookieText(Cookie.Domain, "Example.COM"),
		"set-cookie borrowed fields mismatch");
	testRequire((Cookie.Flags & (XSET_COOKIE_HAS_PATH |
		XSET_COOKIE_HAS_DOMAIN | XSET_COOKIE_HTTP_ONLY |
		XSET_COOKIE_SECURE | XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_PARTITIONED | XSET_COOKIE_HAS_PRIORITY)) ==
		(XSET_COOKIE_HAS_PATH | XSET_COOKIE_HAS_DOMAIN |
		XSET_COOKIE_HTTP_ONLY | XSET_COOKIE_SECURE |
		XSET_COOKIE_HAS_SAME_SITE | XSET_COOKIE_PARTITIONED |
		XSET_COOKIE_HAS_PRIORITY),
		"set-cookie known flags mismatch");
	testRequire((Cookie.SameSite == XCOOKIE_SAME_SITE_LAX) &&
		(Cookie.Priority == XCOOKIE_PRIORITY_HIGH),
		"set-cookie enum attribute mismatch");
	while ( xrtSetCookieAttributeNext(
		Cookie.RawAttributes, &iOffset, &Attribute
	) == XCOOKIE_ATTRIBUTE_ITEM ) {
		iAttributes++;
		if ( testSetCookieText(Attribute.Name, "SameParty") ) {
			testRequire((Attribute.Flags &
				XCOOKIE_ATTRIBUTE_HAS_VALUE) == 0,
				"set-cookie boolean extension gained a value");
		}
	}
	testRequire(iAttributes == 10,
		"set-cookie raw attribute count mismatch");

	testRequire(xrtSetCookieParse(
		XRT_STR_LITERAL("nameless-value"), &Cookie
	) && (Cookie.Name.Size == 0) &&
		testSetCookieText(Cookie.Value, "nameless-value"),
		"set-cookie user-agent nameless form failed");
	testRequire(xrtSetCookieParse(
		XRT_STR_LITERAL("a=b,c\\d"), &Cookie
	) && testSetCookieText(Cookie.Value, "b,c\\d"),
		"set-cookie compatible value was made artificially strict");
	testRequire(xrtSetCookieParse(
		XRT_STR_LITERAL("sid=x; Max-Age=bad; SameSite=other"), &Cookie
	) && ((Cookie.Flags & XSET_COOKIE_HAS_MAX_AGE) == 0) &&
		((Cookie.Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		(Cookie.SameSite == XCOOKIE_SAME_SITE_DEFAULT),
		"set-cookie invalid known attributes were not ignored correctly");

	iOffset = 0;
	testRequire(xrtSetCookieAttributeNext(
		XRT_STR_LITERAL("Path=/; Bad=\r"), &iOffset, &Attribute
	) == XCOOKIE_ATTRIBUTE_ITEM,
		"set-cookie attribute iterator did not return the valid prefix");
	testRequire(xrtSetCookieAttributeNext(
		XRT_STR_LITERAL("Path=/; Bad=\r"), &iOffset, &Attribute
	) == XCOOKIE_ATTRIBUTE_ERROR && (iOffset == 7),
		"set-cookie attribute iterator did not reject the current CTL segment");
}



/* 验证 cookie-date 的宽松历史格式、后缀规则和日期有效性。 */
static void testCookieDate(void)
{
	xtime iExpected;
	xtime iParsed;

	testRequire(xrtDateTime(
		2021, 6, 9, 10, 18, 14, 0, &iExpected
	), "cookie date fixture creation failed");
	testRequire(xrtCookieDateParse(
		XRT_STR_LITERAL("Wed, 09 Jun 2021 10:18:14 GMT"), &iParsed
	) && (iParsed == iExpected), "cookie IMF date parse failed");
	testRequire(xrtCookieDateParse(
		XRT_STR_LITERAL("Wednesday, 09-Jun-21 10:18:14 GMT"), &iParsed
	) && (iParsed == iExpected), "cookie RFC850 date parse failed");
	testRequire(xrtCookieDateParse(
		XRT_STR_LITERAL("Wed Jun 09 10:18:14 2021-extra"), &iParsed
	) && (iParsed == iExpected), "cookie date token suffix parse failed");
	testRequire(!xrtCookieDateParse(
		XRT_STR_LITERAL("Wed, 31 Feb 2021 10:18:14 GMT"), &iParsed
	), "cookie date accepted an impossible day");
	testRequire(!xrtCookieDateParse(
		XRT_STR_LITERAL("Wed, 09 Jun 1500 10:18:14 GMT"), &iParsed
	), "cookie date accepted a pre-1601 year");
}



/* 验证严格服务器语法与宽松接收语义不会混为一个布尔口径。 */
static void testSetCookieStrictValidation(void)
{
	testRequire(xrtSetCookieValidate(XRT_STR_LITERAL(
		"sid=abc123; Path=/; Domain=example.com; "
		"Expires=Wed, 09 Jun 2021 10:18:14 GMT; Max-Age=60; "
		"SameSite=Lax; Secure; HttpOnly; Partitioned; "
		"Priority=High; Vendor=on"
	)), "strict set-cookie rejected a valid server field");
	testRequire(xrtSetCookieValidate(
		XRT_STR_LITERAL("plain=value")
	), "strict set-cookie rejected a field without attributes");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("nameless")
	), "strict set-cookie accepted a nameless cookie");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("bad name=value")
	), "strict set-cookie accepted a non-token name");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Max-Age=0")
	), "strict set-cookie accepted a non-positive Max-Age");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Secure; secure")
	), "strict set-cookie accepted a duplicate attribute");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc;")
	), "strict set-cookie accepted a trailing delimiter");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Domain=-example.com")
	), "strict set-cookie accepted an invalid domain");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Partitioned=value; Secure")
	), "strict set-cookie accepted a valued Partitioned attribute");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Priority=Urgent")
	), "strict set-cookie accepted an invalid Priority");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; SameSite=None")
	), "strict set-cookie accepted SameSite=None without Secure");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("sid=abc; Partitioned")
	), "strict set-cookie accepted Partitioned without Secure");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("__Secure-sid=abc")
	), "strict set-cookie accepted __Secure without Secure");
	testRequire(!xrtSetCookieValidate(
		XRT_STR_LITERAL("__Host-sid=abc; Secure; Path=/sub")
	), "strict set-cookie accepted an invalid __Host path");
}



/* 验证用户代理接收上限、控制字节和超长属性忽略规则。 */
static void testSetCookieReceiveLimits(void)
{
	char Pair[XSET_COOKIE_MAX_PAIR_BYTES + 3u];
	char Attribute[XSET_COOKIE_MAX_ATTRIBUTE_VALUE + 32u];
	xsetcookie Cookie;
	size_t i;

	Pair[0] = 'a';
	Pair[1] = '=';
	for ( i = 2; i < sizeof(Pair); i++ ) {
		Pair[i] = 'x';
	}
	testRequire(xrtSetCookieParse(
		(xstrview){ Pair, XSET_COOKIE_MAX_PAIR_BYTES + 1u }, &Cookie
	), "set-cookie rejected the 4096-byte pair boundary");
	testRequire(!xrtSetCookieParse(
		(xstrview){ Pair, XSET_COOKIE_MAX_PAIR_BYTES + 2u }, &Cookie
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"set-cookie accepted an oversized pair");
	testRequire(!xrtSetCookieParse(
		XRT_STR_LITERAL("sid=ok\r\nInjected: yes"), &Cookie
	), "set-cookie accepted field injection");

	memcpy(Attribute, "sid=x; Path=", 12);
	for ( i = 12; i < sizeof(Attribute); i++ ) {
		Attribute[i] = 'x';
	}
	testRequire(xrtSetCookieParse(
		(xstrview){ Attribute, sizeof(Attribute) }, &Cookie
	) && ((Cookie.Flags & XSET_COOKIE_HAS_PATH) == 0),
		"set-cookie did not ignore an oversized attribute value");
}



/* 验证结构化构建、扩展属性、安全约束和解析回环。 */
static void testSetCookieWrite(void)
{
	static const xcookieattribute Extensions[] = {
		{ 0, XRT_STR_INIT("SameParty"), { NULL, 0 } },
		{ XCOOKIE_ATTRIBUTE_HAS_VALUE,
		  XRT_STR_INIT("Vendor"), XRT_STR_INIT("on") }
	};
	xsetcookie Cookie;
	xsetcookie Parsed;
	char Text[512];
	char Before[512];
	str sBuilt;
	size_t iSize;
	xtime iExpires;

	memset(&Cookie, 0, sizeof(Cookie));
	testRequire(xrtDateTime(
		2026, 6, 9, 10, 18, 14, 0, &iExpires
	), "set-cookie expiry fixture creation failed");
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.Value = XRT_STR_LITERAL("abc123");
	Cookie.Domain = XRT_STR_LITERAL("example.com");
	Cookie.Path = XRT_STR_LITERAL("/");
	Cookie.Expires = iExpires;
	Cookie.MaxAge = 60;
	Cookie.SameSite = XCOOKIE_SAME_SITE_LAX;
	Cookie.Priority = XCOOKIE_PRIORITY_HIGH;
	Cookie.Extensions = Extensions;
	Cookie.ExtensionCount = 2;
	Cookie.Flags = XSET_COOKIE_HAS_DOMAIN | XSET_COOKIE_HAS_PATH |
		XSET_COOKIE_HAS_EXPIRES | XSET_COOKIE_HAS_MAX_AGE |
		XSET_COOKIE_HAS_SAME_SITE | XSET_COOKIE_SECURE |
		XSET_COOKIE_HTTP_ONLY | XSET_COOKIE_PARTITIONED |
		XSET_COOKIE_HAS_PRIORITY;

	memset(Text, 0x5A, sizeof(Text));
	memcpy(Before, Text, sizeof(Text));
	testRequire(xrtSetCookieWrite(
		&Cookie, NULL, 0, &iSize
	), "set-cookie size query failed");
	testRequire(!xrtSetCookieWrite(
		&Cookie, Text, iSize - 1u, &iSize
	) && (memcmp(Text, Before, sizeof(Text)) == 0),
		"set-cookie short write was not atomic");
	testRequire(xrtSetCookieWrite(
		&Cookie, Text, sizeof(Text), &iSize
	) && ((uint8)Text[iSize] == UINT8_C(0x5A)),
		"set-cookie write failed or appended a hidden terminator");
	sBuilt = xrtSetCookieBuild(&Cookie, &iSize);
	testRequire((sBuilt != NULL) &&
		(strstr(sBuilt, "sid=abc123; Domain=example.com; Path=/") == sBuilt) &&
		(strstr(sBuilt, "; Expires=Tue, 09 Jun 2026 10:18:14 GMT") != NULL) &&
		(strstr(sBuilt, "; Max-Age=60; SameSite=Lax; Secure; HttpOnly; "
			"Partitioned; Priority=High; SameParty; Vendor=on") != NULL),
		"set-cookie allocated build mismatch");
	testRequire(xrtSetCookieParse(
		(xstrview){ sBuilt, iSize }, &Parsed
	) && testSetCookieText(Parsed.Name, "sid") &&
		(Parsed.MaxAge == 60) && (Parsed.Expires == iExpires),
		"set-cookie build/parse roundtrip failed");
	xrtFree(sBuilt);

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = XRT_STR_LITERAL("age");
	Cookie.Value = XRT_STR_LITERAL("edge");
	Cookie.Flags = XSET_COOKIE_HAS_MAX_AGE;
	Cookie.MaxAge = INT64_MIN;
	testRequire(xrtSetCookieWrite(
		&Cookie, Text, sizeof(Text), &iSize
	) && xrtSetCookieParse(
		(xstrview){ Text, iSize }, &Parsed
	) && (Parsed.MaxAge == INT64_MIN),
		"set-cookie signed Max-Age boundary failed");
}



/* 验证结构化构建器主动阻止浏览器会拒绝的安全组合。 */
static void testSetCookieSafety(void)
{
	xsetcookie Cookie;
	char Output[128];
	size_t iSize = 77;

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = XRT_STR_LITERAL("__Secure-sid");
	Cookie.Value = XRT_STR_LITERAL("x");
	testRequire(!xrtSetCookieWrite(
		&Cookie, Output, sizeof(Output), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"set-cookie accepted __Secure without Secure");
	Cookie.Name = XRT_STR_LITERAL("__Host-sid");
	Cookie.Flags = XSET_COOKIE_SECURE | XSET_COOKIE_HAS_PATH;
	Cookie.Path = XRT_STR_LITERAL("/sub");
	testRequire(!xrtSetCookieWrite(
		&Cookie, Output, sizeof(Output), &iSize
	), "set-cookie accepted an invalid __Host path");
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.Path = (xstrview){ NULL, 0 };
	Cookie.Flags = XSET_COOKIE_HAS_SAME_SITE;
	Cookie.SameSite = XCOOKIE_SAME_SITE_NONE;
	testRequire(!xrtSetCookieWrite(
		&Cookie, Output, sizeof(Output), &iSize
	), "set-cookie accepted SameSite=None without Secure");
	Cookie.SameSite = XCOOKIE_SAME_SITE_DEFAULT;
	Cookie.Flags = XSET_COOKIE_PARTITIONED;
	testRequire(!xrtSetCookieWrite(
		&Cookie, Output, sizeof(Output), &iSize
	), "set-cookie accepted Partitioned without Secure");
}



/* 执行 Set-Cookie 接收、生成、日期、安全和边界测试。 */
int main(void)
{
	testSetCookieParseCompat();
	testCookieDate();
	testSetCookieStrictValidation();
	testSetCookieReceiveLimits();
	testSetCookieWrite();
	testSetCookieSafety();
	printf("[PASS] set_cookie\n");
	return 0;
}

