#include "../internal/xrt_http.h"

#include <xrt/http_upgrade.h>



#if defined(XRT_FEATURE_HTTP_UPGRADE)

/* 严格解析一个已经与列表分隔符分开的协议元素。 */
static bool __xrtHttpUpgradeParseValue(
	xstrview Text,
	xhttpupgradeitem* pUpgrade
)
{
	xhttpupgradeitem Upgrade;
	size_t iStart;
	size_t i;

	memset(&Upgrade, 0, sizeof(Upgrade));
	Text = xrtHttpOwsTrim(Text);
	i = 0;
	iStart = i;
	while ( (i < Text.Size) &&
		__xrtHttpTokenByte((unsigned char)Text.Data[i]) ) {
		i++;
	}
	if ( i == iStart ) {
		return false;
	}
	Upgrade.Protocol = (xstrview){
		Text.Data + iStart, i - iStart
	};
	if ( i < Text.Size ) {
		if ( Text.Data[i] != '/' ) {
			return false;
		}
		i++;
		iStart = i;
		while ( (i < Text.Size) &&
			__xrtHttpTokenByte(
				(unsigned char)Text.Data[i]
			) ) {
			i++;
		}
		if ( (i == iStart) || (i != Text.Size) ) {
			return false;
		}
		Upgrade.Version = (xstrview){
			Text.Data + iStart, i - iStart
		};
	}
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	return true;
}



