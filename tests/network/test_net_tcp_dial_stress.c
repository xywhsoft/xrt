#include "../test.h"



#if !defined(TEST_TCP_DIAL_BACKEND)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_SELECT
#endif



#define TEST_DIAL_STRESS_COUNT 64u
#define TEST_DIAL_STRESS_CANCEL_THREADS 4u
#define TEST_DIAL_STRESS_CANCELLED (TEST_DIAL_STRESS_COUNT / 2u)
#define TEST_DIAL_STRESS_CONNECTED \
	(TEST_DIAL_STRESS_COUNT - TEST_DIAL_STRESS_CANCELLED)



typedef struct testdialstress testdialstress;



typedef struct testdialstressattempt {
	testdialstress* Context;
	xnetdial* Dial;
	xnetstream* Client;
	xatomic32 Terminal;
} testdialstressattempt;



typedef struct testdialstresscancel {
	testdialstress* Context;
	uint32 Index;
} testdialstresscancel;



struct testdialstress {
	testdialstressattempt Attempts[TEST_DIAL_STRESS_COUNT];
	xnetstream* Servers[TEST_DIAL_STRESS_CONNECTED];
	xatomic32 LookupEntered;
	xatomic32 LookupGate;
	xatomic32 LookupCalls;
	xatomic32 CancelStart;
	xatomic32 CancelThreadsDone;
	xatomic32 Done;
	xatomic32 Cancelled;
	xatomic32 Connected;
	xatomic32 NextServer;
	xatomic32 Accepted;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xatomic32 Failure;
};



/* 在压力测试截止时间前等待原子计数达到下限。 */
static void testDialStressWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞一次真实查询，使全部同主机请求稳定进入 Resolver 合并组。 */
static xnetaddrlist* testDialStressLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialstress* pContext = (testdialstress*)pData;
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)xrtAtomic32FetchAdd(
		&pContext->LookupCalls,
		1,
		XMEMORY_ACQ_REL
	);
	xrtAtomic32Store(
		&pContext->LookupEntered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->LookupGate,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 接管每个服务端 Stream，并在发布计数前保存其调用方引用。 */
static bool testDialStressAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testdialstress* pContext = (testdialstress*)pData;
	uint32 iIndex = xrtAtomic32FetchAdd(
		&pContext->NextServer,
		1,
		XMEMORY_ACQ_REL
	);

	(void)pListener;
	if ( iIndex >= TEST_DIAL_STRESS_CONNECTED ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		return false;
	}
	if ( !xrtNetStreamSetData(pStream, pContext) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		return false;
	}
	pContext->Servers[iIndex] = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 任意 Listener 后台错误都表示压力契约失败。 */
static void testDialStressListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testdialstress* pContext = (testdialstress*)pData;

	(void)pListener;
	(void)pError;
	xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
}



