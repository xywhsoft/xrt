#include "../internal/xrt_http_cache_validate.h"



#if defined(XRT_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断字段名称是否匹配指定 ASCII 字面量。 */
static bool __xrtHttpCacheFieldName(
	const xhttpfield* pField,
	xstrview Name
)
{
	return xrtHttpFieldNameEqual(pField->Name, Name);
}



/* 把 HTTP 日期向负无穷取整为整秒。 */
static int64 __xrtHttpCacheDateSecond(xtime iTime)
{
	return __xrtTimeFloorDiv(iTime, XRT_TIME_SECOND);
}



/* 扫描不可重复的 ETag 字段；重复或非法值只会使验证器不可用。 */
static void __xrtHttpCacheETagRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i], XRT_STR_LITERAL("ETag")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	pValidator->ETagPresent = iMatches != 0;
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		pValidator->ETagValid = __xrtHttpETagParseValue(
			Value, &pValidator->ETag
		);
	}
}



/* 扫描 Last-Modified，并结合单个有效 Date 判断它能否作为强验证器。 */
static void __xrtHttpCacheLastModifiedRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	xhttpcachetime Time;
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("Last-Modified")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	pValidator->LastModifiedPresent =
		iMatches != 0;
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		pValidator->LastModifiedValid =
			__xrtTimeParseHTTPDateValue(
				Value,
				&pValidator->LastModified
			);
	}

	/* Date 已由缓存时间模块按单值规则解析。 */
	if ( !xrtHttpCacheTimeParse(
		pFields, iCount, &Time
	) ) {
		return;
	}
	pValidator->DateValid =
		(Time.DateCount == 1) &&
		((Time.Flags & (
			XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID
		 )) == 0);
	if ( pValidator->DateValid ) {
		pValidator->Date = Time.Date;
	}
	if ( pValidator->LastModifiedValid &&
		pValidator->DateValid ) {
		int64 iDate = __xrtHttpCacheDateSecond(
			pValidator->Date
		);
		int64 iModified = __xrtHttpCacheDateSecond(
			pValidator->LastModified
		);

		pValidator->LastModifiedStrong =
			(iDate > iModified);
	}
}



/* 扫描一个或多个语义相同的 Content-Length 字段。 */
static void __xrtHttpCacheContentLengthRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	uint64 iLength = 0;
	uint64 iValue;
	bool bHaveValue = false;
	size_t i;

	pValidator->ContentLengthValid = true;
	for ( i = 0; i < iCount; i++ ) {
		xrt_http_content_length Result;

		if ( !__xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("Content-Length")
		) ) {
			continue;
		}
		pValidator->ContentLengthPresent = true;
		Result = __xrtHttpContentLengthParse(
			pFields[i].Value, &iValue
		);
		if ( (Result != XRT_HTTP_CONTENT_LENGTH_VALID) ||
			(bHaveValue && (iValue != iLength)) ) {
			pValidator->ContentLengthValid = false;
			continue;
		}
		iLength = iValue;
		bHaveValue = true;
	}
	if ( !pValidator->ContentLengthPresent ||
		!bHaveValue ) {
		pValidator->ContentLengthValid =
			!pValidator->ContentLengthPresent;
	}
	if ( pValidator->ContentLengthValid &&
		pValidator->ContentLengthPresent ) {
		pValidator->ContentLength = iLength;
	}
}



