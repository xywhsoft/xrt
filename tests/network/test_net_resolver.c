#include "../test.h"



#define TEST_RESOLVER_REQUESTS 8u



typedef struct testresolver {
	xmutex Lock;
	xcond Condition;
	bool Block;
	bool Release;
	uint32 Calls;
	uint32 Waiting;
	uint32 Completed;
	uint32 Resolved;
	uint32 Failed;
	uint32 Cancelled;
	uint32 Unexpected;
	uint64 MainThread;
	bool CheckShared;
	xnetaddrlist* Shared;
} testresolver;



/* 在截止时间前等待测试计数到达目标。 */
static void testResolverWait(
	testresolver* pContext,
	uint32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver test lock failed");
	while ( *pValue < iExpected ) {
		testRequire(
			xrtCondWaitUntil(
				&pContext->Condition,
				&pContext->Lock,
				iDeadline
			) == XWAIT_OK,
			sMessage
		);
	}
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver test unlock failed");
}



/* 自定义查询过程提供可控阻塞、失败和完整地址列表。 */
static xnetaddrlist* testResolverLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testresolver* pContext = (testresolver*)pData;
	xnetaddr Address;
	xerror* pError;

	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver provider lock failed");
	pContext->Calls++;
	if ( pContext->Block && (strcmp(sHost, "missing.test") != 0) ) {
		pContext->Waiting++;
		testRequire(xrtCondBroadcast(&pContext->Condition),
			"resolver provider signal failed");
		while ( !pContext->Release ) {
			testRequire(
				xrtCondWait(&pContext->Condition, &pContext->Lock) ==
				XWAIT_OK,
				"resolver provider wait failed"
			);
		}
	}
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver provider unlock failed");

	if ( strcmp(sHost, "missing.test") == 0 ) {
		pError = xrtErrorCreate(
			XERR_NOT_FOUND,
			"test.resolver",
			1,
			"test host was not found"
		);
		testRequire(pError != NULL, "resolver provider error create failed");
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			Family == XNET_FAMILY_IPV6 ?
				XNET_FAMILY_IPV6 : XNET_FAMILY_IPV4,
			0
		),
		"resolver provider address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 收集终态并验证所有用户回调都离开提交线程执行。 */
static void testResolverDone(xnetresolveop* pOperation, ptr pData)
{
	testresolver* pContext = (testresolver*)pData;
	xnetresolveopstate State = xrtNetResolveOpState(pOperation);
	xnetaddrlist* pAddresses = NULL;
	bool bUnexpected = xrtThreadCurrentId() == pContext->MainThread;

	if ( State == XNET_RESOLVE_RESOLVED ) {
		pAddresses = xrtNetResolveOpResult(pOperation);
		if ( (pAddresses == NULL) ||
			 (xrtNetAddrListCount(pAddresses) == 0) ) {
			bUnexpected = true;
		}
	} else if ( State == XNET_RESOLVE_FAILED ) {
		if ( xrtNetResolveOpError(pOperation) == NULL ) {
			bUnexpected = true;
		}
	} else if ( State == XNET_RESOLVE_CANCELLED ) {
		if ( xrtErrorKind(xrtNetResolveOpError(pOperation)) !=
			 XERR_CANCELLED ) {
			bUnexpected = true;
		}
	} else {
		bUnexpected = true;
	}

	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver callback lock failed");
	if ( (pAddresses != NULL) && pContext->CheckShared ) {
		if ( pContext->Shared == NULL ) {
			pContext->Shared = pAddresses;
			pAddresses = NULL;
		} else if ( pContext->Shared != pAddresses ) {
			bUnexpected = true;
		}
	}
	pContext->Completed++;
	if ( State == XNET_RESOLVE_RESOLVED ) {
		pContext->Resolved++;
	} else if ( State == XNET_RESOLVE_FAILED ) {
		pContext->Failed++;
	} else if ( State == XNET_RESOLVE_CANCELLED ) {
		pContext->Cancelled++;
	}
	if ( bUnexpected ) {
		pContext->Unexpected++;
	}
	testRequire(xrtCondBroadcast(&pContext->Condition),
		"resolver callback signal failed");
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver callback unlock failed");
	xrtNetAddrListDestroy(pAddresses);
}



/* 修改自定义查询阻塞门并唤醒全部工作线程。 */
static void testResolverBlock(
	testresolver* pContext,
	bool bBlock,
	bool bRelease
)
{
	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver block lock failed");
	pContext->Block = bBlock;
	pContext->Release = bRelease;
	pContext->Waiting = 0;
	testRequire(xrtCondBroadcast(&pContext->Condition),
		"resolver block signal failed");
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver block unlock failed");
}



/* 等待 Resolver 的查询、回调和在途计数全部回到零。 */
static void testResolverWaitIdle(
	xnetresolver* pResolver,
	xnetresolverstats* pStats
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		testRequire(xrtNetResolverStats(pResolver, pStats),
			"resolver idle stats failed");
		if ( (pStats->Outstanding == 0) &&
			 (pStats->ActiveQueries == 0) &&
			 (pStats->QueuedQueries == 0) &&
			 (pStats->RunningQueries == 0) &&
			 (pStats->ReadyCallbacks == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"resolver did not become idle");
		xrtThreadYield();
	}
}



