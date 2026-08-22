#include "../test.h"



/* 验证参数语义 Host 支持转义、IP 字面地址、长名称和严格失败。 */
static void testHttpParamHost(void)
{
	static const xstrview Valid[] = {
		XRT_STR_INIT("exa\\mple.com:443"),
		XRT_STR_INIT("\\[2001:db8::1]:443"),
		XRT_STR_INIT("[v1.long-future-address]:65536"),
		XRT_STR_INIT("name%2Eexample:"),
		XRT_STR_INIT("")
	};
	static const xstrview Invalid[] = {
		XRT_STR_INIT("[2001:db8::1"),
		XRT_STR_INIT("[2001:db8::1]tail"),
		XRT_STR_INIT("[v1.]"),
		XRT_STR_INIT("name%2Gexample"),
		XRT_STR_INIT("name:port"),
		XRT_STR_INIT("user@example")
	};
	xhttpparam Param = {
		XRT_STR_LITERAL("host"),
		{ NULL, 0 },
		XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
	};
	size_t i;

	for ( i = 0; i < (sizeof(Valid) / sizeof(Valid[0])); i++ ) {
		Param.Value = Valid[i];
		testRequire(xrtHttpParamHostValid(&Param),
			"HTTP parameter Host rejected valid syntax");
	}
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		Param.Value = Invalid[i];
		testRequire(!xrtHttpParamHostValid(&Param),
			"HTTP parameter Host accepted invalid syntax");
		xrtClearError();
	}
}



/* 验证公开 IP 谓词与 Host 使用相同的严格文本规则。 */
static void testHttpIpText(void)
{
	testRequire(
		xrtHttpIpv4Valid(XRT_STR_LITERAL("192.0.2.1")) &&
		!xrtHttpIpv4Valid(XRT_STR_LITERAL("192.00.2.1")),
		"HTTP IPv4 validation mismatch"
	);
	xrtClearError();
	testRequire(
		xrtHttpIpv6Valid(XRT_STR_LITERAL("2001:db8::1")) &&
		xrtHttpIpv6Valid(XRT_STR_LITERAL("::ffff:192.0.2.1")) &&
		!xrtHttpIpv6Valid(XRT_STR_LITERAL("2001:db8:::1")),
		"HTTP IPv6 validation mismatch"
	);
	xrtClearError();
}



int main(void)
{
	testHttpParamHost();
	testHttpIpText();
	printf("[PASS] http_param_host\n");
	return 0;
}
