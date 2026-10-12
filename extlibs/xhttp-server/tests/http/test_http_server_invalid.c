#include "../test.h"



/* 验证当前线程保存了预期 HTTP Server 错误。 */
static void testHttpServerInvalidError(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverstats ServerStats;
	xhttpconnstats ConnStats;
	xnetaddr Address;
	xnetaddr ExpectedAddress;
	xnetengine* pEngine;

	xrtClearError();
	xrtHttpServerConfigInit(NULL);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null config error mismatch"
	);
	xrtHttpServerEventsInit(NULL);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null events error mismatch"
	);
	xrtHttpServerConfigInit((xhttpserverconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server wrapping config init error mismatch"
	);
	xrtHttpServerEventsInit((xhttpserverevents*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server wrapping events init error mismatch"
	);
	testRequire(
		xrtHttpServerStart(NULL, NULL, NULL) == NULL,
		"HTTP server started without an engine"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null engine error mismatch"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		pEngine != NULL,
		"HTTP server invalid test engine create failed"
	);
	testRequire(xrtHttpServerStart(
		pEngine,
		(const xhttpserverconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL
	) == NULL, "HTTP server accepted wrapping config");
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server wrapping config error mismatch"
	);
	testRequire(xrtHttpServerStart(
		pEngine,
		NULL,
		(const xhttpserverevents*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP server accepted wrapping events");
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server wrapping events error mismatch"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	ServerConfig.WriteSize = 0;
	testRequire(
		xrtHttpServerStart(
			pEngine,
			&ServerConfig,
			NULL
		) == NULL,
		"HTTP server accepted zero write size"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTP server zero write size error mismatch"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	ServerConfig.WriteSize =
		ServerConfig.Network.Listen.Stream.WriteLimit + 1;
	testRequire(
		xrtHttpServerStart(
			pEngine,
			&ServerConfig,
			NULL
		) == NULL,
		"HTTP server accepted write size above TCP limit"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTP server write limit error mismatch"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	ServerConfig.MaxInformations = 0;
	testRequire(
		xrtHttpServerStart(
			pEngine,
			&ServerConfig,
			NULL
		) == NULL,
		"HTTP server accepted zero information limit"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTP server information limit error mismatch"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	ServerConfig.Http1.Head.MaxHead = 3;
	testRequire(
		xrtHttpServerStart(
			pEngine,
			&ServerConfig,
			NULL
		) == NULL,
		"HTTP server accepted invalid Exchange limits"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_CONFIG,
		"HTTP server Exchange config error mismatch"
	);

	testRequire(
		xrtHttpServerRef(NULL) == NULL,
		"HTTP server retained a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null retain error mismatch"
	);
	testRequire(
		!xrtHttpServerDrain(NULL),
		"HTTP server drained a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null drain error mismatch"
	);
	testRequire(
		!xrtHttpServerAbort(NULL),
		"HTTP server aborted a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null abort error mismatch"
	);
	testRequire(
		(xrtHttpServerState(NULL) == XHTTP_SERVER_CLOSED) &&
		(xrtHttpServerEndpointCount(NULL) == 0) &&
		(xrtHttpServerListenerCount(NULL) == 0) &&
		(xrtHttpServerError(NULL) == NULL),
		"HTTP server null state mismatch"
	);
	testRequire(
		xrtHttpServerNetwork(NULL) == NULL,
		"HTTP server returned a network for a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null network error mismatch"
	);
	memset(&Address, 0x5a, sizeof(Address));
	ExpectedAddress = Address;
	testRequire(
		!xrtHttpServerLocal(NULL, 0, &Address) &&
		(memcmp(
			&Address,
			&ExpectedAddress,
			sizeof(Address)
		 ) == 0),
		"HTTP server null local query changed output"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null local error mismatch"
	);
	memset(&ServerStats, 0x5a, sizeof(ServerStats));
	testRequire(
		!xrtHttpServerStats(NULL, &ServerStats) &&
		(ServerStats.Accepted ==
		 UINT64_C(0x5a5a5a5a5a5a5a5a)),
		"HTTP server null stats query changed output"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP server null stats error mismatch"
	);

	testRequire(
		xrtHttpConnRef(NULL) == NULL,
		"HTTP connection retained a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null retain error mismatch"
	);
	testRequire(
		(xrtHttpConnState(NULL) == XHTTP_CONN_CLOSED) &&
		(xrtHttpConnEndpoint(NULL) == SIZE_MAX) &&
		(xrtHttpConnServer(NULL) == NULL) &&
		(xrtHttpConnRequest(NULL) == NULL) &&
		(xrtHttpConnWorker(NULL) == NULL) &&
		(xrtHttpConnTcp(NULL) == NULL) &&
		(xrtHttpConnError(NULL) == NULL),
		"HTTP connection null query contract mismatch"
	);
	memset(&Address, 0x5a, sizeof(Address));
	ExpectedAddress = Address;
	testRequire(
		!xrtHttpConnLocal(NULL, &Address) &&
		(memcmp(
			&Address,
			&ExpectedAddress,
			sizeof(Address)
		 ) == 0),
		"HTTP connection null local query changed output"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null local error mismatch"
	);
	memset(&Address, 0x5a, sizeof(Address));
	ExpectedAddress = Address;
	testRequire(
		!xrtHttpConnRemote(NULL, &Address) &&
		(memcmp(
			&Address,
			&ExpectedAddress,
			sizeof(Address)
		 ) == 0),
		"HTTP connection null remote query changed output"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null remote error mismatch"
	);
	memset(&ConnStats, 0x5a, sizeof(ConnStats));
	testRequire(
		!xrtHttpConnStats(NULL, &ConnStats) &&
		(ConnStats.Requests ==
		 UINT64_C(0x5a5a5a5a5a5a5a5a)),
		"HTTP connection null stats query changed output"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null stats error mismatch"
	);
	testRequire(
		!xrtHttpConnSetRequestBodyLimit(NULL, 1),
		"HTTP connection limited a null request body"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null body limit error mismatch"
	);
	testRequire(
		!xrtHttpConnPauseRequestBody(NULL),
		"HTTP connection paused a null request body"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null body pause error mismatch"
	);
	testRequire(
		!xrtHttpConnResumeRequestBody(NULL),
		"HTTP connection resumed a null request body"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null body resume error mismatch"
	);
	testRequire(
		!xrtHttpConnRequestBodyPaused(NULL),
		"HTTP connection reported a null paused request body"
	);
	testRequire(
		xrtHttpConnInform(NULL, NULL) ==
		 XNET_RESULT_ERROR,
		"HTTP connection informed a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null information error mismatch"
	);
	testRequire(
		xrtHttpConnRespond(NULL, NULL) ==
		 XNET_RESULT_ERROR,
		"HTTP connection responded through a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null response error mismatch"
	);
	testRequire(
		xrtHttpConnReply(
			NULL,
			200,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("x")
		) == XNET_RESULT_ERROR,
		"HTTP connection Reply accepted a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null Reply error mismatch"
	);
	testRequire(
		!xrtHttpConnClose(NULL),
		"HTTP connection closed a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null close error mismatch"
	);
	testRequire(
		!xrtHttpConnAbort(NULL),
		"HTTP connection aborted a null object"
	);
	testHttpServerInvalidError(
		XERR_ARGUMENT,
		XHTTP_SERVER_ERROR_ARGUMENT,
		"HTTP connection null abort error mismatch"
	);

	xrtHttpServerDestroy(NULL);
	xrtHttpConnDestroy(NULL);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server invalid test engine destroy failed"
	);
	printf("[PASS] HTTP server invalid boundaries\n");
	return 0;
}
