#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 等待字节 Future 并返回由 Future 持有的只读视图。 */
static xnetbytes* exampleTcpFutureBytes(xfuture* pFuture)
{
	if ( (pFuture == NULL) ||
		 (xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return NULL;
	}
	return (xnetbytes*)xrtFutureValue(pFuture);
}



/* 使用拉取 Accept 和同一个 Future 契约完成一次请求响应。 */
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
	xfuture* pReadable;
	xfuture* pDrain;
	xfuture* pRequest;
	xfuture* pReply;
	xfuture* pClientClose;
	xfuture* pServerClose;
	xnetbytes* pBytes;
	xbytesview View;
	static const char sRequest[] = "hello future TCP";
	static const char sReply[] = "received";

	xrtNetEngineConfigInit(&EngineConfig);
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

	/* 先登记水平可读条件；数据到达后仍保留在 Stream 缓冲中。 */
	pReadable = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_READ
	);
	if ( (pReadable == NULL) || (xrtNetStreamSend(
		pClient,
		sRequest,
		sizeof(sRequest) - 1u
	) != XNET_RESULT_OK) ) {
		return 6;
	}

	/* Drain 表示发送预算已经归零，不代表对端已经消费应用字节。 */
	pDrain = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_DRAIN
	);
	if ( (pDrain == NULL) ||
		(xrtFutureWaitFor(pReadable, 3000000u) != XWAIT_OK) ||
		(xrtFutureState(pReadable) != XFUTURE_RESOLVED) ||
		(xrtFutureWaitFor(pDrain, 3000000u) != XWAIT_OK) ||
		(xrtFutureState(pDrain) != XFUTURE_RESOLVED) ) {
		return 6;
	}

	/* 可读条件完成后，再用字节 Future 消费当前可用前缀。 */
	pRequest = xrtNetStreamRecvAsync(pServer, 0);
	pBytes = exampleTcpFutureBytes(pRequest);
	if ( pBytes == NULL ) {
		return 7;
	}
	View = xrtNetBytesView(pBytes);
	printf("server received: %.*s\n",
		(int)View.Size, (const char*)View.Data);

	pReply = xrtNetStreamRecvAsync(pClient, 0);
	if ( (pReply == NULL) || (xrtNetStreamSend(
		pServer,
		sReply,
		sizeof(sReply) - 1u
	) != XNET_RESULT_OK) ) {
		return 8;
	}
	pBytes = exampleTcpFutureBytes(pReply);
	if ( pBytes == NULL ) {
		return 9;
	}
	View = xrtNetBytesView(pBytes);
	printf("client received: %.*s\n",
		(int)View.Size, (const char*)View.Data);

	pClientClose = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_CLOSE
	);
	pServerClose = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_CLOSE
	);
	if ( (pClientClose == NULL) || (pServerClose == NULL) ||
		 !xrtNetStreamClose(pClient) || !xrtNetStreamClose(pServer) ||
		 (xrtFutureWaitFor(pClientClose, 3000000u) != XWAIT_OK) ||
		 (xrtFutureWaitFor(pServerClose, 3000000u) != XWAIT_OK) ) {
		return 10;
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtFutureDestroy(pAccept);
	xrtFutureDestroy(pOpen);
	xrtFutureDestroy(pReadable);
	xrtFutureDestroy(pDrain);
	xrtFutureDestroy(pRequest);
	xrtFutureDestroy(pReply);
	xrtFutureDestroy(pClientClose);
	xrtFutureDestroy(pServerClose);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	return xrtNetEngineDestroy(pEngine) ? 0 : 11;
}
