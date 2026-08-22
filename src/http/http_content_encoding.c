#include "../internal/xrt_http.h"

#include <xrt/http_encoding.h>



#if defined(XRT_FEATURE_HTTP_ENCODING)

/* 判断字段是否承载 Content-Encoding 列表。 */
static bool __xrtHttpContentEncodingField(
	const xhttpfield* pField
)
{
	return xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Content-Encoding")
	);
}



/* 初始化 Content-Encoding 前向游标。 */
XRT_API void xrtHttpContentEncodingCursorInit(
	xhttpcontentencodingcursor* pCursor
)
{
	if ( pCursor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pCursor, 0, sizeof(*pCursor));
}



/* 跨越重复字段读取下一个内容编码成员。 */
XRT_API xhttpnext xrtHttpContentEncodingNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingcursor* pCursor,
	xhttpcontentencodingitem* pItem
)
{
	xhttpcontentencodingcursor Cursor;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || (pCursor == NULL) ||
		(pItem == NULL) ||
		(pCursor->Field > iCount) ||
		((pCursor->Field == iCount) &&
		 (pCursor->Offset != 0)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(*pCursor),
			pItem, sizeof(*pItem)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pCursor, sizeof(*pCursor)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pItem, sizeof(*pItem)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(pItem, 0, sizeof(*pItem));
	Cursor = *pCursor;
	while ( Cursor.Field < iCount ) {
		xhttpfield Field;

		__xrtHttpFieldLoad(
			pFields, Cursor.Field, &Field
		);

		if ( !__xrtHttpContentEncodingField(&Field) ) {
			if ( Cursor.Offset != 0 ) {
				__xrtErrorSetInvalidArgument();
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			continue;
		}
		if ( Cursor.Offset > Field.Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		{
			xstrview Token;
			xhttpnext Next = xrtHttpTokenNext(
				Field.Value,
				&Cursor.Offset,
				&Token
			);

			if ( Next == XHTTP_NEXT_ERROR ) {
				return XHTTP_NEXT_ERROR;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				pItem->Token = Token;
				pItem->Coding =
					xrtHttpCodingParse(Token);
				*pCursor = Cursor;
				return XHTTP_NEXT_ITEM;
			}
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	*pCursor = Cursor;
	return XHTTP_NEXT_END;
}



/* 汇总重复字段、编码层和原始合并值大小。 */
XRT_API bool xrtHttpContentEncodingPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingplan* pPlan
)
{
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpnext Next;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || (pPlan == NULL) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pPlan, sizeof(*pPlan)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Plan, 0, sizeof(Plan));
	for ( i = 0; i < iCount; i++ ) {
		xhttpfield Field;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpContentEncodingField(
			&Field
		) ) {
			continue;
		}
		if ( (Plan.FieldCount != 0) &&
			(Plan.JoinedSize > (SIZE_MAX - 2u)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		if ( Field.Value.Size >
			(SIZE_MAX - Plan.JoinedSize -
			 (Plan.FieldCount != 0 ? 2u : 0u)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		if ( Plan.FieldCount != 0 ) {
			Plan.JoinedSize += 2u;
		}
		Plan.JoinedSize += Field.Value.Size;
		Plan.FieldCount++;
	}
	if ( Plan.FieldCount != 0 ) {
		Plan.Flags |= XHTTP_CONTENT_ENCODING_PRESENT;
	}
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( Plan.CodingCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Plan.CodingCount++;
		if ( Item.Coding == XHTTP_CODING_IDENTITY ) {
			Plan.Flags |= XHTTP_CONTENT_ENCODING_IDENTITY;
		} else if ( Item.Coding == XHTTP_CODING_NONE ) {
			if ( Plan.UnknownCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			Plan.UnknownCount++;
			Plan.Flags |= XHTTP_CONTENT_ENCODING_UNKNOWN;
		} else {
			if ( Plan.DecoderCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			Plan.DecoderCount++;
		}
		if ( xrtHttpTokenEqual(
			Item.Token,
			XRT_STR_LITERAL("x-gzip")
		) ) {
			Plan.Flags |= XHTTP_CONTENT_ENCODING_LEGACY;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	*pPlan = Plan;
	return true;
}



/* 写出保持字段值字节和字段顺序的合并编码列表。 */
XRT_API bool xrtHttpContentEncodingWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpcontentencodingplan Plan;
	bytes pBytes = (bytes)pOutput;
	size_t iOffset = 0;
	size_t iField = 0;
	size_t i;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pFields == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pFields))) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			pSize, sizeof(*pSize)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pSize, sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pSize = 0;
	if ( !xrtHttpContentEncodingPlan(
		pFields, iCount, &Plan
	) ) {
		return false;
	}
	*pSize = Plan.JoinedSize;
	if ( iCapacity < Plan.JoinedSize ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( Plan.JoinedSize == 0 ) {
		return true;
	}
	if ( pOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtHttpFieldArrayOverlap(
		pFields, iCount,
		pOutput, Plan.JoinedSize
	) || __xrtRangesOverlap(
		pOutput, Plan.JoinedSize,
		pSize, sizeof(*pSize)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		xhttpfield Field;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpContentEncodingField(
			&Field
		) ) {
			continue;
		}
		if ( iField != 0 ) {
			pBytes[iOffset] = (uint8)',';
			pBytes[iOffset + 1u] = (uint8)' ';
			iOffset += 2u;
		}
		if ( Field.Value.Size != 0 ) {
			memcpy(
				pBytes + iOffset,
				Field.Value.Data,
				Field.Value.Size
			);
			iOffset += Field.Value.Size;
		}
		iField++;
	}
	return true;
}

#endif
