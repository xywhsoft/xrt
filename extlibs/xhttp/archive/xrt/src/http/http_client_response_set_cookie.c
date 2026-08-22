#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_SET_COOKIE)

/* 按线路字段顺序解析下一条独立 Set-Cookie。 */
XRT_API xhttpnext xrtHttpResponseSetCookieNext(
	const xhttpresponse* pResponse,
	size_t* pHeaderIndex,
	xsetcookie* pCookie
)
{
	const xhttpheaders* pHeaders;
	xsetcookie Empty = { 0 };
	size_t iCount;
	size_t i;

	if ( !__xrtHttpResponseOutputValid(
			pResponse, pHeaderIndex, sizeof(i)
		) || !__xrtHttpResponseOutputValid(
			pResponse, pCookie, sizeof(*pCookie)
		) || __xrtRangesOverlap(
			pHeaderIndex,
			sizeof(i),
			pCookie,
			sizeof(*pCookie)
		) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"parse-http-response-set-cookie",
			"HTTP response, Header index or Set-Cookie output is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	iCount = xrtHttpHeadersCount(pHeaders);
	memcpy(&i, pHeaderIndex, sizeof(i));
	memcpy(pCookie, &Empty, sizeof(Empty));
	if ( i > iCount ) {
		__xrtHttpResponseSetError(
			XERR_RANGE,
			XHTTP_RESPONSE_ERROR_INDEX,
			"parse-http-response-set-cookie",
			"HTTP response Header index is out of range",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	while ( i < iCount ) {
		const xhttpfield* pField = xrtHttpResponseHeaderAt(
			pResponse,
			i
		);
		xsetcookie Cookie;

		if ( (pField == NULL) || !xrtHttpFieldNameEqual(
			pField->Name,
			XRT_STR_LITERAL("Set-Cookie")
		) ) {
			i++;
			continue;
		}
		if ( !xrtSetCookieParse(pField->Value, &Cookie) ) {
			memcpy(pHeaderIndex, &i, sizeof(i));
			__xrtHttpResponseWrapError(
				XERR_VALUE,
				XHTTP_RESPONSE_ERROR_SET_COOKIE,
				"parse-http-response-set-cookie",
				"HTTP response Set-Cookie field is invalid"
			);
			return XHTTP_NEXT_ERROR;
		}
		i++;
		memcpy(pHeaderIndex, &i, sizeof(i));
		memcpy(pCookie, &Cookie, sizeof(Cookie));
		return XHTTP_NEXT_ITEM;
	}
	memcpy(pHeaderIndex, &iCount, sizeof(iCount));
	return XHTTP_NEXT_END;
}

#endif
