#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 使用现有 Engine 完成一次本地阻塞 Accept 和接收。 */
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
	bool bReceived;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
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
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) || !xrtNetListenerLocal(pListener, &Address) ) {
		return 2;
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
		 (xrtNetStreamSend(pClient, "hello", 5) != XNET_RESULT_OK) ) {
		return 3;
	}
	pBytes = xrtNetStreamRecv(
		pServer,
		0,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	View = xrtNetBytesView(pBytes);
	bReceived = pBytes != NULL;
	if ( pBytes != NULL ) {
		printf("received: %.*s\n", (int)View.Size, (cstr)View.Data);
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
	return xrtNetEngineDestroy(pEngine) && bReceived ? 0 : 4;
}
