#include "../test.h"



#define TEST_RESOLVER_OOM_DIRECT_SIZE 16384u
#define TEST_RESOLVER_OOM_ADDRESS_COUNT 1024u



typedef union testresolveroomheader {
	uint64 Alignment;
	struct {
		size_t Size;
	} Value;
} testresolveroomheader;



typedef struct testresolveroomalloc {
	xatomic32 Calls;
	xatomic32 FailAt;
	xatomic32 Hit;
	xatomic32 Live;
	xatomic32 DirectLive;
	xatomic32 Failure;
} testresolveroomalloc;



typedef struct testresolveroomquery {
	testresolveroomalloc* Allocator;
	xatomic32 Callbacks;
	xatomic32 Resolved;
	xatomic32 Failed;
} testresolveroomquery;



/* 记录首个跨线程故障，不在分配器或 Resolver Worker 中终止进程。 */
static void testResolverOomFail(
	testresolveroomalloc* pState,
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



/* 判断本次分配是否命中调用方选择的故障序号。 */
static bool testResolverOomShouldFail(testresolveroomalloc* pState)
{
	uint32 iCall = xrtAtomic32FetchAdd(
		&pState->Calls,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	uint32 iFailAt = xrtAtomic32Load(
		&pState->FailAt,
		XMEMORY_ACQUIRE
	);

	if ( (iFailAt != 0) && (iCall == iFailAt) ) {
		xrtAtomic32Store(&pState->Hit, 1, XMEMORY_RELEASE);
		return true;
	}
	return false;
}



/* 在指定调用处拒绝分配，其余请求交给系统堆并计入存活块。 */
static ptr testResolverOomAlloc(ptr pData, size_t iSize)
{
	testresolveroomalloc* pState = (testresolveroomalloc*)pData;
	testresolveroomheader* pHeader;

	if ( testResolverOomShouldFail(pState) ) {
		return NULL;
	}
	if ( iSize > (SIZE_MAX - sizeof(*pHeader)) ) {
		return NULL;
	}
	pHeader = (testresolveroomheader*)malloc(sizeof(*pHeader) + iSize);
	if ( pHeader != NULL ) {
		pHeader->Value.Size = iSize;
		(void)xrtAtomic32FetchAdd(
			&pState->Live,
			1,
			XMEMORY_RELAXED
		);
		if ( iSize >= TEST_RESOLVER_OOM_DIRECT_SIZE ) {
			(void)xrtAtomic32FetchAdd(
				&pState->DirectLive,
				1,
				XMEMORY_RELAXED
			);
		}
	}
	return pHeader != NULL ? (ptr)(pHeader + 1) : NULL;
}



/* 重分配失败时保留原块，空指针成功时新增一个存活块。 */
static ptr testResolverOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testresolveroomalloc* pState = (testresolveroomalloc*)pData;
	testresolveroomheader* pHeader;
	testresolveroomheader* pResult;
	size_t iOldSize = 0;

	if ( testResolverOomShouldFail(pState) ) {
		return NULL;
	}
	if ( iSize > (SIZE_MAX - sizeof(*pHeader)) ) {
		return NULL;
	}
	pHeader = pMemory != NULL ?
		((testresolveroomheader*)pMemory - 1) : NULL;
	if ( pHeader != NULL ) {
		iOldSize = pHeader->Value.Size;
	}
	pResult = (testresolveroomheader*)realloc(
		pHeader,
		sizeof(*pHeader) + iSize
	);
	if ( pResult == NULL ) {
		return NULL;
	}
	pResult->Value.Size = iSize;
	if ( pMemory == NULL ) {
		(void)xrtAtomic32FetchAdd(
			&pState->Live,
			1,
			XMEMORY_RELAXED
		);
	}
	if ( (iOldSize < TEST_RESOLVER_OOM_DIRECT_SIZE) &&
		 (iSize >= TEST_RESOLVER_OOM_DIRECT_SIZE) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->DirectLive,
			1,
			XMEMORY_RELAXED
		);
	} else if ( (iOldSize >= TEST_RESOLVER_OOM_DIRECT_SIZE) &&
		 (iSize < TEST_RESOLVER_OOM_DIRECT_SIZE) ) {
		(void)xrtAtomic32FetchSub(
			&pState->DirectLive,
			1,
			XMEMORY_RELAXED
		);
	}
	return (ptr)(pResult + 1);
}



