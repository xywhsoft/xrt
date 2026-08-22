#include "../test.h"



/* 非法代理选项必须在创建异步 Call 前同步失败，不能进入完成回调。 */
static void testHttpProxyUnexpectedDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
	testRequire(
		false,
		"invalid HTTP proxy options reached completion callback"
	);
}



/* 验证 mode 与显式代理指针的组合约束。 */
static void testHttpProxyReject(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	xhttpproxymode Mode,
	const xnetproxy* pProxy
)
{
	xhttpcalloptions Options;
	xhttpcall* pCall;
	const xerror* pError;

	xrtHttpCallOptionsInit(&Options);
	Options.Proxy.Mode = Mode;
	Options.Proxy.Proxy = pProxy;
	xrtClearError();
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		&Options,
		testHttpProxyUnexpectedDone,
		NULL
	);
	pError = xrtGetError();
	testRequire(
		(pCall == NULL) &&
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(xrtErrorCode(pError) ==
		 XHTTP_CLIENT_ERROR_CONFIG),
		"invalid HTTP proxy option contract mismatch"
	);
	xrtClearError();
}



/* 验证 Client 深持有默认代理，Call 默认策略保持显式可判定。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;
	xnetproxyconfig ProxyConfig;
	xnetproxyinfo ProxyInfo;
	xnetengine* pEngine;
	xnetproxy* pProxy;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	uint8 ProxyOptionsStorage[
		sizeof(xhttpproxyoptions) + 2u
	];
	xhttpproxyoptions ProxyOptions;

	/* 初始化器必须支持完整的未对齐存储并保护两侧字节。 */
	memset(
		ProxyOptionsStorage,
		0xA5,
		sizeof(ProxyOptionsStorage)
	);
	xrtHttpProxyOptionsInit(
		(xhttpproxyoptions*)(ProxyOptionsStorage + 1u)
	);
	memcpy(
		&ProxyOptions,
		ProxyOptionsStorage + 1u,
		sizeof(ProxyOptions)
	);
	testRequire(
		(ProxyOptionsStorage[0] == 0xA5) &&
		(ProxyOptionsStorage[
			sizeof(ProxyOptionsStorage) - 1u
		] == 0xA5) &&
		(ProxyOptions.Mode == XHTTP_PROXY_DEFAULT) &&
		(ProxyOptions.Proxy == NULL),
		"HTTP proxy unaligned initializer mismatch"
	);

	/* 回绕地址必须同步拒绝，且不能尝试写入调用方存储。 */
	xrtClearError();
	xrtHttpProxyOptionsInit(
		(xhttpproxyoptions*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP proxy wrapping initializer contract mismatch"
	);
	xrtClearError();

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = 8080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(pProxy != NULL,
		"HTTP client proxy fixture creation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP client proxy engine start failed");

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Proxy = pProxy;
	pClient = xrtHttpClientCreate(
		pEngine,
		&ClientConfig
	);
	testRequire(pClient != NULL,
		"HTTP client proxy client creation failed");
	xrtNetProxyRelease(pProxy);
	pProxy = NULL;
	testRequire(
		xrtNetProxyInfo(
			xrtHttpClientProxy(pClient),
			&ProxyInfo
		) &&
		(ProxyInfo.Type == XNET_PROXY_HTTP_CONNECT) &&
		(ProxyInfo.Port == 8080) &&
		(ProxyInfo.Host.Size == 10u) &&
		(memcmp(
			ProxyInfo.Host.Data,
			"proxy.test",
			10
		) == 0),
		"HTTP client did not retain its default proxy"
	);

	xrtHttpCallOptionsInit(&CallOptions);
	testRequire(
		(CallOptions.Proxy.Mode == XHTTP_PROXY_DEFAULT) &&
		(CallOptions.Proxy.Proxy == NULL),
		"HTTP call proxy defaults mismatch"
	);
	xrtHttpProxyOptionsInit(&CallOptions.Proxy);
	testRequire(
		(CallOptions.Proxy.Mode == XHTTP_PROXY_DEFAULT) &&
		(CallOptions.Proxy.Proxy == NULL),
		"HTTP proxy option initializer mismatch"
	);
	xrtClearError();
	xrtHttpProxyOptionsInit(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP proxy null initializer contract mismatch"
	);
	xrtClearError();

	/* 所有非法组合都必须在网络调度前拒绝。 */
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://origin.test/config")
	);
	testRequire(
		pRequest != NULL,
		"HTTP proxy option request creation failed"
	);
	testHttpProxyReject(
		pClient,
		pRequest,
		XHTTP_PROXY_DEFAULT,
		xrtHttpClientProxy(pClient)
	);
	testHttpProxyReject(
		pClient,
		pRequest,
		XHTTP_PROXY_DIRECT,
		xrtHttpClientProxy(pClient)
	);
	testHttpProxyReject(
		pClient,
		pRequest,
		XHTTP_PROXY_EXPLICIT,
		NULL
	);
	testHttpProxyReject(
		pClient,
		pRequest,
		(xhttpproxymode)UINT32_MAX,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);

	xrtHttpClientDestroy(pClient);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP client proxy engine destroy failed"
	);
	printf("[PASS] HTTP client proxy configuration ownership\n");
	return 0;
}
