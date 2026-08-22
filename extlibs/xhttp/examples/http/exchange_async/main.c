#include <stdio.h>

#include <xhttp.h>

#include "../common/async_body.h"



/* 等待 Future 后继续拉取请求正文，直到完整输出请求。 */
int main(void)
{
	example_http_async_body Body;
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.com/upload")
	);
	xhttpbody* pBody = exampleHttpAsyncBodyCreate(
		&Body,
		XRT_BYTES_LITERAL("hello")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xbytesview Data;
	xhttp1outputstatus Status;
	xfuture* pFuture;

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
		return 1;
	}
	pExchange = xrtHttp1ExchangeCreate(pPlan, NULL, NULL);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
		return 1;
	}
	for ( ;; ) {
		Status = xrtHttp1ExchangeOutput(
			pExchange,
			16u * 1024u,
			&Data
		);
		if ( Status == XHTTP1_OUTPUT_DATA ) {
			(void)fwrite(Data.Data, 1, Data.Size, stdout);
			if ( !xrtHttp1ExchangeOutputConsume(
				pExchange,
				Data.Size
			) ) {
				break;
			}
			continue;
		}
		if ( Status != XHTTP1_OUTPUT_AGAIN ) {
			break;
		}
		pFuture = xrtHttp1ExchangeOutputWait(pExchange);
		if ( pFuture == NULL ) {
			break;
		}
		if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
			xrtFutureDestroy(pFuture);
			break;
		}
		xrtFutureDestroy(pFuture);
	}
	xrtHttp1ExchangeDestroy(pExchange);
	return Status == XHTTP1_OUTPUT_DONE ? 0 : 1;
}

