#include "../internal/xrt_http_server.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH)

/* 读取唯一认证字段，并把重复字段包装到服务端请求错误域。 */
xhttpnext __xrtHttpServerRequestAuthField(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	const xhttpfield** ppField
)
{
	const xhttpfield* pField = NULL;
	xhttpnext Next;

	if ( ppField != NULL ) {
		memcpy(ppField, &pField, sizeof(pField));
	}
	if ( (pRequest == NULL) || (ppField == NULL) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-auth",
			"HTTP server request or authentication output is null",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = xrtHttpFieldGetUnique(
		pRequest->Fields,
		pRequest->FieldCount,
		Name,
		&pField
	);
	memcpy(ppField, &pField, sizeof(pField));
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtHttpServerRequestWrapError(
			XERR_PROTOCOL,
			XHTTP_SERVER_REQUEST_ERROR_HEADER,
			"parse-http-server-auth",
			"HTTP request contains multiple authentication fields"
		);
	}
	return Next;
}



/* 使用指定字段名解析通用认证凭据。 */
static xhttpnext __xrtHttpServerRequestAuth(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	xhttpauth* pAuth
)
{
	xhttpauth Auth = { 0 };
	const xhttpfield* pField;
	xhttpnext Next;

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pAuth, sizeof(Auth)
	) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-auth",
			"HTTP authentication output is null",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	Next = __xrtHttpServerRequestAuthField(
		pRequest,
		Name,
		&pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpAuthParse(pField->Value, &Auth) ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_AUTH,
			"parse-http-server-auth",
			"HTTP request authentication credentials are invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	return XHTTP_NEXT_ITEM;
}



/* 解析源站 Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestAuth(
	const xhttpserverrequest* pRequest,
	xhttpauth* pAuth
)
{
	return __xrtHttpServerRequestAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		pAuth
	);
}



/* 解析代理 Proxy-Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestProxyAuth(
	const xhttpserverrequest* pRequest,
	xhttpauth* pAuth
)
{
	return __xrtHttpServerRequestAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		pAuth
	);
}

#endif
