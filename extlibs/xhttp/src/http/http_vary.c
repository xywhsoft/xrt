#include "../internal/xrt_http.h"

#include <xrt/http_vary.h>



#if defined(XHTTP_FEATURE_HTTP_VARY)

/* 判断字段是否承载 Vary 选择维度列表。 */
static bool __xrtHttpVaryField(const xhttpfield* pField)
{
	return xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Vary")
	);
}



/* 判断 Vary 成员是否为特殊星号。 */
static bool __xrtHttpVaryWildcard(xstrview Name)
{
	return (Name.Size == 1) && (Name.Data[0] == '*');
}



/* 初始化 Vary 前向游标。 */
XRT_API void xrtHttpVaryCursorInit(xhttpvarycursor* pCursor)
{
	const xhttpvarycursor Cursor = { 0 };

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 跨越重复字段读取下一个 Vary 成员。 */
XRT_API xhttpnext xrtHttpVaryNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpvarycursor* pCursor,
	xhttpvaryitem* pItem
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item = { 0 };

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || !__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pItem, sizeof(Item)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pItem, sizeof(Item)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pCursor, sizeof(Cursor)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pItem, sizeof(Item)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( (Cursor.Field > iCount) ||
		((Cursor.Field == iCount) && (Cursor.Offset != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pItem, &Item, sizeof(Item));
	while ( Cursor.Field < iCount ) {
		xhttpfield Field;

		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !__xrtHttpVaryField(&Field) ) {
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
			xstrview Name;
			xhttpnext Next = xrtHttpTokenNext(
				Field.Value,
				&Cursor.Offset,
				&Name
			);

			if ( Next == XHTTP_NEXT_ERROR ) {
				return XHTTP_NEXT_ERROR;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				Item.Name = Name;
				Item.Wildcard =
					__xrtHttpVaryWildcard(Name);
				memcpy(pItem, &Item, sizeof(Item));
				memcpy(pCursor, &Cursor, sizeof(Cursor));
				return XHTTP_NEXT_ITEM;
			}
		}
		Cursor.Field++;
		Cursor.Offset = 0;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 汇总重复字段、选择名称和原始合并值大小。 */
XRT_API bool xrtHttpVaryPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpvaryplan* pPlan
)
{
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || !__xrtRangeValid(pPlan, sizeof(Plan)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pPlan, sizeof(Plan)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Plan, 0, sizeof(Plan));
	for ( i = 0; i < iCount; i++ ) {
		xhttpfield Field;
		size_t iItems;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpVaryField(&Field) ) {
			continue;
		}
		if ( !xrtHttpTokenListCount(
			Field.Value, &iItems
		) ) {
			return false;
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
		if ( Plan.FieldCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Plan.FieldCount++;
		if ( iItems == 0 ) {
			if ( Plan.EmptyFieldCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			Plan.EmptyFieldCount++;
		}
	}
	if ( Plan.FieldCount != 0 ) {
		Plan.Flags |= XHTTP_VARY_PRESENT;
	}
	if ( Plan.EmptyFieldCount != 0 ) {
		Plan.Flags |= XHTTP_VARY_EMPTY;
	}
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( Plan.ItemCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Plan.ItemCount++;
		if ( Item.Wildcard ) {
			Plan.Flags |= XHTTP_VARY_WILDCARD;
		} else {
			if ( Plan.NameCount == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			Plan.NameCount++;
			Plan.Flags |= XHTTP_VARY_NAMES;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( ((Plan.Flags & XHTTP_VARY_WILDCARD) != 0) &&
		((Plan.Flags & XHTTP_VARY_NAMES) != 0) ) {
		Plan.Flags |= XHTTP_VARY_MIXED;
	}
	memcpy(pPlan, &Plan, sizeof(Plan));
	return true;
}



/* 查找选择字段，并继续验证完整 Vary 列表。 */
XRT_API xhttpnext xrtHttpVaryFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpvaryitem* pItem
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpvaryitem Found;
	xhttpnext Next;
	bool bFound = false;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || !xrtHttpTokenValid(Name) ||
		__xrtHttpVaryWildcard(Name) ||
		!__xrtRangeValid(pItem, sizeof(Found)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pItem, sizeof(Found)
		) ||
		__xrtRangesOverlap(
			pItem, sizeof(Found),
			Name.Data, Name.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Found, 0, sizeof(Found));
	memcpy(pItem, &Found, sizeof(Found));
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( !bFound && !Item.Wildcard &&
			xrtHttpFieldNameEqual(Item.Name, Name) ) {
			Found = Item;
			bFound = true;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_NEXT_ERROR;
	}
	if ( bFound ) {
		memcpy(pItem, &Found, sizeof(Found));
		return XHTTP_NEXT_ITEM;
	}
	return XHTTP_NEXT_END;
}



/* 写出保持字段值字节和字段顺序的组合 Vary 值。 */
XRT_API bool xrtHttpVaryWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpvaryplan Plan;
	bytes pBytes = (bytes)pOutput;
	size_t iOffset = 0;
	size_t iField = 0;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iOffset)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpVaryPlan(
		pFields, iCount, &Plan
	) ) {
		return false;
	}
	if ( __xrtHttpFieldArrayOverlap(
		pFields, iCount,
		pSize, sizeof(Plan.JoinedSize)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(
			pSize, &Plan.JoinedSize, sizeof(Plan.JoinedSize)
		);
		return true;
	}
	if ( iCapacity < Plan.JoinedSize ) {
		memcpy(
			pSize, &Plan.JoinedSize, sizeof(Plan.JoinedSize)
		);
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtRangeValid(pOutput, Plan.JoinedSize) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pOutput, Plan.JoinedSize
		) || __xrtRangesOverlap(
			pOutput, Plan.JoinedSize,
			pSize, sizeof(Plan.JoinedSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &Plan.JoinedSize, sizeof(Plan.JoinedSize));
	for ( i = 0; i < iCount; i++ ) {
		xhttpfield Field;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpVaryField(&Field) ) {
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
