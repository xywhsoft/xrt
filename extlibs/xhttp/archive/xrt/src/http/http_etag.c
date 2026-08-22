#include "../internal/xrt_http_semantics.h"



#if defined(XRT_FEATURE_HTTP_ETAG)

/* 判断字节是否属于 entity-tag 的 etagc 集合。 */
static bool __xrtHttpETagByte(unsigned char iByte)
{
	return (iByte == 0x21u) ||
		((iByte >= 0x23u) && (iByte <= 0x7Eu)) ||
		(iByte >= 0x80u);
}



/* 快照并无错误副作用地验证实体标签描述符和正文。 */
static bool __xrtHttpETagRead(
	const xhttpetag* pInput,
	xhttpetag* pTag
)
{
	xhttpetag Tag;
	size_t i;

	if ( !__xrtRangeValid(pInput, sizeof(Tag)) ) {
		return false;
	}
	memcpy(&Tag, pInput, sizeof(Tag));
	if (
		!__xrtRangeValid(
			Tag.Opaque.Data,
			Tag.Opaque.Size
		) ) {
		return false;
	}
	for ( i = 0; i < Tag.Opaque.Size; i++ ) {
		if ( !__xrtHttpETagByte(
			(unsigned char)Tag.Opaque.Data[i]
		) ) {
			return false;
		}
	}
	*pTag = Tag;
	return true;
}



/* 无错误副作用地验证实体标签正文。 */
bool __xrtHttpETagValid(const xhttpetag* pTag)
{
	xhttpetag Tag;

	return __xrtHttpETagRead(pTag, &Tag);
}



/* 无错误副作用地解析一个完整实体标签。 */
bool __xrtHttpETagParseValue(
	xstrview Text,
	xhttpetag* pTag
)
{
	xhttpetag Tag;
	size_t iStart = 0;
	size_t i;

	if ( (pTag == NULL) || !__xrtHttpViewValid(Text) ) {
		return false;
	}
	Tag.Opaque = (xstrview){ NULL, 0 };
	Tag.Weak = false;
	if ( (Text.Size >= 2) &&
		(Text.Data[0] == 'W') &&
		(Text.Data[1] == '/') ) {
		Tag.Weak = true;
		iStart = 2;
	}
	if ( (Text.Size < (iStart + 2u)) ||
		(Text.Data[iStart] != '"') ||
		(Text.Data[Text.Size - 1u] != '"') ) {
		return false;
	}
	Tag.Opaque.Data = Text.Data + iStart + 1u;
	Tag.Opaque.Size = Text.Size - iStart - 2u;
	for ( i = 0; i < Tag.Opaque.Size; i++ ) {
		if ( !__xrtHttpETagByte(
			(unsigned char)Tag.Opaque.Data[i]
		) ) {
			return false;
		}
	}
	memcpy(pTag, &Tag, sizeof(Tag));
	return true;
}



/* 严格解析一个完整实体标签。 */
XRT_API bool xrtHttpETagParse(
	xstrview Text,
	xhttpetag* pTag
)
{
	xhttpetag Tag;

	if ( !__xrtHttpViewValid(Text) ||
		!__xrtRangeValid(pTag, sizeof(Tag)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pTag, sizeof(*pTag), Text.Data, Text.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pTag, 0, sizeof(Tag));
	if ( !__xrtHttpETagParseValue(Text, &Tag) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pTag, &Tag, sizeof(Tag));
	return true;
}



/* 判断字段值去掉 OWS 后是否精确等于星号。 */
static bool __xrtHttpETagAny(xstrview List)
{
	List = xrtHttpOwsTrim(List);
	return (List.Size == 1) && (List.Data[0] == '*');
}



/* 迭代一个实体标签列表。 */
XRT_API xhttpnext xrtHttpETagNext(
	xstrview List,
	size_t* pOffset,
	xhttpetagitem* pItem
)
{
	size_t iOffset;
	size_t iStart;
	size_t iEnd;
	xstrview Text;
	xhttpetag Tag;
	xhttpetagitem Item = {
		XHTTP_ETAG_VALUE,
		{ { NULL, 0 }, false }
	};

	if ( !__xrtHttpViewValid(List) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pItem, sizeof(Item)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), List.Data, List.Size
		) || __xrtRangesOverlap(
			pItem, sizeof(Item), List.Data, List.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset), pItem, sizeof(Item)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > List.Size ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( iOffset == List.Size ) {
		memcpy(pItem, &Item, sizeof(Item));
		return XHTTP_NEXT_END;
	}
	if ( (iOffset == 0) && __xrtHttpETagAny(List) ) {
		Item.Kind = XHTTP_ETAG_ANY;
		iOffset = List.Size;
		memcpy(pOffset, &iOffset, sizeof(iOffset));
		memcpy(pItem, &Item, sizeof(Item));
		return XHTTP_NEXT_ITEM;
	}
	while ( iOffset < List.Size ) {
		iStart = iOffset;
		iEnd = iStart;
		while ( (iEnd < List.Size) && (List.Data[iEnd] != ',') ) {
			iEnd++;
		}
		iOffset = (iEnd < List.Size) ? (iEnd + 1u) : iEnd;
		Text.Data = List.Data + iStart;
		Text.Size = iEnd - iStart;
		Text = xrtHttpOwsTrim(Text);
		if ( Text.Size == 0 ) {
			continue;
		}
		if ( !__xrtHttpETagParseValue(Text, &Tag) ) {
			memcpy(pOffset, &iOffset, sizeof(iOffset));
			memcpy(pItem, &Item, sizeof(Item));
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Item.Kind = XHTTP_ETAG_VALUE;
		Item.Tag = Tag;
		memcpy(pOffset, &iOffset, sizeof(iOffset));
		memcpy(pItem, &Item, sizeof(Item));
		return XHTTP_NEXT_ITEM;
	}
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	memcpy(pItem, &Item, sizeof(Item));
	return XHTTP_NEXT_END;
}



/* 按 opaque-tag 字节精确比较两个已验证标签。 */
static bool __xrtHttpETagOpaqueEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
)
{
	return (pLeft->Opaque.Size == pRight->Opaque.Size) &&
		((pLeft->Opaque.Size == 0) ||
		 (memcmp(
			pLeft->Opaque.Data,
			pRight->Opaque.Data,
			pLeft->Opaque.Size
		) == 0));
}



