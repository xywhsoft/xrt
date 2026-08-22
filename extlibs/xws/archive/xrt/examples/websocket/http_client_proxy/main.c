#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xrt.h>



/* 保存异步 WebSocket 建连转移给调用方的对象。 */
typedef struct example_ws_proxy {
	xatomic32 Done;
	xnetresult Result;
	xwsconn* Connection;
	xhttpresponse* Response;
} example_ws_proxy;



/* 严格解析命令行中的代理端口。 */
static bool exampleWsProxyPort(
	cstr sText,
	uint16* pPort
)
{
	char* pEnd;
	unsigned long iValue;

	if ( (sText == NULL) || (pPort == NULL) ) {
		return false;
	}
	iValue = strtoul(sText, &pEnd, 10);
	if ( (*sText == 0) || (*pEnd != 0) ||
		(iValue == 0) || (iValue > UINT16_MAX) ) {
		return false;
	}
	*pPort = (uint16)iValue;
	return true;
}



/* 输出完整错误原因链，不丢失 DNS、代理、HTTP 或握手阶段。 */
static void exampleWsProxyError(const xerror* pError)
{
	while ( pError != NULL ) {
		fprintf(
			stderr,
			"%s/%d %s: %s\n",
			xrtErrorDomain(pError),
			(int)xrtErrorCode(pError),
			xrtErrorOperation(pError),
			xrtErrorMessage(pError)
		);
		pError = xrtErrorCause(pError);
	}
}



/* 接管成功 Connection 和 101 Response；失败错误只在回调期间借用。 */
static void exampleWsProxyDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	example_ws_proxy* pExample =
		(example_ws_proxy*)pData;

	(void)pCall;
	pExample->Result = Result;
	pExample->Connection = pConnection;
	pExample->Response = pResponse;
	if ( Result != XNET_RESULT_OK ) {
		exampleWsProxyError(pError);
	}
	xrtAtomic32Store(
		&pExample->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待建连完成或关闭终态，不在网络 Worker 内阻塞。 */
static bool exampleWsProxyWait(
	const example_ws_proxy* pExample,
	xdeadline Deadline
)
{
	while ( xrtAtomic32Load(
		&pExample->Done,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 通过 HTTP CONNECT 代理建立 WebSocket，并演示正常 Close 收敛。 */
int main(
	int argc,
	char** argv
)
{
	example_ws_proxy Example;
	xnetengineconfig EngineConfig;
	xnetproxyconfig ProxyConfig;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xnetengine* pEngine = NULL;
	xnetproxy* pProxy = NULL;
	xhttpclient* pClient = NULL;
	xhttpcall* pCall = NULL;
	xdeadline Deadline;
	xstrview Protocol;
	uint16 iProxyPort;
	int iResult = 1;

	if ( (argc != 4) ||
		!exampleWsProxyPort(argv[2], &iProxyPort) ) {
		fprintf(
			stderr,
			"usage: http_client_proxy "
			"<proxy-host> <proxy-port> <ws-url>\n"
		);
		return argc == 1 ? 0 : 2;
	}
	memset(&Example, 0, sizeof(Example));
	xrtAtomic32Init(&Example.Done, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		exampleWsProxyError(xrtGetError());
		goto Cleanup;
	}
	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = (xstrview) {
		argv[1],
		strlen(argv[1])
	};
	ProxyConfig.Port = iProxyPort;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		exampleWsProxyError(xrtGetError());
		goto Cleanup;
	}

	/* WebSocket 复用 HTTP Client 的默认代理、DNS、TLS 和连接策略。 */
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Proxy = pProxy;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	xrtNetProxyRelease(pProxy);
	pProxy = NULL;
	if ( pClient == NULL ) {
		exampleWsProxyError(xrtGetError());
		goto Cleanup;
	}
	xrtWsClientConfigInit(&WsConfig);
	pCall = xrtWsConnect(
		pClient,
		(xstrview) { argv[3], strlen(argv[3]) },
		&WsConfig,
		NULL,
		NULL,
		exampleWsProxyDone,
		&Example
	);
	if ( pCall == NULL ) {
		exampleWsProxyError(xrtGetError());
		goto Cleanup;
	}
	if ( !exampleWsProxyWait(
		&Example,
		xrtDeadlineAfter(UINT64_C(35000000))
	) ) {
		(void)xrtHttpCallCancel(pCall);
		(void)exampleWsProxyWait(
			&Example,
			xrtDeadlineAfter(UINT64_C(5000000))
		);
		goto Cleanup;
	}
	if ( (Example.Result != XNET_RESULT_OK) ||
		(Example.Connection == NULL) ||
		(Example.Response == NULL) ) {
		goto Cleanup;
	}
	Protocol = xrtWsConnProtocol(Example.Connection);
	printf(
		"status=%u protocol=%.*s proxy=%s:%u\n",
		(unsigned)xrtHttpResponseStatus(Example.Response),
		(int)Protocol.Size,
		Protocol.Data != NULL ? Protocol.Data : "",
		argv[1],
		(unsigned)iProxyPort
	);
	if ( xrtWsConnClose(
		Example.Connection,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("done")
	) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtWsConnState(Example.Connection) !=
		XWS_CONN_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			(void)xrtWsConnAbort(Example.Connection);
			goto Cleanup;
		}
		xrtThreadYield();
	}
	iResult = 0;

Cleanup:
	if ( (Example.Connection != NULL) &&
		(xrtWsConnState(Example.Connection) !=
		 XWS_CONN_CLOSED) ) {
		(void)xrtWsConnAbort(Example.Connection);
	}
	xrtWsConnDestroy(Example.Connection);
	xrtHttpResponseDestroy(Example.Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpClientDestroy(pClient);
	xrtNetProxyRelease(pProxy);
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iResult;
}
