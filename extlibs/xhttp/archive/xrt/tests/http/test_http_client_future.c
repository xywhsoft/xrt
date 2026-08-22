#include "../test.h"
#include "../fixtures/http_origin.h"



#if !defined(TEST_HTTP_CLIENT_FUTURE_BACKEND)
	#define TEST_HTTP_CLIENT_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_FUTURE_BACKEND_NAME "select"
#endif



/* 为 Future 用例创建只使用 IPv4 的高层 Client，排除双栈竞速噪声。 */
static xhttpclient* testHttpFutureClient(xnetengine* pEngine)
{
	xhttpclientconfig Config;
	xhttpclient* pClient;

	xrtHttpClientConfigInit(&Config);
	Config.Dial.Family = XNET_FAMILY_IPV4;
	Config.Dial.MaxAttempts = 1;
	Config.Timeout = UINT64_C(5000000);
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient != NULL,
		"HTTP Future client create failed"
	);
	return pClient;
}



/* 为指定 origin 路径创建一条拥有型 GET 请求。 */
static xhttprequest* testHttpFutureRequest(
	const testhttporigin* pOrigin,
	cstr sPath
)
{
	char Url[256];
	int iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u%s",
		(unsigned)testHttpOriginPort(pOrigin),
		sPath
	);
	xhttprequest* pRequest;

	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP Future URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP Future request create failed"
	);
	return pRequest;
}



/* 创建声明 xrt-test Upgrade 的拥有型请求。 */
static xhttprequest* testHttpFutureUpgradeRequest(
	const testhttporigin* pOrigin,
	cstr sPath
)
{
	xhttprequest* pRequest = testHttpFutureRequest(
		pOrigin,
		sPath
	);

	testRequire(
		xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("Upgrade")
		) &&
		xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("xrt-test")
		),
		"HTTP Future Upgrade headers failed"
	);
	return pRequest;
}



/* 验证 Future 成功值、结果引用和响应所有权取出。 */
static void testHttpFutureSuccess(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xhttpresult* pResult;
	xhttpresult* pRetained;
	xhttpresponse* pResponse;
	uint8 OptionsStorage[sizeof(xhttpcalloptions) + 2u];
	uint8 InfoStorage[sizeof(xhttpcallinfo) + 2u];
	xhttpcallinfo Info;

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(&Origin, "/future");
	xrtClearError();
	testRequire(
		(xrtHttpClientDoAsync(
			pClient,
			pRequest,
			(const xhttpcalloptions*)(uintptr_t)(
				UINTPTR_MAX - 1u
			)
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Future accepted a wrapping options range"
	);
	xrtClearError();
	memset(OptionsStorage, 0xA5, sizeof(OptionsStorage));
	xrtHttpCallOptionsInit(
		(xhttpcalloptions*)(void*)(OptionsStorage + 1u)
	);
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		(const xhttpcalloptions*)(const void*)(
			OptionsStorage + 1u
		)
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pFuture != NULL,
		"HTTP Future submission failed"
	);
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK,
		"HTTP Future success wait failed"
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"HTTP Future success state mismatch"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	memset(InfoStorage, 0xA5, sizeof(InfoStorage));
	testRequire(
		(pResult != NULL) &&
		!xrtHttpResultUpgraded(pResult) &&
		(xrtHttpResultTcp(pResult) == NULL) &&
		(xrtHttpResultBuffered(pResult) == 0) &&
		(xrtHttpResultRedirects(pResult) == 0) &&
		xrtHttpResultInfo(
			pResult,
			(xhttpcallinfo*)(void*)(InfoStorage + 1u)
		) &&
		(InfoStorage[0] == 0xA5) &&
		(InfoStorage[sizeof(InfoStorage) - 1u] == 0xA5),
		"HTTP Future result unaligned info storage mismatch"
	);
	memcpy(&Info, InfoStorage + 1u, sizeof(Info));
	testRequire(
		(Info.State == XHTTP_CALL_SUCCEEDED) &&
		(Info.Phase == XHTTP_CALL_PHASE_RESPONSE_BODY) &&
		(Info.Result == XNET_RESULT_OK) &&
		(Info.Error == XHTTP_CLIENT_ERROR_NONE) &&
		(Info.RequestWireBytes != 0) &&
		(Info.ResponseWireBytes != 0) &&
		(Info.ResponseBodyBytes == 2u) &&
		(Info.Completed >= Info.LastProgress),
		"HTTP Future result metadata mismatch"
	);
	testRequire(
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 200) &&
		(xrtHttpResponseBody(
			xrtHttpResultResponse(pResult)
		).Size == 2u),
		"HTTP Future response mismatch"
	);

	pRetained = xrtHttpResultRef(pResult);
	xrtFutureDestroy(pFuture);
	testRequire(
		pRetained != NULL,
		"HTTP Future result retain failed"
	);
	pResponse = xrtHttpResultTakeResponse(pRetained);
	testRequire(
		(pResponse != NULL) &&
		(xrtHttpResultResponse(pRetained) == NULL) &&
		(memcmp(
			xrtHttpResponseBody(pResponse).Data,
			"OK",
			2u
		) == 0),
		"HTTP Future response ownership transfer failed"
	);
	xrtHttpResultDestroy(pRetained);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 记录 Future 调用通过 Body 回调流式交付的最终响应正文。 */
typedef struct test_http_future_stream {
	unsigned char Data[16];
	size_t Size;
	size_t Calls;
} test_http_future_stream;



/* 收集流式响应片段，不让响应对象保存第二份正文。 */
static bool testHttpFutureStreamBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_future_stream* pStream =
		(test_http_future_stream*)pData;

	(void)pCall;
	testRequire(
		(xrtHttpResponseStatus(pResponse) == 200) &&
		(Data.Size <=
		 (sizeof(pStream->Data) - pStream->Size)),
		"HTTP Future stream callback input mismatch"
	);
	memcpy(
		pStream->Data + pStream->Size,
		Data.Data,
		Data.Size
	);
	pStream->Size += Data.Size;
	pStream->Calls++;
	return true;
}



