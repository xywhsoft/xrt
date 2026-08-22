#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_BODY_ASYNC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头测试来源先等待一次，再结束空正文。 */
typedef struct test_single_http_body_async {
	bool Ready;
} test_single_http_body_async;



/* 未就绪时返回 AGAIN，就绪后返回 EOF。 */
static xhttpbodystatus testSingleHttpBodyNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_single_http_body_async* pState =
		(test_single_http_body_async*)pContext;

	(void)iMaxBytes;
	(void)pChunk;
	return pState->Ready ? XHTTP_BODY_EOF : XHTTP_BODY_AGAIN;
}



/* 创建立即完成的可读 Future。 */
static xfuture* testSingleHttpBodyWait(ptr pContext)
{
	test_single_http_body_async* pState =
		(test_single_http_body_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);

	if ( pPromise == NULL ) {
		return NULL;
	}
	pState->Ready = true;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 打开异步单头测试来源。 */
static bool testSingleHttpBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testSingleHttpBodyNext;
	pOps->Wait = testSingleHttpBodyWait;
	*ppReader = pFactory;
	return true;
}



/* 验证单头文件实际执行异步正文等待路径。 */
int main(void)
{
	xhttpbodyops Ops = {
		testSingleHttpBodyOpen,
		NULL
	};
	test_single_http_body_async State = { false };
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops, &State, 0, XHTTP_BODY_REPLAYABLE
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pFuture;
	bool bPass = false;

	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	if ( (pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 1, &Chunk
		) == XHTTP_BODY_AGAIN) ) {
		pFuture = xrtHttpBodyReaderWait(pReader);
		if ( pFuture != NULL ) {
			bPass = (xrtFutureWait(pFuture) == XWAIT_OK) &&
				(xrtHttpBodyNext(
					pReader, 1, &Chunk
				) == XHTTP_BODY_EOF);
			xrtFutureDestroy(pFuture);
		}
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return bPass ? 0 : 1;
}
