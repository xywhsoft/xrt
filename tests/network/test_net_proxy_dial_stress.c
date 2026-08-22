#include "../test.h"



#if !defined(TEST_PROXY_DIAL_BACKEND)
	#define TEST_PROXY_DIAL_BACKEND XNET_PORT_SELECT
#endif



#define TEST_PROXY_DIAL_STRESS_COUNT		64u
#define TEST_PROXY_DIAL_CANCEL_THREADS	4u



typedef struct testproxydialstress testproxydialstress;



typedef struct testproxydialattempt {
	testproxydialstress* Context;
	xnetproxydial* Dial;
	xatomic32 Terminal;
} testproxydialattempt;



typedef struct testproxydialcancel {
	testproxydialstress* Context;
	uint32 Index;
} testproxydialcancel;



struct testproxydialstress {
	testproxydialattempt Attempts[TEST_PROXY_DIAL_STRESS_COUNT];
	xatomic32 LookupEntered;
	xatomic32 LookupGate;
	xatomic32 LookupCalls;
	xatomic32 CancelStart;
	xatomic32 CancelThreadsDone;
	xatomic32 Done;
	xatomic32 Cancelled;
	xatomic32 Failure;
};



/* 在压力测试截止时间前等待原子计数达到下限。 */
static void testProxyDialStressWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞唯一真实查询，使全部同主机请求稳定进入 Resolver 合并组。 */
static xnetaddrlist* testProxyDialStressLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testproxydialstress* pContext = (testproxydialstress*)pData;
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
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 9) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 校验每个 Proxy Dial 只发布一次取消终态且不转移 Stream。 */
static void testProxyDialStressDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testproxydialattempt* pAttempt = (testproxydialattempt*)pData;
	testproxydialstress* pContext = pAttempt->Context;
	uint32 iTerminal = xrtAtomic32FetchAdd(
		&pAttempt->Terminal,
		1,
		XMEMORY_ACQ_REL
	);

	if ( (iTerminal != 0) ||
		(Result != XNET_RESULT_CANCELLED) ||
		(pStream != NULL) || (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_CANCELLED) ||
		(xrtErrorCode(pError) != XNET_ERROR_PROXY_CONNECT) ||
		(xrtNetProxyDialState(pDial) != XNET_PROXY_DIAL_CANCELLED) ) {
		xrtAtomic32Store(
			&pContext->Failure,
			1,
			XMEMORY_RELEASE
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pContext->Cancelled,
			1,
			XMEMORY_RELEASE
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 四个外部线程同时取消互不重叠的 Proxy Dial，并验证取消唯一性。 */
static int32 testProxyDialStressCancelThread(ptr pData)
{
	testproxydialcancel* pCancel = (testproxydialcancel*)pData;
	testproxydialstress* pContext = pCancel->Context;

	while ( xrtAtomic32Load(
		&pContext->CancelStart,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	for ( uint32 i = pCancel->Index;
		 i < TEST_PROXY_DIAL_STRESS_COUNT;
		 i += TEST_PROXY_DIAL_CANCEL_THREADS ) {
		if ( !xrtNetProxyDialCancel(pContext->Attempts[i].Dial) ||
			xrtNetProxyDialCancel(pContext->Attempts[i].Dial) ) {
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



/* 等待查询线程和全部回调退出 Resolver，再读取稳定统计。 */
static void testProxyDialStressWaitResolver(
	xnetresolver* pResolver,
	xnetresolverstats* pStats
)
{
	xdeadline Deadline = xrtDeadlineAfter(15000000u);

	for ( ;; ) {
		testRequire(xrtNetResolverStats(pResolver, pStats),
			"proxy Dial stress resolver statistics failed");
		if ( (pStats->Outstanding == 0) &&
			(pStats->ActiveQueries == 0) &&
			(pStats->QueuedQueries == 0) &&
			(pStats->RunningQueries == 0) &&
			(pStats->ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial stress resolver did not become idle");
		xrtThreadYield();
	}
}



/* 重试 Engine 销毁，验证取消命令、Timer 和底层 Dial 引用最终归零。 */
static void testProxyDialStressDestroyEngine(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(15000000u);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(Deadline),
			"proxy Dial stress retained an engine resource");
		xrtThreadYield();
	}
}



/* 验证解析合并、多线程唯一取消、终态顺序和完整资源回收。 */
int main(void)
{
	testproxydialstress Context;
	testproxydialcancel Cancels[TEST_PROXY_DIAL_CANCEL_THREADS];
	xthread* CancelThreads[TEST_PROXY_DIAL_CANCEL_THREADS];
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetresolverstats ResolverStats;
	xnetproxyconfig ProxyConfig;
	xnetproxydialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetproxy* pProxy;

	memset(&Context, 0, sizeof(Context));
	memset(Cancels, 0, sizeof(Cancels));
	memset(CancelThreads, 0, sizeof(CancelThreads));

	/* 建立多 Worker Engine、无缓存 Resolver 和共享不可变 Proxy。 */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_PROXY_DIAL_BACKEND;
	EngineConfig.Workers = 4;
	EngineConfig.CommandCapacity = 16384;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"proxy Dial stress engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 2;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.SuccessTTL = 0;
	ResolverConfig.FailureTTL = 0;
	ResolverConfig.Lookup = testProxyDialStressLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"proxy Dial stress resolver create failed");
	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Host = XRT_STR_LITERAL("coalesced.proxy");
	ProxyConfig.Port = 1080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(pProxy != NULL,
		"proxy Dial stress proxy create failed");

	/* 提交同一代理主机的全部请求，使 Resolver 形成单一查询组。 */
	xrtNetProxyDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.Timeout = 10000000u;
	DialConfig.Transport.MaxAttempts = 1;
	DialConfig.Timeout = 0;
	for ( uint32 i = 0; i < TEST_PROXY_DIAL_STRESS_COUNT; i++ ) {
		Context.Attempts[i].Context = &Context;
		DialConfig.Transport.Affinity = i;
		Context.Attempts[i].Dial = xrtNetProxyDial(
			pEngine,
			pResolver,
			pProxy,
			"origin.test",
			443,
			&DialConfig,
			NULL,
			NULL,
			testProxyDialStressDone,
			&Context.Attempts[i]
		);
		testRequire(Context.Attempts[i].Dial != NULL,
			"proxy Dial stress submit failed");
	}
	testProxyDialStressWait(
		&Context.LookupEntered,
		1,
		"proxy Dial stress lookup did not start"
	);

	/* 从四个普通线程取消全部请求，查询仍阻塞时就应发布取消终态。 */
	for ( uint32 i = 0; i < TEST_PROXY_DIAL_CANCEL_THREADS; i++ ) {
		Cancels[i].Context = &Context;
		Cancels[i].Index = i;
		CancelThreads[i] = xrtThreadCreate(
			testProxyDialStressCancelThread,
			&Cancels[i],
			0
		);
		testRequire(CancelThreads[i] != NULL,
			"proxy Dial stress cancel thread create failed");
	}
	xrtAtomic32Store(&Context.CancelStart, 1, XMEMORY_RELEASE);
	testProxyDialStressWait(
		&Context.CancelThreadsDone,
		TEST_PROXY_DIAL_CANCEL_THREADS,
		"proxy Dial stress cancel threads did not finish"
	);
	testProxyDialStressWait(
		&Context.Done,
		TEST_PROXY_DIAL_STRESS_COUNT,
		"proxy Dial stress cancellation terminals were lost"
	);
	for ( uint32 i = 0; i < TEST_PROXY_DIAL_CANCEL_THREADS; i++ ) {
		testRequire(xrtThreadWait(CancelThreads[i]) == XWAIT_OK,
			"proxy Dial stress cancel thread wait failed");
		testRequire(xrtThreadExitCode(CancelThreads[i]) == 0,
			"proxy Dial stress cancel thread returned failure");
		xrtThreadDestroy(CancelThreads[i]);
	}

	/* 放行已经无人等待的真实查询，并等待 Resolver 后台状态归零。 */
	xrtAtomic32Store(&Context.LookupGate, 1, XMEMORY_RELEASE);
	testProxyDialStressWaitResolver(pResolver, &ResolverStats);
	testRequire((xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	) == 0) && (xrtAtomic32Load(
		&Context.Cancelled,
		XMEMORY_ACQUIRE
	) == TEST_PROXY_DIAL_STRESS_COUNT) &&
		(xrtAtomic32Load(
			&Context.LookupCalls,
			XMEMORY_ACQUIRE
		) == 1), "proxy Dial stress terminal or query count mismatch");
	testRequire((ResolverStats.Submitted == TEST_PROXY_DIAL_STRESS_COUNT) &&
		(ResolverStats.QueriesStarted == 1) &&
		(ResolverStats.Coalesced >=
			(TEST_PROXY_DIAL_STRESS_COUNT - 1u)) &&
		(ResolverStats.Cancelled == TEST_PROXY_DIAL_STRESS_COUNT) &&
		(ResolverStats.Resolved == 0),
		"proxy Dial stress Resolver coalescing statistics mismatch");

	/* 释放调用方持有，再验证 Resolver 和 Engine 都没有内部残留。 */
	for ( uint32 i = 0; i < TEST_PROXY_DIAL_STRESS_COUNT; i++ ) {
		testRequire(xrtAtomic32Load(
			&Context.Attempts[i].Terminal,
			XMEMORY_ACQUIRE
		) == 1, "proxy Dial stress duplicate terminal callback");
		xrtNetProxyDialDestroy(Context.Attempts[i].Dial);
	}
	xrtNetProxyRelease(pProxy);
	testRequire(xrtNetResolverDestroy(pResolver),
		"proxy Dial stress resolver destroy failed");
	testProxyDialStressDestroyEngine(pEngine);
	printf("[PASS] managed proxy Dial concurrent cancellation\n");
	return 0;
}
