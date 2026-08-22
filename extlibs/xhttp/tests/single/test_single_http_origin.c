#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Origin 解析和默认端口比较。 */
int main(void)
{
	xhttporigin Left;
	xhttporigin Right;

	return xrtHttpOriginParse(
		XRT_STR_LITERAL("https://example.test:443"), &Left
	) && xrtHttpOriginParse(
		XRT_STR_LITERAL("HTTPS://EXAMPLE.TEST"), &Right
	) && xrtHttpOriginSame(&Left, &Right) ? 0 : 1;
}
