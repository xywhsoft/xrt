#include "../test.h"



#ifndef TEST_HTTP_SERVER_RAW_BACKEND
	#define TEST_HTTP_SERVER_RAW_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_RAW_BACKEND_NAME
	#define TEST_HTTP_SERVER_RAW_BACKEND_NAME "select"
#endif



static const uint8 __g_TestHttpServerRawCopy[] =
	"HTTP/1.1 201 Created\r\n"
	"Content-Length: 4\r\n"
	"Content-Type: text/plain\r\n"
	"Connection: close\r\n"
	"\r\n"
	"copy";

static const uint8 __g_TestHttpServerRawReference[] =
	"HTTP/1.1 202 Accepted\r\n"
	"Content-Length: 3\r\n"
	"X-Mode: reference\r\n"
	"\r\n"
	"ref";

static const uint8 __g_TestHttpServerRawRefsHead[] =
	"HTTP/1.1 203 Non-Authoritative Information\r\n"
	"Content-Length: 4\r\n"
	"Connection: close\r\n"
	"\r\n";

static const uint8 __g_TestHttpServerRawRefsBody[] = "refs";

static const uint8 __g_TestHttpServerRawTake[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Length: 4\r\n"
	"Connection: close\r\n"
	"\r\n"
	"take";



/* 记录原始响应路径的请求、所有权释放与终态事件。 */
typedef struct test_http_server_raw {
	xatomic32 Requests;
	xatomic32 Releases;
	xatomic32 RefReleases;
	xatomic32 FailedReleases;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
} test_http_server_raw;



/* 最后一个引用型 Body 租约释放时归还原始报文。 */
static void testHttpServerRawRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pContext;

	testRequire(
		(pData != NULL) &&
		(iSize == sizeof(__g_TestHttpServerRawReference) - 1u),
		"HTTP server raw reference release mismatch"
	);
	xrtFree((ptr)pData);
	(void)xrtAtomic32FetchAdd(
		&pState->Releases,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录多引用线缆响应中每个非空片段的精确释放。 */
static void testHttpServerRawRefRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pContext;
	bool bHead = (pData == __g_TestHttpServerRawRefsHead) &&
		(iSize == sizeof(__g_TestHttpServerRawRefsHead) - 1u);
	bool bBody = (pData == __g_TestHttpServerRawRefsBody) &&
		(iSize == sizeof(__g_TestHttpServerRawRefsBody) - 1u);

	testRequire(
		bHead || bBody,
		"HTTP server raw refs release mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->RefReleases,
		1,
		XMEMORY_RELEASE
	);
}



/* 失败原子引用不能被服务器接管或释放。 */
static void testHttpServerRawFailedRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		&pState->FailedReleases,
		1,
		XMEMORY_RELEASE
	);
}



/* 比较请求 target 与一个完整 ASCII 路径。 */
static bool testHttpServerRawTarget(
	const xhttpserverrequest* pRequest,
	cstr sTarget
)
{
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	size_t iSize = strlen(sTarget);

	return (Target.Size == iSize) &&
		(memcmp(Target.Data, sTarget, iSize) == 0);
}



