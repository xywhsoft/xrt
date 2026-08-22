#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_MIME_TYPES
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证内置扩展名表可在独立裁剪配置中使用。 */
int main(void)
{
	xstrview Type = xrtMimeByPath(XRT_STR_LITERAL("asset/app.js"));

	return (Type.Size == 30u) && (memcmp(
		Type.Data, "text/javascript; charset=utf-8", 30u
	) == 0) ? 0 : 1;
}
