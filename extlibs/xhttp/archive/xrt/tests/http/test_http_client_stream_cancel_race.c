#include "../test.h"



#if !defined(TEST_HTTP_CLIENT_STREAM_BACKEND)
	#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "select"
#endif

#define TEST_HTTP_CLIENT_STREAM_RACE_ITERATIONS 96
#define TEST_HTTP_CLIENT_STREAM_CANCEL_THREADS 4



typedef enum test_http_client_stream_race_mode {
	TEST_HTTP_CLIENT_STREAM_RESPONSE_FIRST = 0,
	TEST_HTTP_CLIENT_STREAM_CANCEL_FIRST,
	TEST_HTTP_CLIENT_STREAM_SIMULTANEOUS,
	TEST_HTTP_CLIENT_STREAM_CLOSE_FIRST
} test_http_client_stream_race_mode;



typedef struct test_http_client_stream_race {
	xnetengine* Engine;
	xnetsocket Listener;
	xnetaddr Address;
	xhttp1exchange* Exchange;
	xatomicptr Call;
	xatomic32 Iteration;
	xatomic32 Started;
	xatomic32 RequestReady;
	xatomic32 Race;
	xatomic32 Mode;
	xatomic32 CancelDone;
	xatomic32 CancelAccepted;
	xatomic32 ForcedCancel;
	xatomic32 Completed;
	xhttp1callstate FinalState;
	xnetresult FinalResult;
	uint32 Successes;
	uint32 Failures;
	uint32 Cancellations;
} test_http_client_stream_race;



/* 在截止时间前等待单调递增的测试阶段。 */
static void testHttpClientStreamRaceWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 创建方与完成回调竞争发布同一 Call，并校验两侧观察到的身份一致。 */
static void testHttpClientStreamRacePublishCall(
	xatomicptr* pTarget,
	xhttp1call* pCall
)
{
	ptr pExpected = NULL;

	if ( !xrtAtomicPtrCompareExchange(
		pTarget,
		&pExpected,
		pCall,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		testRequire(
			pExpected == pCall,
			"HTTP cancel race callback identity mismatch"
		);
	}
}



/* 判断阻塞源站是否已经收到完整的 HTTP Header。 */
static bool testHttpClientStreamRaceHeaderComplete(
	const char* pData,
	size_t iSize
)
{
	size_t i;

	for ( i = 3; i < iSize; ++i ) {
		if ( (pData[i - 3] == '\r') &&
			(pData[i - 2] == '\n') &&
			(pData[i - 1] == '\r') &&
			(pData[i] == '\n') ) {
			return true;
		}
	}
	return false;
}



/* 尽力写完整响应；取消先提交时，对端关闭属于预期结果。 */
static void testHttpClientStreamRaceRespond(xnetsocket Socket)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	size_t iOffset = 0;

	while ( iOffset < (sizeof(Response) - 1u) ) {
		size_t iSent = 0;

		if ( (xrtNetSocketSend(
			Socket,
			Response + iOffset,
			(sizeof(Response) - 1u) - iOffset,
			&iSent
		) != XNET_RESULT_OK) || (iSent == 0) ) {
			return;
		}
		iOffset += iSent;
	}
}



/* 逐连接接收请求，并按四种时序参与成功、失败与取消竞争。 */
static int32 testHttpClientStreamRaceServer(ptr pData)
{
	test_http_client_stream_race* pState =
		(test_http_client_stream_race*)pData;
	uint32 iIteration;

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_STREAM_RACE_ITERATIONS;
		++iIteration ) {
		xnetsocket Accepted = NULL;
		char Request[2048];
		size_t iUsed = 0;
		uint32 iMode;

		testRequire(
			xrtNetSocketAccept(
				pState->Listener,
				&Accepted,
				NULL
			) == XNET_RESULT_OK,
			"HTTP cancel race accept failed"
		);
		while ( !testHttpClientStreamRaceHeaderComplete(
			Request,
			iUsed
		) ) {
			size_t iReceived = 0;

			testRequire(iUsed < sizeof(Request),
				"HTTP cancel race request exceeded fixture");
			testRequire(
				(xrtNetSocketRecv(
					Accepted,
					Request + iUsed,
					sizeof(Request) - iUsed,
					&iReceived
				) == XNET_RESULT_OK) &&
				(iReceived != 0),
				"HTTP cancel race request receive failed"
			);
			iUsed += iReceived;
		}

		iMode = xrtAtomic32Load(
			&pState->Mode,
			XMEMORY_ACQUIRE
		);
		if ( iMode == TEST_HTTP_CLIENT_STREAM_RESPONSE_FIRST ) {
			testHttpClientStreamRaceRespond(Accepted);
			xrtAtomic32Store(
				&pState->RequestReady,
				iIteration,
				XMEMORY_RELEASE
			);
		} else if ( iMode ==
			TEST_HTTP_CLIENT_STREAM_CLOSE_FIRST ) {
			(void)xrtNetSocketClose(Accepted);
			Accepted = NULL;
			xrtAtomic32Store(
				&pState->RequestReady,
				iIteration,
				XMEMORY_RELEASE
			);
		} else {
			xrtAtomic32Store(
				&pState->RequestReady,
				iIteration,
				XMEMORY_RELEASE
			);
			testHttpClientStreamRaceWait(
				&pState->Race,
				iIteration,
				"HTTP cancel race was not released"
			);
			if ( iMode ==
				TEST_HTTP_CLIENT_STREAM_CANCEL_FIRST ) {
				testHttpClientStreamRaceWait(
					&pState->ForcedCancel,
					iIteration,
					"HTTP forced cancellation was not accepted"
				);
			}
			if ( iMode ==
				TEST_HTTP_CLIENT_STREAM_SIMULTANEOUS ) {
				testHttpClientStreamRaceRespond(Accepted);
			}
		}
		if ( Accepted != NULL ) {
			(void)xrtNetSocketClose(Accepted);
		}
	}
	return 0;
}



