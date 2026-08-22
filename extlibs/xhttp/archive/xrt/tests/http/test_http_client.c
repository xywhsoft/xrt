#include "../test.h"



#if !defined(TEST_HTTP_CLIENT_BACKEND)
	#define TEST_HTTP_CLIENT_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_BACKEND_NAME "select"
#endif



typedef enum test_http_client_scenario {
	TEST_HTTP_CLIENT_SUCCESS = 0,
	TEST_HTTP_CLIENT_TRAILING,
	TEST_HTTP_CLIENT_STREAM,
	TEST_HTTP_CLIENT_PAUSE,
	TEST_HTTP_CLIENT_IDLE_PROGRESS,
	TEST_HTTP_CLIENT_CANCEL,
	TEST_HTTP_CLIENT_CANCEL_BODY,
	TEST_HTTP_CLIENT_TIMEOUT,
	TEST_HTTP_CLIENT_IDLE_TIMEOUT,
	TEST_HTTP_CLIENT_CALLBACK,
	TEST_HTTP_CLIENT_PROTOCOL,
	TEST_HTTP_CLIENT_TRANSPORT
} test_http_client_scenario;



/* 高层客户端夹具只保存跨 Worker 发布后仍然有效的对象和终态。 */
typedef struct test_http_client {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpcall* EventCall;
	xhttpresponse* Response;
	xhttpcallinfo Info;
	xcancel* Cancel;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 Paused;
	xatomic32 ServerTailSent;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_http_client_scenario Scenario;
	size_t ChunkIndex;
	size_t Streamed;
	bool Responded;
	bool CancelAccepted;
} test_http_client;



/* 验证所有高层响应事件都直接携带同一个 Call。 */
static void testHttpClientEventCall(
	test_http_client* pState,
	xhttpcall* pCall
)
{
	testRequire(
		(pCall != NULL) &&
		((pState->EventCall == NULL) ||
		 (pState->EventCall == pCall)),
		"HTTP client event Call identity changed"
	);
	pState->EventCall = pCall;
}



/* 在截止时间前等待一个由网络 Worker 发布的状态。 */
static void testHttpClientWait(
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



/* 等待请求已完整发送并进入响应头阶段。 */
static void testHttpClientWaitRequestSent(xhttpcall* pCall)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	for ( ;; ) {
		xhttpcallinfo Info;

		testRequire(
			xrtHttpCallInfo(pCall, &Info),
			"HTTP client request progress query failed"
		);
		if ( (Info.RequestSent != 0) &&
			(Info.Phase == XHTTP_CALL_PHASE_RESPONSE_HEADERS) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP client request did not enter response wait"
		);
		xrtThreadYield();
	}
}



/* 为测试域名返回本机 IPv4，隔离操作系统 DNS 和外部网络。 */
static xnetaddrlist* testHttpClientLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "client.test") == 0,
		"HTTP client resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTP client resolver fixture failed");
	return xrtNetAddrListCreate(&Address, 1);
}



/* 正常响应完成后接收客户端 FIN，并完成服务端半关闭。 */
static void testHttpClientServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP client server half-close failed"
	);
}



/* 记录服务端传输已经释放底层套接字。 */
static void testHttpClientServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 按固定间隔发送剩余正文，验证每次真实读取都会延长空闲截止时间。 */
static void testHttpClientServerChunk(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	static const char Body[] = "BCDE";
	test_http_client* pState = (test_http_client*)pData;
	xnetstream* pStream = pState->Server;

	testRequire(
		(pWorker == xrtNetStreamWorker(pStream)) &&
		(Id != 0) &&
		(Result == XNET_RESULT_OK) &&
		(pState->ChunkIndex < (sizeof(Body) - 1u)),
		"HTTP client progress timer identity mismatch"
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			Body + pState->ChunkIndex,
			1
		) == XNET_RESULT_OK,
		"HTTP client progress body send failed"
	);
	pState->ChunkIndex++;
	if ( pState->ChunkIndex < (sizeof(Body) - 1u) ) {
		testRequire(
			xrtNetEngineAfter(
				pState->Engine,
				xrtNetWorkerIndex(pWorker),
				UINT64_C(100000),
				testHttpClientServerChunk,
				pState
			) != 0,
			"HTTP client progress timer reschedule failed"
		);
	}
}



