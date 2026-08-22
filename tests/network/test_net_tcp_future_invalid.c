#include "../test.h"



typedef struct testtcpfutureinvalid {
	xatomicptr Server;
	xatomic32 Accepted;
	xatomic32 Closed;
} testtcpfutureinvalid;



typedef struct testtcpfutureswitch {
	testtcpfutureinvalid* Context;
	xatomic32 Done;
	xatomic32 Matched;
	bool Push;
	bool Expected;
} testtcpfutureswitch;



/* 推送模式回调只用于确认拉取等待不能和同一读消费者并存。 */
static void testTcpFutureInvalidRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pData;
	(void)xrtNetBufConsume(pBuffer, xrtNetBufSize(pBuffer));
}



/* 接管服务端 Stream 并发布给测试线程。 */
static bool testTcpFutureInvalidAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpfutureinvalid* pContext = (testtcpfutureinvalid*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"invalid TCP Future accepted data setup failed");
	xrtAtomicPtrStore(&pContext->Server, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录正常关闭。 */
static void testTcpFutureInvalidClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfutureinvalid* pContext = (testtcpfutureinvalid*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"invalid TCP Future close mismatch");
	(void)xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 在截止时间前等待原子状态到达目标。 */
static void testTcpFutureInvalidWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 在 Stream Worker 上切换读消费者模式并记录预期结果。 */
static void testTcpFutureInvalidSwitch(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpfutureswitch* pSwitch = (testtcpfutureswitch*)pData;
	testtcpfutureinvalid* pContext = pSwitch->Context;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pContext->Server,
		XMEMORY_ACQUIRE
	);
	xnetstreamevents Events;
	bool bResult;

	(void)pWorker;
	memset(&Events, 0, sizeof(Events));
	Events.Close = testTcpFutureInvalidClose;
	if ( pSwitch->Push ) {
		Events.Read = testTcpFutureInvalidRead;
	}
	bResult = xrtNetStreamSetEvents(pStream, &Events, pContext);
	if ( (bResult == pSwitch->Expected) &&
		 (bResult || ((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_READ))) ) {
		xrtAtomic32Store(&pSwitch->Matched, 1, XMEMORY_RELEASE);
	}
	xrtClearError();
	xrtAtomic32Store(&pSwitch->Done, 1, XMEMORY_RELEASE);
}



/* 提交一次模式切换并等待 Worker 给出确定结果。 */
static void testTcpFutureInvalidSwitchWait(
	xnetengine* pEngine,
	testtcpfutureinvalid* pContext,
	bool bPush,
	bool bExpected,
	cstr sMessage
)
{
	testtcpfutureswitch Switch;

	memset(&Switch, 0, sizeof(Switch));
	Switch.Context = pContext;
	Switch.Push = bPush;
	Switch.Expected = bExpected;
	xrtAtomic32Init(&Switch.Done, 0);
	xrtAtomic32Init(&Switch.Matched, 0);
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTcpFutureInvalidSwitch,
		&Switch
	), sMessage);
	testTcpFutureInvalidWait(&Switch.Done, 1, sMessage);
	testRequire(xrtAtomic32Load(
		&Switch.Matched,
		XMEMORY_ACQUIRE
	) == 1, sMessage);
}