/* 从列表位置读取下一项，不修改公开错误状态。 */
static xhttpnext __xrtHttpUpgradeItemNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xhttpupgradeitem* pUpgrade
)
{
	xhttpupgradeitem Upgrade;
	size_t iStart;
	size_t i = iOffset;

	memset(&Upgrade, 0, sizeof(Upgrade));
	while ( true ) {
		while ( (i < Value.Size) &&
			((Value.Data[i] == ' ') ||
			 (Value.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == Value.Size ) {
			*pNext = i;
			memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
			return XHTTP_NEXT_END;
		}
		if ( Value.Data[i] != ',' ) {
			break;
		}
		i++;
	}

	iStart = i;
	while ( (i < Value.Size) &&
		__xrtHttpTokenByte((unsigned char)Value.Data[i]) ) {
		i++;
	}
	if ( i == iStart ) {
		return XHTTP_NEXT_ERROR;
	}
	Upgrade.Protocol = (xstrview){
		Value.Data + iStart, i - iStart
	};
	if ( (i < Value.Size) && (Value.Data[i] == '/') ) {
		i++;
		iStart = i;
		while ( (i < Value.Size) &&
			__xrtHttpTokenByte(
				(unsigned char)Value.Data[i]
			) ) {
			i++;
		}
		if ( i == iStart ) {
			return XHTTP_NEXT_ERROR;
		}
		Upgrade.Version = (xstrview){
			Value.Data + iStart, i - iStart
		};
	}
	while ( (i < Value.Size) &&
		((Value.Data[i] == ' ') ||
		 (Value.Data[i] == '\t')) ) {
		i++;
	}
	if ( i < Value.Size ) {
		if ( Value.Data[i] != ',' ) {
			return XHTTP_NEXT_ERROR;
		}
		i++;
	}
	*pNext = i;
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	return XHTTP_NEXT_ITEM;
}



/* 完整验证一个字段值，并可同时统计协议数量。 */
static bool __xrtHttpUpgradeMeasure(
	xstrview Value,
	size_t* pCount
)
{
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iNext;
	size_t iCount = 0;

	for ( ;; ) {
		Next = __xrtHttpUpgradeItemNext(
			Value, iOffset, &iNext, &Upgrade
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			*pCount = iCount;
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
		iOffset = iNext;
	}
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpUpgradeCursorInit(
	xhttpupgradecursor* pCursor
)
{
	xhttpupgradecursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpUpgradeFieldCursorInit(
	xhttpupgradefieldcursor* pCursor
)
{
	xhttpupgradefieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 严格解析一个 Upgrade 协议元素。 */
XRT_API bool xrtHttpUpgradeParse(
	xstrview Text,
	xhttpupgradeitem* pUpgrade
)
{
	xhttpupgradeitem Upgrade;

	memset(&Upgrade, 0, sizeof(Upgrade));
	if ( !__xrtHttpViewValid(Text) ||
		!__xrtRangeValid(pUpgrade, sizeof(Upgrade)) ||
		__xrtRangesOverlap(
			Text.Data, Text.Size,
			pUpgrade, sizeof(Upgrade)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	if ( !__xrtHttpUpgradeParseValue(Text, &Upgrade) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	return true;
}



/* 完整验证一个 Upgrade 字段值。 */
XRT_API bool xrtHttpUpgradeValid(xstrview Value)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpUpgradeMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 完整验证并统计一个 Upgrade 字段值。 */
XRT_API bool xrtHttpUpgradeCount(
	xstrview Value,
	size_t* pCount
)
{
	size_t iCount = 0;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pCount, sizeof(iCount)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	if ( !__xrtHttpUpgradeMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	return true;
}



/* 验证单字段游标状态。 */
static bool __xrtHttpUpgradeCursorValid(
	const xhttpupgradecursor* pCursor,
	size_t iSize
)
{
	return (pCursor->Validated <= 1u) &&
		(pCursor->Offset <= iSize) &&
		!((pCursor->Validated == 0) &&
		  (pCursor->Offset != 0));
}



/* 按线路顺序迭代一个完整 Upgrade 字段值。 */
XRT_API xhttpnext xrtHttpUpgradeNext(
	xstrview Value,
	xhttpupgradecursor* pCursor,
	xhttpupgradeitem* pUpgrade
)
{
	xhttpupgradecursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	size_t iIgnored;
	size_t iNext;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pUpgrade, sizeof(Upgrade)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size,
			pUpgrade, sizeof(Upgrade)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pUpgrade, sizeof(Upgrade)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Upgrade, 0, sizeof(Upgrade));
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	if ( !__xrtHttpUpgradeCursorValid(
		&Cursor, Value.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpUpgradeMeasure(Value, &iIgnored) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpUpgradeItemNext(
		Value, Cursor.Offset, &iNext, &Upgrade
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtErrorSetInvalidArgument();
		return Next;
	}
	Cursor.Offset = iNext;
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return Next;
}



/* 完整预校验全部重复 Upgrade 字段行。 */
static bool __xrtHttpUpgradeFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iItems;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Upgrade")
		) && !__xrtHttpUpgradeMeasure(
			Field.Value, &iItems
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证重复字段游标的跨字段状态。 */
static bool __xrtHttpUpgradeFieldCursorValid(
	const xhttpupgradefieldcursor* pCursor,
	size_t iCount
)
{
	return (pCursor->Validated <= 1u) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Validated == 0) &&
		  ((pCursor->Field != 0) ||
		   (pCursor->Offset != 0))) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 跨重复 Upgrade 字段行按线路顺序迭代协议。 */
XRT_API xhttpnext xrtHttpUpgradeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpupgradefieldcursor* pCursor,
	xhttpupgradeitem* pUpgrade
)
{
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpfield Field;
	xhttpnext Next;
	size_t iNext;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pUpgrade, sizeof(Upgrade)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pUpgrade, sizeof(Upgrade)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pUpgrade, sizeof(Upgrade)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Upgrade, 0, sizeof(Upgrade));
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	if ( !__xrtHttpUpgradeFieldCursorValid(
		&Cursor, iCount
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpUpgradeFieldsValidate(
			pFields, iCount
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Upgrade")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( Cursor.Offset > Field.Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		Next = __xrtHttpUpgradeItemNext(
			Field.Value, Cursor.Offset, &iNext, &Upgrade
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtErrorSetInvalidArgument();
			return Next;
		}
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		Cursor.Offset = iNext;
		memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memset(&Upgrade, 0, sizeof(Upgrade));
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}

#endif
