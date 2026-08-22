#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xhttp.h>



/* 保存异步 HTTP Call 的终态和转移给调用方的响应。 */
typedef struct example_http_proxy {
	xatomic32 Done;
	xhttpresponse* Response;
	bool Success;
} example_http_proxy;



/* 严格解析命令行中的代理端口。 */
static bool exampleHttpProxyPort(
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



/* 从高层错误到网络原因逐层输出，不丢失代理拒绝的具体来源。 */
static void exampleHttpProxyError(
	const xerror* pError
)
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



/* 接管成功响应，失败错误只在回调期间借用。 */
static void exampleHttpProxyDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	example_http_proxy* pExample =
		(example_http_proxy*)pData;

	(void)pCall;
	if ( (pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) ) {
		pExample->Response = pResult->Response;
		pExample->Success = true;
	} else if ( pResult != NULL ) {
		exampleHttpProxyError(pResult->Error);
	}
	xrtAtomic32Store(
		&pExample->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待回调；超时由调用方通过统一 Call 取消入口收敛。 */
static bool exampleHttpProxyWait(
	example_http_proxy* pExample,
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



/*
	通过默认 HTTP CONNECT 或 SOCKS5 代理执行 GET。
	最后一个参数为 --direct 时只对本次调用绕过 Client 默认代理。
*/
int main(
	int argc,
	char** argv
)
{
	example_http_proxy Example;
	xnetengineconfig EngineConfig;
	xnetproxyconfig ProxyConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;
	xnetengine* pEngine = NULL;
	xnetproxy* pProxy = NULL;
	xhttpclient* pClient = NULL;
	xhttprequest* pRequest = NULL;
	xhttpcall* pCall = NULL;
	xdeadline Deadline;
	xnetproxytype ProxyType;
	uint16 iProxyPort;
	int iResult = 1;

	if ( argc == 1 ) {
		printf(
			"usage: client_proxy <http-connect|socks5> "
			"<proxy-host> <proxy-port> <http-url> "
			"[--direct]\n"
		);
		return 0;
	}
	if ( strcmp(argv[1], "http-connect") == 0 ) {
		#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
			ProxyType = XNET_PROXY_HTTP_CONNECT;
		#else
			fprintf(
				stderr,
				"HTTP CONNECT support is not compiled\n"
			);
			return 2;
		#endif
	} else if ( strcmp(argv[1], "socks5") == 0 ) {
		#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
			ProxyType = XNET_PROXY_SOCKS5;
		#else
			fprintf(
				stderr,
				"SOCKS5 support is not compiled\n"
			);
			return 2;
		#endif
	} else {
		fprintf(stderr, "unknown proxy type: %s\n", argv[1]);
		return 2;
	}
	if ( (argc < 5) || (argc > 6) ||
		!exampleHttpProxyPort(argv[3], &iProxyPort) ||
		(strncmp(argv[4], "http://", 7) != 0) ) {
		fprintf(
			stderr,
			"usage: client_proxy <http-connect|socks5> "
			"<proxy-host> <proxy-port> <http-url> "
			"[--direct]\n"
		);
		return 2;
	}
	if ( (argc == 6) &&
		(strcmp(argv[5], "--direct") != 0) ) {
		fprintf(stderr, "unknown option: %s\n", argv[5]);
		return 2;
	}
	memset(&Example, 0, sizeof(Example));
	xrtAtomic32Init(&Example.Done, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		exampleHttpProxyError(xrtGetError());
		goto Cleanup;
	}

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = ProxyType;
	ProxyConfig.Host.Data = argv[2];
	ProxyConfig.Host.Size = strlen(argv[2]);
	ProxyConfig.Port = iProxyPort;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		exampleHttpProxyError(xrtGetError());
		goto Cleanup;
	}

	/* Client 保留默认代理，创建成功后即可释放调用方引用。 */
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Proxy = pProxy;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	xrtNetProxyRelease(pProxy);
	pProxy = NULL;
	if ( pClient == NULL ) {
		exampleHttpProxyError(xrtGetError());
		goto Cleanup;
	}

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ argv[4], strlen(argv[4]) }
	);
	if ( pRequest == NULL ) {
		exampleHttpProxyError(xrtGetError());
		goto Cleanup;
	}
	xrtHttpCallOptionsInit(&CallOptions);
	if ( argc == 6 ) {
		CallOptions.Proxy.Mode = XHTTP_PROXY_DIRECT;
	}
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		&CallOptions,
		exampleHttpProxyDone,
		&Example
	);
	xrtHttpRequestDestroy(pRequest);
	pRequest = NULL;
	if ( pCall == NULL ) {
		exampleHttpProxyError(xrtGetError());
		goto Cleanup;
	}

	Deadline = xrtDeadlineAfter(35000000u);
	if ( !exampleHttpProxyWait(&Example, Deadline) ) {
		(void)xrtHttpCallCancel(pCall);
		(void)exampleHttpProxyWait(
			&Example,
			xrtDeadlineAfter(5000000u)
		);
		goto Cleanup;
	}
	if ( Example.Success ) {
		printf(
			"status=%u body=%zu bytes proxy=%s\n",
			(unsigned)xrtHttpResponseStatus(
				Example.Response
			),
			xrtHttpResponseBody(Example.Response).Size,
			argc == 6 ? "direct" : "default"
		);
		iResult = 0;
	}

Cleanup:
	xrtHttpResponseDestroy(Example.Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	xrtNetProxyRelease(pProxy);
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) ) {
		iResult = 1;
	}
	return iResult;
}


