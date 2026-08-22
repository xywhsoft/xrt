#include "../test.h"



#define TEST_RESOLVER_STRESS_PRODUCERS 8u
#define TEST_RESOLVER_STRESS_REQUESTS 256u
#define TEST_RESOLVER_STRESS_TOTAL \
	(TEST_RESOLVER_STRESS_PRODUCERS * TEST_RESOLVER_STRESS_REQUESTS)
#define TEST_RESOLVER_STRESS_HOSTS 48u



typedef struct testresolverstress testresolverstress;



typedef struct testresolverstressitem {
	testresolverstress* State;
	xatomic32 Terminals;
} testresolverstressitem;



typedef struct testresolverstressproducer {
	testresolverstress* State;
	uint32 Index;
} testresolverstressproducer;



struct testresolverstress {
	xnetresolver* Resolver;
	xatomic32 Ready;
	xatomic32 Start;
	xatomic32 Accepted;
	xatomic32 Rejected;
	xatomic32 Callbacks;
	xatomic32 Resolved;
	xatomic32 Cancelled;
	xatomic32 ProviderCalls;
	xatomic32 Failure;
	testresolverstressitem Items[TEST_RESOLVER_STRESS_TOTAL];
};



/* 只记录首个并发故障，避免多个 Worker 覆盖最早的诊断。 */
static void testResolverStressFail(
	testresolverstress* pState,
	uint32 iCode
)
{
	uint32 iExpected = 0;

	(void)xrtAtomic32CompareExchange(
		&pState->Failure,
		&iExpected,
		iCode,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
}



/* 模拟短暂阻塞的系统查询，使提交、合并、取消和限流真实交错。 */
static xnetaddrlist* testResolverStressLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testresolverstress* pState = (testresolverstress*)pData;
	xnetaddr Address;

	(void)sHost;
	(void)xrtAtomic32FetchAdd(
		&pState->ProviderCalls,
		1,
		XMEMORY_RELAXED
	);
	xrtSleep(1);
	if ( !xrtNetAddrLoopback(&Address, Family, 0) ) {
		testResolverStressFail(pState, 1);
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 每个已受理请求只能收到一个可读取的解析或取消终态。 */
static void testResolverStressDone(
	xnetresolveop* pOperation,
	ptr pData
)
{
	testresolverstressitem* pItem = (testresolverstressitem*)pData;
	testresolverstress* pState = pItem->State;
	xnetresolveopstate State = xrtNetResolveOpState(pOperation);
	xnetaddrlist* pAddresses;

	if ( xrtAtomic32FetchAdd(
		&pItem->Terminals,
		1,
		XMEMORY_ACQ_REL
	) != 0 ) {
		testResolverStressFail(pState, 2);
	}
	if ( State == XNET_RESOLVE_RESOLVED ) {
		pAddresses = xrtNetResolveOpResult(pOperation);
		if ( (pAddresses == NULL) ||
			 (xrtNetAddrListCount(pAddresses) != 1) ||
			 (xrtNetAddrListGet(pAddresses, 0) == NULL) ||
			 (xrtNetAddrListGet(pAddresses, 0)->Port != 0) ) {
			testResolverStressFail(pState, 3);
		}
		xrtNetAddrListDestroy(pAddresses);
		(void)xrtAtomic32FetchAdd(
			&pState->Resolved,
			1,
			XMEMORY_RELEASE
		);
	} else if ( State == XNET_RESOLVE_CANCELLED ) {
		if ( xrtErrorKind(xrtNetResolveOpError(pOperation)) !=
			 XERR_CANCELLED ) {
			testResolverStressFail(pState, 4);
		}
		(void)xrtAtomic32FetchAdd(
			&pState->Cancelled,
			1,
			XMEMORY_RELEASE
		);
	} else {
		testResolverStressFail(pState, 5);
	}
	(void)xrtAtomic32FetchAdd(
		&pState->Callbacks,
		1,
		XMEMORY_RELEASE
	);
}



/* 一个生产者反复命中共享主机集合，并穿插取消和主动让出执行权。 */
static int32 testResolverStressProducer(ptr pData)
{
	testresolverstressproducer* pProducer =
		(testresolverstressproducer*)pData;
	testresolverstress* pState = pProducer->State;
	uint32 iBase = pProducer->Index * TEST_RESOLVER_STRESS_REQUESTS;

	(void)xrtAtomic32FetchAdd(&pState->Ready, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pState->Start, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	for ( uint32 i = 0; i < TEST_RESOLVER_STRESS_REQUESTS; i++ ) {
		testresolverstressitem* pItem = &pState->Items[iBase + i];
		xnetresolveop* pOperation;
		char sHost[32];
		uint32 iHost = i % TEST_RESOLVER_STRESS_HOSTS;

		(void)snprintf(
			sHost,
			sizeof(sHost),
			((pProducer->Index + i) & 1u) != 0 ?
				"NODE%02u.TEST" : "node%02u.test",
			iHost
		);
		pOperation = xrtNetResolverResolve(
			pState->Resolver,
			sHost,
			XNET_FAMILY_IPV4,
			testResolverStressDone,
			pItem
		);
		if ( pOperation == NULL ) {
			if ( xrtErrorKind(xrtGetError()) != XERR_AGAIN ) {
				testResolverStressFail(pState, 6);
			}
			xrtClearError();
			(void)xrtAtomic32FetchAdd(
				&pState->Rejected,
				1,
				XMEMORY_RELAXED
			);
		} else {
			(void)xrtAtomic32FetchAdd(
				&pState->Accepted,
				1,
				XMEMORY_RELAXED
			);
			if ( ((iBase + i) % 5u) == 0 ) {
				(void)xrtNetResolveOpCancel(pOperation);
			}
			xrtNetResolveOpDestroy(pOperation);
		}
		if ( (i & 15u) == 15u ) {
			xrtThreadYield();
		}
	}
	return 0;
}



/* 等待全部回调和内部队列归零，覆盖回调计数先于 Outstanding 回落的窗口。 */
static void testResolverStressWaitIdle(
	testresolverstress* pState,
	xnetresolverstats* pStats
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	for ( ;; ) {
		uint32 iAccepted = xrtAtomic32Load(
			&pState->Accepted,
			XMEMORY_ACQUIRE
		);
		uint32 iCallbacks = xrtAtomic32Load(
			&pState->Callbacks,
			XMEMORY_ACQUIRE
		);

		testRequire(xrtNetResolverStats(pState->Resolver, pStats),
			"resolver stress stats failed");
		if ( (iCallbacks == iAccepted) &&
			 (pStats->Outstanding == 0) &&
			 (pStats->ActiveQueries == 0) &&
			 (pStats->QueuedQueries == 0) &&
			 (pStats->RunningQueries == 0) &&
			 (pStats->ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"resolver stress did not become idle");
		xrtThreadYield();
	}
}



/* 多生产者压力覆盖硬限流、规范化合并、取消、提前释放句柄和唯一终态。 */
int main(void)
{
	testresolverstress* pState;
	testresolverstressproducer Producers[TEST_RESOLVER_STRESS_PRODUCERS];
	xthread* Threads[TEST_RESOLVER_STRESS_PRODUCERS];
	xnetresolverconfig Config;
	xnetresolverstats Stats;
	xdeadline iDeadline;
	uint32 iAccepted;
	uint32 iRejected;
	uint32 iCallbacks;

	pState = (testresolverstress*)xrtCalloc(1, sizeof(*pState));
	testRequire(pState != NULL, "resolver stress state allocation failed");
	xrtAtomic32Init(&pState->Ready, 0);
	xrtAtomic32Init(&pState->Start, 0);
	xrtAtomic32Init(&pState->Accepted, 0);
	xrtAtomic32Init(&pState->Rejected, 0);
	xrtAtomic32Init(&pState->Callbacks, 0);
	xrtAtomic32Init(&pState->Resolved, 0);
	xrtAtomic32Init(&pState->Cancelled, 0);
	xrtAtomic32Init(&pState->ProviderCalls, 0);
	xrtAtomic32Init(&pState->Failure, 0);
	for ( uint32 i = 0; i < TEST_RESOLVER_STRESS_TOTAL; i++ ) {
		pState->Items[i].State = pState;
		xrtAtomic32Init(&pState->Items[i].Terminals, 0);
	}
	xrtNetResolverConfigInit(&Config);
	Config.Workers = 4;
	Config.RequestLimit = 64;
	Config.QueryLimit = 16;
	Config.CacheEntries = 64;
	Config.Lookup = testResolverStressLookup;
	Config.LookupData = pState;
	pState->Resolver = xrtNetResolverCreate(&Config);
	testRequire(pState->Resolver != NULL,
		"resolver stress create failed");

	for ( uint32 i = 0; i < TEST_RESOLVER_STRESS_PRODUCERS; i++ ) {
		Producers[i].State = pState;
		Producers[i].Index = i;
		Threads[i] = xrtThreadCreate(
			testResolverStressProducer,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"resolver stress producer create failed");
	}
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&pState->Ready, XMEMORY_ACQUIRE) <
		TEST_RESOLVER_STRESS_PRODUCERS ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"resolver stress producers did not become ready");
		xrtThreadYield();
	}
	xrtAtomic32Store(&pState->Start, 1, XMEMORY_RELEASE);
	for ( uint32 i = 0; i < TEST_RESOLVER_STRESS_PRODUCERS; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"resolver stress producer wait failed");
		xrtThreadDestroy(Threads[i]);
	}

	testResolverStressWaitIdle(pState, &Stats);
	iAccepted = xrtAtomic32Load(&pState->Accepted, XMEMORY_ACQUIRE);
	iRejected = xrtAtomic32Load(&pState->Rejected, XMEMORY_ACQUIRE);
	iCallbacks = xrtAtomic32Load(&pState->Callbacks, XMEMORY_ACQUIRE);
	testRequire(
		(iAccepted + iRejected) == TEST_RESOLVER_STRESS_TOTAL,
		"resolver stress submission accounting mismatch"
	);
	testRequire((iAccepted != 0) && (iRejected != 0),
		"resolver stress did not exercise acceptance and rejection");
	testRequire(iCallbacks == iAccepted,
		"resolver stress callback count mismatch");
	testRequire(
		(xrtAtomic32Load(&pState->Resolved, XMEMORY_ACQUIRE) +
		 xrtAtomic32Load(&pState->Cancelled, XMEMORY_ACQUIRE)) ==
		iAccepted,
		"resolver stress terminal accounting mismatch"
	);
	testRequire(xrtAtomic32Load(&pState->Failure, XMEMORY_ACQUIRE) == 0,
		"resolver stress worker contract failed");
	for ( uint32 i = 0; i < TEST_RESOLVER_STRESS_TOTAL; i++ ) {
		uint32 iTerminals = xrtAtomic32Load(
			&pState->Items[i].Terminals,
			XMEMORY_ACQUIRE
		);

		testRequire(iTerminals <= 1,
			"resolver stress item completed more than once");
	}
	testRequire(
		(Stats.Submitted == iAccepted) &&
		(Stats.Rejected == iRejected) &&
		((Stats.Resolved + Stats.Cancelled) == iAccepted) &&
		(Stats.Coalesced != 0) &&
		(Stats.QueriesStarted != 0),
		"resolver stress statistics mismatch"
	);
	testRequire(xrtNetResolverDestroy(pState->Resolver),
		"resolver stress destroy failed");
	xrtFree(pState);
	printf("[PASS] network resolver stress\n");
	return 0;
}
