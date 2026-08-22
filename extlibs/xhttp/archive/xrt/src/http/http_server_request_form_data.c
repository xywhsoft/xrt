#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_FORM_DATA)

/* 解析完整缓冲的 multipart/form-data 请求正文。 */
XRT_API xformdata* xrtHttpServerRequestFormData(
	const xhttpserverrequest* pRequest,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	xbytesview Body;
	xformdataconfig Config;
	const xformdataconfig* pResolvedConfig = NULL;
	const xhttpfield* pContentType;
	xmultiparterrorinfo Error = { 0 };
	xformdata* pForm;
	xmediatype Type;
	xmultipartlimits Limits;
	const xmultipartlimits* pResolvedLimits = NULL;
	xhttpnext Next;

	if ( ((pConfig != NULL) && !__xrtRangeValid(
		pConfig, sizeof(Config)
	)) || ((pLimits != NULL) && !__xrtRangeValid(
		pLimits, sizeof(Limits)
	)) || ((pError != NULL) &&
		(!__xrtHttpServerRequestOutputValid(
			pRequest, pError, sizeof(Error)
		) || ((pConfig != NULL) && __xrtRangesOverlap(
			pError, sizeof(Error), pConfig, sizeof(Config)
		)) || ((pLimits != NULL) && __xrtRangesOverlap(
			pError, sizeof(Error), pLimits, sizeof(Limits)
		)))) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-form-data",
			"HTTP FormData config or error output is invalid",
			NULL
		);
		return NULL;
	}
	if ( pConfig != NULL ) {
		memcpy(&Config, pConfig, sizeof(Config));
		pResolvedConfig = &Config;
	}
	if ( pLimits != NULL ) {
		memcpy(&Limits, pLimits, sizeof(Limits));
		pResolvedLimits = &Limits;
	}
	if ( pError != NULL ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	Next = xrtHttpServerRequestContentType(
		pRequest,
		&Type
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return NULL;
	}
	if ( (Next != XHTTP_NEXT_ITEM) ||
		!xrtHttpMediaTypeEqual(
			&Type,
			XRT_STR_LITERAL("multipart"),
			XRT_STR_LITERAL("form-data")
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE,
			"parse-http-server-form-data",
			"HTTP request is not multipart/form-data",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpServerRequestBufferedBody(
		pRequest,
		&Body,
		"parse-http-server-form-data"
	) ) {
		return NULL;
	}
	pContentType = xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	);
	if ( pContentType == NULL ) {
		__xrtHttpServerRequestSetError(
			XERR_INTERNAL,
			XHTTP_SERVER_REQUEST_ERROR_HEADER,
			"parse-http-server-form-data",
			"validated Content-Type field is unavailable",
			NULL
		);
		return NULL;
	}
	pForm = xrtFormDataParseContentType(
		pContentType->Value,
		Body,
		pResolvedConfig,
		pResolvedLimits,
		&Error
	);
	if ( pError != NULL ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	if ( pForm == NULL ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_FORM,
			"parse-http-server-form-data",
			"HTTP multipart form body could not be decoded"
		);
	}
	return pForm;
}

#endif
