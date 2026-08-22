#include "../test.h"



typedef struct testdialoom {
	xatomic32 Fail;
	xatomic32 Done;
	xatomic64 Allocations;
	xnetresult Result;
	xerrkind ErrorKind;
} testdialoom;



#define TEST_DIAL_OOM_ADDRESS_COUNT 64u
#define TEST_DIAL_OOM_HOST_SIZE 2048u



/* 正常阶段转发分配，故障阶段拒绝全部新内存。 */
static ptr testDialOomAlloc(ptr pData, size_t iSize)
{
	testdialoom* pContext = (testdialoom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testDialOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testdialoom* pContext = (testdialoom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Allocations,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testDialOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 先建立完整地址结果，再关闭分配器以命中 Dial 候选构造。 */
static xnetaddrlist* testDialOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialoom* pContext = (testdialoom*)pData;
	xnetaddr Addresses[TEST_DIAL_OOM_ADDRESS_COUNT];
	xnetaddrlist* pList;

	(void)sHost;
	(void)Family;
	for ( size_t i = 0; i < TEST_DIAL_OOM_ADDRESS_COUNT; i++ ) {
		if ( !xrtNetAddrLoopback(
			&Addresses[i],
			XNET_FAMILY_IPV4,
			0
		) ) {
			return NULL;
		}
		Addresses[i].Address[3] = (uint8)(i + 1u);
	}
	pList = xrtNetAddrListCreate(
		Addresses,
		TEST_DIAL_OOM_ADDRESS_COUNT
	);
	xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
	return pList;
}



/* OOM 终态必须使用不分配的静态错误并且不能返回半成品 Stream。 */
static void testDialOomDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testdialoom* pContext = (testdialoom*)pData;

	(void)pDial;
	testRequire((pStream == NULL) && (pError != NULL),
		"dial OOM returned a stream or lost its error");
	pContext->Result = Result;
	pContext->ErrorKind = xrtErrorKind(pError);
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证入口 OOM 与解析后 OOM 都保持可回收终态。 */
int main(void)
{
	testdialoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetdialconfig DialConfig;
	xnetdialstats Stats;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetdial* pDial;
	xdeadline iDeadline;
	char sLargeHost[TEST_DIAL_OOM_HOST_SIZE + 1u];

	memset(&Context, 0, sizeof(Context));
	Allocator.Context = &Context;
	Allocator.Alloc = testDialOomAlloc;
	Allocator.Realloc = testDialOomRealloc;
	Allocator.Free = testDialOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"dial OOM allocator install failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial OOM engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.SuccessTTL = 0;
	ResolverConfig.FailureTTL = 0;
	ResolverConfig.Lookup = testDialOomLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "dial OOM resolver create failed");
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.Timeout = 0;

	/* 大主机名绕过尺寸类缓存，验证入口失败不会留下 Engine 持有。 */
	memset(sLargeHost, 'h', TEST_DIAL_OOM_HOST_SIZE);
	sLargeHost[TEST_DIAL_OOM_HOST_SIZE] = 0;
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetDial(
		pEngine,
		pResolver,
		sLargeHost,
		443,
		&DialConfig,
		NULL,
		NULL,
		testDialOomDone,
		&Context
	) == NULL, "dial entry survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"dial entry OOM error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);

	/* 64 个地址让解析后复制必然走大块分配，消除尺寸类缓存差异。 */
	pDial = xrtNetDial(
		pEngine,
		pResolver,
		"oom.test",
		443,
		&DialConfig,
		NULL,
		NULL,
		testDialOomDone,
		&Context
	);
	testRequire(pDial != NULL, "dial OOM operation setup failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&Context.Done, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial OOM terminal callback timed out");
		xrtThreadYield();
	}
	(void)xrtNetDialStats(pDial, &Stats);
	testRequire((Context.Result == XNET_RESULT_ERROR) &&
		(Context.ErrorKind == XERR_MEMORY) &&
		(Stats.State == XNET_DIAL_FAILED) &&
		(Stats.AttemptsStarted == 0) &&
		(Stats.AttemptsFailed == 0) &&
		(Stats.ActiveAttempts == 0),
		"dial OOM terminal or statistics mismatch");
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtNetDialDestroy(pDial);
	testRequire(xrtNetResolverDestroy(pResolver),
		"dial OOM resolver destroy failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial OOM retained an internal engine resource");
		xrtThreadYield();
	}
	testRequire(xrtAtomic64Load(
		&Context.Allocations,
		XMEMORY_ACQUIRE
	) != 0, "dial OOM allocator was not exercised");
	printf("[PASS] managed TCP dial OOM cleanup\n");
	return 0;
}
