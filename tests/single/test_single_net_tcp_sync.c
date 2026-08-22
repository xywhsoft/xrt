#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含无隐藏 Engine 的 TCP 阻塞便利层。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetbytes* pBytes;
	xbytesview View;
	xnetaddr Address;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetListenConfigInit(&ListenConfig);
	if ( !xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		return 2;
	}
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 3;
	}
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	if ( (pClient == NULL) || (pServer == NULL) ||
		 !xrtNetStreamWait(
			pClient,
			XNET_STREAM_WAIT_OPEN,
			xrtDeadlineAfter(3000000u),
			NULL
		 ) || (xrtNetStreamSend(
			pClient,
			"single-sync",
			11
		 ) != XNET_RESULT_OK) ) {
		return 4;
	}
	pBytes = xrtNetStreamRecv(
		pServer,
		0,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	View = xrtNetBytesView(pBytes);
	if ( (pBytes == NULL) || (View.Size != 11) ||
		 (memcmp(View.Data, "single-sync", 11) != 0) ) {
		return 5;
	}
	xrtNetBytesDestroy(pBytes);
	(void)xrtNetStreamAbort(pClient);
	(void)xrtNetStreamAbort(pServer);
	(void)xrtNetListenerClose(pListener);
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ||
		 (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	return xrtNetEngineDestroy(pEngine) ? 0 : 6;
}
