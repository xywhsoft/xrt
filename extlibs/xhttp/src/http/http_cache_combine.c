#include "../internal/xrt_http_cache_range.h"



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 整组片段可以同时保留强 ETag 和强 Last-Modified 候选。 */
typedef struct xrt_http_cache_strong_set {
	xhttpetag ETag;
	xtime Date;
	bool ETagAny;
	bool ETagCommon;
	bool DateCommon;
} xrt_http_cache_strong_set;



/* 判断验证元数据是否带有强 ETag。 */
static bool __xrtHttpCacheValidatorStrongETag(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagValid &&
		!pValidator->ETag.Weak;
}



/* 判断验证元数据是否带有强 Last-Modified。 */
static bool __xrtHttpCacheValidatorStrongDate(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->LastModifiedValid &&
		pValidator->LastModifiedStrong;
}



/*
	计算全部片段共享的强验证器。
	任意两个强 ETag 明确冲突时，即使日期相同也不能组合。
*/
static bool __xrtHttpCacheStrongSetRead(
	const xhttpcachefragment* pStored,
	size_t iCount,
	xrt_http_cache_strong_set* pSet
)
{
	xrt_http_cache_validator Validator;
	xrt_http_cache_strong_set Set;
	bool bETagAll = true;
	bool bDateAll = true;
	bool bETagConflict = false;
	bool bDateAny = false;
	bool bETag;
	bool bDate;
	size_t i;

	memset(&Set, 0, sizeof(Set));
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheValidatorRead(
			pStored[i].Entry.Fields,
			pStored[i].Entry.FieldCount,
			&Validator
		) ) {
			return false;
		}
		bETag = __xrtHttpCacheValidatorStrongETag(
			&Validator
		);
		bDate = __xrtHttpCacheValidatorStrongDate(
			&Validator
		);

		if ( !bETag ) {
			bETagAll = false;
		} else if ( !Set.ETagAny ) {
			Set.ETag = Validator.ETag;
			Set.ETagAny = true;
		} else if ( !xrtHttpETagStrongEqual(
			&Set.ETag, &Validator.ETag
		) ) {
			bETagAll = false;
			bETagConflict = true;
		}

		if ( !bDate ) {
			bDateAll = false;
		} else if ( !bDateAny ) {
			Set.Date = Validator.LastModified;
			bDateAny = true;
		} else if ( !__xrtHttpCacheDateEqual(
			Set.Date, Validator.LastModified
		) ) {
			bDateAll = false;
		}
	}
	Set.ETagCommon =
		bETagAll && Set.ETagAny;
	Set.DateCommon =
		bDateAll && bDateAny;
	*pSet = Set;
	return !bETagConflict &&
		((iCount < 2) ||
		 Set.ETagCommon ||
		 Set.DateCommon);
}



/* 判断新片段与整组现有片段共享一个强验证器。 */
static bool __xrtHttpCacheStrongSetMatch(
	const xrt_http_cache_strong_set* pSet,
	const xhttpcachefragment* pIncoming
)
{
	xrt_http_cache_validator Incoming;
	bool bETag;

	if ( !__xrtHttpCacheValidatorRead(
		pIncoming->Entry.Fields,
		pIncoming->Entry.FieldCount,
		&Incoming
	) ) {
		return false;
	}
	bETag = __xrtHttpCacheValidatorStrongETag(
		&Incoming
	);
	if ( pSet->ETagAny && bETag &&
		!xrtHttpETagStrongEqual(
			&pSet->ETag, &Incoming.ETag
		) ) {
		return false;
	}
	if ( pSet->ETagCommon && bETag ) {
		return true;
	}
	return pSet->DateCommon &&
		__xrtHttpCacheValidatorStrongDate(
			&Incoming
		) &&
		__xrtHttpCacheDateEqual(
			pSet->Date,
			Incoming.LastModified
		);
}



