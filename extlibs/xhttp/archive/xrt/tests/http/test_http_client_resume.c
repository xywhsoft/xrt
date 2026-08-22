#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_CLIENT_RESUME_BACKEND)
	#define TEST_HTTP_CLIENT_RESUME_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_RESUME_BACKEND_NAME "select"
#endif



#if !defined(TEST_HTTP_CLIENT_RESUME_LATE_TICKET)
	#define TEST_HTTP_CLIENT_RESUME_LATE_TICKET 0
#endif



#define TEST_HTTP_CLIENT_RESUME_ROUNDS 2u
#define TEST_HTTP_CLIENT_RESUME_HOST "example.com"



typedef struct test_http_client_resume {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xtlsresume* ServerResume;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xatomic32 Round;
	xatomic32 Accepted;
	xatomic32 ServerOpened;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	xatomic32 VerifyCalls;
	bool Responded;
} test_http_client_resume;



/* 在统一截止时间前等待累计事件达到指定轮次。 */
static void testHttpClientResumeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待所有 TLS Stream 释放其 Engine 持有，避免把异步关闭误判为泄漏。 */
static void testHttpClientResumeEngineDestroy(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( !xrtNetEngineDestroy(pEngine) ) {
		const xerror* pError = xrtGetError();

		testRequire(
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_STATE) &&
			(xrtErrorCode(pError) == XNET_ERROR_ENGINE_STOP),
			"HTTP resume engine destroy reported an unexpected error"
		);
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP resume retained an Engine object"
		);
		xrtThreadYield();
	}
}



/* 把测试域名解析到本地 Listener，同时保留真实的 DNS 验证身份。 */
static xnetaddrlist* testHttpClientResumeLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, TEST_HTTP_CLIENT_RESUME_HOST) == 0,
		"HTTP resume resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"HTTP resume resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 完整握手必须验证一次证书，恢复握手不应再次进入证书验证。 */
static xtlsverifydecision testHttpClientResumeVerify(
	const xtlspeer* pPeer,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	testRequire(
		(pPeer != NULL) &&
		(pPeer->Name.Size ==
		 (sizeof(TEST_HTTP_CLIENT_RESUME_HOST) - 1u)) &&
		(memcmp(
			pPeer->Name.Data,
			TEST_HTTP_CLIENT_RESUME_HOST,
			pPeer->Name.Size
		) == 0),
		"HTTP resume verified the wrong target identity"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->VerifyCalls,
		1,
		XMEMORY_RELEASE
	);
	return testTlsServerAccept(pPeer, NULL);
}



/* 服务端按 ticket 标识返回上一轮保存的恢复对象。 */
static const xtlsresume* testHttpClientResumeLookupTicket(
	ptr pData,
	const xtlsserverresumerequest* pRequest
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	testRequire(
		(pRequest != NULL) &&
		(pRequest->Ticket.Size != 0) &&
		(pState->ServerResume != NULL),
		"HTTP resume lookup request is invalid"
	);
	return pState->ServerResume;
}



/* 保存服务端新签发的 ticket，供下一轮恢复查询。 */
static void testHttpClientResumeIssueTicket(
	test_http_client_resume* pState,
	xtlssession* pSession
)
{
	xtlsresume* pResume = NULL;

	testRequire(
		(xrtTlsServerTicketNew(pSession, &pResume) == XTLS_OK) &&
		(pResume != NULL),
		"HTTP resume server ticket issue failed"
	);
	xrtTlsResumeRelease(pState->ServerResume);
	pState->ServerResume = pResume;
}



#if TEST_HTTP_CLIENT_RESUME_LATE_TICKET

/* HTTP 已进入终态后，在服务端 Worker 签发 ticket 并关闭空闲连接。 */
static void testHttpClientResumeLateTicket(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	testRequire(
		(pState->Server != NULL) &&
		(pWorker == xrtNetStreamWorker(
			xrtTlsStreamTransport(pState->Server)
		)),
		"HTTP late-ticket task worker mismatch"
	);
	testHttpClientResumeIssueTicket(
		pState,
		xrtTlsStreamSession(pState->Server)
	);
	testRequire(
		xrtTlsStreamClose(pState->Server),
		"HTTP resume late-ticket close failed"
	);
}

#endif



/* 验证握手类型，并在每轮握手完成后签发下一张单次 ticket。 */
static void testHttpClientResumeOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	xbytesview Name;
	uint32 iRound = xrtAtomic32Load(
		&pState->Round,
		XMEMORY_ACQUIRE
	);

	testRequire(
		(pSession != NULL) &&
		xrtTlsServerName(pSession, &Name) &&
		(Name.Size == (sizeof(TEST_HTTP_CLIENT_RESUME_HOST) - 1u)) &&
		(memcmp(
			Name.Data,
			TEST_HTTP_CLIENT_RESUME_HOST,
			Name.Size
		) == 0),
		"HTTP resume server SNI mismatch"
	);
	testRequire(
		xrtTlsServerResumed(pSession) == (iRound != 0),
		"HTTP resume server handshake type mismatch"
	);
	#if !TEST_HTTP_CLIENT_RESUME_LATE_TICKET
		testHttpClientResumeIssueTicket(pState, pSession);
	#endif
	(void)xrtAtomic32FetchAdd(
		&pState->ServerOpened,
		1,
		XMEMORY_RELEASE
	);
}