/* 在客户端暂停读取后发送尾段，验证传输层门控不会继续交付正文。 */
static void testHttpClientServerTail(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;
	xnetstream* pStream = pState->Server;

	testRequire(
		(pWorker == xrtNetStreamWorker(pStream)) &&
		(Id != 0) &&
		(Result == XNET_RESULT_OK),
		"HTTP client pause timer identity mismatch"
	);
	testRequire(
		xrtNetStreamSend(pStream, "B", 1) == XNET_RESULT_OK,
		"HTTP client pause tail send failed"
	);
	xrtAtomic32Store(
		&pState->ServerTailSent,
		1,
		XMEMORY_RELEASE
	);
}



/* 成功场景收到完整请求头后发送固定长度响应，其他场景保持挂起。 */
static void testHttpClientServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";
	static const char TrailingResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OKextra";
	static const char ProgressResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"A";
	static const char PauseResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"A";
	static const char InvalidResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Invalid Header\r\n"
		"\r\n";
	test_http_client* pState = (test_http_client*)pData;
	char Request[1024];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t i;
	bool bComplete = false;

	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTP client request exceeded fixture capacity"
	);
	testRequire(xrtNetBufPeek(
		pBuffer,
		0,
		Request,
		iSize
	) == iSize, "HTTP client request peek failed");
	for ( i = 3; i < iSize; ++i ) {
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
		memcmp(Request, "GET /ready HTTP/1.1\r\n", 21) == 0,
		"HTTP client emitted the wrong request target"
	);
	testRequire(
		strstr(Request, "\r\nHost: client.test:") != NULL,
		"HTTP client omitted the effective Host port"
	);
	if ( (pState->Scenario == TEST_HTTP_CLIENT_CANCEL) ||
		(pState->Scenario == TEST_HTTP_CLIENT_TIMEOUT) ||
		(pState->Scenario ==
		 TEST_HTTP_CLIENT_IDLE_TIMEOUT) ) {
		return;
	}
	testRequire(
		!pState->Responded,
		"HTTP client fixture sent duplicate responses"
	);
	pState->Responded = true;
	testRequire(
		xrtNetBufConsume(pBuffer, iSize) == iSize,
		"HTTP client request consume failed"
	);
	if ( pState->Scenario == TEST_HTTP_CLIENT_TRANSPORT ) {
		testRequire(
			xrtNetStreamAbort(pStream),
			"HTTP client transport fixture abort failed"
		);
	} else if ( pState->Scenario == TEST_HTTP_CLIENT_PAUSE ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				PauseResponse,
				sizeof(PauseResponse) - 1u
			) == XNET_RESULT_OK,
			"HTTP client pause response head send failed"
		);
		testRequire(
			xrtNetEngineAfter(
				pState->Engine,
				xrtNetWorkerIndex(
					xrtNetStreamWorker(pStream)
				),
				UINT64_C(100000),
				testHttpClientServerTail,
				pState
			) != 0,
			"HTTP client pause tail timer start failed"
		);
	} else if (
		pState->Scenario == TEST_HTTP_CLIENT_PROTOCOL
	) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				InvalidResponse,
				sizeof(InvalidResponse) - 1u
			) == XNET_RESULT_OK,
			"HTTP client invalid response send failed"
		);
	} else if (
		pState->Scenario == TEST_HTTP_CLIENT_TRAILING
	) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				TrailingResponse,
				sizeof(TrailingResponse) - 1u
			) == XNET_RESULT_OK,
			"HTTP client trailing response send failed"
		);
	} else if (
		pState->Scenario == TEST_HTTP_CLIENT_SUCCESS ||
		pState->Scenario == TEST_HTTP_CLIENT_STREAM ||
		pState->Scenario == TEST_HTTP_CLIENT_CALLBACK ||
		pState->Scenario == TEST_HTTP_CLIENT_CANCEL_BODY
	) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				sizeof(Response) - 1u
			) == XNET_RESULT_OK,
			"HTTP client response send failed"
		);
	} else {
		testRequire(
			xrtNetStreamSend(
				pStream,
				ProgressResponse,
				sizeof(ProgressResponse) - 1u
			) == XNET_RESULT_OK,
			"HTTP client progress response head send failed"
		);
		testRequire(
			xrtNetEngineAfter(
				pState->Engine,
				xrtNetWorkerIndex(
					xrtNetStreamWorker(pStream)
				),
				UINT64_C(100000),
				testHttpClientServerChunk,
				pState
			) != 0,
			"HTTP client progress timer start failed"
		);
	}
}



