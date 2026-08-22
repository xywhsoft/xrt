#include "../test.h"

#include <xrt/websocket_server_router.h>



/* 记录成功移交给 WebSocket Router 的最终 Data 清理责任。 */
static void testWsServerRouterRelease(ptr pData)
{
	uint32* pReleases = (uint32*)pData;

	(*pReleases)++;
}



/* 验证 WebSocket Router 稳定错误域、代码和操作名。 */
static void testWsServerRouterError(
	xerrkind Kind,
	xwsserverroutererror Code,
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
			"xrt.websocket.server.router"
		 ) == 0) &&
		(strcmp(xrtErrorOperation(pError), sOperation) == 0),
		sMessage
	);
	xrtClearError();
}



/* 锁定配置预检、注册失败回滚和成功路由最终释放契约。 */
int main(void)
{
	uint8 ConfigStorage[sizeof(xwsserverrouteconfig) + 2u];
	xwsserverrouteconfig* pUnaligned =
		(xwsserverrouteconfig*)(void*)(ConfigStorage + 1u);
	const xwsserverrouteconfig* pWrappingConfig =
		(const xwsserverrouteconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	xstrview WrappingPattern = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xhttprouterconfig RouterConfig;
	xwsserverrouteconfig Config;
	xhttpserverrouter* pRouter;
	uint32 iOwnedReleases = 0;
	uint32 iFailedReleases = 0;

	xrtWsServerRouteConfigInit(NULL);
	testWsServerRouterError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		"config-init-websocket-server-route",
		"WebSocket server route ConfigInit error mismatch"
	);
	memset(ConfigStorage, 0xC3, sizeof(ConfigStorage));
	xrtWsServerRouteConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xC3)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xC3)) &&
		xrtWsServerConfigValid(&Config.Server),
		"WebSocket server Router rejected unaligned configuration storage"
	);
	xrtWsServerRouteConfigInit(
		(xwsserverrouteconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testWsServerRouterError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		"config-init-websocket-server-route",
		"WebSocket server Router initialized a wrapping configuration"
	);
	testRequire(
		!xrtWsConnConfigValid(NULL),
		"WebSocket Connection accepted a null configuration"
	);
	xrtWsServerRouteConfigInit(&Config);
	testRequire(
		xrtWsServerConfigValid(&Config.Server),
		"WebSocket default server configuration is invalid"
	);
	Config.Server.Connection.MessageLimit = 0;
	testRequire(
		!xrtWsServerConfigValid(&Config.Server),
		"WebSocket server accepted an invalid Connection configuration"
	);

	xrtHttpRouterConfigInit(&RouterConfig);
	RouterConfig.MaxRoutes = 1u;
	pRouter = xrtHttpServerRouterCreate(&RouterConfig);
	testRequire(
		pRouter != NULL,
		"WebSocket server Router fixture creation failed"
	);
	testRequire(
		!xrtWsServerRoute(
			pRouter,
			XRT_STR_LITERAL("/wrapping-config"),
			pWrappingConfig
		),
		"WebSocket server Router accepted a wrapping configuration"
	);
	testWsServerRouterError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		"register-websocket-server-route",
		"WebSocket server Router configuration range error mismatch"
	);
	testRequire(
		!xrtWsServerRoute(pRouter, WrappingPattern, NULL),
		"WebSocket server Router accepted a wrapping pattern"
	);
	testWsServerRouterError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		"register-websocket-server-route",
		"WebSocket server Router pattern range error mismatch"
	);
	Config.Release = testWsServerRouterRelease;
	Config.Data = &iFailedReleases;
	testRequire(
		!xrtWsServerRoute(
			pRouter,
			XRT_STR_LITERAL("/invalid"),
			&Config
		) && (iFailedReleases == 0),
		"WebSocket invalid route configuration transferred ownership"
	);
	testWsServerRouterError(
		XERR_VALUE,
		XWS_SERVER_ROUTER_ERROR_CONFIG,
		"register-websocket-server-route",
		"WebSocket invalid route configuration error mismatch"
	);

	xrtWsServerRouteConfigInit(&Config);
	Config.Server.Protocols = XRT_STR_LITERAL("chat.v2, chat.v1");
	Config.Release = testWsServerRouterRelease;
	Config.Data = &iOwnedReleases;
	memcpy(pUnaligned, &Config, sizeof(Config));
	testRequire(
		xrtWsServerRoute(
			pRouter,
			XRT_STR_LITERAL("/chat"),
			pUnaligned
		) &&
		(ConfigStorage[0] == UINT8_C(0xC3)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xC3)),
		"WebSocket server route registration failed"
	);
	Config.Data = &iFailedReleases;
	testRequire(
		!xrtWsServerRoute(
			pRouter,
			XRT_STR_LITERAL("/second"),
			&Config
		) && (iFailedReleases == 0),
		"WebSocket failed route registration transferred ownership"
	);
	testWsServerRouterError(
		XERR_RANGE,
		XWS_SERVER_ROUTER_ERROR_ROUTE,
		"register-websocket-server-route",
		"WebSocket route limit error mismatch"
	);
	testRequire(
		xrtHttpServerRouterFreeze(pRouter),
		"WebSocket server Router freeze failed"
	);
	xrtHttpServerRouterDestroy(pRouter);
	testRequire(
		(iOwnedReleases == 1u) &&
		(iFailedReleases == 0),
		"WebSocket server Router ownership release mismatch"
	);

	puts("[PASS] WebSocket server router contract");
	return 0;
}
