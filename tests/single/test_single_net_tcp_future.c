#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含拉取 Accept、连接等待和字节接收 Future。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xfuture* pAccept;
	xfuture* pOpen;
	xfuture* pReceive;
	xfuture* pClientClose;
	xfuture* pServerClose;
	xnetbytes* pBytes;
	xbytesview View;
	static const char sPayload[] = "single-future";

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	ListenConfig.AcceptConcurrency = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 2;
	}
	pAccept = xrtNetListenerAcceptAsync(pListener);
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	);
	if ( (pAccept == NULL) || (pClient == NULL) ) {
		return 3;
	}
	pOpen = xrtNetStreamWaitAsync(pClient, XNET_STREAM_WAIT_OPEN);
	if ( (pOpen == NULL) ||
		 (xrtFutureWaitFor(pAccept, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pAccept) != XFUTURE_RESOLVED) ||
		 (xrtFutureWaitFor(pOpen, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pOpen) != XFUTURE_RESOLVED) ) {
		return 4;
	}
	pServer = xrtNetStreamRef(
		(xnetstream*)xrtFutureValue(pAccept)
	);
	if ( pServer == NULL ) {
		return 5;
	}
	pReceive = xrtNetStreamRecvAsync(pServer, 0);
	if ( (pReceive == NULL) || (xrtNetStreamSend(
		pClient,
		sPayload,
		sizeof(sPayload) - 1u
	) != XNET_RESULT_OK) ) {
		return 6;
	}
	if ( (xrtFutureWaitFor(pReceive, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pReceive) != XFUTURE_RESOLVED) ) {
		return 7;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pReceive);
	View = xrtNetBytesView(pBytes);
	if ( (pBytes == NULL) || (View.Size != (sizeof(sPayload) - 1u)) ||
		 (memcmp(View.Data, sPayload, View.Size) != 0) ) {
		return 8;
	}
	pClientClose = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_CLOSE
	);
	pServerClose = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_CLOSE
	);
	if ( (pClientClose == NULL) || (pServerClose == NULL) ||
		 !xrtNetStreamClose(pClient) || !xrtNetStreamClose(pServer) ) {
		return 9;
	}
	if ( (xrtFutureWaitFor(pClientClose, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pClientClose) != XFUTURE_RESOLVED) ||
		 (xrtFutureWaitFor(pServerClose, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pServerClose) != XFUTURE_RESOLVED) ) {
		return 10;
	}
	if ( !xrtNetListenerClose(pListener) ) {
		return 11;
	}
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtFutureDestroy(pAccept);
	xrtFutureDestroy(pOpen);
	xrtFutureDestroy(pReceive);
	xrtFutureDestroy(pClientClose);
	xrtFutureDestroy(pServerClose);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	return xrtNetEngineDestroy(pEngine) ? 0 : 12;
}