/* 释放测试块并检查存活计数没有下溢。 */
static void testResolverOomFree(ptr pData, ptr pMemory)
{
	testresolveroomalloc* pState = (testresolveroomalloc*)pData;
	testresolveroomheader* pHeader;
	uint32 iLive;

	if ( pMemory == NULL ) {
		return;
	}
	iLive = xrtAtomic32FetchSub(
		&pState->Live,
		1,
		XMEMORY_RELAXED
	);
	if ( iLive == 0 ) {
		testResolverOomFail(pState, 1);
	}
	pHeader = (testresolveroomheader*)pMemory - 1;
	if ( pHeader->Value.Size >= TEST_RESOLVER_OOM_DIRECT_SIZE ) {
		if ( xrtAtomic32FetchSub(
			&pState->DirectLive,
			1,
			XMEMORY_RELAXED
		) == 0 ) {
			testResolverOomFail(pState, 6);
		}
	}
	free(pHeader);
}



/* 自定义查询只构建一个地址列表，使 Worker 分配失败也进入正常失败终态。 */
static xnetaddrlist* testResolverOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testresolveroomquery* pQuery = (testresolveroomquery*)pData;
	xnetaddr Addresses[TEST_RESOLVER_OOM_ADDRESS_COUNT];

	(void)sHost;
	if ( !xrtNetAddrLoopback(&Addresses[0], Family, 0) ) {
		testResolverOomFail(pQuery->Allocator, 2);
		return NULL;
	}
	for ( uint32 i = 1; i < TEST_RESOLVER_OOM_ADDRESS_COUNT; i++ ) {
		Addresses[i] = Addresses[0];
	}
	return xrtNetAddrListCreate(
		Addresses,
		TEST_RESOLVER_OOM_ADDRESS_COUNT
	);
}



