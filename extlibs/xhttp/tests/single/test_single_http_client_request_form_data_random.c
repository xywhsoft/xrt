#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件请求能够生成并发布安全随机 FormData boundary。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	bool bPass = (pRequest != NULL) && (pForm != NULL) &&
		xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("value")
		) && xrtHttpRequestSetFormDataRandom(
			pRequest,
			pForm,
			&Boundary
		) && (Boundary.Size == 45u);

	xrtFormDataDestroy(pForm);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

