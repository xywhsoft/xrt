#include "../test.h"

#include "../../src/internal/xrt_http_client_runtime.h"



#if !defined(TEST_HTTP_COOKIE_BACKEND)
	#define TEST_HTTP_COOKIE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_COOKIE_BACKEND_NAME "select"
#endif



/* 验证调用级 Cookie 选项初始化器的完整存储契约。 */
static void testHttpCookieOptionsStorage(void)
{
	uint8 Storage[sizeof(xhttpcookieoptions) + 2u];
	xhttpcookieoptions Options;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttpCookieOptionsInit(
		(xhttpcookieoptions*)(void*)(Storage + 1u)
	);
	memcpy(&Options, Storage + 1u, sizeof(Options));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Options.Flags == XHTTP_COOKIE_SAME_SITE) &&
		(Options.PartitionKey.Data == NULL) &&
		(Options.PartitionKey.Size == 0),
		"HTTP Cookie unaligned options initializer mismatch"
	);

	xrtClearError();
	xrtHttpCookieOptionsInit(
		(xhttpcookieoptions*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Cookie wrapping options initializer mismatch"
	);
	xrtClearError();
}



typedef enum test_http_cookie_scenario {
	TEST_HTTP_COOKIE_AUTOMATIC = 0,
	TEST_HTTP_COOKIE_EXPLICIT,
	TEST_HTTP_COOKIE_DISABLED
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		, TEST_HTTP_COOKIE_REDIRECT
	#endif
} test_http_cookie_scenario;



/* Cookie Client 夹具保存一次逻辑调用及重定向可能建立的两条连接。 */
typedef struct test_http_cookie {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Streams[2];
	xhttpclient* Client;
	xcookiejar* Cookies;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 NextStream;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	test_http_cookie_scenario Scenario;
	bool Responded[2];
	char URL[160];
	size_t URLSize;
	size_t HeaderCalls;
} test_http_cookie;



/* 在截止时间前等待一个并发计数达到目标值。 */
static void testHttpCookieWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 为隔离测试域名返回本机 IPv4。 */
static xnetaddrlist* testHttpCookieLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "cookie.test") == 0,
		"HTTP Cookie resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTP Cookie resolver fixture failed");
	return xrtNetAddrListCreate(&Address, 1);
}



/* 完成服务端半关闭。 */
static void testHttpCookieServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP Cookie server half-close failed"
	);
}



/* 记录每条服务端 Stream 的最终关闭。 */
static void testHttpCookieServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 查找当前服务端 Stream 的稳定测试索引。 */
static size_t testHttpCookieStreamIndex(
	const test_http_cookie* pState,
	const xnetstream* pStream
)
{
	size_t i;

	for ( i = 0; i < 2; i++ ) {
		if ( pState->Streams[i] == pStream ) {
			return i;
		}
	}
	testRequire(false, "HTTP Cookie server stream was not registered");
	return 0;
}



