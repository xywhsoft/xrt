#include "../test.h"



/* 按长度比较字符串视图与 ASCII 字面量。 */
static bool testUrlText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 解析 URI-reference，并要求结构化写出后保持同一文本。 */
static xurl testUrlRoundTrip(cstr sText)
{
	xstrview Text = { sText, strlen(sText) };
	xurl Url;
	char Output[512];
	size_t iSize = 0;

	testRequire(xrtUrlParse(Text, &Url), "URL parse failed");
	testRequire(xrtUrlWrite(
		&Url, Output, sizeof(Output), &iSize
	), "URL write failed");
	testRequire((iSize == Text.Size) &&
		((iSize == 0) || (memcmp(Output, Text.Data, iSize) == 0)),
		"URL round trip changed text");
	return Url;
}



/* 验证空组件、层次 URI、无层次 URI 与相对引用。 */
static void testUrlParseForms(void)
{
	xurl Url;
	char Output[256];
	size_t iSize;
	uint16 iPort;

	Url = testUrlRoundTrip("");
	testRequire((Url.Flags == 0) && (Url.Path.Size == 0),
		"empty URL reference mismatch");
	Url = testUrlRoundTrip("?");
	testRequire(((Url.Flags & XURL_HAS_QUERY) != 0) &&
		(Url.Query.Size == 0), "empty query was lost");
	Url = testUrlRoundTrip("#");
	testRequire(((Url.Flags & XURL_HAS_FRAGMENT) != 0) &&
		(Url.Fragment.Size == 0), "empty fragment was lost");
	Url = testUrlRoundTrip("?#");
	testRequire(((Url.Flags & XURL_HAS_QUERY) != 0) &&
		((Url.Flags & XURL_HAS_FRAGMENT) != 0),
		"empty query and fragment were not distinguished");

	Url = testUrlRoundTrip(
		"https://user@example.com:0/a//b?x?#f"
	);
	testRequire(xrtUrlSchemeIs(&Url, XRT_STR_LITERAL("HTTPS")) &&
		xrtUrlSecure(&Url) && testUrlText(Url.UserInfo, "user") &&
		testUrlText(Url.Host, "example.com") &&
		(Url.Port == 0) && ((Url.Flags & XURL_HAS_PORT) != 0) &&
		testUrlText(Url.Path, "/a//b") &&
		testUrlText(Url.Query, "x?") && testUrlText(Url.Fragment, "f"),
		"hierarchical URL components mismatch");
	testRequire(xrtUrlTargetWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (iSize == strlen("/a//b?x?")) &&
		(memcmp(Output, "/a//b?x?", iSize) == 0),
		"HTTP target included fragment or changed path");

	Url = testUrlRoundTrip("mailto:user@example.com");
	testRequire(testUrlText(Url.Scheme, "mailto") &&
		testUrlText(Url.Path, "user@example.com") &&
		((Url.Flags & XURL_HAS_AUTHORITY) == 0),
		"rootless URI mismatch");
	Url = testUrlRoundTrip("urn:isbn:0451450523");
	testRequire(testUrlText(Url.Scheme, "urn") &&
		testUrlText(Url.Path, "isbn:0451450523"),
		"opaque-style URI mismatch");
	Url = testUrlRoundTrip("//host/path");
	testRequire(((Url.Flags & XURL_HAS_SCHEME) == 0) &&
		testUrlText(Url.Host, "host"),
		"network-path reference mismatch");
	(void)testUrlRoundTrip("a/b");
	(void)testUrlRoundTrip("./a:b");

	Url = testUrlRoundTrip("http://host:");
	testRequire(((Url.Flags & XURL_HAS_PORT) != 0) &&
		((Url.Flags & XURL_PORT_EMPTY) != 0) &&
		((Url.Flags & XURL_PORT_VALUE) == 0) &&
		(Url.Port == 0) && xrtUrlPort(&Url, &iPort) &&
		(iPort == 80) &&
		xrtUrlPortIsDefault(&Url),
		"explicit empty port default mismatch");
	Url = testUrlRoundTrip("http://host:00080/path");
	testRequire(((Url.Flags & XURL_PORT_VALUE) != 0) &&
		(Url.Port == 80) && testUrlText(Url.PortText, "00080"),
		"lexical port text was not preserved");
	Url = testUrlRoundTrip("http://host:65536/path");
	iPort = 77;
	testRequire(((Url.Flags & XURL_PORT_VALUE) == 0) &&
		(Url.Port == 0) &&
		testUrlText(Url.PortText, "65536") &&
		!xrtUrlPort(&Url, &iPort) && (iPort == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"URL did not separate protocol port text from network range");
	xrtClearError();
	(void)testUrlRoundTrip("file:///path");
}



/* 验证 IP-literal、authority 与默认端口便利函数。 */
static void testUrlAuthority(void)
{
	static const cstr ValidIpv6[] = {
		"http://[::]/",
		"http://[::1]/",
		"http://[1::]/",
		"http://[1:2:3:4:5:6:7:8]/",
		"http://[1:2:3:4:5:6::8]/",
		"http://[::ffff:192.0.2.128]/",
		"http://[1:2:3:4:5:6:192.0.2.1]/"
	};
	xurl Url;
	char Output[128];
	size_t iSize;
	size_t i;
	uint16 iPort;

	Url = testUrlRoundTrip("https://[2001:db8::1]:443/a");
	testRequire(((Url.Flags & XURL_HOST_IP_LITERAL) != 0) &&
		testUrlText(Url.Host, "2001:db8::1") &&
		xrtUrlPort(&Url, &iPort) && (iPort == 443) &&
		xrtUrlPortIsDefault(&Url),
		"IPv6 or default port mismatch");
	testRequire(xrtUrlAuthorityWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (iSize == strlen("[2001:db8::1]:443")) &&
		(memcmp(Output, "[2001:db8::1]:443", iSize) == 0),
		"authority write mismatch");
	testRequire(xrtUrlHostWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (iSize == strlen("[2001:db8::1]:443")) &&
		(memcmp(Output, "[2001:db8::1]:443", iSize) == 0),
		"host write mismatch");

	Url = testUrlRoundTrip("//name:pass@[vF.a:b]:9/");
	testRequire(testUrlText(Url.UserInfo, "name:pass") &&
		testUrlText(Url.Host, "vF.a:b") && (Url.Port == 9),
		"IPvFuture authority mismatch");
	testRequire(xrtUrlAuthorityParse(
		XRT_STR_LITERAL("user@example.test:80"), &Url
	) && testUrlText(Url.UserInfo, "user") &&
		testUrlText(Url.Host, "example.test") && (Url.Port == 80),
		"standalone authority parse mismatch");
	testRequire((xrtUrlDefaultPort(XRT_STR_LITERAL("http")) == 80) &&
		(xrtUrlDefaultPort(XRT_STR_LITERAL("WSS")) == 443) &&
		(xrtUrlDefaultPort(XRT_STR_LITERAL("ftp")) == 0) &&
		(xrtUrlDefaultPort(XRT_STR_LITERAL("custom")) == 0),
		"default port mapping mismatch");
	for ( i = 0; i < (sizeof(ValidIpv6) / sizeof(ValidIpv6[0])); i++ ) {
		testRequire(xrtUrlParse(
			(xstrview){ ValidIpv6[i], strlen(ValidIpv6[i]) }, &Url
		) && ((Url.Flags & XURL_HOST_IP_LITERAL) != 0),
			"valid IPv6 URL was rejected");
	}
}



/* 验证格式错误输入被拒绝且失败结果被清零。 */
static void testUrlInvalid(void)
{
	static const cstr Inputs[] = {
		"a b",
		"a%2",
		"a%GG",
		"1a:b",
		"http://a@b@c/",
		"http://[::1/",
		"http://[1:2:3:4:5:6:7:8:]/",
		"http://[1:2:3:4:5:6:7]/",
		"http://[1:2:3:4:5:6:7:8:9]/",
		"http://[::ffff:192.168.001.1]/",
		"http://[1::2::3]/",
		"http://[:1]/",
		"http://[1:]/",
		"http://[::ffff:192.0.2.256]/",
		"http://[::ffff:192.0.2]/",
		"http://[::ffff:192.0.2.1:80]/",
		"http://[]/",
		"http://[fe80::1%25eth!]/",
		"http://[fe80::1%25eth0]/",
		"http://[fe80::1%eth0]/",
		"http://[v.abc]/",
		"http://::1/",
		"http://host:abc/",
		"//host/path#one#two"
	};
	xurl Url;
	size_t i;
	const char Control[] = { 'a', '\n', 'b' };
	const char NonAscii[] = { 'a', (char)0xC3, (char)0xA9 };

	for ( i = 0; i < (sizeof(Inputs) / sizeof(Inputs[0])); i++ ) {
		memset(&Url, 0xA5, sizeof(Url));
		testRequire(!xrtUrlParse(
			(xstrview){ Inputs[i], strlen(Inputs[i]) }, &Url
		) && (Url.Flags == 0) && (Url.Path.Size == 0),
			"invalid URL was accepted or exposed partial state");
		xrtClearError();
	}
	testRequire(!xrtUrlParse(
		(xstrview){ Control, sizeof(Control) }, &Url
	), "URL parser accepted ASCII control byte");
	xrtClearError();
	testRequire(!xrtUrlParse(
		(xstrview){ NonAscii, sizeof(NonAscii) }, &Url
	), "URI parser accepted raw non-ASCII bytes");
	xrtClearError();
}



/* 验证长度查询、短缓冲、重叠检查和分配型构建。 */
static void testUrlOutput(void)
{
	char Input[] = "https://example.test/path?q=1#f";
	char Output[128];
	xurl Url;
	str sBuilt;
	size_t iSize = 0;

	testRequire(xrtUrlParse(
		(xstrview){ Input, strlen(Input) }, &Url
	), "URL output fixture parse failed");
	testRequire(xrtUrlWrite(&Url, NULL, 0, &iSize) &&
		(iSize == strlen(Input)), "URL size query mismatch");
	memset(Output, 'z', sizeof(Output));
	testRequire(!xrtUrlWrite(
		&Url, Output, iSize - 1u, &iSize
	) && (iSize == strlen(Input)) && (Output[0] == 'z'),
		"URL short output was not atomic");
	xrtClearError();
	testRequire(!xrtUrlWrite(
		&Url, Input, sizeof(Input), &iSize
	) && (memcmp(Input, "https://example.test/path?q=1#f",
		strlen(Input)) == 0), "URL output overlap was not rejected");
	xrtClearError();
	sBuilt = xrtUrlBuild(&Url, &iSize);
	testRequire((sBuilt != NULL) && (iSize == strlen(Input)) &&
		(memcmp(sBuilt, Input, iSize + 1u) == 0),
		"allocated URL build mismatch");
	xrtFree(sBuilt);

	Url.Path = XRT_STR_LITERAL("relative");
	testRequire(!xrtUrlTargetWrite(
		&Url, Output, sizeof(Output), &iSize
	), "HTTP origin target accepted a relative path");
	xrtClearError();
}



/* 验证空参数、非法视图和不一致的手工结构都按统一错误契约失败。 */
static void testUrlArguments(void)
{
	uint8 UrlStorage[sizeof(xurl) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xurl Url;
	xurl Relative;
	union {
		xurl Url;
		uint16 Port;
	} Alias;
	union {
		xurl Base;
		size_t Size;
	} Shared;
	uint8 PortStorage[sizeof(uint16) + 1u];
	char Output[64] = { 0 };
	str sBuilt;
	size_t iSize;
	uint16 iPort;

	testRequire(!xrtUrlParse(
		(xstrview){ NULL, 1 }, &Url
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL invalid view error mismatch");
	xrtClearError();
	testRequire(!xrtUrlParse(
		XRT_STR_LITERAL("/"), NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL null output error mismatch");
	xrtClearError();
	testRequire(xrtUrlParse((xstrview){ NULL, 0 }, &Url) &&
		xrtUrlTargetWrite(&Url, Output, sizeof(Output), &iSize) &&
		(iSize == 1) && (Output[0] == '/'),
		"empty reference HTTP target mismatch");
	testRequire(!xrtUrlWrite(
		&Url, Output, sizeof(Output), NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL null size output error mismatch");
	xrtClearError();
	testRequire(!xrtUrlPort(NULL, &iPort) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL port accepted a null descriptor");
	xrtClearError();
	testRequire(!xrtUrlPort(&Url, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL port accepted a null output");
	xrtClearError();
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("https://example.test:8443/"), &Url
	) && xrtUrlPort(
		&Url, (uint16*)(void*)(PortStorage + 1u)
	), "URL port rejected an unaligned output");
	memcpy(&iPort, PortStorage + 1u, sizeof(iPort));
	testRequire(iPort == 8443u,
		"URL unaligned port output content mismatch");

	memset(UrlStorage, 0xA5, sizeof(UrlStorage));
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("https://unaligned.test:9443/path"),
		(xurl*)(void*)(UrlStorage + 1u)
	) && (UrlStorage[0] == 0xA5) &&
		(UrlStorage[sizeof(UrlStorage) - 1u] == 0xA5),
		"URL parser rejected an unaligned output");
	memcpy(&Url, UrlStorage + 1u, sizeof(Url));
	testRequire(testUrlText(Url.Host, "unaligned.test") &&
		(Url.Port == 9443u),
		"URL unaligned parse content mismatch");

	memset(UrlStorage, 0xA5, sizeof(UrlStorage));
	testRequire(xrtUrlAuthorityParse(
		XRT_STR_LITERAL("user@authority.test:8080"),
		(xurl*)(void*)(UrlStorage + 1u)
	) && (UrlStorage[0] == 0xA5) &&
		(UrlStorage[sizeof(UrlStorage) - 1u] == 0xA5),
		"URL authority parser rejected an unaligned output");
	memcpy(&Url, UrlStorage + 1u, sizeof(Url));
	testRequire(testUrlText(Url.Host, "authority.test") &&
		(Url.Port == 8080u),
		"URL unaligned authority content mismatch");

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(xrtUrlAuthorityWrite(
		(xurl*)(void*)(UrlStorage + 1u),
		Output,
		sizeof(Output),
		(size_t*)(void*)(SizeStorage + 1u)
	) && (SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"URL writer rejected unaligned value or size storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == strlen("user@authority.test:8080")) &&
		(memcmp(Output, "user@authority.test:8080", iSize) == 0),
		"URL unaligned writer content mismatch");

	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	sBuilt = xrtUrlBuild(
		(xurl*)(void*)(UrlStorage + 1u),
		(size_t*)(void*)(SizeStorage + 1u)
	);
	testRequire((sBuilt != NULL) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"URL builder rejected unaligned value or size storage");
	xrtFree(sBuilt);

	xrtClearError();
	testRequire(!xrtUrlParse(
		XRT_STR_LITERAL("/"),
		(xurl*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL parser accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtUrlAuthorityParse(
		XRT_STR_LITERAL("example.test"),
		(xurl*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL authority parser accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtUrlWrite(
		&Url,
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL writer accepted a wrapping size output");
	xrtClearError();
	testRequire(!xrtUrlWrite(
		&Url,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		8u,
		&iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL writer accepted a wrapping byte output");
	xrtClearError();
	testRequire(xrtUrlBuild(
		&Url,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL builder accepted a wrapping size output");
	xrtClearError();

	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("https://example.test/"), &Alias.Url
	) && !xrtUrlPort(&Alias.Url, &Alias.Port) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL port accepted descriptor/output overlap");
	xrtClearError();

	memset(&Url, 0, sizeof(Url));
	Url.Flags = XURL_PORT_EMPTY;
	testRequire(!xrtUrlWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"URL inconsistent empty port state was accepted");
	xrtClearError();

	memset(&Url, 0, sizeof(Url));
	Url.Flags = XURL_HAS_AUTHORITY | XURL_HAS_HOST;
	Url.Host = XRT_STR_LITERAL("example.test");
	Url.Port = 80;
	testRequire(!xrtUrlWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"URL accepted a port without its presence flag");
	xrtClearError();
	memset(&Url, 0, sizeof(Url));
	Url.Flags = XURL_HAS_AUTHORITY | XURL_HAS_HOST |
		XURL_HAS_PORT | XURL_PORT_VALUE;
	Url.Host = XRT_STR_LITERAL("example.test");
	Url.Port = 80;
	testRequire(xrtUrlHostWrite(
		&Url, Output, sizeof(Output), &iSize
	) && (iSize == 15u) &&
		(memcmp(Output, "example.test:80", iSize) == 0),
		"URL rejected a valid hand-built numeric port");

	testRequire(xrtUrlParse(XRT_STR_LITERAL("relative/path"), &Relative) &&
		(xrtUrlResolveBuild(
			&Relative, XRT_STR_LITERAL("next"), &iSize
		) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"URL resolve accepted a base without scheme");
	xrtClearError();
	iPort = 77u;
	testRequire(!xrtUrlPort(&Relative, &iPort) && (iPort == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"URL port fabricated a value without explicit or default port");
	xrtClearError();
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("https://example.test/base"),
		&Shared.Base
	), "URL resolve overlap fixture parse failed");
	testRequire(!xrtUrlResolve(
		&Shared.Base, XRT_STR_LITERAL("next"),
		NULL, 0, &Shared.Size
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL resolve accepted Base/size overlap");
	xrtClearError();
}



/* 验证 URL 长度只受 size_t 与可用内存约束，不继承旧固定缓冲上限。 */
static void testUrlLong(void)
{
	char Input[6200];
	char Output[6200];
	xurl Url;
	size_t iInput;
	size_t iOutput;
	uint16 iPort = 77u;

	memcpy(Input, "https://example.test/", 21u);
	memset(Input + 21u, 'a', 5000u);
	memcpy(Input + 5021u, "/file?q=1", 9u);
	iInput = 5030u;
	Input[iInput] = '\0';
	testRequire(xrtUrlParse(
		(xstrview){ Input, iInput }, &Url
	) && xrtUrlWrite(
		&Url, Output, sizeof(Output), &iOutput
	) && (iOutput == iInput) &&
		(memcmp(Output, Input, iInput) == 0),
		"long URL parse or write retained a fixed-size limit");

	memcpy(Input, "http://host:", 12u);
	memset(Input + 12u, '9', 5000u);
	Input[5012] = '/';
	iInput = 5013u;
	testRequire(xrtUrlParse(
		(xstrview){ Input, iInput }, &Url
	) && (Url.PortText.Size == 5000u) &&
		((Url.Flags & XURL_PORT_VALUE) == 0) &&
		xrtUrlWrite(&Url, Output, sizeof(Output), &iOutput) &&
		(iOutput == iInput) &&
		(memcmp(Output, Input, iInput) == 0) &&
		!xrtUrlPort(&Url, &iPort) && (iPort == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"long URL port text was limited, changed, or truncated");
	xrtClearError();
}



/* 规范化路径并比较精确结果。 */
static void testUrlNormalizeOne(cstr sInput, cstr sExpected)
{
	char Output[256];
	size_t iSize = 0;

	testRequire(xrtUrlPathNormalize(
		(xstrview){ sInput, strlen(sInput) },
		Output,
		sizeof(Output),
		&iSize
	), "URL path normalize failed");
	testRequire((iSize == strlen(sExpected)) &&
		((iSize == 0) || (memcmp(Output, sExpected, iSize) == 0)),
		"URL path normalize result mismatch");
}



/* 验证 RFC 3986 remove_dot_segments 且不折叠重复斜杠。 */
static void testUrlNormalize(void)
{
	char Overlap[64] = "/a/b/../c";
	char Reverse[64] = "a/b/../../g";
	char Unsafe[32] = "a/b/../../g";
	char UnsafeSaved[32];
	char Short[4] = { 'z', 'z', 'z', 'z' };
	size_t iSize;

	testUrlNormalizeOne("", "");
	testUrlNormalizeOne("/a/b/c/./../../g", "/a/g");
	testUrlNormalizeOne("mid/content=5/../6", "mid/6");
	testUrlNormalizeOne("/a//b/./c/../", "/a//b/");
	testUrlNormalizeOne("../a", "a");
	testUrlNormalizeOne("../../a", "a");
	testUrlNormalizeOne("a/./b", "a/b");
	testUrlNormalizeOne("a/../b", "/b");
	testUrlNormalizeOne("/../", "/");
	testUrlNormalizeOne("/.", "/");
	testRequire(xrtUrlPathNormalize(
		(xstrview){ Overlap, strlen(Overlap) },
		Overlap,
		sizeof(Overlap),
		&iSize
	) && (iSize == strlen("/a/c")) &&
		(memcmp(Overlap, "/a/c", iSize) == 0),
		"URL path normalize overlap mismatch");
	testRequire(xrtUrlPathNormalize(
		(xstrview){ Reverse, strlen(Reverse) },
		Reverse + 1u, sizeof(Reverse) - 1u, &iSize
	) && (iSize == strlen("/g")) &&
		(memcmp(Reverse + 1u, "/g", iSize) == 0),
		"URL reverse-overlap normalize mismatch");
	memcpy(UnsafeSaved, Unsafe, sizeof(Unsafe));
	testRequire(!xrtUrlPathNormalize(
		(xstrview){ Unsafe, strlen(Unsafe) },
		Unsafe + 1u, strlen("/g"), &iSize
	) && (iSize == strlen("/g")) &&
		(memcmp(Unsafe, UnsafeSaved, sizeof(Unsafe)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL unsafe reverse overlap was not atomic");
	xrtClearError();
	testRequire(!xrtUrlPathNormalize(
		XRT_STR_LITERAL("/long/path"), Short, sizeof(Short), &iSize
	) && (iSize == strlen("/long/path")) && (Short[0] == 'z'),
		"URL path normalize short output was not atomic");
	xrtClearError();
}



/* 解析一个相对引用并与 RFC 3986 期望值比较。 */
static void testUrlResolveOne(
	const xurl* pBase,
	cstr sReference,
	cstr sExpected
)
{
	char Output[256];
	str sResolved;
	size_t iSize;
	size_t iWritten;

	sResolved = xrtUrlResolveBuild(
		pBase,
		(xstrview){ sReference, strlen(sReference) },
		&iSize
	);
	testRequire((sResolved != NULL) && (iSize == strlen(sExpected)) &&
		(memcmp(sResolved, sExpected, iSize + 1u) == 0),
		"URL reference resolution mismatch");
	testRequire(xrtUrlResolve(
		pBase,
		(xstrview){ sReference, strlen(sReference) },
		Output, sizeof(Output), &iWritten
	) && (iWritten == iSize) &&
		(memcmp(Output, sResolved, iSize) == 0),
		"URL direct and Build resolution diverged");
	xrtFree(sResolved);
}



/* 验证 RFC 3986 5.4 的正常与异常引用解析向量。 */
static void testUrlResolve(void)
{
	static const struct {
		cstr Reference;
		cstr Expected;
	} Cases[] = {
		{ "g:h", "g:h" },
		{ "g", "http://a/b/c/g" },
		{ "./g", "http://a/b/c/g" },
		{ "g/", "http://a/b/c/g/" },
		{ "/g", "http://a/g" },
		{ "//g", "http://g" },
		{ "?y", "http://a/b/c/d;p?y" },
		{ "g?y", "http://a/b/c/g?y" },
		{ "#s", "http://a/b/c/d;p?q#s" },
		{ "g#s", "http://a/b/c/g#s" },
		{ "g?y#s", "http://a/b/c/g?y#s" },
		{ ";x", "http://a/b/c/;x" },
		{ "g;x", "http://a/b/c/g;x" },
		{ "g;x?y#s", "http://a/b/c/g;x?y#s" },
		{ "", "http://a/b/c/d;p?q" },
		{ ".", "http://a/b/c/" },
		{ "./", "http://a/b/c/" },
		{ "..", "http://a/b/" },
		{ "../", "http://a/b/" },
		{ "../g", "http://a/b/g" },
		{ "../..", "http://a/" },
		{ "../../", "http://a/" },
		{ "../../g", "http://a/g" },
		{ "../../../g", "http://a/g" },
		{ "../../../../g", "http://a/g" },
		{ "/./g", "http://a/g" },
		{ "/../g", "http://a/g" },
		{ "g.", "http://a/b/c/g." },
		{ ".g", "http://a/b/c/.g" },
		{ "g..", "http://a/b/c/g.." },
		{ "..g", "http://a/b/c/..g" },
		{ "./../g", "http://a/b/g" },
		{ "./g/.", "http://a/b/c/g/" },
		{ "g/./h", "http://a/b/c/g/h" },
		{ "g/../h", "http://a/b/c/h" },
		{ "g;x=1/./y", "http://a/b/c/g;x=1/y" },
		{ "g;x=1/../y", "http://a/b/c/y" },
		{ "g?y/./x", "http://a/b/c/g?y/./x" },
		{ "g?y/../x", "http://a/b/c/g?y/../x" },
		{ "g#s/./x", "http://a/b/c/g#s/./x" },
		{ "g#s/../x", "http://a/b/c/g#s/../x" },
		{ "http:g", "http:g" }
	};
	xurl Base;
	xurl FragmentBase;
	char Reference[32] = "relative/path";
	char ReferenceSaved[32];
	char Output[64] = { 0 };
	size_t i;
	size_t iSize;

	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("http://a/b/c/d;p?q"), &Base
	), "URL resolve base parse failed");
	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		testUrlResolveOne(&Base, Cases[i].Reference, Cases[i].Expected);
	}
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("http://a/b?x#old"), &FragmentBase
	), "URL fragment base parse failed");
	testUrlResolveOne(&FragmentBase, "", "http://a/b?x");
	testUrlResolveOne(&FragmentBase, "#new", "http://a/b?x#new");

	memset(Output, 'z', sizeof(Output));
	testRequire(!xrtUrlResolve(
		&Base, XRT_STR_LITERAL("relative/path"),
		Output, 4, &iSize
	) && (iSize == strlen("http://a/b/c/relative/path")) &&
		(Output[0] == 'z'), "URL resolve short output was not atomic");
	xrtClearError();
	memcpy(ReferenceSaved, Reference, sizeof(Reference));
	testRequire(!xrtUrlResolve(
		&Base,
		(xstrview){ Reference, strlen(Reference) },
		Reference, sizeof(Reference), &iSize
	) && (memcmp(
		Reference, ReferenceSaved, sizeof(Reference)
	) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL resolve accepted Reference/output overlap");
	xrtClearError();
}



/* 执行 URL 基础协议层的全部确定性测试。 */
int main(void)
{
	testUrlParseForms();
	testUrlAuthority();
	testUrlInvalid();
	testUrlOutput();
	testUrlArguments();
	testUrlLong();
	testUrlNormalize();
	testUrlResolve();
	printf("[PASS] url\n");
	return 0;
}
