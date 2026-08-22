#include "../fixtures/http_connect_proxy.h"



#if !defined(TEST_HTTP_CLIENT_PROXY_FAILURE_BACKEND)
	#define TEST_HTTP_CLIENT_PROXY_FAILURE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_PROXY_FAILURE_BACKEND_NAME "select"
#endif



/* 每个用例独立覆盖拒绝、总超时或显式取消中的一种终态。 */
typedef enum test_http_proxy_failure_mode {
	TEST_HTTP_PROXY_REJECT = 0,
	TEST_HTTP_PROXY_TIMEOUT,
	TEST_HTTP_PROXY_CANCEL
} test_http_proxy_failure_mode;



/* 保存一条失败路径的服务端资源和异步终态。 */
typedef struct test_http_proxy_failure {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xatomic32 Accepted;
	xatomic32 Handshake;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_http_proxy_failure_mode Mode;
	uint16 Port;
} test_http_proxy_failure;



/* 在统一截止时间内等待 Worker 发布测试状态。 */
static void testHttpProxyFailureWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 只允许解析代理端点，目标域名必须保留给 CONNECT。 */
static xnetaddrlist* testHttpProxyFailureLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "proxy.test") == 0,
		"HTTP proxy failure unexpectedly resolved the target"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP proxy failure resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 在 cause 链中精确查找指定错误域和代码。 */
static bool testHttpProxyFailureCause(
	const xerror* pError,
	cstr sDomain,
	int32 iCode
)
{
	while ( pError != NULL ) {
		if ( (strcmp(
			xrtErrorDomain(pError),
			sDomain
		) == 0) && (xrtErrorCode(pError) == iCode) ) {
			return true;
		}
		pError = xrtErrorCause(pError);
	}
	return false;
}



/* 完整 CONNECT 到达后按用例拒绝或保持停滞。 */
static void testHttpProxyFailureRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Rejected[] =
		"HTTP/1.1 407 Proxy Authentication Required\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	test_http_proxy_failure* pState =
		(test_http_proxy_failure*)pData;

	if ( xrtAtomic32Load(
		&pState->Handshake,
		XMEMORY_ACQUIRE
	) != 0 ) {
		return;
	}
	if ( !testHttpConnectProxyRequest(
		pBuffer,
		"origin.test",
		8080,
		false
	) ) {
		return;
	}
	xrtAtomic32Store(
		&pState->Handshake,
		1,
		XMEMORY_RELEASE
	);
	if ( pState->Mode == TEST_HTTP_PROXY_REJECT ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				Rejected,
				sizeof(Rejected) - 1u
			) == XNET_RESULT_OK,
			"HTTP proxy rejection response failed"
		);
	}
}



