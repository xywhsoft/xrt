#include "../test.h"



#ifndef TEST_HTTP_SERVER_BACKEND
	#define TEST_HTTP_SERVER_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_BACKEND_NAME
	#define TEST_HTTP_SERVER_BACKEND_NAME "select"
#endif



typedef struct test_http_server {
	xnetengine* Engine;
	xhttpserver* Server;
	xatomic32 Opened;
	xatomic32 Headers;
	xatomic32 Bodies;
	xatomic32 Requested;
	xatomic32 Closed;
	xatomic32 Errors;
	xatomic32 Shutdown;
	xatomic32 DrainReady;
	xatomic32 RequestBodyPaused;
	xhttpconn* PausedConnection;
	char StreamBody[64];
	size_t StreamBodySize;
} test_http_server;



typedef struct test_http_server_client {
	xnetstream* Stream;
	const char* Request;
	size_t RequestSize;
	const char* ContinueBody;
	size_t ContinueBodySize;
	xatomic32 Opened;
	xatomic32 Closed;
	xatomic32 BodySent;
	char Response[8192];
	size_t ResponseSize;
} test_http_server_client;



/* 在测试截止时间前等待 Worker 发布指定状态。 */
static void testHttpServerWait(
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



/* 比较借用字符串与零结尾测试文本。 */
static bool testHttpServerViewEqual(
	xstrview Value,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Value.Size == iSize) &&
		(memcmp(Value.Data, sExpected, iSize) == 0);
}



/* 验证固定响应失败使用统一 Server 错误域并清理线程错误。 */
static void testHttpServerResponseError(
	xerrkind Kind,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_SERVER_ERROR_RESPONSE),
		sMessage
	);
	xrtClearError();
}



/* 验证正文限额失败保留底层范围错误并提升到统一协议错误。 */
static void testHttpServerBodyLimitError(cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_RANGE) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_SERVER_ERROR_PROTOCOL) &&
		(strcmp(
			xrtErrorOperation(pError),
			"limit-http-request-body"
		 ) == 0),
		sMessage
	);
	xrtClearError();
}



