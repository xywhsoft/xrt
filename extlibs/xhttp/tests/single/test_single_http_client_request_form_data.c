#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_FORM_DATA
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件请求能够接管给定 boundary 的流式 FormData 正文。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	const xhttpfield* pType;
	bool bPass = (pRequest != NULL) && (pForm != NULL) &&
		xrtMultipartBoundaryParse(
			XRT_STR_LITERAL("xrt-single"),
			&Boundary
		) && xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("value")
		) && xrtHttpRequestSetFormData(
			pRequest,
			pForm,
			&Boundary
		);

	pType = bPass ? xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	) : NULL;
	bPass = bPass && (pType != NULL) &&
		(xrtHttpRequestBody(pRequest) != NULL);
	xrtFormDataDestroy(pForm);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

