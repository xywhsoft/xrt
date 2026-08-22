#include "../internal/xrt_http_client.h"
#include "../internal/xrt_form_data.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM)

/* 生成安全随机 boundary 与组合正文，并只在完整提交后发布 boundary。 */
XRT_API bool xrtHttpRequestSetFormDataRandom(
	xhttprequest* pRequest,
	const xformdata* pForm,
	xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;
	xhttpbody* pBody;

	if ( (pRequest == NULL) || (pForm == NULL) ||
		!__xrtHttpRequestOutputValid(
			pRequest, pBoundary, sizeof(Boundary)
		) || !__xrtFormDataOutputValid(
			pForm, pBoundary, sizeof(Boundary)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pBody = xrtFormDataBodyRandom(pForm, &Boundary);
	if ( pBody == NULL ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM_DATA,
			"set-form-data",
			"random multipart/form-data body could not be created"
		);
		return false;
	}
	if ( !__xrtHttpRequestCommitFormData(
		pRequest,
		pBody,
		&Boundary
	) ) {
		return false;
	}
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	return true;
}

#endif
