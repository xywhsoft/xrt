#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件包含 TLS-over-TCP 配置、状态和结构化错误契约。 */
int main(void)
{
	xtlsstreamconfig Config;

	xrtTlsStreamConfigInit(&Config);
	if ( (Config.HandshakeTimeout !=
			XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT) ||
		(Config.CloseTimeout != XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT) ) {
		return 1;
	}
	xrtClearError();
	if ( (xrtTlsStreamState(NULL) != XTLS_STREAM_FAILED) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ||
		(xrtErrorCode(xrtGetError()) != (int32)XTLS_ERROR_ARGUMENT) ) {
		return 2;
	}
	xrtClearError();
	if ( xrtTlsStreamSetEvents(NULL, NULL, NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 3;
	}
	xrtClearError();
	if ( xrtTlsStreamClient(
		NULL, NULL, NULL, NULL, NULL, NULL
	) || (xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 4;
	}
	printf("[PASS] single-tls-stream\n");
	return 0;
}
