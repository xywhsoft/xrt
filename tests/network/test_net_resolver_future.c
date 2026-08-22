#include "../test.h"



typedef struct testresolverfuture {
	xmutex Lock;
	xcond Condition;
	bool Block;
	bool Release;
	uint32 Entered;
} testresolverfuture;



/* Future 测试查询过程支持成功、失败和不可中断阻塞。 */
static xnetaddrlist* testResolverFutureLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testresolverfuture* pContext = (testresolverfuture*)pData;
	xnetaddr Address;

	if ( strcmp(sHost, "missing.test") == 0 ) {
		xerror* pError = xrtErrorCreate(
			XERR_NOT_FOUND,
			"test.resolver.future",
			1,
			"test host was not found"
		);

		testRequire(pError != NULL,
			"resolver Future provider error create failed");
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver Future provider lock failed");
	if ( pContext->Block ) {
		pContext->Entered++;
		testRequire(xrtCondBroadcast(&pContext->Condition),
			"resolver Future provider signal failed");
		while ( !pContext->Release ) {
			testRequire(
				xrtCondWait(&pContext->Condition, &pContext->Lock) ==
				XWAIT_OK,
				"resolver Future provider wait failed"
			);
		}
	}
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver Future provider unlock failed");
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			Family == XNET_FAMILY_IPV6 ?
				XNET_FAMILY_IPV6 : XNET_FAMILY_IPV4,
			0
		),
		"resolver Future provider address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 等待阻塞查询进入自定义过程。 */
static void testResolverFutureWaitEntered(testresolverfuture* pContext)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	testRequire(xrtMutexLock(&pContext->Lock),
		"resolver Future wait lock failed");
	while ( pContext->Entered == 0 ) {
		testRequire(
			xrtCondWaitUntil(
				&pContext->Condition,
				&pContext->Lock,
				iDeadline
			) == XWAIT_OK,
			"resolver Future provider did not start"
		);
	}
	testRequire(xrtMutexUnlock(&pContext->Lock),
		"resolver Future wait unlock failed");
}



/* 验证解析 Future 的成功值、失败错误和双向取消。 */
int main(void)
{
	testresolverfuture Context;
	xnetresolverconfig Config;
	xnetresolver* pResolver;
	xfuture* pFuture;
	xnetaddrlist* pAddresses;
	const xnetaddr* pAddress;

	memset(&Context, 0, sizeof(Context));
	testRequire(xrtMutexInit(&Context.Lock),
		"resolver Future test mutex init failed");
	testRequire(xrtCondInit(&Context.Condition),
		"resolver Future test condition init failed");
	xrtNetResolverConfigInit(&Config);
	Config.Workers = 2;
	Config.Lookup = testResolverFutureLookup;
	Config.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&Config);
	testRequire(pResolver != NULL, "resolver Future resolver create failed");

	/* 成功列表由 Future 持有，调用方读取借用结果。 */
	pFuture = xrtNetResolveAsync(
		pResolver,
		"success.test",
		XNET_FAMILY_IPV4
	);
	testRequire(pFuture != NULL, "resolver Future create failed");
	testRequire(xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK,
		"resolver Future success wait failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"resolver Future success state mismatch");
	pAddresses = (xnetaddrlist*)xrtFutureValue(pFuture);
	pAddress = pAddresses != NULL ?
		xrtNetAddrListGet(pAddresses, 0) : NULL;
	testRequire(
		(pAddresses != NULL) &&
		(xrtNetAddrListCount(pAddresses) == 1) &&
		(pAddress != NULL) && (pAddress->Port == 0),
		"resolver Future success value mismatch"
	);
	xrtFutureDestroy(pFuture);

	/* 查询失败保留提供者的结构化错误。 */
	pFuture = xrtNetResolveAsync(
		pResolver,
		"missing.test",
		XNET_FAMILY_UNSPEC
	);
	testRequire(pFuture != NULL, "resolver failed Future create failed");
	testRequire(xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK,
		"resolver failed Future wait failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_FAILED,
		"resolver failed Future state mismatch");
	testRequire(xrtErrorKind(xrtFutureError(pFuture)) == XERR_NOT_FOUND,
		"resolver failed Future error mismatch");
	xrtFutureDestroy(pFuture);

	/* FutureCancel 请求必须把底层订阅者推进取消终态。 */
	testRequire(xrtMutexLock(&Context.Lock),
		"resolver Future block lock failed");
	Context.Block = true;
	Context.Release = false;
	Context.Entered = 0;
	testRequire(xrtMutexUnlock(&Context.Lock),
		"resolver Future block unlock failed");
	pFuture = xrtNetResolveAsync(
		pResolver,
		"cancel.test",
		XNET_FAMILY_IPV4
	);
	testRequire(pFuture != NULL, "resolver cancelled Future create failed");
	testResolverFutureWaitEntered(&Context);
	testRequire(xrtFutureCancel(pFuture),
		"resolver Future cancel request failed");
	testRequire(xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK,
		"resolver cancelled Future wait failed");
	testRequire(xrtFutureState(pFuture) == XFUTURE_CANCELLED,
		"resolver cancelled Future state mismatch");
	xrtFutureDestroy(pFuture);

	/* 释放不可中断提供者后，Destroy 仍须完整排空底层查询。 */
	testRequire(xrtMutexLock(&Context.Lock),
		"resolver Future release lock failed");
	Context.Release = true;
	testRequire(xrtCondBroadcast(&Context.Condition),
		"resolver Future release signal failed");
	testRequire(xrtMutexUnlock(&Context.Lock),
		"resolver Future release unlock failed");
	testRequire(xrtNetResolverDestroy(pResolver),
		"resolver Future resolver destroy failed");
	testRequire(xrtCondUnit(&Context.Condition),
		"resolver Future test condition unit failed");
	testRequire(xrtMutexUnit(&Context.Lock),
		"resolver Future test mutex unit failed");
	printf("[PASS] network resolver Future\n");
	return 0;
}
