#include "../test.h"



#if !defined(TEST_TCP_DIAL_BACKEND)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_SELECT
#endif



#define TEST_DIAL_FUTURE_OOM_LIMIT 4096u



typedef struct testdialfutureoom {
	xatomic32 Fail;
	xatomic32 Entered;
	xatomic32 Gate;
	xatomic64 Allocations;
} testdialfutureoom;



/* 正常阶段转发系统分配，故障阶段拒绝全部新的底层内存。 */
static ptr testDialFutureOomAlloc(ptr pData, size_t iSize)
{
	testdialfutureoom* pContext = (testdialfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testDialFutureOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testdialfutureoom* pContext = (testdialfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testDialFutureOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 阻塞唯一查询，使已经创建的 Future 持有完整桥接链。 */
static xnetaddrlist* testDialFutureOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialfutureoom* pContext = (testdialfutureoom*)pData;
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)xrtAtomic32FetchAdd(
		&pContext->Entered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->Gate,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 在截止时间前等待原子计数达到目标。 */
static void testDialFutureOomWait(
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



/* 在截止时间前拉取一个已接受 Stream。 */
static xnetstream* testDialFutureOomAccept(xnetlistener* pListener)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetstream* pStream;

	while ( (pStream = xrtNetListenerAccept(pListener)) == NULL ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future OOM accept timed out");
		xrtThreadYield();
	}
	return pStream;
}



/* 等待 Resolver 清空取消后的查询和回调资源。 */
static void testDialFutureOomResolverIdle(xnetresolver* pResolver)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetresolverstats Stats;

	for ( ;; ) {
		testRequire(xrtNetResolverStats(pResolver, &Stats),
			"dial Future OOM resolver statistics failed");
		if ( (Stats.Outstanding == 0) &&
			 (Stats.ActiveQueries == 0) &&
			 (Stats.ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future OOM resolver did not become idle");
		xrtThreadYield();
	}
}



/* 验证 Future 桥接创建链 OOM 完整回滚，并在恢复后完成真实连接。 */
int main(void)
{
	testdialfutureoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetdialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetenginestats EngineStats;
	xnetstream* pClient;
	xnetstream* pServer;
	xfuture* pRecovery;
	xfuture** pPending;
	xnetaddr Address;
	xdeadline iDeadline;
	size_t iPending = 0;

	memset(&Context, 0, sizeof(Context));
	Allocator.Context = &Context;
	Allocator.Alloc = testDialFutureOomAlloc;
	Allocator.Realloc = testDialFutureOomRealloc;
	Allocator.Free = testDialFutureOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"dial Future OOM allocator install failed");
	pPending = (xfuture**)malloc(
		TEST_DIAL_FUTURE_OOM_LIMIT * sizeof(*pPending)
	);
	testRequire(pPending != NULL,
		"dial Future OOM system allocation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial Future OOM engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.SuccessTTL = 0;
	ResolverConfig.FailureTTL = 0;
	ResolverConfig.Lookup = testDialFutureOomLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"dial Future OOM resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "dial Future OOM listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"dial Future OOM listener create failed");
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.Timeout = 0;

	/* 首个请求保证查询线程进入阻塞点，后续请求只消耗桥接资源。 */
	pPending[iPending] = xrtNetDialAsync(
		pEngine,
		pResolver,
		"future-oom.test",
		Address.Port,
		&DialConfig,
		NULL,
		NULL
	);
	testRequire(pPending[iPending++] != NULL,
		"dial Future OOM setup submit failed");
	testDialFutureOomWait(
		&Context.Entered,
		1,
		"dial Future OOM lookup did not start"
	);

	/* 保留全部成功对象直到尺寸类耗尽，首次失败必须保持 MEMORY 根因。 */
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	while ( iPending < TEST_DIAL_FUTURE_OOM_LIMIT ) {
		xfuture* pFuture = xrtNetDialAsync(
			pEngine,
			pResolver,
			"future-oom.test",
			Address.Port,
			&DialConfig,
			NULL,
			NULL
		);

		if ( pFuture == NULL ) {
			break;
		}
		pPending[iPending++] = pFuture;
	}
	testRequire(iPending < TEST_DIAL_FUTURE_OOM_LIMIT,
		"dial Future bridge did not reach backing OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"dial Future bridge OOM error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);

	/* 每个已经返回的 Future 都必须保持可取消、可等待和可销毁。 */
	for ( size_t i = 0; i < iPending; i++ ) {
		testRequire(xrtFutureCancel(pPending[i]),
			"dial Future OOM pending cancellation failed");
	}
	for ( size_t i = 0; i < iPending; i++ ) {
		testRequire((xrtFutureWaitFor(
			pPending[i],
			5000000u
		) == XWAIT_OK) &&
			(xrtFutureState(pPending[i]) == XFUTURE_CANCELLED),
			"dial Future OOM pending terminal mismatch");
		xrtFutureDestroy(pPending[i]);
	}
	testRequire(xrtNetEngineStats(pEngine, &EngineStats) &&
		(EngineStats.LiveObjects == 1) &&
		(EngineStats.ActiveTimers == 0),
		"dial Future terminal retained a Dial Engine resource");
	xrtAtomic32Store(&Context.Gate, 1, XMEMORY_RELEASE);
	testDialFutureOomResolverIdle(pResolver);

	/* 故障解除后，同一桥接层必须完成一次端到端回环连接。 */
	pRecovery = xrtNetDialAsync(
		pEngine,
		pResolver,
		"future-oom.test",
		Address.Port,
		&DialConfig,
		NULL,
		NULL
	);
	testRequire((pRecovery != NULL) &&
		(xrtFutureWaitFor(pRecovery, 5000000u) == XWAIT_OK) &&
		(xrtFutureState(pRecovery) == XFUTURE_RESOLVED),
		"dial Future did not recover after OOM");
	pClient = (xnetstream*)xrtFutureValue(pRecovery);
	pServer = testDialFutureOomAccept(pListener);
	testRequire((pClient != NULL) && xrtNetStreamClose(pClient) &&
		xrtNetStreamClose(pServer),
		"dial Future OOM recovery close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future OOM recovery streams did not close");
		xrtThreadYield();
	}
	xrtFutureDestroy(pRecovery);
	xrtNetStreamDestroy(pServer);

	testRequire(xrtNetListenerClose(pListener),
		"dial Future OOM listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"dial Future OOM resolver destroy failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future OOM retained an Engine resource");
		xrtThreadYield();
	}
	testRequire(xrtAtomic64Load(
		&Context.Allocations,
		XMEMORY_ACQUIRE
	) != 0, "dial Future OOM allocator was not exercised");
	free(pPending);
	printf("[PASS] managed TCP dial Future OOM cleanup\n");
	return 0;
}
