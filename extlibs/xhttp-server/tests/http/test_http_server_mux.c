#include "../test.h"

#include <xrt/http_server_mux.h>



/* 表契约测试不会执行处理器。 */
static void testHttpServerMuxHandler(
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



/* 创建包含一个 GET 路由的冻结 Router。 */
static xhttpserverrouter* testHttpServerMuxRouter(cstr sPath)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	testRequire(
		(pRouter != NULL) &&
		xrtHttpServerGet(
			pRouter,
			(xstrview){ sPath, strlen(sPath) },
			testHttpServerMuxHandler,
			NULL
		) && xrtHttpServerRouterFreeze(pRouter),
		"HTTP server mux Router fixture failed"
	);
	return pRouter;
}



/* 验证稳定 Mux 错误域、代码和操作。 */
static void testHttpServerMuxError(
	xerrkind Kind,
	xhttpservermuxerror Code,
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
			"xrt.http.server.mux"
		 ) == 0) &&
		(strcmp(
			xrtErrorOperation(pError),
			sOperation
		 ) == 0),
		sMessage
	);
	xrtClearError();
}



/* 验证 Mux 固定描述符支持未对齐存储并在创建时完成快照。 */
static void testHttpServerMuxMemoryContracts(void)
{
	char HostStorage[32] = "memory.test";
	uint8 ConfigStorage[sizeof(xhttpservermuxconfig) + 2u];
	uint8 RouterStorage[sizeof(xhttpserverrouter*) + 2u];
	uint8 StatsStorage[sizeof(xhttpservermuxstats) + 2u];
	xhttpservermuxconfig Config;
	xhttpservermuxstats Stats;
	xhttpservermux* pMux;
	xhttpserverrouter* pRouter = testHttpServerMuxRouter("/memory");
	xhttpserverrouter* pMatched;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpServerMuxConfigInit(
		(xhttpservermuxconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.MaxHosts == 256u) &&
		(Config.MaxHostBytes == (64u * 1024u)),
		"HTTP server mux config did not support unaligned storage");
	Config.MaxHosts = 1u;
	Config.MaxHostBytes = 16u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	pMux = xrtHttpServerMuxCreate(
		(const xhttpservermuxconfig*)(const void*)(ConfigStorage + 1u)
	);
	testRequire(pMux != NULL,
		"HTTP server mux create did not support unaligned config");
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	testRequire(xrtHttpServerMuxHost(
		pMux, XRT_STR_LITERAL("memory.test"), pRouter
	), "HTTP server mux retained caller config storage");
	testRequire(
		xrtHttpServerMuxMatch(
			pMux,
			XRT_STR_LITERAL("memory.test"),
			(xhttpserverrouter**)(void*)pMux
		) == XHTTP_SERVER_MUX_ERROR,
		"HTTP server mux accepted object-overlapping match output"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"match-http-server-mux",
		"HTTP server mux match overlap error mismatch"
	);
	testRequire(
		(xrtHttpServerMuxMatch(
			pMux,
			(xstrview){ HostStorage, strlen(HostStorage) },
			(xhttpserverrouter**)(void*)(HostStorage + 1u)
		 ) == XHTTP_SERVER_MUX_ERROR) &&
		(strcmp(HostStorage, "memory.test") == 0),
		"HTTP server mux accepted Host-overlapping match output"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"match-http-server-mux",
		"HTTP server mux Host overlap error mismatch"
	);
	testRequire(
		!xrtHttpServerMuxStats(
			pMux,
			(xhttpservermuxstats*)(void*)pMux
		),
		"HTTP server mux accepted object-overlapping stats output"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"stat-http-server-mux",
		"HTTP server mux stats overlap error mismatch"
	);
	memset(RouterStorage, 0xA5, sizeof(RouterStorage));
	testRequire(xrtHttpServerMuxMatch(
		pMux,
		XRT_STR_LITERAL("MEMORY.TEST"),
		(xhttpserverrouter**)(void*)(RouterStorage + 1u)
	) == XHTTP_SERVER_MUX_HOST,
		"HTTP server mux match did not support unaligned output");
	memcpy(&pMatched, RouterStorage + 1u, sizeof(pMatched));
	testRequire((pMatched == pRouter) &&
		(RouterStorage[0] == 0xA5) &&
		(RouterStorage[sizeof(RouterStorage) - 1u] == 0xA5),
		"HTTP server mux match corrupted output guards");
	xrtHttpServerRouterDestroy(pMatched);
	memset(StatsStorage, 0xA5, sizeof(StatsStorage));
	testRequire(xrtHttpServerMuxStats(
		pMux,
		(xhttpservermuxstats*)(void*)(StatsStorage + 1u)
	), "HTTP server mux stats did not support unaligned output");
	memcpy(&Stats, StatsStorage + 1u, sizeof(Stats));
	testRequire((Stats.Hosts == 1u) &&
		(Stats.HostBytes == 11u) &&
		(StatsStorage[0] == 0xA5) &&
		(StatsStorage[sizeof(StatsStorage) - 1u] == 0xA5),
		"HTTP server mux stats corrupted output guards");
	testRequire(
		xrtHttpServerMuxMatch(
			pMux,
			XRT_STR_LITERAL("memory.test"),
			(xhttpserverrouter**)(uintptr_t)(UINTPTR_MAX - 1u)
		) == XHTTP_SERVER_MUX_ERROR,
		"HTTP server mux accepted wrapping match output"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"match-http-server-mux",
		"HTTP server mux wrapping match error mismatch"
	);
	testRequire(
		!xrtHttpServerMuxStats(
			pMux,
			(xhttpservermuxstats*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"HTTP server mux accepted wrapping stats output"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"stat-http-server-mux",
		"HTTP server mux wrapping stats error mismatch"
	);
	xrtHttpServerMuxDestroy(pMux);
	xrtHttpServerRouterDestroy(pRouter);

	xrtHttpServerMuxConfigInit((xhttpservermuxconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"init-http-server-mux-config",
		"HTTP server mux config accepted wrapping output"
	);
	testRequire(xrtHttpServerMuxCreate(
		(const xhttpservermuxconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP server mux create accepted wrapping config");
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"create-http-server-mux",
		"HTTP server mux wrapping config error mismatch"
	);
}



/* 锁定 Host 规范化、默认回退、热替换、限额和引用所有权。 */
int main(void)
{
	xhttpservermuxconfig Config;
	xhttpservermuxstats Stats;
	xhttpservermux* pMux;
	xhttpserverrouter* pDefault =
		testHttpServerMuxRouter("/default");
	xhttpserverrouter* pOne =
		testHttpServerMuxRouter("/one");
	xhttpserverrouter* pTwo =
		testHttpServerMuxRouter("/two");
	xhttpserverrouter* pMatched = NULL;
	xhttpserverrouter* pUnfrozen;

	testHttpServerMuxMemoryContracts();
	xrtHttpServerMuxConfigInit(NULL);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_ARGUMENT,
		"init-http-server-mux-config",
		"HTTP server mux null config error mismatch"
	);
	xrtHttpServerMuxConfigInit(&Config);
	Config.MaxHosts = 2u;
	Config.MaxHostBytes = 16u;
	pMux = xrtHttpServerMuxCreate(&Config);
	testRequire(
		(pMux != NULL) &&
		xrtHttpServerMuxDefault(pMux, pDefault) &&
		xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("One.Test"),
			pOne
		) && xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("[::1]"),
			pTwo
		),
		"HTTP server mux initial routes failed"
	);
	testRequire(
		xrtHttpServerMuxMatch(
			pMux,
			XRT_STR_LITERAL("ONE.TEST"),
			&pMatched
		) == XHTTP_SERVER_MUX_HOST &&
		(pMatched == pOne),
		"HTTP server mux case-insensitive Host mismatch"
	);
	xrtHttpServerRouterDestroy(pMatched);
	pMatched = NULL;
	testRequire(
		xrtHttpServerMuxMatch(
			pMux,
			XRT_STR_LITERAL("missing.test"),
			&pMatched
		) == XHTTP_SERVER_MUX_DEFAULT &&
		(pMatched == pDefault),
		"HTTP server mux default fallback mismatch"
	);
	xrtHttpServerRouterDestroy(pMatched);
	pMatched = NULL;
	testRequire(
		xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("one.test"),
			pTwo
		) && xrtHttpServerMuxMatch(
			pMux,
			XRT_STR_LITERAL("One.Test"),
			&pMatched
		) == XHTTP_SERVER_MUX_HOST &&
		(pMatched == pTwo),
		"HTTP server mux hot replacement mismatch"
	);
	xrtHttpServerRouterDestroy(pMatched);
	pMatched = NULL;
	testRequire(
		xrtHttpServerMuxStats(pMux, &Stats) &&
		(Stats.Hosts == 2u) &&
		(Stats.HostBytes == 11u) &&
		Stats.HasDefault,
		"HTTP server mux stats mismatch"
	);
	testRequire(
		!xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("three.test"),
			pOne
		),
		"HTTP server mux exceeded Host limit"
	);
	testHttpServerMuxError(
		XERR_RANGE,
		XHTTP_SERVER_MUX_ERROR_LIMIT,
		"set-http-server-mux-host",
		"HTTP server mux limit error mismatch"
	);
	testRequire(
		!xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("one.test:80"),
			pOne
		),
		"HTTP server mux accepted a registration port"
	);
	testHttpServerMuxError(
		XERR_PROTOCOL,
		XHTTP_SERVER_MUX_ERROR_HOST,
		"set-http-server-mux-host",
		"HTTP server mux Host error mismatch"
	);
	pUnfrozen = xrtHttpServerRouterCreate(NULL);
	testRequire(
		(pUnfrozen != NULL) &&
		!xrtHttpServerMuxDefault(pMux, pUnfrozen),
		"HTTP server mux accepted an unfrozen Router"
	);
	testHttpServerMuxError(
		XERR_STATE,
		XHTTP_SERVER_MUX_ERROR_STATE,
		"set-http-server-mux-default",
		"HTTP server mux Router state error mismatch"
	);
	xrtHttpServerRouterDestroy(pUnfrozen);
	testRequire(
		xrtHttpServerMuxRemove(
			pMux, XRT_STR_LITERAL("ONE.TEST")
		) == XHTTP_SERVER_MUX_HOST &&
		xrtHttpServerMuxRemove(
			pMux, XRT_STR_LITERAL("one.test")
		) == XHTTP_SERVER_MUX_NOT_FOUND,
		"HTTP server mux Host removal mismatch"
	);
	testRequire(
		(xrtHttpServerMuxStart(
			NULL, NULL, pMux, NULL
		 ) == NULL),
		"HTTP server mux accepted a null Engine"
	);
	testHttpServerMuxError(
		XERR_ARGUMENT,
		XHTTP_SERVER_MUX_ERROR_START,
		"start-http-server-mux",
		"HTTP server mux start error mismatch"
	);
	xrtHttpServerMuxDestroy(pMux);
	xrtHttpServerRouterDestroy(pDefault);
	xrtHttpServerRouterDestroy(pOne);
	xrtHttpServerRouterDestroy(pTwo);
	puts("[PASS] HTTP server mux");
	return 0;
}
