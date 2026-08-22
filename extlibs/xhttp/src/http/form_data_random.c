#include "../internal/xrt_form_data.h"



#if defined(XHTTP_FEATURE_FORM_DATA_RANDOM)

/* 生成安全随机 boundary，并创建对应 multipart/form-data 正文。 */
XRT_API xhttpbody* xrtFormDataBodyRandom(
	const xformdata* pForm,
	xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;
	xhttpbody* pBody;

	if ( (pForm == NULL) || (pBoundary == NULL) ||
		!__xrtFormDataOutputValid(
			pForm, pBoundary, sizeof(*pBoundary)
		) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT,
			XFORM_DATA_ERROR_ARGUMENT,
			"encode",
			"random FormData output overlaps its container"
		);
		return NULL;
	}
	if ( !xrtMultipartBoundaryRandom(&Boundary) ) {
		return NULL;
	}
	pBody = xrtFormDataBody(pForm, &Boundary);
	if ( pBody == NULL ) {
		return NULL;
	}
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	return pBody;
}

#endif

