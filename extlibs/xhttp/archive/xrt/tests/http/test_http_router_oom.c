#include "../test.h"

#include <xrt/http_router.h>



/* 注册一条路由，并在失败时验证全部可见统计保持不变。 */
static bool testHttpRouterOomAdd(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	ptr pValue
)
{
	size_t iRoutes = xrtHttpRouterCount(pRouter);
	size_t iNodes = xrtHttpRouterNodes(pRouter);
	size_t iBytes = xrtHttpRouterBytes(pRouter);

	if ( xrtHttpRouterAdd(
		pRouter, Method, Pattern, pValue
	) ) {
		return true;
	}
	testRequire(
		(xrtHttpRouterCount(pRouter) == iRoutes) &&
		(xrtHttpRouterNodes(pRouter) == iNodes) &&
		(xrtHttpRouterBytes(pRouter) == iBytes) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP router Add OOM was not atomic"
	);
	return false;
}



/* 验证一次成功或恢复后的冻结 Router 仍能完成尾参数匹配。 */
static void testHttpRouterOomMatch(xhttprouter* pRouter)
{
	xhttproutermatch Match;
	xhttprouteparam Params[2];
	size_t iCount;

	testRequire(
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/files/a/b"),
			Params, 2, &iCount, &Match
		) == XHTTP_ROUTER_MATCH &&
		(Match.Value == (ptr)3) && (iCount == 1),
		"HTTP router OOM recovery match failed"
	);
}



/* 在给定逻辑故障序号执行完整创建、注册和冻结。 */
static bool testHttpRouterOomAttempt(size_t iFail)
{
	xhttprouter* pRouter;
	bool bComplete = false;
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"HTTP router OOM fault setup failed"
	);
	pRouter = xrtHttpRouterCreate(NULL);
	if ( pRouter == NULL ) {
		goto finish;
	}
	if ( !testHttpRouterOomAdd(
		pRouter, XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/users/{id}"), (ptr)1
	) || !testHttpRouterOomAdd(
		pRouter, XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/users/{name}/detail"), (ptr)2
	) || !testHttpRouterOomAdd(
		pRouter, XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/files/{path...}"), (ptr)3
	) ) {
		goto finish;
	}
	if ( !xrtHttpRouterFreeze(pRouter) ) {
		testRequire(
			!xrtHttpRouterFrozen(pRouter) &&
			(xrtHttpRouterCount(pRouter) == 3u) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP router Freeze OOM was not retryable"
		);
		goto finish;
	}
	bComplete = true;

finish:
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( bComplete ) {
		testRequire(
			!bTriggered,
			"HTTP router ignored a triggered allocation fault"
		);
		testHttpRouterOomMatch(pRouter);
	} else {
		testRequire(
			bTriggered &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP router failed without injected OOM"
		);
		xrtClearError();
		if ( (pRouter != NULL) &&
			(xrtHttpRouterCount(pRouter) == 3u) ) {
			testRequire(
				xrtHttpRouterFreeze(pRouter),
				"HTTP router Freeze retry failed"
			);
			testHttpRouterOomMatch(pRouter);
		}
	}
	xrtHttpRouterDestroy(pRouter);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP router OOM attempt leaked storage"
	);
	return bComplete;
}



/* 逐个扫描逻辑分配点，直到第一轮未触发故障并完整成功。 */
int main(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 64u; iFail++ ) {
		if ( testHttpRouterOomAttempt(iFail) ) {
			testRequire(
				iFail != 0,
				"HTTP router OOM path had no allocations"
			);
			printf(
				"[PASS] HTTP router OOM (%u fault points)\n",
				(unsigned int)iFail
			);
			return 0;
		}
	}
	testRequire(false, "HTTP router OOM scan did not converge");
	return 1;
}
