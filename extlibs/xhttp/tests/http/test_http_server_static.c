#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 为静态服务测试创建拥有型请求快照。 */
static xhttpserverrequest* testHttpServerStaticRequest(
	cstr sWire
)
{
	xhttpfield Fields[16];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 16);
	if ( xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)sWire,
			strlen(sWire)
		},
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY ) {
		return NULL;
	}
	if ( !xrtHttp1RequestBodyPlan(
		&Head,
		&Plan
	) ) {
		return NULL;
	}
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
}



/* 在文件根内写入一个完整短文件。 */
static void testHttpServerStaticWrite(
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
		"HTTP static server fixture write failed"
	);
}



/* 等待异步正文当前片段准备完成。 */
static void testHttpServerStaticBodyWait(
	xhttpbodyreader* pReader
)
{
	xfuture* pFuture = xrtHttpBodyReaderWait(pReader);

	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static server body wait failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 用动态小窗口读取完整静态正文。 */
static size_t testHttpServerStaticBodyRead(
	xhttpbody* pBody,
	void* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	size_t iUsed = 0;
	size_t iLimit = 1;

	testRequire(
		pReader != NULL,
		"HTTP static server body open failed"
	);
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader,
			iLimit,
			&Chunk
		);

		if ( Status == XHTTP_BODY_AGAIN ) {
			testHttpServerStaticBodyWait(pReader);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			(Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= iLimit) &&
			(Chunk.Size <= (iCapacity - iUsed)),
			"HTTP static server body read failed"
		);
		memcpy(
			(bytes)pOutput + iUsed,
			Chunk.Data,
			Chunk.Size
		);
		iUsed += Chunk.Size;
		iLimit = (iLimit % 5u) + 1u;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iUsed;
}



/* 等待 Reply Future 并返回仍由 Future 保活的借用值。 */
static xhttpreply* testHttpServerStaticFutureValue(
	xfuture* pFuture
)
{
	xwaitresult Wait;
	xfuturestate State;
	const xerror* pError;

	if ( pFuture == NULL ) {
		fprintf(
			stderr,
			"[DIAG] HTTP static server Future is NULL: %s\n",
			xrtErrorMessage(xrtGetError())
		);
		testRequire(
			false,
			"HTTP static server Future failed"
		);
	}
	Wait = xrtFutureWaitFor(
		pFuture,
		UINT64_C(3000000)
	);
	State = xrtFutureState(pFuture);
	if ( (Wait != XWAIT_OK) ||
		(State != XFUTURE_RESOLVED) ) {
		pError = xrtFutureError(pFuture);
		fprintf(
			stderr,
			"[DIAG] HTTP static server Future: wait=%d state=%d "
			"domain=%s operation=%s message=%s\n",
			(int)Wait,
			(int)State,
			(pError != NULL) &&
				(xrtErrorDomain(pError) != NULL) ?
				xrtErrorDomain(pError) : "-",
			(pError != NULL) &&
				(xrtErrorOperation(pError) != NULL) ?
				xrtErrorOperation(pError) : "-",
			(pError != NULL) &&
				(xrtErrorMessage(pError) != NULL) ?
				xrtErrorMessage(pError) : "-"
		);
	}
	testRequire(
		(Wait == XWAIT_OK) &&
		(State == XFUTURE_RESOLVED),
		"HTTP static server Future failed"
	);
	return (xhttpreply*)xrtFutureValue(pFuture);
}



/* 验证静态服务默认索引在相邻操作后仍保持只读内容。 */
static void testHttpServerStaticDefaultIndex(
	const xhttpstaticserveconfig* pConfig,
	cstr sMessage
)
{
	testRequire(
		(pConfig != NULL) &&
		(pConfig->Indexes != NULL) &&
		(pConfig->IndexCount == 1u) &&
		(pConfig->Indexes[0].Size == 10u) &&
		(memcmp(
			pConfig->Indexes[0].Data,
			"index.html",
			10u
		) == 0) &&
		xrtPathIsSafeEntry(
			pConfig->Indexes[0],
			false
		),
		sMessage
	);
}