/* 判断缓存条目视图、字段数组和正文范围标志是否自洽。 */
XRT_API bool xrtHttpCacheEntryValid(
	const xhttpcacheentry* pEntry
)
{
	if ( (pEntry == NULL) ||
		((pEntry->Flags & ~(
			XHTTP_CACHE_ENTRY_PARTIAL |
			XHTTP_CACHE_ENTRY_RANGE_COVERED
		 )) != 0) ||
		(((pEntry->Flags &
		   XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0) &&
		 ((pEntry->Flags &
		   XHTTP_CACHE_ENTRY_PARTIAL) == 0)) ) {
		return false;
	}
	return __xrtHttpCacheFieldsValid(
		pEntry->Fields, pEntry->FieldCount
	);
}



/* 验证借用字段数组的内存与 HTTP 名称和值语法。 */
bool __xrtHttpCacheFieldsValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpTokenValid(
			pFields[i].Name
		) || !xrtHttpFieldValueValid(
			pFields[i].Value
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证缓存条目数组及其中全部借用字段。 */
bool __xrtHttpCacheEntriesValid(
	const xhttpcacheentry* pEntries,
	size_t iCount
)
{
	size_t i;

	if ( ((pEntries == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pEntries))) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpCacheEntryValid(&pEntries[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断一段内存是否覆盖条目数组或任一借用 Header。 */
bool __xrtHttpCacheEntriesOverlap(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	size_t i;

	if ( __xrtRangesOverlap(
		pEntries,
		iCount * sizeof(*pEntries),
		pMemory,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpFieldArrayOverlap(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 从响应 Header 读取 ETag、日期和 Content-Length，不分配内存。 */
bool __xrtHttpCacheValidatorRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	if ( (pValidator == NULL) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iCount
		) ) {
		return false;
	}
	memset(pValidator, 0, sizeof(*pValidator));
	__xrtHttpCacheETagRead(
		pFields, iCount, pValidator
	);
	__xrtHttpCacheLastModifiedRead(
		pFields, iCount, pValidator
	);
	__xrtHttpCacheContentLengthRead(
		pFields, iCount, pValidator
	);
	return true;
}



/* 判断条目能否满足当前完整或 Range 请求。 */
bool __xrtHttpCacheEntryEligible(
	const xhttpcacheentry* pEntry,
	bool Range
)
{
	if ( (pEntry->Flags &
		 XHTTP_CACHE_ENTRY_PARTIAL) == 0 ) {
		return true;
	}
	return Range &&
		((pEntry->Flags &
		  XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0);
}



/* 精确比较包含强弱标志的两个实体标签。 */
bool __xrtHttpCacheETagExact(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
)
{
	return (pLeft->Weak == pRight->Weak) &&
		(pLeft->Opaque.Size == pRight->Opaque.Size) &&
		((pLeft->Opaque.Size == 0) ||
		 (memcmp(
			pLeft->Opaque.Data,
			pRight->Opaque.Data,
			pLeft->Opaque.Size
		 ) == 0));
}



/* 按 HTTP 秒精度比较两个日期。 */
bool __xrtHttpCacheDateEqual(
	xtime iLeft,
	xtime iRight
)
{
	return __xrtHttpCacheDateSecond(iLeft) ==
		__xrtHttpCacheDateSecond(iRight);
}



/* 返回候选用于“最新响应”排序的 Date 或接收时间。 */
xtime __xrtHttpCacheEntryDate(
	const xhttpcacheentry* pEntry,
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->DateValid ?
		pValidator->Date :
		pEntry->ResponseTime;
}



/* 判断一个可用 ETag 是否已经由更早的可用条目贡献。 */
static bool __xrtHttpCacheETagEarlier(
	const xhttpcacheentry* pEntries,
	size_t iIndex,
	bool Range,
	const xhttpetag* pTag
)
{
	xrt_http_cache_validator Validator;
	size_t i;

	for ( i = 0; i < iIndex; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( Validator.ETagValid &&
			xrtHttpETagWeakEqual(
				&Validator.ETag, pTag
			) ) {
			return true;
		}
	}
	return false;
}



/* 累加验证计划中的 ETag 条目和线路长度。 */
static bool __xrtHttpCacheValidateTagAdd(
	xhttpcachevalidateplan* pPlan,
	const xhttpetag* pTag
)
{
	size_t iTagSize;
	size_t iWrapper;
	size_t iAdd;

	iWrapper = pTag->Weak ? 4u : 2u;
	if ( pTag->Opaque.Size > (SIZE_MAX - iWrapper) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iTagSize = pTag->Opaque.Size + iWrapper;
	if ( (pPlan->ETagCount != 0) &&
		(iTagSize > (SIZE_MAX - 2u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iAdd = iTagSize +
		((pPlan->ETagCount == 0) ? 0u : 2u);
	if ( pPlan->ETagSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pPlan->ETagSize += iAdd;
	pPlan->ETagCount++;
	return true;
}



/* 生成验证计划的内部实现同时供长度写出复用。 */
static bool __xrtHttpCacheValidateMeasure(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
)
{
	xrt_http_cache_validator Validator;
	xtime iLastModified = 0;
	bool bLastModified = false;
	size_t i;

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_VALIDATE_NONE;
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		pPlan->EligibleCount++;
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( Validator.ETagValid &&
			!__xrtHttpCacheETagEarlier(
				pEntries, i, Range,
				&Validator.ETag
			) &&
			!__xrtHttpCacheValidateTagAdd(
				pPlan, &Validator.ETag
			) ) {
			return false;
		}
		if ( Validator.LastModifiedValid ) {
			iLastModified =
				Validator.LastModified;
			bLastModified = true;
		}
	}
	if ( pPlan->ETagCount != 0 ) {
		pPlan->Actions |=
			XHTTP_CACHE_VALIDATE_IF_NONE_MATCH;
	}
	if ( !Range &&
		(pPlan->EligibleCount == 1) &&
		bLastModified ) {
		pPlan->LastModified = iLastModified;
		pPlan->Actions |=
			XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE;
	}
	if ( pPlan->Actions != 0 ) {
		pPlan->Decision =
			XHTTP_CACHE_VALIDATE_CONDITIONAL;
	}
	return true;
}



/* 按选定候选生成条件验证请求计划。 */
XRT_API xhttpcachevalidatedecision xrtHttpCacheValidatePlan(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
)
{
	xhttpcachevalidateplan Plan;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iCount
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pPlan, sizeof(*pPlan)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_VALIDATE_ERROR;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_VALIDATE_ERROR;
	if ( !__xrtHttpCacheValidateMeasure(
		pEntries, iCount, Range, &Plan
	) ) {
		return XHTTP_CACHE_VALIDATE_ERROR;
	}
	*pPlan = Plan;
	return Plan.Decision;
}



/* 写出计划中的去重 If-None-Match 字段值。 */
XRT_API bool xrtHttpCacheValidateETagsWrite(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpcachevalidateplan Plan;
	xrt_http_cache_validator Validator;
	bytes pBytes = (bytes)pOutput;
	size_t iWritten = 0;
	size_t iTagSize;
	size_t i;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iCount
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pSize, sizeof(*pSize)
		) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			pSize, sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pSize = 0;
	if ( !__xrtHttpCacheValidateMeasure(
		pEntries, iCount, Range, &Plan
	) ) {
		return false;
	}
	*pSize = Plan.ETagSize;
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < Plan.ETagSize ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( Plan.ETagSize == 0 ) {
		return true;
	}
	if ( __xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pOutput, Plan.ETagSize
		) ||
		__xrtRangesOverlap(
			pOutput, Plan.ETagSize,
			pSize, sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	/* 容量与重叠已经验证，实际写出不会留下半成品。 */
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( !Validator.ETagValid ||
			__xrtHttpCacheETagEarlier(
				pEntries, i, Range,
				&Validator.ETag
			) ) {
			continue;
		}
		if ( iWritten != 0 ) {
			pBytes[iWritten] = (uint8)',';
			pBytes[iWritten + 1u] = (uint8)' ';
			iWritten += 2u;
		}
		iTagSize = 0;
		if ( !xrtHttpETagWrite(
			&Validator.ETag,
			pBytes + iWritten,
			Plan.ETagSize - iWritten,
			&iTagSize
		) ) {
			return false;
		}
		iWritten += iTagSize;
	}
	return iWritten == Plan.ETagSize;
}



/* 为单个缓存条目的 Range 请求选择 If-Range。 */
XRT_API xhttpcacheifrangekind xrtHttpCacheIfRangePlan(
	const xhttpcacheentry* pEntry,
	xhttpcacheifrange* pPlan
)
{
	xrt_http_cache_validator Validator;
	xhttpcacheifrange Plan;

	if ( !xrtHttpCacheEntryValid(pEntry) ||
		(pPlan == NULL) ||
		__xrtHttpFieldArrayOverlap(
			pEntry->Fields,
			pEntry->FieldCount,
			pPlan,
			sizeof(*pPlan)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_IF_RANGE_ERROR;
	}
	memset(&Plan, 0, sizeof(Plan));
	Plan.Kind = XHTTP_CACHE_IF_RANGE_NONE;
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Kind = XHTTP_CACHE_IF_RANGE_ERROR;
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Validator
	);
	if ( Validator.ETagPresent ) {
		if ( Validator.ETagValid &&
			!Validator.ETag.Weak ) {
			Plan.ETag = Validator.ETag;
			Plan.Kind = XHTTP_CACHE_IF_RANGE_ETAG;
		}
	} else if ( Validator.LastModifiedValid &&
		Validator.LastModifiedStrong ) {
		Plan.Date = Validator.LastModified;
		Plan.Kind = XHTTP_CACHE_IF_RANGE_DATE;
	}
	*pPlan = Plan;
	return Plan.Kind;
}



/* 扫描缓存可评估的 If-None-Match 条件。 */
static bool __xrtHttpCacheIfNoneMatch(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cache_validator* pValidator,
	bool* pPresent,
	bool* pMatch
)
{
	xhttpetagitem Item;
	xhttpnext Next;
	xstrview Value;
	size_t iOffset;
	size_t iItems = 0;
	size_t iFields = 0;
	bool bAny = false;
	size_t i;

	*pPresent = false;
	*pMatch = false;
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("If-None-Match")
		) ) {
			continue;
		}
		*pPresent = true;
		iFields++;
		Value = xrtHttpOwsTrim(pFields[i].Value);
		iOffset = 0;
		do {
			Next = xrtHttpETagNext(
				Value, &iOffset, &Item
			);
			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				iItems++;
				if ( Item.Kind ==
					XHTTP_ETAG_ANY ) {
					bAny = true;
					*pMatch = true;
				} else if (
					pValidator->ETagValid &&
					xrtHttpETagWeakEqual(
						&Item.Tag,
						&pValidator->ETag
					) ) {
					*pMatch = true;
				}
			}
		} while ( Next == XHTTP_NEXT_ITEM );
	}
	if ( !*pPresent ) {
		return true;
	}
	if ( (iItems == 0) ||
		(bAny &&
		 ((iItems != 1) || (iFields != 1))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 读取最多一个 If-Modified-Since；重复或非法日期按规范忽略。 */
static bool __xrtHttpCacheIfModifiedSince(
	const xhttpfield* pFields,
	size_t iCount,
	bool* pValid,
	xtime* pDate
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	*pValid = false;
	*pDate = 0;
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("If-Modified-Since")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		*pValid = __xrtTimeParseHTTPDateValue(
			Value, pDate
		);
	}
	return true;
}



/* 缓存只评估适用于已保存 GET/HEAD 表示的条件字段。 */
XRT_API xhttpprecondition xrtHttpCachePreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestCount,
	const xhttpcacheentry* pEntry
)
{
	xrt_http_cache_validator Validator;
	xtime iCondition;
	xtime iStored;
	bool bPresent;
	bool bMatch;
	bool bValid;
	bool bGet;
	bool bHead;

	if ( !__xrtHttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		!__xrtHttpCacheFieldsValid(
			pRequestFields, iRequestCount
		) ||
		!xrtHttpCacheEntryValid(pEntry) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_PRECONDITION_ERROR;
	}
	bGet = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("GET")
	);
	bHead = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);
	if ( !bGet && !bHead ) {
		return XHTTP_PRECONDITION_PROCEED;
	}
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Validator
	);

	/* If-None-Match 优先，并且星号命中任何已保存表示。 */
	if ( !__xrtHttpCacheIfNoneMatch(
		pRequestFields,
		iRequestCount,
		&Validator,
		&bPresent,
		&bMatch
	) ) {
		return XHTTP_PRECONDITION_ERROR;
	}
	if ( bPresent ) {
		return bMatch ?
			XHTTP_PRECONDITION_NOT_MODIFIED :
			XHTTP_PRECONDITION_PROCEED;
	}

	/* 缺少 Last-Modified 时按 Date、接收时间顺序回退。 */
	(void)__xrtHttpCacheIfModifiedSince(
		pRequestFields,
		iRequestCount,
		&bValid,
		&iCondition
	);
	if ( !bValid ) {
		return XHTTP_PRECONDITION_PROCEED;
	}
	if ( Validator.LastModifiedValid ) {
		iStored = Validator.LastModified;
	} else if ( Validator.DateValid ) {
		iStored = Validator.Date;
	} else {
		iStored = pEntry->ResponseTime;
	}
	return (__xrtHttpCacheDateSecond(iStored) <=
		__xrtHttpCacheDateSecond(iCondition)) ?
		XHTTP_PRECONDITION_NOT_MODIFIED :
		XHTTP_PRECONDITION_PROCEED;
}



/* 分类条件请求的最终响应。 */
XRT_API xhttpcachevalidateresult xrtHttpCacheValidateResult(
	uint16 iStatus,
	bool Treat5xxAsFailure
)
{
	if ( (iStatus < 200u) || (iStatus > 599u) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_VALIDATE_RESULT_ERROR;
	}
	if ( iStatus == XHTTP_STATUS_NOT_MODIFIED ) {
		return XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED;
	}
	if ( Treat5xxAsFailure && (iStatus >= 500u) ) {
		return XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE;
	}
	return XHTTP_CACHE_VALIDATE_RESULT_FULL;
}

#endif