/* 验证请求 Cookie，并返回该跳的固定响应。 */
static cstr testHttpCookieResponse(
	test_http_cookie* pState,
	size_t iIndex,
	cstr sRequest
)
{
	static const cstr Automatic =
		"HTTP/1.1 103 Early Hints\r\n"
		"Set-Cookie: early=ignored; Path=/\r\n"
		"\r\n"
		"HTTP/1.1 200 OK\r\n"
		"Set-Cookie: next=two; Path=/; HttpOnly\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n\r\n"
		"OK";
	static const cstr Explicit =
		"HTTP/1.1 200 OK\r\n"
		"Set-Cookie: accepted=yes; Path=/\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n\r\n"
		"OK";
	static const cstr Disabled =
		"HTTP/1.1 200 OK\r\n"
		"Set-Cookie: ignored=yes; Path=/\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n\r\n"
		"OK";
	#if !defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		(void)iIndex;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		static const cstr Redirect =
			"HTTP/1.1 302 Found\r\n"
			"Location: /next\r\n"
			"Set-Cookie: hop=two; Path=/next; HttpOnly\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n\r\n";
		static const cstr Final =
			"HTTP/1.1 200 OK\r\n"
			"Set-Cookie: final=three; Path=/\r\n"
			"Content-Length: 2\r\n"
			"Connection: close\r\n\r\n"
			"OK";
	#endif

	if ( pState->Scenario == TEST_HTTP_COOKIE_EXPLICIT ) {
		testRequire(
			strstr(
				sRequest,
				"\r\nCookie: manual=yes\r\n"
			) != NULL,
			"explicit Cookie did not override CookieJar"
		);
		testRequire(
			strstr(sRequest, "seed=one") == NULL,
			"CookieJar replaced an explicit Cookie"
		);
		return Explicit;
	}
	if ( pState->Scenario == TEST_HTTP_COOKIE_DISABLED ) {
		testRequire(
			strstr(sRequest, "\r\nCookie:") == NULL,
			"disabled automatic Cookie was emitted"
		);
		return Disabled;
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pState->Scenario ==
			TEST_HTTP_COOKIE_REDIRECT ) {
			if ( iIndex == 0 ) {
				testRequire(
					strncmp(
						sRequest,
						"GET /start HTTP/1.1\r\n",
						21
					) == 0,
					"redirect Cookie start target mismatch"
				);
				testRequire(
					strstr(sRequest, "seed=one") != NULL,
					"redirect start omitted CookieJar entry"
				);
				return Redirect;
			}
			testRequire(
				strncmp(
					sRequest,
					"GET /next HTTP/1.1\r\n",
					20
				) == 0,
				"redirect Cookie final target mismatch"
			);
			testRequire(
				(strstr(sRequest, "seed=one") != NULL) &&
				(strstr(sRequest, "hop=two") != NULL),
				"redirect did not reselect newly stored Cookie"
			);
			return Final;
		}
	#endif
	testRequire(
		strstr(sRequest, "\r\nCookie: seed=one\r\n") != NULL,
		"automatic CookieJar entry was not emitted"
	);
	return Automatic;
}



/* 收到完整请求头后验证并发送场景响应。 */
static void testHttpCookieServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;
	char Request[2048];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iIndex;
	cstr sResponse;

	if ( iSize < 4 ) {
		return;
	}
	testRequire(
		iSize < sizeof(Request),
		"HTTP Cookie request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP Cookie request peek failed"
	);
	Request[iSize] = '\0';
	if ( strstr(Request, "\r\n\r\n") == NULL ) {
		return;
	}
	iIndex = testHttpCookieStreamIndex(
		pState,
		pStream
	);
	testRequire(
		!pState->Responded[iIndex],
		"HTTP Cookie fixture sent a duplicate response"
	);
	pState->Responded[iIndex] = true;
	sResponse = testHttpCookieResponse(
		pState,
		iIndex,
		Request
	);
	testRequire(
		xrtNetBufConsume(pBuffer, iSize) == iSize,
		"HTTP Cookie request consume failed"
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			sResponse,
			strlen(sResponse)
		) == XNET_RESULT_OK,
		"HTTP Cookie response send failed"
	);
}



/* 接管一条新服务端连接。 */
static bool testHttpCookieAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;
	xnetstreamevents Events;
	uint32 iIndex;

	(void)pListener;
	iIndex = xrtAtomic32FetchAdd(
		&pState->NextStream,
		1,
		XMEMORY_ACQ_REL
	);
	testRequire(
		iIndex < 2,
		"HTTP Cookie accepted too many connections"
	);
	pState->Streams[iIndex] = pStream;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpCookieServerRead;
	Events.End = testHttpCookieServerEnd;
	Events.Close = testHttpCookieServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"HTTP Cookie server event takeover failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已经关闭。 */
