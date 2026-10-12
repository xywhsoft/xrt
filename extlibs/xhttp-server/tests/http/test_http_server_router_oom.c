#include "../test.h"

#include <xrt/http_server_router.h>



/* OOM 注册测试只保存回调地址，不会实际执行处理器。 */
static void testHttpServerRouterOomHandler(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
}



/* 注册失败必须保持高层回调表和通用索引的可见数量一致。 */
static bool testHttpServerRouterOomAdd(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern
)
{
	size_t iCount = xrtHttpServerRouterCount(pRouter);

	if ( xrtHttpServerRoute(
		pRouter,
		Method,
		Pattern,
		testHttpServerRouterOomHandler,
		NULL
	) ) {
		return true;
	}
	testRequire(
		(xrtHttpServerRouterCount(pRouter) == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server router Add OOM was not atomic"
	);
	return false;
}



/* 在一个逻辑故障序号下执行完整创建、注册和冻结。 */
static bool testHttpServerRouterOomAttempt(size_t iFail)
{
	xhttpserverrouter* pRouter;
	bool bComplete = false;
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"HTTP server router OOM fault setup failed"
	);
	pRouter = xrtHttpServerRouterCreate(NULL);
	if ( pRouter == NULL ) {
		goto Finish;
	}
	if ( !testHttpServerRouterOomAdd(
		pRouter,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/users/{id}")
	) || !testHttpServerRouterOomAdd(
		pRouter,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/users/{name}/detail")
	) || !testHttpServerRouterOomAdd(
		pRouter,
		XRT_STR_LITERAL("PUT"),
		XRT_STR_LITERAL("/files/{path...}")
	) ) {
		goto Finish;
	}
	if ( !xrtHttpServerRouterFreeze(pRouter) ) {
		testRequire(
			!xrtHttpServerRouterFrozen(pRouter) &&
			(xrtHttpServerRouterCount(pRouter) == 3u) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP server router Freeze OOM was not retryable"
		);
		goto Finish;
	}
	bComplete = true;

Finish:
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( bComplete ) {
		testRequire(
			!bTriggered,
			"HTTP server router ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP server router failed without injected OOM"
		);
		xrtClearError();
		if ( (pRouter != NULL) &&
			(xrtHttpServerRouterCount(pRouter) == 3u) ) {
			testRequire(
				xrtHttpServerRouterFreeze(pRouter),
				"HTTP server router Freeze retry failed"
			);
		}
	}
	xrtHttpServerRouterDestroy(pRouter);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP server router OOM attempt leaked storage"
	);
	return bComplete;
}



/* 启动适配器失败必须归还临时引用并保持调用方 Router 可用。 */
static void testHttpServerRouterStartOom(void)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	testRequire(
		(pRouter != NULL) && xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/ready"),
			testHttpServerRouterOomHandler,
			NULL
		) && xrtHttpServerRouterFreeze(pRouter),
		"HTTP server router start OOM fixture failed"
	);
	xrtMemDebugSnapshot(&Before);
	testRequire(
		xrtMemDebugFailAfter(0) &&
		(xrtHttpServerRouterStart(
			NULL, NULL, pRouter, NULL
		 ) == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server router runtime allocation OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire(
		(Before.LiveCount == After.LiveCount) &&
		(Before.LiveBytes == After.LiveBytes) &&
		xrtHttpServerRouterFrozen(pRouter) &&
		(xrtHttpServerRouterCount(pRouter) == 1u),
		"HTTP server router runtime OOM leaked or released Router"
	);
	testRequire(
		xrtHttpServerRouterStart(
			NULL, NULL, pRouter, NULL
		) == NULL,
		"HTTP server router accepted a null engine"
	);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire(
		(Before.LiveCount == After.LiveCount) &&
		(Before.LiveBytes == After.LiveBytes),
		"HTTP server router start failure leaked runtime state"
	);
	xrtHttpServerRouterDestroy(pRouter);
	testMemoryDebugDrain(
		"HTTP server router start OOM leaked storage"
	);
}



/* 逐个扫描逻辑分配点，直到第一轮未触发故障并完整成功。 */
int main(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 128u; iFail++ ) {
		if ( testHttpServerRouterOomAttempt(iFail) ) {
			testRequire(
				iFail != 0,
				"HTTP server router OOM path had no allocations"
			);
			testHttpServerRouterStartOom();
			printf(
				"[PASS] HTTP server router OOM (%u fault points)\n",
				(unsigned)iFail
			);
			return 0;
		}
	}
	testRequire(
		false,
		"HTTP server router OOM scan did not converge"
	);
	return 1;
}
