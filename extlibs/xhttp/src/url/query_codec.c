#include "../internal/xrt_query_codec.h"



#if defined(XHTTP_FEATURE_QUERY_CODEC)

/* 验证全部 Query 项并计算 RFC 3986 编码后的精确字节数。 */
static bool __xrtQueryCodecMeasure(
	const xquerypair* pPairs,
	size_t iCount,
	const xpercentmap* pSafe,
	size_t* pRequired
)
{
	size_t iRequired = 0;
	size_t i;

	if ( !__xrtQueryPairsValid(pPairs, iCount) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		size_t iKey;
		size_t iValue = 0;
		size_t iAdd;

		if ( !xrtPercentMeasure(
			pPairs[i].Key.Data,
			pPairs[i].Key.Size, pSafe, false, &iKey
		) ) {
			return false;
		}
		if ( (pPairs[i].Flags & XQUERY_HAS_VALUE) != 0 ) {
			if ( !xrtPercentMeasure(
				pPairs[i].Value.Data,
				pPairs[i].Value.Size, pSafe, false, &iValue
			) ) {
				return false;
			}
			if ( iKey == SIZE_MAX ) {
				xrtSetErrorKind(XERR_RANGE);
				return false;
			}
			iAdd = iKey + 1u;
			if ( iAdd > (SIZE_MAX - iValue) ) {
				xrtSetErrorKind(XERR_RANGE);
				return false;
			}
			iAdd += iValue;
		} else {
			iAdd = iKey;
		}
		if ( i != 0 ) {
			if ( iAdd == SIZE_MAX ) {
				xrtSetErrorKind(XERR_RANGE);
				return false;
			}
			iAdd++;
		}
		if ( iRequired > (SIZE_MAX - iAdd) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iRequired += iAdd;
	}
	if ( iRequired == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pRequired = iRequired;
	return true;
}



/* 按 RFC 3986 编码键和值并原子写出 Query。 */
XHTTP_API bool xrtQueryWrite(
	const xquerypair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xpercentmap Safe;
	char* sWrite = (char*)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( (pSize == NULL) || ((pOutput == NULL) && (iCapacity != 0)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtPercentMapInit(
		&Safe, (xstrview){ NULL, 0 }, true
	) ) {
		return false;
	}
	if ( !__xrtQueryCodecMeasure(
		pPairs, iCount, &Safe, &iRequired
	) ) {
		return false;
	}
	if ( __xrtQueryMetadataOverlap(
		pPairs, iCount, pSize, sizeof(*pSize)
	) || ((pOutput != NULL) && xrtMemRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pSize = iRequired;
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( __xrtQueryMetadataOverlap(
		pPairs, iCount, pOutput, iRequired
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( i != 0 ) {
			sWrite[iOffset++] = '&';
		}
		iOffset += xrtPercentWriteMeasured(
			pPairs[i].Key.Data,
			pPairs[i].Key.Size, &Safe, false, sWrite + iOffset
		);
		if ( (pPairs[i].Flags & XQUERY_HAS_VALUE) != 0 ) {
			sWrite[iOffset++] = '=';
			iOffset += xrtPercentWriteMeasured(
				pPairs[i].Value.Data,
				pPairs[i].Value.Size, &Safe, false, sWrite + iOffset
			);
		}
	}
	*pSize = iRequired;
	return true;
}



/* 分配并构建零结尾的 RFC 3986 Query。 */
XHTTP_API str xrtQueryBuild(
	const xquerypair* pPairs,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) && (pPairs != NULL) &&
		(iCount <= (SIZE_MAX / sizeof(*pPairs))) &&
		__xrtQueryMetadataOverlap(
			pPairs, iCount, pSize, sizeof(*pSize)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( !xrtQueryWrite(pPairs, iCount, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtQueryWrite(
		pPairs, iCount, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		*pSize = iRequired;
	}
	return sOutput;
}

#endif