static void testHttpCookieListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 用户 Header 回调必须看到本次最终响应已经提交的 Cookie。 */
static bool testHttpCookieHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;
	size_t iExpected = 2;

	(void)pCall;
	(void)pResponse;
	if ( pState->Scenario == TEST_HTTP_COOKIE_DISABLED ) {
		iExpected = 1;
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pState->Scenario ==
			TEST_HTTP_COOKIE_REDIRECT ) {
			iExpected = 3;
		}
	#endif
	testRequire(
		xrtCookieJarCount(pState->Cookies) == iExpected,
		"HTTP Cookie was not committed before user Header callback"
	);
	pState->HeaderCalls++;
	return true;
}



/* 接管成功响应并验证包装器没有改变缓冲语义。 */
static void testHttpCookieDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_cookie* pState =
		(test_http_cookie*)pData;
	xbytesview Body;

	testRequire(
		(pCall == pState->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Error == NULL),
		"HTTP Cookie call result mismatch"
	);
	testRequire(
		(pCall->CookiePartitionKey == NULL) &&
		(pCall->CookiePartitionSize == 0) &&
		!pCall->CookieAutomatic,
		"HTTP Cookie terminal state retained call context"
	);
	Body = xrtHttpResponseBody(pResult->Response);
	testRequire(
		((xrtHttpResponseFlags(pResult->Response) &
		  XHTTP_RESPONSE_STREAMED) == 0) &&
		(Body.Size == 2) &&
		(memcmp(Body.Data, "OK", 2) == 0),
		"HTTP Cookie wrapper changed buffered body semantics"
	);
	testRequire(
		pState->HeaderCalls == 1u,
		"HTTP Cookie exposed the wrong final Header callbacks"
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		testRequire(
			pResult->Info.Redirects ==
				(pState->Scenario ==
				 TEST_HTTP_COOKIE_REDIRECT ? 1u : 0u),
			"HTTP Cookie redirect count mismatch"
		);
	#endif
	pState->Response = pResult->Response;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证场景结束后的 Jar 内容。 */
static void testHttpCookieJarResult(
	test_http_cookie* pState
)
{
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		char RedirectURL[160];
	#endif
	xstrview URL = {
		pState->URL,
		pState->URLSize
	};
	str sCookies;
	size_t iSize;

	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pState->Scenario ==
			TEST_HTTP_COOKIE_REDIRECT ) {
			cstr sStart = strstr(
				pState->URL,
				"/start"
			);
			size_t iPrefix;

			testRequire(
				sStart != NULL,
				"HTTP Cookie redirect URL fixture mismatch"
			);
			iPrefix = (size_t)(sStart - pState->URL);
			testRequire(
				(iPrefix + 5u) < sizeof(RedirectURL),
				"HTTP Cookie redirect result URL overflowed"
			);
			memcpy(
				RedirectURL,
				pState->URL,
				iPrefix
			);
			memcpy(
				RedirectURL + iPrefix,
				"/next",
				5
			);
			RedirectURL[iPrefix + 5u] = '\0';
			URL = (xstrview){
				RedirectURL,
				iPrefix + 5u
			};
		}
	#endif
	sCookies = xrtCookieJarBuildUrl(
		pState->Cookies,
		URL,
		&iSize
	);
	testRequire(
		sCookies != NULL,
		"HTTP Cookie result build failed"
	);
	if ( pState->Scenario == TEST_HTTP_COOKIE_DISABLED ) {
		testRequire(
			(iSize == 8) &&
			(memcmp(sCookies, "seed=one", 8) == 0),
			"disabled call changed CookieJar"
		);
	} else if ( pState->Scenario ==
		TEST_HTTP_COOKIE_EXPLICIT ) {
		testRequire(
			(strstr(sCookies, "seed=one") != NULL) &&
			(strstr(sCookies, "accepted=yes") != NULL),
			"explicit Cookie prevented Set-Cookie storage"
		);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
	} else if ( pState->Scenario ==
		TEST_HTTP_COOKIE_REDIRECT ) {
		testRequire(
			(strstr(sCookies, "seed=one") != NULL) &&
			(strstr(sCookies, "hop=two") != NULL) &&
			(strstr(sCookies, "final=three") != NULL),
			"redirect responses were not fully stored"
		);
	#endif
	} else {
		testRequire(
			(strstr(sCookies, "seed=one") != NULL) &&
			(strstr(sCookies, "next=two") != NULL) &&
			(strstr(sCookies, "early=ignored") == NULL),
			"final or informational Set-Cookie handling was wrong"
		);
	}
	xrtFree(sCookies);
}