/* 验证流式响应、Future 终态和响应计数共享同一调用契约。 */
static void testHttpFutureStream(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: close\r\n"
		"\r\n"
		"3\r\nabc\r\n"
		"5\r\ndefgh\r\n"
		"0\r\n\r\n";
	testhttporigin Origin;
	test_http_future_stream Stream;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	xfuture* pFuture;
	xhttpresult* pResult;
	xhttpcallinfo Info;

	memset(&Stream, 0, sizeof(Stream));
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(
		&Origin,
		"/future-stream"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Events.Body = testHttpFutureStreamBody;
	Options.Events.Data = &Stream;
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		&Options
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP Future stream completion failed"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		(Stream.Calls != 0) &&
		(Stream.Size == 8u) &&
		(memcmp(Stream.Data, "abcdefgh", 8u) == 0) &&
		(xrtHttpResponseBody(
			xrtHttpResultResponse(pResult)
		).Size == 0) &&
		(xrtHttpResponseBodyBytes(
			xrtHttpResultResponse(pResult)
		) == 8u) &&
		xrtHttpResultInfo(pResult, &Info) &&
		(Info.ResponseBodyBytes == 8u) &&
		(Info.Result == XNET_RESULT_OK),
		"HTTP Future streamed response mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 记录未知长度请求正文的 Reader、租约和工厂生命周期。 */
typedef struct test_http_future_source {
	size_t Opens;
	size_t Closes;
	size_t Releases;
	size_t Destroys;
	size_t Offset;
	size_t Reads;
} test_http_future_source;



/* 释放一次请求正文租约。 */
static void testHttpFutureSourceRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_future_source* pSource =
		(test_http_future_source*)pContext;

	(void)pData;
	(void)iSize;
	pSource->Releases++;
}



/* 以最多四字节片段生成未知长度请求正文。 */
static xhttpbodystatus testHttpFutureSourceNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	static const unsigned char Data[] = "hello-future";
	test_http_future_source* pSource =
		(test_http_future_source*)pContext;
	size_t iRemain;
	size_t iChunk;

	if ( pSource->Offset == (sizeof(Data) - 1u) ) {
		pSource->Reads++;
		return XHTTP_BODY_EOF;
	}
	testRequire(
		iMaxBytes != 0,
		"HTTP Future source received a zero byte limit"
	);
	iRemain = (sizeof(Data) - 1u) - pSource->Offset;
	iChunk = iRemain < 4u ? iRemain : 4u;
	if ( iChunk > iMaxBytes ) {
		iChunk = iMaxBytes;
	}
	pChunk->Data = Data + pSource->Offset;
	pChunk->Size = iChunk;
	pChunk->Release = testHttpFutureSourceRelease;
	pChunk->Context = pSource;
	pSource->Offset += iChunk;
	pSource->Reads++;
	return XHTTP_BODY_DATA;
}



/* 记录 Reader 关闭。 */
static void testHttpFutureSourceClose(ptr pContext)
{
	test_http_future_source* pSource =
		(test_http_future_source*)pContext;

	pSource->Closes++;
}



/* 为一次 Future 调用建立独立 Reader。 */
static bool testHttpFutureSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_future_source* pSource =
		(test_http_future_source*)pFactory;

	pSource->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpFutureSourceNext;
	pOps->Close = testHttpFutureSourceClose;
	*ppReader = pSource;
	return true;
}