/* 记录失败用例的服务端连接已释放。 */
static void testHttpProxyFailureClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_proxy_failure* pState =
		(test_http_proxy_failure*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管唯一连接并为 Stream 事件安装独立数据。 */
static bool testHttpProxyFailureAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_proxy_failure* pState =
		(test_http_proxy_failure*)pData;

	(void)pListener;
	testRequire(
		xrtAtomic32Load(
			&pState->Accepted,
			XMEMORY_ACQUIRE
		) == 0,
		"HTTP proxy failure accepted duplicate connections"
	);
	pState->Server = pStream;
	testRequire(
		xrtNetStreamSetData(pStream, pState),
		"HTTP proxy failure stream data setup failed"
	);
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录失败用例 Listener 已经排空。 */
static void testHttpProxyFailureListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_proxy_failure* pState =
		(test_http_proxy_failure*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证高层稳定错误与底层代理原因链均未丢失。 */
static void testHttpProxyFailureDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_proxy_failure* pState =
		(test_http_proxy_failure*)pData;
	const xerror* pError;

	testRequire(
		(pCall != NULL) &&
		(pResult != NULL) &&
		(pResult->Response == NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Error != NULL),
		"HTTP proxy failure result shape mismatch"
	);
	pError = pResult->Error;
	testRequire(
		strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0,
		"HTTP proxy failure domain mismatch"
	);
	if ( pState->Mode == TEST_HTTP_PROXY_REJECT ) {
		testRequire(
			(pResult->Result == XNET_RESULT_ERROR) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_FAILED) &&
			(xrtErrorKind(pError) == XERR_PERMISSION) &&
			(xrtErrorCode(pError) ==
			 XHTTP_CLIENT_ERROR_PROXY) &&
			testHttpProxyFailureCause(
				pError,
				"xrt.net",
				XNET_ERROR_PROXY_AUTH
			),
			"HTTP proxy rejection error contract mismatch"
		);
	} else if ( pState->Mode ==
		TEST_HTTP_PROXY_TIMEOUT ) {
		testRequire(
			(pResult->Result == XNET_RESULT_TIMEOUT) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_FAILED) &&
			(xrtErrorKind(pError) == XERR_TIMEOUT) &&
			(xrtErrorCode(pError) ==
			 XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL),
			"HTTP proxy total timeout contract mismatch"
		);
	} else {
		testRequire(
			(pResult->Result ==
			 XNET_RESULT_CANCELLED) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_CANCELLED) &&
			(xrtErrorKind(pError) == XERR_CANCELLED) &&
			(xrtErrorCode(pError) ==
			 XHTTP_CLIENT_ERROR_CANCELLED),
			"HTTP proxy cancellation contract mismatch"
		);
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 独立执行一个失败模式并排空所有网络对象。 */
static void testHttpProxyFailureRun(
	test_http_proxy_failure_mode Mode
)
{
	test_http_proxy_failure State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;
	xnetproxyconfig ProxyConfig;
	xnetproxy* pProxy;
	xhttprequest* pRequest;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	State.Mode = Mode;
	ListenerEvents.Accept = testHttpProxyFailureAccept;
	ListenerEvents.Close = testHttpProxyFailureListenerClose;
	StreamEvents.Read = testHttpProxyFailureRead;
	StreamEvents.Close = testHttpProxyFailureClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_HTTP_CLIENT_PROXY_FAILURE_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP proxy failure engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP proxy failure listener address failed"
	);
	ListenConfig.Stream.ReadSize = 64;
	ListenConfig.Stream.ReadLimit = 4096;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTP proxy failure listener creation failed"
	);
	State.Port = Address.Port;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = State.Port;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(
		pProxy != NULL,
		"HTTP proxy failure endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpProxyFailureLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Proxy = pProxy;
	ClientConfig.Timeout = 2000000u;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	xrtNetProxyRelease(pProxy);
	testRequire(
		State.Client != NULL,
		"HTTP proxy failure client creation failed"
	);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"http://origin.test:8080/failure"
		)
	);
	testRequire(
		pRequest != NULL,
		"HTTP proxy failure request creation failed"
	);
	xrtHttpCallOptionsInit(&CallOptions);
	if ( Mode == TEST_HTTP_PROXY_TIMEOUT ) {
		CallOptions.Timeout = 50000u;
	}
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		&CallOptions,
		testHttpProxyFailureDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP proxy failure call submission failed"
	);
	testHttpProxyFailureWait(
		&State.Handshake,
		"HTTP proxy failure CONNECT did not arrive"
	);
	if ( Mode == TEST_HTTP_PROXY_CANCEL ) {
		testRequire(
			xrtHttpCallCancel(State.Call) &&
			!xrtHttpCallCancel(State.Call),
			"HTTP proxy cancellation was not unique"
		);
	}
	testHttpProxyFailureWait(
		&State.Completed,
		"HTTP proxy failure call did not complete"
	);

	if ( xrtNetStreamState(State.Server) !=
		XNET_STREAM_CLOSED ) {
		(void)xrtNetStreamAbort(State.Server);
		testHttpProxyFailureWait(
			&State.ServerClosed,
			"HTTP proxy failure server did not close"
		);
	}
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP proxy failure listener close failed"
	);
	testHttpProxyFailureWait(
		&State.ListenerClosed,
		"HTTP proxy failure listener did not close"
	);
	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP proxy failure engine did not drain"
	);
}



/* 三种终态必须在同一后端上保持一致错误语义。 */
int main(void)
{
	testHttpProxyFailureRun(TEST_HTTP_PROXY_REJECT);
	testHttpProxyFailureRun(TEST_HTTP_PROXY_TIMEOUT);
	testHttpProxyFailureRun(TEST_HTTP_PROXY_CANCEL);
	printf(
		"[PASS] HTTP client proxy failure contracts (%s)\n",
		TEST_HTTP_CLIENT_PROXY_FAILURE_BACKEND_NAME
	);
	return 0;
}