/* 运行一个自动、显式、禁用或重定向 Cookie 场景。 */
static void testHttpCookieRun(
	test_http_cookie_scenario Scenario
)
{
	test_http_cookie State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xnetaddr Address;
	cstr sPath;
	uint32 iConnections;
	int iLength;
	uint32 i;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&State.NextStream, 0);
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	State.Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_COOKIE_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP Cookie engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP Cookie listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpCookieAccept;
	ListenerEvents.Close = testHttpCookieListenerClose;
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
		"HTTP Cookie listener creation failed"
	);

	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		sPath = Scenario == TEST_HTTP_COOKIE_REDIRECT ?
			"/start" : "/first";
	#else
		sPath = "/first";
	#endif
	iLength = snprintf(
		State.URL,
		sizeof(State.URL),
		"http://cookie.test:%u%s",
		(unsigned int)Address.Port,
		sPath
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(State.URL)),
		"HTTP Cookie URL fixture overflowed"
	);
	State.URLSize = (size_t)iLength;
	State.Cookies = xrtCookieJarCreate(NULL);
	testRequire(
		(State.Cookies != NULL) &&
		(xrtCookieJarStoreUrl(
			State.Cookies,
			(xstrview){ State.URL, State.URLSize },
			XRT_STR_LITERAL("seed=one; Path=/"),
			NULL
		) == XCOOKIE_STORE_STORED),
		"HTTP Cookie seed store failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpCookieLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Cookies = State.Cookies;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		(State.Client != NULL) &&
		(xrtHttpClientCookieJar(State.Client) ==
		 State.Cookies),
		"HTTP Client did not retain CookieJar"
	);
	xrtCookieJarRelease(State.Cookies);
	State.Cookies = xrtHttpClientCookieJar(
		State.Client
	);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ State.URL, State.URLSize }
	);
	testRequire(
		pRequest != NULL,
		"HTTP Cookie request creation failed"
	);
	if ( Scenario == TEST_HTTP_COOKIE_EXPLICIT ) {
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Cookie"),
				XRT_STR_LITERAL("manual=yes")
			),
			"explicit Cookie setup failed"
		);
	}
	xrtHttpCallOptionsInit(&Options);
	Options.Events.Headers = testHttpCookieHeaders;
	Options.Events.Data = &State;
	if ( Scenario == TEST_HTTP_COOKIE_DISABLED ) {
		Options.Cookies.Flags |= XHTTP_COOKIE_DISABLED;
	} else if ( Scenario == TEST_HTTP_COOKIE_AUTOMATIC ) {
		Options.Cookies.PartitionKey =
			XRT_STR_LITERAL("tenant-a");
	}
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		&Options,
		testHttpCookieDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP Cookie call submission failed"
	);
	testHttpCookieWait(
		&State.Completed,
		1,
		"HTTP Cookie call did not complete"
	);
	iConnections = 1;
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( Scenario == TEST_HTTP_COOKIE_REDIRECT ) {
			iConnections = 2;
		}
	#endif
	testHttpCookieWait(
		&State.Accepted,
		iConnections,
		"HTTP Cookie connections were not accepted"
	);
	testHttpCookieWait(
		&State.Closed,
		iConnections,
		"HTTP Cookie transports did not close"
	);
	testHttpCookieJarResult(&State);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP Cookie listener close failed"
	);
	testHttpCookieWait(
		&State.ListenerClosed,
		1,
		"HTTP Cookie listener did not close"
	);

	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	for ( i = 0; i < iConnections; i++ ) {
		xrtNetStreamDestroy(State.Streams[i]);
	}
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP Cookie engine destroy failed"
	);
}



