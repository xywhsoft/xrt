#include "../test.h"



/* 比较借用文本与零结尾常量。 */
static bool testHttpTargetTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证 origin、absolute、authority 与 asterisk 四种形式。 */
static void testHttpTargetForms(void)
{
	xhttptarget Target;
	uint16 iPort;

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/items?q=1"),
		&Target
	) && (Target.Form == XHTTP_TARGET_ORIGIN) &&
		testHttpTargetTextEqual(Target.Path, "/items") &&
		testHttpTargetTextEqual(Target.Query, "q=1"),
		"HTTP origin-form target mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/items?q=1"
		),
		&Target
	) && (Target.Form == XHTTP_TARGET_ABSOLUTE) &&
		testHttpTargetTextEqual(Target.Scheme, "https") &&
		testHttpTargetTextEqual(Target.Host.Host, "example.test") &&
		(Target.Host.Port == 8443) &&
		testHttpTargetTextEqual(Target.Path, "/items"),
		"HTTP absolute-form target mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test:/items"),
		&Target
	) && (Target.Form == XHTTP_TARGET_ABSOLUTE) &&
		((Target.Host.Flags & XHTTP_AUTHORITY_PORT_EMPTY) != 0) &&
		xrtHttpAuthorityPort(&Target.Host, 80u, &iPort) && (iPort == 80),
		"HTTP absolute-form empty port mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("[2001:db8::1]:443"),
		&Target
	) && (Target.Form == XHTTP_TARGET_AUTHORITY) &&
		testHttpTargetTextEqual(Target.Host.Host, "2001:db8::1") &&
		(Target.Host.Port == 443),
		"HTTP authority-form target mismatch");
	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("example.test:65536"),
		&Target
	) && (Target.Form == XHTTP_TARGET_AUTHORITY) &&
		((Target.Host.Flags & XHTTP_AUTHORITY_PORT_VALUE) == 0) &&
		testHttpTargetTextEqual(Target.Host.PortText, "65536"),
		"HTTP target parser imposed a network port range");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("OPTIONS"),
		XRT_STR_LITERAL("*"),
		&Target
	) && (Target.Form == XHTTP_TARGET_ASTERISK) &&
		(Target.Flags == 0),
		"HTTP asterisk-form target mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("FETCH"),
		XRT_STR_LITERAL("urn:example:item"),
		&Target
	) && (Target.Form == XHTTP_TARGET_ABSOLUTE) &&
		testHttpTargetTextEqual(Target.Scheme, "urn"),
		"HTTP opaque absolute-form target mismatch");
}



/* 验证每种形式选择协议规定的有效 authority。 */
static void testHttpTargetEffectiveAuthority(void)
{
	xhttptarget Target;
	xhttpauthority Authority;

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		&Target
	) && xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("origin.test:8080"),
		&Authority
	) && testHttpTargetTextEqual(
		Authority.Host, "origin.test"
	) && (Authority.Port == 8080),
		"HTTP origin-form authority mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://target.test:81/resource"),
		&Target
	) && xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
		&Authority
	) && testHttpTargetTextEqual(
		Authority.Host, "target.test"
	) && (Authority.Port == 81),
		"HTTP absolute-form did not override Host");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("tunnel.test:443"),
		&Target
	) && xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
		&Authority
	) && testHttpTargetTextEqual(
		Authority.Host, "tunnel.test"
	) && (Authority.Port == 443),
		"HTTP CONNECT authority mismatch");

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("urn:example:item"),
		&Target
	) && !xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
		&Authority
	), "HTTP opaque absolute URI published a false authority");
	xrtClearError();
}



/* 验证方法不匹配、非法 URI 和不安全 authority 全部被拒绝。 */
static void testHttpTargetInvalid(void)
{
	static const struct {
		xstrview Method;
		xstrview Target;
	} Cases[] = {
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("example.test/path") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("//example.test/path") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("/path#fragment") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("/bad%ZZ") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("http:///path") },
		{ XRT_STR_INIT("GET"), XRT_STR_INIT("http://user@example.test/") },
		{ XRT_STR_INIT("CONNECT"), XRT_STR_INIT("/path") },
		{ XRT_STR_INIT("CONNECT"), XRT_STR_INIT("example.test") },
		{ XRT_STR_INIT("CONNECT"), XRT_STR_INIT("example.test:") },
		{ XRT_STR_INIT("CONNECT"), XRT_STR_INIT("user@example.test:443") }
	};
	xhttptarget Target;
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		memset(&Target, 0xA5, sizeof(Target));
		testRequire(!xrtHttpTargetParse(
			Cases[i].Method,
			Cases[i].Target,
			&Target
		) && (Target.Form == 0) &&
			(Target.Text.Data == NULL),
			"HTTP target accepted an invalid form");
		xrtClearError();
	}
}



