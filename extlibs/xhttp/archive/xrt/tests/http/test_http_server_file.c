#include "../test.h"



typedef struct test_http_server_file {
	xtaskpool* Pool;
	cstr Path;
	xatomic32 Requested;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
} test_http_server_file;



/* 独占工作线程，确保文件准备任务仍停留在队列中。 */
typedef struct test_http_server_file_block {
	xatomic32 Started;
	xatomic32 Release;
} test_http_server_file_block;



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerFileWaitCount(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
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



/* 在系统临时目录构造文件响应测试路径。 */
static str testHttpServerFilePath(void)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(
		sDirectory != NULL,
		"HTTP server file temp directory failed"
	);
	sPath = xrtPathJoin(
		sDirectory,
		"xrt-http-server-file-\xE8\xBE\xB9\xE7\x95\x8C.tmp"
	);
	xrtFree(sDirectory);
	testRequire(
		sPath != NULL,
		"HTTP server file temp path failed"
	);
	return sPath;
}



/* 写入固定文件响应内容。 */
static void testHttpServerFileWrite(cstr sPath)
{
	xfile File;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE |
		XFILE_EXCLUSIVE
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			"0123456789",
			10,
			NULL
		) &&
		xrtClose(File),
		"HTTP server file fixture write failed"
	);
}



/* 等待 Reply Future 成功并返回其借用值。 */
static xhttpreply* testHttpServerFileReply(
	xfuture* pFuture,
	cstr sMessage
)
{
	testRequire(pFuture != NULL, sMessage);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		sMessage
	);
	return (xhttpreply*)xrtFutureValue(pFuture);
}



/* 读取异步 Body 的全部字节，验证 Reply Builder 没有复制正文实现。 */
static size_t testHttpServerFileBodyRead(
	xhttpbody* pBody,
	bytes pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	size_t iUsed = 0;

	testRequire(
		pReader != NULL,
		"HTTP server file body reader open failed"
	);
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader,
			3,
			&Chunk
		);

		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pWait =
				xrtHttpBodyReaderWait(pReader);

			testRequire(
				(pWait != NULL) &&
				(xrtFutureWaitFor(
					pWait,
					UINT64_C(2000000)
				 ) == XWAIT_OK),
				"HTTP server file body wait failed"
			);
			xrtFutureDestroy(pWait);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			(Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= (iCapacity - iUsed)),
			"HTTP server file body read failed"
		);
		memcpy(
			pOutput + iUsed,
			Chunk.Data,
			Chunk.Size
		);
		iUsed += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iUsed;
}



/* 在唯一工作线程中等待测试端释放。 */
static xtaskoutcome testHttpServerFileBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	test_http_server_file_block* pBlock =
		(test_http_server_file_block*)pData;

	(void)pCancel;
	(void)pResult;
	xrtAtomic32Store(
		&pBlock->Started,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pBlock->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	return XTASK_SUCCESS;
}



