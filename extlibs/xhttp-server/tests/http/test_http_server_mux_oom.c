#include "../test.h"

#include <xrt/http_server_mux.h>



/* OOM 表测试只保存处理器地址，不执行网络回调。 */
static void testHttpServerMuxOomHandler(
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



/* 创建一个不受当前故障序号影响的冻结 Router fixture。 */
static xhttpserverrouter* testHttpServerMuxOomRouter(cstr sPath)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	testRequire(
		(pRouter != NULL) &&
		xrtHttpServerGet(
			pRouter,
			(xstrview){ sPath, strlen(sPath) },
			testHttpServerMuxOomHandler,
			NULL
		) && xrtHttpServerRouterFreeze(pRouter),
		"HTTP server mux OOM Router fixture failed"
	);
	return pRouter;
}



/* 失败的 Host 写入必须保持统计和原有 Router 完全不变。 */
static bool testHttpServerMuxOomHost(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter* pRouter
)
{
	xhttpservermuxstats Before;
	xhttpservermuxstats After;

	testRequire(
		xrtHttpServerMuxStats(pMux, &Before),
		"HTTP server mux OOM pre-write stats failed"
	);
	if ( xrtHttpServerMuxHost(pMux, Host, pRouter) ) {
		return true;
	}
	testRequire(
		xrtHttpServerMuxStats(pMux, &After) &&
		(Before.Hosts == After.Hosts) &&
		(Before.HostBytes == After.HostBytes) &&
		(Before.HasDefault == After.HasDefault) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server mux Host OOM was not atomic"
	);
	return false;
}



/* 扫描一次完整创建、默认站点、插入、替换和再次插入流程。 */
static bool testHttpServerMuxOomAttempt(size_t iFail)
{
	xhttpserverrouter* pOne =
		testHttpServerMuxOomRouter("/one");
	xhttpserverrouter* pTwo =
		testHttpServerMuxOomRouter("/two");
	xhttpservermux* pMux = NULL;
	xhttpserverrouter* pMatched = NULL;
	bool bComplete = false;
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"HTTP server mux OOM fault setup failed"
	);
	pMux = xrtHttpServerMuxCreate(NULL);
	if ( pMux == NULL ) {
		goto Finish;
	}
	if ( !xrtHttpServerMuxDefault(pMux, pOne) ||
		!testHttpServerMuxOomHost(
			pMux,
			XRT_STR_LITERAL("one.test"),
			pOne
		) ) {
		goto Finish;
	}
	if ( !testHttpServerMuxOomHost(
		pMux,
		XRT_STR_LITERAL("ONE.TEST"),
		pTwo
	) ) {
		testRequire(
			xrtHttpServerMuxMatch(
				pMux,
				XRT_STR_LITERAL("one.test"),
				&pMatched
			) == XHTTP_SERVER_MUX_HOST &&
			(pMatched == pOne),
			"HTTP server mux replacement OOM changed old Router"
		);
		xrtHttpServerRouterDestroy(pMatched);
		pMatched = NULL;
		goto Finish;
	}
	if ( !testHttpServerMuxOomHost(
		pMux,
		XRT_STR_LITERAL("two.test"),
		pTwo
	) ) {
		goto Finish;
	}
	bComplete = true;

Finish:
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( bComplete ) {
		testRequire(
			!bTriggered,
			"HTTP server mux ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP server mux failed without injected OOM"
		);
	}
	xrtClearError();
	xrtHttpServerRouterDestroy(pMatched);
	xrtHttpServerMuxDestroy(pMux);
	xrtHttpServerRouterDestroy(pOne);
	xrtHttpServerRouterDestroy(pTwo);
	testMemoryDebugDrain(
		"HTTP server mux OOM attempt leaked storage"
	);
	return bComplete;
}



/* Runtime 创建失败必须归还临时 Mux 引用。 */
static void testHttpServerMuxStartOom(void)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);

	testRequire(
		pMux != NULL,
		"HTTP server mux start OOM fixture failed"
	);
	xrtMemDebugSnapshot(&Before);
	testRequire(
		xrtMemDebugFailAfter(0) &&
		(xrtHttpServerMuxStart(
			NULL, NULL, pMux, NULL
		 ) == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server mux runtime allocation OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire(
		(Before.LiveCount == After.LiveCount) &&
		(Before.LiveBytes == After.LiveBytes),
		"HTTP server mux runtime OOM leaked state"
	);
	xrtHttpServerMuxDestroy(pMux);
	testMemoryDebugDrain(
		"HTTP server mux start OOM leaked storage"
	);
}



/* 扫描全部逻辑分配点，直到完整流程未触发故障。 */
int main(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 64u; iFail++ ) {
		if ( testHttpServerMuxOomAttempt(iFail) ) {
			testRequire(
				iFail != 0,
				"HTTP server mux OOM path had no allocations"
			);
			testHttpServerMuxStartOom();
			printf(
				"[PASS] HTTP server mux OOM (%u fault points)\n",
				(unsigned)iFail
			);
			return 0;
		}
	}
	testRequire(
		false,
		"HTTP server mux OOM scan did not converge"
	);
	return 1;
}
