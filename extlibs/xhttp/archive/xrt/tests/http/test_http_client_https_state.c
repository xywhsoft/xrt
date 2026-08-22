#include "../test.h"



#if !defined(TEST_HTTP_CLIENT_HTTPS_BACKEND)
	#define TEST_HTTP_CLIENT_HTTPS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME "select"
#endif



/* 握手状态夹具故意不启动服务端 TLS，使客户端稳定停留在 HANDSHAKING。 */
typedef struct test_http_client_https_state {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
} test_http_client_https_state;



/* 在截止时间前等待跨 Worker 终态。 */
static void testHttpClientHttpsStateWait(
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



/* 为握手状态测试返回本机 IPv4。 */
static xnetaddrlist* testHttpClientHttpsStateLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "handshake.test") == 0,
		"HTTPS state test resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS state resolver fixture failed");
	return xrtNetAddrListCreate(&Address, 1);
}



/* 该验证器不会被挂起握手调用，仅用于满足安全配置不变量。 */
static xtlsverifydecision testHttpClientHttpsStateVerify(
	const xtlspeer* pPeer,
	ptr pData
)
{
	(void)pPeer;
	(void)pData;
	return XTLS_VERIFY_REJECT;
}



/* 记录挂起 TLS 握手的原始服务端 TCP 已被取消关闭。 */
static void testHttpClientHttpsStateServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_https_state* pState =
		(test_http_client_https_state*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管 TCP 后不创建 TLS Server，固定客户端的握手等待窗口。 */
static bool testHttpClientHttpsStateAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client_https_state* pState =
		(test_http_client_https_state*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Close = testHttpClientHttpsStateServerClose;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		&Events,
		pState
	), "HTTPS state server event takeover failed");
	pState->Server = pStream;
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录测试 Listener 已经完成关闭。 */
static void testHttpClientHttpsStateListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_client_https_state* pState =
		(test_http_client_https_state*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证握手阶段取消仍发布标准客户端取消终态。 */
static void testHttpClientHttpsStateDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_https_state* pState =
		(test_http_client_https_state*)pData;

	testRequire(
		(pCall == pState->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_CANCELLED) &&
		(pResult->Response == NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls == NULL) &&
		(pResult->Error != NULL) &&
		(xrtErrorKind(pResult->Error) ==
		 XERR_CANCELLED) &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_CANCELLED),
		"HTTPS handshake cancellation result mismatch"
	);
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证公开 Call 状态能够真实观察 TLS 握手并从任意线程取消。 */
int main(void)
{
	test_http_client_https_state State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	xhttprequest* pRequest;
	xnetaddr Address;
	xdeadline Deadline;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);

	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify =
		testHttpClientHttpsStateVerify;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS state verifier creation failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_HTTPS_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS state engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS state listener address failed");
	ListenerEvents.Accept =
		testHttpClientHttpsStateAccept;
	ListenerEvents.Close =
		testHttpClientHttpsStateListenerClose;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTPS state listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpClientHttpsStateLookup;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.TlsVerifier = pVerifier;
	ClientConfig.SystemTrust = false;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTPS state client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"https://handshake.test:%u/wait",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTPS state URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTPS state request creation failed"
	);
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		NULL,
		testHttpClientHttpsStateDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTPS state call submission failed"
	);
	testHttpClientHttpsStateWait(
		&State.Accepted,
		"HTTPS state connection was not accepted"
	);
	Deadline = xrtDeadlineAfter(5000000u);
	while ( xrtHttpCallState(State.Call) !=
		XHTTP_CALL_HANDSHAKING ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTPS Call never exposed HANDSHAKING"
		);
		xrtThreadYield();
	}
	testRequire(
		xrtHttpCallCancel(State.Call),
		"HTTPS handshake cancellation failed"
	);
	testHttpClientHttpsStateWait(
		&State.Completed,
		"HTTPS handshake cancellation did not complete"
	);
	testHttpClientHttpsStateWait(
		&State.ServerClosed,
		"HTTPS state server transport did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTPS state listener close failed"
	);
	testHttpClientHttpsStateWait(
		&State.ListenerClosed,
		"HTTPS state listener did not close"
	);

	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS state engine destroy failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	printf(
		"[PASS] high-level HTTPS handshake state (%s)\n",
		TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME
	);
	return 0;
}
