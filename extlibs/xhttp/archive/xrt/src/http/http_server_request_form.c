#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_FORM)

/* 解析完整缓冲的 application/x-www-form-urlencoded 请求正文。 */
XRT_API xqueryparams* xrtHttpServerRequestForm(
	const xhttpserverrequest* pRequest,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
)
{
	xbytesview Body;
	xqueryparamsconfig Config;
	const xqueryparamsconfig* pResolvedConfig = NULL;
	xmediatype Type;
	xqueryparams* pParams;
	size_t iError = 0;
	xhttpnext Next;

	if ( ((pConfig != NULL) && !__xrtRangeValid(
		pConfig, sizeof(Config)
	)) || ((pErrorOffset != NULL) &&
		(!__xrtHttpServerRequestOutputValid(
			pRequest, pErrorOffset, sizeof(iError)
		) || ((pConfig != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(iError),
			pConfig, sizeof(Config)
		)))) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-form",
			"HTTP form config or error output is invalid",
			NULL
		);
		return NULL;
	}
	if ( pConfig != NULL ) {
		memcpy(&Config, pConfig, sizeof(Config));
		pResolvedConfig = &Config;
	}
	if ( pErrorOffset != NULL ) {
		memcpy(pErrorOffset, &iError, sizeof(iError));
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
			XRT_STR_LITERAL("application"),
			XRT_STR_LITERAL("x-www-form-urlencoded")
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE,
			"parse-http-server-form",
			"HTTP request is not application/x-www-form-urlencoded",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpServerRequestBufferedBody(
		pRequest,
		&Body,
		"parse-http-server-form"
	) ) {
		return NULL;
	}
	pParams = xrtQueryParamsParse(
		(xstrview){ (cstr)Body.Data, Body.Size },
		pResolvedConfig,
		&iError
	);
	if ( pErrorOffset != NULL ) {
		memcpy(pErrorOffset, &iError, sizeof(iError));
	}
	if ( pParams == NULL ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_FORM,
			"parse-http-server-form",
			"HTTP urlencoded form body could not be decoded"
		);
	}
	return pParams;
}

#endif
