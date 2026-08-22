#include <stdio.h>
#include <xhttp.h>



/* 演示自定义传输如何逐段取得 HTTP/1 请求线路。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.com/api")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xbytesview Data;
	xhttp1outputstatus Status;

	if ( (pRequest == NULL) ||
		!xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){ (cbytes)"{}", 2 },
			XRT_STR_LITERAL("application/json")
		) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return 1;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, NULL, NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
		return 1;
	}
	while ( (Status = xrtHttp1ExchangeOutput(
		pExchange, 4096, &Data
	)) == XHTTP1_OUTPUT_DATA ) {
		fwrite(Data.Data, 1, Data.Size, stdout);
		if ( !xrtHttp1ExchangeOutputConsume(
			pExchange, Data.Size
		) ) {
			xrtHttp1ExchangeDestroy(pExchange);
			return 1;
		}
	}
	xrtHttp1ExchangeDestroy(pExchange);
	return Status == XHTTP1_OUTPUT_DONE ? 0 : 1;
}

