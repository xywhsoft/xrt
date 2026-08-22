#include "../internal/xrt_http_semantics.h"
#include "../internal/xrt_time.h"



#if defined(XRT_FEATURE_HTTP_PRECONDITION)

/* 条件字段扫描结果保留存在性、星号、命中和条目总数。 */
typedef struct xrt_http_etag_condition {
	bool Present;
	bool Any;
	bool Match;
	size_t Items;
} xrt_http_etag_condition;



/* 验证条件评估输入并快照当前表示。 */
bool __xrtHttpPreconditionInputRead(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pInput,
	xhttprepresentation* pCurrent
)
{
	xhttpfield Field;
	size_t i;

	if ( !__xrtHttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		!__xrtRangeValid(pInput, sizeof(*pCurrent)) ||
		!__xrtHttpFieldArrayValid(pFields, iFieldCount) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCurrent, pInput, sizeof(*pCurrent));
	if ( (!pCurrent->Exists &&
		(pCurrent->HasETag || pCurrent->HasLastModified)) ||
		(pCurrent->LastModifiedStrong &&
		 !pCurrent->HasLastModified) ||
		(pCurrent->HasETag &&
		 !__xrtHttpETagValid(&pCurrent->ETag)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpTokenValid(Field.Name) ||
			!xrtHttpFieldValueValid(Field.Value) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 判断一个字段名称是否匹配目标条件字段。 */
static bool __xrtHttpConditionName(
	const xhttpfield* pField,
	xstrview Name
)
{
	return xrtHttpFieldNameEqual(pField->Name, Name);
}



/* 扫描一个可重复的实体标签条件字段并验证全部列表项。 */
static bool __xrtHttpETagConditionRead(
	const xhttpfield* pFields,
	size_t iFieldCount,
	xstrview Name,
	const xhttprepresentation* pCurrent,
	bool bWeak,
	xrt_http_etag_condition* pCondition
)
{
	xhttpetagitem Item;
	xhttpfield Field;
	xhttpnext Next;
	xstrview Value;
	size_t iOffset;
	size_t iFields = 0;
	size_t i;

	memset(pCondition, 0, sizeof(*pCondition));
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpConditionName(&Field, Name) ) {
			continue;
		}
		pCondition->Present = true;
		iFields++;
		Value = xrtHttpOwsTrim(Field.Value);
		iOffset = 0;
		do {
			Next = xrtHttpETagNext(Value, &iOffset, &Item);
			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				pCondition->Items++;
				if ( Item.Kind == XHTTP_ETAG_ANY ) {
					pCondition->Any = true;
				} else if ( pCurrent->HasETag ) {
					if ( bWeak ) {
						pCondition->Match =
							xrtHttpETagWeakEqual(
								&Item.Tag,
								&pCurrent->ETag
							) || pCondition->Match;
					} else {
						pCondition->Match =
							xrtHttpETagStrongEqual(
								&Item.Tag,
								&pCurrent->ETag
							) || pCondition->Match;
					}
				}
			}
		} while ( Next == XHTTP_NEXT_ITEM );
	}
	if ( !pCondition->Present ) {
		return true;
	}
	if ( (pCondition->Items == 0) ||
		(pCondition->Any &&
		 ((pCondition->Items != 1) || (iFields != 1))) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 读取一个不可重复的 HTTP-date 条件字段。 */
static bool __xrtHttpDateConditionRead(
	const xhttpfield* pFields,
	size_t iFieldCount,
	xstrview Name,
	bool* pPresent,
	bool* pValid,
	xtime* pDate
)
{
	xhttpfield Field;
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	*pPresent = false;
	*pValid = false;
	*pDate = 0;
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtHttpConditionName(&Field, Name) ) {
			Value = Field.Value;
			iMatches++;
		}
	}
	if ( iMatches == 0 ) {
		return true;
	}
	*pPresent = true;
	if ( iMatches != 1 ) {
		return true;
	}
	Value = xrtHttpOwsTrim(Value);
	*pValid = __xrtTimeParseHTTPDateValue(Value, pDate);
	return true;
}



/* 把微秒时间向负无穷取整为整秒，避免负时间比较偏差。 */
static int64 __xrtHttpTimeSecond(xtime iTime)
{
	return __xrtTimeFloorDiv(iTime, XRT_TIME_SECOND);
}