/* 比较 Reply 字段与固定文本。 */
static bool testHttpServerStaticHeaderEqual(
	const xhttpreply* pReply,
	cstr sName,
	cstr sValue
)
{
	const xhttpfield* pField = xrtHttpReplyHeader(
		pReply,
		(xstrview){ sName, strlen(sName) }
	);
	size_t iSize = strlen(sValue);

	return (pField != NULL) &&
		(pField->Value.Size == iSize) &&
		(memcmp(
			pField->Value.Data,
			sValue,
			iSize
		) == 0);
}



/* 验证普通 GET、MIME 推断和完整异步文件正文。 */
static void testHttpServerStaticGet(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET /asset.txt HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	xfuture* pFuture;
	xhttpreply* pReply;
	char Output[32];
	size_t iSize;

	testRequire(
		pRequest != NULL,
		"HTTP static server GET request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) == XHTTP_STATUS_OK) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Type",
			"text/plain; charset=utf-8"
		) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Length",
			"10"
		),
		"HTTP static server GET metadata failed"
	);
	iSize = testHttpServerStaticBodyRead(
		xrtHttpReplyBody(pReply),
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize == 10u) &&
		(memcmp(Output, "0123456789", 10) == 0),
		"HTTP static server GET body failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证默认目录索引和 HEAD 的无正文精确长度语义。 */
static void testHttpServerStaticHeadIndex(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"HEAD / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	xfuture* pFuture;
	xhttpreply* pReply;

	testRequire(
		pRequest != NULL,
		"HTTP static server HEAD request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) == XHTTP_STATUS_OK) &&
		(xrtHttpReplyBody(pReply) == NULL) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Type",
			"text/html; charset=utf-8"
		) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Length",
			"5"
		),
		"HTTP static server HEAD index failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证目录索引按顺序跳过不可见候选并返回首个可用文件。 */
static void testHttpServerStaticIndexFallback(
	xtaskpool* pPool,
	xroot Root
)
{
	static const xstrview Indexes[] = {
		XRT_STR_INIT("missing.html"),
		XRT_STR_INIT("index.html")
	};
	xhttpstaticserveconfig Config;
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	xfuture* pFuture;
	xhttpreply* pReply;
	char Output[16];
	size_t iSize;

	xrtHttpStaticServeConfigInit(&Config);
	Config.Indexes = Indexes;
	Config.IndexCount = sizeof(Indexes) / sizeof(Indexes[0]);
	testRequire(
		pRequest != NULL,
		"HTTP static server index fallback request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		&Config
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	iSize = testHttpServerStaticBodyRead(
		xrtHttpReplyBody(pReply),
		Output,
		sizeof(Output)
	);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) == XHTTP_STATUS_OK) &&
		(iSize == 5u) &&
		(memcmp(Output, "index", 5u) == 0),
		"HTTP static server index fallback failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证多范围响应由共享 canonical 编码器和流式 Body 共同产生。 */
static void testHttpServerStaticRange(
	xtaskpool* pPool,
	xroot Root
)
{
	static const char sExpected[] =
		"--parts\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Range: bytes 0-2/10\r\n"
		"\r\n"
		"012\r\n"
		"--parts\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Range: bytes 7-9/10\r\n"
		"\r\n"
		"789\r\n"
		"--parts--\r\n";
	xhttpstaticserveconfig Config;
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET /asset.txt HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Range: bytes=0-2,7-9\r\n\r\n"
		);
	xfuture* pFuture;
	xhttpreply* pReply;
	char Output[512];
	size_t iSize;
	char sLength[32];

	xrtHttpStaticServeConfigInit(&Config);
	Config.Reply.Boundary = XRT_STR_LITERAL("parts");
	testRequire(
		pRequest != NULL,
		"HTTP static server Range request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		&Config
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	iSize = testHttpServerStaticBodyRead(
		xrtHttpReplyBody(pReply),
		Output,
		sizeof(Output)
	);
	(void)snprintf(
		sLength,
		sizeof(sLength),
		"%u",
		(unsigned int)(sizeof(sExpected) - 1u)
	);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_PARTIAL_CONTENT) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Type",
			"multipart/byteranges; boundary=parts"
		) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Length",
			sLength
		) &&
		(iSize == (sizeof(sExpected) - 1u)) &&
		(memcmp(
			Output,
			sExpected,
			iSize
		) == 0),
		"HTTP static server multipart Range failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证默认多范围 boundary 来自安全随机源并同时用于字段和正文。 */