/* 记录正文对象最后一个引用的销毁。 */
static void testHttpFutureSourceDestroy(ptr pFactory)
{
	test_http_future_source* pSource =
		(test_http_future_source*)pFactory;

	pSource->Destroys++;
}



/* 验证未知长度 chunked 请求可以完整通过 Future 执行和回收。 */
static void testHttpFutureChunkedRequest(
	xnetengine* pEngine
)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	static const xhttpbodyops Ops = {
		testHttpFutureSourceOpen,
		testHttpFutureSourceDestroy
	};
	testhttporigin Origin;
	test_http_future_source Source;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpbody* pBody;
	xfuture* pFuture;
	xhttpresult* pResult;
	xhttpcallinfo Info;

	memset(&Source, 0, sizeof(Source));
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	testHttpOriginExpectChunked(
		&Origin,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/future-chunked")
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(
		&Origin,
		"/future-chunked"
	);
	testRequire(
		xrtHttpRequestSetMethod(
			pRequest,
			XRT_STR_LITERAL("POST")
		),
		"HTTP Future chunked method setup failed"
	);
	pBody = xrtHttpBodyCreate(
		&Ops,
		&Source,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_NONE
	);
	testRequire(
		(pBody != NULL) &&
		xrtHttpRequestSetBody(pRequest, pBody),
		"HTTP Future chunked body setup failed"
	);
	xrtHttpBodyDestroy(pBody);
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP Future chunked completion failed"
	);
	memset(&Info, 0, sizeof(Info));
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	if (
		(pResult == NULL) ||
		!xrtHttpResultInfo(pResult, &Info) ||
		(Info.RequestSent == 0) ||
		(Info.RequestWireBytes <= 12u) ||
		(Source.Opens != 1u) ||
		(Source.Offset != 12u) ||
		(Source.Releases != 3u) ||
		(Source.Closes != 1u) ||
		(Source.Destroys != 1u)
	) {
		fprintf(
			stderr,
			"[DETAIL] sent=%llu wire=%llu"
			" opens=%zu offset=%zu reads=%zu releases=%zu"
			" closes=%zu destroys=%zu\n",
			(unsigned long long)Info.RequestSent,
			(unsigned long long)Info.RequestWireBytes,
			Source.Opens,
			Source.Offset,
			Source.Reads,
			Source.Releases,
			Source.Closes,
			Source.Destroys
		);
		testRequire(
			false,
			"HTTP Future chunked request lifecycle mismatch"
		);
	}
	xrtFutureDestroy(pFuture);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 验证 Future 取消请求能够推进底层 Call 并确认取消终态。 */
