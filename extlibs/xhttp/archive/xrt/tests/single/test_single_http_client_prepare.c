#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头发布包含请求准备和正文保留能力。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/api")
	);
	xhttp1requestplan* pPlan;

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
	if ( (pPlan == NULL) ||
		(xrtHttp1RequestPlanBodyMode(pPlan) !=
		 XHTTP_REQUEST_BODY_FIXED) ||
		(xrtHttp1RequestPlanBodyLength(pPlan) != 2) ||
		(xrtHttp1RequestPlanHead(pPlan).Size == 0) ) {
		xrtHttp1RequestPlanDestroy(pPlan);
		return 2;
	}
	xrtHttp1RequestPlanDestroy(pPlan);
	return 0;
}