/* 运行一次无网络 Cookie 选择并核对客户端到 Jar 的上下文映射。 */
static void testHttpCookiePrepareCase(
	xcookiejar* pJar,
	xstrview Method,
	xstrview URL,
	uint32 iFlags,
	xstrview PartitionKey,
	cstr sExpected
)
{
	xhttpclient Client;
	xhttpcall Call;
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	const xhttpfield* pCookie;

	pRequest = xrtHttpRequestCreate(Method, URL);
	testRequire(
		pRequest != NULL,
		"HTTP Cookie policy request creation failed"
	);
	memset(&Client, 0, sizeof(Client));
	Client.Cookies = pJar;
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	xrtHttpCallOptionsInit(&Options);
	Options.Cookies.Flags = iFlags;
	Options.Cookies.PartitionKey = PartitionKey;
	testRequire(
		__xrtHttpCookieInit(&Call, &Options) &&
		__xrtHttpCookiePrepare(&Call),
		"HTTP Cookie policy preparation failed"
	);
	pCookie = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Cookie")
	);
	if ( sExpected == NULL ) {
		testRequire(
			pCookie == NULL,
			"HTTP Cookie policy emitted an excluded Cookie"
		);
	} else {
		size_t iExpected = strlen(sExpected);

		testRequire(
			(pCookie != NULL) &&
			(pCookie->Value.Size == iExpected) &&
			(memcmp(
				pCookie->Value.Data,
				sExpected,
				iExpected
			) == 0),
			"HTTP Cookie policy selected the wrong Cookie set"
		);
	}
	__xrtHttpCookieUnit(&Call);
	testRequire(
		(Call.CookiePartitionKey == NULL) &&
		(Call.CookiePartitionSize == 0) &&
		!Call.CookieAutomatic,
		"HTTP Cookie policy cleanup retained call context"
	);
	xrtHttpRequestDestroy(pRequest);
}



/* 覆盖 SameSite、方法安全性、顶层请求和 CHIPS 分区键映射。 */
static void testHttpCookiePolicyMatrix(void)
{
	xcookiejar* pJar;
	xcookiestorecontext Store;
	xhttpclient Client;
	xhttpcall Call;
	xhttpcalloptions Options;
	xhttprequest* pRequest;

	pJar = xrtCookieJarCreate(NULL);
	testRequire(
		pJar != NULL,
		"HTTP Cookie policy Jar creation failed"
	);
	testRequire(
		(xrtCookieJarStoreUrl(
			pJar,
			XRT_STR_LITERAL("http://site.test/path"),
			XRT_STR_LITERAL(
				"strict=one; Path=/; SameSite=Strict"
			),
			NULL
		) == XCOOKIE_STORE_STORED) &&
		(xrtCookieJarStoreUrl(
			pJar,
			XRT_STR_LITERAL("http://site.test/path"),
			XRT_STR_LITERAL(
				"lax=one; Path=/; SameSite=Lax"
			),
			NULL
		) == XCOOKIE_STORE_STORED),
		"HTTP Cookie SameSite policy fixture failed"
	);
	memset(&Store, 0, sizeof(Store));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Store.URL = XRT_STR_LITERAL(
		"https://partition.test/path"
	);
	Store.PartitionKey = XRT_STR_LITERAL("tenant-a");
	testRequire(
		xrtCookieJarStore(
			pJar,
			&Store,
			XRT_STR_LITERAL(
				"part=one; Path=/; Secure; Partitioned"
			),
			NULL
		) == XCOOKIE_STORE_STORED,
		"HTTP Cookie partition policy fixture failed"
	);

	testHttpCookiePrepareCase(
		pJar,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://site.test/path"),
		0,
		(xstrview){ NULL, 0 },
		NULL
	);
	testHttpCookiePrepareCase(
		pJar,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://site.test/path"),
		XHTTP_COOKIE_TOP_LEVEL,
		(xstrview){ NULL, 0 },
		"lax=one"
	);
	testHttpCookiePrepareCase(
		pJar,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://site.test/path"),
		XHTTP_COOKIE_SAME_SITE,
		(xstrview){ NULL, 0 },
		"strict=one; lax=one"
	);
	testHttpCookiePrepareCase(
		pJar,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://partition.test/path"),
		XHTTP_COOKIE_SAME_SITE,
		XRT_STR_LITERAL("tenant-a"),
		"part=one"
	);
	testHttpCookiePrepareCase(
		pJar,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://partition.test/path"),
		XHTTP_COOKIE_SAME_SITE,
		XRT_STR_LITERAL("tenant-b"),
		NULL
	);

	memset(&Client, 0, sizeof(Client));
	Client.Cookies = pJar;
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://site.test/path")
	);
	testRequire(
		pRequest != NULL,
		"HTTP Cookie invalid option fixture failed"
	);
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	xrtHttpCallOptionsInit(&Options);
	Options.Cookies.Flags = UINT32_MAX;
	testRequire(
		!__xrtHttpCookieInit(&Call, &Options) &&
		(Call.CookieError == XHTTP_CLIENT_ERROR_COOKIE) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Cookie invalid flags were accepted"
	);
	xrtClearError();
	xrtHttpCallOptionsInit(&Options);
	Options.Cookies.PartitionKey =
		(xstrview){ NULL, 1u };
	testRequire(
		!__xrtHttpCookieInit(&Call, &Options) &&
		(Call.CookieError == XHTTP_CLIENT_ERROR_COOKIE) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Cookie invalid partition view was accepted"
	);
	xrtClearError();
	__xrtHttpCookieUnit(&Call);
	xrtHttpRequestDestroy(pRequest);
	xrtCookieJarRelease(pJar);
}



