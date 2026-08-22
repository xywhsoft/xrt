#include "../test.h"



/* 异步 Server Response 测试源先返回 AGAIN，再发布固定正文。 */
typedef struct test_http_server_response_async {
	size_t Step;
	size_t Nexts;
	size_t Waits;
	size_t Closes;
	bool WaitFails;
} test_http_server_response_async;



/* 静态正文租约不需要真实回收。 */
static void testHttpServerResponseAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次读取返回 AGAIN，就绪后发布正文并正常结束。 */
static xhttpbodystatus testHttpServerResponseAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_server_response_async* pState =
		(test_http_server_response_async*)pContext;

	(void)iMaxBytes;
	pState->Nexts++;
	if ( pState->Step == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Step == 1 ) {
		pState->Step = 2;
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release =
			testHttpServerResponseAsyncRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 创建立即完成的可读 Future，或发布可识别的来源错误。 */
static xfuture* testHttpServerResponseAsyncWait(ptr pContext)
{
	test_http_server_response_async* pState =
		(test_http_server_response_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	pState->Waits++;
	if ( pState->WaitFails ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.server.response.wait",
			91,
			"wait failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		return NULL;
	}
	pState->Step = 1;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 记录 Response 最终关闭 Reader 的次数。 */
static void testHttpServerResponseAsyncClose(ptr pContext)
{
	test_http_server_response_async* pState =
		(test_http_server_response_async*)pContext;

	pState->Closes++;
}



/* 打开共享测试状态对应的单个异步 Reader。 */
static bool testHttpServerResponseAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpServerResponseAsyncNext;
	pOps->Close = testHttpServerResponseAsyncClose;
	pOps->Wait = testHttpServerResponseAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 创建带固定长度异步正文的 HTTP/1.1 Server Response。 */
static xhttp1serverresponse* testHttpServerResponseAsyncCreate(
	test_http_server_response_async* pState,
	int64 iLength
)
{
	static const xhttpbodyops Ops = {
		testHttpServerResponseAsyncOpen,
		NULL
	};
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops, pState, iLength, XHTTP_BODY_NONE
	);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttp1serverresponse* pResponse;

	if ( (pBody == NULL) || (pReply == NULL) ||
		!xrtHttpReplySetBody(pReply, pBody) ) {
		xrtHttpReplyDestroy(pReply);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	xrtHttpBodyDestroy(pBody);
	pResponse = xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		0,
		pReply
	);
	xrtHttpReplyDestroy(pReply);
	return pResponse;
}



/* 消费响应 Header，直到正文源第一次返回 AGAIN。 */
static bool testHttpServerResponseAsyncReachAgain(
	xhttp1serverresponse* pResponse,
	size_t* pBytes
)
{
	size_t iGuard = 0;

	while ( iGuard++ < 32 ) {
		xbytesview Data;
		xhttp1serveroutputstatus Status =
			xrtHttp1ServerResponseOutput(
				pResponse, 4096, &Data
			);

		if ( Status == XHTTP1_SERVER_OUTPUT_AGAIN ) {
			return true;
		}
		if ( (Status != XHTTP1_SERVER_OUTPUT_DATA) ||
			(Data.Data == NULL) || (Data.Size == 0) ||
			!xrtHttp1ServerResponseOutputConsume(
				pResponse, Data.Size
			) ) {
			return false;
		}
		if ( pBytes != NULL ) {
			*pBytes += Data.Size;
		}
	}
	return false;
}



/* 收集异步正文恢复后的全部输出，用于核对分帧和终止块。 */
static bool testHttpServerResponseAsyncFinish(
	xhttp1serverresponse* pResponse,
	char* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iOffset = 0;
	size_t iGuard = 0;

	while ( iGuard++ < 32 ) {
		xbytesview Data;
		xhttp1serveroutputstatus Status =
			xrtHttp1ServerResponseOutput(
				pResponse, 4096, &Data
			);

		if ( Status == XHTTP1_SERVER_OUTPUT_DONE ) {
			pOutput[iOffset] = '\0';
			*pSize = iOffset;
			return true;
		}
		if ( (Status != XHTTP1_SERVER_OUTPUT_DATA) ||
			(Data.Data == NULL) ||
			(Data.Size == 0) ||
			(Data.Size > ((iCapacity - 1) - iOffset)) ) {
			return false;
		}
		memcpy(
			pOutput + iOffset,
			Data.Data,
			Data.Size
		);
		iOffset += Data.Size;
		if ( !xrtHttp1ServerResponseOutputConsume(
			pResponse,
			Data.Size
		) ) {
			return false;
		}
	}
	return false;
}



/* 沿原因链查找指定错误域。 */
static bool testHttpServerResponseAsyncHasDomain(
	const xerror* pError,
	cstr sDomain
)
{
	while ( pError != NULL ) {
		if ( strcmp(
			xrtErrorDomain(pError),
			sDomain
		) == 0 ) {
			return true;
		}
		pError = xrtErrorCause(pError);
	}
	return false;
}



/* 验证 AGAIN、Future、恢复输出和来源错误终态。 */
int main(void)
{
	test_http_server_response_async Ready = { 0 };
	test_http_server_response_async Failed = {
		0, 0, 0, 0, true
	};
	test_http_server_response_async Chunked = { 0 };
	xhttp1serverresponse* pReady =
		testHttpServerResponseAsyncCreate(&Ready, 5);
	xhttp1serverresponse* pFailed =
		testHttpServerResponseAsyncCreate(&Failed, 5);
	xhttp1serverresponse* pChunked =
		testHttpServerResponseAsyncCreate(
			&Chunked,
			XHTTP_BODY_UNKNOWN
		);
	xbytesview Data;
	xfuture* pFuture;
	const xerror* pError;
	char ChunkedOutput[64];
	size_t iChunkedSize = 0;
	size_t iHeaderBytes = 0;

	testRequire(
		(pReady != NULL) &&
		(pFailed != NULL) &&
		(pChunked != NULL),
		"async HTTP server response create failed");
	testRequire(
		xrtHttp1ServerResponseWait(pReady) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP server response wait succeeded before AGAIN"
	);
	xrtClearError();
	testRequire(
		testHttpServerResponseAsyncReachAgain(
			pReady, &iHeaderBytes
		) &&
		(iHeaderBytes != 0) &&
		(Ready.Nexts == 1),
		"async HTTP server response did not reach AGAIN"
	);
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pReady, 1, &Data
		) == XHTTP1_SERVER_OUTPUT_AGAIN) &&
		(Ready.Nexts == 1),
		"repeated Output re-entered an unready response body"
	);
	pFuture = xrtHttp1ServerResponseWait(pReady);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK) &&
		(Ready.Waits == 1),
		"async HTTP server response wait failed"
	);
	xrtFutureDestroy(pFuture);
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pReady, 16, &Data
		) == XHTTP1_SERVER_OUTPUT_DATA) &&
		(Data.Size == 5) &&
		(memcmp(Data.Data, "ready", 5) == 0) &&
		xrtHttp1ServerResponseOutputConsume(
			pReady, Data.Size
		) &&
		(xrtHttp1ServerResponseOutput(
			pReady, 16, &Data
		) == XHTTP1_SERVER_OUTPUT_DONE) &&
		(Ready.Nexts == 3) &&
		(Ready.Closes == 1),
		"async HTTP server response resume mismatch"
	);

	testRequire(
		testHttpServerResponseAsyncReachAgain(
			pFailed, NULL
		) &&
		(xrtHttp1ServerResponseWait(pFailed) == NULL) &&
		(Failed.Waits == 1),
		"async HTTP server response wait failure was not terminal"
	);
	pError = xrtHttp1ServerResponseError(pFailed);
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_BODY) &&
		testHttpServerResponseAsyncHasDomain(
			pError,
			"test.http.server.response.wait"
		) &&
		(xrtHttp1ServerResponseOutput(
			pFailed, 16, &Data
		) == XHTTP1_SERVER_OUTPUT_ERROR),
		"async HTTP server response wait cause mismatch"
	);

	testRequire(
		testHttpServerResponseAsyncReachAgain(
			pChunked,
			NULL
		),
		"chunked async HTTP response did not reach AGAIN"
	);
	pFuture = xrtHttp1ServerResponseWait(pChunked);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"chunked async HTTP response wait failed"
	);
	xrtFutureDestroy(pFuture);
	testRequire(
		testHttpServerResponseAsyncFinish(
			pChunked,
			ChunkedOutput,
			sizeof(ChunkedOutput),
			&iChunkedSize
		) &&
		(iChunkedSize == 15) &&
		(memcmp(
			ChunkedOutput,
			"5\r\nready\r\n0\r\n\r\n",
			15
		 ) == 0) &&
		(Chunked.Nexts == 3) &&
		(Chunked.Closes == 1),
		"chunked async HTTP response framing mismatch"
	);

	xrtHttp1ServerResponseDestroy(pChunked);
	xrtHttp1ServerResponseDestroy(pFailed);
	xrtHttp1ServerResponseDestroy(pReady);
	xrtClearError();
	printf("[PASS] async HTTP server response\n");
	return 0;
}
