#include "../test.h"



/* 异步来源状态在 Wait 前不可读，之后按来源上限分片发布。 */
typedef struct test_http_body_deflate_async {
	xpromise* Promise;
	size_t Offset;
	size_t Closes;
	bool Ready;
	bool WaitFails;
	bool Pending;
} test_http_body_deflate_async;



/* 异步测试借用静态数据，Chunk 不需要真实回收。 */
static void testHttpBodyDeflateAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 在 Ready 后发布测试数据，否则返回 AGAIN。 */
static xhttpbodystatus testHttpBodyDeflateAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	static const uint8 Data[] = "async deflate body";
	test_http_body_deflate_async* pState =
		(test_http_body_deflate_async*)pContext;
	size_t iRemaining;
	size_t iSize;

	if ( !pState->Ready ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Offset == (sizeof(Data) - 1u) ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = (sizeof(Data) - 1u) - pState->Offset;
	iSize = iRemaining < iMaxBytes ? iRemaining : iMaxBytes;
	pChunk->Data = Data + pState->Offset;
	pChunk->Size = iSize;
	pChunk->Release = testHttpBodyDeflateAsyncRelease;
	pState->Offset += iSize;
	return XHTTP_BODY_DATA;
}



/* 创建一次已完成 Future，或发布来源自己的等待错误。 */
static xfuture* testHttpBodyDeflateAsyncWait(ptr pContext)
{
	test_http_body_deflate_async* pState =
		(test_http_body_deflate_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	if ( pState->WaitFails ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.body.deflate.wait",
			82,
			"wait failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	if ( pState->Pending ) {
		pState->Promise = xrtPromiseCreate(&pFuture, NULL);
		return pState->Promise != NULL ? pFuture : NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		return NULL;
	}
	pState->Ready = true;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 记录变换 Reader 是否关闭了活动来源 Reader。 */
static void testHttpBodyDeflateAsyncClose(ptr pContext)
{
	test_http_body_deflate_async* pState =
		(test_http_body_deflate_async*)pContext;

	pState->Closes++;
}



/* 异步来源打开同一非可重放状态。 */
static bool testHttpBodyDeflateAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyDeflateAsyncNext;
	pOps->Wait = testHttpBodyDeflateAsyncWait;
	pOps->Close = testHttpBodyDeflateAsyncClose;
	*ppReader = pFactory;
	return true;
}



/* 创建一次异步测试来源及其流式压缩包装。 */
static xhttpbody* testHttpBodyDeflateAsyncCreate(
	test_http_body_deflate_async* pState
)
{
	static const xhttpbodyops Ops = {
		testHttpBodyDeflateAsyncOpen,
		NULL
	};
	xhttpbodydeflateconfig Config;
	xhttpbody* pSource = xrtHttpBodyCreate(
		&Ops,
		pState,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_NONE
	);
	xhttpbody* pBody;

	if ( pSource == NULL ) {
		return NULL;
	}
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.ReadSize = 3;
	pBody = xrtHttpBodyDeflate(pSource, &Config);
	xrtHttpBodyDestroy(pSource);
	return pBody;
}



/* 验证 AGAIN/Wait 透明组合和来源等待错误传播。 */
int main(void)
{
	static const uint8 Input[] = "async deflate body";
	test_http_body_deflate_async State = { 0 };
	xhttpbody* pBody = testHttpBodyDeflateAsyncCreate(&State);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	xfuture* pFuture;
	uint8 Output[128];
	bytes pExpected;
	size_t iExpected;
	size_t iOutput = 0;

	testRequire(pBody != NULL,
		"async HTTP Deflate body creation failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 5, &Chunk
		) == XHTTP_BODY_AGAIN),
		"async HTTP Deflate body did not return AGAIN");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"async HTTP Deflate body wait failed");
	xrtFutureDestroy(pFuture);
	while ( (Status = xrtHttpBodyNext(
		pReader, 5, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire(Chunk.Size <= (sizeof(Output) - iOutput),
			"async HTTP Deflate output overflow");
		memcpy(Output + iOutput, Chunk.Data, Chunk.Size);
		iOutput += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(Status == XHTTP_BODY_EOF,
		"async HTTP Deflate body did not end");
	pExpected = xrtDeflateAll(
		(xbytesview){ Input, sizeof(Input) - 1u },
		NULL,
		&iExpected
	);
	testRequire((pExpected != NULL) &&
		(iOutput == iExpected) &&
		(memcmp(Output, pExpected, iExpected) == 0),
		"async HTTP Deflate output mismatch");
	xrtFree(pExpected);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);

	memset(&State, 0, sizeof(State));
	State.WaitFails = true;
	pBody = testHttpBodyDeflateAsyncCreate(&State);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 5, &Chunk
		) == XHTTP_BODY_AGAIN) &&
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.deflate.wait"
		) == 0),
		"async HTTP Deflate replaced source wait error");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	memset(&State, 0, sizeof(State));
	State.Pending = true;
	pBody = testHttpBodyDeflateAsyncCreate(&State);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 5, &Chunk
		) == XHTTP_BODY_AGAIN),
		"pending HTTP Deflate body did not return AGAIN");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) && (State.Promise != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_PENDING),
		"pending HTTP Deflate Future setup failed");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(State.Closes == 1,
		"pending HTTP Deflate source Reader was not closed");
	testRequire(xrtPromiseResolve(State.Promise, NULL),
		"pending HTTP Deflate Future resolve failed");
	xrtPromiseDestroy(State.Promise);
	State.Promise = NULL;
	testRequire((xrtFutureWait(pFuture) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"pending HTTP Deflate Future expired with its Reader");
	xrtFutureDestroy(pFuture);

	printf("[PASS] async HTTP Deflate body\n");
	return 0;
}
