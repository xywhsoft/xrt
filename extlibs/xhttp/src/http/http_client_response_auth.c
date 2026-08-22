#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH)

/* 校验通用响应认证迭代参数，并把公开游标读取到对齐快照。 */
static bool __xrtHttpResponseChallengeNextValid(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge,
	const xhttpheaders** ppHeaders
)
{
	const xhttpheaders* pHeaders;
	size_t iCount;
	xhttpauthcursor Cursor;
	xhttpauth Empty = { 0 };

	if ( !__xrtHttpResponseOutputValid(
		pResponse, pCursor, sizeof(*pCursor)
	) || !__xrtHttpResponseOutputValid(
		pResponse, pChallenge, sizeof(*pChallenge)
	) ||
		__xrtRangesOverlap(
			pCursor, sizeof(*pCursor),
			pChallenge, sizeof(*pChallenge)
		) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"parse-http-response-auth",
			"HTTP response authentication iterator is invalid",
			NULL
		);
		return false;
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	iCount = xrtHttpHeadersCount(pHeaders);
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(pChallenge, &Empty, sizeof(Empty));
	if ( Cursor.FieldIndex > iCount ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_INDEX,
			"parse-http-response-auth",
			"HTTP response authentication cursor is out of range",
			NULL
		);
		return false;
	}
	*ppHeaders = pHeaders;
	return true;
}



/* 跨指定响应认证字段迭代 challenge，并统一响应错误域。 */
static xhttpnext __xrtHttpResponseChallengeNext(
	const xhttpresponse* pResponse,
	xstrview Name,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
)
{
	const xhttpheaders* pHeaders;
	xhttpnext Next;

	if ( !__xrtHttpResponseChallengeNextValid(
		pResponse, pCursor, pChallenge, &pHeaders
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	Next = xrtHttpFieldChallengeNext(
		xrtHttpHeadersData(pHeaders),
		xrtHttpHeadersCount(pHeaders),
		Name,
		pCursor,
		pChallenge
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtHttpResponseWrapError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_AUTH,
			"parse-http-response-auth",
			"HTTP response authentication challenge is invalid"
		);
	}
	return Next;
}



/* 校验专用 challenge 解码输出，并建立失败原子的空结果。 */
static bool __xrtHttpResponseChallengeReadPrepare(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge,
	size_t iChallengeSize,
	cstr sOperation
)
{
	const xhttpheaders* pHeaders;
	xhttpauthcursor Cursor;
	size_t iCount;
	size_t iZero = 0;

	if ( !__xrtHttpResponseOutputValid(
		pResponse, pCursor, sizeof(Cursor)
	) || !__xrtHttpResponseOutputValid(
		pResponse, pSize, sizeof(*pSize)
	) ||
		(iChallengeSize == 0) ||
		!__xrtHttpResponseOutputValid(
			pResponse, pChallenge, iChallengeSize
		) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtHttpResponseOutputValid(
			pResponse, pOutput, iCapacity
		 )) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pChallenge, iChallengeSize
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pChallenge, iChallengeSize
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pChallenge, iChallengeSize
		))) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			sOperation,
			"HTTP response authentication decode output is invalid",
			NULL
		);
		return false;
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	iCount = xrtHttpHeadersCount(pHeaders);
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(pSize, &iZero, sizeof(iZero));
	memset(pChallenge, 0, iChallengeSize);
	if ( Cursor.FieldIndex > iCount ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_INDEX,
			sOperation,
			"HTTP response authentication cursor is out of range",
			NULL
		);
		return false;
	}
	return true;
}