/* 用自定义原因拒绝正文，验证高层不会把用户回调误报成协议错误。 */
static bool testHttpClientRejectBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;
	xerror* pError = xrtErrorCreate(
		XERR_MEMORY,
		"test.http.callback",
		71,
		"fixture body storage failed"
	);

	(void)pResponse;
	(void)Data;
	testHttpClientEventCall(pState, pCall);
	testRequire(
		pError != NULL,
		"HTTP client callback error creation failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 流式接受正文并验证分块顺序，终态统计必须保留已交付字节。 */
static bool testHttpClientStreamBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	static const char Expected[] = "OK";
	test_http_client* pState = (test_http_client*)pData;

	(void)pResponse;
	testHttpClientEventCall(pState, pCall);
	testRequire(
		(Data.Size <=
		 ((sizeof(Expected) - 1u) - pState->Streamed)) &&
		(memcmp(
			Data.Data,
			Expected + pState->Streamed,
			Data.Size
		) == 0),
		"HTTP client streamed body order mismatch"
	);
	pState->Streamed += Data.Size;
	return true;
}



/* 首段正文到达时暂停高层 Call，恢复后继续验证正文顺序。 */
static bool testHttpClientPauseBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	static const char Expected[] = "AB";
	test_http_client* pState = (test_http_client*)pData;

	(void)pResponse;
	testHttpClientEventCall(pState, pCall);
	testRequire(
		(Data.Size <=
		 ((sizeof(Expected) - 1u) - pState->Streamed)) &&
		(memcmp(
			Data.Data,
			Expected + pState->Streamed,
			Data.Size
		) == 0),
		"HTTP client paused body order mismatch"
	);
	pState->Streamed += Data.Size;
	if ( pState->Streamed == 1u ) {
		testRequire(
			xrtHttpCallPause(pCall),
			"HTTP client body callback pause was rejected"
		);
		xrtAtomic32Store(
			&pState->Paused,
			1,
			XMEMORY_RELEASE
		);
	}
	return true;
}



/* 从正文回调重入取消高层 Call，验证回调路径不持有所有权锁。 */
static bool testHttpClientCancelBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;

	(void)pResponse;
	testHttpClientEventCall(pState, pCall);
	testRequire(Data.Size != 0,
		"HTTP client cancellation body is empty");
	testRequire(
		xrtHttpCallCancel(pCall),
		"HTTP client reentrant cancellation was rejected"
	);
	pState->CancelAccepted = true;
	return true;
}



