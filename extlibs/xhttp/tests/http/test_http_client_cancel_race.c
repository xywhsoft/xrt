#include "../test.h"



#if !defined(TEST_HTTP_CLIENT_RACE_BACKEND)
	#define TEST_HTTP_CLIENT_RACE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_RACE_BACKEND_NAME "select"
#endif

#define TEST_HTTP_CLIENT_RACE_ITERATIONS 96
#define TEST_HTTP_CLIENT_RACE_THREADS 4



typedef enum test_http_client_race_mode {
	TEST_HTTP_CLIENT_RACE_RESPONSE_FIRST = 0,
	TEST_HTTP_CLIENT_RACE_CANCEL_FIRST,
	TEST_HTTP_CLIENT_RACE_SIMULTANEOUS,
	TEST_HTTP_CLIENT_RACE_CLOSE_FIRST
} test_http_client_race_mode;



/* 高层并发夹具在全部线程退出前保留 Client、Call 和累计结果。 */
typedef struct test_http_client_race {
	xnetengine* Engine;
	xhttpclient* Client;
	xnetsocket Listener;
	xnetaddr Address;
	xatomicptr Call;
	xatomic32 Iteration;
	xatomic32 RequestReady;
	xatomic32 Race;
	xatomic32 Mode;
	xatomic32 CancelDone;
	xatomic32 CancelAccepted;
	xatomic32 ForcedCancel;
	xatomic32 Completed;
	xhttpcallstate FinalState;
	xnetresult FinalResult;
	xhttpclienterror FinalError;
	uint32 Successes;
	uint32 Failures;
	uint32 Cancellations;
} test_http_client_race;



/* 在截止时间前等待单调递增的测试阶段。 */
static void testHttpClientRaceWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 创建方与完成回调竞争发布同一 Call，并校验两侧观察到的身份一致。 */
static void testHttpClientRacePublishCall(
	xatomicptr* pTarget,
	xhttpcall* pCall
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
			"HTTP client race callback identity mismatch"
		);
	}
}



/* 判断阻塞源站是否已经收到完整的 HTTP Header。 */
static bool testHttpClientRaceHeaderComplete(
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
static void testHttpClientRaceRespond(xnetsocket Socket)
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
static int32 testHttpClientRaceServer(ptr pData)
{
	test_http_client_race* pState =
		(test_http_client_race*)pData;
	uint32 iIteration;

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_RACE_ITERATIONS;
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
			"HTTP client race accept failed"
		);
		while ( !testHttpClientRaceHeaderComplete(
			Request,
			iUsed
		) ) {
			size_t iReceived = 0;

			testRequire(iUsed < sizeof(Request),
				"HTTP client race request exceeded fixture");
			testRequire(
				(xrtNetSocketRecv(
					Accepted,
					Request + iUsed,
					sizeof(Request) - iUsed,
					&iReceived
				) == XNET_RESULT_OK) &&
				(iReceived != 0),
				"HTTP client race request receive failed"
			);
			iUsed += iReceived;
		}
		iMode = xrtAtomic32Load(
			&pState->Mode,
			XMEMORY_ACQUIRE
		);
		if ( iMode == TEST_HTTP_CLIENT_RACE_RESPONSE_FIRST ) {
			testHttpClientRaceRespond(Accepted);
			xrtAtomic32Store(
				&pState->RequestReady,
				iIteration,
				XMEMORY_RELEASE
			);
		} else if ( iMode == TEST_HTTP_CLIENT_RACE_CLOSE_FIRST ) {
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
			testHttpClientRaceWait(
				&pState->Race,
				iIteration,
				"HTTP client race was not released"
			);
			if ( iMode == TEST_HTTP_CLIENT_RACE_CANCEL_FIRST ) {
				testHttpClientRaceWait(
					&pState->ForcedCancel,
					iIteration,
					"HTTP client forced cancellation was not accepted"
				);
			} else {
				testHttpClientRaceRespond(Accepted);
			}
		}
		if ( Accepted != NULL ) {
			(void)xrtNetSocketClose(Accepted);
		}
	}
	return 0;
}



