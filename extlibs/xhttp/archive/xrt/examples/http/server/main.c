#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 比较借用视图与零结尾文本。 */
static bool exampleHttpEqual(xstrview Value, cstr sText)
{
	size_t iSize = strlen(sText);

	return (Value.Size == iSize) &&
		(memcmp(Value.Data, sText, iSize) == 0);
}



/* 直接提交固定响应，不要求应用创建 Reply 或 JSON 对象。 */
static void exampleHttpRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	(void)pData;
	if ( exampleHttpEqual(Target, "/health") ) {
		(void)xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			),
			XRT_BYTES_LITERAL(
				"{\"code\":200,\"msg\":\"OK\"}"
			)
		);
		return;
	}
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_NOT_FOUND,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Not Found")
	);
}



/* 输出稳定错误域、操作和消息，便于服务端日志关联。 */
static void exampleHttpError(
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



/* 启动动态端口 HTTP 服务，按回车后优雅排空。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xnetaddr Address;
	xdeadline Deadline;
	str sEndpoint = NULL;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
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
	Events.Request = exampleHttpRequest;
	Events.Error = exampleHttpError;
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
		"listening on http://%s/health\n"
		"press Enter to drain\n",
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
		Deadline = xrtDeadlineAfter(UINT64_C(5000000));
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
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	return iResult;
}
