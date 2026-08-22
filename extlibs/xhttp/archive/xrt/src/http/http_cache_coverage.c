#include "../internal/xrt_http_cache_range.h"



#if defined(XRT_FEATURE_HTTP_CACHE_RANGE)

/* 判断规范覆盖集的排序、间隔和完整长度约束是否成立。 */
XRT_API bool xrtHttpCacheCoverageValid(
	const xhttpcachecoverage* pCoverage
)
{
	size_t i;

	if ( (pCoverage == NULL) ||
		((pCoverage->Ranges == NULL) &&
		 (pCoverage->RangeCount != 0)) ||
		(pCoverage->RangeCount >
		 (SIZE_MAX / sizeof(*pCoverage->Ranges))) ) {
		return false;
	}
	if ( pCoverage->HasLength &&
		(pCoverage->Length == 0) &&
		(pCoverage->RangeCount != 0) ) {
		return false;
	}
	for ( i = 0; i < pCoverage->RangeCount; i++ ) {
		if ( !__xrtHttpCacheRangeValid(
			&pCoverage->Ranges[i]
		) ) {
			return false;
		}
		if ( pCoverage->HasLength &&
			(pCoverage->Ranges[i].Last >=
			 pCoverage->Length) ) {
			return false;
		}
		if ( (i != 0) &&
			__xrtHttpCacheRangeJoins(
				&pCoverage->Ranges[i - 1u],
				&pCoverage->Ranges[i]
			) ) {
			return false;
		}
	}
	return true;
}



/* 判断两个可写输出是否覆盖覆盖集、目标或彼此。 */
static bool __xrtHttpCacheCoverageOutputOverlap(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	const xhttpcachemissingcursor* pCursor,
	const xhttpbyterange* pMissing
)
{
	return __xrtRangesOverlap(
		pCursor, sizeof(*pCursor),
		pCoverage, sizeof(*pCoverage)
	) || __xrtRangesOverlap(
		pCursor, sizeof(*pCursor),
		pCoverage->Ranges,
		pCoverage->RangeCount *
			sizeof(*pCoverage->Ranges)
	) || __xrtRangesOverlap(
		pCursor, sizeof(*pCursor),
		pTarget, sizeof(*pTarget)
	) || __xrtRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCoverage, sizeof(*pCoverage)
	) || __xrtRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCoverage->Ranges,
		pCoverage->RangeCount *
			sizeof(*pCoverage->Ranges)
	) || __xrtRangesOverlap(
		pMissing, sizeof(*pMissing),
		pTarget, sizeof(*pTarget)
	) || __xrtRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCursor, sizeof(*pCursor)
	);
}



/* 判断目标闭区间是否与已知完整长度一致。 */
static bool __xrtHttpCacheCoverageTargetValid(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget
)
{
	return __xrtHttpCacheRangeValid(pTarget) &&
		(!pCoverage->HasLength ||
		 (pTarget->Last < pCoverage->Length));
}



/* 判断目标区间是否完全位于规范覆盖集中。 */
XRT_API xhttpcachecoverageresult xrtHttpCacheCoverageCovers(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pRange
)
{
	size_t i;

	if ( !xrtHttpCacheCoverageValid(pCoverage) ||
		!__xrtHttpCacheCoverageTargetValid(
			pCoverage, pRange
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_COVERAGE_ERROR;
	}
	for ( i = 0; i < pCoverage->RangeCount; i++ ) {
		if ( pCoverage->Ranges[i].Last <
			pRange->First ) {
			continue;
		}
		if ( pCoverage->Ranges[i].First >
			pRange->First ) {
			return XHTTP_CACHE_COVERAGE_MISS;
		}
		return (pCoverage->Ranges[i].Last >=
			pRange->Last) ?
			XHTTP_CACHE_COVERAGE_HIT :
			XHTTP_CACHE_COVERAGE_MISS;
	}
	return XHTTP_CACHE_COVERAGE_MISS;
}



/* 初始化一个尚未开始迭代的缺口游标。 */
XRT_API void xrtHttpCacheMissingCursorInit(
	xhttpcachemissingcursor* pCursor
)
{
	if ( pCursor != NULL ) {
		memset(pCursor, 0, sizeof(*pCursor));
	}
}



/* 验证调用方可见的缺口游标状态。 */
static bool __xrtHttpCacheMissingCursorValid(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	const xhttpcachemissingcursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Range > pCoverage->RangeCount) ) {
		return false;
	}
	if ( !pCursor->Started ) {
		return (pCursor->Range == 0) &&
			(pCursor->Next == 0) &&
			!pCursor->Finished;
	}
	if ( pCursor->Finished ) {
		return true;
	}
	return (pCursor->Next >= pTarget->First) &&
		(pCursor->Next <= pTarget->Last);
}



/* 在目标区间内迭代尚未保存的连续缺口。 */
XRT_API xhttpnext xrtHttpCacheMissingNext(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	xhttpcachemissingcursor* pCursor,
	xhttpbyterange* pMissing
)
{
	xhttpcachemissingcursor Cursor;
	xhttpbyterange Missing;

	if ( (pCursor == NULL) || (pMissing == NULL) ||
		!xrtHttpCacheCoverageValid(pCoverage) ||
		!__xrtHttpCacheCoverageTargetValid(
			pCoverage, pTarget
		) ||
		__xrtHttpCacheCoverageOutputOverlap(
			pCoverage, pTarget, pCursor, pMissing
		) ||
		!__xrtHttpCacheMissingCursorValid(
			pCoverage, pTarget, pCursor
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	Cursor = *pCursor;
	if ( Cursor.Finished ) {
		return XHTTP_NEXT_END;
	}
	if ( !Cursor.Started ) {
		Cursor.Started = true;
		Cursor.Next = pTarget->First;
	}

	while ( Cursor.Range < pCoverage->RangeCount ) {
		const xhttpbyterange* pStored =
			&pCoverage->Ranges[Cursor.Range];

		if ( pStored->Last < Cursor.Next ) {
			Cursor.Range++;
			continue;
		}
		if ( pStored->First > pTarget->Last ) {
			break;
		}
		if ( pStored->First > Cursor.Next ) {
			Missing.First = Cursor.Next;
			Missing.Last = pStored->First -
				UINT64_C(1);
			if ( Missing.Last > pTarget->Last ) {
				Missing.Last = pTarget->Last;
			}
			if ( Missing.Last == UINT64_MAX ) {
				Cursor.Finished = true;
			} else {
				Cursor.Next =
					Missing.Last + UINT64_C(1);
			}
			*pCursor = Cursor;
			*pMissing = Missing;
			return XHTTP_NEXT_ITEM;
		}
		if ( pStored->Last >= pTarget->Last ) {
			Cursor.Finished = true;
			*pCursor = Cursor;
			return XHTTP_NEXT_END;
		}
		Cursor.Next =
			pStored->Last + UINT64_C(1);
		Cursor.Range++;
	}

	Missing.First = Cursor.Next;
	Missing.Last = pTarget->Last;
	Cursor.Finished = true;
	*pCursor = Cursor;
	*pMissing = Missing;
	return XHTTP_NEXT_ITEM;
}

#endif
