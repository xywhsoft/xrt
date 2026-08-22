#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件构建必须公开原始响应接口及其参数错误。 */
int main(void)
{
	xnetref Ref = { NULL, 0, NULL, NULL };

	return (xrtHttpConnRespondRaw(
		NULL,
		XRT_BYTES_LITERAL("HTTP/1.1 204 No Content\r\n\r\n"),
		XHTTP_SERVER_RAW_NONE
	) == XNET_RESULT_ERROR) &&
		(xrtHttpConnRespondRawRef(
			NULL,
			&Ref,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtHttpConnRespondRawTake(
			NULL,
			NULL,
			0,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) ?
			0 : 1;
}