/* 按强比较规则比较两个实体标签。 */
XRT_API bool xrtHttpETagStrongEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
)
{
	xhttpetag Left;
	xhttpetag Right;

	if ( !__xrtHttpETagRead(pLeft, &Left) ||
		!__xrtHttpETagRead(pRight, &Right) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return !Left.Weak &&
		!Right.Weak &&
		__xrtHttpETagOpaqueEqual(&Left, &Right);
}



/* 按弱比较规则比较两个实体标签。 */
XRT_API bool xrtHttpETagWeakEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
)
{
	xhttpetag Left;
	xhttpetag Right;

	if ( !__xrtHttpETagRead(pLeft, &Left) ||
		!__xrtHttpETagRead(pRight, &Right) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpETagOpaqueEqual(&Left, &Right);
}



/* 按指定比较强度扫描并完整验证一个实体标签列表。 */
static bool __xrtHttpETagListHas(
	xstrview List,
	const xhttpetag* pTag,
	bool bWeak
)
{
	xhttpetagitem Item;
	xhttpetag Tag;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;
	bool bMatch = false;

	if ( !__xrtHttpETagRead(pTag, &Tag) ||
		!__xrtHttpViewValid(List) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	do {
		Next = xrtHttpETagNext(List, &iOffset, &Item);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			iCount++;
			if ( Item.Kind == XHTTP_ETAG_ANY ) {
				bMatch = true;
			} else if ( bWeak ) {
				bMatch = __xrtHttpETagOpaqueEqual(
					&Item.Tag,
					&Tag
				) || bMatch;
			} else {
				bMatch = (!Item.Tag.Weak &&
					!Tag.Weak &&
					__xrtHttpETagOpaqueEqual(
						&Item.Tag,
						&Tag
					)) || bMatch;
			}
		}
	} while ( Next == XHTTP_NEXT_ITEM );
	if ( iCount == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	return bMatch;
}



/* 按强比较规则检查完整实体标签列表。 */
XRT_API bool xrtHttpETagListStrongHas(
	xstrview List,
	const xhttpetag* pTag
)
{
	return __xrtHttpETagListHas(List, pTag, false);
}



/* 按弱比较规则检查完整实体标签列表。 */
XRT_API bool xrtHttpETagListWeakHas(
	xstrview List,
	const xhttpetag* pTag
)
{
	return __xrtHttpETagListHas(List, pTag, true);
}



/* 写出一个实体标签。 */
XRT_API bool xrtHttpETagWrite(
	const xhttpetag* pTag,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpetag Tag;
	uint8* pWrite = (uint8*)pOutput;
	size_t iPrefix;
	size_t iRequired;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpETagRead(pTag, &Tag) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iPrefix = Tag.Weak ? 2u : 0u;
	if ( Tag.Opaque.Size > (SIZE_MAX - iPrefix - 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = iPrefix + Tag.Opaque.Size + 2u;
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), Tag.Opaque.Data, Tag.Opaque.Size
	) || __xrtRangesOverlap(
		pSize, sizeof(iRequired), pTag, sizeof(Tag)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( (iCapacity >= iRequired) &&
		!__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pSize, sizeof(iRequired), pOutput, iRequired
	) || __xrtRangesOverlap(
		pTag, sizeof(Tag), pOutput, iRequired
	) || __xrtRangesOverlap(
		Tag.Opaque.Data,
		Tag.Opaque.Size,
		pOutput,
		iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( Tag.Weak ) {
		pWrite[0] = (uint8)'W';
		pWrite[1] = (uint8)'/';
	}
	pWrite[iPrefix] = (uint8)'"';
	if ( Tag.Opaque.Size != 0 ) {
		memcpy(
			pWrite + iPrefix + 1u,
			Tag.Opaque.Data,
			Tag.Opaque.Size
		);
	}
	pWrite[iRequired - 1u] = (uint8)'"';
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 构建零结尾实体标签。 */
XRT_API str xrtHttpETagBuild(
	const xhttpetag* pTag,
	size_t* pSize
)
{
	xhttpetag Tag;
	str sOutput;
	size_t iRequired;

	if ( !__xrtHttpETagRead(pTag, &Tag) ||
		((pSize != NULL) &&
		 !__xrtRangeValid(pSize, sizeof(iRequired))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSize != NULL ) {
		if ( __xrtRangesOverlap(
				pSize,
				sizeof(iRequired),
				Tag.Opaque.Data,
				Tag.Opaque.Size
			) || __xrtRangesOverlap(
				pSize,
				sizeof(iRequired),
				pTag,
				sizeof(Tag)
			) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
	}
	if ( !xrtHttpETagWrite(
		&Tag, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpETagWrite(
		&Tag, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}

#endif
