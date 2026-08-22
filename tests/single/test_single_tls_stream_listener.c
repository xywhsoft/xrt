#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须公开 Listener 默认值和严格参数契约。 */
int main(void)
{
	xtlslistenerconfig Config;
	xtlslistenerstats Stats;

	xrtTlsListenerConfigInit(&Config);
	if ( (Config.AcceptQueueLimit == 0) ||
		(Config.HandshakeLimit == 0) ||
		(Config.Listen.AcceptConcurrency == 0) ) {
		return 1;
	}
	xrtClearError();
	if ( (xrtTlsListenerAccept(NULL) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	xrtClearError();
	if ( xrtTlsListenerStats(NULL, &Stats) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 3;
	}
	return 0;
}