/* 分别提交复制报文、引用报文与普通 Reply。 */
static void testHttpServerRawRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pData;

	(void)pServer;
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_RELEASE
	);
	if ( testHttpServerRawTarget(pRequest, "/copy") ) {
		xnetref Rejected = {
			__g_TestHttpServerRawRefsBody,
			sizeof(__g_TestHttpServerRawRefsBody) - 1u,
			testHttpServerRawFailedRelease,
			pState
		};

		testRequire(
			(xrtHttpConnRespondRaw(
				pConnection,
				(xbytesview){
					(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
					4u
				},
				XHTTP_SERVER_RAW_NONE
			 ) == XNET_RESULT_ERROR) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XHTTP_SERVER_ERROR_ARGUMENT),
			"HTTP server raw response accepted a wrapping byte range"
		);
		xrtClearError();
		testRequire(
			xrtHttpConnRespondRaw(
				pConnection,
				(xbytesview){
					__g_TestHttpServerRawCopy,
					sizeof(__g_TestHttpServerRawCopy) - 1u
				},
				XHTTP_SERVER_RAW_NONE
			) == XNET_RESULT_OK,
			"HTTP server raw copy response failed"
		);
		testRequire(
			(xrtHttpConnRespondRawRef(
				pConnection,
				&Rejected,
				XHTTP_SERVER_RAW_NONE
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE),
			"HTTP server raw duplicate Ref response succeeded"
		);
		xrtClearError();
		return;
	}
	if ( testHttpServerRawTarget(pRequest, "/refs") ) {
		const xnetref Refs[] = {
			{
				__g_TestHttpServerRawRefsHead,
				sizeof(__g_TestHttpServerRawRefsHead) - 1u,
				testHttpServerRawRefRelease,
				pState
			},
			{ NULL, 0, NULL, NULL },
			{
				__g_TestHttpServerRawRefsBody,
				sizeof(__g_TestHttpServerRawRefsBody) - 1u,
				testHttpServerRawRefRelease,
				pState
			}
		};

		testRequire(
			xrtHttpConnRespondRawRefs(
				pConnection,
				Refs,
				sizeof(Refs) / sizeof(Refs[0]),
				XHTTP_SERVER_RAW_NONE
			) == XNET_RESULT_OK,
			"HTTP server raw refs response failed"
		);
		return;
	}
	if ( testHttpServerRawTarget(pRequest, "/take") ) {
		size_t iSize = sizeof(__g_TestHttpServerRawTake) - 1u;
		bytes pWire = (bytes)xrtMemDup(
			__g_TestHttpServerRawTake,
			iSize
		);

		testRequire(
			(pWire != NULL) &&
			(xrtHttpConnRespondRawTake(
				pConnection,
				pWire,
				iSize,
				XHTTP_SERVER_RAW_NONE
			 ) == XNET_RESULT_OK),
			"HTTP server raw take response failed"
		);
		return;
	}
	if ( testHttpServerRawTarget(pRequest, "/reference") ) {
		size_t iSize =
			sizeof(__g_TestHttpServerRawReference) - 1u;
		bytes pWire = (bytes)xrtMalloc(iSize);
		xhttpbody* pBody;

		testRequire(
			pWire != NULL,
			"HTTP server raw reference bytes allocation failed"
		);
		memcpy(
			pWire,
			__g_TestHttpServerRawReference,
			iSize
		);
		pBody = xrtHttpBodyReference(
			(xbytesview){ pWire, iSize },
			testHttpServerRawRelease,
			pState
		);
		testRequire(
			(pBody != NULL) &&
			(xrtHttpConnRespondRawBody(
				pConnection,
				pBody,
				XHTTP_SERVER_RAW_KEEP_ALIVE
			 ) == XNET_RESULT_OK),
			"HTTP server raw reference response failed"
		);
		xrtHttpBodyDestroy(pBody);
		return;
	}
	testRequire(
		testHttpServerRawTarget(pRequest, "/reply") &&
		(xrtHttpConnReply(
			pConnection,
			204,
			XRT_STR_LITERAL("text/plain"),
			(xbytesview){ NULL, 0 }
		 ) == XNET_RESULT_OK),
		"HTTP server normal Reply after raw keep-alive failed"
	);
}



/* 原始响应测试不应发布稳定连接错误。 */
static void testHttpServerRawError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 核对每条测试连接只发布一次正常关闭终态。 */
static void testHttpServerRawClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pData;

	(void)pServer;
	(void)pConnection;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"HTTP server raw connection close mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 已经排空所有原始响应连接。 */
static void testHttpServerRawShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_raw* pState =
		(test_http_server_raw*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTP server raw shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待一个 Worker 事件计数。 */
static void testHttpServerRawWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline =
		xrtDeadlineAfter(UINT64_C(10000000));

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



/* 完整发送一段请求字节，避免短写掩盖响应状态机。 */
static void testHttpServerRawSend(
	xnetsocket Socket,
	cbytes pData,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			(xrtNetSocketSend(
				Socket,
				pData + iOffset,
				iSize - iOffset,
				&iSent
			 ) == XNET_RESULT_OK) &&
			(iSent != 0),
			"HTTP server raw client send failed"
		);
		iOffset += iSent;
	}
}



/* 精确读取一条已知长度原始报文。 */
static void testHttpServerRawRead(
	xnetsocket Socket,
	bytes pOutput,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iRead = 0;

		testRequire(
			(xrtNetSocketRecv(
				Socket,
				pOutput + iOffset,
				iSize - iOffset,
				&iRead
			 ) == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server raw client receive failed"
		);
		iOffset += iRead;
	}
}



/* 建立一个阻塞式回环 Client。 */
static xnetsocket testHttpServerRawOpen(
	const xnetaddr* pAddress
)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket,
			pAddress
		 ) == XNET_RESULT_OK),
		"HTTP server raw client connect failed"
	);
	return Socket;
}