/* 接管 Listener 交付的服务端 Stream 并安装场景处理器。 */
static bool testHttpClientAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpClientServerRead;
	Events.End = testHttpClientServerEnd;
	Events.Close = testHttpClientServerClose;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		&Events,
		pState
	), "HTTP client server event takeover failed");
	pState->Server = pStream;
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已经完成关闭。 */
static void testHttpClientListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在回调入口验证响应所有权、终态和稳定错误链。 */
static void testHttpClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client* pState = (test_http_client*)pData;
	uint8 InfoStorage[sizeof(xhttpcallinfo) + 2u];
	xhttpcallinfo Info;

	testRequire(
		(pCall != NULL) && (pResult != NULL),
		"HTTP client returned an empty completion"
	);
	testRequire(
		xrtNetWorkerIsCurrent(xrtHttpCallWorker(pCall)),
		"HTTP client completion ran outside its stable Worker"
	);
	pState->CallbackCall = pCall;
	memset(InfoStorage, 0xA5, sizeof(InfoStorage));
	testRequire(
		xrtHttpCallInfo(
			pCall,
			(xhttpcallinfo*)(void*)(InfoStorage + 1u)
		) &&
		(InfoStorage[0] == 0xA5) &&
		(InfoStorage[sizeof(InfoStorage) - 1u] == 0xA5),
		"HTTP client Call info unaligned storage mismatch"
	);
	memcpy(&Info, InfoStorage + 1u, sizeof(Info));
	testRequire(
		(Info.State == pResult->Info.State) &&
		(Info.Phase == pResult->Info.Phase) &&
		(Info.Result == pResult->Info.Result) &&
		(Info.Error == pResult->Info.Error) &&
		(Info.Completed == pResult->Info.Completed),
		"HTTP client Call info result snapshot mismatch"
	);
	pState->Info = Info;
	if ( (pState->Scenario == TEST_HTTP_CLIENT_SUCCESS) ||
		(pState->Scenario == TEST_HTTP_CLIENT_TRAILING) ||
		(pState->Scenario == TEST_HTTP_CLIENT_STREAM) ||
		(pState->Scenario == TEST_HTTP_CLIENT_PAUSE) ||
		(pState->Scenario ==
		 TEST_HTTP_CLIENT_IDLE_PROGRESS) ) {
		size_t iBodySize =
			pState->Scenario == TEST_HTTP_CLIENT_IDLE_PROGRESS ?
				5u : 2u;
		cstr sBody =
			pState->Scenario == TEST_HTTP_CLIENT_IDLE_PROGRESS ?
				"ABCDE" :
				(pState->Scenario == TEST_HTTP_CLIENT_PAUSE ?
					"AB" : "OK");
		size_t iStored =
			((pState->Scenario == TEST_HTTP_CLIENT_STREAM) ||
			 (pState->Scenario == TEST_HTTP_CLIENT_PAUSE)) ?
				0u : iBodySize;

		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error == NULL) &&
			(pResult->Buffered == 0) &&
			!pResult->Upgraded &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_SUCCEEDED) &&
			(xrtHttpCallError(pCall) == NULL) &&
			(Info.State == XHTTP_CALL_SUCCEEDED) &&
			(Info.Phase ==
			 XHTTP_CALL_PHASE_RESPONSE_BODY) &&
			(Info.Result == XNET_RESULT_OK) &&
			(Info.Error == XHTTP_CLIENT_ERROR_NONE) &&
			(Info.Submitted != 0) &&
			(Info.Started >= Info.Submitted) &&
			(Info.TransportReady >= Info.Started) &&
			(Info.RequestSent >= Info.TransportReady) &&
			(Info.FirstByte >= Info.RequestSent) &&
			(Info.Headers >= Info.FirstByte) &&
			(Info.LastProgress >= Info.FirstByte) &&
			(Info.Completed >= Info.LastProgress) &&
			(Info.RequestWireBytes != 0) &&
			(Info.ResponseWireBytes != 0) &&
			(Info.ResponseBodyBytes == iBodySize) &&
			(Info.Redirects == 0) &&
			!Info.ReusedConnection &&
			!Info.Secure,
			"HTTP client success result mismatch"
		);
		testRequire(
			(xrtHttpResponseStatus(pResult->Response) == 200) &&
			(xrtHttpResponseBody(pResult->Response).Size ==
			 iStored) &&
			((iStored == 0) ||
			 (memcmp(
				xrtHttpResponseBody(
					pResult->Response
				).Data,
				sBody,
				iStored
			 ) == 0)) &&
			(((pState->Scenario !=
			   TEST_HTTP_CLIENT_STREAM) &&
			  (pState->Scenario !=
			   TEST_HTTP_CLIENT_PAUSE)) ||
			 (pState->Streamed == iBodySize)),
			"HTTP client response mismatch"
		);
		pState->Response = pResult->Response;
	} else if ( (pState->Scenario == TEST_HTTP_CLIENT_CANCEL) ||
		(pState->Scenario == TEST_HTTP_CLIENT_CANCEL_BODY) ) {
		bool bCancelResult =
			(pResult->Result == XNET_RESULT_CANCELLED) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_CANCELLED) &&
			(xrtErrorCode(pResult->Error) ==
			 XHTTP_CLIENT_ERROR_CANCELLED) &&
			(strcmp(
				xrtErrorDomain(pResult->Error),
				"xrt.http.client"
			) == 0) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_CANCELLED) &&
			(xrtHttpCallError(pCall) ==
			 pResult->Error) &&
			(Info.State == XHTTP_CALL_CANCELLED) &&
			(Info.Result == XNET_RESULT_CANCELLED) &&
			(Info.Error ==
			 XHTTP_CLIENT_ERROR_CANCELLED) &&
			(Info.Phase ==
			 (pState->Scenario ==
			  TEST_HTTP_CLIENT_CANCEL_BODY ?
				XHTTP_CALL_PHASE_RESPONSE_BODY :
				XHTTP_CALL_PHASE_RESPONSE_HEADERS)) &&
			(Info.Completed >= Info.Started);

		if ( !bCancelResult ) {
			fprintf(
				stderr,
				"[DETAIL] scenario=%u result=%d state=%u"
				" info_result=%d info_error=%u phase=%u"
				" started=%llu completed=%llu error_kind=%u"
				" error_code=%d\n",
				(uint32)pState->Scenario,
				(int)pResult->Result,
				(uint32)xrtHttpCallState(pCall),
				(int)Info.Result,
				(uint32)Info.Error,
				(uint32)Info.Phase,
				(unsigned long long)Info.Started,
				(unsigned long long)Info.Completed,
				pResult->Error == NULL ? 0u :
					(uint32)xrtErrorKind(pResult->Error),
				pResult->Error == NULL ? 0 :
					xrtErrorCode(pResult->Error)
			);
			testRequire(
				false,
				"HTTP client cancellation result mismatch"
			);
		}
		if ( pState->Scenario == TEST_HTTP_CLIENT_CANCEL_BODY ) {
			testRequire(
				pState->CancelAccepted,
				"HTTP client body callback did not accept cancellation"
			);
		}
	} else if (
		(pState->Scenario == TEST_HTTP_CLIENT_TIMEOUT) ||
		(pState->Scenario ==
		 TEST_HTTP_CLIENT_IDLE_TIMEOUT)
	) {
		xhttpclienterror Expected =
			pState->Scenario == TEST_HTTP_CLIENT_TIMEOUT ?
				XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL :
				XHTTP_CLIENT_ERROR_TIMEOUT_IDLE;

		testRequire(
			(pResult->Result == XNET_RESULT_TIMEOUT) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_TIMEOUT) &&
			(xrtErrorCode(pResult->Error) ==
			 (int32)Expected) &&
			(strcmp(
				xrtErrorDomain(pResult->Error),
				"xrt.http.client"
			) == 0) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_FAILED) &&
			(xrtHttpCallError(pCall) ==
			 pResult->Error) &&
			(Info.State == XHTTP_CALL_FAILED) &&
			(Info.Result == XNET_RESULT_TIMEOUT) &&
			(Info.Error == Expected) &&
			(Info.Phase ==
			 XHTTP_CALL_PHASE_RESPONSE_HEADERS) &&
			(Info.Completed >= Info.LastProgress),
			"HTTP client timeout result mismatch"
		);
	} else {
		xhttpclienterror Expected =
			pState->Scenario == TEST_HTTP_CLIENT_CALLBACK ?
				XHTTP_CLIENT_ERROR_CALLBACK :
				(pState->Scenario ==
				 TEST_HTTP_CLIENT_PROTOCOL ?
					XHTTP_CLIENT_ERROR_PROTOCOL :
					XHTTP_CLIENT_ERROR_TRANSPORT);
		xerrkind ExpectedKind =
			pState->Scenario == TEST_HTTP_CLIENT_CALLBACK ?
				XERR_MEMORY :
				(pState->Scenario ==
				 TEST_HTTP_CLIENT_PROTOCOL ?
					XERR_PROTOCOL :
					XERR_IO);
		const xerror* pCallError;

		testRequire(
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 ExpectedKind) &&
			(xrtErrorCode(pResult->Error) ==
			 (int32)Expected) &&
			(strcmp(
				xrtErrorDomain(pResult->Error),
				"xrt.http.client"
			) == 0) &&
			(Info.State == XHTTP_CALL_FAILED) &&
			(Info.Result == XNET_RESULT_ERROR) &&
			(Info.Error == Expected),
			"HTTP client classified failure mismatch"
		);
		pCallError = xrtErrorFind(
			pResult->Error,
			"xrt.http.call",
			pState->Scenario ==
				TEST_HTTP_CLIENT_TRANSPORT ?
				XHTTP1_CALL_ERROR_TRANSPORT :
				XHTTP1_CALL_ERROR_EXCHANGE
		);
		testRequire(
			pCallError != NULL,
			"HTTP client omitted the low-level Call cause"
		);
		if ( pState->Scenario == TEST_HTTP_CLIENT_CALLBACK ) {
			testRequire(
				(xrtErrorFind(
					pResult->Error,
					"xrt.http.exchange",
					XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK
				) != NULL) &&
				(xrtErrorFind(
					pResult->Error,
					"test.http.callback",
					71
				) != NULL),
				"HTTP client callback cause chain mismatch"
			);
		} else if (
			pState->Scenario == TEST_HTTP_CLIENT_PROTOCOL
		) {
			testRequire(
				xrtErrorFind(
					pResult->Error,
					"xrt.http.exchange",
					XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD
				) != NULL,
				"HTTP client protocol cause chain mismatch"
			);
		}
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 运行一条完整 URL 到 HTTP/1 终态的高层客户端场景。 */
static void testHttpClientRun(
	test_http_client_scenario Scenario
)
{
	test_http_client State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xnetaddr Address;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.Paused, 0);
	xrtAtomic32Init(&State.ServerTailSent, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	State.Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP client engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTP client listener address failed");
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpClientAccept;
	ListenerEvents.Close = testHttpClientListenerClose;
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
		"HTTP client listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	if ( Scenario == TEST_HTTP_CLIENT_PAUSE ) {
		/* 本次 Call 必须覆盖 Client 的普通响应累计上限。 */
		ClientConfig.Exchange.Body.MaxBody = 1u;
	}
	ClientConfig.Resolver.Lookup = testHttpClientLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		/* 基础生命周期测试不保留空闲连接，复用由 Pool 套件覆盖。 */
		ClientConfig.Pool.MaxIdle = 0;
	#endif
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTP client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://client.test:%u/ready",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP client URL fixture overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP client request creation failed"
	);
	xrtHttpCallOptionsInit(&Options);
	if ( Scenario == TEST_HTTP_CLIENT_CANCEL ) {
		State.Cancel = xrtCancelCreate();
		testRequire(
			State.Cancel != NULL,
			"HTTP client cancel token creation failed"
		);
		Options.Cancel = State.Cancel;
		Options.Timeout = UINT64_MAX - 1u;
		Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
	} else if ( Scenario == TEST_HTTP_CLIENT_TIMEOUT ) {
		Options.Timeout = 1000000u;
		Options.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
	} else if ( Scenario == TEST_HTTP_CLIENT_IDLE_TIMEOUT ) {
		Options.Timeout = 5000000u;
		Options.IdleTimeout = 200000u;
	} else if ( Scenario ==
		TEST_HTTP_CLIENT_IDLE_PROGRESS ) {
		Options.Timeout = 5000000u;
		Options.IdleTimeout = 250000u;
	} else if ( Scenario == TEST_HTTP_CLIENT_CALLBACK ) {
		Options.Events.Body = testHttpClientRejectBody;
		Options.Events.Data = &State;
	} else if ( Scenario == TEST_HTTP_CLIENT_STREAM ) {
		Options.Events.Body = testHttpClientStreamBody;
		Options.Events.Data = &State;
	} else if ( Scenario == TEST_HTTP_CLIENT_PAUSE ) {
		Options.Events.Body = testHttpClientPauseBody;
		Options.Events.Data = &State;
		Options.ResponseBodyLimit = UINT64_MAX;
	} else if ( Scenario == TEST_HTTP_CLIENT_CANCEL_BODY ) {
		Options.Events.Body = testHttpClientCancelBody;
		Options.Events.Data = &State;
	}
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		&Options,
		testHttpClientDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP client call submission failed"
	);
	testHttpClientWait(
		&State.Accepted,
		"HTTP client connection was not accepted"
	);
	if ( Scenario == TEST_HTTP_CLIENT_CANCEL ) {
		testHttpClientWaitRequestSent(State.Call);
		testRequire(
			(xrtHttpCallError(State.Call) == NULL),
			"HTTP client running call exposed a terminal error"
		);
		testRequire(
			xrtCancelRequest(State.Cancel),
			"HTTP client cancellation request failed"
		);
	} else if ( Scenario == TEST_HTTP_CLIENT_PAUSE ) {
		testHttpClientWait(
			&State.Paused,
			"HTTP client body callback did not pause"
		);
		testHttpClientWait(
			&State.ServerTailSent,
			"HTTP client server did not send the paused tail"
		);
		xrtSleep(20);
		testRequire(
			(xrtAtomic32Load(
				&State.Completed,
				XMEMORY_ACQUIRE
			) == 0) &&
			(State.Streamed == 1u) &&
			xrtHttpCallPaused(State.Call),
			"HTTP client delivered data through the pause gate"
		);
		testRequire(
			xrtHttpCallResume(State.Call) &&
			!xrtHttpCallResume(State.Call),
			"HTTP client cross-thread resume coalescing failed"
		);
	}
	testHttpClientWait(
		&State.Completed,
		"HTTP client call did not complete"
	);
	testRequire(
		(State.CallbackCall == State.Call) &&
		!xrtHttpCallCancel(State.Call),
		"HTTP client terminal call changed after completion"
	);
	if ( (Scenario == TEST_HTTP_CLIENT_STREAM) ||
		(Scenario == TEST_HTTP_CLIENT_PAUSE) ||
		(Scenario == TEST_HTTP_CLIENT_CANCEL_BODY) ||
		(Scenario == TEST_HTTP_CLIENT_CALLBACK) ) {
		testRequire(
			State.EventCall == State.Call,
			"HTTP client response event exposed a different Call"
		);
	}
	testHttpClientWait(
		&State.ServerClosed,
		"HTTP client transport did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP client listener close failed"
	);
	testHttpClientWait(
		&State.ListenerClosed,
		"HTTP client listener did not close"
	);

	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	xrtCancelDestroy(State.Cancel);
	xrtHttpClientDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP client engine destroy failed"
	);
}