/* 验证空参数、非法等待类型、双读消费者和 Worker 边界。 */
int main(void)
{
	testtcpfutureinvalid Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xfuture* pWrite;
	xfuture* pReadMode;
	char iByte = 0;

	testRequire(xrtNetStreamWaitAsync(
		NULL,
		XNET_STREAM_WAIT_READ
	) == NULL, "TCP Future wait accepted NULL Stream");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"TCP Future NULL wait error mismatch");
	xrtClearError();
	testRequire(xrtNetStreamRecvAsync(NULL, 0) == NULL,
		"TCP Future receive accepted NULL Stream");
	xrtClearError();
	testRequire(xrtNetListenerAcceptAsync(NULL) == NULL,
		"TCP Future accept accepted NULL Listener");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"TCP Future NULL accept error mismatch");
	xrtClearError();

	memset(&Context, 0, sizeof(Context));
	xrtAtomicPtrInit(&Context.Server, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpFutureInvalidAccept;
	StreamEvents.Read = testTcpFutureInvalidRead;
	StreamEvents.Close = testTcpFutureInvalidClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"invalid TCP Future engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "invalid TCP Future listener address failed");
	ListenConfig.AcceptConcurrency = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"invalid TCP Future listener start failed");
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "invalid TCP Future connect failed");
	testTcpFutureInvalidWait(&Context.Accepted, 1,
		"invalid TCP Future accept callback missing");
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Server,
		XMEMORY_ACQUIRE
	);
	testRequire(pServer != NULL, "invalid TCP Future server missing");
	testRequire(xrtNetListenerAccept(pListener) == NULL,
		"TCP pull accept accepted a push-mode Listener");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_LISTENER_ACCEPT),
		"TCP push Listener pull error mismatch");
	xrtClearError();
	testRequire(xrtNetListenerAcceptAsync(pListener) == NULL,
		"TCP accept Future accepted a push-mode Listener");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_LISTENER_ACCEPT),
		"TCP push Listener Future error mismatch");
	xrtClearError();

	testRequire(xrtNetStreamWaitAsync(
		pServer,
		(xnetstreamwait)-1
	) == NULL, "TCP Future wait accepted a negative condition");
	xrtClearError();
	testRequire(xrtNetStreamWaitAsync(
		pServer,
		(xnetstreamwait)(XNET_STREAM_WAIT_CLOSE + 1)
	) == NULL, "TCP Future wait accepted an unknown condition");
	xrtClearError();
	testRequire(xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_READ
	) == NULL, "TCP Future read wait accepted a push-mode Stream");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_READ),
		"TCP Future push-mode wait error mismatch");
	xrtClearError();
	testRequire(xrtNetStreamRecvAsync(pServer, 1) == NULL,
		"TCP Future receive accepted a push-mode Stream");
	xrtClearError();
	pWrite = xrtNetStreamWaitAsync(pServer, XNET_STREAM_WAIT_WRITE);
	testRequire((pWrite != NULL) &&
		 (xrtFutureWaitFor(pWrite, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pWrite) == XFUTURE_RESOLVED),
		"push-mode Stream rejected a non-read wait");

	testTcpFutureInvalidSwitchWait(
		pEngine,
		&Context,
		false,
		true,
		"TCP Future could not switch to pull mode"
	);
	pReadMode = xrtNetStreamRecvAsync(pServer, 1);
	testRequire(pReadMode != NULL,
		"TCP Future pull-mode receive creation failed");
	testTcpFutureInvalidSwitchWait(
		pEngine,
		&Context,
		true,
		false,
		"TCP Future pending read allowed a push consumer"
	);
	testRequire(xrtFutureCancel(pReadMode) &&
		(xrtFutureWaitFor(pReadMode, 5000000u) == XWAIT_OK) &&
		(xrtFutureState(pReadMode) == XFUTURE_CANCELLED),
		"TCP Future read-mode cancellation failed");
	xrtFutureDestroy(pReadMode);
	testTcpFutureInvalidSwitchWait(
		pEngine,
		&Context,
		true,
		true,
		"TCP Future push mode did not recover after cancellation"
	);

	testRequire(xrtNetStreamBuffer(pServer) == NULL,
		"external thread borrowed a TCP pull buffer");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"external TCP buffer error mismatch");
	xrtClearError();
	testRequire(xrtNetStreamRead(pServer, &iByte, 1) == 0,
		"external thread read a TCP pull buffer");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"external TCP read error mismatch");
	xrtClearError();
	testRequire(xrtNetStreamConsume(pServer, 1) == 0,
		"external thread consumed a TCP pull buffer");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"external TCP consume error mismatch");
	xrtClearError();

	testRequire(xrtNetStreamClose(pClient) && xrtNetStreamClose(pServer),
		"invalid TCP Future close request failed");
	testTcpFutureInvalidWait(&Context.Closed, 2,
		"invalid TCP Future close callbacks missing");
	testRequire(xrtNetListenerClose(pListener),
		"invalid TCP Future listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtFutureDestroy(pWrite);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"invalid TCP Future engine destroy failed");
	printf("[PASS] network TCP Future invalid inputs\n");
	return 0;
}
