#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MIME
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证媒体类型解析和参数查询的裁剪闭包。 */
int main(void)
{
	xmediatype Type;
	xhttpparam Param;

	return xrtHttpMediaTypeParse(
		XRT_STR_LITERAL("application/json; charset=utf-8"),
		&Type
	) && (xrtHttpMediaTypeParam(
		&Type, XRT_STR_LITERAL("charset"), &Param
	) == XHTTP_NEXT_ITEM) ? 0 : 1;
}