/* 收到完整请求头后发送固定响应，确保 ticket 记录先于 HTTP 数据上线路。 */
static void testHttpClientResumeRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;
	char Request[1024];
	size_t iSize = xrtTlsStreamAvailable(pStream);
	size_t iWritten = 0;
	bool bComplete = false;

	testRequire(
		pBuffer == xrtTlsStreamBuffer(pStream),
		"HTTP resume server plaintext buffer mismatch"
	);
	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTP resume request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(pBuffer, 0, Request, iSize) == iSize,
		"HTTP resume request peek failed"
	);
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (Request[i - 3] == '\r') &&
			(Request[i - 2] == '\n') &&
			(Request[i - 1] == '\r') &&
			(Request[i] == '\n') ) {
			bComplete = true;
			break;
		}
	}
	if ( !bComplete ) {
		return;
	}
	testRequire(
		!pState->Responded &&
		(memcmp(Request, "GET /resume HTTP/1.1\r\n", 22u) == 0),
		"HTTP resume request target mismatch"
	);
	pState->Responded = true;
	testRequire(
		xrtTlsStreamConsume(pStream, iSize),
		"HTTP resume request consume failed"
	);
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == (sizeof(Response) - 1u)),
		"HTTP resume response send failed"
	);
}



/* 对端关闭时回送 close_notify，压住高层自动关闭路径。 */
static void testHttpClientResumeEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	#if TEST_HTTP_CLIENT_RESUME_LATE_TICKET
		(void)pStream;
	#else
		testRequire(
			xrtTlsStreamClose(pStream),
			"HTTP resume server close_notify failed"
		);
	#endif
}



/* 记录每轮服务端 TLS Stream 的认证关闭终态。 */
static void testHttpClientResumeClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED),
		"HTTP resume server close result mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 为每次 TCP Accept 建立使用恢复回调的 TLS 服务端。 */