/* 验证空指针和非法视图不会发布半成品。 */
static void testHttpTargetContracts(void)
{
	uint8 TargetStorage[sizeof(xhttptarget) + 2u];
	uint8 AuthorityStorage[sizeof(xhttpauthority) + 2u];
	union {
		xhttptarget Target;
		char Text[sizeof(xhttptarget)];
	} Alias;
	xhttptarget Target;
	xhttptarget Before;
	xhttpauthority Authority;

	memset(TargetStorage, 0xA5, sizeof(TargetStorage));
	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/unaligned?q=1"),
		(xhttptarget*)(void*)(TargetStorage + 1u)
	) && (TargetStorage[0] == 0xA5) &&
		(TargetStorage[sizeof(TargetStorage) - 1u] == 0xA5),
		"HTTP target rejected an unaligned output");
	memcpy(&Target, TargetStorage + 1u, sizeof(Target));
	testRequire((Target.Form == XHTTP_TARGET_ORIGIN) &&
		testHttpTargetTextEqual(Target.Path, "/unaligned"),
		"HTTP target unaligned output content mismatch");

	memset(AuthorityStorage, 0xA5, sizeof(AuthorityStorage));
	testRequire(xrtHttpTargetAuthority(
		(xhttptarget*)(void*)(TargetStorage + 1u),
		XRT_STR_LITERAL("authority.test:8080"),
		(xhttpauthority*)(void*)(AuthorityStorage + 1u)
	) && (AuthorityStorage[0] == 0xA5) &&
		(AuthorityStorage[sizeof(AuthorityStorage) - 1u] == 0xA5),
		"HTTP target authority rejected unaligned storage");
	memcpy(&Authority, AuthorityStorage + 1u, sizeof(Authority));
	testRequire(testHttpTargetTextEqual(
		Authority.Host, "authority.test"
	) && (Authority.Port == 8080u),
		"HTTP target unaligned authority content mismatch");

	xrtClearError();
	testRequire(!xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		(xhttptarget*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP target accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("authority.test"),
		(xhttpauthority*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP target authority accepted a wrapping output");
	xrtClearError();

	testRequire(!xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		NULL
	), "HTTP target accepted a null output");
	xrtClearError();

	memset(&Target, 0xA5, sizeof(Target));
	Before = Target;
	testRequire(!xrtHttpTargetParse(
		(xstrview){ NULL, 1 },
		XRT_STR_LITERAL("/"),
		&Target
	) && (memcmp(&Target, &Before, sizeof(Target)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP target accepted an invalid method view");
	xrtClearError();
	testRequire(!xrtHttpTargetParse(
		XRT_STR_LITERAL("BAD METHOD"),
		XRT_STR_LITERAL("/"),
		&Target
	) && (Target.Form == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP target syntax error used the argument category");
	xrtClearError();

	memset(&Alias, 0xA5, sizeof(Alias));
	memcpy(Alias.Text, "/alias", 6u);
	testRequire(!xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Alias.Text, 6u },
		&Alias.Target
	) && (memcmp(Alias.Text, "/alias", 6u) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP target accepted overlapping output");
	xrtClearError();

	memset(&Authority, 0xA5, sizeof(Authority));
	testRequire(!xrtHttpTargetAuthority(
		NULL,
		XRT_STR_LITERAL("example.test"),
		&Authority
	) && (Authority.Flags == 0),
		"HTTP target authority accepted a null target");
	xrtClearError();

	testRequire(xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"), &Target
	), "HTTP target authority alias fixture failed");
	Before = Target;
	testRequire(!xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("example.test"),
		(xhttpauthority*)&Target
	) && (memcmp(&Target, &Before, sizeof(Target)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP target authority accepted overlapping output");
	xrtClearError();
}



/* 运行 HTTP request-target 协议测试。 */
int main(void)
{
	testHttpTargetForms();
	testHttpTargetEffectiveAuthority();
	testHttpTargetInvalid();
	testHttpTargetContracts();
	printf("[PASS] HTTP request-target\n");
	return 0;
}
