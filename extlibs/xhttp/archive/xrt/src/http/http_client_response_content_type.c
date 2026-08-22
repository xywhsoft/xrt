#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_CONTENT_TYPE)

/* 解析客户端响应中的唯一 Content-Type 字段。 */
XRT_API xhttpnext xrtHttpResponseContentType(
	const xhttpresponse* pResponse,
	xmediatype* pType
)
{
	const xhttpheaders* pHeaders;
	const xhttpfield* pField = NULL;
	xmediatype Empty = { 0 };
	xmediatype Type;
	xhttpnext Next;

	if ( !__xrtHttpResponseOutputValid(
		pResponse, pType, sizeof(Type)
	) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"parse-http-response-content-type",
			"HTTP response or media type output is null",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pType, &Empty, sizeof(Empty));
	pHeaders = xrtHttpResponseHeaders(pResponse);
	Next = xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("Content-Type"),
		&pField
	);
	if ( Next == XHTTP_NEXT_END ) {
		return XHTTP_NEXT_END;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		xerror* pCause = xrtTakeError();

		__xrtHttpResponseSetError(
			XERR_PROTOCOL,
			XHTTP_RESPONSE_ERROR_HEADER,
			"parse-http-response-content-type",
			"HTTP response contains multiple Content-Type fields",
			pCause
		);
		xrtErrorFree(pCause);
		return XHTTP_NEXT_ERROR;
	}
	if ( !xrtHttpMediaTypeParse(pField->Value, &Type) ) {
		__xrtHttpResponseWrapError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_CONTENT_TYPE,
			"parse-http-response-content-type",
			"HTTP response Content-Type is invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pType, &Type, sizeof(Type));
	return XHTTP_NEXT_ITEM;
}

#endif
