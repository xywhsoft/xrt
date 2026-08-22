#include "../test.h"
#include "test_http_body_inflate_fixture.h"



/* 异步来源状态在 Wait 前不可读，之后按来源上限分片发布。 */
typedef struct test_http_body_inflate_async {
	size_t Offset;
	bool Ready;
	bool WaitFails;
} test_http_body_inflate_async;



/* 异步测试借用静态数据，Chunk 不需要真实回收。 */
static void testHttpBodyInflateAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 在 Ready 后发布 gzip 测试数据，否则返回 AGAIN。 */
static xhttpbodystatus testHttpBodyInflateAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_body_inflate_async* pState =
		(test_http_body_inflate_async*)pContext;
	size_t iRemaining;
	size_t iSize;

	if ( !pState->Ready ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Offset == sizeof(TestHttpBodyInflateAsyncGzip) ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = sizeof(TestHttpBodyInflateAsyncGzip) -
		pState->Offset;
	iSize = iRemaining < iMaxBytes ? iRemaining : iMaxBytes;
	pChunk->Data = TestHttpBodyInflateAsyncGzip +
		pState->Offset;
	pChunk->Size = iSize;
	pChunk->Release = testHttpBodyInflateAsyncRelease;
	pState->Offset += iSize;
	return XHTTP_BODY_DATA;
}



/* 创建一次已完成 Future，或发布来源自己的等待错误。 */
static xfuture* testHttpBodyInflateAsyncWait(ptr pContext)
{
	test_http_body_inflate_async* pState =
		(test_http_body_inflate_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	if ( pState->WaitFails ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.body.inflate.wait",
			92,
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
	pState->Ready = true;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 异步来源打开同一非可重放状态。 */
static bool testHttpBodyInflateAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyInflateAsyncNext;
	pOps->Wait = testHttpBodyInflateAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 创建一次异步测试来源及其流式解压包装。 */
static xhttpbody* testHttpBodyInflateAsyncCreate(
	test_http_body_inflate_async* pState
)
{
	static const xhttpbodyops Ops = {
		testHttpBodyInflateAsyncOpen,
		NULL
	};
	xhttpbodyinflateconfig Config;
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
	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = XINFLATE_GZIP;
	Config.ReadSize = 3;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	xrtHttpBodyDestroy(pSource);
	return pBody;
}



/* 验证 AGAIN/Wait 透明组合和来源等待错误传播。 */
int main(void)
{
	test_http_body_inflate_async State = { 0 };
	xhttpbody* pBody = testHttpBodyInflateAsyncCreate(&State);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	xfuture* pFuture;
	uint8 Output[128];
	size_t iOutput = 0;

	testRequire(pBody != NULL,
		"async HTTP Inflate body creation failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 5, &Chunk
		) == XHTTP_BODY_AGAIN),
		"async HTTP Inflate body did not return AGAIN");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"async HTTP Inflate body wait failed");
	xrtFutureDestroy(pFuture);
	while ( (Status = xrtHttpBodyNext(
		pReader, 5, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire(Chunk.Size <= (sizeof(Output) - iOutput),
			"async HTTP Inflate output overflow");
		memcpy(Output + iOutput, Chunk.Data, Chunk.Size);
		iOutput += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire((Status == XHTTP_BODY_EOF) &&
		(iOutput == (sizeof(TestHttpBodyInflateAsyncPlain) - 1u)) &&
		(memcmp(
			Output,
			TestHttpBodyInflateAsyncPlain,
			iOutput
		) == 0),
		"async HTTP Inflate output mismatch");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);

	memset(&State, 0, sizeof(State));
	State.WaitFails = true;
	pBody = testHttpBodyInflateAsyncCreate(&State);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 5, &Chunk
		) == XHTTP_BODY_AGAIN) &&
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.inflate.wait"
		) == 0),
		"async HTTP Inflate replaced source wait error");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	printf("[PASS] async HTTP Inflate body\n");
	return 0;
}
