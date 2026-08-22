#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_URL_PARAM)

/* 创建保留 quoted-pair 线路形式的参数原始子区间。 */
static xhttpparam __xrtUrlParamRange(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd
)
{
	xhttpparam Range;

	memset(&Range, 0, sizeof(Range));
	Range.Value.Data = (pParam->Value.Data == NULL) ? NULL :
		pParam->Value.Data + iStart;
	Range.Value.Size = iEnd - iStart;
	Range.Flags = pParam->Flags;
	return Range;
}



/* 验证参数原始区间解码后是一个完整 URI scheme。 */
static bool __xrtUrlParamSchemeValid(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd
)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Range;
	xhttpnext Next;
	uint8 iByte;
	size_t iIndex = 0;

	if ( iStart == iEnd ) {
		return false;
	}
	Range = __xrtUrlParamRange(pParam, iStart, iEnd);
	xrtHttpParamValueCursorInit(&Cursor);
	while ( (Next = xrtHttpParamValueNext(
		&Range, &Cursor, &iByte
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtUrlSchemeByte(iByte, iIndex++) ) {
			return false;
		}
	}
	return (Next == XHTTP_NEXT_END) && (iIndex != 0);
}



/* 验证参数原始区间解码后是指定种类的 URI 组件。 */
static bool __xrtUrlParamComponentValid(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd,
	uint32 iAllowed
)
{
	xrt_url_component_state State;
	xhttpparamvaluecursor Cursor;
	xhttpparam Range;
	xhttpnext Next;
	uint8 iByte;

	if ( iStart == iEnd ) {
		return true;
	}
	Range = __xrtUrlParamRange(pParam, iStart, iEnd);
	xrtHttpParamValueCursorInit(&Cursor);
	__xrtUrlComponentStateInit(&State);
	while ( (Next = xrtHttpParamValueNext(
		&Range, &Cursor, &iByte
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtUrlComponentStateByte(
			&State, iByte, iAllowed
		) ) {
			return false;
		}
	}
	return (Next == XHTTP_NEXT_END) &&
		__xrtUrlComponentStateValid(&State);
}



/* 检查参数区间是否以两个指定语义字节开头。 */
static xhttpnext __xrtUrlParamPrefix(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd,
	uint8 iFirst,
	uint8 iSecond,
	size_t* pAfter
)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Range;
	xhttpnext Next;
	uint8 iByte;

	if ( iStart == iEnd ) {
		return XHTTP_NEXT_END;
	}
	Range = __xrtUrlParamRange(pParam, iStart, iEnd);
	xrtHttpParamValueCursorInit(&Cursor);
	Next = xrtHttpParamValueNext(&Range, &Cursor, &iByte);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( iByte != iFirst ) {
		return XHTTP_NEXT_END;
	}
	Next = xrtHttpParamValueNext(&Range, &Cursor, &iByte);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return (Next == XHTTP_NEXT_ERROR) ?
			XHTTP_NEXT_ERROR : XHTTP_NEXT_END;
	}
	if ( iByte != iSecond ) {
		return XHTTP_NEXT_END;
	}
	*pAfter = iStart + Cursor.Offset;
	return XHTTP_NEXT_ITEM;
}



/* 查找 authority 之后的首个路径斜杠。 */
static bool __xrtUrlParamAuthorityEnd(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd,
	size_t* pAuthorityEnd
)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Range;
	xhttpnext Next;
	uint8 iByte;
	size_t iBefore;

	*pAuthorityEnd = iEnd;
	if ( iStart == iEnd ) {
		return true;
	}
	Range = __xrtUrlParamRange(pParam, iStart, iEnd);
	xrtHttpParamValueCursorInit(&Cursor);
	for ( ;; ) {
		iBefore = Cursor.Offset;
		Next = xrtHttpParamValueNext(&Range, &Cursor, &iByte);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( iByte == (uint8)'/' ) {
			*pAuthorityEnd = iStart + iBefore;
			return true;
		}
	}
}



/* 验证 authority 的可选 userinfo、Host 和端口。 */
static bool __xrtUrlParamAuthorityValid(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd
)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Host;
	xhttpparam Range;
	xhttpnext Next;
	uint8 iByte;
	size_t iAtBefore = 0;
	size_t iAtAfter = 0;
	size_t iBefore;
	bool bAt = false;

	Range = __xrtUrlParamRange(pParam, iStart, iEnd);
	if ( iStart != iEnd ) {
		xrtHttpParamValueCursorInit(&Cursor);
		for ( ;; ) {
			iBefore = Cursor.Offset;
			Next = xrtHttpParamValueNext(
				&Range, &Cursor, &iByte
			);
			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			if ( iByte != (uint8)'@' ) {
				continue;
			}
			if ( bAt ) {
				return false;
			}
			bAt = true;
			iAtBefore = iStart + iBefore;
			iAtAfter = iStart + Cursor.Offset;
		}
	}
	if ( bAt && !__xrtUrlParamComponentValid(
		pParam, iStart, iAtBefore, XRT_URL_COMPONENT_COLON
	) ) {
		return false;
	}
	Host = __xrtUrlParamRange(
		pParam, bAt ? iAtAfter : iStart, iEnd
	);
	return (Host.Value.Size == 0) ||
		xrtHttpParamHostValid(&Host);
}



