#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_QUERY)

/* 从已经通过 HTTP/1 校验的 request-target 创建拥有型查询参数。 */
XRT_API xqueryparams* xrtHttpServerRequestQueryParams(
	const xhttpserverrequest* pRequest,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
)
{
	xqueryparamsconfig Config;
	const xqueryparamsconfig* pResolvedConfig = NULL;
	xhttptarget Target;
	xqueryparams* pParams;
	size_t iError = 0;

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
			"parse-http-server-query",
			"HTTP server query config or error output is invalid",
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
	if ( pRequest == NULL ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-query",
			"HTTP server request is null",
			NULL
		);
		return NULL;
	}
	if ( !xrtHttpServerRequestParseTarget(
		pRequest, &Target
	) ) {
		__xrtHttpServerRequestWrapError(
			XERR_PROTOCOL,
			XHTTP_SERVER_REQUEST_ERROR_TARGET,
			"parse-http-server-query",
			"HTTP request-target could not be parsed"
		);
		return NULL;
	}
	pParams = xrtQueryParamsParse(
		Target.Uri.Query,
		pResolvedConfig,
		&iError
	);
	if ( pErrorOffset != NULL ) {
		memcpy(pErrorOffset, &iError, sizeof(iError));
	}
	if ( pParams == NULL ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_QUERY,
			"parse-http-server-query",
			"HTTP request query could not be decoded"
		);
	}
	return pParams;
}

#endif
