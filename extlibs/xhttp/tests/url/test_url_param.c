#include "../test.h"



/* 验证 quoted-string 参数的 URL 语义与连续 URL 解析保持一致。 */
static void testUrlParamParity(void)
{
	static const cstr Valid[] = {
		"",
		"../a",
		"mailto:user@example.com",
		"https://user@[2001:db8::1]:443/a%20b?q=?#f",
		"//[vF.a:b]:9/path",
		"?#"
	};
	static const cstr Invalid[] = {
		"a b",
		"a%2",
		"a%GG",
		"1a:b",
		"http://a@b@c/",
		"http://host:abc/",
		"http://[::1/",
		"//host/path#one#two"
	};
	xhttpparam Param;
	xurl Url;
	size_t i;

	memset(&Param, 0, sizeof(Param));
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	for ( i = 0; i < (sizeof(Valid) / sizeof(Valid[0])); i++ ) {
		Param.Value = (xstrview){ Valid[i], strlen(Valid[i]) };
		testRequire(xrtUrlParse(Param.Value, &Url) &&
			xrtUrlParamValid(&Param),
			"URL parameter rejected valid URI-reference");
	}
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		Param.Value = (xstrview){ Invalid[i], strlen(Invalid[i]) };
		testRequire(!xrtUrlParse(Param.Value, &Url) &&
			!xrtUrlParamValid(&Param),
			"URL parameter accepted invalid URI-reference");
		xrtClearError();
	}
}



/* 验证 quoted-pair 分隔符按解码后的 URI 语义参与解析。 */
static void testUrlParamQuotedPair(void)
{
	xhttpparam Param;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL(
		"https:\\/\\/example.test\\/a%20b?x=1#f"
	);
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	testRequire(xrtUrlParamValid(&Param),
		"URL parameter rejected escaped URI delimiters");

	Param.Value = XRT_STR_LITERAL("http:\\/\\/a@b@c\\/");
	testRequire(!xrtUrlParamValid(&Param),
		"URL parameter accepted escaped duplicate userinfo delimiter");
	xrtClearError();

	Param.Value = XRT_STR_LITERAL("relative");
	Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(xrtUrlParamValid(&Param),
		"URL parameter rejected token URI-reference");
	Param.Flags = XHTTP_PARAM_NONE;
	testRequire(!xrtUrlParamValid(&Param) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"URL parameter accepted omitted parameter value");
	xrtClearError();
}



/* 执行 HTTP 参数 URI-reference 验证测试。 */
int main(void)
{
	testUrlParamParity();
	testUrlParamQuotedPair();
	printf("[PASS] url_param\n");
	return 0;
}