/* 严格验证参数语义值的 URI-reference 结构。 */
XRT_API bool xrtUrlParamValid(const xhttpparam* pParam)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Param;
	xhttpnext Next;
	uint8 iByte;
	size_t iBefore;
	size_t iAfter;
	size_t iMainEnd;
	size_t iPathStart = 0;
	size_t iColonBefore = 0;
	size_t iColonAfter = 0;
	size_t iSlashBefore = 0;
	size_t iQueryStart = 0;
	size_t iQueryEnd = 0;
	size_t iFragmentStart = 0;
	size_t iAuthorityStart = 0;
	size_t iAuthorityEnd = 0;
	bool bColon = false;
	bool bSlash = false;
	bool bQuery = false;
	bool bFragment = false;
	bool bScheme = false;
	bool bValid;

	if ( !__xrtRangeValid(pParam, sizeof(Param)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Param, pParam, sizeof(Param));
	iMainEnd = Param.Value.Size;
	xrtHttpParamValueCursorInit(&Cursor);
	for ( ;; ) {
		iBefore = Cursor.Offset;
		Next = xrtHttpParamValueNext(
			pParam, &Cursor, &iByte
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		iAfter = Cursor.Offset;

		/* 第一个井号终止 query 或主路径，后续字节属于 fragment。 */
		if ( !bFragment && (iByte == (uint8)'#') ) {
			bFragment = true;
			iFragmentStart = iAfter;
			if ( bQuery ) {
				iQueryEnd = iBefore;
			} else {
				iMainEnd = iBefore;
			}
			continue;
		}
		if ( bFragment ) {
			continue;
		}

		/* 第一个问号终止主路径；query 内的问号属于组件正文。 */
		if ( !bQuery && (iByte == (uint8)'?') ) {
			bQuery = true;
			iQueryStart = iAfter;
			iQueryEnd = Param.Value.Size;
			iMainEnd = iBefore;
			continue;
		}
		if ( bQuery ) {
			continue;
		}

		/* 首个冒号与首个斜杠共同判定是否存在 scheme。 */
		if ( !bColon && (iByte == (uint8)':') ) {
			bColon = true;
			iColonBefore = iBefore;
			iColonAfter = iAfter;
		}
		if ( !bSlash && (iByte == (uint8)'/') ) {
			bSlash = true;
			iSlashBefore = iBefore;
		}
	}

	/* 冒号早于首个斜杠时，首段必须满足 scheme 语法。 */
	bScheme = bColon && (!bSlash ||
		(iColonBefore < iSlashBefore));
	if ( bScheme ) {
		if ( !__xrtUrlParamSchemeValid(
			&Param, 0, iColonBefore
		) ) {
			__xrtErrorSetValue();
			return false;
		}
		iPathStart = iColonAfter;
	}

	/* 双斜杠引入 authority，路径从 authority 后的首个斜杠开始。 */
	Next = __xrtUrlParamPrefix(
		&Param, iPathStart, iMainEnd,
		(uint8)'/', (uint8)'/', &iAuthorityStart
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		if ( !__xrtUrlParamAuthorityEnd(
			&Param, iAuthorityStart, iMainEnd,
			&iAuthorityEnd
		) || !__xrtUrlParamAuthorityValid(
			&Param, iAuthorityStart, iAuthorityEnd
		) ) {
			__xrtErrorSetValue();
			return false;
		}
		iPathStart = iAuthorityEnd;
	}

	/* path、query 和 fragment 使用各自允许的 RFC 3986 字符集合。 */
	bValid = __xrtUrlParamComponentValid(
		&Param, iPathStart, iMainEnd,
		XRT_URL_COMPONENT_COLON |
		XRT_URL_COMPONENT_AT |
		XRT_URL_COMPONENT_SLASH
	);
	if ( bValid && bQuery ) {
		bValid = __xrtUrlParamComponentValid(
			&Param, iQueryStart, iQueryEnd,
			XRT_URL_COMPONENT_COLON |
			XRT_URL_COMPONENT_AT |
			XRT_URL_COMPONENT_SLASH |
			XRT_URL_COMPONENT_QUESTION
		);
	}
	if ( bValid && bFragment ) {
		bValid = __xrtUrlParamComponentValid(
			&Param, iFragmentStart, Param.Value.Size,
			XRT_URL_COMPONENT_COLON |
			XRT_URL_COMPONENT_AT |
			XRT_URL_COMPONENT_SLASH |
			XRT_URL_COMPONENT_QUESTION
		);
	}
	if ( !bValid ) {
		__xrtErrorSetValue();
	}
	return bValid;
}

#endif
