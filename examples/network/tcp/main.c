#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampletcp {
	xnetstream* Client;
	xatomicptr Server;
	xatomic32 Accepted;
	xatomic32 Reply;
	xatomic32 Closed;
} exampletcp;



/* 接管服务端连接并设置回调数据。 */
static bool exampleAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	exampletcp* pExample = (exampletcp*)pData;

	(void)pListener;
	(void)xrtNetStreamSetData(pStream, pExample);
	xrtAtomicPtrStore(
		&pExample->Server,
		pStream,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic32FetchAdd(
		&pExample->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 服务端直接回送字节，客户端输出回复。 */
static void exampleRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	exampletcp* pExample = (exampletcp*)pData;
	xnetstream* pServer = (xnetstream*)xrtAtomicPtrLoad(
		&pExample->Server,
		XMEMORY_ACQUIRE
	);
	char Data[64];
	size_t iSize;

	if ( pStream == pServer ) {
		(void)xrtNetStreamSendBuffer(pStream, pBuffer);
		return;
	}
	iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));
	printf("reply: %.*s\n", (int)iSize, Data);
	(void)xrtAtomic32FetchAdd(
		&pExample->Reply,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录正常关闭。 */
static void exampleClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	exampletcp* pExample = (exampletcp*)pData;

	(void)pStream;
	if ( (Result == XNET_RESULT_OK) && (pError == NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pExample->Closed,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 建立回环服务器和客户端，发送一次请求后完整回收。 */
int main(void)
{
	exampletcp Example;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pServer;
	xnetaddr Address;
	xdeadline iDeadline;
	str sEndpoint;
	int iResult = 1;

	memset(&Example, 0, sizeof(Example));
	xrtAtomicPtrInit(&Example.Server, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = exampleAccept;
	StreamEvents.Read = exampleRead;
	StreamEvents.Close = exampleClose;
	pEngine = NULL;
	pListener = NULL;
	pServer = NULL;
	sEndpoint = NULL;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Example
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		iResult = 3;
		goto Cleanup;
	}
	printf("listening on %s\n", sEndpoint);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	Example.Client = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		&StreamEvents,
		&Example
	);
	if ( Example.Client == NULL ) {
		iResult = 4;
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtAtomic32Load(&Example.Accepted, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 5;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Example.Server,
		XMEMORY_ACQUIRE
	);
	if ( pServer == NULL ) {
		iResult = 5;
		goto Cleanup;
	}
	if ( xrtNetStreamSend(
		Example.Client,
		"hello TCP",
		9
	) != XNET_RESULT_OK ) {
		iResult = 6;
		goto Cleanup;
	}
	while ( xrtAtomic32Load(&Example.Reply, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 7;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	(void)xrtNetStreamClose(Example.Client);
	(void)xrtNetStreamClose(pServer);
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtAtomic32Load(&Example.Closed, XMEMORY_ACQUIRE) != 2 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 8;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	(void)xrtNetListenerClose(pListener);
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 8;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	iResult = 0;

Cleanup:
	xrtFree(sEndpoint);
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Example.Server,
		XMEMORY_ACQUIRE
	);
	if ( (Example.Client != NULL) &&
		 (xrtNetStreamState(Example.Client) != XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(Example.Client);
	}
	if ( (pServer != NULL) &&
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ) {
		(void)xrtNetStreamAbort(pServer);
	}
	if ( (pListener != NULL) &&
		 (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) ) {
		(void)xrtNetListenerClose(pListener);
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( ((Example.Client != NULL) &&
		  (xrtNetStreamState(Example.Client) != XNET_STREAM_CLOSED)) ||
		 ((pServer != NULL) &&
		  (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED)) ||
		 ((pListener != NULL) &&
		  (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED)) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			if ( iResult == 0 ) {
				iResult = 8;
			}
			break;
		}
		xrtThreadYield();
	}
	xrtNetStreamDestroy(Example.Client);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 8;
	}
	return iResult;
}
