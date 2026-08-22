#include "../internal/xrt_http.h"

#include <xrt/http_trailer.h>



#if defined(XRT_FEATURE_HTTP_TRAILER)

/* RFC 9110 禁止把分帧、路由、控制、认证或正文解释所需字段放入 trailer。 */
static const cstr __xrtHttpForbiddenTrailers[] = {
	"age",
	"authorization",
	"cache-control",
	"connection",
	"content-encoding",
	"content-language",
	"content-length",
	"content-location",
	"content-range",
	"content-type",
	"cookie",
	"date",
	"expect",
	"expires",
	"host",
	"if-match",
	"if-modified-since",
	"if-none-match",
	"if-range",
	"if-unmodified-since",
	"keep-alive",
	"location",
	"max-forwards",
	"pragma",
	"proxy-authenticate",
	"proxy-authorization",
	"proxy-connection",
	"range",
	"retry-after",
	"set-cookie",
	"te",
	"trailer",
	"transfer-encoding",
	"upgrade",
	"vary",
	"warning",
	"www-authenticate"
};



/* 单遍验证全部 Trailer 声明，并可同时统计或查找字段名。 */
static bool __xrtHttpTrailerScan(
	const xhttpfield* pFields,
	size_t iCount,
	const xstrview* pExpected,
	size_t* pNameCount,
	bool* pFound
)
{
	xhttpfieldtokencursor Cursor;
	xstrview Name;
	xhttpnext Next;
	size_t iNames = 0;
	bool bFound = false;

	xrtHttpFieldTokenCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpFieldTokenNext(
			pFields,
			iCount,
			XRT_STR_LITERAL("Trailer"),
			&Cursor,
			&Name
		);
		if ( Next == XHTTP_NEXT_END ) {
			*pNameCount = iNames;
			if ( pFound != NULL ) {
				*pFound = bFound;
			}
			return true;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( !xrtHttpTrailerNameValid(Name) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( iNames == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iNames++;
		if ( (pExpected != NULL) &&
			xrtHttpFieldNameEqual(Name, *pExpected) ) {
			bFound = true;
		}
	}
}



/* 读取字段名并判断它是否第一次出现。 */
static bool __xrtHttpTrailerNameFirst(
	const xhttpfield* pTrailers,
	size_t iIndex,
	xhttpfield* pTrailer
)
{
	xhttpfield Previous;
	size_t i;

	__xrtHttpFieldLoad(pTrailers, iIndex, pTrailer);
	for ( i = 0; i < iIndex; i++ ) {
		__xrtHttpFieldLoad(pTrailers, i, &Previous);
		if ( xrtHttpFieldNameEqual(
			Previous.Name, pTrailer->Name
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证实际 trailer 集合并测量去重后的声明长度。 */
static bool __xrtHttpTrailerNamesMeasure(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	size_t* pRequired
)
{
	xhttpfield Trailer;
	size_t iRequired = 0;
	size_t iNames = 0;
	size_t i;

	if ( !xrtHttpTrailerSectionValid(
		pTrailers, iTrailerCount
	) ) {
		return false;
	}
	for ( i = 0; i < iTrailerCount; i++ ) {
		if ( !__xrtHttpTrailerNameFirst(
			pTrailers, i, &Trailer
		) ) {
			continue;
		}
		if ( ((iNames != 0) &&
			 !__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpSizeAdd(
				&iRequired, Trailer.Name.Size
			) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iNames++;
	}
	*pRequired = iRequired;
	return true;
}



/* 判断字段名是否可作为通用 HTTP trailer 发送。 */
XRT_API bool xrtHttpTrailerNameValid(xstrview Name)
{
	size_t i;

	if ( !xrtHttpTokenValid(Name) ) {
		return false;
	}
	for ( i = 0;
		i < (sizeof(__xrtHttpForbiddenTrailers) /
			sizeof(__xrtHttpForbiddenTrailers[0]));
		i++ ) {
		if ( xrtHttpFieldNameEqual(
			Name,
			(xstrview){
				__xrtHttpForbiddenTrailers[i],
				strlen(__xrtHttpForbiddenTrailers[i])
			}
		) ) {
			return false;
		}
	}
	return true;
}



/* 完整验证实际 trailer section 的字段名称和值。 */
XRT_API bool xrtHttpTrailerSectionValid(
	const xhttpfield* pTrailers,
	size_t iCount
)
{
	xhttpfield Trailer;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pTrailers, iCount) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pTrailers, i, &Trailer);
		if ( !xrtHttpTrailerNameValid(Trailer.Name) ||
			!xrtHttpFieldValueValid(Trailer.Value) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	return true;
}



/* 完整验证重复 Trailer 字段行并统计其中声明的名称。 */
XRT_API bool xrtHttpTrailerCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pNameCount
)
{
	size_t iNames = 0;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pNameCount, sizeof(iNames)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pNameCount, sizeof(iNames)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pNameCount, &iNames, sizeof(iNames));
	if ( !__xrtHttpTrailerScan(
		pFields, iCount, NULL, &iNames, NULL
	) ) {
		return false;
	}
	memcpy(pNameCount, &iNames, sizeof(iNames));
	return true;
}



/* 完整验证声明并单遍查找一个 trailer 字段名。 */
XRT_API xhttpnext xrtHttpTrailerFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	size_t iIgnored;
	bool bFound;

	if ( !xrtHttpTrailerNameValid(Name) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	if ( !__xrtHttpTrailerScan(
		pFields, iCount, &Name, &iIgnored, &bFound
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	return bFound ? XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}



/* 写出去重后的 Trailer 声明值。 */
XRT_API bool xrtHttpTrailerNamesWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpfield Trailer;
	bytes pBytes = (bytes)pOutput;
	size_t iRequired;
	size_t iPosition = 0;
	size_t iNames = 0;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpTrailerNamesMeasure(
		pTrailers, iTrailerCount, &iRequired
	) ) {
		return false;
	}
	if ( __xrtHttpFieldArrayOverlap(
		pTrailers, iTrailerCount,
		pSize, sizeof(iRequired)
	) || ((pOutput != NULL) &&
		(__xrtHttpFieldArrayOverlap(
			pTrailers, iTrailerCount,
			pOutput, iRequired
		 ) || __xrtRangesOverlap(
			pOutput, iRequired,
			pSize, sizeof(iRequired)
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	for ( i = 0; i < iTrailerCount; i++ ) {
		if ( !__xrtHttpTrailerNameFirst(
			pTrailers, i, &Trailer
		) ) {
			continue;
		}
		if ( iNames != 0 ) {
			memcpy(pBytes + iPosition, ", ", 2u);
			iPosition += 2u;
		}
		memcpy(
			pBytes + iPosition,
			Trailer.Name.Data,
			Trailer.Name.Size
		);
		iPosition += Trailer.Name.Size;
		iNames++;
	}
	return true;
}



/* 以一次精确分配构建零结尾的 Trailer 声明值。 */
XRT_API str xrtHttpTrailerNamesBuild(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	size_t* pSize
)
{
	size_t iRequired;
	str sOutput;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		 !__xrtHttpFieldArrayValid(
			pTrailers, iTrailerCount
		 ) || __xrtHttpFieldArrayOverlap(
			pTrailers, iTrailerCount,
			pSize, sizeof(*pSize)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpTrailerNamesWrite(
		pTrailers, iTrailerCount,
		NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpTrailerNamesWrite(
		pTrailers, iTrailerCount,
		sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = 0;
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}

#endif