static void testHttpServerStaticAutoBoundary(
	xtaskpool* pPool,
	xroot Root
)
{
	static const char sPrefix[] =
		"multipart/byteranges; boundary=";
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET /asset.txt HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Range: bytes=0-2,7-9\r\n\r\n"
		);
	const xhttpfield* pType;
	xstrview Boundary;
	xfuture* pFuture;
	xhttpreply* pReply;
	char Output[512];
	size_t iSize;
	size_t i;

	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	pType = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pType != NULL) &&
		(pType->Value.Size ==
		 (sizeof(sPrefix) - 1u + 36u)) &&
		(memcmp(
			pType->Value.Data,
			sPrefix,
			sizeof(sPrefix) - 1u
		 ) == 0),
		"HTTP static server automatic boundary field failed"
	);
	Boundary.Data =
		pType->Value.Data + sizeof(sPrefix) - 1u;
	Boundary.Size = 36;
	testRequire(
		memcmp(Boundary.Data, "xrt-", 4) == 0,
		"HTTP static server automatic boundary prefix failed"
	);
	for ( i = 4; i < Boundary.Size; i++ ) {
		char iValue = Boundary.Data[i];

		testRequire(
			((iValue >= '0') && (iValue <= '9')) ||
			((iValue >= 'a') && (iValue <= 'f')),
			"HTTP static server automatic boundary hex failed"
		);
	}
	iSize = testHttpServerStaticBodyRead(
		xrtHttpReplyBody(pReply),
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize > (Boundary.Size * 2u + 12u)) &&
		(Output[0] == '-') &&
		(Output[1] == '-') &&
		(memcmp(
			Output + 2u,
			Boundary.Data,
			Boundary.Size
		 ) == 0) &&
		(memcmp(
			Output + iSize - Boundary.Size - 4u,
			Boundary.Data,
			Boundary.Size
		 ) == 0) &&
		(memcmp(
			Output + iSize - 4u,
			"--\r\n",
			4
		 ) == 0),
		"HTTP static server automatic boundary body failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证 ETag 条件请求直接生成无正文 304。 */
static void testHttpServerStaticConditional(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpserverrequest* pGet =
		testHttpServerStaticRequest(
			"GET /asset.txt HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	xhttpserverrequest* pConditional;
	const xhttpfield* pETag;
	xfuture* pFuture;
	xhttpreply* pReply;
	char Wire[512];
	int iWire;

	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pGet,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	pETag = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("ETag")
	);
	testRequire(
		pETag != NULL,
		"HTTP static server ETag missing"
	);
	iWire = snprintf(
		Wire,
		sizeof(Wire),
		"GET /asset.txt HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"If-None-Match: %.*s\r\n\r\n",
		(int)pETag->Value.Size,
		pETag->Value.Data
	);
	testRequire(
		(iWire > 0) &&
		((size_t)iWire < sizeof(Wire)),
		"HTTP static server conditional wire failed"
	);
	pConditional = testHttpServerStaticRequest(Wire);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pGet);
	testRequire(
		pConditional != NULL,
		"HTTP static server conditional request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pConditional,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	testRequire(
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_NOT_MODIFIED) &&
		(xrtHttpReplyBody(pReply) == NULL) &&
		(xrtHttpReplyHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length")
		 ) == NULL),
		"HTTP static server conditional response failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pConditional);
}