/* 所有取消线程持续竞争同一轮调用，只有一个线程可以被接纳。 */
static int32 testHttpClientStreamRaceCancel(ptr pData)
{
	test_http_client_stream_race* pState =
		(test_http_client_stream_race*)pData;
	uint32 iIteration;

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_STREAM_RACE_ITERATIONS;
		++iIteration ) {
		xhttp1call* pCall;
		bool bAccepted;

		testHttpClientStreamRaceWait(
			&pState->Race,
			iIteration,
			"HTTP cancel worker was not released"
		);
		pCall = (xhttp1call*)xrtAtomicPtrLoad(
			&pState->Call,
			XMEMORY_ACQUIRE
		);
		testRequire(pCall != NULL,
			"HTTP cancel worker observed no call");
		bAccepted = xrtHttp1CallCancel(pCall);
		if ( bAccepted ) {
			(void)xrtAtomic32FetchAdd(
				&pState->CancelAccepted,
				1,
				XMEMORY_ACQ_REL
			);
			if ( xrtAtomic32Load(
				&pState->Mode,
				XMEMORY_ACQUIRE
			) == TEST_HTTP_CLIENT_STREAM_CANCEL_FIRST ) {
				xrtAtomic32Store(
					&pState->ForcedCancel,
					iIteration,
					XMEMORY_RELEASE
				);
			}
		}
		(void)xrtAtomic32FetchAdd(
			&pState->CancelDone,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return 0;
}



/* 接收终态并立即释放回调入口转移的成功结果。 */
static void testHttpClientStreamRaceDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	test_http_client_stream_race* pState =
		(test_http_client_stream_race*)pData;
	uint32 iIteration = xrtAtomic32Load(
		&pState->Iteration,
		XMEMORY_ACQUIRE
	);

	testHttpClientStreamRacePublishCall(&pState->Call, pCall);
	testRequire(pResult != NULL,
		"HTTP cancel race result is null");
	pState->FinalState = xrtHttp1CallState(pCall);
	pState->FinalResult = pResult->Result;
	if ( pResult->Response != NULL ) {
		xrtHttpResponseDestroy(pResult->Response);
	}
	if ( pResult->Tcp != NULL ) {
		(void)xrtNetStreamAbort(pResult->Tcp);
		xrtNetStreamDestroy(pResult->Tcp);
	}
	xrtAtomic32Store(
		&pState->Completed,
		iIteration,
		XMEMORY_RELEASE
	);
}



/* 在连接所属 Worker 上把 Stream 与 Exchange 交给低级调用驱动器。 */
static void testHttpClientStreamRaceOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client_stream_race* pState =
		(test_http_client_stream_race*)pData;
	xhttp1callevents Events;
	xhttp1call* pCall;
	uint32 iIteration = xrtAtomic32Load(
		&pState->Iteration,
		XMEMORY_ACQUIRE
	);

	xrtHttp1CallEventsInit(&Events);
	Events.Done = testHttpClientStreamRaceDone;
	Events.Data = pState;
	pCall = xrtHttp1CallTcp(
		pStream,
		pState->Exchange,
		NULL,
		&Events
	);
	testRequire(pCall != NULL,
		"HTTP cancel race call creation failed");
	pState->Exchange = NULL;
	testHttpClientStreamRacePublishCall(&pState->Call, pCall);
	xrtAtomic32Store(
		&pState->Started,
		iIteration,
		XMEMORY_RELEASE
	);
}