/*
	验证 Reply Future 的取消会传播到尚未执行的文件任务。
	任务池统计必须把文件准备记为取消，不能在工作线程释放后继续打开文件。
*/
static void testHttpServerFileCancellation(cstr sPath)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	test_http_server_file_block Block;
	xdeadline Deadline;
	xtaskpoolstats Stats;
	xtaskpool* pPool;
	xfuture* pBlockFuture;
	xfuture* pReplyFuture;

	xrtAtomic32Init(&Block.Started, 0);
	xrtAtomic32Init(&Block.Release, 0);
	memset(&Stats, 0, sizeof(Stats));
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(
		pPool != NULL,
		"HTTP server file cancellation pool failed"
	);
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testHttpServerFileBlockTask,
		&Block,
		NULL
	);
	testRequire(
		pBlockFuture != NULL,
		"HTTP server file cancellation blocker submit failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(2000000));
	while ( xrtAtomic32Load(
		&Block.Started,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server file cancellation blocker did not start"
		);
		xrtThreadYield();
	}
	pReplyFuture = xrtHttpReplyFileFuture(
		pPool,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain"),
		sPath
	);
	testRequire(
		(pReplyFuture != NULL) &&
		xrtFutureCancel(pReplyFuture),
		"HTTP server file cancellation request failed"
	);
	xrtAtomic32Store(
		&Block.Release,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtFutureWaitFor(
			pBlockFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pBlockFuture) ==
		 XFUTURE_RESOLVED) &&
		(xrtFutureWaitFor(
			pReplyFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pReplyFuture) ==
		 XFUTURE_CANCELLED) &&
		xrtTaskPoolClose(pPool) &&
		(xrtTaskPoolWaitFor(
			pPool,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		xrtTaskPoolGet(pPool, &Stats) &&
		(Stats.Submitted == 2) &&
		(Stats.Succeeded == 1) &&
		(Stats.Cancelled == 1),
		"HTTP server file cancellation blocker release failed"
	);
	xrtFutureDestroy(pReplyFuture);
	xrtFutureDestroy(pBlockFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP server file cancellation pool destroy failed"
	);
}



/* 验证独立 Reply Future 的状态、字段、正文、区间和错误透传。 */
static void testHttpServerFileBuilder(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfuture* pFuture = xrtHttpReplyFileFuture(
		pPool,
		203,
		XRT_STR_LITERAL("application/x-xrt-test"),
		sPath
	);
	xhttpreply* pReply = testHttpServerFileReply(
		pFuture,
		"HTTP server full file Reply failed"
	);
	const xhttpfield* pType = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Content-Type")
	);
	unsigned char arrBody[16];
	size_t iSize;

	testRequire(
		(xrtHttpReplyStatus(pReply) == 203) &&
		(pType != NULL) &&
		(pType->Value.Size == 22) &&
		(memcmp(
			pType->Value.Data,
			"application/x-xrt-test",
			22
		 ) == 0) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		 ) == 10),
		"HTTP server full file Reply metadata mismatch"
	);
	iSize = testHttpServerFileBodyRead(
		xrtHttpReplyBody(pReply),
		arrBody,
		sizeof(arrBody)
	);
	testRequire(
		(iSize == 10) &&
		(memcmp(arrBody, "0123456789", 10) == 0),
		"HTTP server full file Reply body mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtHttpReplyFileRangeFuture(
		pPool,
		206,
		XRT_STR_LITERAL("text/plain"),
		sPath,
		2,
		4
	);
	pReply = testHttpServerFileReply(
		pFuture,
		"HTTP server range file Reply failed"
	);
	iSize = testHttpServerFileBodyRead(
		xrtHttpReplyBody(pReply),
		arrBody,
		sizeof(arrBody)
	);
	testRequire(
		(xrtHttpReplyStatus(pReply) == 206) &&
		(iSize == 4) &&
		(memcmp(arrBody, "2345", 4) == 0),
		"HTTP server range file Reply mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtHttpReplyFileFuture(
		pPool,
		200,
		(xstrview){ NULL, 0 },
		"xrt-http-server-file-missing.tmp"
	);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED) &&
		(strcmp(
			xrtErrorDomain(
				xrtFutureError(pFuture)
			),
			"http.body.file"
		 ) == 0),
		"HTTP server file Reply error was not preserved"
	);
	xrtFutureDestroy(pFuture);
}



/* 在 Connection Worker 上选择完整文件或严格区间 Helper。 */
static void testHttpServerFileRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_file* pState =
		(test_http_server_file*)pData;
	xstrview Target =
		xrtHttpServerRequestTarget(pRequest);
	bool bResult;

	(void)pServer;
	if ( (Target.Size == 6) &&
		(memcmp(Target.Data, "/range", 6) == 0) ) {
		bResult = xrtHttpConnFileRange(
			pConnection,
			pState->Pool,
			206,
			XRT_STR_LITERAL("text/plain"),
			pState->Path,
			2,
			4
		);
	} else {
		bResult = xrtHttpConnFile(
			pConnection,
			pState->Pool,
			200,
			XRT_STR_LITERAL(
				"application/octet-stream"
			),
			pState->Path
		);
	}
	testRequire(
		bResult,
		"HTTP server file response binding failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录文件响应链路中不应出现的运行时错误。 */
static void testHttpServerFileError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_file* pState =
		(test_http_server_file*)pData;

	(void)pServer;
	(void)pConnection;
	fprintf(
		stderr,
		"[HTTP server file error] %s/%s: %s\n",
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



/* 记录文件响应连接终态。 */
static void testHttpServerFileClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_file* pState =
		(test_http_server_file*)pData;

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



/* 记录 HTTP Server 排空终态。 */
static void testHttpServerFileShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_file* pState =
		(test_http_server_file*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server file shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 完整发送一个短 HTTP 请求。 */
static void testHttpServerFileSend(
	xnetsocket Socket,
	cstr sPath
)
{
	char Request[256];
	int iSize = snprintf(
		Request,
		sizeof(Request),
		"GET %s HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		sPath
	);
	size_t iOffset = 0;

	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(Request)),
		"HTTP server file request format failed"
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
			"HTTP server file request send failed"
		);
		iOffset += iSent;
	}
}



