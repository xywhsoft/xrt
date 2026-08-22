#include "../internal/xrt_http_server.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH_BEARER)

/* 使用指定字段名读取 Bearer token。 */
static xhttpnext __xrtHttpServerRequestBearerAuth(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	xstrview* pToken
)
{
	const xhttpfield* pField;
	xstrview Token = { NULL, 0 };
	xhttpnext Next;

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pToken, sizeof(Token)
	) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-bearer-auth",
			"HTTP Bearer token output is null",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pToken, &Token, sizeof(Token));
	Next = __xrtHttpServerRequestAuthField(
		pRequest,
		Name,
		&pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpBearerRead(pField->Value, &Token) ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_AUTH,
			"parse-http-server-bearer-auth",
			"HTTP request Bearer credentials are invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pToken, &Token, sizeof(Token));
	return XHTTP_NEXT_ITEM;
}



/* 解析源站 Bearer Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestBearerAuth(
	const xhttpserverrequest* pRequest,
	xstrview* pToken
)
{
	return __xrtHttpServerRequestBearerAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		pToken
	);
}



/* 解析代理 Bearer Proxy-Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestProxyBearerAuth(
	const xhttpserverrequest* pRequest,
	xstrview* pToken
)
{
	return __xrtHttpServerRequestBearerAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		pToken
	);
}

#endif
