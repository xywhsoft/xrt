#include "../internal/xrt_http_cache_range.h"



#if defined(XRT_FEATURE_HTTP_CACHE_RANGE)

/* 初始化尚未收到任何协议数据的片段输入。 */
XRT_API void xrtHttpCacheFragmentInputInit(
	xhttpcachefragmentinput* pInput
)
{
	if ( pInput != NULL ) {
		memset(pInput, 0, sizeof(*pInput));
	}
}



/* 判断闭区间是否合法。 */
bool __xrtHttpCacheRangeValid(
	const xhttpbyterange* pRange
)
{
	return (pRange != NULL) &&
		(pRange->First <= pRange->Last);
}



/* 判断两个闭区间是否重叠或相邻。 */
bool __xrtHttpCacheRangeJoins(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
)
{
	if ( pRight->First <= pLeft->Last ) {
		return true;
	}
	return (pLeft->Last != UINT64_MAX) &&
		(pRight->First == (pLeft->Last + UINT64_C(1)));
}



/* 判断片段是否覆盖完整表示。 */
static bool __xrtHttpCacheFragmentIsComplete(
	const xhttpcachefragment* pFragment
)
{
	if ( (pFragment->Flags &
		 XHTTP_CACHE_FRAGMENT_HAS_LENGTH) == 0 ) {
		return false;
	}
	if ( pFragment->Length == 0 ) {
		return (pFragment->Flags &
			XHTTP_CACHE_FRAGMENT_HAS_RANGE) == 0;
	}
	return ((pFragment->Flags &
			 XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0) &&
		(pFragment->Range.First == 0) &&
		(pFragment->Range.Last ==
		 (pFragment->Length - UINT64_C(1)));
}



/* 判断公开片段的 Header、区间和完整性标志是否自洽。 */
XRT_API bool xrtHttpCacheFragmentValid(
	const xhttpcachefragment* pFragment
)
{
	bool bComplete;
	bool bHasRange;

	if ( (pFragment == NULL) ||
		((pFragment->SourceStatus != XHTTP_STATUS_OK) &&
		 (pFragment->SourceStatus !=
		  XHTTP_STATUS_PARTIAL_CONTENT)) ||
		((pFragment->Flags & ~(
			XHTTP_CACHE_FRAGMENT_HAS_RANGE |
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH
		 )) != 0) ||
		((pFragment->Entry.Flags &
		  XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0) ||
		!xrtHttpCacheEntryValid(&pFragment->Entry) ) {
		return false;
	}
	bHasRange = (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0;
	if ( bHasRange ) {
		if ( !__xrtHttpCacheRangeValid(
			&pFragment->Range
		) ) {
			return false;
		}
		if ( (pFragment->SourceStatus ==
			 XHTTP_STATUS_OK) &&
			(pFragment->Range.First != 0) ) {
			return false;
		}
		if ( ((pFragment->Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0) &&
			(pFragment->Range.Last >=
			 pFragment->Length) ) {
			return false;
		}
	} else if ( ((pFragment->Flags &
				  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) == 0) ||
		(pFragment->Length != 0) ||
		(pFragment->SourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		return false;
	}

	bComplete = __xrtHttpCacheFragmentIsComplete(
		pFragment
	);
	return bComplete ==
		((pFragment->Entry.Flags &
		  XHTTP_CACHE_ENTRY_PARTIAL) == 0);
}



/* 判断片段是否已经覆盖完整表示。 */
XRT_API bool xrtHttpCacheFragmentComplete(
	const xhttpcachefragment* pFragment
)
{
	return xrtHttpCacheFragmentValid(pFragment) &&
		__xrtHttpCacheFragmentIsComplete(pFragment);
}



/* 判断输入和输出计划是否发生任何内存重叠。 */
static bool __xrtHttpCacheFragmentPlanOverlap(
	const xhttpcachefragmentinput* pInput,
	const xhttpcachefragmentplan* pPlan
)
{
	if ( __xrtRangesOverlap(
		pInput, sizeof(*pInput),
		pPlan, sizeof(*pPlan)
	) || __xrtRangesOverlap(
		pInput->Method.Data,
		pInput->Method.Size,
		pPlan,
		sizeof(*pPlan)
	) || __xrtHttpFieldArrayOverlap(
		pInput->Fields,
		pInput->FieldCount,
		pPlan,
		sizeof(*pPlan)
	) || __xrtHttpFieldArrayOverlap(
		pInput->RangeFields,
		pInput->RangeFieldCount,
		pPlan,
		sizeof(*pPlan)
	) ) {
		return true;
	}
	return false;
}



/* 验证片段输入结构及其借用视图。 */
static bool __xrtHttpCacheFragmentInputValid(
	const xhttpcachefragmentinput* pInput
)
{
	if ( (pInput == NULL) ||
		((pInput->Flags & ~(
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE |
			XHTTP_CACHE_FRAGMENT_MULTIPART_PART |
			XHTTP_CACHE_FRAGMENT_TRANSFORMED
		 )) != 0) ||
		!__xrtHttpViewValid(pInput->Method) ||
		!xrtHttpTokenValid(pInput->Method) ||
		!__xrtHttpCacheFieldsValid(
			pInput->Fields,
			pInput->FieldCount
		) ||
		!__xrtHttpCacheFieldsValid(
			pInput->RangeFields,
			pInput->RangeFieldCount
		) ) {
		return false;
	}
	return true;
}



/* 记录协议性跳过并保持其余计划字段为零。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragmentSkip(
	xhttpcachefragmentplan* pPlan,
	uint32 iReasons
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_SKIP;
	pPlan->Reasons = iReasons;
	return pPlan->Decision;
}



/* 从指定字段组读取唯一、满足形式的 bytes Content-Range。 */
static bool __xrtHttpCacheContentRangeRead(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentrange* pRange
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Range")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	return (iMatches == 1) &&
		__xrtHttpContentRangeParseValue(
			xrtHttpOwsTrim(Value), pRange
		) &&
		pRange->Satisfied;
}



/* 判断已接收字节数与声明的完整或截断消息长度是否一致。 */
static bool __xrtHttpCacheBodyLengthValid(
	uint64 iReceived,
	uint64 iDeclared,
	bool bComplete
)
{
	return bComplete ?
		(iReceived == iDeclared) :
		(iReceived < iDeclared);
}



/* 计算从声明范围起点开始实际收到的闭区间。 */
static bool __xrtHttpCacheReceivedRange(
	uint64 iFirst,
	uint64 iSize,
	xhttpbyterange* pRange
)
{
	uint64 iTail;

	if ( iSize == 0 ) {
		return false;
	}
	iTail = iSize - UINT64_C(1);
	if ( iTail > (UINT64_MAX - iFirst) ) {
		return false;
	}
	pRange->First = iFirst;
	pRange->Last = iFirst + iTail;
	return true;
}



/* 规划完整或截断 200 响应片段。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragment200(
	const xhttpcachefragmentinput* pInput,
	const xrt_http_cache_validator* pValidator,
	xhttpcachefragmentplan* pPlan
)
{
	xhttpcachefragment Fragment;
	bool bComplete = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE) != 0;

	if ( (pInput->RangeFields != NULL) ||
		(pInput->RangeFieldCount != 0) ||
		((pInput->Flags &
		  XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!pValidator->ContentLengthValid ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!__xrtHttpCacheBodyLengthValid(
			pInput->BodySize,
			pValidator->ContentLength,
			bComplete
		) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bComplete && (pInput->BodySize == 0) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_EMPTY
		);
	}

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Fields = pInput->Fields;
	Fragment.Entry.FieldCount = pInput->FieldCount;
	Fragment.Entry.ResponseTime = pInput->ResponseTime;
	Fragment.SourceStatus = XHTTP_STATUS_OK;
	if ( pInput->BodySize != 0 ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_RANGE;
		Fragment.Range.First = 0;
		Fragment.Range.Last =
			pInput->BodySize - UINT64_C(1);
	}
	if ( pValidator->ContentLengthPresent ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = pValidator->ContentLength;
	} else if ( bComplete ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = pInput->BodySize;
	}
	if ( !bComplete ) {
		Fragment.Entry.Flags =
			XHTTP_CACHE_ENTRY_PARTIAL;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Fragment = Fragment;
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_STORE;
	if ( !bComplete ) {
		pPlan->Actions =
			XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}



/* 判断一个 Content-Range 声明区间的可表示字节数。 */
static bool __xrtHttpCacheDeclaredRangeSize(
	const xhttpcontentrange* pRange,
	uint64* pSize
)
{
	uint64 iDifference =
		pRange->Last - pRange->First;

	if ( iDifference == UINT64_MAX ) {
		return false;
	}
	*pSize = iDifference + UINT64_C(1);
	return true;
}



/* 规划单段或 multipart part 的 206 响应片段。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragment206(
	const xhttpcachefragmentinput* pInput,
	const xrt_http_cache_validator* pValidator,
	xhttpcachefragmentplan* pPlan
)
{
	const xhttpfield* pRangeFields = pInput->RangeFields;
	size_t iRangeCount = pInput->RangeFieldCount;
	xhttpcachefragment Fragment;
	xhttpcontentrange ContentRange;
	uint64 iDeclared = 0;
	bool bDeclared;
	bool bComplete = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE) != 0;
	bool bMultipart = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0;

	if ( !bMultipart &&
		(pRangeFields == NULL) &&
		(iRangeCount == 0) ) {
		pRangeFields = pInput->Fields;
		iRangeCount = pInput->FieldCount;
	}
	if ( bMultipart &&
		((pRangeFields == NULL) ||
		 (iRangeCount == 0)) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( !__xrtHttpCacheContentRangeRead(
		pRangeFields,
		iRangeCount,
		&ContentRange
	) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!pValidator->ContentLengthValid ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH
		);
	}

	bDeclared = __xrtHttpCacheDeclaredRangeSize(
		&ContentRange, &iDeclared
	);
	if ( bComplete &&
		(!bDeclared ||
		 (pInput->BodySize != iDeclared)) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bComplete &&
		((pInput->BodySize == 0) ||
		 (bDeclared &&
		  (pInput->BodySize >= iDeclared))) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			(pInput->BodySize == 0) ?
				XHTTP_CACHE_FRAGMENT_REASON_EMPTY :
				XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bMultipart &&
		pValidator->ContentLengthPresent &&
		!__xrtHttpCacheBodyLengthValid(
			pInput->BodySize,
			pValidator->ContentLength,
			bComplete
		) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Fields = pInput->Fields;
	Fragment.Entry.FieldCount = pInput->FieldCount;
	Fragment.Entry.ResponseTime = pInput->ResponseTime;
	Fragment.SourceStatus =
		XHTTP_STATUS_PARTIAL_CONTENT;
	Fragment.Flags = XHTTP_CACHE_FRAGMENT_HAS_RANGE;
	if ( !__xrtHttpCacheReceivedRange(
		ContentRange.First,
		pInput->BodySize,
		&Fragment.Range
	) || (Fragment.Range.Last >
		ContentRange.Last) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( ContentRange.HasLength ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = ContentRange.Length;
	}
	if ( !__xrtHttpCacheFragmentIsComplete(
		&Fragment
	) ) {
		Fragment.Entry.Flags =
			XHTTP_CACHE_ENTRY_PARTIAL;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Fragment = Fragment;
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_STORE;
	pPlan->Actions =
		XHTTP_CACHE_FRAGMENT_AS_200 |
		XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE |
		XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH;
	if ( __xrtHttpCacheFragmentIsComplete(
		&Fragment
	) ) {
		pPlan->Actions |=
			XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH;
	} else {
		pPlan->Actions |=
			XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}



/* 把收到的 200 或 206 数据规范化为缓存片段。 */
XRT_API xhttpcachefragmentdecision xrtHttpCacheFragmentPlan(
	const xhttpcachefragmentinput* pInput,
	xhttpcachefragmentplan* pPlan
)
{
	xrt_http_cache_validator Validator;
	uint32 iReasons = 0;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheFragmentInputValid(pInput) ||
		__xrtHttpCacheFragmentPlanOverlap(
			pInput, pPlan
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_FRAGMENT_ERROR;
	}
	if ( !xrtHttpMethodEqual(
		pInput->Method,
		XRT_STR_LITERAL("GET")
	) ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_METHOD;
	}
	if ( (pInput->Status != XHTTP_STATUS_OK) &&
		(pInput->Status !=
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_STATUS;
	}
	if ( (pInput->Flags &
		 XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE) == 0 ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_HEADERS;
	}
	if ( (pInput->Flags &
		 XHTTP_CACHE_FRAGMENT_TRANSFORMED) != 0 ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED;
	}
	if ( iReasons != 0 ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan, iReasons
		);
	}
	if ( !__xrtHttpCacheValidatorRead(
		pInput->Fields,
		pInput->FieldCount,
		&Validator
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_FRAGMENT_ERROR;
	}
	if ( pInput->Status == XHTTP_STATUS_OK ) {
		return __xrtHttpCacheFragment200(
			pInput, &Validator, pPlan
		);
	}
	return __xrtHttpCacheFragment206(
		pInput, &Validator, pPlan
	);
}

#endif
