#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_EXT_VALUE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 RFC 8187 扩展值的解析与解码裁剪闭包。 */
int main(void)
{
	xhttpextvalue Value;
	char Output[32];
	size_t iSize = 0;

	return xrtHttpExtValueParse(
		XRT_STR_LITERAL("UTF-8'zh-CN'hello%20xrt"), &Value
	) && xrtHttpExtValueRead(
		&Value, Output, sizeof(Output), &iSize
	) && (iSize == 9u) &&
		(memcmp(Output, "hello xrt", 9u) == 0) ? 0 : 1;
}
