#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_TCP_BACKEND)
	#define TEST_SINGLE_TCP_BACKEND XNET_PORT_SELECT
#endif



typedef struct testsingletcp {
	xnetstream* Client;
	xnetstream* Server;
	xatomic32 Accepted;
	xatomic32 ClientRead;
	xatomic32 Closed;
} testsingletcp;



/* 接管单头文件 Listener 发布的服务端 Stream。 */
static bool testSingleTcpAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsingletcp* pState = (testsingletcp*)pData;

	(void)pListener;
	(void)xrtNetStreamSetData(pStream, pState);
	pState->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 服务端回送数据，客户端记录完整接收。 */
static void testSingleTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testsingletcp* pState = (testsingletcp*)pData;
	char Data[16];
	size_t iSize;

	if ( pStream == pState->Server ) {
		(void)xrtNetStreamSendBuffer(pStream, pBuffer);
		return;
	}
	iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));
	if ( (iSize == 4) && (memcmp(Data, "echo", 4) == 0) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->ClientRead,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 记录两个 Stream 的唯一关闭回调。 */
static void testSingleTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsingletcp* pState = (testsingletcp*)pData;

	(void)pStream;
	if ( (Result == XNET_RESULT_OK) && (pError == NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->Closed,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 验证单头文件能够真实完成 TCP 回环收发。 */
int main(void)
{
	testsingletcp State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtClearError();
	if ( xrtNetStreamSetEvents(NULL, NULL, NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 11;
	}
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testSingleTcpAccept;
	StreamEvents.Read = testSingleTcpRead;
	StreamEvents.Close = testSingleTcpClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_SINGLE_TCP_BACKEND;
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
		&ListenerEvents,
		&StreamEvents,
		&State
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 2;
	}
	State.Client = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		&StreamEvents,
		&State
	);
	if ( State.Client == NULL ) {
		return 3;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtAtomic32Load(&State.Accepted, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 4;
		}
		xrtThreadYield();
	}
	if ( xrtNetStreamSend(State.Client, "echo", 4) != XNET_RESULT_OK ) {
		return 5;
	}
	while ( xrtAtomic32Load(&State.ClientRead, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 6;
		}
		xrtThreadYield();
	}
	if ( !xrtNetStreamClose(State.Client) ||
		 !xrtNetStreamClose(State.Server) ) {
		return 7;
	}
	while ( xrtAtomic32Load(&State.Closed, XMEMORY_ACQUIRE) != 2 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 8;
		}
		xrtThreadYield();
	}
	if ( !xrtNetListenerClose(pListener) ) {
		return 9;
	}
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(pListener);
	return xrtNetEngineDestroy(pEngine) ? 0 : 10;
}