/* 验证正常未命中、安全拒绝和挂载未命中都返回 404 Reply。 */
static void testHttpServerStaticNotFound(
	xtaskpool* pPool,
	xroot Root
)
{
	static const char* sRequests[] = {
		"GET /missing.txt HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		"GET /bad%2fpath HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		"GET /other/file.txt HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n"
	};
	xhttpstaticserveconfig Config;
	size_t i;

	xrtHttpStaticServeConfigInit(&Config);
	Config.Path.Mount = XRT_STR_LITERAL("/");
	for ( i = 0; i < 2u; i++ ) {
		xhttpserverrequest* pRequest =
			testHttpServerStaticRequest(sRequests[i]);
		xfuture* pFuture = xrtHttpReplyStaticFuture(
			pPool,
			Root,
			pRequest,
			&Config
		);
		xhttpreply* pReply =
			testHttpServerStaticFutureValue(pFuture);

		testRequire(
			xrtHttpReplyStatus(pReply) ==
				XHTTP_STATUS_NOT_FOUND,
			"HTTP static server 404 mapping failed"
		);
		xrtFutureDestroy(pFuture);
		xrtHttpServerRequestDestroy(pRequest);
	}
	Config.Path.Mount = XRT_STR_LITERAL("/assets");
	{
		xhttpserverrequest* pRequest =
			testHttpServerStaticRequest(sRequests[2]);
		xfuture* pFuture = xrtHttpReplyStaticFuture(
			pPool,
			Root,
			pRequest,
			&Config
		);
		xhttpreply* pReply =
			testHttpServerStaticFutureValue(pFuture);

		testRequire(
			xrtHttpReplyStatus(pReply) ==
				XHTTP_STATUS_NOT_FOUND,
			"HTTP static server mount miss failed"
		);
		xrtFutureDestroy(pFuture);
		xrtHttpServerRequestDestroy(pRequest);
	}
}



/* 验证畸形 request-target 作为正常 HTTP 400 结果返回。 */
static void testHttpServerStaticBadTarget(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET /bad%ZZ HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	xfuture* pFuture;
	xhttpreply* pReply;

	testRequire(
		pRequest != NULL,
		"HTTP static server bad target request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		NULL
	);
	pReply = testHttpServerStaticFutureValue(pFuture);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_BAD_REQUEST),
		"HTTP static server bad target status failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证高层配置错误统一进入静态服务结构化错误域。 */
static void testHttpServerStaticConfigError(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpstaticserveconfig Config;
	xstrview InvalidIndex = { NULL, 1 };
	xhttpserverrequest* pRequest =
		testHttpServerStaticRequest(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n"
		);
	const xerror* pError;

	xrtHttpStaticServeConfigInit(&Config);
	Config.Indexes = &InvalidIndex;
	Config.IndexCount = 1u;
	testRequire(
		(pRequest != NULL) &&
		(xrtHttpReplyStaticFuture(
			pPool,
			Root,
			pRequest,
			&Config
		 ) == NULL),
		"HTTP static server invalid config was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"http.server.static"
		 ) == 0) &&
		(xrtErrorCode(pError) ==
		 XHTTP_SERVER_STATIC_ERROR_CONFIG) &&
		(xrtErrorOperation(pError) != NULL) &&
		(strcmp(
			xrtErrorOperation(pError),
			"config"
		 ) == 0),
		"HTTP static server config error mismatch"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证静态服务配置支持未对齐存储，并在异步任务启动前独立保留借用文本。 */
