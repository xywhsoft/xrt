#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 单头文件异步来源状态。 */
typedef struct test_single_http_body_compose_async {
	bool Ready;
} test_single_http_body_compose_async;



/* 未就绪时等待，就绪后结束。 */
static xhttpbodystatus testSingleHttpBodyComposeAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_single_http_body_compose_async* pState =
		(test_single_http_body_compose_async*)pContext;

	(void)iMaxBytes;
	(void)pChunk;
	return pState->Ready ? XHTTP_BODY_EOF : XHTTP_BODY_AGAIN;
}



/* 返回完成 Future 并推进来源。 */
static xfuture* testSingleHttpBodyComposeAsyncWait(ptr pContext)
{
	test_single_http_body_compose_async* pState =
		(test_single_http_body_compose_async*)pContext;
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



/* 返回异步测试 Reader。 */
static bool testSingleHttpBodyComposeAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testSingleHttpBodyComposeAsyncNext;
	pOps->Close = NULL;
	pOps->Wait = testSingleHttpBodyComposeAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 验证单头文件组合层包含异步等待转发。 */
int main(void)
{
	static const xhttpbodyops Ops = {
		testSingleHttpBodyComposeAsyncOpen,
		NULL
	};
	test_single_http_body_compose_async State = { false };
	xhttpbody* pChild = xrtHttpBodyCreate(
		&Ops, &State, 0, XHTTP_BODY_NONE
	);
	xhttpbodypiece Pieces[2];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pFuture;

	if ( pChild == NULL ) {
		return 1;
	}
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ NULL, 0 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	pBody = xrtHttpBodyCompose(Pieces, 2);
	xrtHttpBodyDestroy(pChild);
	pReader = pBody != NULL ? xrtHttpBodyOpen(pBody) : NULL;
	if ( (pReader == NULL) ||
		(xrtHttpBodyNext(
			pReader, 1, &Chunk
		) != XHTTP_BODY_AGAIN) ) {
		xrtHttpBodyReaderDestroy(pReader);
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	pFuture = xrtHttpBodyReaderWait(pReader);
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		xrtHttpBodyReaderDestroy(pReader);
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	xrtFutureDestroy(pFuture);
	if ( xrtHttpBodyNext(
		pReader, 1, &Chunk
	) != XHTTP_BODY_EOF ) {
		xrtHttpBodyReaderDestroy(pReader);
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] single-http-body-compose-async\n");
	return 0;
}