/* 连接测试 Server，发送请求并读取到服务端关闭。 */
static size_t testHttpServerFileExchange(
	const xnetaddr* pAddress,
	cstr sPath,
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
		"HTTP server file client connect failed"
	);
	testHttpServerFileSend(Socket, sPath);
	while ( iOffset < (iCapacity - 1) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			pResponse + iOffset,
			iCapacity - iOffset - 1,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		if ( (Result != XNET_RESULT_OK) ||
			(iRead == 0) ) {
			fprintf(
				stderr,
				"[HTTP server file receive] result=%d read=%zu error=%s\n",
				(int)Result,
				iRead,
				xrtGetError() != NULL ?
					xrtErrorMessage(
						xrtGetError()
					) : "(null)"
			);
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server file response receive failed"
		);
		iOffset += iRead;
	}
	pResponse[iOffset] = '\0';
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP server file client close failed"
	);
	return iOffset;
}



/* HTTP 文件 Reply 与连接 Helper 回归入口。 */
int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	test_http_server_file State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	str sPath = testHttpServerFilePath();
	char Response[4096];

	memset(&State, 0, sizeof(State));
	State.Path = sPath;
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	testHttpServerFileWrite(sPath);
	State.Pool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		State.Pool != NULL,
		"HTTP server file task pool create failed"
	);
	testHttpServerFileBuilder(State.Pool, sPath);
	testHttpServerFileCancellation(sPath);

	xrtNetEngineConfigInit(&EngineConfig);
	#ifdef TEST_HTTP_SERVER_BACKEND
	EngineConfig.Backend = TEST_HTTP_SERVER_BACKEND;
	#else
	EngineConfig.Backend = XNET_PORT_SELECT;
	#endif
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server file engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server file address setup failed"
	);
	ServerConfig.RequestTimeout =
		UINT64_C(5000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerFileRequest;
	Events.Error = testHttpServerFileError;
	Events.Close = testHttpServerFileClose;
	Events.Shutdown = testHttpServerFileShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server file start failed"
	);

	testRequire(
		(testHttpServerFileExchange(
			&Address,
			"/file",
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 200 OK\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Length: 10\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\n0123456789"
		 ) != NULL),
		"HTTP server full file response mismatch"
	);
	testHttpServerFileWaitCount(
		&State.Requested,
		1,
		"HTTP server full file request missing"
	);
	testHttpServerFileWaitCount(
		&State.Closed,
		1,
		"HTTP server full file connection did not close"
	);

	testRequire(
		(testHttpServerFileExchange(
			&Address,
			"/range",
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 206 Partial Content\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nContent-Length: 4\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\n2345") != NULL),
		"HTTP server range file response mismatch"
	);
	testHttpServerFileWaitCount(
		&State.Requested,
		2,
		"HTTP server range file request missing"
	);
	testHttpServerFileWaitCount(
		&State.Closed,
		2,
		"HTTP server range file connection did not close"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		) == 0,
		"HTTP server file runtime reported an error"
	);

	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server file drain failed"
	);
	testHttpServerFileWaitCount(
		&State.Shutdown,
		1,
		"HTTP server file shutdown missing"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtTaskPoolDestroy(State.Pool),
		"HTTP server file task pool destroy failed"
	);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server file engine destroy failed"
	);
	testRequire(
		xrtFileDelete(sPath),
		"HTTP server file fixture cleanup failed"
	);
	xrtFree(sPath);
	printf("[PASS] HTTP server file response\n");
	return 0;
}
