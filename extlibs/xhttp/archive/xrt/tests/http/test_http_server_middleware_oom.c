#include "../test.h"

#include <xrt/http_server_middleware.h>



/* OOM 测试回调不会执行，只用于构造完整中间件记录。 */
static bool testHttpServerMiddlewareOomHandler(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pNext;
	(void)pData;
	return true;
}



/* 每个成功注册的拥有型记录在 Router 终态释放一次。 */
static void testHttpServerMiddlewareOomRelease(ptr pData)
{
	size_t* pReleased = (size_t*)pData;

	(*pReleased)++;
}



/* 在一个逻辑分配序号下注册九层，以跨过三次容量增长。 */
static bool testHttpServerMiddlewareOomAttempt(size_t iFail)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);
	size_t iReleased = 0;
	size_t iRegistered = 0;
	bool bComplete = true;
	bool bTriggered;
	size_t i;

	testRequire(
		pRouter != NULL,
		"HTTP server middleware OOM Router create failed"
	);
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"HTTP server middleware OOM fault setup failed"
	);
	for ( i = 0; i < 9u; i++ ) {
		if ( !xrtHttpServerUseOwned(
			pRouter,
			testHttpServerMiddlewareOomHandler,
			&iReleased,
			testHttpServerMiddlewareOomRelease
		) ) {
			bComplete = false;
			testRequire(
				(xrtHttpServerMiddlewareCount(pRouter) ==
				 iRegistered) &&
				(iReleased == 0) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP server middleware OOM registration was not atomic"
			);
			break;
		}
		iRegistered++;
	}
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( bComplete ) {
		testRequire(
			!bTriggered &&
			(iRegistered == 9u),
			"HTTP server middleware ignored an injected allocation fault"
		);
	} else {
		testRequire(
			bTriggered,
			"HTTP server middleware failed without injected OOM"
		);
		xrtClearError();
	}
	xrtHttpServerRouterDestroy(pRouter);
	testRequire(
		iReleased == iRegistered,
		"HTTP server middleware OOM release count mismatch"
	);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP server middleware OOM attempt leaked storage"
	);
	return bComplete;
}



/* 验证参数、冻结、容量和空 Next 的稳定错误分类。 */
static void testHttpServerMiddlewareContract(void)
{
	xhttprouterconfig Config;
	xhttpserverrouter* pRouter;

	testRequire(
		!xrtHttpServerUse(NULL, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT),
		"HTTP server middleware null registration error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtHttpServerNext(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT),
		"HTTP server middleware null Next error mismatch"
	);
	xrtClearError();
	xrtHttpRouterConfigInit(&Config);
	Config.MaxRoutes = 2u;
	pRouter = xrtHttpServerRouterCreate(&Config);
	testRequire(
		(pRouter != NULL) &&
		xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareOomHandler,
			NULL
		) && xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareOomHandler,
			NULL
		) && !xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareOomHandler,
			NULL
		) &&
		(xrtHttpServerMiddlewareCount(pRouter) == 2u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT),
		"HTTP server middleware count limit mismatch"
	);
	xrtClearError();
	xrtHttpServerRouterDestroy(pRouter);
	testMemoryDebugDrain(
		"HTTP server middleware contract leaked storage"
	);
}



/* 逐分配点扫描中间件记录增长，并运行非分配错误契约。 */
int main(void)
{
	size_t iFail;

	testHttpServerMiddlewareContract();
	for ( iFail = 0; iFail < 16u; iFail++ ) {
		if ( testHttpServerMiddlewareOomAttempt(iFail) ) {
			testRequire(
				iFail == 3u,
				"HTTP server middleware allocation count drifted"
			);
			printf(
				"[PASS] HTTP server middleware OOM (%u fault points)\n",
				(unsigned)iFail
			);
			return 0;
		}
	}
	testRequire(
		false,
		"HTTP server middleware OOM scan did not converge"
	);
	return 1;
}
