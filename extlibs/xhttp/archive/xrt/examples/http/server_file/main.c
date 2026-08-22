#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 文件服务示例状态由 Server 事件表借用。 */
typedef struct example_http_file {
	xtaskpool* Pool;
	cstr Path;
} example_http_file;



/* 用一行 Helper 发送指定文件，受理失败时返回固定错误正文。 */
static void exampleHttpFileRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	example_http_file* pState =
		(example_http_file*)pData;

	(void)pServer;
	(void)pRequest;
	if ( xrtHttpConnFile(
		pConnection,
		pState->Pool,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL(
			"application/octet-stream"
		),
		pState->Path
	) ) {
		return;
	}
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_SERVICE_UNAVAILABLE,
		XRT_STR_LITERAL(
			"text/plain; charset=utf-8"
		),
		XRT_BYTES_LITERAL("Service Unavailable")
	);
}



/* 输出异步文件或 HTTP 传输错误。 */
static void exampleHttpFileError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pData;
	fprintf(
		stderr,
		"HTTP error [%s/%s]: %s\n",
		xrtErrorDomain(pError),
		xrtErrorOperation(pError),
		xrtErrorMessage(pError)
	);
}



/* 启动文件响应服务，按回车后优雅排空。 */
int main(int iArgc, char** ppArgv)
{
	xtaskpoolconfig PoolConfig = { 2, 128, 0 };
	example_http_file State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xnetaddr Address;
	xdeadline Deadline;
	str sEndpoint = NULL;
	int iResult = 1;

	if ( iArgc != 2 ) {
		fprintf(stderr, "usage: server_file <path>\n");
		return 0;
	}
	memset(&State, 0, sizeof(State));
	State.Path = ppArgv[1];
	State.Pool = xrtTaskPoolCreate(&PoolConfig);
	if ( State.Pool == NULL ) {
		goto Cleanup;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		goto Cleanup;
	}
	xrtHttpServerEventsInit(&Events);
	Events.Request = exampleHttpFileRequest;
	Events.Error = exampleHttpFileError;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address) ) {
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		goto Cleanup;
	}
	printf(
		"serving %s on http://%s/file\n"
		"press Enter to drain\n",
		State.Path,
		sEndpoint
	);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	(void)getchar();
	if ( !xrtHttpServerDrain(pServer) ) {
		goto Cleanup;
	}
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtHttpServerState(pServer) !=
		XHTTP_SERVER_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			(void)xrtHttpServerAbort(pServer);
			break;
		}
		xrtThreadYield();
	}
	iResult = 0;

Cleanup:
	xrtFree(sEndpoint);
	if ( (pServer != NULL) &&
		(xrtHttpServerState(pServer) !=
		 XHTTP_SERVER_CLOSED) ) {
		(void)xrtHttpServerAbort(pServer);
		Deadline = xrtDeadlineAfter(
			UINT64_C(5000000)
		);
		while ( xrtHttpServerState(pServer) !=
			XHTTP_SERVER_CLOSED ) {
			if ( xrtDeadlineExpired(Deadline) ) {
				iResult = 2;
				break;
			}
			xrtThreadYield();
		}
	}
	xrtHttpServerDestroy(pServer);
	if ( (State.Pool != NULL) &&
		!xrtTaskPoolDestroy(State.Pool) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	return iResult;
}
