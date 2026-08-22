#include "../test.h"



/* 异步 Exchange 测试来源先返回 AGAIN，再发布固定正文。 */
typedef struct test_http_exchange_async {
	size_t Step;
	size_t Nexts;
	size_t Closes;
	bool WaitFails;
} test_http_exchange_async;



/* 静态正文租约不需要真实回收。 */
static void testHttpExchangeAsyncRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 第一次等待，Future 完成后发布正文，最后正常结束。 */
static xhttpbodystatus testHttpExchangeAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_exchange_async* pState =
		(test_http_exchange_async*)pContext;

	(void)iMaxBytes;
	pState->Nexts++;
	if ( pState->Step == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pState->Step == 1 ) {
		pState->Step = 2;
		pChunk->Data = (cbytes)"ready";
		pChunk->Size = 5;
		pChunk->Release = testHttpExchangeAsyncRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 创建立即完成的 Future，或发布一个可识别的来源错误。 */
static xfuture* testHttpExchangeAsyncWait(ptr pContext)
{
	test_http_exchange_async* pState =
		(test_http_exchange_async*)pContext;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	if ( pState->WaitFails ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.exchange.wait",
			81,
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



/* 记录 Exchange 最终关闭 Reader 的次数。 */
static void testHttpExchangeAsyncClose(ptr pContext)
{
	test_http_exchange_async* pState =
		(test_http_exchange_async*)pContext;

	pState->Closes++;
}



/* 打开共享测试状态对应的单个 Reader。 */
static bool testHttpExchangeAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpExchangeAsyncNext;
	pOps->Close = testHttpExchangeAsyncClose;
	pOps->Wait = testHttpExchangeAsyncWait;
	*ppReader = pFactory;
	return true;
}



/* 创建带固定长度异步正文的请求 Exchange。 */
static xhttp1exchange* testHttpExchangeAsyncCreate(
	test_http_exchange_async* pState
)
{
	static const xhttpbodyops Ops = {
		testHttpExchangeAsyncOpen,
		NULL
	};
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/async")
	);
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops, pState, 5, XHTTP_BODY_REPLAYABLE
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;

	if ( (pRequest == NULL) || (pBody == NULL) ||
		!xrtHttpRequestSetBody(pRequest, pBody) ) {
		xrtHttpBodyDestroy(pBody);
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	xrtHttpBodyDestroy(pBody);
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, NULL, NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 验证 AGAIN、Future、恢复输出和来源错误原因链。 */
int main(void)
{
	test_http_exchange_async Ready = { 0 };
	test_http_exchange_async Failed = { 0, 0, 0, true };
	xhttp1exchange* pExchange =
		testHttpExchangeAsyncCreate(&Ready);
	xhttp1exchange* pFailed =
		testHttpExchangeAsyncCreate(&Failed);
	xbytesview Data;
	xfuture* pFuture;
	const xerror* pError;

	testRequire((pExchange != NULL) &&
		(pFailed != NULL),
		"async HTTP exchange create failed");
	testRequire(xrtHttp1ExchangeOutputWait(
		pExchange
	) == NULL, "HTTP exchange wait succeeded before AGAIN");
	xrtClearError();
	testRequire((xrtHttp1ExchangeOutput(
		pExchange, 4096, &Data
	) == XHTTP1_OUTPUT_DATA) &&
		xrtHttp1ExchangeOutputConsume(
			pExchange, Data.Size
		) &&
		(xrtHttp1ExchangeOutput(
			pExchange, 16, &Data
		) == XHTTP1_OUTPUT_AGAIN) &&
		(Ready.Nexts == 1),
		"async HTTP exchange did not reach AGAIN");
	pFuture = xrtHttp1ExchangeOutputWait(pExchange);
	testRequire((pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK),
		"async HTTP exchange wait Future failed");
	xrtFutureDestroy(pFuture);
	testRequire((xrtHttp1ExchangeOutput(
		pExchange, 16, &Data
	) == XHTTP1_OUTPUT_DATA) &&
		(Data.Size == 5) &&
		(memcmp(Data.Data, "ready", 5) == 0) &&
		xrtHttp1ExchangeOutputConsume(
			pExchange, Data.Size
		) &&
		(xrtHttp1ExchangeOutput(
			pExchange, 16, &Data
		) == XHTTP1_OUTPUT_DONE) &&
		(Ready.Closes == 1),
		"async HTTP exchange output resume mismatch");

	testRequire((xrtHttp1ExchangeOutput(
		pFailed, 4096, &Data
	) == XHTTP1_OUTPUT_DATA) &&
		xrtHttp1ExchangeOutputConsume(
			pFailed, Data.Size
		) &&
		(xrtHttp1ExchangeOutput(
			pFailed, 16, &Data
		) == XHTTP1_OUTPUT_AGAIN) &&
		(xrtHttp1ExchangeOutputWait(pFailed) == NULL),
		"async HTTP exchange wait failure was not terminal");
	pError = xrtHttp1ExchangeError(pFailed);
	testRequire((pError != NULL) &&
		(xrtErrorCode(pError) ==
		 XHTTP1_EXCHANGE_ERROR_REQUEST_BODY) &&
		(xrtErrorCause(pError) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(pError)),
			"test.http.exchange.wait"
		) == 0),
		"async HTTP exchange wait cause mismatch");

	xrtHttp1ExchangeDestroy(pFailed);
	xrtHttp1ExchangeDestroy(pExchange);
	xrtClearError();
	printf("[PASS] async HTTP client exchange\n");
	return 0;
}
