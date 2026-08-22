#include "../internal/xrt_http.h"

#include <xrt/http_language.h>



#if defined(XRT_FEATURE_HTTP_LANGUAGE)

/* 按 ASCII 大小写不敏感规则比较两个等长语言文本。 */
static bool __xrtHttpLanguageEqual(
	xstrview Left,
	xstrview Right
)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xrtHttpAsciiLower(
			(uint8)Left.Data[i]
		) != __xrtHttpAsciiLower(
			(uint8)Right.Data[i]
		) ) {
			return false;
		}
	}
	return true;
}



/* 在已验证文本之间执行 RFC 4647 Basic Filtering。 */
static bool __xrtHttpLanguageBasicMatch(
	xstrview Range,
	xstrview Tag
)
{
	size_t i;

	if ( (Range.Size == 1u) &&
		(Range.Data[0] == '*') ) {
		return true;
	}
	if ( Range.Size > Tag.Size ) {
		return false;
	}
	for ( i = 0; i < Range.Size; i++ ) {
		if ( __xrtHttpAsciiLower(
			(uint8)Range.Data[i]
		) != __xrtHttpAsciiLower(
			(uint8)Tag.Data[i]
		) ) {
			return false;
		}
	}
	return (Range.Size == Tag.Size) ||
		(Tag.Data[Range.Size] == '-');
}



/* 验证语言标签数组和输出索引之间没有非法范围或别名。 */
static bool __xrtHttpLanguageArrayValid(
	const xstrview* pTags,
	size_t iCount,
	size_t* pIndex
)
{
	xstrview Tag;
	size_t i;

	if ( !__xrtRangeValid(pIndex, sizeof(*pIndex)) ||
		((pTags == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pTags))) ||
		!__xrtRangeValid(
			pTags, iCount * sizeof(*pTags)
		) || __xrtRangesOverlap(
			pTags, iCount * sizeof(*pTags),
			pIndex, sizeof(*pIndex)
		) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Tag,
			(const uint8*)pTags + i * sizeof(*pTags),
			sizeof(Tag)
		);
		if ( !xrtHttpLanguageTagValid(Tag) ||
			__xrtRangesOverlap(
				Tag.Data, Tag.Size,
				pIndex, sizeof(*pIndex)
			) ) {
			return false;
		}
	}
	return true;
}