static void testHttpFutureCancel(xnetengine* pEngine)
{
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;

	testHttpOriginStart(&Origin, pEngine, NULL, 0);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(&Origin, "/cancel");
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pFuture != NULL,
		"HTTP cancelled Future submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"HTTP cancelled Future did not reach origin"
	);
	testRequire(
		xrtFutureCancel(pFuture),
		"HTTP Future cancel request failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED),
		"HTTP Future cancelled terminal mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 验证 Call 外部取消令牌也会反映为 Future 取消终态。 */
static void testHttpFutureParentCancel(xnetengine* pEngine)
{
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	xcancel* pCancel;
	xfuture* pFuture;

	testHttpOriginStart(&Origin, pEngine, NULL, 0);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(&Origin, "/parent-cancel");
	pCancel = xrtCancelCreate();
	testRequire(
		pCancel != NULL,
		"HTTP Future parent cancel create failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Cancel = pCancel;
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		&Options
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pFuture != NULL,
		"HTTP parent-cancel Future submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"HTTP parent-cancel Future did not reach origin"
	);
	testRequire(
		xrtCancelRequest(pCancel),
		"HTTP Future parent cancellation failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED),
		"HTTP Future parent cancellation terminal mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtCancelDestroy(pCancel);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 验证 Future 成功值不会丢弃 101 响应、缓冲余量或 Upgrade Stream。 */
static void testHttpFutureUpgrade(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n"
		"RAW";
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xhttpresult* pResult;
	xhttpresponse* pResponse;
	xnetstream* pStream;

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureUpgradeRequest(
		&Origin,
		"/upgrade"
	);
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP Future Upgrade completion failed"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		xrtHttpResultUpgraded(pResult) &&
		(xrtHttpResultTcp(pResult) != NULL) &&
		(xrtHttpResultBuffered(pResult) == 3u) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 101),
		"HTTP Future Upgrade result mismatch"
	);
	pResponse = xrtHttpResultTakeResponse(pResult);
	pStream = xrtHttpResultTakeTcp(pResult);
	testRequire(
		(pResponse != NULL) &&
		(pStream != NULL) &&
		(xrtHttpResultResponse(pResult) == NULL) &&
		(xrtHttpResultTcp(pResult) == NULL),
		"HTTP Future Upgrade ownership transfer failed"
	);
	xrtFutureDestroy(pFuture);
	(void)xrtNetStreamAbort(pStream);
	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pStream);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 最后一个结果引用必须中止并释放未被调用方取走的 Upgrade 传输。 */
static void testHttpFutureUpgradeCleanup(
	xnetengine* pEngine
)
{
	static const char Wire[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n";
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xhttpresult* pResult;

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureUpgradeRequest(
		&Origin,
		"/upgrade-cleanup"
	);
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP Future unclaimed Upgrade completion failed"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		xrtHttpResultUpgraded(pResult) &&
		(xrtHttpResultTcp(pResult) != NULL),
		"HTTP Future unclaimed Upgrade result mismatch"
	);
	xrtFutureDestroy(pFuture);
	testHttpOriginWait(
		&Origin.StreamClosed,
		1,
		"HTTP Future result did not abort unclaimed Upgrade"
	);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* Worker 内同步等待必须在提交调用前失败，避免自我死锁。 */
typedef struct test_http_sync_worker {
	xhttpclient* Client;
	xhttprequest* Request;
	xatomic32 Done;
	xerrkind Kind;
	int32 Code;
} test_http_sync_worker;



/* 在网络 Worker 中验证同步入口的死锁保护。 */
static void testHttpSyncWorkerTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_sync_worker* pState =
		(test_http_sync_worker*)pData;
	xhttpresult* pResult;
	const xerror* pError;

	(void)pWorker;
	pResult = xrtHttpClientDoSync(
		pState->Client,
		pState->Request,
		NULL
	);
	pError = xrtGetError();
	pState->Kind = pError != NULL ?
		xrtErrorKind(pError) : XERR_NONE;
	pState->Code = pError != NULL ?
		xrtErrorCode(pError) : 0;
	xrtHttpResultDestroy(pResult);
	xrtClearError();
	xrtAtomic32Store(
		&pState->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证宿主线程同步便利入口和 Worker 死锁保护。 */
static void testHttpFutureSync(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 204 No Content\r\n"
		"Connection: close\r\n"
		"\r\n";
	testhttporigin Origin;
	test_http_sync_worker Worker;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpresult* pResult;

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	pClient = testHttpFutureClient(pEngine);
	pRequest = testHttpFutureRequest(&Origin, "/sync");
	pResult = xrtHttpClientDoSync(
		pClient,
		pRequest,
		NULL
	);
	testRequire(
		(pResult != NULL) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 204),
		"HTTP synchronous result mismatch"
	);
	xrtHttpResultDestroy(pResult);

	memset(&Worker, 0, sizeof(Worker));
	xrtAtomic32Init(&Worker.Done, 0);
	Worker.Client = pClient;
	Worker.Request = pRequest;
	testRequire(
		xrtNetEnginePost(
			pEngine,
			0,
			testHttpSyncWorkerTask,
			&Worker
		),
		"HTTP sync Worker check post failed"
	);
	testHttpOriginWait(
		&Worker.Done,
		1,
		"HTTP sync Worker check did not complete"
	);
	testRequire(
		(Worker.Kind == XERR_STATE) &&
		(Worker.Code == XHTTP_CLIENT_ERROR_INTERNAL),
		"HTTP sync Worker error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* 等待全部异步析构退出 Engine，再验证最终对象计数归零。 */
static void testHttpFutureEngineDestroy(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP Future retained an Engine object"
		);
		xrtThreadYield();
	}
}



/* 覆盖成功、双向取消、Upgrade、同步等待与 Worker 防死锁。 */
int main(void)
{
	xnetengineconfig Config;
	xnetengine* pEngine;

	xrtNetEngineConfigInit(&Config);
	Config.Backend = TEST_HTTP_CLIENT_FUTURE_BACKEND;
	Config.Workers = 2;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP Future engine start failed"
	);
	testHttpFutureSuccess(pEngine);
	testHttpFutureStream(pEngine);
	testHttpFutureChunkedRequest(pEngine);
	testHttpFutureCancel(pEngine);
	testHttpFutureParentCancel(pEngine);
	testHttpFutureUpgrade(pEngine);
	testHttpFutureUpgradeCleanup(pEngine);
	testHttpFutureSync(pEngine);
	testHttpFutureEngineDestroy(pEngine);
	printf(
		"[PASS] high-level HTTP Future/sync contract (%s)\n",
		TEST_HTTP_CLIENT_FUTURE_BACKEND_NAME
	);
	return 0;
}
