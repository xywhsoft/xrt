#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_COOKIE)

/* 判断请求字段是否是 Cookie，字段名比较不区分大小写。 */
static bool __xrtHttpServerRequestIsCookie(
	const xhttpfield* pField
)
{
	return (pField != NULL) && xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Cookie")
	);
}



/* 严格验证完整 Cookie 字段集合并累计全部 pair 数量。 */
static bool __xrtHttpServerRequestCookiesCount(
	const xhttpserverrequest* pRequest,
	size_t* pRequired
)
{
	size_t iCount = xrtHttpServerRequestHeaderCount(pRequest);
	size_t iRequired = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iFieldCount;

		if ( !__xrtHttpServerRequestIsCookie(pField) ) {
			continue;
		}
		if ( !xrtCookieValidate(
			pField->Value,
			NULL,
			&iFieldCount
		) ) {
			__xrtHttpServerRequestWrapError(
				XERR_VALUE,
				XHTTP_SERVER_REQUEST_ERROR_HEADER,
				"find-http-server-cookie",
				"HTTP request contains an invalid Cookie field"
			);
			return false;
		}
		if ( iRequired > (SIZE_MAX - iFieldCount) ) {
			__xrtHttpServerRequestSetError(
				XERR_RANGE,
				XHTTP_SERVER_REQUEST_ERROR_HEADER,
				"parse-http-server-cookies",
				"HTTP request Cookie count overflows size_t",
				NULL
			);
			return false;
		}
		iRequired += iFieldCount;
	}
	if ( pRequired != NULL ) {
		memcpy(pRequired, &iRequired, sizeof(iRequired));
	}
	return true;
}



/* 一次性解析全部 Cookie 字段，避免多键查询反复扫描字段文本。 */
XRT_API bool xrtHttpServerRequestCookies(
	const xhttpserverrequest* pRequest,
	xcookiepair* pCookies,
	size_t iCapacity,
	size_t* pCount
)
{
	xcookiepair Cookie;
	size_t iOutputBytes;
	size_t iRequired;
	size_t iIndex = 0;
	size_t iFieldCount;
	size_t i;

	if ( iCapacity > (SIZE_MAX / sizeof(Cookie)) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-cookies",
			"HTTP server Cookie capacity overflows size_t",
			NULL
		);
		return false;
	}
	iOutputBytes = iCapacity * sizeof(Cookie);
	if ( (pRequest == NULL) ||
		!__xrtHttpServerRequestOutputValid(
			pRequest, pCookies, iOutputBytes
		) || !__xrtHttpServerRequestOutputValid(
			pRequest, pCount, sizeof(iRequired)
		) || __xrtRangesOverlap(
			pCookies, iOutputBytes,
			pCount, sizeof(iRequired)
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-cookies",
			"HTTP server request or Cookie output is invalid",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerRequestCookiesCount(
		pRequest, &iRequired
	) ) {
		return false;
	}
	if ( pCookies == NULL ) {
		memcpy(pCount, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pCount, &iRequired, sizeof(iRequired));
		__xrtHttpServerRequestSetError(
			XERR_RANGE,
			XHTTP_SERVER_REQUEST_ERROR_HEADER,
			"parse-http-server-cookies",
			"HTTP server Cookie output capacity is too small",
			NULL
		);
		return false;
	}
	iFieldCount = xrtHttpServerRequestHeaderCount(pRequest);
	for ( i = 0; i < iFieldCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;

		if ( !__xrtHttpServerRequestIsCookie(pField) ) {
			continue;
		}
		while ( iOffset < pField->Value.Size ) {
			if ( xrtCookieNext(
				pField->Value, &iOffset, &Cookie
			) != XCOOKIE_NEXT_ITEM ) {
				__xrtHttpServerRequestWrapError(
					XERR_INTERNAL,
					XHTTP_SERVER_REQUEST_ERROR_HEADER,
					"parse-http-server-cookies",
					"validated HTTP Cookie field changed during parsing"
				);
				return false;
			}
			memcpy(
				(uint8*)pCookies +
					(iIndex * sizeof(Cookie)),
				&Cookie,
				sizeof(Cookie)
			);
			iIndex++;
		}
	}
	memcpy(pCount, &iRequired, sizeof(iRequired));
	return true;
}



/* 在全部有效 Cookie 字段中按线路顺序查找首个同名项。 */
XRT_API xcookienext xrtHttpServerRequestCookie(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	xcookiepair* pCookie
)
{
	xcookiepair Cookie = { 0 };
	size_t iHeaderCount;
	size_t i;

	if ( (pRequest == NULL) || !xrtHttpTokenValid(Name) ||
		!__xrtHttpServerRequestOutputValid(
			pRequest, pCookie, sizeof(*pCookie)
		) || __xrtRangesOverlap(
			pCookie, sizeof(*pCookie), Name.Data, Name.Size
		) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"find-http-server-cookie",
			"HTTP server request, Cookie name or output is invalid",
			NULL
		);
		return XCOOKIE_NEXT_ERROR;
	}
	memcpy(pCookie, &Cookie, sizeof(Cookie));
	if ( !__xrtHttpServerRequestCookiesCount(
		pRequest, NULL
	) ) {
		return XCOOKIE_NEXT_ERROR;
	}
	iHeaderCount = xrtHttpServerRequestHeaderCount(pRequest);
	for ( i = 0; i < iHeaderCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);
		size_t iOffset = 0;

		if ( !__xrtHttpServerRequestIsCookie(pField) ) {
			continue;
		}
		while ( iOffset < pField->Value.Size ) {
			if ( xrtCookieNext(
				pField->Value, &iOffset, &Cookie
			) != XCOOKIE_NEXT_ITEM ) {
				__xrtHttpServerRequestWrapError(
					XERR_INTERNAL,
					XHTTP_SERVER_REQUEST_ERROR_HEADER,
					"find-http-server-cookie",
					"validated HTTP Cookie field changed during lookup"
				);
				return XCOOKIE_NEXT_ERROR;
			}
			if ( (Cookie.Name.Size == Name.Size) &&
				(memcmp(
					Cookie.Name.Data, Name.Data, Name.Size
				 ) == 0) ) {
				memcpy(pCookie, &Cookie, sizeof(Cookie));
				return XCOOKIE_NEXT_ITEM;
			}
		}
	}
	return XCOOKIE_NEXT_END;
}

#endif
