#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件包含会话契约、实现和 TLS 结构化错误路径。 */
int main(void)
{
	xrtTlsSessionDestroy(NULL);
	xrtClearError();
	if ( (xrtTlsSessionFeedBuffer(NULL, NULL) != XTLS_ERROR) ||
		(xrtGetError() == NULL) ||
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tls") != 0) ||
		(xrtErrorCode(xrtGetError()) != (int32)XTLS_ERROR_ARGUMENT) ||
		(XTLS_WAIT_INPUT == XTLS_WAIT_OUTPUT) ) {
		return 1;
	}
	printf("[PASS] single-tls-session\n");
	return 0;
}
