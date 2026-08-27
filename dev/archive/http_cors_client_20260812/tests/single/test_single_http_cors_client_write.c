#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 CORS 客户端写出模块可单头文件独立使用。 */
int main(void)
{
	xhttporigin Origin;
	size_t iSize;

	return xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origin
	) && xrtHttpCorsPreflightFieldsWrite(
		&Origin,
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		NULL,
		0,
		&iSize
	) && (iSize != 0) ? 0 : 1;
}
