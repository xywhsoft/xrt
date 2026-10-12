#include "../test.h"

#include "../../src/internal/xrt_websocket_server_router.h"



/* 记录成功移交给 WebSocket Router 的用户数据最终释放次数。 */
static void testWsServerRouterOomRelease(ptr pData)
{
	uint32* pReleases = (uint32*)pData;

	(*pReleases)++;
}



/* 在一个逻辑故障序号下执行完整 Router 创建和 WebSocket 路由注册。 */
static bool testWsServerRouterOomAttempt(size_t iFail)
{
	xwsserverrouteconfig Config;
	xhttpserverrouter* pRouter;
	uint32 iReleases = 0;
	bool bComplete = false;
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"WebSocket server router OOM fault setup failed"
	);
	pRouter = xrtHttpServerRouterCreate(NULL);
	if ( pRouter == NULL ) {
		goto Finish;
	}
	xrtWsServerRouteConfigInit(&Config);
	Config.Server.Protocols = XRT_STR_LITERAL("chat.v2, chat.v1");
	Config.Release = testWsServerRouterOomRelease;
	Config.Data = &iReleases;
	if ( !xrtWsServerRoute(
		pRouter,
		XRT_STR_LITERAL("/chat/{room}"),
		&Config
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
			"WebSocket server router ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(iReleases == 0),
			"WebSocket server router OOM was not atomic"
		);
	}
	xrtHttpServerRouterDestroy(pRouter);
	testRequire(
		iReleases == (bComplete ? 1u : 0u),
		"WebSocket server router OOM ownership mismatch"
	);
	xrtClearError();
	testMemoryDebugDrain(
		"WebSocket server router OOM attempt leaked storage"
	);
	return bComplete;
}



/* 精确命中每次 Upgrade 上下文分配并验证路由引用不会泄漏。 */
static void testWsServerRouterUpgradeContextOom(void)
{
	xrt_ws_server_route* pRoute;
	xrt_ws_server_route_connection* pConnection;
	uint32 iReleases = 0;

	pRoute = (xrt_ws_server_route*)xrtCalloc(
		1, sizeof(*pRoute) + 1u
	);
	testRequire(
		pRoute != NULL,
		"WebSocket server router OOM route fixture failed"
	);
	pRoute->References = 1;
	xrtWsServerRouteConfigInit(&pRoute->Config);
	pRoute->Config.Release = testWsServerRouterOomRelease;
	pRoute->Config.Data = &iReleases;

	testRequire(
		xrtMemDebugFailAfter(0) &&
		(__xrtWsServerRouteConnectionCreate(pRoute) == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(pRoute->References == 1) &&
		(iReleases == 0),
		"WebSocket Upgrade context OOM retained the route"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	pRoute->References = INT32_MAX;
	testRequire(
		(__xrtWsServerRouteConnectionCreate(pRoute) == NULL) &&
		(pRoute->References == INT32_MAX) &&
		(iReleases == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XWS_SERVER_ROUTER_ERROR_STATE),
		"WebSocket saturated route reference was transferred"
	);
	pRoute->References = 1;
	xrtClearError();

	pConnection = __xrtWsServerRouteConnectionCreate(pRoute);
	testRequire(
		(pConnection != NULL) &&
		(pRoute->References == 2),
		"WebSocket Upgrade context retry failed"
	);
	__xrtWsServerRouteRelease(pRoute);
	testRequire(
		iReleases == 0,
		"WebSocket active connection released route data early"
	);
	__xrtWsServerRouteConnectionRelease(pConnection);
	testRequire(
		iReleases == 1u,
		"WebSocket active connection did not release route data"
	);
	testMemoryDebugDrain(
		"WebSocket Upgrade context OOM leaked storage"
	);
}



/* 扫描全部注册分配点，再验证单次 Upgrade 上下文的精确失败边界。 */
int main(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 128u; iFail++ ) {
		if ( testWsServerRouterOomAttempt(iFail) ) {
			testRequire(
				iFail != 0,
				"WebSocket server router OOM path had no allocations"
			);
			testWsServerRouterUpgradeContextOom();
			printf(
				"[PASS] WebSocket server router OOM (%u fault points)\n",
				(unsigned)iFail
			);
			return 0;
		}
	}
	testRequire(
		false,
		"WebSocket server router OOM scan did not converge"
	);
	return 1;
}
