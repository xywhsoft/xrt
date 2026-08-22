#include "../test.h"

#include <xrt/http_client.h>
#include <xrt/http_connection.h>
#include <xrt/http_te.h>



/* 验证一步式 Trailer 声明可直接通过 HTTP/1 准备并在线路上重解析。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/events")
	);
	xhttp1requestplan* pPlan;
	xhttpfield Fields[8];
	xhttp1errorinfo Error;
	xhttp1head Head;
	xhttpteinfo Te;
	xbytesview Wire;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("gzip;q=0.5")
		) && xrtHttp1RequestAcceptTrailers(pRequest),
		"HTTP request TE HTTP/1 setup failed"
	);
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan != NULL,
		"HTTP request TE fields failed HTTP/1 preparation");
	Wire = xrtHttp1RequestPlanHead(pPlan);
	xrtHttp1HeadInit(&Head, Fields, 8u);
	testRequire(
		(xrtHttp1RequestParse(
			Wire, &Head, NULL, &Error
		) == XHTTP1_READY) &&
		xrtHttpTeParse(Head.Fields, Head.FieldCount, &Te) &&
		((Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
		((Te.Flags & XHTTP_TE_HAS_TRANSFER_CODINGS) != 0) &&
		(xrtHttpConnectionFind(
			Head.Fields,
			Head.FieldCount,
			XRT_STR_LITERAL("TE")
		) == XHTTP_NEXT_ITEM),
		"HTTP request TE fields did not survive wire round-trip"
	);
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
	printf("[PASS] http_client_request_te_http1\n");
	return 0;
}
