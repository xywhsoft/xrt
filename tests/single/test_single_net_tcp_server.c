#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct test_single_tcp_server {
	xnetstream* Client;
	xnetstream* Accepted;
	xatomic32 AcceptedCount;
	xatomic32 Echoed;
	xatomic32 StreamsClosed;
	xatomic32 ServerClosed;
} test_single_tcp_server;



/* 接管聚合 Server 交付的 Stream 引用。 */
static bool testSingleTcpServerAccept(
	xnetserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream,
	ptr pData
)
{
	test_single_tcp_server* pState =
		(test_single_tcp_server*)pData;

	(void)pServer;
	if ( iEndpoint != 0 ) {
		return false;
	}
	pState->Accepted = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->AcceptedCount,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 服务端移动接收缓冲，客户端确认回显内容。 */
static void testSingleTcpServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_single_tcp_server* pState =
		(test_single_tcp_server*)pData;
	char Data[8];
	size_t iSize;

	if ( pStream == pState->Accepted ) {
		(void)xrtNetStreamSendBuffer(pStream, pBuffer);
		return;
	}
	iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));
	if ( (iSize == 4) && (memcmp(Data, "echo", 4) == 0) ) {
		xrtAtomic32Store(&pState->Echoed, 1, XMEMORY_RELEASE);
	}
}



/* 记录两个 Stream 的唯一终态。 */
static void testSingleTcpServerStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_single_tcp_server* pState =
		(test_single_tcp_server*)pData;

	(void)pStream;
	if ( (Result == XNET_RESULT_OK) && (pError == NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->StreamsClosed,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 记录聚合 Server 的唯一关闭事件。 */
static void testSingleTcpServerClose(
	xnetserver* pServer,
	ptr pData
)
{
	test_single_tcp_server* pState =
		(test_single_tcp_server*)pData;

	if ( xrtNetServerState(pServer) == XNET_SERVER_CLOSED ) {
		xrtAtomic32Store(
			&pState->ServerClosed,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 验证单头文件中的聚合 Server 能完成真实回环收发和关闭。 */
int main(void)
{
	test_single_tcp_server State;
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverevents ServerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetaddr Address;
	xdeadline Deadline;

	memset(&State, 0, sizeof(State));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ServerEvents.Accept = testSingleTcpServerAccept;
	ServerEvents.Close = testSingleTcpServerClose;
	StreamEvents.Read = testSingleTcpServerRead;
	StreamEvents.Close = testSingleTcpServerStreamClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetServerConfigInit(&ServerConfig);
	(void)xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	);
	ServerConfig.Listen.AcceptConcurrency = 1;
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents,
		&StreamEvents,
		&State
	);
	if ( (pServer == NULL) ||
		!xrtNetServerLocal(pServer, 0, &Address) ) {
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
	Deadline = xrtDeadlineAfter(3000000u);
	while ( xrtAtomic32Load(
		&State.AcceptedCount,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return 4;
		}
		xrtThreadYield();
	}
	if ( xrtNetStreamSend(
		State.Client,
		"echo",
		4
	) != XNET_RESULT_OK ) {
		return 5;
	}
	while ( xrtAtomic32Load(
		&State.Echoed,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return 6;
		}
		xrtThreadYield();
	}
	if ( !xrtNetStreamClose(State.Client) ||
		!xrtNetStreamClose(State.Accepted) ||
		!xrtNetServerClose(pServer) ) {
		return 7;
	}
	while ( (xrtAtomic32Load(
		&State.StreamsClosed,
		XMEMORY_ACQUIRE
	) != 2) || !xrtAtomic32Load(
		&State.ServerClosed,
		XMEMORY_ACQUIRE
	) ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return 8;
		}
		xrtThreadYield();
	}
	xrtNetStreamDestroy(State.Client);
	xrtNetStreamDestroy(State.Accepted);
	xrtNetServerDestroy(pServer);
	return xrtNetEngineDestroy(pEngine) ? 0 : 9;
}
