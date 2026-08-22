#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_EXCHANGE_ASYNC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头异步来源先等待，再结束空正文。 */
typedef struct test_single_http_exchange_async {
	bool Ready;
} test_single_http_exchange_async;



/* 未就绪时返回 AGAIN，Future 完成后返回 EOF。 */
static xhttpbodystatus testSingleHttpExchangeAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_single_http_exchange_async* pState =
		(test_single_http_exchange_async*)pContext;

	(void)iMaxBytes;
	(void)pChunk;
	return pState->Ready ?
		XHTTP_BODY_EOF : XHTTP_BODY_AGAIN;
}



/* 创建立即完成的正文可读 Future。 */
static xfuture* testSingleHttpExchangeAsyncWait(
	ptr pContext
)
{
	test_single_http_exchange_async* pState =
		(test_single_http_exchange_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(
		&pFuture, NULL
	);

	if ( pPromise == NULL ) {
		return NULL;
	}
	pState->Ready = true;
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 打开单头测试 Reader。 */
static bool testSingleHttpExchangeAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testSingleHttpExchangeAsyncNext;
	pOps->Wait = testSingleHttpExchangeAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 验证单头文件包含 Exchange 异步正文桥。 */
int main(void)
{
	static const xhttpbodyops Ops = {
		testSingleHttpExchangeAsyncOpen,
		NULL
	};
	test_single_http_exchange_async State = { false };
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/")
	);
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops, &State, 0, XHTTP_BODY_REPLAYABLE
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xbytesview Data;
	xfuture* pFuture;
	bool bPass = false;

	if ( (pRequest == NULL) || (pBody == NULL) ||
		!xrtHttpRequestSetBody(pRequest, pBody) ) {
		xrtHttpBodyDestroy(pBody);
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	xrtHttpBodyDestroy(pBody);
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return 2;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, NULL, NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
		return 3;
	}
	if ( (xrtHttp1ExchangeOutput(
		pExchange, 4096, &Data
	) == XHTTP1_OUTPUT_DATA) &&
		xrtHttp1ExchangeOutputConsume(
			pExchange, Data.Size
		) &&
		(xrtHttp1ExchangeOutput(
			pExchange, 1, &Data
		) == XHTTP1_OUTPUT_AGAIN) ) {
		pFuture = xrtHttp1ExchangeOutputWait(pExchange);
		if ( pFuture != NULL ) {
			bPass = (xrtFutureWait(pFuture) == XWAIT_OK) &&
				(xrtHttp1ExchangeOutput(
					pExchange, 1, &Data
				) == XHTTP1_OUTPUT_DONE);
			xrtFutureDestroy(pFuture);
		}
	}
	xrtHttp1ExchangeDestroy(pExchange);
	return bPass ? 0 : 4;
}