/* 所有取消线程持续竞争同一轮调用，只有一个线程可以被接纳。 */
static int32 testHttpClientRaceCancel(ptr pData)
{
	test_http_client_race* pState =
		(test_http_client_race*)pData;
	uint32 iIteration;

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_RACE_ITERATIONS;
		++iIteration ) {
		xhttpcall* pCall;
		bool bAccepted;

		testHttpClientRaceWait(
			&pState->Race,
			iIteration,
			"HTTP client cancel worker was not released"
		);
		pCall = (xhttpcall*)xrtAtomicPtrLoad(
			&pState->Call,
			XMEMORY_ACQUIRE
		);
		testRequire(pCall != NULL,
			"HTTP client cancel worker observed no Call");
		bAccepted = xrtHttpCallCancel(pCall);
		if ( bAccepted ) {
			(void)xrtAtomic32FetchAdd(
				&pState->CancelAccepted,
				1,
				XMEMORY_ACQ_REL
			);
			if ( xrtAtomic32Load(
				&pState->Mode,
				XMEMORY_ACQUIRE
			) == TEST_HTTP_CLIENT_RACE_CANCEL_FIRST ) {
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



/* 接收高层终态，并立即释放回调入口转移的响应或升级传输。 */
static void testHttpClientRaceDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_race* pState =
		(test_http_client_race*)pData;
	uint32 iIteration = xrtAtomic32Load(
		&pState->Iteration,
		XMEMORY_ACQUIRE
	);

	testHttpClientRacePublishCall(&pState->Call, pCall);
	testRequire(pResult != NULL,
		"HTTP client race result is null");
	pState->FinalState = xrtHttpCallState(pCall);
	pState->FinalResult = pResult->Result;
	pState->FinalError = pResult->Info.Error;
	xrtHttpResponseDestroy(pResult->Response);
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



/* 为测试域名返回本机 IPv4，隔离操作系统 DNS 和外部网络。 */
static xnetaddrlist* testHttpClientRaceLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(strcmp(sHost, "client.test") == 0,
		"HTTP client race resolved an unexpected host");
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP client race resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 建立绑定到动态回环端口的阻塞测试源站。 */
static xnetsocket testHttpClientRaceListener(xnetaddr* pAddress)
{
	xnetsocket Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(Listener != NULL,
		"HTTP client race listener open failed");
	testRequire(
		xrtNetSocketSet(
			Listener,
			XNET_OPTION_REUSE_ADDRESS,
			1
		),
		"HTTP client race reuse-address failed"
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
		"HTTP client race listener setup failed"
	);
	return Listener;
}



/* 等待并释放一个测试线程。 */
static void testHttpClientRaceJoin(
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



/* 等待取消命令、Timer 和网络对象全部离开 Engine。 */
static void testHttpClientRaceWaitEngineIdle(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(
			xrtNetEngineStats(pEngine, &Stats),
			"HTTP client race Engine stats query failed"
		);
		if ( (Stats.PendingCommands == 0) &&
			(Stats.ActiveTimers == 0) &&
			(Stats.LiveObjects == 0) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP client race Engine resources did not drain"
		);
		xrtThreadYield();
	}
}



/* 验证高层取消接纳、超时门和终态提交共享同一线性化边界。 */
int main(void)
{
	test_http_client_race State;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xthread* ServerThread;
	xthread* CancelThreads[TEST_HTTP_CLIENT_RACE_THREADS];
	char Url[128];
	int iUrlSize;
	uint32 iIteration;
	uint32 i;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Call, NULL);
	xrtAtomic32Init(&State.Iteration, 0);
	xrtAtomic32Init(&State.RequestReady, 0);
	xrtAtomic32Init(&State.Race, 0);
	xrtAtomic32Init(&State.Mode, 0);
	xrtAtomic32Init(&State.CancelDone, 0);
	xrtAtomic32Init(&State.CancelAccepted, 0);
	xrtAtomic32Init(&State.ForcedCancel, 0);
	xrtAtomic32Init(&State.Completed, 0);

	State.Listener = testHttpClientRaceListener(&State.Address);
	ServerThread = xrtThreadCreate(
		testHttpClientRaceServer,
		&State,
		0
	);
	testRequire(ServerThread != NULL,
		"HTTP client race server thread creation failed");
	for ( i = 0; i < TEST_HTTP_CLIENT_RACE_THREADS; ++i ) {
		CancelThreads[i] = xrtThreadCreate(
			testHttpClientRaceCancel,
			&State,
			0
		);
		testRequire(CancelThreads[i] != NULL,
			"HTTP client race worker creation failed");
	}

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_RACE_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP client race Engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpClientRaceLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(State.Client != NULL,
		"HTTP client race Client creation failed");
	iUrlSize = snprintf(
		Url,
		sizeof(Url),
		"http://client.test:%u/race",
		(unsigned int)State.Address.Port
	);
	testRequire(
		(iUrlSize > 0) &&
		((size_t)iUrlSize < sizeof(Url)),
		"HTTP client race URL overflowed"
	);

	for ( iIteration = 1;
		iIteration <= TEST_HTTP_CLIENT_RACE_ITERATIONS;
		++iIteration ) {
		xhttprequest* pRequest;
		xhttpcalloptions Options;
		xhttpcallinfo Info;
		xhttpcall* pCall;
		uint32 iAcceptedBefore;
		uint32 iAcceptedAfter;
		uint32 iMode = (iIteration - 1u) % 4u;

		State.FinalState = XHTTP_CALL_QUEUED;
		State.FinalResult = XNET_RESULT_AGAIN;
		State.FinalError = XHTTP_CLIENT_ERROR_NONE;
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
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			(xstrview){ Url, (size_t)iUrlSize }
		);
		testRequire(pRequest != NULL,
			"HTTP client race request creation failed");
		xrtHttpCallOptionsInit(&Options);
		Options.Timeout = UINT64_MAX - 1u;
		Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
		pCall = xrtHttpClientDo(
			State.Client,
			pRequest,
			&Options,
			testHttpClientRaceDone,
			&State
		);
		xrtHttpRequestDestroy(pRequest);
		testRequire(pCall != NULL,
			"HTTP client race Call creation failed");
		testHttpClientRacePublishCall(&State.Call, pCall);
		testHttpClientRaceWait(
			&State.RequestReady,
			iIteration,
			"HTTP client race server did not receive request"
		);
		if ( (iMode == TEST_HTTP_CLIENT_RACE_RESPONSE_FIRST) ||
			(iMode == TEST_HTTP_CLIENT_RACE_CLOSE_FIRST) ) {
			testHttpClientRaceWait(
				&State.Completed,
				iIteration,
				"HTTP client pre-race terminal Call did not complete"
			);
		}
		xrtAtomic32Store(
			&State.Race,
			iIteration,
			XMEMORY_RELEASE
		);
		testHttpClientRaceWait(
			&State.CancelDone,
			iIteration * TEST_HTTP_CLIENT_RACE_THREADS,
			"HTTP client race workers did not finish"
		);
		testHttpClientRaceWait(
			&State.Completed,
			iIteration,
			"HTTP client race Call did not complete"
		);
		iAcceptedAfter = xrtAtomic32Load(
			&State.CancelAccepted,
			XMEMORY_ACQUIRE
		);
		testRequire(
			xrtHttpCallInfo(pCall, &Info) &&
			(Info.State == State.FinalState) &&
			(Info.Result == State.FinalResult) &&
			(Info.Error == State.FinalError) &&
			(Info.Completed != 0),
			"HTTP client terminal Info snapshot is inconsistent"
		);
		testRequire(
			(iAcceptedAfter - iAcceptedBefore) <= 1u,
			"HTTP client accepted duplicate cancellation"
		);
		if ( iAcceptedAfter != iAcceptedBefore ) {
			const xerror* pError = xrtHttpCallError(pCall);

			testRequire(
				(State.FinalState == XHTTP_CALL_CANCELLED) &&
				(State.FinalResult == XNET_RESULT_CANCELLED) &&
				(State.FinalError ==
				 XHTTP_CLIENT_ERROR_CANCELLED) &&
				(xrtHttpCallState(pCall) ==
				 XHTTP_CALL_CANCELLED) &&
				(pError != NULL) &&
				(xrtErrorKind(pError) == XERR_CANCELLED) &&
				(xrtErrorCode(pError) ==
				 XHTTP_CLIENT_ERROR_CANCELLED) &&
				(strcmp(
					xrtErrorDomain(pError),
					"xrt.http.client"
				) == 0),
				"accepted HTTP client cancellation did not decide terminal state"
			);
			++State.Cancellations;
		} else if ( iMode == TEST_HTTP_CLIENT_RACE_CLOSE_FIRST ) {
			testRequire(
				(State.FinalState == XHTTP_CALL_FAILED) &&
				(State.FinalResult == XNET_RESULT_ERROR) &&
				(xrtHttpCallState(pCall) ==
				 XHTTP_CALL_FAILED) &&
				(xrtHttpCallError(pCall) != NULL),
				"rejected HTTP client cancellation changed failed terminal state"
			);
			++State.Failures;
		} else {
			testRequire(
				(State.FinalState == XHTTP_CALL_SUCCEEDED) &&
				(State.FinalResult == XNET_RESULT_OK) &&
				(xrtHttpCallState(pCall) ==
				 XHTTP_CALL_SUCCEEDED) &&
				(xrtHttpCallError(pCall) == NULL),
				"rejected HTTP client cancellation changed successful terminal state"
			);
			++State.Successes;
		}
		xrtAtomicPtrStore(
			&State.Call,
			NULL,
			XMEMORY_RELEASE
		);
		xrtHttpCallDestroy(pCall);
	}

	testRequire(
		(State.Successes != 0) &&
		(State.Failures != 0) &&
		(State.Cancellations != 0),
		"HTTP client race did not cover all terminal outcomes"
	);
	for ( i = 0; i < TEST_HTTP_CLIENT_RACE_THREADS; ++i ) {
		testHttpClientRaceJoin(
			CancelThreads[i],
			"HTTP client race worker failed"
		);
	}
	testHttpClientRaceJoin(
		ServerThread,
		"HTTP client race server failed"
	);
	testRequire(xrtNetSocketClose(State.Listener),
		"HTTP client race listener close failed");
	xrtHttpClientDestroy(State.Client);
	testHttpClientRaceWaitEngineIdle(State.Engine);
	testRequire(xrtNetEngineDestroy(State.Engine),
		"HTTP client race Engine destroy failed");
	printf(
		"[PASS] high-level HTTP client cancel linearization (%s, %u success, %u failed, %u cancelled)\n",
		TEST_HTTP_CLIENT_RACE_BACKEND_NAME,
		State.Successes,
		State.Failures,
		State.Cancellations
	);
	return 0;
}
