#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_URL_PARAM
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 HTTP 参数中的流式 URI-reference 验证。 */
int main(void)
{
	xhttpparam Param;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL("https://example.test/a?q=1#f");
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	return xrtUrlParamValid(&Param) ? 0 : 1;
}
