#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 对常见健康检查直接提交预构建报文，其他路径使用普通 Reply。 */
static void onRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	static const uint8 Health[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"Content-Length: 23\r\n"
		"Connection: close\r\n"
		"\r\n"
		"{\"code\":200,\"msg\":\"OK\"}";
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	(void)pData;
	if ( (Target.Size == 7u) &&
		(memcmp(Target.Data, "/health", 7u) == 0) ) {
		(void)xrtHttpConnRespondRaw(
			pConnection,
			(xbytesview){ Health, sizeof(Health) - 1u },
			XHTTP_SERVER_RAW_NONE
		);
		return;
	}
	(void)xrtHttpConnReply(
		pConnection,
		404,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Not Found")
	);
}



/* 启动一个最小原始响应 Server，并在标准输入结束后排空。 */
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
	Events.Request = onRequest;
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
		"listening on http://%s/health\n",
		sEndpoint
	);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	printf("press Enter to drain\n");
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
