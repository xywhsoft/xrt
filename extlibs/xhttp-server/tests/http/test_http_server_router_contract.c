#include "../test.h"

#include <xrt/http_server_router.h>

#include "../../src/internal/xrt_http_server_router.h"



/* 契约测试只验证注册和错误，不执行路由处理器。 */
static void testHttpServerRouterContractHandler(
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



/* 记录成功注册路由最终移交给 Router 的唯一清理责任。 */
static void testHttpServerRouterContractRelease(ptr pData)
{
	uint32* pReleases = (uint32*)pData;

	(*pReleases)++;
}



/* 验证服务端 Router 顶层错误的稳定分类、代码、域和操作名。 */
static void testHttpServerRouterContractError(
	xerrkind Kind,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(xrtErrorCode(pError) == (int32)Code) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server.router"
		 ) == 0) &&
		(strcmp(xrtErrorOperation(pError), sOperation) == 0),
		sMessage
	);
	xrtClearError();
}



/* 验证流式请求缓存会复制栈内参数，并直接接管极端模板的堆参数。 */
static void testHttpServerRouterMatchCache(void)
{
	xrt_http_server_route_match Match;
	xrt_http_server_route_match* pStored;
	xhttprouteparam* pHeap;
	size_t i;

	testRequire(
		__xrtHttpServerRouterMatchTake(NULL) == NULL,
		"HTTP server router cached a null match"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"cache-http-server-route",
		"HTTP server router null match cache error mismatch"
	);

	memset(&Match, 0, sizeof(Match));
	Match.Params = Match.Local;
	Match.Count = 2u;
	Match.Local[0].Name = XRT_STR_LITERAL("first");
	Match.Local[0].Value = XRT_STR_LITERAL("one");
	Match.Local[1].Name = XRT_STR_LITERAL("second");
	Match.Local[1].Value = XRT_STR_LITERAL("two");
	pStored = __xrtHttpServerRouterMatchTake(&Match);
	testRequire(
		(pStored != NULL) &&
		(Match.Params == NULL) &&
		(pStored->Params == pStored->Local) &&
		(pStored->Params != Match.Local) &&
		(pStored->Count == 2u) &&
		(pStored->Params[0].Name.Size == 5u) &&
		(memcmp(pStored->Params[0].Name.Data, "first", 5u) == 0) &&
		(pStored->Params[1].Value.Size == 3u) &&
		(memcmp(pStored->Params[1].Value.Data, "two", 3u) == 0),
		"HTTP server router local match cache ownership mismatch"
	);
	__xrtHttpServerRouterMatchDestroy(pStored);
	__xrtHttpServerRouterMatchClear(&Match);

	memset(&Match, 0, sizeof(Match));
	Match.Count = 9u;
	Match.Params = (xhttprouteparam*)xrtMalloc(
		Match.Count * sizeof(*Match.Params)
	);
	testRequire(
		Match.Params != NULL,
		"HTTP server router heap match fixture allocation failed"
	);
	pHeap = Match.Params;
	for ( i = 0; i < Match.Count; i++ ) {
		Match.Params[i].Name = XRT_STR_LITERAL("part");
		Match.Params[i].Value = XRT_STR_LITERAL("value");
	}
	pStored = __xrtHttpServerRouterMatchTake(&Match);
	testRequire(
		(pStored != NULL) &&
		(Match.Params == NULL) &&
		(pStored->Params == pHeap) &&
		(pStored->Params != pStored->Local) &&
		(pStored->Count == 9u) &&
		(pStored->Params[8].Value.Size == 5u) &&
		(memcmp(pStored->Params[8].Value.Data, "value", 5u) == 0),
		"HTTP server router heap match cache ownership mismatch"
	);
	__xrtHttpServerRouterMatchDestroy(pStored);
	__xrtHttpServerRouterMatchClear(&Match);
	__xrtHttpServerRouterMatchDestroy(NULL);
}