/* 为单轮竞争创建不带正文的 HTTP/1 Exchange。 */
static xhttp1exchange* testHttpClientStreamRaceExchange(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://127.0.0.1/race")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan,
		NULL,
		NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 建立绑定到动态回环端口的阻塞测试源站。 */
static xnetsocket testHttpClientStreamRaceListener(
	xnetaddr* pAddress
)
{
	xnetsocket Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(Listener != NULL,
		"HTTP cancel race listener open failed");
	testRequire(
		xrtNetSocketSet(
			Listener,
			XNET_OPTION_REUSE_ADDRESS,
			1
		),
		"HTTP cancel race reuse-address failed"
	);
	testRequire(
		xrtNetAddrLoopback(
			pAddress,
			XNET_FAMILY_IPV4,
			0
		) &&
		xrtNetSocketBind(Listener, pAddress) &&
		xrtNetSocketLocal(Listener, pAddress) &&
		xrtNetSocketListen(Listener, 16),
		"HTTP cancel race listener setup failed"
	);
	return Listener;
}



/* 等待并释放一个测试线程。 */
static void testHttpClientStreamRaceJoin(
	xthread* pThread,
	cstr sMessage
)
{
	testRequire(
		(xrtThreadWait(pThread) == XWAIT_OK) &&
		(xrtThreadExitCode(pThread) == 0),
		sMessage
	);
	xrtThreadDestroy(pThread);
}



/* 等待最后一次 Abort 命令和 Stream 内部引用离开 Engine。 */
static void testHttpClientStreamRaceWaitEngineIdle(
	xnetengine* pEngine
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(
			xrtNetEngineStats(pEngine, &Stats),
			"HTTP cancel race Engine stats query failed"
		);
		if ( (Stats.PendingCommands == 0) &&
			(Stats.ActiveTimers == 0) &&
			(Stats.LiveObjects == 0) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"HTTP cancel race Engine resources did not drain"
		);
		xrtThreadYield();
	}
}



