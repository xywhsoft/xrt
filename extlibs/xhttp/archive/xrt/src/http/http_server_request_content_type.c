#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE)

/* 解析请求唯一 Content-Type，结果借用拥有型请求快照。 */
XRT_API xhttpnext xrtHttpServerRequestContentType(
	const xhttpserverrequest* pRequest,
	xmediatype* pType
)
{
	const xhttpfield* pField = NULL;
	xmediatype Type = { 0 };
	xhttpnext Next;

	if ( (pRequest == NULL) ||
		!__xrtHttpServerRequestOutputValid(
			pRequest, pType, sizeof(Type)
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-content-type",
			"HTTP server request or media type output is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pType, &Type, sizeof(Type));
	Next = xrtHttpFieldGetUnique(
		pRequest->Fields,
		pRequest->FieldCount,
		XRT_STR_LITERAL("Content-Type"),
		&pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		if ( Next == XHTTP_NEXT_ERROR ) {
			xerror* pCause = xrtTakeError();

			__xrtHttpServerRequestSetError(
				XERR_PROTOCOL,
				XHTTP_SERVER_REQUEST_ERROR_HEADER,
				"parse-http-server-content-type",
				"HTTP request contains multiple Content-Type fields",
				pCause
			);
			xrtErrorFree(pCause);
		}
		return Next;
	}
	if ( !xrtHttpMediaTypeParse(pField->Value, &Type) ) {
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE,
			"parse-http-server-content-type",
			"HTTP request Content-Type is invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pType, &Type, sizeof(Type));
	return XHTTP_NEXT_ITEM;
}

#endif