/* 记录 Listener 已经排空全部预投递 Accept。 */
static void testDialStressListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testdialstress* pContext = (testdialstress*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pContext->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 全部公开连接必须且只能正常关闭一次。 */
static void testDialStressStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testdialstress* pContext = (testdialstress*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 校验每个 Dial 只有一个终态，并接管成功 Stream 引用。 */
static void testDialStressDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testdialstressattempt* pAttempt =
		(testdialstressattempt*)pData;
	testdialstress* pContext = pAttempt->Context;
	uint32 iTerminal = xrtAtomic32FetchAdd(
		&pAttempt->Terminal,
		1,
		XMEMORY_ACQ_REL
	);

	if ( iTerminal != 0 ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	if ( Result == XNET_RESULT_OK ) {
		if ( (pStream == NULL) || (pError != NULL) ||
			 (xrtNetDialState(pDial) != XNET_DIAL_CONNECTED) ||
			 (xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ) {
			xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		} else {
			pAttempt->Client = pStream;
			(void)xrtAtomic32FetchAdd(
				&pContext->Connected,
				1,
				XMEMORY_RELEASE
			);
		}
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		if ( (pStream != NULL) || (pError == NULL) ||
			 (xrtErrorKind(pError) != XERR_CANCELLED) ||
			 (xrtNetDialState(pDial) != XNET_DIAL_CANCELLED) ) {
			xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		} else {
			(void)xrtAtomic32FetchAdd(
				&pContext->Cancelled,
				1,
				XMEMORY_RELEASE
			);
		}
	} else {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 四个外部线程同时取消互不重叠的偶数 Dial。 */
static int32 testDialStressCancelThread(ptr pData)
{
	testdialstresscancel* pCancel =
		(testdialstresscancel*)pData;
	testdialstress* pContext = pCancel->Context;
	uint32 iStep = TEST_DIAL_STRESS_CANCEL_THREADS * 2u;

	while ( xrtAtomic32Load(
		&pContext->CancelStart,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	for ( uint32 i = pCancel->Index * 2u;
		 i < TEST_DIAL_STRESS_COUNT;
		 i += iStep ) {
		if ( !xrtNetDialCancel(pContext->Attempts[i].Dial) ) {
			xrtAtomic32Store(
				&pContext->Failure,
				1,
				XMEMORY_RELEASE
			);
		}
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->CancelThreadsDone,
		1,
		XMEMORY_RELEASE
	);
	return 0;
}



/* 重试 Engine 销毁，验证延迟 Resolver 回调和候选资源最终归零。 */
static void testDialStressDestroyEngine(xnetengine* pEngine)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial stress retained an engine resource");
		xrtThreadYield();
	}
}



/* 验证解析合并、跨线程取消、并发连接和完整资源回收。 */
int main(void)
{
	testdialstress Context;
	testdialstresscancel Cancels[TEST_DIAL_STRESS_CANCEL_THREADS];
	xthread* CancelThreads[TEST_DIAL_STRESS_CANCEL_THREADS];
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetresolverstats ResolverStats;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetdialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	memset(Cancels, 0, sizeof(Cancels));
	memset(CancelThreads, 0, sizeof(CancelThreads));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testDialStressAccept;
	ListenerEvents.Error = testDialStressListenerError;
	ListenerEvents.Close = testDialStressListenerClose;
	StreamEvents.Close = testDialStressStreamClose;

	/* 建立多 Worker Engine、无缓存 Resolver 和高并发 Listener。 */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_BACKEND;
	EngineConfig.Workers = 4;
	EngineConfig.CommandCapacity = 16384;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial stress engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 2;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.SuccessTTL = 0;
	ResolverConfig.FailureTTL = 0;
	ResolverConfig.Lookup = testDialStressLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "dial stress resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "dial stress listener address failed");
	ListenConfig.AcceptConcurrency = 16;
	ListenConfig.AcceptQueueLimit = TEST_DIAL_STRESS_CONNECTED;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"dial stress listener create failed");

	/* 提交全部同主机 Dial，确保 Resolver 可以观察完整合并批次。 */
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.Timeout = 10000000u;
	DialConfig.MaxAttempts = 1;
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_COUNT; i++ ) {
		Context.Attempts[i].Context = &Context;
		DialConfig.Affinity = i;
		Context.Attempts[i].Dial = xrtNetDial(
			pEngine,
			pResolver,
			"coalesced.test",
			Address.Port,
			&DialConfig,
			&StreamEvents,
			&Context,
			testDialStressDone,
			&Context.Attempts[i]
		);
		testRequire(Context.Attempts[i].Dial != NULL,
			"dial stress submit failed");
	}
	testDialStressWait(&Context.LookupEntered, 1,
		"dial stress lookup did not start");

	/* 从四个普通线程取消一半请求，并等待取消终态先于 DNS 放行。 */
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_CANCEL_THREADS; i++ ) {
		Cancels[i].Context = &Context;
		Cancels[i].Index = i;
		CancelThreads[i] = xrtThreadCreate(
			testDialStressCancelThread,
			&Cancels[i],
			0
		);
		testRequire(CancelThreads[i] != NULL,
			"dial stress cancel thread create failed");
	}
	xrtAtomic32Store(&Context.CancelStart, 1, XMEMORY_RELEASE);
	testDialStressWait(
		&Context.CancelThreadsDone,
		TEST_DIAL_STRESS_CANCEL_THREADS,
		"dial stress cancel threads did not finish"
	);
	testDialStressWait(
		&Context.Cancelled,
		TEST_DIAL_STRESS_CANCELLED,
		"dial stress cancellation terminals were lost"
	);
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_CANCEL_THREADS; i++ ) {
		testRequire(xrtThreadWait(CancelThreads[i]) == XWAIT_OK,
			"dial stress cancel thread wait failed");
		testRequire(xrtThreadExitCode(CancelThreads[i]) == 0,
			"dial stress cancel thread returned failure");
		xrtThreadDestroy(CancelThreads[i]);
	}

	/* 放行唯一查询，并等待剩余请求全部连接和接受。 */
	xrtAtomic32Store(&Context.LookupGate, 1, XMEMORY_RELEASE);
	testDialStressWait(
		&Context.Done,
		TEST_DIAL_STRESS_COUNT,
		"dial stress operations did not finish"
	);
	testDialStressWait(
		&Context.Connected,
		TEST_DIAL_STRESS_CONNECTED,
		"dial stress connections did not finish"
	);
	testDialStressWait(
		&Context.Accepted,
		TEST_DIAL_STRESS_CONNECTED,
		"dial stress accepts did not finish"
	);
	testRequire((xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	) == 0) && (xrtAtomic32Load(
		&Context.LookupCalls,
		XMEMORY_ACQUIRE
	) == 1), "dial stress terminal or query count mismatch");
	testRequire(xrtNetResolverStats(pResolver, &ResolverStats) &&
		(ResolverStats.Submitted == TEST_DIAL_STRESS_COUNT) &&
		(ResolverStats.QueriesStarted == 1) &&
		(ResolverStats.Coalesced >= (TEST_DIAL_STRESS_COUNT - 1u)) &&
		(ResolverStats.Cancelled == TEST_DIAL_STRESS_CANCELLED) &&
		(ResolverStats.Resolved == TEST_DIAL_STRESS_CONNECTED),
		"dial stress Resolver coalescing statistics mismatch");

	/* 对称关闭全部公开端点，再释放 Dial、Listener、Resolver 和 Engine。 */
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_COUNT; i++ ) {
		if ( Context.Attempts[i].Client != NULL ) {
			testRequire(xrtNetStreamClose(
				Context.Attempts[i].Client
			), "dial stress client close failed");
		}
	}
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_CONNECTED; i++ ) {
		testRequire(xrtNetStreamClose(Context.Servers[i]),
			"dial stress server close failed");
	}
	testDialStressWait(
		&Context.Closed,
		TEST_DIAL_STRESS_CONNECTED * 2u,
		"dial stress streams did not close"
	);
	testRequire(xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	) == 0, "dial stress stream close mismatch");
	testRequire(xrtNetListenerClose(pListener),
		"dial stress listener close failed");
	testDialStressWait(&Context.ListenerClosed, 1,
		"dial stress listener did not close");
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_COUNT; i++ ) {
		xrtNetStreamDestroy(Context.Attempts[i].Client);
		xrtNetDialDestroy(Context.Attempts[i].Dial);
	}
	for ( uint32 i = 0; i < TEST_DIAL_STRESS_CONNECTED; i++ ) {
		xrtNetStreamDestroy(Context.Servers[i]);
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"dial stress resolver destroy failed");
	testDialStressDestroyEngine(pEngine);
	printf("[PASS] managed TCP dial concurrent stress\n");
	return 0;
}