/* 已受理查询在 OOM 下也必须恰好进入成功或带错误的失败终态。 */
static void testResolverOomDone(
	xnetresolveop* pOperation,
	ptr pData
)
{
	testresolveroomquery* pQuery = (testresolveroomquery*)pData;
	xnetresolveopstate State = xrtNetResolveOpState(pOperation);
	xnetaddrlist* pAddresses;

	if ( State == XNET_RESOLVE_RESOLVED ) {
		pAddresses = xrtNetResolveOpResult(pOperation);
		if ( (pAddresses == NULL) ||
			 (xrtNetAddrListCount(pAddresses) != 1) ) {
			testResolverOomFail(pQuery->Allocator, 3);
		}
		xrtNetAddrListDestroy(pAddresses);
		(void)xrtAtomic32FetchAdd(
			&pQuery->Resolved,
			1,
			XMEMORY_RELEASE
		);
	} else if ( State == XNET_RESOLVE_FAILED ) {
		if ( xrtNetResolveOpError(pOperation) == NULL ) {
			testResolverOomFail(pQuery->Allocator, 4);
		}
		(void)xrtAtomic32FetchAdd(
			&pQuery->Failed,
			1,
			XMEMORY_RELEASE
		);
	} else {
		testResolverOomFail(pQuery->Allocator, 5);
	}
	(void)xrtAtomic32FetchAdd(
		&pQuery->Callbacks,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待指定回调完成且 Resolver 的内部工作全部归零。 */
static void testResolverOomWaitIdle(
	xnetresolver* pResolver,
	const testresolveroomquery* pQuery,
	uint32 iCallbacks
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		xnetresolverstats Stats;

		testRequire(xrtNetResolverStats(pResolver, &Stats),
			"resolver OOM stats failed");
		if ( (xrtAtomic32Load(
			&pQuery->Callbacks,
			XMEMORY_ACQUIRE
		) == iCallbacks) &&
			 (Stats.Outstanding == 0) &&
			 (Stats.ActiveQueries == 0) &&
			 (Stats.QueuedQueries == 0) &&
			 (Stats.RunningQueries == 0) &&
			 (Stats.ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"resolver OOM work did not become idle");
		xrtThreadYield();
	}
}



/* 逐个创建分配点故障注入，验证线程启动回滚和资源完整释放。 */
static void testResolverOomCreate(testresolveroomalloc* pState)
{
	xnetresolverconfig Config;
	uint32 iCovered = 0;

	xrtNetResolverConfigInit(&Config);
	Config.Workers = 1;
	/* 两张大桶表都走直通分配，稳定覆盖独立回滚阶段。 */
	Config.CacheEntries = 4096;
	for ( uint32 iOffset = 1; iOffset <= 64; iOffset++ ) {
		xnetresolver* pResolver;
		uint32 iBase = xrtAtomic32Load(
			&pState->Calls,
			XMEMORY_ACQUIRE
		);

		xrtAtomic32Store(&pState->Hit, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(
			&pState->FailAt,
			iBase + iOffset,
			XMEMORY_RELEASE
		);
		pResolver = xrtNetResolverCreate(&Config);
		xrtAtomic32Store(&pState->FailAt, 0, XMEMORY_RELEASE);
		if ( pResolver != NULL ) {
			testRequire(xrtNetResolverDestroy(pResolver),
				"resolver OOM successful create destroy failed");
		}
		xrtClearError();
		if ( xrtAtomic32Load(
			&pState->DirectLive,
			XMEMORY_ACQUIRE
		) != 0 ) {
			fprintf(
				stderr,
				"resolver create OOM leak: offset=%u fail_at=%u calls=%u live=%u direct=%u\n",
				(unsigned)iOffset,
				(unsigned)(iBase + iOffset),
				(unsigned)xrtAtomic32Load(&pState->Calls, XMEMORY_ACQUIRE),
				(unsigned)xrtAtomic32Load(&pState->Live, XMEMORY_ACQUIRE),
				(unsigned)xrtAtomic32Load(&pState->DirectLive, XMEMORY_ACQUIRE)
			);
		}
		testRequire(xrtAtomic32Load(
			&pState->DirectLive,
			XMEMORY_ACQUIRE
		) == 0, "resolver create OOM leaked direct memory");
		if ( xrtAtomic32Load(&pState->Hit, XMEMORY_ACQUIRE) == 0 ) {
			break;
		}
		iCovered++;
	}
	testRequire(iCovered >= 2,
		"resolver create OOM covered too few allocation points");
}



/* 逐个提交分配点故障注入，验证拒绝与 Worker 失败都保持原子回滚。 */
static void testResolverOomSubmit(testresolveroomalloc* pState)
{
	testresolveroomquery Query;
	xnetresolverconfig Config;
	xnetresolver* pResolver;
	uint32 iBaseDirect;
	uint32 iCovered = 0;

	memset(&Query, 0, sizeof(Query));
	Query.Allocator = pState;
	xrtAtomic32Init(&Query.Callbacks, 0);
	xrtAtomic32Init(&Query.Resolved, 0);
	xrtAtomic32Init(&Query.Failed, 0);
	xrtNetResolverConfigInit(&Config);
	Config.Workers = 1;
	Config.CacheEntries = 0;
	Config.Lookup = testResolverOomLookup;
	Config.LookupData = &Query;
	pResolver = xrtNetResolverCreate(&Config);
	testRequire(pResolver != NULL, "resolver OOM submit create failed");
	iBaseDirect = xrtAtomic32Load(
		&pState->DirectLive,
		XMEMORY_ACQUIRE
	);

	for ( uint32 iOffset = 1; iOffset <= 16; iOffset++ ) {
		xnetresolveop* pOperation;
		uint32 iBefore = xrtAtomic32Load(
			&Query.Callbacks,
			XMEMORY_ACQUIRE
		);
		uint32 iBase = xrtAtomic32Load(
			&pState->Calls,
			XMEMORY_ACQUIRE
		);
		char sHost[32];

		(void)snprintf(sHost, sizeof(sHost), "oom%u.test", iOffset);
		xrtAtomic32Store(&pState->Hit, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(
			&pState->FailAt,
			iBase + iOffset,
			XMEMORY_RELEASE
		);
		pOperation = xrtNetResolverResolve(
			pResolver,
			sHost,
			XNET_FAMILY_IPV4,
			testResolverOomDone,
			&Query
		);
		if ( pOperation != NULL ) {
			testResolverOomWaitIdle(pResolver, &Query, iBefore + 1u);
			xrtNetResolveOpDestroy(pOperation);
		} else {
			xrtAtomic32Store(&pState->FailAt, 0, XMEMORY_RELEASE);
			xrtClearError();
			testResolverOomWaitIdle(pResolver, &Query, iBefore);
		}
		xrtAtomic32Store(&pState->FailAt, 0, XMEMORY_RELEASE);
		xrtClearError();
		testRequire(xrtAtomic32Load(
			&pState->DirectLive,
			XMEMORY_ACQUIRE
		) == iBaseDirect, "resolver submit OOM leaked direct memory");
		if ( xrtAtomic32Load(&pState->Hit, XMEMORY_ACQUIRE) == 0 ) {
			break;
		}
		iCovered++;
	}
	testRequire(iCovered >= 1,
		"resolver submit OOM covered too few allocation points");
	testRequire(
		(xrtAtomic32Load(&Query.Resolved, XMEMORY_ACQUIRE) +
		 xrtAtomic32Load(&Query.Failed, XMEMORY_ACQUIRE)) ==
		xrtAtomic32Load(&Query.Callbacks, XMEMORY_ACQUIRE),
		"resolver submit OOM terminal accounting mismatch"
	);
	testRequire(xrtNetResolverDestroy(pResolver),
		"resolver OOM submit destroy failed");
	xrtClearError();
	testRequire(xrtAtomic32Load(
		&pState->DirectLive,
		XMEMORY_ACQUIRE
	) == 0, "resolver submit OOM retained direct resolver memory");
}



/* Resolver OOM 门禁遍历创建和查询分配点，并检查完整回滚。 */
int main(void)
{
	static testresolveroomalloc State;
	xallocator Allocator;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		bool bDebugEnabled = xrtMemDebugEnabled();
	#endif

	memset(&State, 0, sizeof(State));
	memset(&Allocator, 0, sizeof(Allocator));
	xrtAtomic32Init(&State.Calls, 0);
	xrtAtomic32Init(&State.FailAt, 0);
	xrtAtomic32Init(&State.Hit, 0);
	xrtAtomic32Init(&State.Live, 0);
	xrtAtomic32Init(&State.DirectLive, 0);
	xrtAtomic32Init(&State.Failure, 0);
	Allocator.Context = &State;
	Allocator.Alloc = testResolverOomAlloc;
	Allocator.Realloc = testResolverOomRealloc;
	Allocator.Free = testResolverOomFree;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		/* 底层回滚测试必须绕过调试隔离队列，才能观察真实释放时刻。 */
		if ( bDebugEnabled ) {
			testRequire(xrtMemDebugEnable(false),
				"resolver OOM could not suspend memory debug quarantine");
		}
	#endif
	testRequire(xrtSetAllocator(&Allocator),
		"resolver OOM allocator install failed");
	testResolverOomCreate(&State);
	testResolverOomSubmit(&State);
	testRequire(xrtAtomic32Load(&State.Failure, XMEMORY_ACQUIRE) == 0,
		"resolver OOM cross-thread contract failed");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		if ( bDebugEnabled ) {
			testRequire(xrtMemDebugEnable(true),
				"resolver OOM could not restore memory debugging");
		}
	#endif
	printf("[PASS] network resolver OOM\n");
	return 0;
}