/* 判断片段是否带有正文覆盖区间。 */
static bool __xrtHttpCacheFragmentHasRange(
	const xhttpcachefragment* pFragment
)
{
	return (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0;
}



/* 判断片段是否带有已知完整表示长度。 */
static bool __xrtHttpCacheFragmentHasLength(
	const xhttpcachefragment* pFragment
)
{
	return (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0;
}



/* 判断两个有序片段区间是否仍保留至少一个字节缺口。 */
static bool __xrtHttpCacheRangesSeparate(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
)
{
	if ( pLeft->Last == UINT64_MAX ) {
		return false;
	}
	return (pLeft->Last + UINT64_C(1)) <
		pRight->First;
}



/* 验证现有片段数组已经表示同一强验证器下的规范覆盖集。 */
static bool __xrtHttpCacheStoredValid(
	const xhttpcachefragment* pStored,
	size_t iCount,
	xrt_http_cache_strong_set* pValidators
)
{
	bool bHaveRange = false;
	bool bHaveLength = false;
	uint64 iLength = 0;
	size_t i;

	if ( ((pStored == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pStored))) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpCacheFragmentValid(
			&pStored[i]
		) ) {
			return false;
		}
		if ( __xrtHttpCacheFragmentHasLength(
			&pStored[i]
		) ) {
			if ( bHaveLength &&
				(iLength != pStored[i].Length) ) {
				return false;
			}
			bHaveLength = true;
			iLength = pStored[i].Length;
		}
		if ( __xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) ) {
			if ( bHaveRange &&
				!__xrtHttpCacheRangesSeparate(
					&pStored[i - 1u].Range,
					&pStored[i].Range
				) ) {
				return false;
			}
			bHaveRange = true;
		} else if ( (iCount != 1) ||
			!xrtHttpCacheFragmentComplete(
				&pStored[i]
			) ) {
			return false;
		}
	}
	return __xrtHttpCacheStrongSetRead(
		pStored, iCount, pValidators
	);
}



/* 判断组合计划输出是否覆盖任一片段结构或借用 Header。 */
static bool __xrtHttpCacheCombineOutputOverlap(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	const xhttpcachecombineplan* pPlan
)
{
	size_t i;

	if ( __xrtRangesOverlap(
		pPlan, sizeof(*pPlan),
		pStored, iCount * sizeof(*pStored)
	) || __xrtRangesOverlap(
		pPlan, sizeof(*pPlan),
		pIncoming, sizeof(*pIncoming)
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpFieldArrayOverlap(
			pStored[i].Entry.Fields,
			pStored[i].Entry.FieldCount,
			pPlan,
			sizeof(*pPlan)
		) ) {
			return true;
		}
	}
	return __xrtHttpFieldArrayOverlap(
		pIncoming->Entry.Fields,
		pIncoming->Entry.FieldCount,
		pPlan,
		sizeof(*pPlan)
	);
}



/* 发布不需要区间计划的协议决定。 */
static xhttpcachecombinedecision __xrtHttpCacheCombineDecision(
	xhttpcachecombineplan* pPlan,
	xhttpcachecombinedecision Decision
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = Decision;
	return Decision;
}



/* 读取现有片段集合已经确认的完整表示长度。 */
static bool __xrtHttpCacheStoredLength(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint64* pLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFragmentHasLength(
			&pStored[i]
		) ) {
			*pLength = pStored[i].Length;
			return true;
		}
	}
	return false;
}



/* 判断现有覆盖区间能否容纳新确认的完整长度。 */
static bool __xrtHttpCacheStoredWithinLength(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint64 iLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) && (pStored[i].Range.Last >=
			iLength) ) {
			return false;
		}
	}
	return true;
}



/* 选择最晚收到的指定来源状态 Header。 */
static size_t __xrtHttpCacheNewestHeader(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint16 iStatus
)
{
	size_t iSelected = XRT_NPOS;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( (iStatus != 0) &&
			(pStored[i].SourceStatus != iStatus) ) {
			continue;
		}
		if ( (iSelected == XRT_NPOS) ||
			(pStored[i].Entry.ResponseTime >=
			 pStored[iSelected].Entry.ResponseTime) ) {
			iSelected = i;
		}
	}
	return iSelected;
}



/* 根据最新响应状态选择组合后的 Header 来源和更新动作。 */
static void __xrtHttpCacheCombineHeaders(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	size_t iHeader = XRT_NPOS;

	if ( pIncoming->SourceStatus == XHTTP_STATUS_OK ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS;
		return;
	}

	iHeader = __xrtHttpCacheNewestHeader(
		pStored,
		iCount,
		XHTTP_STATUS_OK
	);
	if ( iHeader == XRT_NPOS ) {
		iHeader = __xrtHttpCacheNewestHeader(
			pStored, iCount, 0
		);
	}
	if ( iHeader != XRT_NPOS ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS;
	}
	pPlan->HeaderIndex = iHeader;
	if ( iHeader == XRT_NPOS ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS;
	}
	pPlan->Actions |=
		XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE;
	if ( (iHeader == XRT_NPOS) ||
		(pStored[iHeader].SourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH;
	}
}



/* 建立完整片段替换全部现有覆盖的计划。 */
static xhttpcachecombinedecision __xrtHttpCacheReplacePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	(void)pStored;
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = XHTTP_CACHE_COMBINE_REPLACE;
	pPlan->Index = 0;
	pPlan->RemoveCount = iCount;
	pPlan->ResultCount =
		__xrtHttpCacheFragmentHasRange(
			pIncoming
		) ? 1u : 0u;
	pPlan->HasRange =
		__xrtHttpCacheFragmentHasRange(
			pIncoming
		);
	pPlan->HasLength = true;
	pPlan->Length = pIncoming->Length;
	pPlan->Complete = true;
	if ( pPlan->HasRange ) {
		pPlan->Range = pIncoming->Range;
	}
	pPlan->Actions =
		XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS |
		XHTTP_CACHE_COMBINE_AS_200 |
		XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH;
	if ( pIncoming->SourceStatus ==
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE |
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH;
	}
	return pPlan->Decision;
}



