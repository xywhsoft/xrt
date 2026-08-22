#include "../test.h"



/* 异步子正文先等待，再发布固定数据。 */
typedef struct test_http_body_compose_async {
	size_t Step;
} test_http_body_compose_async;



/* 静态异步测试数据不需要回收。 */
static void testHttpBodyComposeAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次要求等待，第二次发布数据，最后结束。 */
static xhttpbodystatus testHttpBodyComposeAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_body_compose_async* pState =
		(test_http_body_compose_async*)pContext;

	(void)iMaxBytes;
	if ( pState->Step == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Step == 1 ) {
		pState->Step = 2;
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release = testHttpBodyComposeAsyncRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 完成一次等待并把来源推进到可读状态。 */
static xfuture* testHttpBodyComposeAsyncWait(ptr pContext)
{
	test_http_body_compose_async* pState =
		(test_http_body_compose_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);

	if ( pPromise == NULL ) {
		return NULL;
	}
	pState->Step = 1;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 为异步测试返回共享的一次读取状态。 */
static bool testHttpBodyComposeAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyComposeAsyncNext;
	pOps->Wait = testHttpBodyComposeAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 待完成来源用于验证 Future 脱离组合 Reader 后的独立生命周期。 */
typedef struct test_http_body_compose_pending {
	xpromise* Promise;
	xerror* Error;
	size_t Closes;
} test_http_body_compose_pending;



/* 待完成来源始终要求调用方进入 Wait。 */
static xhttpbodystatus testHttpBodyComposePendingNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	return XHTTP_BODY_AGAIN;
}



/* 返回待完成 Future，或发布配置的持久错误。 */
static xfuture* testHttpBodyComposePendingWait(ptr pContext)
{
	test_http_body_compose_pending* pState =
		(test_http_body_compose_pending*)pContext;
	xfuture* pFuture = NULL;

	if ( pState->Error != NULL ) {
		xrtSetError(pState->Error);
		return NULL;
	}
	pState->Promise = xrtPromiseCreate(&pFuture, NULL);
	return pState->Promise != NULL ? pFuture : NULL;
}



/* 记录组合 Reader 是否关闭了活动子 Reader。 */
static void testHttpBodyComposePendingClose(ptr pContext)
{
	test_http_body_compose_pending* pState =
		(test_http_body_compose_pending*)pContext;

	pState->Closes++;
}



/* 打开一个等待来源。 */
static bool testHttpBodyComposePendingOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyComposePendingNext;
	pOps->Wait = testHttpBodyComposePendingWait;
	pOps->Close = testHttpBodyComposePendingClose;
	*ppReader = pFactory;
	return true;
}



/* 验证组合层透明转发子正文的 AGAIN 与 Future。 */
static void testHttpBodyComposeAsyncForward(void)
{
	static const xhttpbodyops Ops = {
		testHttpBodyComposeAsyncOpen,
		NULL
	};
	test_http_body_compose_async State = { 0 };
	xhttpbody* pChild = xrtHttpBodyCreate(
		&Ops, &State, 5, XHTTP_BODY_NONE
	);
	xhttpbodypiece Pieces[3];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pFuture;

	testRequire(pChild != NULL,
		"async composed child create failed");
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"[", 1 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	Pieces[2] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"]", 1 }
	);
	pBody = xrtHttpBodyCompose(Pieces, 3);
	xrtHttpBodyDestroy(pChild);
	testRequire(pBody != NULL,
		"async composed HTTP body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Data[0] == '['),
		"async composed prefix mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_AGAIN,
		"async composed child did not return AGAIN");
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"async composed wait did not reach child");
	xrtFutureDestroy(pFuture);
	testRequire((xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 5) &&
		(memcmp(Chunk.Data, "ready", 5) == 0),
		"async composed child data mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire((xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 1) &&
		(Chunk.Data[0] == ']'),
		"async composed suffix mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_EOF,
		"async composed HTTP body did not end");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
}



/* 验证 Future 独立生命周期与嵌套来源错误的准确重放。 */
static void testHttpBodyComposeAsyncLifetime(void)
{
	static const xhttpbodyops Ops = {
		testHttpBodyComposePendingOpen,
		NULL
	};
	test_http_body_compose_pending State = { 0 };
	xhttpbodypiece Pieces[2];
	xhttpbody* pChild;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pFuture;
	xerror* pError;

	pChild = xrtHttpBodyCreate(
		&Ops,
		&State,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	Pieces[0] = xrtHttpBodyPieceBytes(XRT_BYTES_LITERAL("["));
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	pBody = xrtHttpBodyCompose(Pieces, 2);
	xrtHttpBodyDestroy(pChild);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pBody != NULL) && (pReader != NULL) &&
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA),
		"async composed lifetime setup failed"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_AGAIN,
		"async composed lifetime source was not pending"
	);
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire(
		(pFuture != NULL) && (State.Promise != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_PENDING),
		"async composed lifetime Future setup failed"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(State.Closes == 1,
		"async composed lifetime did not close child Reader");
	(void)xrtPromiseResolve(State.Promise, NULL);
	xrtPromiseDestroy(State.Promise);
	State.Promise = NULL;
	testRequire(
		(xrtFutureWait(pFuture) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"async composed Future expired with its Reader"
	);
	xrtFutureDestroy(pFuture);

	memset(&State, 0, sizeof(State));
	pError = xrtErrorCreate(
		XERR_IO,
		"test.http.body.compose.wait",
		85,
		"wait failed"
	);
	State.Error = pError;
	pChild = xrtHttpBodyCreate(
		&Ops,
		&State,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	Pieces[0] = xrtHttpBodyPieceBytes(XRT_BYTES_LITERAL("["));
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	pBody = xrtHttpBodyCompose(Pieces, 2);
	xrtHttpBodyDestroy(pChild);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pError != NULL) && (pBody != NULL) && (pReader != NULL) &&
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA),
		"async composed wait error setup failed"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_AGAIN,
		"async composed wait error source was not pending"
	);
	xrtSetError(pError);
	testRequire(
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtHttpBodyReaderError(pReader) == pError) &&
		(xrtGetError() == pError),
		"async composed wait replaced a republished source error"
	);
	xrtClearError();
	testRequire(
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtGetError() == pError),
		"async composed wait error was not stable"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	xrtErrorFree(pError);
}



/* 运行组合正文的异步转发、错误和生命周期回归。 */
int main(void)
{
	testHttpBodyComposeAsyncForward();
	testHttpBodyComposeAsyncLifetime();
	printf("[PASS] async HTTP body compose\n");
	return 0;
}