static void testHttpServerStaticConfigMemory(
	xtaskpool* pPool,
	xroot Root
)
{
	unsigned char ReplyStorage[
		sizeof(xhttpstaticreplyconfig) + 2u
	];
	unsigned char ServeStorage[
		sizeof(xhttpstaticserveconfig) + 2u
	];
	xhttpstaticreplyconfig ReplyConfig;
	xhttpstaticserveconfig ServeConfig;
	xhttpserverrequest* pRequest;
	xfuture* pFuture;
	xhttpreply* pReply;
	char sContentType[] = "application/x-xrt-static";
	char sCacheControl[] = "private, max-age=7";

	memset(ReplyStorage, 0xA5, sizeof(ReplyStorage));
	xrtHttpStaticReplyConfigInit(
		(xhttpstaticreplyconfig*)(void*)(ReplyStorage + 1u)
	);
	memcpy(
		&ReplyConfig,
		ReplyStorage + 1u,
		sizeof(ReplyConfig)
	);
	testRequire(
		(ReplyStorage[0] == 0xA5u) &&
		(ReplyStorage[sizeof(ReplyStorage) - 1u] == 0xA5u) &&
		(ReplyConfig.ContentType.Size == 0) &&
		(ReplyConfig.CacheControl.Size == 0) &&
		(ReplyConfig.Boundary.Size == 0),
		"HTTP static Reply config unaligned init failed"
	);

	memset(ServeStorage, 0x5A, sizeof(ServeStorage));
	xrtHttpStaticServeConfigInit(
		(xhttpstaticserveconfig*)(void*)(ServeStorage + 1u)
	);
	memcpy(
		&ServeConfig,
		ServeStorage + 1u,
		sizeof(ServeConfig)
	);
	testRequire(
		(ServeStorage[0] == 0x5Au) &&
		(ServeStorage[sizeof(ServeStorage) - 1u] == 0x5Au) &&
		(ServeConfig.IndexCount == 1u) &&
		(ServeConfig.Indexes != NULL) &&
		(ServeConfig.Indexes[0].Size == 10u) &&
		(memcmp(
			ServeConfig.Indexes[0].Data,
			"index.html",
			10
		 ) == 0),
		"HTTP static service config unaligned init failed"
	);
	ServeConfig.Reply.ContentType = (xstrview){
		sContentType,
		sizeof(sContentType) - 1u
	};
	ServeConfig.Reply.CacheControl = (xstrview){
		sCacheControl,
		sizeof(sCacheControl) - 1u
	};
	memcpy(
		ServeStorage + 1u,
		&ServeConfig,
		sizeof(ServeConfig)
	);
	pRequest = testHttpServerStaticRequest(
		"GET /asset.txt HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n"
	);
	testRequire(
		pRequest != NULL,
		"HTTP static service unaligned request failed"
	);
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		(const xhttpstaticserveconfig*)(const void*)(
			ServeStorage + 1u
		)
	);
	testRequire(
		(ServeStorage[0] == 0x5Au) &&
		(ServeStorage[sizeof(ServeStorage) - 1u] == 0x5Au),
		"HTTP static service config read crossed its range"
	);
	memset(sContentType, 'x', sizeof(sContentType) - 1u);
	memset(sCacheControl, 'y', sizeof(sCacheControl) - 1u);
	memset(ServeStorage + 1u, 0xCC, sizeof(ServeConfig));
	pReply = testHttpServerStaticFutureValue(pFuture);
	testRequire(
		(pReply != NULL) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Content-Type",
			"application/x-xrt-static"
		) &&
		testHttpServerStaticHeaderEqual(
			pReply,
			"Cache-Control",
			"private, max-age=7"
		),
		"HTTP static service did not retain config text"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpServerRequestDestroy(pRequest);

	xrtClearError();
	xrtHttpStaticReplyConfigInit(
		(xhttpstaticreplyconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP static Reply config accepted wrapping output"
	);
	xrtClearError();
	xrtHttpStaticServeConfigInit(
		(xhttpstaticserveconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP static service config accepted wrapping output"
	);
	xrtClearError();

	pRequest = testHttpServerStaticRequest(
		"GET /asset.txt HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n"
	);
	testRequire(
		(pRequest != NULL) &&
		(xrtHttpReplyStaticFuture(
			pPool,
			Root,
			pRequest,
			(const xhttpstaticserveconfig*)(uintptr_t)(
				UINTPTR_MAX - 1u
			)
		 ) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_STATIC_ERROR_CONFIG),
		"HTTP static service accepted wrapping config input"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
}



/* 未知长度正文测试源打开后直接结束。 */
static xhttpbodystatus testHttpServerStaticUnknownNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	return XHTTP_BODY_EOF;
}



/* 为未知长度测试源返回无状态 Reader。 */
static bool testHttpServerStaticUnknownOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpServerStaticUnknownNext;
	*ppReader = NULL;
	return true;
}