/* 验证 HTTP Connection 在所属 Worker 上打开。 */
static void testHttpServerOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;
	xhttpreply* pReply;
	xnetaddr Local;
	xnetaddr Remote;

	testRequire(
		(pServer == pState->Server) &&
		(xrtHttpConnServer(pConnection) == pServer) &&
		(xrtHttpConnTcp(pConnection) != NULL) &&
		xrtNetWorkerIsCurrent(
			xrtHttpConnWorker(pConnection)
		) &&
		xrtHttpConnLocal(pConnection, &Local) &&
		xrtHttpConnRemote(pConnection, &Remote) &&
		(Local.Port != 0) &&
		(Remote.Port != 0),
		"HTTP server Open contract mismatch"
	);
	if ( xrtAtomic32Load(
		&pState->Opened,
		XMEMORY_ACQUIRE
	) == 0 ) {
		pReply = xrtHttpReplyCreate(200);
		testRequire(
			(pReply != NULL) &&
			(xrtHttpConnRequest(pConnection) == NULL) &&
			(xrtHttpConnRespond(
				pConnection,
				pReply
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"HTTP server allowed a final response without a request"
		);
		xrtHttpReplyDestroy(pReply);
		xrtClearError();

		pReply = xrtHttpReplyCreate(103);
		testRequire(
			(pReply != NULL) &&
			(xrtHttpConnInform(
				pConnection,
				pReply
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"HTTP server allowed information without a request"
		);
		xrtHttpReplyDestroy(pReply);
		xrtClearError();
	}
	(void)xrtAtomic32FetchAdd(
		&pState->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 按路由选择缓冲或流式正文策略。 */
static xhttpserverbodypolicy testHttpServerHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	testRequire(
		xrtHttpConnRequest(pConnection) == pRequest,
		"HTTP server did not publish the active request"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Headers,
		1,
		XMEMORY_RELEASE
	);
	if ( testHttpServerViewEqual(Target, "/header-hints") ) {
		xhttpreply* pHints = xrtHttpReplyCreate(103);

		testRequire(
			(pHints != NULL) &&
			xrtHttpReplyAddHeader(
				pHints,
				XRT_STR_LITERAL("Link"),
				XRT_STR_LITERAL("</header.css>; rel=preload")
			) &&
			(xrtHttpConnInform(
				pConnection,
				pHints
			 ) == XNET_RESULT_OK),
			"HTTP server Headers information response failed"
		);
		xrtHttpReplyDestroy(pHints);
	} else if ( testHttpServerViewEqual(
		Target, "/header-final"
	) ) {
		testRequire(
			xrtHttpConnReply(
				pConnection,
				401,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("early")
			) == XNET_RESULT_OK,
			"HTTP server header final response failed"
		);
		/* 最终响应已经决定正文策略，无正文请求仍应完成并保持复用。 */
		return XHTTP_SERVER_BODY_REJECT;
	} else if ( testHttpServerViewEqual(
		Target, "/large-allowed"
	) ) {
		testRequire(
			xrtHttpConnSetRequestBodyLimit(
				pConnection, 16
			),
			"HTTP server route body limit increase failed"
		);
	} else if ( testHttpServerViewEqual(
		Target, "/small-limit"
	) ) {
		testRequire(
			!xrtHttpConnSetRequestBodyLimit(
				pConnection, 3
			),
			"HTTP server route body limit accepted an oversized body"
		);
		testHttpServerBodyLimitError(
			"HTTP server route body limit error mismatch"
		);
	} else if ( testHttpServerViewEqual(
		Target, "/reject"
	) ) {
		testRequire(
			xrtHttpConnReply(
				pConnection,
				403,
				XRT_STR_LITERAL(
					"application/json; charset=utf-8"
				),
				XRT_BYTES_LITERAL(
					"{\"code\":403,\"msg\":\"Forbidden\"}"
				)
			) == XNET_RESULT_OK,
			"HTTP server early rejection response failed"
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( testHttpServerViewEqual(Target, "/stream") ||
		testHttpServerViewEqual(Target, "/pause-body") ||
		testHttpServerViewEqual(Target, "/body-final") ||
		testHttpServerViewEqual(Target, "/stream-fail") ) {
		pState->StreamBodySize = 0;
		return XHTTP_SERVER_BODY_STREAM;
	}
	if ( testHttpServerViewEqual(Target, "/discard") ) {
		return XHTTP_SERVER_BODY_DISCARD;
	}
	return XHTTP_SERVER_BODY_BUFFER;
}



/* 收集流式正文并验证片段只在所属 Worker 借用。 */
static bool testHttpServerBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	testRequire(
		(xrtHttpConnRequest(pConnection) == pRequest) &&
		(Data.Data != NULL) &&
		(Data.Size <=
		 (sizeof(pState->StreamBody) -
		  pState->StreamBodySize)),
		"HTTP server streaming body contract mismatch"
	);
	memcpy(
		pState->StreamBody + pState->StreamBodySize,
		Data.Data,
		Data.Size
	);
	pState->StreamBodySize += Data.Size;
	(void)xrtAtomic32FetchAdd(
		&pState->Bodies,
		1,
		XMEMORY_RELEASE
	);
	if ( testHttpServerViewEqual(Target, "/body-final") ) {
		xhttpconnstats Stats;

		testRequire(
			(Data.Size == 1) && (Data.Data[0] == 'D'),
			"HTTP server early body response received an unexpected prefix"
		);
		testRequire(
			xrtHttpConnPauseRequestBody(pConnection) &&
			xrtHttpConnRequestBodyPaused(pConnection),
			"HTTP server early body response could not pause input"
		);
		testRequire(
			xrtHttpConnReply(
				pConnection,
				XHTTP_STATUS_UNPROCESSABLE_CONTENT,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("stopped")
			) == XNET_RESULT_OK,
			"HTTP server early body response failed"
		);
		testRequire(
			xrtHttpConnStats(pConnection, &Stats) &&
			!xrtHttpConnRequestBodyPaused(pConnection) &&
			(Stats.State == XHTTP_CONN_RESPONSE) &&
			Stats.FinalCommitted,
			"HTTP server early body response state mismatch"
		);
		/* 已提交的最终响应优先于回调返回值，不能退化为第二条 500。 */
		return false;
	}
	if ( testHttpServerViewEqual(Target, "/pause-body") &&
		!xrtAtomic32Load(
			&pState->RequestBodyPaused,
			XMEMORY_ACQUIRE
		) ) {
		xhttpconnstats Stats;
		xhttpconn* pRetained = xrtHttpConnRef(pConnection);

		testRequire(
			pRetained != NULL,
			"HTTP server paused body connection retain failed"
		);
		testRequire(
			xrtHttpConnPauseRequestBody(pConnection),
			"HTTP server request body pause operation failed"
		);
		testRequire(
			xrtHttpConnRequestBodyPaused(pConnection),
			"HTTP server request body pause snapshot failed"
		);
		testRequire(
			xrtHttpConnStats(pConnection, &Stats),
			"HTTP server paused body stats query failed"
		);
		testRequire(
			Stats.RequestBodyPaused,
			"HTTP server paused body stats flag missing"
		);
		testRequire(
			Stats.State == XHTTP_CONN_BODY,
			"HTTP server paused body state mismatch"
		);
		pState->PausedConnection = pRetained;
		{
			xhttpreply* pInformation =
				xrtHttpReplyCreate(103);

			testRequire(
				(pInformation != NULL) &&
				(xrtHttpConnInform(
					pConnection,
					pInformation
				 ) == XNET_RESULT_OK),
				"HTTP server paused body information failed"
			);
			xrtHttpReplyDestroy(pInformation);
		}
		xrtAtomic32Store(
			&pState->RequestBodyPaused,
			1,
			XMEMORY_RELEASE
		);
	}
	if ( testHttpServerViewEqual(Target, "/stream-fail") ) {
		return false;
	}
	return true;
}



/* 排空场景稍后在同一 Worker 提交最终响应。 */
static void testHttpServerDelayedReply(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	testRequire(
		(Id != 0) &&
		(Result == XNET_RESULT_OK) &&
		(pWorker == xrtHttpConnWorker(pConnection)) &&
		(xrtHttpConnReply(
			pConnection,
			200,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("drained")
		 ) == XNET_RESULT_OK),
		"HTTP server delayed drain response failed"
	);
	xrtHttpConnDestroy(pConnection);
}



/* 验证完整请求并使用一调用响应 Helper。 */
static void testHttpServerRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	xbytesview Body = xrtHttpServerRequestBody(pRequest);
	xbytesview Reply = XRT_BYTES_LITERAL("ok");

	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtHttpServerRequestFlags(pRequest) &
		 XHTTP_SERVER_REQUEST_COMPLETE) != 0,
		"HTTP server delivered an incomplete request"
	);
	if ( testHttpServerViewEqual(Target, "/buffer") ) {
		testRequire(
			(Body.Size == 4) &&
			(memcmp(Body.Data, "DATA", 4) == 0),
			"HTTP server buffered body mismatch"
		);
		Reply = XRT_BYTES_LITERAL("buffered");
	} else if ( testHttpServerViewEqual(
		Target, "/large-allowed"
	) ) {
		testRequire(
			(Body.Size == 9) &&
			(memcmp(Body.Data, "123456789", 9) == 0),
			"HTTP server route body limit payload mismatch"
		);
		Reply = XRT_BYTES_LITERAL("large-allowed");
	} else if ( testHttpServerViewEqual(Target, "/stream") ) {
		testRequire(
			(Body.Size == 0) &&
			(pState->StreamBodySize == 6) &&
			(memcmp(
				pState->StreamBody,
				"STREAM",
				6
			 ) == 0),
			"HTTP server streamed body mismatch"
		);
		Reply = XRT_BYTES_LITERAL("streamed");
	} else if ( testHttpServerViewEqual(
		Target, "/pause-body"
	) ) {
		testRequire(
			(Body.Size == 0) &&
			(pState->StreamBodySize == 6) &&
			(memcmp(
				pState->StreamBody,
				"PAUSED",
				6
			 ) == 0) &&
			!xrtHttpConnRequestBodyPaused(pConnection),
			"HTTP server resumed body mismatch"
		);
		Reply = XRT_BYTES_LITERAL("resumed");
	} else if ( testHttpServerViewEqual(Target, "/discard") ) {
		uint32 iFlags = xrtHttpServerRequestFlags(pRequest);

		testRequire(
			(Body.Size == 0) &&
			(xrtHttpServerRequestBodyBytes(pRequest) == 7) &&
			((iFlags & XHTTP_SERVER_REQUEST_STREAMED) != 0) &&
			((iFlags & XHTTP_SERVER_REQUEST_DISCARDED) != 0),
			"HTTP server discarded body mismatch"
		);
		Reply = XRT_BYTES_LITERAL("discarded");
	} else if ( testHttpServerViewEqual(Target, "/expect") ) {
		testRequire(
			(Body.Size == 4) &&
			(memcmp(Body.Data, "WAIT", 4) == 0),
			"HTTP server Expect body mismatch"
		);
		Reply = XRT_BYTES_LITERAL("continued");
	} else if ( testHttpServerViewEqual(Target, "/one") ) {
		Reply = XRT_BYTES_LITERAL("one");
	} else if ( testHttpServerViewEqual(Target, "/two") ) {
		Reply = XRT_BYTES_LITERAL("two");
	} else if ( testHttpServerViewEqual(
		Target, "/header-hints"
	) ) {
		Reply = XRT_BYTES_LITERAL("header-hinted");
	} else if ( testHttpServerViewEqual(Target, "/timeout") ) {
		return;
	} else if ( testHttpServerViewEqual(Target, "/hints") ) {
		xhttpreply* pEarly = xrtHttpReplyCreate(103);
		xhttpreply* pProcessing = xrtHttpReplyCreate(102);

		testRequire(
			(pEarly != NULL) &&
			(pProcessing != NULL) &&
			xrtHttpReplyAddHeader(
				pEarly,
				XRT_STR_LITERAL("Link"),
				XRT_STR_LITERAL("</style.css>; rel=preload")
			) &&
			(xrtHttpConnInform(
				pConnection,
				pEarly
			 ) == XNET_RESULT_OK) &&
			(xrtHttpConnInform(
				pConnection,
				pProcessing
			 ) == XNET_RESULT_OK) &&
			(xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("hinted")
			 ) == XNET_RESULT_OK),
			"HTTP server queued information responses failed"
		);
		xrtHttpReplyDestroy(pEarly);
		xrtHttpReplyDestroy(pProcessing);
		return;
	} else if ( testHttpServerViewEqual(Target, "/drain") ) {
		xhttpconn* pRetained = xrtHttpConnRef(pConnection);

		testRequire(
			(pRetained != NULL) &&
			(xrtNetEngineAfter(
				pState->Engine,
				xrtNetWorkerIndex(
					xrtHttpConnWorker(pConnection)
				),
				UINT64_C(50000),
				testHttpServerDelayedReply,
				pRetained
			 ) != 0),
			"HTTP server drain timer could not start"
		);
		xrtAtomic32Store(
			&pState->DrainReady,
			1,
			XMEMORY_RELEASE
		);
		return;
	} else if ( testHttpServerViewEqual(Target, "/json") ) {
		testRequire(
			xrtHttpConnReply(
				pConnection,
				99,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("invalid")
			) == XNET_RESULT_ERROR,
			"HTTP server accepted an invalid fixed Reply status"
		);
		testHttpServerResponseError(
			XERR_RANGE,
			"HTTP server invalid fixed Reply error mismatch"
		);
		testRequire(
			xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain\r\nInjected: yes"),
				XRT_BYTES_LITERAL("invalid")
			) == XNET_RESULT_ERROR,
			"HTTP server accepted an invalid fixed Content-Type"
		);
		testHttpServerResponseError(
			XERR_VALUE,
			"HTTP server invalid fixed Content-Type error mismatch"
		);
		testRequire(
			xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain"),
				(xbytesview){ NULL, 1 }
			) == XNET_RESULT_ERROR,
			"HTTP server accepted an invalid fixed body view"
		);
		testHttpServerResponseError(
			XERR_ARGUMENT,
			"HTTP server invalid fixed body error mismatch"
		);
		Reply = XRT_BYTES_LITERAL("{\"code\":200}");
	}
	if ( testHttpServerViewEqual(Target, "/json") ) {
		xhttpbody* pReplyBody = xrtHttpBodyBorrow(Reply);

		testRequire(
			(pReplyBody != NULL) &&
			(xrtHttpConnReplyBody(
				pConnection,
				200,
				XRT_STR_LITERAL(
					"application/json; charset=utf-8"
				),
				pReplyBody
			 ) == XNET_RESULT_OK),
			"HTTP server Body response submission failed"
		);
		xrtHttpBodyDestroy(pReplyBody);
	} else {
		testRequire(
			xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain"),
				Reply
			) == XNET_RESULT_OK,
			"HTTP server final response submission failed"
		);
	}
	(void)pServer;
}



