#include "../test.h"



/* 验证 Host authority 覆盖域名、IPv4、IPv6 与显式端口。 */
static void testHttpHostValidValues(void)
{
	static const xstrview Values[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(":"),
		XRT_STR_INIT("example.test"),
		XRT_STR_INIT("example.test:8080"),
		XRT_STR_INIT("127.0.0.1"),
		XRT_STR_INIT("127.0.0.1:80"),
		XRT_STR_INIT("[::1]"),
		XRT_STR_INIT("[::1]:"),
		XRT_STR_INIT("[2001:db8::1]:443"),
		XRT_STR_INIT("[vF.example:route]:9"),
		XRT_STR_INIT("service.example:0"),
		XRT_STR_INIT("service.example:"),
		XRT_STR_INIT("service.example:65536"),
		XRT_STR_INIT("service.example:12345678901234567890"),
		XRT_STR_INIT("svc!$&'()*+,;=.example")
	};
	xhttpauthority Host;
	uint16 iPort;
	size_t i;

	for ( i = 0; i < (sizeof(Values) / sizeof(Values[0])); i++ ) {
		testRequire(xrtHttpHostParse(Values[i], &Host) &&
			xrtHttpHostValid(Values[i]) &&
			xrtHttpAuthorityValid(&Host) &&
			(Host.Text.Size == Values[i].Size),
			"HTTP Host rejected a valid authority");
	}
	testRequire(xrtHttpHostParse(
		XRT_STR_LITERAL("[2001:db8::1]:443"), &Host
	) && ((Host.Flags & XHTTP_AUTHORITY_IP_LITERAL) != 0) &&
		(Host.Port == 443) && (Host.Host.Size == 11u) &&
		(memcmp(Host.Host.Data, "2001:db8::1", 11u) == 0),
		"HTTP Host structured parse mismatch");
	testRequire(xrtHttpHostParse(
		XRT_STR_LITERAL("example.test:"), &Host
	) && ((Host.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0) &&
		((Host.Flags & XHTTP_AUTHORITY_PORT_EMPTY) != 0) &&
		(Host.PortText.Size == 0),
		"HTTP Host empty port fact was not preserved");
	testRequire(xrtHttpHostParse(
		XRT_STR_LITERAL("example.test:65536"), &Host
	) && ((Host.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0) &&
		((Host.Flags & XHTTP_AUTHORITY_PORT_VALUE) == 0) &&
		(Host.Port == 0) && (Host.PortText.Size == 5u),
		"HTTP Host imposed the network port range on protocol syntax");

	memset(&Host, 0, sizeof(Host));
	Host.Flags = XHTTP_AUTHORITY_HAS_PORT |
		XHTTP_AUTHORITY_PORT_VALUE;
	Host.Host = XRT_STR_LITERAL("example.test");
	Host.Port = 8443u;
	testRequire(xrtHttpAuthorityValid(&Host) &&
		xrtHttpAuthorityPort(&Host, 80u, &iPort) &&
		(iPort == 8443u),
		"HTTP authority rejected a hand-built numeric port");
	Host.PortText = XRT_STR_LITERAL("443");
	testRequire(!xrtHttpAuthorityValid(&Host),
		"HTTP authority accepted mismatched port text");
	xrtClearError();
}



/* 验证空 Host、userinfo、错误端口和非法 URI 字节被拒绝。 */
static void testHttpHostInvalidValues(void)
{
	static const xstrview Values[] = {
		XRT_STR_INIT("@example.test"),
		XRT_STR_INIT("user@example.test"),
		XRT_STR_INIT("user@@example.test"),
		XRT_STR_INIT("[]"),
		XRT_STR_INIT("[::1]suffix"),
		XRT_STR_INIT("example.test:+1"),
		XRT_STR_INIT("example%zz.test"),
		XRT_STR_INIT("example.test/path"),
		XRT_STR_INIT("example.test?query"),
		XRT_STR_INIT("example.test#fragment"),
		XRT_STR_INIT("example.test, other.test"),
		XRT_STR_INIT(" example.test"),
		XRT_STR_INIT("example.test "),
		XRT_STR_INIT("2001:db8::1")
	};
	size_t i;

	for ( i = 0; i < (sizeof(Values) / sizeof(Values[0])); i++ ) {
		testRequire(!xrtHttpHostValid(Values[i]),
			"HTTP Host accepted an invalid authority");
		xrtClearError();
	}
}



/* 验证参数错误、值错误和输出别名不会损坏调用方内存。 */
static void testHttpHostContracts(void)
{
	union {
		xhttpauthority Host;
		char Text[sizeof(xhttpauthority)];
	} Alias;
	xhttpauthority Host;

	testRequire(
		xrtHttpHostEqual(
			XRT_STR_LITERAL("EXAMPLE.test"),
			XRT_STR_LITERAL("example.TEST")
		) && !xrtHttpHostEqual(
			XRT_STR_LITERAL("example.test"),
			XRT_STR_LITERAL("example.test.")
		) && !xrtHttpHostEqual(
			(xstrview){ NULL, 1u },
			XRT_STR_LITERAL("example.test")
		),
		"HTTP Host equality mismatch"
	);

	testRequire(!xrtHttpHostParse(
		XRT_STR_LITERAL("example.test"), NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Host null output error mismatch");
	xrtClearError();
	testRequire(!xrtHttpAuthorityValid(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP authority null input error mismatch");
	xrtClearError();

	testRequire(!xrtHttpHostParse(
		(xstrview){ NULL, 1u }, &Host
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Host invalid view error mismatch");
	xrtClearError();

	memset(&Host, 0xA5, sizeof(Host));
	testRequire(!xrtHttpHostParse(
		XRT_STR_LITERAL("user@example.test"), &Host
	) && (Host.Flags == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Host protocol value error mismatch");
	xrtClearError();

	memset(&Alias, 0xA5, sizeof(Alias));
	memcpy(Alias.Text, "example.test", 12u);
	testRequire(!xrtHttpHostParse(
		(xstrview){ Alias.Text, 12u }, &Alias.Host
	) && (memcmp(Alias.Text, "example.test", 12u) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Host accepted overlapping output");
	xrtClearError();
}



/* 运行共享 Host 字段语义测试。 */
int main(void)
{
	testHttpHostValidValues();
	testHttpHostInvalidValues();
	testHttpHostContracts();
	printf("[PASS] HTTP Host\n");
	return 0;
}