/* 计算新片段在规范区间数组中需要替换的连续窗口。 */
static void __xrtHttpCacheCombineWindow(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpbyterange* pIncoming,
	size_t* pIndex,
	size_t* pRemoveCount,
	xhttpbyterange* pMerged
)
{
	size_t i = 0;

	*pMerged = *pIncoming;
	while ( (i < iCount) &&
		__xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) &&
		__xrtHttpCacheRangesSeparate(
			&pStored[i].Range, pMerged
		) ) {
		i++;
	}
	*pIndex = i;
	while ( (i < iCount) &&
		__xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) &&
		!__xrtHttpCacheRangesSeparate(
			pMerged, &pStored[i].Range
		) ) {
		if ( pStored[i].Range.First <
			pMerged->First ) {
			pMerged->First =
				pStored[i].Range.First;
		}
		if ( pStored[i].Range.Last >
			pMerged->Last ) {
			pMerged->Last =
				pStored[i].Range.Last;
		}
		i++;
	}
	*pRemoveCount = i - *pIndex;
}



/* 判断区间替换后的规范覆盖是否完整。 */
static bool __xrtHttpCacheCombineComplete(
	const xhttpcachecombineplan* pPlan
)
{
	if ( !pPlan->HasLength ) {
		return false;
	}
	if ( pPlan->Length == 0 ) {
		return pPlan->ResultCount == 0;
	}
	return (pPlan->ResultCount == 1) &&
		pPlan->HasRange &&
		(pPlan->Range.First == 0) &&
		(pPlan->Range.Last ==
		 (pPlan->Length - UINT64_C(1)));
}



/* 规划强验证器一致的部分响应组合。 */
XRT_API xhttpcachecombinedecision xrtHttpCacheCombinePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	xrt_http_cache_strong_set StoredValidators;
	bool bStoredLength;
	bool bIncomingLength;
	uint64 iStoredLength = 0;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheStoredValid(
			pStored, iCount, &StoredValidators
		) ||
		!xrtHttpCacheFragmentValid(pIncoming) ||
		__xrtRangesOverlap(
			pStored,
			iCount * sizeof(*pStored),
			pIncoming,
			sizeof(*pIncoming)
		) ||
		__xrtHttpCacheCombineOutputOverlap(
			pStored, iCount, pIncoming, pPlan
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_COMBINE_ERROR;
	}
	if ( xrtHttpCacheFragmentComplete(pIncoming) ) {
		return __xrtHttpCacheReplacePlan(
			pStored, iCount, pIncoming, pPlan
		);
	}

	bStoredLength = __xrtHttpCacheStoredLength(
		pStored, iCount, &iStoredLength
	);
	bIncomingLength =
		__xrtHttpCacheFragmentHasLength(pIncoming);
	if ( bStoredLength && bIncomingLength &&
		(iStoredLength != pIncoming->Length) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( bStoredLength &&
		(pIncoming->Range.Last >= iStoredLength) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( !bStoredLength && bIncomingLength &&
		!__xrtHttpCacheStoredWithinLength(
			pStored, iCount, pIncoming->Length
		) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( (iCount != 0) &&
		!__xrtHttpCacheStrongSetMatch(
			&StoredValidators, pIncoming
		) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan,
			XHTTP_CACHE_COMBINE_SEPARATE
		);
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = XHTTP_CACHE_COMBINE_APPLY;
	pPlan->HasRange = true;
	pPlan->HasLength =
		bStoredLength || bIncomingLength;
	pPlan->Length = bStoredLength ?
		iStoredLength : pIncoming->Length;
	__xrtHttpCacheCombineWindow(
		pStored,
		iCount,
		&pIncoming->Range,
		&pPlan->Index,
		&pPlan->RemoveCount,
		&pPlan->Range
	);
	pPlan->ResultCount =
		iCount - pPlan->RemoveCount + 1u;
	pPlan->Actions = XHTTP_CACHE_COMBINE_AS_200;
	__xrtHttpCacheCombineHeaders(
		pStored, iCount, pIncoming, pPlan
	);
	pPlan->Complete =
		__xrtHttpCacheCombineComplete(pPlan);
	if ( pPlan->Complete ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH;
	} else {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}

#endif
