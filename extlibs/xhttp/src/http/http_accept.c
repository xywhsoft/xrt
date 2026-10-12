#include "../internal/xrt_http.h"

#include <xrt/http_accept.h>



#if defined(XHTTP_FEATURE_HTTP_ACCEPT)

/* 检查当前参数之前是否已经出现同名参数。 */
static bool __xrtHttpAcceptParamSeen(
	xstrview Parameters,
	size_t iBefore,
	xstrview Name
)
{
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;

	while ( iOffset < iBefore ) {
		Next = xrtHttpParamNext(
			Parameters, &iOffset, &Param
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( xrtHttpTokenEqual(Param.Name, Name) ) {
			return true;
		}
	}
	return false;
}



/* 严格解析一个媒体范围成员及其媒体参数和质量值。 */
static bool __xrtHttpMediaRangeParse(
	xstrview Member,
	xhttpmediarange* pRange
)
{
	xhttpmediarange Range;
	xstrview Main;
	xstrview Parameters = { NULL, 0 };
	xhttpparam Param;
	xhttpnext Next;
	cstr sSemi;
	cstr sSlash;
	size_t iOffset = 0;
	size_t iBefore;
	bool bQuality = false;

	memset(&Range, 0, sizeof(Range));
	Range.Quality = XHTTP_QUALITY_MAX;
	sSemi = (cstr)memchr(Member.Data, ';', Member.Size);
	if ( sSemi == NULL ) {
		Main = Member;
	} else {
		Main = (xstrview){
			Member.Data, (size_t)(sSemi - Member.Data)
		};
		Parameters = xrtHttpOwsTrim((xstrview){
			sSemi + 1,
			Member.Size - (size_t)(sSemi + 1 - Member.Data)
		});
		if ( Parameters.Size == 0 ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		Range.Parameters = Parameters;
	}
	Main = xrtHttpOwsTrim(Main);
	sSlash = (cstr)memchr(Main.Data, '/', Main.Size);
	if ( (sSlash == NULL) ||
		(memchr(
			sSlash + 1, '/',
			Main.Size - (size_t)(sSlash + 1 - Main.Data)
		) != NULL) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	Range.Type = (xstrview){
		Main.Data, (size_t)(sSlash - Main.Data)
	};
	Range.Subtype = (xstrview){
		sSlash + 1,
		Main.Size - (size_t)(sSlash + 1 - Main.Data)
	};
	if ( (Range.Type.Size == 1u) &&
		(Range.Type.Data[0] == '*') ) {
		if ( (Range.Subtype.Size != 1u) ||
			(Range.Subtype.Data[0] != '*') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		Range.Specificity = XHTTP_MEDIA_RANGE_ANY;
	} else if ( !xrtHttpTokenValid(Range.Type) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	} else if ( (Range.Subtype.Size == 1u) &&
		(Range.Subtype.Data[0] == '*') ) {
		Range.Specificity = XHTTP_MEDIA_RANGE_TYPE;
	} else if ( xrtHttpTokenValid(Range.Subtype) ) {
		Range.Specificity = XHTTP_MEDIA_RANGE_EXACT;
	} else {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}

	/* q 无论位于何处都表示权重，其余参数全部参与媒体范围匹配。 */
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtHttpParamNext(
			Parameters, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( __xrtHttpAcceptParamSeen(
			Parameters, iBefore, Param.Name
		) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("q")
		) ) {
			if ( bQuality ||
				((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
				((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpQualityParse(
					Param.Value, &Range.Quality
				) ) {
				xrtSetErrorKind(XERR_VALUE);
				return false;
			}
			bQuality = true;
			continue;
		}
		if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( Range.ParameterCount == SIZE_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		Range.ParameterCount++;
	}
	*pRange = Range;
	return true;
}



/* 在已验证媒体范围中迭代非 q 媒体参数。 */
static xhttpnext __xrtHttpMediaRangeParamNextValidated(
	const xhttpmediarange* pRange,
	size_t iOffset,
	size_t* pNext,
	xhttpparam* pParam
)
{
	xhttpparam Param;
	xhttpnext Next;

	for ( ;; ) {
		Next = xrtHttpParamNext(
			pRange->Parameters, &iOffset, &Param
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			if ( Next == XHTTP_NEXT_END ) {
				*pNext = iOffset;
			}
			return Next;
		}
		if ( !xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("q")
		) ) {
			*pNext = iOffset;
			*pParam = Param;
			return XHTTP_NEXT_ITEM;
		}
	}
}



/* 验证媒体范围结构仍满足解析器发布的规范形态。 */
static bool __xrtHttpMediaRangeValid(
	const xhttpmediarange* pRange
)
{
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset;
	size_t iBefore;
	size_t iParameters = 0;
	uint16 iQuality = XHTTP_QUALITY_MAX;
	bool bQuality = false;

	if ( (pRange == NULL) ||
		!__xhttpViewValid(pRange->Type) ||
		!__xhttpViewValid(pRange->Subtype) ||
		!__xhttpViewValid(pRange->Parameters) ||
		(pRange->Quality > XHTTP_QUALITY_MAX) ) {
		return false;
	}
	if ( pRange->Specificity == XHTTP_MEDIA_RANGE_ANY ) {
		if ( (pRange->Type.Size != 1u) ||
			(pRange->Type.Data[0] != '*') ||
			(pRange->Subtype.Size != 1u) ||
			(pRange->Subtype.Data[0] != '*') ) {
			return false;
		}
	} else if ( pRange->Specificity ==
		XHTTP_MEDIA_RANGE_TYPE ) {
		if ( !xrtHttpTokenValid(pRange->Type) ||
			(pRange->Subtype.Size != 1u) ||
			(pRange->Subtype.Data[0] != '*') ) {
			return false;
		}
	} else if ( pRange->Specificity ==
		XHTTP_MEDIA_RANGE_EXACT ) {
		if ( !xrtHttpTokenValid(pRange->Type) ||
			!xrtHttpTokenValid(pRange->Subtype) ) {
			return false;
		}
	} else {
		return false;
	}

	/* q 必须唯一且等于发布权重；其他参数必须带值并计入数量。 */
	iOffset = 0;
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtHttpParamNext(
			pRange->Parameters, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return (iParameters == pRange->ParameterCount) &&
				(iQuality == pRange->Quality);
		}
		if ( __xrtHttpAcceptParamSeen(
				pRange->Parameters, iBefore, Param.Name
			) ) {
			return false;
		}
		if ( xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("q")
		) ) {
			if ( bQuality ||
				((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
				((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				!xrtHttpQualityParse(Param.Value, &iQuality) ) {
				return false;
			}
			bQuality = true;
			continue;
		}
		if ( ((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
			(iParameters == SIZE_MAX) ) {
			return false;
		}
		iParameters++;
	}
}



/* 比较两个参数解码后的语义字节；charset 值按规范忽略 ASCII 大小写。 */
static bool __xrtHttpAcceptParamValueEqual(
	const xhttpparam* pLeft,
	const xhttpparam* pRight
)
{
	size_t iLeft = 0;
	size_t iRight = 0;
	uint8 iLeftByte;
	uint8 iRightByte;
	bool bLeft;
	bool bRight;
	bool bFold;

	bFold = xrtHttpTokenEqual(
		pLeft->Name, XRT_STR_LITERAL("charset")
	);
	for ( ;; ) {
		bLeft = __xhttpParamSemanticNext(
			pLeft, &iLeft, &iLeftByte
		);
		bRight = __xhttpParamSemanticNext(
			pRight, &iRight, &iRightByte
		);
		if ( bLeft != bRight ) {
			return false;
		}
		if ( !bLeft ) {
			return true;
		}
		if ( bFold ) {
			iLeftByte = __xhttpAsciiLower(iLeftByte);
			iRightByte = __xhttpAsciiLower(iRightByte);
		}
		if ( iLeftByte != iRightByte ) {
			return false;
		}
	}
}



/* 在两个已验证结构之间执行媒体范围匹配。 */
static xhttpnext __xrtHttpMediaRangeMatch(
	const xhttpmediarange* pRange,
	const xmediatype* pType
)
{
	xhttpparam RangeParam;
	xhttpparam TypeParam;
	xhttpnext Next;
	size_t iOffset = 0;

	if ( (pRange->Specificity != XHTTP_MEDIA_RANGE_ANY) &&
		!xrtHttpTokenEqual(pRange->Type, pType->Type) ) {
		return XHTTP_NEXT_END;
	}
	if ( (pRange->Specificity == XHTTP_MEDIA_RANGE_EXACT) &&
		!xrtHttpTokenEqual(pRange->Subtype, pType->Subtype) ) {
		return XHTTP_NEXT_END;
	}
	for ( ;; ) {
		Next = __xrtHttpMediaRangeParamNextValidated(
			pRange, iOffset, &iOffset, &RangeParam
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return XHTTP_NEXT_ITEM;
		}
		Next = xrtHttpParamFind(
			pType->Parameters, RangeParam.Name, &TypeParam
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return Next;
		}
		if ( !__xrtHttpAcceptParamValueEqual(
			&RangeParam, &TypeParam
		) ) {
			return XHTTP_NEXT_END;
		}
	}
}



/* 验证候选媒体类型数组和输出索引的完整内存及语法边界。 */
static bool __xrtHttpAcceptMediaTypesValid(
	const xstrview* pMediaTypes,
	size_t iCount,
	size_t* pIndex
)
{
	xmediatype Type;
	xstrview MediaType;
	size_t i;

	if ( !xrtMemRangeValid(pIndex, sizeof(*pIndex)) ||
		((pMediaTypes == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pMediaTypes))) ||
		!xrtMemRangeValid(
			pMediaTypes, iCount * sizeof(*pMediaTypes)
		) || xrtMemRangesOverlap(
			pMediaTypes, iCount * sizeof(*pMediaTypes),
			pIndex, sizeof(*pIndex)
		) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&MediaType,
			(const uint8*)pMediaTypes +
				i * sizeof(*pMediaTypes),
			sizeof(MediaType)
		);
		if ( !__xhttpViewValid(MediaType) ||
			xrtMemRangesOverlap(
				MediaType.Data, MediaType.Size,
				pIndex, sizeof(*pIndex)
			) || !xrtHttpMediaTypeParse(MediaType, &Type) ) {
			return false;
		}
	}
	return true;
}



/* 判断一个范围是否比当前匹配更具体。 */
static bool __xrtHttpAcceptMatchBetter(
	const xhttpmediarange* pRange,
	const xhttpacceptmatch* pMatch,
	bool bMatched
)
{
	if ( !bMatched ) {
		return true;
	}
	if ( pRange->Specificity != pMatch->Specificity ) {
		return pRange->Specificity > pMatch->Specificity;
	}
	return pRange->ParameterCount > pMatch->ParameterCount;
}



/* 初始化 Accept 字段游标。 */
XHTTP_API void xrtHttpAcceptCursorInit(xhttpacceptcursor* pCursor)
{
	xhttpacceptcursor Cursor = { 0, 0 };

	if ( !xrtMemRangeValid(pCursor, sizeof(Cursor)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 迭代单个 Accept 字段值。 */
XHTTP_API xhttpnext xrtHttpMediaRangeNext(
	xstrview List,
	size_t* pOffset,
	xhttpmediarange* pRange
)
{
	xhttpmediarange Range;
	xstrview Member;
	xhttpnext Next;
	size_t iOffset;
	size_t iNext;

	memset(&Range, 0, sizeof(Range));
	if ( !__xhttpViewValid(List) ||
		!xrtMemRangeValid(pOffset, sizeof(iOffset)) ||
		!xrtMemRangeValid(pRange, sizeof(Range)) ||
		xrtMemRangesOverlap(
			List.Data, List.Size, pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			List.Data, List.Size, pRange, sizeof(Range)
		) || xrtMemRangesOverlap(
			pOffset, sizeof(iOffset), pRange, sizeof(Range)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	memcpy(pRange, &Range, sizeof(Range));
	if ( iOffset > List.Size ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	Next = xrtHttpListNext(
		List, iOffset, &iNext, &Member
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pOffset, &iNext, sizeof(iNext));
		}
		return Next;
	}
	if ( !__xrtHttpMediaRangeParse(Member, &Range) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pRange, &Range, sizeof(Range));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XHTTP_NEXT_ITEM;
}



/* 迭代已解析媒体范围中的非 q 参数。 */
XHTTP_API xhttpnext xrtHttpMediaRangeParamNext(
	const xhttpmediarange* pInputRange,
	size_t* pOffset,
	xhttpparam* pOutput
)
{
	xhttpmediarange Range;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset;
	size_t iNext;

	memset(&Param, 0, sizeof(Param));
	if ( !xrtMemRangeValid(pInputRange, sizeof(Range)) ||
		!xrtMemRangeValid(pOffset, sizeof(iOffset)) ||
		!xrtMemRangeValid(pOutput, sizeof(Param)) ||
		xrtMemRangesOverlap(
			pInputRange, sizeof(Range),
			pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			pInputRange, sizeof(Range),
			pOutput, sizeof(Param)
		) || xrtMemRangesOverlap(
			pOffset, sizeof(iOffset),
			pOutput, sizeof(Param)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Range, pInputRange, sizeof(Range));
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( !__xrtHttpMediaRangeValid(&Range) ||
		xrtMemRangesOverlap(
			Range.Type.Data, Range.Type.Size,
			pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			Range.Subtype.Data, Range.Subtype.Size,
			pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			Range.Parameters.Data, Range.Parameters.Size,
			pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			Range.Type.Data, Range.Type.Size,
			pOutput, sizeof(Param)
		) || xrtMemRangesOverlap(
			Range.Subtype.Data, Range.Subtype.Size,
			pOutput, sizeof(Param)
		) || xrtMemRangesOverlap(
			Range.Parameters.Data, Range.Parameters.Size,
			pOutput, sizeof(Param)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOutput, &Param, sizeof(Param));
	if ( iOffset > Range.Parameters.Size ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	Next = __xrtHttpMediaRangeParamNextValidated(
		&Range, iOffset, &iNext, &Param
	);
	if ( Next == XHTTP_NEXT_ITEM ) {
		memcpy(pOutput, &Param, sizeof(Param));
		memcpy(pOffset, &iNext, sizeof(iNext));
	} else if ( Next == XHTTP_NEXT_END ) {
		memcpy(pOffset, &iNext, sizeof(iNext));
	}
	return Next;
}



/* 跨越重复 Accept 字段迭代媒体范围。 */
XHTTP_API xhttpnext xrtHttpAcceptNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptcursor* pCursor,
	xhttpmediarange* pRange
)
{
	xhttpacceptcursor Cursor;
	xhttpacceptcursor End;
	xhttpfield Field;
	xhttpmediarange Range;
	xhttpnext Next;
	size_t iOffset;

	memset(&Range, 0, sizeof(Range));
	if ( !__xhttpFieldArrayValid(pFields, iCount) ||
		!xrtMemRangeValid(pCursor, sizeof(Cursor)) ||
		!xrtMemRangeValid(pRange, sizeof(Range)) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xhttpFieldArrayOverlap(
			pFields, iCount, pRange, sizeof(Range)
		) || xrtMemRangesOverlap(
			pCursor, sizeof(Cursor), pRange, sizeof(Range)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(pRange, &Range, sizeof(Range));
	if ( (Cursor.Field > iCount) ||
		((Cursor.Field == iCount) &&
		 (Cursor.Offset != 0)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	while ( Cursor.Field < iCount ) {
		__xhttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Accept")
		) ) {
			if ( Cursor.Offset != 0 ) {
				xrtSetErrorKind(XERR_ARGUMENT);
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		iOffset = Cursor.Offset;
		Next = xrtHttpMediaRangeNext(
			Field.Value, &iOffset, &Range
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			Cursor.Offset = iOffset;
			memcpy(pRange, &Range, sizeof(Range));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return XHTTP_NEXT_ITEM;
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	End.Field = iCount;
	End.Offset = 0;
	memcpy(pCursor, &End, sizeof(End));
	return XHTTP_NEXT_END;
}



/* 判断已解析媒体范围是否匹配已解析媒体类型。 */
XHTTP_API xhttpnext xrtHttpMediaRangeMatch(
	const xhttpmediarange* pInputRange,
	const xmediatype* pInputType
)
{
	xhttpmediarange Range;
	xmediatype Type;

	if ( !xrtMemRangeValid(pInputRange, sizeof(Range)) ||
		!xrtMemRangeValid(pInputType, sizeof(Type)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Range, pInputRange, sizeof(Range));
	memcpy(&Type, pInputType, sizeof(Type));
	if ( !__xrtHttpMediaRangeValid(&Range) ||
		!__xrtHttpMediaTypeValid(&Type) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	return __xrtHttpMediaRangeMatch(&Range, &Type);
}



/* 计算具体媒体类型的有效 Accept 匹配。 */
XHTTP_API bool xrtHttpAcceptMatch(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview MediaType,
	xhttpacceptmatch* pOutput
)
{
	xhttpacceptcursor Cursor;
	xhttpacceptmatch Match;
	xhttpmediarange Range;
	xmediatype Type;
	xhttpfield Field;
	xhttpnext Next;
	size_t iOrder = 0;
	size_t i;
	bool bPresent = false;
	bool bMatched = false;

	memset(&Match, 0, sizeof(Match));
	Match.Field = XRT_NPOS;
	Match.Order = XRT_NPOS;
	if ( !__xhttpFieldArrayValid(pFields, iCount) ||
		!__xhttpViewValid(MediaType) ||
		!xrtMemRangeValid(pOutput, sizeof(Match)) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Match)
		) || xrtMemRangesOverlap(
			MediaType.Data, MediaType.Size,
			pOutput, sizeof(Match)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(pOutput, 0, sizeof(Match));
	if ( !xrtHttpMediaTypeParse(MediaType, &Type) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Accept")
		) ) {
			bPresent = true;
		}
	}
	if ( !bPresent ) {
		Match.Quality = XHTTP_QUALITY_MAX;
		memcpy(pOutput, &Match, sizeof(Match));
		return true;
	}
	xrtHttpAcceptCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpAcceptNext(
			pFields, iCount, &Cursor, &Range
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		Next = __xrtHttpMediaRangeMatch(&Range, &Type);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( (Next == XHTTP_NEXT_ITEM) &&
			__xrtHttpAcceptMatchBetter(
				&Range, &Match, bMatched
			) ) {
			Match.Field = Cursor.Field;
			Match.Order = iOrder;
			Match.ParameterCount = Range.ParameterCount;
			Match.Quality = Range.Quality;
			Match.Specificity = Range.Specificity;
			bMatched = true;
		}
		if ( iOrder == SIZE_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iOrder++;
	}
	memcpy(pOutput, &Match, sizeof(Match));
	return true;
}



/* 返回媒体类型的有效 Accept 质量值。 */
XHTTP_API uint16 xrtHttpAcceptQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview MediaType
)
{
	xhttpacceptmatch Match;

	return xrtHttpAcceptMatch(
		pFields, iCount, MediaType, &Match
	) ? Match.Quality : 0;
}



/* 从服务端偏好顺序中选择客户端质量最高的媒体类型。 */
XHTTP_API xhttpnext xrtHttpAcceptSelect(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pMediaTypes,
	size_t iMediaTypeCount,
	size_t* pIndex
)
{
	xhttpacceptmatch Match;
	xstrview MediaType;
	size_t iSelected = XRT_NPOS;
	uint16 iQuality = 0;
	size_t i;

	if ( !__xhttpFieldArrayValid(
		pFields, iFieldCount
	) || !__xrtHttpAcceptMediaTypesValid(
		pMediaTypes, iMediaTypeCount, pIndex
	) || __xhttpFieldArrayOverlap(
			pFields, iFieldCount,
			pIndex, sizeof(iSelected)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pIndex, &iSelected, sizeof(iSelected));
	for ( i = 0; i < iMediaTypeCount; i++ ) {
		memcpy(
			&MediaType,
			(const uint8*)pMediaTypes +
				i * sizeof(*pMediaTypes),
			sizeof(MediaType)
		);
		if ( !xrtHttpAcceptMatch(
			pFields, iFieldCount, MediaType, &Match
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Match.Quality > iQuality ) {
			iQuality = Match.Quality;
			iSelected = i;
		}
	}
	if ( iSelected == XRT_NPOS ) {
		return XHTTP_NEXT_END;
	}
	memcpy(pIndex, &iSelected, sizeof(iSelected));
	return XHTTP_NEXT_ITEM;
}

#endif
