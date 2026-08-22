#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须公开 TLS Stream Future 默认值和参数拒绝契约。 */
int main(void)
{
	xtlsstreamconfig Config;
	xfuture* pFuture;

	xrtTlsStreamConfigInit(&Config);
	if ( (Config.AsyncBytesLimit !=
			XTLS_STREAM_ASYNC_BYTES_DEFAULT) ||
		(Config.AsyncCountLimit !=
			XTLS_STREAM_ASYNC_COUNT_DEFAULT) ||
		(Config.AsyncBatch !=
			XTLS_STREAM_ASYNC_BATCH_DEFAULT) ) {
		return 1;
	}
	xrtClearError();
	pFuture = xrtTlsStreamWaitAsync(
		NULL,
		XTLS_STREAM_WAIT_CLOSE
	);
	if ( (pFuture != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ||
		(xrtErrorCode(xrtGetError()) !=
		 (int32)XTLS_ERROR_ARGUMENT) ) {
		return 2;
	}
	xrtClearError();
	pFuture = xrtTlsStreamSendAsync(NULL, NULL, 0);
	if ( (pFuture != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 3;
	}
	xrtClearError();
	return 0;
}