/* 查找与指定范围完全相同的第一个可用语言标签。 */
static size_t __xrtHttpLanguageExactFind(
	const xstrview* pTags,
	size_t iCount,
	xstrview Range
)
{
	xstrview Tag;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Tag,
			(const uint8*)pTags + i * sizeof(*pTags),
			sizeof(Tag)
		);
		if ( __xrtHttpLanguageEqual(Range, Tag) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 按 Lookup 截断规则查找一个语言范围的最佳可用标签。 */
static size_t __xrtHttpLanguageLookupRange(
	xstrview Range,
	const xstrview* pTags,
	size_t iCount
)
{
	size_t iSize = Range.Size;
	size_t iFound;
	size_t iDash;
	size_t iLast;

	while ( iSize != 0 ) {
		iFound = __xrtHttpLanguageExactFind(
			pTags,
			iCount,
			(xstrview){ Range.Data, iSize }
		);
		if ( iFound != XRT_NPOS ) {
			return iFound;
		}
		iDash = iSize;
		while ( (iDash != 0) &&
			(Range.Data[iDash - 1u] != '-') ) {
			iDash--;
		}
		if ( iDash == 0 ) {
			break;
		}
		iSize = iDash - 1u;

		/* extension 或 private-use singleton 与刚删除的尾项一并移除。 */
		iLast = iSize;
		while ( (iLast != 0) &&
			(Range.Data[iLast - 1u] != '-') ) {
			iLast--;
		}
		if ( (iSize - iLast) == 1u ) {
			iSize = iLast == 0 ? 0 : iLast - 1u;
		}
	}
	return XRT_NPOS;
}



/* 判断文本是否是 basic language range。 */
XRT_API bool xrtHttpLanguageRangeValid(xstrview Range)
{
	return __xrtHttpLanguageTextValid(
		Range, true, false, NULL
	);
}



/* 判断文本是否是可匹配的基本语言标签。 */
XRT_API bool xrtHttpLanguageTagValid(xstrview Tag)
{
	return __xrtHttpLanguageTextValid(
		Tag, false, false, NULL
	);
}



/* 初始化 Accept-Language 字段游标。 */
XRT_API void xrtHttpLanguageCursorInit(
	xhttplanguagecursor* pCursor
)
{
	xhttplanguagecursor Cursor = { 0, 0 };

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 迭代一个 Accept-Language 字段值。 */
XRT_API xhttpnext xrtHttpLanguageRangeNext(
	xstrview List,
	size_t* pOffset,
	xhttplanguagerange* pOutput
)
{
	xhttplanguagerange Range;
	xhttpweightedtoken Item;
	xhttpnext Next;
	size_t iOffset;

	memset(&Range, 0, sizeof(Range));
	if ( !__xrtHttpViewValid(List) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pOutput, sizeof(Range)) ||
		__xrtRangesOverlap(
			List.Data, List.Size, pOffset, sizeof(iOffset)
		) || __xrtRangesOverlap(
			List.Data, List.Size, pOutput, sizeof(Range)
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), pOutput, sizeof(Range)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	memcpy(pOutput, &Range, sizeof(Range));
	Next = xrtHttpWeightedTokenNext(
		List, &iOffset, &Item
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pOffset, &iOffset, sizeof(iOffset));
		}
		return Next;
	}
	if ( !__xrtHttpLanguageTextValid(
		Item.Token, true, false, &Range.SubtagCount
	) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	Range.Range = Item.Token;
	Range.Quality = Item.Quality;
	memcpy(pOutput, &Range, sizeof(Range));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	return XHTTP_NEXT_ITEM;
}



/* 跨越重复 Accept-Language 字段迭代语言范围。 */
XRT_API xhttpnext xrtHttpAcceptLanguageNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttplanguagecursor* pCursor,
	xhttplanguagerange* pOutput
)
{
	xhttplanguagecursor Cursor;
	xhttplanguagerange Range;
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset;

	memset(&Range, 0, sizeof(Range));
	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Range)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Range)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Range)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(pOutput, &Range, sizeof(Range));
	if ( (Cursor.Field > iCount) ||
		((Cursor.Field == iCount) &&
		 (Cursor.Offset != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Accept-Language")
		) ) {
			if ( Cursor.Offset != 0 ) {
				__xrtErrorSetInvalidArgument();
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			continue;
		}
		iOffset = Cursor.Offset;
		Next = xrtHttpLanguageRangeNext(
			Field.Value, &iOffset, &Range
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			Cursor.Offset = iOffset;
			memcpy(pOutput, &Range, sizeof(Range));
			memcpy(pCursor, &Cursor, sizeof(Cursor));
			return XHTTP_NEXT_ITEM;
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	Cursor.Offset = 0;
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 按 Basic Filtering 判断范围是否匹配标签。 */
XRT_API xhttpnext xrtHttpLanguageBasicMatch(
	xstrview Range,
	xstrview Tag
)
{
	if ( !xrtHttpLanguageRangeValid(Range) ||
		!xrtHttpLanguageTagValid(Tag) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	return __xrtHttpLanguageBasicMatch(Range, Tag) ?
		XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}



/* 计算语言标签的有效 Accept-Language 匹配。 */
XRT_API bool xrtHttpAcceptLanguageMatch(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Tag,
	xhttplanguagematch* pOutput
)
{
	xhttplanguagecursor Cursor;
	xhttplanguagematch Match;
	xhttplanguagerange Range;
	xhttpfield Field;
	xhttpnext Next;
	size_t iOrder = 0;
	size_t i;
	bool bPresent = false;
	bool bMatched = false;

	memset(&Match, 0, sizeof(Match));
	Match.Field = XRT_NPOS;
	Match.Order = XRT_NPOS;
	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpLanguageTagValid(Tag) ||
		!__xrtRangeValid(pOutput, sizeof(Match)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Match)
		) || __xrtRangesOverlap(
			Tag.Data, Tag.Size, pOutput, sizeof(Match)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pOutput, 0, sizeof(Match));
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Accept-Language")
		) ) {
			bPresent = true;
		}
	}
	if ( !bPresent ) {
		Match.Quality = XHTTP_QUALITY_MAX;
		memcpy(pOutput, &Match, sizeof(Match));
		return true;
	}
	xrtHttpLanguageCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpAcceptLanguageNext(
			pFields, iCount, &Cursor, &Range
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( __xrtHttpLanguageBasicMatch(
			Range.Range, Tag
		) && (!bMatched ||
			(Range.SubtagCount > Match.SubtagCount)) ) {
			Match.Field = Cursor.Field;
			Match.Order = iOrder;
			Match.SubtagCount = Range.SubtagCount;
			Match.Quality = Range.Quality;
			bMatched = true;
		}
		if ( iOrder == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iOrder++;
	}
	memcpy(pOutput, &Match, sizeof(Match));
	return true;
}



/* 返回语言标签的有效 Accept-Language 质量值。 */
XRT_API uint16 xrtHttpAcceptLanguageQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Tag
)
{
	xhttplanguagematch Match;

	return xrtHttpAcceptLanguageMatch(
		pFields, iCount, Tag, &Match
	) ? Match.Quality : 0;
}



