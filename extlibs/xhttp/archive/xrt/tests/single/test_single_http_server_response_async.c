#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头测试源在 Wait 前返回 AGAIN，随后结束空正文。 */
typedef struct test_single_http_server_response_async {
	bool Ready;
	bool Done;
} test_single_http_server_response_async;



/* 静态单字节正文不需要真实回收。 */
static void testSingleHttpServerResponseAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 未就绪时返回 AGAIN，就绪后发布一个字节并结束正文。 */
static xhttpbodystatus testSingleHttpServerResponseAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_single_http_server_response_async* pState =
		(test_single_http_server_response_async*)pContext;

	(void)iMaxBytes;
	if ( !pState->Ready ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Done ) {
		return XHTTP_BODY_EOF;
	}
	pState->Done = true;
	pChunk->Data = (cbytes)"x";
	pChunk->Size = 1;
	pChunk->Release =
		testSingleHttpServerResponseAsyncRelease;
	return XHTTP_BODY_DATA;
}



/* 创建立即完成的可读 Future。 */
static xfuture* testSingleHttpServerResponseAsyncWait(
	ptr pContext
)
{
	test_single_http_server_response_async* pState =
		(test_single_http_server_response_async*)pContext;
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



/* 打开单头异步测试 Reader。 */
static bool testSingleHttpServerResponseAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next =
		testSingleHttpServerResponseAsyncNext;
	pOps->Wait =
		testSingleHttpServerResponseAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 验证单头文件实际执行 Server Response 异步正文路径。 */
int main(void)
{
	static const xhttpbodyops Ops = {
		testSingleHttpServerResponseAsyncOpen,
		NULL
	};
	test_single_http_server_response_async State = {
		false,
		false
	};
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops, &State, 1, XHTTP_BODY_NONE
	);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttp1serverresponse* pResponse = NULL;
	xfuture* pFuture = NULL;
	bool bAgain = false;
	bool bPass = false;
	size_t iGuard = 0;

	if ( (pBody != NULL) && (pReply != NULL) &&
		xrtHttpReplySetBody(pReply, pBody) ) {
		pResponse = xrtHttp1ServerResponsePrepare(
			XHTTP_VERSION_1_1,
			XRT_STR_LITERAL("GET"),
			0,
			pReply
		);
	}
	while ( (pResponse != NULL) &&
		(iGuard++ < 32) ) {
		xbytesview Data;
		xhttp1serveroutputstatus Status =
			xrtHttp1ServerResponseOutput(
				pResponse, 4096, &Data
			);

		if ( Status == XHTTP1_SERVER_OUTPUT_AGAIN ) {
			bAgain = true;
			break;
		}
		if ( (Status != XHTTP1_SERVER_OUTPUT_DATA) ||
			!xrtHttp1ServerResponseOutputConsume(
				pResponse, Data.Size
			) ) {
			break;
		}
	}
	if ( bAgain ) {
		pFuture = xrtHttp1ServerResponseWait(
			pResponse
		);
		bPass = (pFuture != NULL) &&
			(xrtFutureWait(pFuture) == XWAIT_OK);
		iGuard = 0;
		while ( bPass && (iGuard++ < 8) ) {
			xbytesview Data;
			xhttp1serveroutputstatus Status =
				xrtHttp1ServerResponseOutput(
					pResponse, 16, &Data
				);

			if ( Status == XHTTP1_SERVER_OUTPUT_DONE ) {
				break;
			}
			if ( (Status != XHTTP1_SERVER_OUTPUT_DATA) ||
				!xrtHttp1ServerResponseOutputConsume(
					pResponse, Data.Size
				) ) {
				bPass = false;
				break;
			}
		}
		bPass = bPass &&
			xrtHttp1ServerResponseComplete(pResponse);
	}
	xrtFutureDestroy(pFuture);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);
	xrtHttpBodyDestroy(pBody);
	return bPass ? 0 : 1;
}