/* 验证合并、缓存、限流、取消、统计和排空销毁契约。 */
int main(void)
{
	testresolver Context;
	xnetresolverconfig Config;
	xnetresolverstats Stats;
	xnetresolver* pResolver;
	xnetresolveop* Operations[TEST_RESOLVER_REQUESTS];
	xnetresolveop* pOperation;
	xnetresolveop* pHold[3];
	char sLongHost[321];
	uint32 iCalls;

	memset(&Context, 0, sizeof(Context));
	testRequire(xrtMutexInit(&Context.Lock),
		"resolver test mutex init failed");
	testRequire(xrtCondInit(&Context.Condition),
		"resolver test condition init failed");
	Context.MainThread = xrtThreadCurrentId();
	Context.CheckShared = true;
	xrtNetResolverConfigInit(&Config);
	Config.Workers = 2;
	Config.RequestLimit = TEST_RESOLVER_REQUESTS;
	Config.QueryLimit = 3;
	Config.CacheEntries = 4;
	Config.Lookup = testResolverLookup;
	Config.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&Config);
	testRequire(pResolver != NULL, "resolver create failed");

	/* 同一主机的大小写变体必须合并为一次底层查询。 */
	testResolverBlock(&Context, true, false);
	for ( uint32 i = 0; i < TEST_RESOLVER_REQUESTS; i++ ) {
		Operations[i] = xrtNetResolverResolve(
			pResolver,
			(i & 1u) != 0 ? "COALESCE.TEST" : "coalesce.test",
			XNET_FAMILY_UNSPEC,
			testResolverDone,
			&Context
		);
		testRequire(Operations[i] != NULL,
			"resolver coalesced submit failed");
	}
	testRequire(
		xrtNetResolverResolve(
			pResolver,
			"coalesce.test",
			XNET_FAMILY_UNSPEC,
			testResolverDone,
			&Context
		) == NULL,
		"resolver request limit was not enforced"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"resolver request limit error mismatch");
	xrtClearError();
	testResolverWait(&Context, &Context.Waiting, 1,
		"resolver provider did not block");
	testRequire(xrtNetResolveOpCancel(Operations[0]),
		"resolver cancellation failed");
	testResolverWait(&Context, &Context.Completed, 1,
		"resolver cancellation callback was not dispatched");
	testResolverBlock(&Context, true, true);
	testResolverWait(&Context, &Context.Completed,
		TEST_RESOLVER_REQUESTS,
		"resolver coalesced callbacks did not complete");
	testRequire(Context.Calls == 1,
		"resolver did not coalesce normalized host queries");
	testRequire((Context.Resolved == 7) && (Context.Cancelled == 1),
		"resolver coalesced terminal counts mismatch");
	for ( uint32 i = 0; i < TEST_RESOLVER_REQUESTS; i++ ) {
		xrtNetResolveOpDestroy(Operations[i]);
	}

	/* 成功和失败缓存都必须共享完整终态并避免重复查询。 */
	testResolverBlock(&Context, false, true);
	pOperation = xrtNetResolverResolve(
		pResolver,
		"Coalesce.Test",
		XNET_FAMILY_UNSPEC,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL, "resolver cache hit submit failed");
	testResolverWait(&Context, &Context.Completed, 9,
		"resolver cache hit callback failed");
	testRequire(Context.Calls == 1,
		"resolver success cache missed normalized key");
	xrtNetResolveOpDestroy(pOperation);
	Context.CheckShared = false;

	pOperation = xrtNetResolverResolve(
		pResolver,
		"missing.test",
		XNET_FAMILY_UNSPEC,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL, "resolver failure submit failed");
	testResolverWait(&Context, &Context.Completed, 10,
		"resolver failure callback failed");
	testRequire(
		xrtErrorKind(xrtNetResolveOpError(pOperation)) == XERR_NOT_FOUND,
		"resolver failure error was not preserved"
	);
	xrtNetResolveOpDestroy(pOperation);
	pOperation = xrtNetResolverResolve(
		pResolver,
		"MISSING.TEST",
		XNET_FAMILY_UNSPEC,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL, "resolver failure cache submit failed");
	testResolverWait(&Context, &Context.Completed, 11,
		"resolver failure cache callback failed");
	testRequire(Context.Calls == 2,
		"resolver failure cache performed another query");
	xrtNetResolveOpDestroy(pOperation);

	/* 清空缓存后必须重新执行底层查询。 */
	testRequire(xrtNetResolverClear(pResolver),
		"resolver cache clear failed");
	pOperation = xrtNetResolverResolve(
		pResolver,
		"coalesce.test",
		XNET_FAMILY_UNSPEC,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL, "resolver submit after clear failed");
	testResolverWait(&Context, &Context.Completed, 12,
		"resolver callback after clear failed");
	testRequire(Context.Calls == 3,
		"resolver clear retained a stale cache entry");
	xrtNetResolveOpDestroy(pOperation);

	/* 两个 Worker 阻塞时，第三个唯一查询排队，第四个必须被硬限流。 */
	testResolverBlock(&Context, true, false);
	iCalls = Context.Calls;
	for ( uint32 i = 0; i < 3; i++ ) {
		char sHost[16];

		(void)snprintf(sHost, sizeof(sHost), "hold%u.test", i);
		pHold[i] = xrtNetResolverResolve(
			pResolver,
			sHost,
			XNET_FAMILY_IPV4,
			testResolverDone,
			&Context
		);
		testRequire(pHold[i] != NULL,
			"resolver unique query submit failed");
	}
	testResolverWait(&Context, &Context.Waiting, 2,
		"resolver workers did not enter blocking lookups");
	testRequire(
		xrtNetResolverResolve(
			pResolver,
			"hold3.test",
			XNET_FAMILY_IPV4,
			testResolverDone,
			&Context
		) == NULL,
		"resolver unique query limit was not enforced"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"resolver unique query limit error mismatch");
	xrtClearError();
	testRequire(xrtNetResolveOpCancel(pHold[2]),
		"resolver queued cancellation failed");
	testResolverBlock(&Context, true, true);
	testResolverWait(&Context, &Context.Completed, 15,
		"resolver limited query callbacks did not finish");
	testRequire(Context.Calls == (iCalls + 2u),
		"resolver executed a cancelled queued query");
	for ( uint32 i = 0; i < 3; i++ ) {
		xrtNetResolveOpDestroy(pHold[i]);
	}

	/* 统计必须回到零在途，并记录命中、合并、拒绝和取消。 */
	testResolverWaitIdle(pResolver, &Stats);
	testRequire(
		(Stats.Outstanding == 0) && (Stats.ActiveQueries == 0) &&
		(Stats.QueuedQueries == 0) && (Stats.RunningQueries == 0) &&
		(Stats.ReadyCallbacks == 0),
		"resolver stats retained completed work"
	);
	testRequire(
		(Stats.CacheHits >= 2) && (Stats.Coalesced >= 7) &&
		(Stats.Rejected >= 2) && (Stats.Cancelled == 2),
		"resolver stats counters mismatch"
	);

	/* 超过栈内规范化容量的主机名也必须正确查询并命中缓存。 */
	memset(sLongHost, 'A', sizeof(sLongHost) - 1u);
	memcpy(
		sLongHost + sizeof(sLongHost) - 6u,
		".TEST",
		6u
	);
	iCalls = Context.Calls;
	pOperation = xrtNetResolverResolve(
		pResolver,
		sLongHost,
		XNET_FAMILY_IPV4,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL,
		"resolver long host submit failed");
	testResolverWait(&Context, &Context.Completed, 16,
		"resolver long host callback failed");
	testRequire(Context.Calls == (iCalls + 1u),
		"resolver long host query was not executed");
	xrtNetResolveOpDestroy(pOperation);
	for ( size_t i = 0; i < sizeof(sLongHost) - 1u; i++ ) {
		if ( (sLongHost[i] >= 'A') && (sLongHost[i] <= 'Z') ) {
			sLongHost[i] = (char)(sLongHost[i] + ('a' - 'A'));
		}
	}
	pOperation = xrtNetResolverResolve(
		pResolver,
		sLongHost,
		XNET_FAMILY_IPV4,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL,
		"resolver long host cache submit failed");
	testResolverWait(&Context, &Context.Completed, 17,
		"resolver long host cache callback failed");
	testRequire(Context.Calls == (iCalls + 1u),
		"resolver long host cache missed normalized key");
	xrtNetResolveOpDestroy(pOperation);

	/* Destroy 必须排空最后一个已受理查询，操作结果可比 Resolver 活得更久。 */
	testResolverBlock(&Context, false, true);
	pOperation = xrtNetResolverResolve(
		pResolver,
		"drain.test",
		XNET_FAMILY_IPV4,
		testResolverDone,
		&Context
	);
	testRequire(pOperation != NULL, "resolver drain submit failed");
	testRequire(xrtNetResolverDestroy(pResolver),
		"resolver destroy failed");
	testRequire(Context.Completed == 18,
		"resolver destroy did not drain accepted callback");
	testRequire(xrtNetResolveOpState(pOperation) == XNET_RESOLVE_RESOLVED,
		"resolver drain operation did not resolve");
	xrtNetResolveOpDestroy(pOperation);

	testRequire(Context.Unexpected == 0,
		"resolver callback contract mismatch");
	xrtNetAddrListDestroy(Context.Shared);
	testRequire(xrtCondUnit(&Context.Condition),
		"resolver test condition unit failed");
	testRequire(xrtMutexUnit(&Context.Lock),
		"resolver test mutex unit failed");
	printf("[PASS] network resolver\n");
	return 0;
}
