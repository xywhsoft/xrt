#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件入口必须清空失败输出并发布稳定的参数错误。 */
int main(void)
{
	xhttpreply* pOutput = (xhttpreply*)(uintptr_t)1u;
	const xerror* pError;

	if ( (xrtHttpServerReplyCompress(
		NULL,
		NULL,
		NULL,
		&pOutput
	) != XHTTP_REPLY_COMPRESS_ERROR) ||
		(pOutput != NULL) ) {
		return 1;
	}
	pError = xrtGetError();
	if ( (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_ARGUMENT) ||
		(xrtErrorCode(pError) !=
		 (int32)XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT) ||
		(strcmp(
			xrtErrorDomain(pError),
			"http.reply.compress"
		 ) != 0) ) {
		return 2;
	}
	xrtClearError();
	puts("[PASS] single HTTP server compression");
	return 0;
}