/* 按 RFC 固定顺序评估条件请求。 */
XRT_API xhttpprecondition xrtHttpPreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent
)
{
	xrt_http_etag_condition Tags;
	xhttprepresentation Current;
	xtime iDate;
	bool bPresent;
	bool bValid;
	bool bGet;
	bool bHead;

	if ( !__xrtHttpPreconditionInputRead(
		Method, pFields, iFieldCount, pCurrent, &Current
	) ) {
		return XHTTP_PRECONDITION_ERROR;
	}
	bGet = __xrtHttpViewEqual(
		Method, XRT_STR_LITERAL("GET")
	);
	bHead = __xrtHttpViewEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);
	if ( __xrtHttpViewEqual(
			Method, XRT_STR_LITERAL("CONNECT")
		) || __xrtHttpViewEqual(
			Method, XRT_STR_LITERAL("OPTIONS")
		) || __xrtHttpViewEqual(
			Method, XRT_STR_LITERAL("TRACE")
		) ) {
		return XHTTP_PRECONDITION_PROCEED;
	}

	/* If-Match 优先使用强比较。 */
	if ( !__xrtHttpETagConditionRead(
		pFields,
		iFieldCount,
		XRT_STR_LITERAL("If-Match"),
		&Current,
		false,
		&Tags
	) ) {
		return XHTTP_PRECONDITION_ERROR;
	}
	if ( Tags.Present ) {
		if ( !(Current.Exists &&
			(Tags.Any || Tags.Match)) ) {
			return XHTTP_PRECONDITION_FAILED;
		}
	} else {
		/* 只有没有 If-Match 时才评估 If-Unmodified-Since。 */
		if ( !__xrtHttpDateConditionRead(
			pFields,
			iFieldCount,
			XRT_STR_LITERAL("If-Unmodified-Since"),
			&bPresent,
			&bValid,
			&iDate
		) ) {
			return XHTTP_PRECONDITION_ERROR;
		}
		if ( bPresent && bValid &&
			Current.HasLastModified &&
			(__xrtHttpTimeSecond(Current.LastModified) >
			 __xrtHttpTimeSecond(iDate)) ) {
			return XHTTP_PRECONDITION_FAILED;
		}
	}

	/* If-None-Match 使用弱比较，读取方法决定失败状态。 */
	if ( !__xrtHttpETagConditionRead(
		pFields,
		iFieldCount,
		XRT_STR_LITERAL("If-None-Match"),
		&Current,
		true,
		&Tags
	) ) {
		return XHTTP_PRECONDITION_ERROR;
	}
	if ( Tags.Present ) {
		if ( Current.Exists && (Tags.Any || Tags.Match) ) {
			return (bGet || bHead) ?
				XHTTP_PRECONDITION_NOT_MODIFIED :
				XHTTP_PRECONDITION_FAILED;
		}
		return XHTTP_PRECONDITION_PROCEED;
	}

	/* If-Modified-Since 只用于 GET/HEAD 且没有 If-None-Match。 */
	if ( bGet || bHead ) {
		if ( !__xrtHttpDateConditionRead(
			pFields,
			iFieldCount,
			XRT_STR_LITERAL("If-Modified-Since"),
			&bPresent,
			&bValid,
			&iDate
		) ) {
			return XHTTP_PRECONDITION_ERROR;
		}
		if ( bPresent && bValid &&
			Current.HasLastModified &&
			(__xrtHttpTimeSecond(Current.LastModified) <=
			 __xrtHttpTimeSecond(iDate)) ) {
			return XHTTP_PRECONDITION_NOT_MODIFIED;
		}
	}
	return XHTTP_PRECONDITION_PROCEED;
}



/* 判断 If-Range 是否与当前表示的强验证器匹配。 */
XRT_API bool xrtHttpIfRangeMatch(
	xstrview Value,
	const xhttprepresentation* pCurrent
)
{
	xhttpetag Tag;
	xhttprepresentation Current;
	xtime iDate;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCurrent, sizeof(Current)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Current, pCurrent, sizeof(Current));
	if ( (!Current.Exists &&
		(Current.HasETag || Current.HasLastModified)) ||
		(Current.LastModifiedStrong &&
		 !Current.HasLastModified) ||
		(Current.HasETag &&
		 !__xrtHttpETagValid(&Current.ETag)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Value = xrtHttpOwsTrim(Value);
	if ( __xrtHttpETagParseValue(Value, &Tag) ) {
		return Current.Exists &&
			Current.HasETag &&
			!Tag.Weak &&
			!Current.ETag.Weak &&
			__xrtHttpETagValid(&Tag) &&
			(Tag.Opaque.Size == Current.ETag.Opaque.Size) &&
			((Tag.Opaque.Size == 0) ||
			 (memcmp(
				Tag.Opaque.Data,
				Current.ETag.Opaque.Data,
				Tag.Opaque.Size
			) == 0));
	}
	if ( !__xrtTimeParseHTTPDateValue(Value, &iDate) ) {
		return false;
	}
	return Current.Exists &&
		Current.HasLastModified &&
		Current.LastModifiedStrong &&
		(__xrtHttpTimeSecond(Current.LastModified) ==
		 __xrtHttpTimeSecond(iDate));
}

#endif