/* 覆盖成功、截止时间、回调、协议和传输错误的稳定高层契约。 */
int main(void)
{
	testHttpClientRun(TEST_HTTP_CLIENT_SUCCESS);
	testHttpClientRun(TEST_HTTP_CLIENT_TRAILING);
	testHttpClientRun(TEST_HTTP_CLIENT_STREAM);
	testHttpClientRun(TEST_HTTP_CLIENT_PAUSE);
	testHttpClientRun(TEST_HTTP_CLIENT_IDLE_PROGRESS);
	testHttpClientRun(TEST_HTTP_CLIENT_CANCEL);
	testHttpClientRun(TEST_HTTP_CLIENT_CANCEL_BODY);
	testHttpClientRun(TEST_HTTP_CLIENT_TIMEOUT);
	testHttpClientRun(TEST_HTTP_CLIENT_IDLE_TIMEOUT);
	testHttpClientRun(TEST_HTTP_CLIENT_CALLBACK);
	testHttpClientRun(TEST_HTTP_CLIENT_PROTOCOL);
	testHttpClientRun(TEST_HTTP_CLIENT_TRANSPORT);
	printf(
		"[PASS] high-level HTTP client lifecycle (%s)\n",
		TEST_HTTP_CLIENT_BACKEND_NAME
	);
	return 0;
}