/* 创建用于验证静态桥长度契约的未知长度正文。 */
static xhttpbody* testHttpServerStaticUnknownBody(void)
{
	static const xhttpbodyops Ops = {
		testHttpServerStaticUnknownOpen,
		NULL
	};

	return xrtHttpBodyCreate(
		&Ops,
		NULL,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
}



/* 验证纯静态响应桥拒绝正文状态和长度不一致。 */
static void testHttpServerStaticBridge(void)
{
	unsigned char Storage[
		sizeof(xhttpstaticresponse) + 2u
	];
	xhttpstaticresponse Response;
	xhttpreply* pReply;
	xhttpbody* pBody;
	xhttpbody* pUnknown;

	memset(&Response, 0, sizeof(Response));
	Response.Status = XHTTP_STATUS_NO_CONTENT;
	pReply = xrtHttpReplyFromStatic(
		&Response,
		NULL
	);
	testRequire(
		(pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_NO_CONTENT),
		"HTTP static Reply bridge failed"
	);
	xrtHttpReplyDestroy(pReply);

	memset(Storage, 0xA5, sizeof(Storage));
	memcpy(Storage + 1u, &Response, sizeof(Response));
	pReply = xrtHttpReplyFromStatic(
		(const xhttpstaticresponse*)(const void*)(Storage + 1u),
		NULL
	);
	testRequire(
		(pReply != NULL) &&
		(Storage[0] == 0xA5u) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5u) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_NO_CONTENT),
		"HTTP static Reply bridge rejected unaligned input"
	);
	xrtHttpReplyDestroy(pReply);

	xrtClearError();
	testRequire(
		xrtHttpReplyFromStatic(
			(const xhttpstaticresponse*)(uintptr_t)(
				UINTPTR_MAX - 1u
			),
			NULL
		) == NULL,
		"HTTP static Reply bridge accepted wrapping input"
	);
	xrtClearError();
	Response.ContentType = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	testRequire(
		xrtHttpReplyFromStatic(&Response, NULL) == NULL,
		"HTTP static Reply bridge accepted wrapping field text"
	);
	xrtClearError();
	Response.ContentType = (xstrview){ NULL, 0 };
	Response.Fields[0].Name = XRT_STR_LITERAL("bad name");
	Response.Fields[0].Value = XRT_STR_LITERAL("value");
	Response.FieldCount = 1u;
	testRequire(
		(xrtHttpReplyFromStatic(&Response, NULL) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP static Reply bridge lost field syntax error"
	);
	xrtClearError();

	memset(&Response, 0, sizeof(Response));
	Response.Status = XHTTP_STATUS_OK;
	Response.SendBody = true;
	Response.BodyLength = 4;
	pBody = xrtHttpBodyCopy(
		(xbytesview){
			(cbytes)"abc",
			3
		}
	);
	testRequire(
		(pBody != NULL) &&
		(xrtHttpReplyFromStatic(
			&Response,
			NULL
		 ) == NULL) &&
		(xrtHttpReplyFromStatic(
			&Response,
			pBody
		 ) == NULL),
		"HTTP static Reply bridge mismatch accepted"
	);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	pUnknown = testHttpServerStaticUnknownBody();
	testRequire(
		(pUnknown != NULL) &&
		(xrtHttpReplyFromStatic(
			&Response,
			pUnknown
		 ) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP static Reply bridge accepted unknown body length"
	);
	xrtHttpBodyDestroy(pUnknown);
	xrtClearError();
}



/* 运行静态协议、文件资源、Future 和 Reply 的组合回归。 */
int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	xhttpstaticserveconfig ServeConfig;
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xtaskpool* pPool;
	int iSize;

	xrtHttpStaticServeConfigInit(&ServeConfig);
	testHttpServerStaticDefaultIndex(
		&ServeConfig,
		"HTTP static server default index failed"
	);

	iSize = snprintf(
		sDirectory,
		sizeof(sDirectory),
		".xrt-http-server-static-%lld",
		(long long)xrtNow()
	);
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(sDirectory)),
		"HTTP static server fixture name failed"
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static server parent root failed"
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
		"HTTP static server directory create failed"
	);
	Root = xrtRootOpenIn(
		Parent,
		sDirectory
	);
	testRequire(
		Root != NULL,
		"HTTP static server root open failed"
	);
	testHttpServerStaticWrite(
		Root,
		"asset.txt",
		"0123456789"
	);
	testHttpServerStaticWrite(
		Root,
		"index.html",
		"index"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static server task pool failed"
	);

	testHttpServerStaticGet(pPool, Root);
	testHttpServerStaticHeadIndex(pPool, Root);
	testHttpServerStaticIndexFallback(pPool, Root);
	testHttpServerStaticRange(pPool, Root);
	testHttpServerStaticAutoBoundary(pPool, Root);
	testHttpServerStaticConditional(pPool, Root);
	testHttpServerStaticNotFound(pPool, Root);
	testHttpServerStaticBadTarget(pPool, Root);
	testHttpServerStaticConfigError(pPool, Root);
	testHttpServerStaticConfigMemory(pPool, Root);
	testHttpServerStaticBridge();

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static server task pool cleanup failed"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootRemove(Root, "index.html") &&
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static server fixture cleanup failed"
	);
	printf("[PASS] http_server_static\n");
	return 0;
}
