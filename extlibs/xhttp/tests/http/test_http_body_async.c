#include "../test.h"



/* 异步测试来源先返回 AGAIN，再通过已完成 Future 变为可读。 */
typedef struct test_http_body_async {
	size_t Step;
	bool WaitFails;
	bool WaitSetsError;
	bool WaitClearsError;
} test_http_body_async;



/* 异步测试 Chunk 借用静态数据，不需要真实回收。 */
static void testHttpBodyAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次读取等待，第二次发布数据，最后结束。 */
static xhttpbodystatus testHttpBodyAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_body_async* pState =
		(test_http_body_async*)pContext;

	(void)iMaxBytes;
	if ( pState->Step == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Step == 1 ) {
		pState->Step = 2;
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release = testHttpBodyAsyncRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 创建立即完成的可读性 Future，并推进来源状态。 */
static xfuture* testHttpBodyAsyncWait(ptr pContext)
{
	test_http_body_async* pState =
		(test_http_body_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	if ( pState->WaitFails ) {
		if ( pState->WaitClearsError ) {
			xrtClearError();
		}
		if ( pState->WaitSetsError ) {
			xerror* pError = xrtErrorCreate(
				XERR_IO,
				"test.http.body.wait",
				72,
				"source wait failed"
			);

			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
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



/* 异步工厂返回 Next 与 Wait 的同一来源状态。 */
static bool testHttpBodyAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyAsyncNext;
	pOps->Wait = testHttpBodyAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 验证 AGAIN、Future、DATA、EOF 和等待失败终态。 */
int main(void)
{
	xhttpbodyops Ops = {
		testHttpBodyAsyncOpen,
		NULL
	};
	test_http_body_async State = { 0 };
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pFuture;
	const xerror* pFailure;

	testRequire((xrtHttpBodyReaderWait(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body wait accepted null Reader");
	xrtClearError();
	pBody = xrtHttpBodyCreate(
		&Ops, &State, 5, XHTTP_BODY_REPLAYABLE
	);
	testRequire(pBody != NULL,
		"async HTTP body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"async HTTP body open failed");
	testRequire(xrtHttpBodyReaderWait(pReader) == NULL,
		"HTTP body wait succeeded before AGAIN");
	xrtClearError();
	testRequire(xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_AGAIN, "async HTTP body did not return AGAIN");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) &&
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"async HTTP body allowed duplicate wait");
	xrtClearError();
	testRequire(xrtFutureWait(pFuture) == XWAIT_OK,
		"async HTTP body wait Future failed");
	xrtFutureDestroy(pFuture);
	testRequire((xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 5) &&
		(memcmp(Chunk.Data, "ready", 5) == 0),
		"async HTTP body data mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_EOF, "async HTTP body EOF mismatch");
	testRequire((xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP body wait succeeded after EOF");
	xrtClearError();
	xrtHttpBodyReaderDestroy(pReader);

	State.Step = 0;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) && (xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_AGAIN),
		"async HTTP body detached Future setup failed");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire(pFuture != NULL,
		"async HTTP body detached Future creation failed");
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(xrtFutureWait(pFuture) == XWAIT_OK,
		"HTTP body wait Future depended on destroyed Reader");
	xrtFutureDestroy(pFuture);

	State.Step = 0;
	State.WaitFails = true;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) && (xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_AGAIN),
		"async HTTP body stale-error setup failed");
	{
		xerror* pOld = xrtErrorCreate(
			XERR_VALUE, "test.old", 11, "old error"
		);

		testRequire(pOld != NULL,
			"async HTTP body old error allocation failed");
		xrtSetError(pOld);
		xrtErrorFree(pOld);
	}
	testRequire((xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"http.body"
		) == 0),
		"async HTTP body wait failure was not stable");
	pFailure = xrtHttpBodyReaderError(pReader);
	xrtClearError();
	testRequire((xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) == pFailure) &&
		(xrtGetError() == pFailure),
		"async HTTP body wait failure changed on replay");
	xrtHttpBodyReaderDestroy(pReader);
	xrtClearError();

	State.Step = 0;
	State.WaitSetsError = true;
	State.WaitClearsError = true;
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_AGAIN) &&
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.wait"
		) == 0) &&
		(xrtErrorCode(
			xrtHttpBodyReaderError(pReader)
		) == 72),
		"async HTTP body source error was replaced");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	printf("[PASS] async HTTP body\n");
	return 0;
}