/* 记录预期协议和超时错误。 */
static void testHttpServerError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;

	(void)pServer;
	(void)pConnection;
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0),
		"HTTP server Error did not expose its stable domain"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录每个 HTTP Connection 的唯一终态。 */
static void testHttpServerClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;
	xhttpconnstats Stats;

	(void)pServer;
	(void)Result;
	(void)pError;
	testRequire(
		xrtHttpConnStats(pConnection, &Stats) &&
		(Stats.State == XHTTP_CONN_CLOSED),
		"HTTP server Close did not publish terminal stats"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 排空完成的唯一终态。 */
static void testHttpServerShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server* pState = (test_http_server*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP server Shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端打开后发送当前场景的请求前缀。 */
static void testHttpServerClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_http_server_client* pClient =
		(test_http_server_client*)pData;

	testRequire(
		xrtNetStreamSend(
			pStream,
			pClient->Request,
			pClient->RequestSize
		) == XNET_RESULT_OK,
		"HTTP server test request send failed"
	);
	xrtAtomic32Store(
		&pClient->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 收集响应；收到 100 Continue 后才发送等待中的正文。 */
static void testHttpServerClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_client* pClient =
		(test_http_server_client*)pData;
	size_t iAvailable = xrtNetBufSize(pBuffer);

	testRequire(
		iAvailable <
		(sizeof(pClient->Response) -
		 pClient->ResponseSize),
		"HTTP server response exceeded test capacity"
	);
	testRequire(
		xrtNetBufRead(
			pBuffer,
			pClient->Response + pClient->ResponseSize,
			iAvailable
		) == iAvailable,
		"HTTP server client response consume failed"
	);
	pClient->ResponseSize += iAvailable;
	pClient->Response[pClient->ResponseSize] = '\0';
	if ( (pClient->ContinueBody != NULL) &&
		!xrtAtomic32Load(
			&pClient->BodySent,
			XMEMORY_ACQUIRE
		) &&
		(strstr(
			pClient->Response,
			"HTTP/1.1 100 Continue\r\n\r\n"
		 ) != NULL) ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				pClient->ContinueBody,
				pClient->ContinueBodySize
			) == XNET_RESULT_OK,
			"HTTP server Expect body send failed"
		);
		xrtAtomic32Store(
			&pClient->BodySent,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 对端写关闭后结束客户端连接。 */
static void testHttpServerClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP server client close after EOF failed"
	);
}



/* 发布客户端连接终态和完整响应。 */
static void testHttpServerClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_client* pClient =
		(test_http_server_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pClient->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 启动一个真实 TCP 客户端。 */
static void testHttpServerStartClient(
	test_http_server* pState,
	const xnetaddr* pAddress,
	cstr sRequest,
	cstr sContinueBody,
	test_http_server_client* pClient
)
{
	xnetstreamconfig Config;
	xnetstreamevents Events;

	memset(pClient, 0, sizeof(*pClient));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&pClient->Opened, 0);
	xrtAtomic32Init(&pClient->Closed, 0);
	xrtAtomic32Init(&pClient->BodySent, 0);
	pClient->Request = sRequest;
	pClient->RequestSize = strlen(sRequest);
	pClient->ContinueBody = sContinueBody;
	pClient->ContinueBodySize =
		sContinueBody != NULL ? strlen(sContinueBody) : 0;
	Events.Open = testHttpServerClientOpen;
	Events.Read = testHttpServerClientRead;
	Events.End = testHttpServerClientEnd;
	Events.Close = testHttpServerClientClose;
	xrtNetStreamConfigInit(&Config);
	Config.ReadSize = 7;
	Config.ReadLimit = sizeof(pClient->Response);
	Config.WriteHighWater = 2048;
	Config.WriteLowWater = 1024;
	Config.WriteLimit = 4096;
	pClient->Stream = xrtNetStreamConnect(
		pState->Engine,
		pAddress,
		1,
		&Config,
		&Events,
		pClient
	);
	testRequire(
		pClient->Stream != NULL,
		"HTTP server test client could not connect"
	);
}



/* 运行一个真实 TCP 客户端直到服务端关闭连接。 */
static void testHttpServerRunClient(
	test_http_server* pState,
	const xnetaddr* pAddress,
	cstr sRequest,
	cstr sContinueBody,
	test_http_server_client* pClient
)
{
	testHttpServerStartClient(
		pState,
		pAddress,
		sRequest,
		sContinueBody,
		pClient
	);
	testHttpServerWait(
		&pClient->Closed,
		1,
		"HTTP server test client did not close"
	);
}



/* 验证响应包含指定线路文本。 */
static void testHttpServerContains(
	const test_http_server_client* pClient,
	cstr sNeedle,
	cstr sMessage
)
{
	testRequire(
		strstr(pClient->Response, sNeedle) != NULL,
		sMessage
	);
}



/* 销毁已进入终态的测试客户端。 */
static void testHttpServerClientDestroy(
	test_http_server_client* pClient
)
{
	xrtNetStreamDestroy(pClient->Stream);
	pClient->Stream = NULL;
}



/*
	禁用空闲超时不应禁用后续请求的 Header 超时。
	首个 keep-alive 请求完成后，第二条部分首行必须收到 408。
*/
static void testHttpServerKeepAliveHeaderTimeout(void)
{
	test_http_server State;
	test_http_server_client Client;
	uint8 ServerConfigStorage[sizeof(xhttpserverconfig) + 2u];
	uint8 EventsStorage[sizeof(xhttpserverevents) + 2u];
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Opened, 0);
	xrtAtomic32Init(&State.Headers, 0);
	xrtAtomic32Init(&State.Bodies, 0);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.DrainReady, 0);
	xrtAtomic32Init(&State.RequestBodyPaused, 0);

	/* 建立只关闭 IdleTimeout 的独立服务端，避免主场景配置干扰。 */
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP server keep-alive timeout engine failed"
	);
	memset(ServerConfigStorage, 0xA5, sizeof(ServerConfigStorage));
	xrtHttpServerConfigInit((xhttpserverconfig*)(void*)(
		ServerConfigStorage + 1u
	));
	memcpy(
		&ServerConfig,
		ServerConfigStorage + 1u,
		sizeof(ServerConfig)
	);
	testRequire((ServerConfig.WriteSize == 16384u) &&
		(ServerConfigStorage[0] == 0xA5) &&
		(ServerConfigStorage[sizeof(ServerConfigStorage) - 1u] == 0xA5),
		"HTTP server config init did not support unaligned storage");
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server keep-alive timeout address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 2;
	ServerConfig.HeaderTimeout = UINT64_C(200000);
	ServerConfig.BodyTimeout = UINT64_C(1000000);
	ServerConfig.RequestTimeout = UINT64_C(1000000);
	ServerConfig.IdleTimeout = 0;
	ServerConfig.WriteTimeout = UINT64_C(1000000);
	memset(EventsStorage, 0xA5, sizeof(EventsStorage));
	xrtHttpServerEventsInit((xhttpserverevents*)(void*)(
		EventsStorage + 1u
	));
	memcpy(&Events, EventsStorage + 1u, sizeof(Events));
	Events.Open = testHttpServerOpen;
	Events.Headers = testHttpServerHeaders;
	Events.Body = testHttpServerBody;
	Events.Request = testHttpServerRequest;
	Events.Close = testHttpServerClose;
	Events.Error = testHttpServerError;
	Events.Shutdown = testHttpServerShutdown;
	Events.Data = &State;
	memcpy(
		ServerConfigStorage + 1u,
		&ServerConfig,
		sizeof(ServerConfig)
	);
	memcpy(EventsStorage + 1u, &Events, sizeof(Events));
	State.Server = xrtHttpServerStart(
		State.Engine,
		(const xhttpserverconfig*)(const void*)(
			ServerConfigStorage + 1u
		),
		(const xhttpserverevents*)(const void*)(
			EventsStorage + 1u
		)
	);
	memset(ServerConfigStorage + 1u, 0, sizeof(ServerConfig));
	memset(EventsStorage + 1u, 0, sizeof(Events));
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address) &&
		(ServerConfigStorage[0] == 0xA5) &&
		(ServerConfigStorage[sizeof(ServerConfigStorage) - 1u] == 0xA5) &&
		(EventsStorage[0] == 0xA5) &&
		(EventsStorage[sizeof(EventsStorage) - 1u] == 0xA5),
		"HTTP server keep-alive timeout start failed"
	);

	/* 同一连接先完成请求，再留下第二条不完整请求等待 Header Timer。 */
	testHttpServerRunClient(
		&State,
		&Address,
		"GET /one HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"\r\n"
		"GET /",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\none",
		"HTTP server keep-alive first response missing"
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 408 Request Timeout\r\n",
		"HTTP server keep-alive header timeout missing"
	);
	testHttpServerClientDestroy(&Client);

	/* 排空并验证该场景只完成一条请求，第二条在 Header 阶段终止。 */
	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTP server keep-alive timeout drain failed"
	);
	testHttpServerWait(
		&State.Shutdown,
		1,
		"HTTP server keep-alive timeout shutdown missing"
	);
	testHttpServerWait(
		&State.Closed,
		1,
		"HTTP server keep-alive timeout close missing"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &Stats) &&
		(Stats.Accepted == 1u) &&
		(Stats.Requests == 1u) &&
		(Stats.Responses == 2u) &&
		(Stats.ProtocolErrors == 1u) &&
		(Stats.Timeouts == 1u) &&
		(xrtAtomic32Load(
			&State.Requested,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 1u),
		"HTTP server keep-alive timeout statistics mismatch"
	);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP server keep-alive timeout engine destroy failed"
	);
}



