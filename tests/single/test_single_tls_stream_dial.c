#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含受管 TLS Dial 的配置和参数错误契约。 */
int main(void)
{
	xtlsdialconfig Config;

	xrtTlsDialConfigInit(&Config);
	if ( !Config.ServerNameFromHost ||
		(Config.Transport.MaxAttempts == 0) ||
		(Config.Stream.HandshakeTimeout !=
		 XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT) ) {
		return 1;
	}
	xrtClearError();
	if ( xrtTlsDialCancel(NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	return 0;
}
