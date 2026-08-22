#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct example_tcp_server_wait {
	xnetengine* Engine;
	xnetserver* Server;
	xnetstream* ClientFuture;
	xnetstream* AcceptedFuture;
	xnetstream* ClientSync;
	xnetstream* AcceptedSync;
	xnetaddr Local;
} example_tcp_server_wait;



/* 启动使用动态回环端口的拉取式 TCP Server。 */
static bool exampleTcpServerWaitStart(example_tcp_server_wait* pState)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;

	memset(pState, 0, sizeof(*pState));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2u;
	pState->Engine = xrtNetEngineCreate(&EngineConfig);
	if ( (pState->Engine == NULL) ||
		!xrtNetEngineStart(pState->Engine) ) {
		return false;
	}
	xrtNetServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		return false;
	}
	pState->Server = xrtNetServerStart(
		pState->Engine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	return (pState->Server != NULL) && xrtNetServerLocal(
		pState->Server,
		0,
		&pState->Local
	);
}



/* 连接刚启动的本地 Server。 */
static xnetstream* exampleTcpServerWaitConnect(
	example_tcp_server_wait* pState
)
{
	return xrtNetStreamConnect(
		pState->Engine,
		&pState->Local,
		1u,
		NULL,
		NULL,
		NULL
	);
}



/* 终止连接、关闭 Server，并等待 Engine 中的对象全部退出。 */
static bool exampleTcpServerWaitStop(example_tcp_server_wait* pState)
{
	xnetstream* Streams[] = {
		pState->ClientFuture,
		pState->AcceptedFuture,
		pState->ClientSync,
		pState->AcceptedSync
	};
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	bool bClosed = true;
	size_t i;

	for ( i = 0; i < (sizeof(Streams) / sizeof(Streams[0])); i++ ) {
		if ( Streams[i] != NULL ) {
			(void)xrtNetStreamAbort(Streams[i]);
		}
	}
	if ( pState->Server != NULL ) {
		(void)xrtNetServerClose(pState->Server);
	}
	for ( ;; ) {
		bool bDone = (pState->Server == NULL) ||
			(xrtNetServerState(pState->Server) == XNET_SERVER_CLOSED);

		for ( i = 0; i < (sizeof(Streams) / sizeof(Streams[0])); i++ ) {
			bDone = bDone && ((Streams[i] == NULL) ||
				(xrtNetStreamState(Streams[i]) == XNET_STREAM_CLOSED));
		}
		if ( bDone ) {
			break;
		}
		if ( xrtDeadlineExpired(Deadline) ) {
			bClosed = false;
			break;
		}
		xrtThreadYield();
	}
	for ( i = 0; i < (sizeof(Streams) / sizeof(Streams[0])); i++ ) {
		xrtNetStreamDestroy(Streams[i]);
	}
	xrtNetServerDestroy(pState->Server);
	if ( (pState->Engine != NULL) &&
		!xrtNetEngineDestroy(pState->Engine) ) {
		bClosed = false;
	}
	return bClosed;
}



/* 依次演示 Future Accept 和非 Worker 线程上的阻塞 Accept。 */
int main(void)
{
	example_tcp_server_wait State;
	xfutureresult Result;
	xfuture* pAccept = NULL;
	int iResult = 1;

	if ( !exampleTcpServerWaitStart(&State) ) {
		goto Cleanup;
	}
	pAccept = xrtNetServerAcceptAsync(State.Server);
	State.ClientFuture = exampleTcpServerWaitConnect(&State);
	if ( (pAccept == NULL) || (State.ClientFuture == NULL) ||
		(xrtFutureWaitFor(
			pAccept,
			UINT64_C(5000000)
		 ) != XWAIT_OK) ||
		!xrtFutureResult(pAccept, &Result) ||
		(Result.State != XFUTURE_RESOLVED) ) {
		goto Cleanup;
	}
	State.AcceptedFuture = xrtNetStreamRef(
		(xnetstream*)Result.Value
	);
	xrtFutureDestroy(pAccept);
	pAccept = NULL;
	if ( State.AcceptedFuture == NULL ) {
		goto Cleanup;
	}

	State.ClientSync = exampleTcpServerWaitConnect(&State);
	if ( State.ClientSync == NULL ) {
		goto Cleanup;
	}
	State.AcceptedSync = xrtNetServerAcceptWait(
		State.Server,
		xrtDeadlineAfter(UINT64_C(5000000)),
		NULL
	);
	if ( State.AcceptedSync == NULL ) {
		goto Cleanup;
	}
	printf(
		"accepted Future and synchronous connections on port %u\n",
		(unsigned)State.Local.Port
	);
	iResult = 0;

Cleanup:
	if ( (pAccept != NULL) &&
		(xrtFutureState(pAccept) == XFUTURE_PENDING) ) {
		(void)xrtFutureCancel(pAccept);
		(void)xrtFutureWaitFor(pAccept, UINT64_C(5000000));
	}
	xrtFutureDestroy(pAccept);
	if ( !exampleTcpServerWaitStop(&State) && (iResult == 0) ) {
		iResult = 2;
	}
	return iResult;
}