/* 验证 Router 配置和路由事件表支持未对齐存储并立即快照。 */
static void testHttpServerRouterMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xhttprouterconfig) + 2u];
	uint8 EventsStorage[sizeof(xhttpserverrouteevents) + 2u];
	xhttprouterconfig Config;
	xhttpserverrouteevents Events;
	xhttpserverrouter* pRouter;
	uint32 iReleases = 0;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(EventsStorage, 0xA5, sizeof(EventsStorage));
	xrtHttpRouterConfigInit(
		(xhttprouterconfig*)(void*)(ConfigStorage + 1u)
	);
	xrtHttpServerRouteEventsInit(
		(xhttpserverrouteevents*)(void*)(EventsStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	memcpy(&Events, EventsStorage + 1u, sizeof(Events));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(EventsStorage[0] == 0xA5) &&
		(EventsStorage[sizeof(EventsStorage) - 1u] == 0xA5) &&
		(Events.Request == NULL) && (Events.Release == NULL),
		"HTTP server router descriptor init did not support unaligned storage");
	Config.MaxRoutes = 1u;
	Events.Request = testHttpServerRouterContractHandler;
	Events.Release = testHttpServerRouterContractRelease;
	Events.Data = &iReleases;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memcpy(EventsStorage + 1u, &Events, sizeof(Events));
	pRouter = xrtHttpServerRouterCreate(
		(const xhttprouterconfig*)(const void*)(ConfigStorage + 1u)
	);
	testRequire((pRouter != NULL) && xrtHttpServerRouteEvents(
		pRouter,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/memory"),
		(const xhttpserverrouteevents*)(const void*)(EventsStorage + 1u)
	), "HTTP server router did not accept unaligned descriptors");
	testRequire(!xrtHttpServerRouteEvents(
		pRouter,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/wrapping"),
		(const xhttpserverrouteevents*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP server router accepted wrapping route events");
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"register-http-server-route",
		"HTTP server router wrapping route events error mismatch"
	);
	testRequire(xrtHttpServerRouterFreeze(pRouter),
		"HTTP server router memory fixture freeze failed");
	testRequire(xrtHttpServerRouterStart(
		NULL,
		NULL,
		pRouter,
		(const xhttpserverevents*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP server router accepted wrapping runtime events");
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"start-http-server-router",
		"HTTP server router wrapping runtime events error mismatch"
	);
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	memset(EventsStorage + 1u, 0, sizeof(Events));
	xrtHttpServerRouterDestroy(pRouter);
	testRequire(iReleases == 1u,
		"HTTP server router retained caller event storage");

	xrtHttpServerRouteEventsInit(
		(xhttpserverrouteevents*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"init-http-server-route-events",
		"HTTP server router events init accepted wrapping output"
	);
	testRequire(xrtHttpServerRouterCreate(
		(const xhttprouterconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP server router create accepted wrapping config");
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"create-http-server-router",
		"HTTP server router wrapping config error mismatch"
	);
}



/* 锁定无效参数、状态、目标、限额和启动错误的公共口径。 */
int main(void)
{
	xhttprouterconfig Config;
	xhttpserverrouter* pRouter;
	xhttpserverrouteevents Events;
	uint32 iReleases = 0;

	testHttpServerRouterMatchCache();
	testHttpServerRouterMemoryContracts();
	xrtHttpServerRouteEventsInit(NULL);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"init-http-server-route-events",
		"HTTP server router EventsInit error mismatch"
	);
	testRequire(
		xrtHttpServerRouterRef(NULL) == NULL,
		"HTTP server router retained null"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"retain-http-server-router",
		"HTTP server router Ref error mismatch"
	);
	testRequire(
		!xrtHttpServerRouterFreeze(NULL),
		"HTTP server router froze null"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"freeze-http-server-router",
		"HTTP server router null Freeze error mismatch"
	);

	xrtHttpRouterConfigInit(&Config);
	Config.MaxRoutes = 0;
	testRequire(
		xrtHttpServerRouterCreate(&Config) == NULL,
		"HTTP server router accepted zero route limit"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"create-http-server-router",
		"HTTP server router invalid config error mismatch"
	);

	xrtHttpRouterConfigInit(&Config);
	Config.MaxRoutes = 1u;
	pRouter = xrtHttpServerRouterCreate(&Config);
	testRequire(
		pRouter != NULL,
		"HTTP server router contract fixture create failed"
	);
	xrtHttpServerRouteEventsInit(&Events);
	testRequire(
		!xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/missing-handler"),
			&Events
		),
		"HTTP server router accepted a missing handler"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
		"register-http-server-route",
		"HTTP server router missing handler error mismatch"
	);
	Events.Request = testHttpServerRouterContractHandler;
	Events.Release = testHttpServerRouterContractRelease;
	Events.Data = &iReleases;
	testRequire(
		!xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("BAD METHOD"),
			XRT_STR_LITERAL("/bad"),
			&Events
		) && (xrtHttpServerRouterCount(pRouter) == 0) &&
		(iReleases == 0),
		"HTTP server router accepted an invalid method"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_TARGET,
		"register-http-server-route",
		"HTTP server router invalid method error mismatch"
	);
	testRequire(
		xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/one"),
			&Events
		),
		"HTTP server router contract route failed"
	);
	testRequire(
		!xrtHttpServerPost(
			pRouter,
			XRT_STR_LITERAL("/two"),
			testHttpServerRouterContractHandler,
			NULL
		) && (xrtHttpServerRouterCount(pRouter) == 1u),
		"HTTP server router exceeded configured route limit"
	);
	testHttpServerRouterContractError(
		XERR_RANGE,
		XHTTP_SERVER_ROUTER_ERROR_LIMIT,
		"register-http-server-route",
		"HTTP server router limit error mismatch"
	);
	testRequire(
		xrtHttpServerRouterFreeze(pRouter),
		"HTTP server router contract freeze failed"
	);
	testRequire(
		!xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/late"),
			testHttpServerRouterContractHandler,
			NULL
		),
		"HTTP server router accepted a route after freeze"
	);
	testHttpServerRouterContractError(
		XERR_STATE,
		XHTTP_SERVER_ROUTER_ERROR_STATE,
		"register-http-server-route",
		"HTTP server router frozen Add error mismatch"
	);
	testRequire(
		xrtHttpServerRouterStart(
			NULL, NULL, pRouter, NULL
		) == NULL,
		"HTTP server router started with a null engine"
	);
	testHttpServerRouterContractError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ROUTER_ERROR_START,
		"start-http-server-router",
		"HTTP server router start error mismatch"
	);
	testRequire(
		xrtHttpServerRouterFrozen(pRouter) &&
		(xrtHttpServerRouterCount(pRouter) == 1u) &&
		!xrtHttpServerRouterFrozen(NULL) &&
		(xrtHttpServerRouterCount(NULL) == 0),
		"HTTP server router query contract mismatch"
	);
	xrtHttpServerRouterDestroy(pRouter);
	testRequire(
		iReleases == 1u,
		"HTTP server router route ownership release mismatch"
	);

	pRouter = xrtHttpServerRouterCreate(NULL);
	testRequire(
		(pRouter != NULL) &&
		(xrtHttpServerRouterStart(
			NULL, NULL, pRouter, NULL
		 ) == NULL),
		"HTTP server router accepted an unfrozen Router"
	);
	testHttpServerRouterContractError(
		XERR_STATE,
		XHTTP_SERVER_ROUTER_ERROR_STATE,
		"start-http-server-router",
		"HTTP server router unfrozen start error mismatch"
	);
	xrtHttpServerRouterDestroy(pRouter);

	puts("[PASS] HTTP server router contract");
	return 0;
}
