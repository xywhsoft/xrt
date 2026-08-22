#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 静态站点示例在 Server 生命周期内拥有文件任务池和目录根。 */
typedef struct example_http_static {
	xtaskpool* Pool;
	xroot Root;
} example_http_static;



/* 一次调用完成安全映射、条件请求、Range、MIME 和异步正文。 */
static void exampleHttpStaticRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	example_http_static* pState =
		(example_http_static*)pData;

	(void)pServer;
	(void)pRequest;
	if ( xrtHttpConnStatic(
		pConnection,
		pState->Pool,
		pState->Root,
		NULL
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



/* 输出静态文件准备或 HTTP 传输错误。 */
static void exampleHttpStaticError(
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



/* 启动静态站点，按回车后停止接收并排空连接。 */
int main(int iArgc, char** ppArgv)
{
	xtaskpoolconfig PoolConfig = { 2, 128, 0 };
	example_http_static State;
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
		fprintf(
			stderr,
			"usage: server_static <directory>\n"
		);
		return 0;
	}
	memset(&State, 0, sizeof(State));
	State.Root = xrtRootOpen(ppArgv[1]);
	State.Pool = xrtTaskPoolCreate(&PoolConfig);
	if ( (State.Root == NULL) ||
		(State.Pool == NULL) ) {
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
	Events.Request = exampleHttpStaticRequest;
	Events.Error = exampleHttpStaticError;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(
			pServer, 0, &Address
		) ) {
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		goto Cleanup;
	}
	printf(
		"serving %s on http://%s/\n"
		"press Enter to drain\n",
		ppArgv[1],
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
	if ( (State.Root != NULL) &&
		!xrtRootClose(State.Root) &&
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