static bool testHttpClientResumeAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;
	xtlsstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpClientResumeOpen;
	Events.Read = testHttpClientResumeRead;
	Events.End = testHttpClientResumeEnd;
	Events.Close = testHttpClientResumeClose;
	testRequire(
		xrtTlsStreamAccept(
			pTransport,
			&pState->ServerConfig,
			&pState->StreamConfig,
			&Events,
			pState,
			&pState->Server
		),
		"HTTP resume server TLS accept failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 的唯一关闭完成事件。 */
static void testHttpClientResumeListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管每轮 HTTP 结果，保持响应到主线程完成统计核对。 */
static void testHttpClientResumeDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_resume* pState =
		(test_http_client_resume*)pData;

	testRequire(
		(pCall == pState->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls == NULL) &&
		(pResult->Error == NULL) &&
		(xrtHttpResponseStatus(pResult->Response) == 200) &&
		(xrtHttpResponseBody(pResult->Response).Size == 2u) &&
		(memcmp(
			xrtHttpResponseBody(pResult->Response).Data,
			"OK",
			2u
		) == 0),
		"HTTP resume high-level result mismatch"
	);
	pState->Response = pResult->Response;
	#if TEST_HTTP_CLIENT_RESUME_LATE_TICKET
		testRequire(
			xrtNetEnginePost(
				pState->Engine,
				xrtNetWorkerIndex(xrtNetStreamWorker(
					xrtTlsStreamTransport(pState->Server)
				)),
				testHttpClientResumeLateTicket,
				pState
			),
			"HTTP late-ticket task post failed"
		);
	#endif
	(void)xrtAtomic32FetchAdd(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 连续两次 HTTPS 调用验证 ticket 产生、命中、更新、统计和清空。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_client_resume State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xhttpresumestats Stats;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetaddr Address;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTP resume TLS fixture creation failed"
	);

	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testHttpClientResumeVerify;
	VerifierConfig.Context = &State;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTP resume verifier creation failed"
	);

	xrtTlsServerConfigInit(&State.ServerConfig);
	State.ServerConfig.Context = pContext;
	State.ServerConfig.Identity = pIdentity;
	State.ServerConfig.Protocols = Protocols;
	State.ServerConfig.ProtocolCount = 1u;
	State.ServerConfig.RequireProtocol = true;
	State.ServerConfig.Resume = testHttpClientResumeLookupTicket;
	State.ServerConfig.ResumeContext = &State;
	xrtTlsStreamConfigInit(&State.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_RESUME_BACKEND;
	EngineConfig.Workers = 2u;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP resume engine start failed"
	);

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP resume listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4u;
	ListenerEvents.Accept = testHttpClientResumeAccept;
	ListenerEvents.Close = testHttpClientResumeListenerClose;
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
		"HTTP resume listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpClientResumeLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1u;
	ClientConfig.TlsContext = pContext;
	ClientConfig.TlsVerifier = pVerifier;
	ClientConfig.SystemTrust = false;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL) && \
		!TEST_HTTP_CLIENT_RESUME_LATE_TICKET
		ClientConfig.Pool.MaxIdle = 0;
	#endif
	State.Client = xrtHttpClientCreate(State.Engine, &ClientConfig);
	testRequire(
		State.Client != NULL,
		"HTTP resume client creation failed"
	);

	iLength = snprintf(
		Url,
		sizeof(Url),
		"https://" TEST_HTTP_CLIENT_RESUME_HOST ":%u/resume",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) && ((size_t)iLength < sizeof(Url)),
		"HTTP resume URL overflowed"
	);

	for ( uint32 iRound = 0;
		iRound < TEST_HTTP_CLIENT_RESUME_ROUNDS;
		iRound++ ) {
		xhttprequest* pRequest;
		bool bStats;

		xrtAtomic32Store(&State.Round, iRound, XMEMORY_RELEASE);
		State.Responded = false;
		State.Response = NULL;
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			(xstrview) { Url, (size_t)iLength }
		);
		testRequire(
			pRequest != NULL,
			"HTTP resume request creation failed"
		);
		State.Call = xrtHttpClientDo(
			State.Client,
			pRequest,
			NULL,
			testHttpClientResumeDone,
			&State
		);
		xrtHttpRequestDestroy(pRequest);
		testRequire(
			State.Call != NULL,
			"HTTP resume call submission failed"
		);

		testHttpClientResumeWait(
			&State.Accepted,
			iRound + 1u,
			"HTTP resume connection was not accepted"
		);
		testHttpClientResumeWait(
			&State.ServerOpened,
			iRound + 1u,
			"HTTP resume TLS server did not open"
		);
		testHttpClientResumeWait(
			&State.Completed,
			iRound + 1u,
			"HTTP resume call did not complete"
		);
		testHttpClientResumeWait(
			&State.ServerClosed,
			iRound + 1u,
			"HTTP resume transport did not authenticate close"
		);

		bStats = xrtHttpClientResumeStats(State.Client, &Stats);
		if ( !bStats ||
			(Stats.Entries != 1u) ||
			(Stats.Hits != (uint64)iRound) ||
			(Stats.Misses != 1u) ||
			(Stats.Stored != (uint64)iRound + 1u) ||
			(Stats.Evicted != 0) ||
			(Stats.Expired != 0) ||
			(Stats.Dropped != 0) ) {
			fprintf(
				stderr,
				"round=%u entries=%zu hits=%llu misses=%llu "
				"stored=%llu evicted=%llu expired=%llu dropped=%llu\n",
				(unsigned)iRound,
				Stats.Entries,
				(unsigned long long)Stats.Hits,
				(unsigned long long)Stats.Misses,
				(unsigned long long)Stats.Stored,
				(unsigned long long)Stats.Evicted,
				(unsigned long long)Stats.Expired,
				(unsigned long long)Stats.Dropped
			);
		}
		testRequire(
			bStats &&
			(Stats.Entries == 1u) &&
			(Stats.Hits == (uint64)iRound) &&
			(Stats.Misses == 1u) &&
			(Stats.Stored == (uint64)iRound + 1u) &&
			(Stats.Evicted == 0) &&
			(Stats.Expired == 0) &&
			(Stats.Dropped == 0),
			"HTTP resume cache statistics mismatch"
		);
		xrtHttpResponseDestroy(State.Response);
		xrtHttpCallDestroy(State.Call);
		xrtTlsStreamDestroy(State.Server);
		State.Response = NULL;
		State.Call = NULL;
		State.Server = NULL;
	}

	testRequire(
		xrtAtomic32Load(
			&State.VerifyCalls,
			XMEMORY_ACQUIRE
		) == 1u,
		"HTTP resumed handshake repeated certificate verification"
	);
	testRequire(
		(xrtHttpClientResumeClear(State.Client) == 1u) &&
		xrtHttpClientResumeStats(State.Client, &Stats) &&
		(Stats.Entries == 0),
		"HTTP resume cache clear mismatch"
	);

	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP resume listener close failed"
	);
	testHttpClientResumeWait(
		&State.ListenerClosed,
		1u,
		"HTTP resume listener did not close"
	);
	xrtHttpClientDestroy(State.Client);
	xrtNetListenerDestroy(State.Listener);
	testHttpClientResumeEngineDestroy(State.Engine);
	xrtTlsResumeRelease(State.ServerResume);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		#if TEST_HTTP_CLIENT_RESUME_LATE_TICKET
			"[PASS] high-level HTTPS late-ticket resumption (%s)\n",
		#else
			"[PASS] high-level HTTPS TLS 1.3 resumption (%s)\n",
		#endif
		TEST_HTTP_CLIENT_RESUME_BACKEND_NAME
	);
	return 0;
}