/* 验证复制、引用所有权、复用、普通 Reply 和极小写租约。 */
int main(void)
{
	static const uint8 CopyRequest[] =
		"GET /copy HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const uint8 ReferenceRequest[] =
		"GET /reference HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"\r\n";
	static const uint8 RefsRequest[] =
		"GET /refs HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const uint8 TakeRequest[] =
		"GET /take HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const uint8 ReplyRequest[] =
		"GET /reply HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	test_http_server_raw State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket CopyClient;
	xnetsocket RefsClient;
	xnetsocket TakeClient;
	xnetsocket KeepAliveClient;
	uint8 Buffer[256];
	char Reply[512];
	size_t iReply = 0;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Releases, 0);
	xrtAtomic32Init(&State.RefReleases, 0);
	xrtAtomic32Init(&State.FailedReleases, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_RAW_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server raw engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server raw address setup failed"
	);
	ServerConfig.WriteSize = 5;
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerRawRequest;
	Events.Error = testHttpServerRawError;
	Events.Close = testHttpServerRawClose;
	Events.Shutdown = testHttpServerRawShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server raw start failed"
	);

	CopyClient = testHttpServerRawOpen(&Address);
	testHttpServerRawSend(
		CopyClient,
		CopyRequest,
		sizeof(CopyRequest) - 1u
	);
	testHttpServerRawRead(
		CopyClient,
		Buffer,
		sizeof(__g_TestHttpServerRawCopy) - 1u
	);
	testRequire(
		memcmp(
			Buffer,
			__g_TestHttpServerRawCopy,
			sizeof(__g_TestHttpServerRawCopy) - 1u
		) == 0,
		"HTTP server raw copied wire bytes mismatch"
	);
	{
		size_t iRead = 0;

		testRequire(
			xrtNetSocketRecv(
				CopyClient,
				Buffer,
				1,
				&iRead
			) == XNET_RESULT_CLOSED,
			"HTTP server raw default response kept connection alive"
		);
	}
	testRequire(
		xrtNetSocketClose(CopyClient),
		"HTTP server raw copied client close failed"
	);

	RefsClient = testHttpServerRawOpen(&Address);
	testHttpServerRawSend(
		RefsClient,
		RefsRequest,
		sizeof(RefsRequest) - 1u
	);
	testHttpServerRawRead(
		RefsClient,
		Buffer,
		(sizeof(__g_TestHttpServerRawRefsHead) - 1u) +
			(sizeof(__g_TestHttpServerRawRefsBody) - 1u)
	);
	testRequire(
		(memcmp(
			Buffer,
			__g_TestHttpServerRawRefsHead,
			sizeof(__g_TestHttpServerRawRefsHead) - 1u
		 ) == 0) &&
		(memcmp(
			Buffer + sizeof(__g_TestHttpServerRawRefsHead) - 1u,
			__g_TestHttpServerRawRefsBody,
			sizeof(__g_TestHttpServerRawRefsBody) - 1u
		 ) == 0),
		"HTTP server raw refs wire bytes mismatch"
	);
	testHttpServerRawWait(
		&State.RefReleases,
		2,
		"HTTP server raw refs were not released"
	);
	testRequire(
		xrtNetSocketClose(RefsClient),
		"HTTP server raw refs client close failed"
	);

	TakeClient = testHttpServerRawOpen(&Address);
	testHttpServerRawSend(
		TakeClient,
		TakeRequest,
		sizeof(TakeRequest) - 1u
	);
	testHttpServerRawRead(
		TakeClient,
		Buffer,
		sizeof(__g_TestHttpServerRawTake) - 1u
	);
	testRequire(
		memcmp(
			Buffer,
			__g_TestHttpServerRawTake,
			sizeof(__g_TestHttpServerRawTake) - 1u
		) == 0,
		"HTTP server raw take wire bytes mismatch"
	);
	testRequire(
		xrtNetSocketClose(TakeClient),
		"HTTP server raw take client close failed"
	);

	KeepAliveClient = testHttpServerRawOpen(&Address);
	testHttpServerRawSend(
		KeepAliveClient,
		ReferenceRequest,
		sizeof(ReferenceRequest) - 1u
	);
	testHttpServerRawRead(
		KeepAliveClient,
		Buffer,
		sizeof(__g_TestHttpServerRawReference) - 1u
	);
	testRequire(
		memcmp(
			Buffer,
			__g_TestHttpServerRawReference,
			sizeof(__g_TestHttpServerRawReference) - 1u
		) == 0,
		"HTTP server raw referenced wire bytes mismatch"
	);
	testHttpServerRawWait(
		&State.Releases,
		1,
		"HTTP server raw referenced Body was not released"
	);
	testHttpServerRawSend(
		KeepAliveClient,
		ReplyRequest,
		sizeof(ReplyRequest) - 1u
	);
	while ( iReply < (sizeof(Reply) - 1u) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			KeepAliveClient,
			Reply + iReply,
			sizeof(Reply) - iReply - 1u,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server Reply after raw receive failed"
		);
		iReply += iRead;
	}
	Reply[iReply] = '\0';
	testRequire(
		(strstr(
			Reply,
			"HTTP/1.1 204 No Content\r\n"
		 ) != NULL) &&
		(strstr(Reply, "\r\n\r\n") != NULL),
		"HTTP server Reply after raw keep-alive mismatch"
	);
	testRequire(
		xrtNetSocketClose(KeepAliveClient),
		"HTTP server raw keep-alive client close failed"
	);

	testHttpServerRawWait(
		&State.Requests,
		5,
		"HTTP server raw request count mismatch"
	);
	testHttpServerRawWait(
		&State.Closed,
		4,
		"HTTP server raw connection close count mismatch"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server raw drain failed"
	);
	testHttpServerRawWait(
		&State.Shutdown,
		1,
		"HTTP server raw shutdown missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 4) &&
		(Stats.Requests == 5) &&
		(Stats.Responses == 5) &&
		(Stats.ProtocolErrors == 0) &&
		(Stats.Timeouts == 0) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&State.FailedReleases,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server raw statistics mismatch"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server raw engine destroy failed"
	);
	printf(
		"[PASS] HTTP server raw responses (%s)\n",
		TEST_HTTP_SERVER_RAW_BACKEND_NAME
	);
	return 0;
}
