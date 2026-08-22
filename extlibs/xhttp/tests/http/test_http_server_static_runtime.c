#include "../test.h"



/* 端到端静态服务状态由 Server 事件表借用。 */
typedef struct test_http_server_static_runtime {
	xtaskpool* Pool;
	xroot Root;
	xatomic32 Requests;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
} test_http_server_static_runtime;



/* 等待一个单调计数达到期望值。 */
static void testHttpServerStaticRuntimeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(3000000)
	);

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



/* 在安全文件根内写入固定短文件。 */
static void testHttpServerStaticRuntimeWrite(
	xroot Root,
	cstr sPath,
	cstr sText
)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		sPath,
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			sText,
			strlen(sText),
			NULL
		) &&
		xrtClose(File),
		"HTTP static runtime fixture write failed"
	);
}



/* 在 Connection Worker 上绑定一站式静态最终响应。 */
static void testHttpServerStaticRuntimeRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_static_runtime* pState =
		(test_http_server_static_runtime*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		xrtHttpConnStatic(
			pConnection,
			pState->Pool,
			pState->Root,
			NULL
		),
		"HTTP static runtime response binding failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录静态服务链路中不应出现的运行时错误。 */
static void testHttpServerStaticRuntimeError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_static_runtime* pState =
		(test_http_server_static_runtime*)pData;

	(void)pServer;
	(void)pConnection;
	fprintf(
		stderr,
		"[HTTP static runtime error] %s/%s: %s\n",
		pError != NULL ?
			xrtErrorDomain(pError) : "(null)",
		pError != NULL ?
			xrtErrorOperation(pError) : "(null)",
		pError != NULL ?
			xrtErrorMessage(pError) : "(null)"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Connection 已经完成关闭。 */
static void testHttpServerStaticRuntimeClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_static_runtime* pState =
		(test_http_server_static_runtime*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 已经完成排空。 */
static void testHttpServerStaticRuntimeShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_static_runtime* pState =
		(test_http_server_static_runtime*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTP static runtime shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 完整发送带可选额外 Header 的短 HTTP 请求。 */
static void testHttpServerStaticRuntimeSend(
	xnetsocket Socket,
	cstr sPath,
	cstr sHeaders
)
{
	char Request[512];
	int iSize = snprintf(
		Request,
		sizeof(Request),
		"GET %s HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"%s"
		"Connection: close\r\n"
		"\r\n",
		sPath,
		sHeaders != NULL ? sHeaders : ""
	);
	size_t iOffset = 0;

	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(Request)),
		"HTTP static runtime request format failed"
	);
	while ( iOffset < (size_t)iSize ) {
		size_t iSent = 0;

		testRequire(
			(xrtNetSocketSend(
				Socket,
				Request + iOffset,
				(size_t)iSize - iOffset,
				&iSent
			 ) == XNET_RESULT_OK) &&
			(iSent != 0),
			"HTTP static runtime request send failed"
		);
		iOffset += iSent;
	}
}



/* 连接测试 Server 并读取到服务端关闭。 */
static size_t testHttpServerStaticRuntimeExchange(
	const xnetaddr* pAddress,
	cstr sPath,
	cstr sHeaders,
	char* pResponse,
	size_t iCapacity
)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	size_t iOffset = 0;

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket,
			pAddress
		 ) == XNET_RESULT_OK),
		"HTTP static runtime client connect failed"
	);
	testHttpServerStaticRuntimeSend(
		Socket,
		sPath,
		sHeaders
	);
	while ( iOffset < (iCapacity - 1u) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			pResponse + iOffset,
			iCapacity - iOffset - 1u,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP static runtime response receive failed"
		);
		iOffset += iRead;
	}
	pResponse[iOffset] = '\0';
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP static runtime client close failed"
	);
	return iOffset;
}



/* 运行 Connection Worker、异步文件 Body 和真实 TCP 线缆回归。 */
int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	test_http_server_static_runtime State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	char sDirectory[96];
	char Response[4096];
	xroot Parent;
	int iSize;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	iSize = snprintf(
		sDirectory,
		sizeof(sDirectory),
		".xrt-http-server-static-runtime-%lld",
		(long long)xrtNow()
	);
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(sDirectory)),
		"HTTP static runtime fixture name failed"
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static runtime parent root failed"
	);
	if ( !xrtRootRemove(
		Parent,
		sDirectory
	) ) {
		xrtClearError();
	}
	testRequire(
		xrtRootDirCreate(
			Parent,
			sDirectory,
			0700u
		),
		"HTTP static runtime directory create failed"
	);
	State.Root = xrtRootOpenIn(
		Parent,
		sDirectory
	);
	testRequire(
		State.Root != NULL,
		"HTTP static runtime root open failed"
	);
	testHttpServerStaticRuntimeWrite(
		State.Root,
		"asset.txt",
		"0123456789"
	);
	State.Pool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		State.Pool != NULL,
		"HTTP static runtime pool create failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	#ifdef TEST_HTTP_SERVER_STATIC_BACKEND
	EngineConfig.Backend =
		TEST_HTTP_SERVER_STATIC_BACKEND;
	#else
	EngineConfig.Backend = XNET_PORT_SELECT;
	#endif
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP static runtime engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP static runtime address setup failed"
	);
	ServerConfig.RequestTimeout =
		UINT64_C(5000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request =
		testHttpServerStaticRuntimeRequest;
	Events.Error =
		testHttpServerStaticRuntimeError;
	Events.Close =
		testHttpServerStaticRuntimeClose;
	Events.Shutdown =
		testHttpServerStaticRuntimeShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP static runtime server start failed"
	);

	testRequire(
		(testHttpServerStaticRuntimeExchange(
			&Address,
			"/asset.txt",
			NULL,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 200 OK\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Type: text/plain; charset=utf-8\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Length: 10\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\n0123456789"
		 ) != NULL),
		"HTTP static runtime full response mismatch"
	);
	testHttpServerStaticRuntimeWait(
		&State.Closed,
		1,
		"HTTP static runtime full connection did not close"
	);

	testRequire(
		(testHttpServerStaticRuntimeExchange(
			&Address,
			"/asset.txt",
			"Range: bytes=2-5\r\n",
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 206 Partial Content\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Range: bytes 2-5/10\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Length: 4\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\n2345") != NULL),
		"HTTP static runtime Range response mismatch"
	);
	testHttpServerStaticRuntimeWait(
		&State.Closed,
		2,
		"HTTP static runtime Range connection did not close"
	);

	testRequire(
		(testHttpServerStaticRuntimeExchange(
			&Address,
			"/missing.txt",
			NULL,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 404 Not Found\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Length: 0\r\n"
		 ) != NULL),
		"HTTP static runtime 404 response mismatch"
	);
	testHttpServerStaticRuntimeWait(
		&State.Requests,
		3,
		"HTTP static runtime request count mismatch"
	);
	testHttpServerStaticRuntimeWait(
		&State.Closed,
		3,
		"HTTP static runtime 404 connection did not close"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		) == 0,
		"HTTP static runtime reported an error"
	);

	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP static runtime drain failed"
	);
	testHttpServerStaticRuntimeWait(
		&State.Shutdown,
		1,
		"HTTP static runtime shutdown missing"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtTaskPoolDestroy(State.Pool),
		"HTTP static runtime pool destroy failed"
	);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP static runtime engine destroy failed"
	);
	testRequire(
		xrtRootRemove(State.Root, "asset.txt") &&
		xrtRootClose(State.Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static runtime fixture cleanup failed"
	);
	printf("[PASS] http_server_static_runtime\n");
	return 0;
}
