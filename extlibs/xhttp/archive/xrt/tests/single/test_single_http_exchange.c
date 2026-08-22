#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头发布包含完整 HTTP/1 Exchange 路径。 */
int main(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xbytesview Output;
	size_t iAccepted;

	if ( pRequest == NULL ) {
		return 1;
	}
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
	while ( xrtHttp1ExchangeOutput(
		pExchange, 1024, &Output
	) == XHTTP1_OUTPUT_DATA ) {
		if ( !xrtHttp1ExchangeOutputConsume(
			pExchange, Output.Size
		) ) {
			xrtHttp1ExchangeDestroy(pExchange);
			return 4;
		}
	}
	if ( xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){
			Response,
			sizeof(Response) - 1u
		},
		false,
		&iAccepted
	) != XHTTP1_FEED_DONE ) {
		xrtHttp1ExchangeDestroy(pExchange);
		return 5;
	}
	if ( (xrtHttpResponseBody(
		xrtHttp1ExchangeResponse(pExchange)
	).Size != 2) || (iAccepted !=
		(sizeof(Response) - 1u)) ) {
		xrtHttp1ExchangeDestroy(pExchange);
		return 6;
	}
	xrtHttp1ExchangeDestroy(pExchange);
	return 0;
}