int main(void)
{
	test_http_server State;
	test_http_server_client Client;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Opened, 0);
	xrtAtomic32Init(&State.Headers, 0);
	xrtAtomic32Init(&State.Bodies, 0);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.DrainReady, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP server select engine could not start"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server loopback address setup failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 4;
	ServerConfig.Network.Listen.Stream.ReadSize = 5;
	ServerConfig.Network.Listen.Stream.ReadLimit = 4096;
	ServerConfig.Network.Listen.Stream.WriteHighWater = 14;
	ServerConfig.Network.Listen.Stream.WriteLowWater = 7;
	ServerConfig.Network.Listen.Stream.WriteLimit = 32;
	ServerConfig.Http1.Body.MaxBody = 8;
	ServerConfig.WriteSize = 7;
	ServerConfig.HeaderTimeout = UINT64_C(200000);
	ServerConfig.BodyTimeout = UINT64_C(200000);
	ServerConfig.RequestTimeout = UINT64_C(200000);
	ServerConfig.IdleTimeout = UINT64_C(200000);
	ServerConfig.WriteTimeout = UINT64_C(1000000);
	xrtHttpServerEventsInit(&Events);
	Events.Open = testHttpServerOpen;
	Events.Headers = testHttpServerHeaders;
	Events.Body = testHttpServerBody;
	Events.Request = testHttpServerRequest;
	Events.Close = testHttpServerClose;
	Events.Error = testHttpServerError;
	Events.Shutdown = testHttpServerShutdown;
	Events.Data = &State;
	State.Server = xrtHttpServerStart(
		State.Engine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address) &&
		(Address.Port != 0),
		"HTTP server could not start on a dynamic port"
	);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /json HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 200 OK\r\n",
		"HTTP server JSON status missing"
	);
	testHttpServerContains(
		&Client,
		"Content-Type: application/json; charset=utf-8\r\n",
		"HTTP server JSON content type missing"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\n{\"code\":200}",
		"HTTP server JSON body mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerStartClient(
		&State,
		&Address,
		"POST /pause-body HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 6\r\n"
		"Connection: close\r\n"
		"\r\n"
		"PAUSED",
		NULL,
		&Client
	);
	testHttpServerWait(
		&State.RequestBodyPaused,
		1,
		"HTTP server request body did not pause"
	);
	testRequire(
		(State.PausedConnection != NULL) &&
		xrtHttpConnRequestBodyPaused(
			State.PausedConnection
		) &&
		!xrtHttpConnPauseRequestBody(
			State.PausedConnection
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP server request body accepted a foreign-thread pause"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnResumeRequestBody(
			State.PausedConnection
		) &&
		xrtHttpConnResumeRequestBody(
			State.PausedConnection
		),
		"HTTP server request body cross-thread resume failed"
	);
	testHttpServerWait(
		&Client.Closed,
		1,
		"HTTP server resumed request did not close"
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 103 Early Hints\r\n",
		"HTTP server paused body information missing"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\nresumed",
		"HTTP server resumed request response mismatch"
	);
	testRequire(
		!xrtHttpConnRequestBodyPaused(
			State.PausedConnection
		),
		"HTTP server retained request body pause after completion"
	);
	xrtHttpConnDestroy(State.PausedConnection);
	State.PausedConnection = NULL;
	testHttpServerClientDestroy(&Client);

	/* 路由可在 Headers 回调中收紧全局限额并自动产生 413。 */
	testHttpServerRunClient(
		&State,
		&Address,
		"POST /small-limit HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n"
		"\r\n"
		"DATA",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 413 Content Too Large\r\n",
		"HTTP server route body limit status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /header-final HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"\r\n"
		"GET /one HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	{
		char* pEarly = strstr(
			Client.Response,
			"HTTP/1.1 401 Unauthorized\r\n"
		);
		char* pNext = strstr(
			Client.Response,
			"HTTP/1.1 200 OK\r\n"
		);

		testRequire(
			(pEarly != NULL) &&
			(pNext != NULL) &&
			(pEarly < pNext) &&
			(strstr(
				Client.Response,
				"\r\n\r\nearlyHTTP/1.1 200 OK\r\n"
			 ) != NULL) &&
			(strstr(pNext, "\r\n\r\none") != NULL),
			"HTTP server header final response did not preserve keep-alive"
		);
	}
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /hints HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	{
		char* pEarly = strstr(
			Client.Response,
			"HTTP/1.1 103 Early Hints\r\n"
		);
		char* pProcessing = strstr(
			Client.Response,
			"HTTP/1.1 102 Processing\r\n"
		);
		char* pFinal = strstr(
			Client.Response,
			"HTTP/1.1 200 OK\r\n"
		);

		testRequire(
			(pEarly != NULL) &&
			(pProcessing != NULL) &&
			(pFinal != NULL) &&
			(pEarly < pProcessing) &&
			(pProcessing < pFinal) &&
			(strstr(
				Client.Response,
				"Link: </style.css>; rel=preload\r\n"
			 ) != NULL) &&
			(strstr(
				Client.Response,
				"\r\n\r\nhinted"
			 ) != NULL),
			"HTTP server information response order mismatch"
		);
	}
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /body-final HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 4\r\n"
		"\r\n"
		"D",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 422 Unprocessable Content\r\n",
		"HTTP server early body response status mismatch"
	);
	testHttpServerContains(
		&Client,
		"Connection: close\r\n",
		"HTTP server early body response did not close"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\nstopped",
		"HTTP server early body response payload mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /header-hints HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	{
		char* pHints = strstr(
			Client.Response,
			"HTTP/1.1 103 Early Hints\r\n"
		);
		char* pFinal = strstr(
			Client.Response,
			"HTTP/1.1 200 OK\r\n"
		);

		testRequire(
			(pHints != NULL) &&
			(pFinal != NULL) &&
			(pHints < pFinal) &&
			(strstr(
				Client.Response,
				"Link: </header.css>; rel=preload\r\n"
			 ) != NULL) &&
			(strstr(
				Client.Response,
				"\r\n\r\nheader-hinted"
			 ) != NULL),
			"HTTP server Headers information response mismatch"
		);
	}
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /reject HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 4\r\n"
		"\r\n"
		"DROP",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 403 Forbidden\r\n",
		"HTTP server early rejection status mismatch"
	);
	testHttpServerContains(
		&Client,
		"Connection: close\r\n",
		"HTTP server early rejection did not close"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\n{\"code\":403,\"msg\":\"Forbidden\"}",
		"HTTP server early rejection body mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /buffer HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n"
		"\r\n"
		"DATA",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\nbuffered",
		"HTTP server buffered response mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /large-allowed HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 9\r\n"
		"Connection: close\r\n"
		"\r\n"
		"123456789",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\nlarge-allowed",
		"HTTP server route body limit response mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /stream HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 6\r\n"
		"Connection: close\r\n"
		"\r\n"
		"STREAM",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\nstreamed",
		"HTTP server streaming response mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /discard HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 7\r\n"
		"\r\n"
		"IGNORED"
		"GET /two HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	{
		char* pDiscarded = strstr(
			Client.Response, "\r\n\r\ndiscarded"
		);
		char* pTwo = strstr(Client.Response, "\r\n\r\ntwo");

		testRequire(
			(pDiscarded != NULL) &&
			(pTwo != NULL) &&
			(pDiscarded < pTwo),
			"HTTP server discarded pipeline response mismatch"
		);
	}
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /stream-fail HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: close\r\n"
		"\r\n"
		"4\r\n"
		"FAIL\r\n"
		"0\r\n"
		"\r\n",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 500 Internal Server Error\r\n",
		"HTTP server body callback failure status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /expect HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 4\r\n"
		"Expect: 100-continue\r\n"
		"Connection: close\r\n"
		"\r\n",
		"WAIT",
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 100 Continue\r\n\r\n",
		"HTTP server automatic 100 Continue missing"
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 200 OK\r\n",
		"HTTP server final Expect response missing"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\ncontinued",
		"HTTP server Expect final body mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /one HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"\r\n"
		"GET /two HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	{
		char* pOne = strstr(Client.Response, "\r\n\r\none");
		char* pTwo = strstr(Client.Response, "\r\n\r\ntwo");

		testRequire(
			(pOne != NULL) &&
			(pTwo != NULL) &&
			(pOne < pTwo),
			"HTTP server pipeline response order mismatch"
		);
	}
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /bad HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Broken Header\r\n"
		"\r\n",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 400 Bad Request\r\n",
		"HTTP server malformed request status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /stream HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: close\r\n"
		"\r\n"
		"Z\r\n"
		"bad\r\n",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 400 Bad Request\r\n",
		"HTTP server malformed chunk status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"POST /large HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Content-Length: 9\r\n"
		"Connection: close\r\n"
		"\r\n"
		"123456789",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 413 Content Too Large\r\n",
		"HTTP server body limit status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /timeout HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 504 Gateway Timeout\r\n",
		"HTTP server request timeout status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerRunClient(
		&State,
		&Address,
		"GET /",
		NULL,
		&Client
	);
	testHttpServerContains(
		&Client,
		"HTTP/1.1 408 Request Timeout\r\n",
		"HTTP server header timeout status mismatch"
	);
	testHttpServerClientDestroy(&Client);

	testHttpServerStartClient(
		&State,
		&Address,
		"GET /drain HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"\r\n",
		NULL,
		&Client
	);
	testHttpServerWait(
		&State.DrainReady,
		1,
		"HTTP server drain request callback missing"
	);
	testRequire(
		xrtHttpServerDrain(State.Server) &&
		(xrtHttpServerState(State.Server) ==
		 XHTTP_SERVER_DRAINING),
		"HTTP server did not enter draining state"
	);
	testHttpServerWait(
		&Client.Closed,
		1,
		"HTTP server drain response did not close"
	);
	testHttpServerContains(
		&Client,
		"\r\n\r\ndrained",
		"HTTP server drain response body mismatch"
	);
	testHttpServerClientDestroy(&Client);
	testHttpServerWait(
		&State.Shutdown,
		1,
		"HTTP server Shutdown callback missing"
	);
	testHttpServerWait(
		&State.Closed,
		20,
		"HTTP server Connection Close callbacks missing"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &Stats),
		"HTTP server final statistics query failed"
	);
	testRequire(
		(Stats.State == XHTTP_SERVER_CLOSED) &&
		(Stats.Accepted == 21) &&
		(Stats.Rejected == 0) &&
		(Stats.Requests == 16) &&
		(Stats.Responses == 24) &&
		(Stats.Informations == 5) &&
		(Stats.ProtocolErrors == 7) &&
		(Stats.Timeouts == 2) &&
		(Stats.Connections == 0) &&
		(Stats.PeakConnections >= 1) &&
		(xrtAtomic32Load(
			&State.Opened,
			XMEMORY_ACQUIRE
		 ) == 21) &&
		(xrtAtomic32Load(
			&State.Requested,
			XMEMORY_ACQUIRE
		 ) == 15) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 7),
		"HTTP server final statistics mismatch"
	);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP server engine could not stop"
	);
	testHttpServerKeepAliveHeaderTimeout();
	printf(
		"[PASS] HTTP server TCP runtime (%s)\n",
		TEST_HTTP_SERVER_BACKEND_NAME
	);
	return 0;
}