/* 从游标快照查找指定 scheme，并返回覆盖完整 challenge 的借用视图。 */
static xhttpnext __xrtHttpResponseChallengeFind(
	const xhttpresponse* pResponse,
	xstrview Name,
	xstrview Scheme,
	const xhttpauthcursor* pCursor,
	xhttpauthcursor* pNextCursor,
	xstrview* pValue
)
{
	xhttpauthcursor Cursor;
	xhttpauth Auth;
	xstrview Value = { 0 };
	xhttpnext Next;
	uintptr_t iStart;
	uintptr_t iEnd;

	memcpy(&Cursor, pCursor, sizeof(Cursor));
	for ( ;; ) {
		Next = __xrtHttpResponseChallengeNext(
			pResponse, Name, &Cursor, &Auth
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			if ( Next == XHTTP_NEXT_END ) {
				memcpy(pNextCursor, &Cursor, sizeof(Cursor));
				memcpy(pValue, &Value, sizeof(Value));
				return XHTTP_NEXT_END;
			}
			if ( Next != XHTTP_NEXT_ERROR ) {
				__xrtHttpResponseSetError(
					XERR_STATE,
					XHTTP_RESPONSE_ERROR_AUTH,
					"parse-http-response-auth",
					"HTTP authentication iterator returned an invalid state",
					NULL
				);
			}
			return XHTTP_NEXT_ERROR;
		}
		if ( !xrtHttpTokenEqual(Auth.Scheme, Scheme) ) {
			continue;
		}
		iStart = (uintptr_t)Auth.Scheme.Data;
		iEnd = (Auth.Kind == XHTTP_AUTH_NONE) ?
			(iStart + Auth.Scheme.Size) :
			((uintptr_t)Auth.Data.Data + Auth.Data.Size);
		if ( iEnd < iStart ) {
			__xrtHttpResponseSetError(
				XERR_STATE,
				XHTTP_RESPONSE_ERROR_AUTH,
				"parse-http-response-auth",
				"HTTP response authentication challenge range is invalid",
				NULL
			);
			return XHTTP_NEXT_ERROR;
		}
		Value = (xstrview){
			Auth.Scheme.Data,
			(size_t)(iEnd - iStart)
		};
		memcpy(pNextCursor, &Cursor, sizeof(Cursor));
		memcpy(pValue, &Value, sizeof(Value));
		return XHTTP_NEXT_ITEM;
	}
}



/* 查找并解码指定 scheme，只有实际写出成功或到达末尾才提交游标。 */
xhttpnext __xrtHttpResponseChallengeRead(
	const xhttpresponse* pResponse,
	xstrview Name,
	xstrview Scheme,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge,
	size_t iChallengeSize,
	__xrtHttpResponseChallengeReadFunction pRead,
	cstr sOperation,
	cstr sMessage
)
{
	xhttpauthcursor NextCursor;
	xstrview Value;
	xhttpnext Next;

	if ( pRead == NULL ) {
		__xrtHttpResponseSetError(
			XERR_STATE,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response authentication decoder is unavailable",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpResponseChallengeReadPrepare(
		pResponse,
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		iChallengeSize,
		sOperation
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	Next = __xrtHttpResponseChallengeFind(
		pResponse,
		Name,
		Scheme,
		pCursor,
		&NextCursor,
		&Value
	);
	if ( Next == XHTTP_NEXT_END ) {
		memcpy(pCursor, &NextCursor, sizeof(NextCursor));
		return XHTTP_NEXT_END;
	}
	if ( Next != XHTTP_NEXT_ITEM ) {
		return XHTTP_NEXT_ERROR;
	}
	if ( !pRead(
		Value,
		pOutput,
		iCapacity,
		pSize,
		pChallenge
	) ) {
		__xrtHttpResponseWrapError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			sMessage
		);
		return XHTTP_NEXT_ERROR;
	}
	if ( pOutput != NULL ) {
		memcpy(pCursor, &NextCursor, sizeof(NextCursor));
	}
	return XHTTP_NEXT_ITEM;
}



/* 迭代源站 WWW-Authenticate challenge。 */
XRT_API xhttpnext xrtHttpResponseChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
)
{
	return __xrtHttpResponseChallengeNext(
		pResponse,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pCursor,
		pChallenge
	);
}



/* 迭代代理 Proxy-Authenticate challenge。 */
XRT_API xhttpnext xrtHttpResponseProxyChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
)
{
	return __xrtHttpResponseChallengeNext(
		pResponse,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		pCursor,
		pChallenge
	);
}

#endif
