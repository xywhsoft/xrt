#include "../test.h"



/* 异步 FormData Part 在一次等待后发布正文。 */
typedef struct test_form_data_async {
	size_t Step;
} test_form_data_async;



/* 静态异步测试数据不需要释放。 */
static void testFormDataAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次返回 AGAIN，等待完成后发布正文。 */
static xhttpbodystatus testFormDataAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_form_data_async* pState =
		(test_form_data_async*)pContext;

	(void)iMaxBytes;
	if ( pState->Step == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Step == 1 ) {
		pState->Step = 2;
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release = testFormDataAsyncRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 完成一次可读性等待并推进来源。 */
static xfuture* testFormDataAsyncWait(ptr pContext)
{
	test_form_data_async* pState =
		(test_form_data_async*)pContext;
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



/* 为异步 Part 返回共享的一次读取状态。 */
static bool testFormDataAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testFormDataAsyncNext;
	pOps->Wait = testFormDataAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 验证 FormData 封包透明传递 Part 的 AGAIN 和 Wait。 */
int main(void)
{
	static const xhttpbodyops Ops = {
		testFormDataAsyncOpen,
		NULL
	};
	test_form_data_async State = { 0 };
	xmultipartboundary Boundary;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xhttpbody* pPart = xrtHttpBodyCreate(
		&Ops, &State, 5, XHTTP_BODY_NONE
	);
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	xfuture* pFuture;
	bool bFound = false;

	testRequire((pForm != NULL) && (pPart != NULL) &&
		xrtMultipartBoundaryParse(
			XRT_STR_LITERAL("async"), &Boundary
		) && xrtFormDataAppendBody(
			pForm,
			XRT_STR_LITERAL("value"),
			pPart,
			NULL,
			(xstrview){ NULL, 0 }
		), "async FormData setup failed");
	xrtHttpBodyDestroy(pPart);
	pBody = xrtFormDataBody(pForm, &Boundary);
	testRequire(pBody != NULL,
		"async FormData body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"async FormData body open failed");
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 64, &Chunk);
		if ( Status == XHTTP_BODY_AGAIN ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"async FormData prefix read failed");
		xrtHttpBodyChunkRelease(&Chunk);
	}
	pFuture = xrtHttpBodyReaderWait(pReader);
	testRequire((pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"async FormData wait failed");
	xrtFutureDestroy(pFuture);
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 64, &Chunk);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"async FormData suffix read failed");
		if ( (Chunk.Size == 5) &&
			(memcmp(Chunk.Data, "ready", 5) == 0) ) {
			bFound = true;
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(bFound,
		"async FormData Part data was not preserved");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtFormDataDestroy(pForm);
	printf("[PASS] async FormData\n");
	return 0;
}