/* 验证取消接纳、传输所有权和终态提交共享同一线性化边界。 */
int main(void)
{
	test_http_client_stream_race State;
	xnetengineconfig EngineConfig;
	xnetstreamevents ClientEvents;
	xthread* ServerThread;
	xthread* CancelThreads[
		TEST_HTTP_CLIENT_STREAM_CANCEL_THREADS
	];
	uint32 iIteration;
	uint32 i;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Call, NULL);
	xrtAtomic32Init(&State.Iteration, 0);
	xrtAtomic32Init(&State.Started, 0);
	xrtAtomic32Init(&State.RequestReady, 0);
	xrtAtomic32Init(&State.Race, 0);
	xrtAtomic32Init(&State.Mode, 0);
	xrtAtomic32Init(&State.CancelDone, 0);
	xrtAtomic32Init(&State.CancelAccepted, 0);
	xrtAtomic32Init(&State.ForcedCancel, 0);
	xrtAtomic32Init(&State.Completed, 0);
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpClientStreamRaceOpen;

	State.Listener = testHttpClientStreamRaceListener(
		&State.Address
	);
	ServerThread = xrtThreadCreate(
		testHttpClientStreamRaceServer,
		&State,
		0
	);
	testRequire(ServerThread != NULL,
		"HTTP cancel race server thread creation failed");
	for ( i = 0;
		i < TEST_HTTP_CLIENT_STREAM_CANCEL_THREADS;
		++i ) {
		CancelThreads[i] = xrtThreadCreate(
			testHttpClientStreamRaceCancel,
			&State,
			0
		);
		testRequire(CancelThreads[i] != NULL,
			"HTTP cancel race worker creation failed");
	}

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_STREAM_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP cancel race Engine start failed"
	);

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_STREAM_RACE_ITERATIONS;
		++iIteration ) {
		xhttp1call* pCall;
		xnetstream* pClient;
		uint32 iAcceptedBefore;
		uint32 iAcceptedAfter;
		uint32 iMode = (iIteration - 1u) % 4u;

		State.FinalState = XHTTP1_CALL_RUNNING;
		State.FinalResult = XNET_RESULT_ERROR;
		State.Exchange = testHttpClientStreamRaceExchange();
		testRequire(State.Exchange != NULL,
			"HTTP cancel race Exchange creation failed");
		xrtAtomic32Store(
			&State.Mode,
			iMode,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&State.Iteration,
			iIteration,
			XMEMORY_RELEASE
		);
		iAcceptedBefore = xrtAtomic32Load(
			&State.CancelAccepted,
			XMEMORY_ACQUIRE
		);
		pClient = xrtNetStreamConnect(
			State.Engine,
			&State.Address,
			0,
			NULL,
			&ClientEvents,
			&State
		);
		testRequire(pClient != NULL,
			"HTTP cancel race connect failed");
		testHttpClientStreamRaceWait(
			&State.Started,
			iIteration,
			"HTTP cancel race call did not start"
		);
		testHttpClientStreamRaceWait(
			&State.RequestReady,
			iIteration,
			"HTTP cancel race server did not receive request"
		);
		if ( (iMode ==
			 TEST_HTTP_CLIENT_STREAM_RESPONSE_FIRST) ||
			(iMode ==
			 TEST_HTTP_CLIENT_STREAM_CLOSE_FIRST) ) {
			testHttpClientStreamRaceWait(
				&State.Completed,
				iIteration,
				"HTTP pre-race terminal call did not complete"
			);
		}
		xrtAtomic32Store(
			&State.Race,
			iIteration,
			XMEMORY_RELEASE
		);
		testHttpClientStreamRaceWait(
			&State.CancelDone,
			iIteration *
				TEST_HTTP_CLIENT_STREAM_CANCEL_THREADS,
			"HTTP cancel race workers did not finish"
		);
		testHttpClientStreamRaceWait(
			&State.Completed,
			iIteration,
			"HTTP cancel race call did not complete"
		);

		iAcceptedAfter = xrtAtomic32Load(
			&State.CancelAccepted,
			XMEMORY_ACQUIRE
		);
		testRequire(
			(iAcceptedAfter - iAcceptedBefore) <= 1u,
			"HTTP call accepted duplicate cancellation"
		);
		pCall = (xhttp1call*)xrtAtomicPtrLoad(
			&State.Call,
			XMEMORY_ACQUIRE
		);
		testRequire(pCall != NULL,
			"HTTP cancel race lost caller reference");
		if ( iAcceptedAfter != iAcceptedBefore ) {
			testRequire(
				(State.FinalState ==
				 XHTTP1_CALL_CANCELLED) &&
				(State.FinalResult ==
				 XNET_RESULT_CANCELLED) &&
				(xrtHttp1CallState(pCall) ==
				 XHTTP1_CALL_CANCELLED) &&
				(xrtHttp1CallError(pCall) != NULL) &&
				(xrtErrorKind(
					xrtHttp1CallError(pCall)
				) == XERR_CANCELLED),
				"accepted HTTP cancellation did not decide terminal state"
			);
			++State.Cancellations;
		} else if ( iMode ==
			TEST_HTTP_CLIENT_STREAM_CLOSE_FIRST ) {
			testRequire(
				(State.FinalState ==
				 XHTTP1_CALL_FAILED) &&
				(State.FinalResult ==
				 XNET_RESULT_ERROR) &&
				(xrtHttp1CallState(pCall) ==
				 XHTTP1_CALL_FAILED) &&
				(xrtHttp1CallError(pCall) != NULL) &&
				(xrtErrorKind(
					xrtHttp1CallError(pCall)
				) == XERR_PROTOCOL),
				"rejected HTTP cancellation changed failed terminal state"
			);
			++State.Failures;
		} else {
			testRequire(
				(State.FinalState ==
				 XHTTP1_CALL_SUCCEEDED) &&
				(State.FinalResult ==
				 XNET_RESULT_OK) &&
				(xrtHttp1CallState(pCall) ==
				 XHTTP1_CALL_SUCCEEDED) &&
				(xrtHttp1CallError(pCall) == NULL),
				"rejected HTTP cancellation changed successful terminal state"
			);
			++State.Successes;
		}
		xrtAtomicPtrStore(
			&State.Call,
			NULL,
			XMEMORY_RELEASE
		);
		xrtHttp1CallDestroy(pCall);
	}

	testRequire(
		(State.Successes != 0) &&
		(State.Failures != 0) &&
		(State.Cancellations != 0),
		"HTTP cancel race did not cover all terminal outcomes"
	);
	for ( i = 0;
		i < TEST_HTTP_CLIENT_STREAM_CANCEL_THREADS;
		++i ) {
		testHttpClientStreamRaceJoin(
			CancelThreads[i],
			"HTTP cancel race worker failed"
		);
	}
	testHttpClientStreamRaceJoin(
		ServerThread,
		"HTTP cancel race server failed"
	);
	testRequire(xrtNetSocketClose(State.Listener),
		"HTTP cancel race listener close failed");
	testHttpClientStreamRaceWaitEngineIdle(State.Engine);
	testRequire(xrtNetEngineDestroy(State.Engine),
		"HTTP cancel race Engine destroy failed");
	printf(
		"[PASS] HTTP/1 cancel linearization (%s, %u success, %u failed, %u cancelled)\n",
		TEST_HTTP_CLIENT_STREAM_BACKEND_NAME,
		State.Successes,
		State.Failures,
		State.Cancellations
	);
	return 0;
}