/* 使用 Basic Filtering 选择质量最高的服务端语言。 */
XRT_API xhttpnext xrtHttpAcceptLanguageSelect(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pTags,
	size_t iTagCount,
	size_t* pIndex
)
{
	xhttplanguagematch Match;
	xstrview Tag;
	size_t iSelected = XRT_NPOS;
	uint16 iQuality = 0;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iFieldCount
	) || !__xrtHttpLanguageArrayValid(
		pTags, iTagCount, pIndex
	) || __xrtHttpFieldArrayOverlap(
		pFields, iFieldCount,
		pIndex, sizeof(*pIndex)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pIndex, &iSelected, sizeof(iSelected));
	for ( i = 0; i < iTagCount; i++ ) {
		memcpy(
			&Tag,
			(const uint8*)pTags + i * sizeof(*pTags),
			sizeof(Tag)
		);
		if ( !xrtHttpAcceptLanguageMatch(
			pFields, iFieldCount, Tag, &Match
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



/* 按质量值和线路顺序执行 RFC 4647 Lookup。 */
XRT_API xhttpnext xrtHttpAcceptLanguageLookup(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pTags,
	size_t iTagCount,
	size_t* pIndex
)
{
	xhttplanguagecursor Cursor;
	xhttplanguagerange Range;
	xhttpfield Field;
	xhttpnext Next;
	size_t iSelected = XRT_NPOS;
	uint16 iQuality = 0;
	size_t i;
	bool bPresent = false;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iFieldCount
	) || !__xrtHttpLanguageArrayValid(
		pTags, iTagCount, pIndex
	) || __xrtHttpFieldArrayOverlap(
		pFields, iFieldCount,
		pIndex, sizeof(*pIndex)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pIndex, &iSelected, sizeof(iSelected));
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Accept-Language")
		) ) {
			bPresent = true;
		}
	}
	if ( !bPresent ) {
		if ( iTagCount == 0 ) {
			return XHTTP_NEXT_END;
		}
		iSelected = 0;
		memcpy(pIndex, &iSelected, sizeof(iSelected));
		return XHTTP_NEXT_ITEM;
	}
	xrtHttpLanguageCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpAcceptLanguageNext(
			pFields, iFieldCount, &Cursor, &Range
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_NEXT_ERROR;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( (Range.Quality == 0) ||
			((Range.Range.Size == 1u) &&
			 (Range.Range.Data[0] == '*')) ||
			(Range.Quality < iQuality) ) {
			continue;
		}
		i = __xrtHttpLanguageLookupRange(
			Range.Range, pTags, iTagCount
		);
		if ( (i != XRT_NPOS) &&
			((iSelected == XRT_NPOS) ||
			 (Range.Quality > iQuality)) ) {
			iSelected = i;
			iQuality = Range.Quality;
		}
	}
	if ( iSelected == XRT_NPOS ) {
		return XHTTP_NEXT_END;
	}
	memcpy(pIndex, &iSelected, sizeof(iSelected));
	return XHTTP_NEXT_ITEM;
}

#endif