/* 验证响应存储沿用调用的同站与顶层上下文。 */
static void testHttpCookieStoreContext(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xhttpclient Client;
	xhttpcall Call;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;

	testRequire(pJar != NULL, "HTTP Cookie store context Jar failed");
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://site.test/path")
	);
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1, 200, XRT_STR_LITERAL("OK"), NULL
	);
	testRequire((pRequest != NULL) && (pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse, XRT_STR_LITERAL("Set-Cookie"),
			XRT_STR_LITERAL("sid=one; Path=/")
		), "HTTP Cookie store context response failed");
	memset(&Client, 0, sizeof(Client));
	Client.Cookies = pJar;
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	Call.CookiesEnabled = true;
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpCookieEvents(&Call, &Next);
	testRequire(pEvents->Headers(pResponse, pEvents->Data) &&
		(xrtCookieJarCount(pJar) == 0),
		"HTTP Cookie cross-site subresource stored a Default cookie");
	Call.CookieFlags = XHTTP_COOKIE_TOP_LEVEL;
	pEvents = __xrtHttpCookieEvents(&Call, &Next);
	testRequire(pEvents->Headers(pResponse, pEvents->Data) &&
		(xrtCookieJarCount(pJar) == 1u),
		"HTTP Cookie top-level response lost its storage context");
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
	xrtCookieJarRelease(pJar);
}



/* 覆盖自动选择、显式覆盖、逐调用禁用和重定向逐跳刷新。 */
int main(void)
{
	testHttpCookieOptionsStorage();
	testHttpCookiePolicyMatrix();
	testHttpCookieStoreContext();
	#if defined(TEST_HTTP_COOKIE_REDIRECT_ONLY)
		testHttpCookieRun(TEST_HTTP_COOKIE_REDIRECT);
	#else
		testHttpCookieRun(TEST_HTTP_COOKIE_AUTOMATIC);
		testHttpCookieRun(TEST_HTTP_COOKIE_EXPLICIT);
		testHttpCookieRun(TEST_HTTP_COOKIE_DISABLED);
	#endif
	printf(
		"[PASS] high-level HTTP Cookie policy (%s)\n",
		TEST_HTTP_COOKIE_BACKEND_NAME
	);
	return 0;
}
