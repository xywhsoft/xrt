#include "../internal/xrt_codec.h"
#include "../internal/xrt_http.h"

#include <xrt/http_proxy_status.h>



#if defined(XRT_FEATURE_HTTP_PROXY_ALIAS)

/* 验证一个别名的百分号编码和 RFC 9532 反斜杠规则。 */
static bool __xrtHttpProxyAliasValid(xstrview Alias)
{
	size_t iOffset = 0;
	bool bEscaped = false;
	uint8 iByte;

	if ( Alias.Size == 0 ) {
		return false;
	}
	while ( iOffset < Alias.Size ) {
		uint8 iWire = (uint8)Alias.Data[iOffset];

		if ( (iWire != (uint8)'%') &&
			!__xrtPercentUnreserved(iWire) ) {
			return false;
		}
		if ( __xrtPercentDecodeNext(
			Alias, false, &iOffset, &iByte
		) != XRT_PERCENT_NEXT_BYTE ) {
			return false;
		}
		if ( bEscaped ) {
			if ( (iByte != (uint8)'.') &&
				(iByte != (uint8)'\\') ) {
				return false;
			}
			bEscaped = false;
		} else if ( iByte == (uint8)'\\' ) {
			bEscaped = true;
		}
	}
	return !bEscaped;
}



/* 验证逗号分隔列表，空列表表达没有 CNAME 记录。 */
static bool __xrtHttpProxyAliasesValid(xstrview Aliases)
{
	size_t iStart = 0;
	size_t iEnd;

	if ( Aliases.Size == 0 ) {
		return true;
	}
	while ( iStart < Aliases.Size ) {
		iEnd = iStart;
		while ( (iEnd < Aliases.Size) &&
			(Aliases.Data[iEnd] != ',') ) {
			iEnd++;
		}
		if ( !__xrtHttpProxyAliasValid((xstrview){
			Aliases.Data + iStart, iEnd - iStart
		}) ) {
			return false;
		}
		if ( iEnd == Aliases.Size ) {
			return true;
		}
		iStart = iEnd + 1u;
	}
	return false;
}



/* 初始化别名列表游标。 */
XRT_API void xrtHttpProxyAliasCursorInit(
	xhttpproxyaliascursor* pCursor
)
{
	xhttpproxyaliascursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 验证完整别名列表。 */
XRT_API bool xrtHttpProxyAliasesValid(xstrview Aliases)
{
	if ( !__xrtHttpViewValid(Aliases) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpProxyAliasesValid(Aliases) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 在已经预校验的列表中定位下一项。 */
static xstrview __xrtHttpProxyAliasAt(
	xstrview Aliases,
	size_t iOffset,
	size_t* pNext
)
{
	size_t iEnd = iOffset;

	while ( (iEnd < Aliases.Size) &&
		(Aliases.Data[iEnd] != ',') ) {
		iEnd++;
	}
	*pNext = (iEnd < Aliases.Size) ? (iEnd + 1u) : iEnd;
	return (xstrview){ Aliases.Data + iOffset, iEnd - iOffset };
}



/* 零拷贝迭代一个已编码别名。 */
XRT_API xhttpnext xrtHttpProxyAliasNext(
	xstrview Aliases,
	xhttpproxyaliascursor* pCursor,
	xstrview* pAlias
)
{
	xhttpproxyaliascursor Cursor;
	xstrview Alias;
	size_t iNext;

	memset(&Alias, 0, sizeof(Alias));
	if ( !__xrtHttpViewValid(Aliases) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pAlias, sizeof(Alias)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pAlias, sizeof(Alias)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Aliases.Data, Aliases.Size
		) || __xrtRangesOverlap(
			pAlias, sizeof(Alias), Aliases.Data, Aliases.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( (Cursor.Validated > 1u) ||
		(Cursor.Offset > Aliases.Size) ||
		((Cursor.Validated == 0) &&
		 ((Cursor.Source != NULL) ||
		  (Cursor.SourceSize != 0) ||
		  (Cursor.Offset != 0))) ||
		((Cursor.Validated != 0) &&
		 ((Cursor.Source != (const void*)Aliases.Data) ||
		  (Cursor.SourceSize != Aliases.Size))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpProxyAliasesValid(Aliases) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Aliases.Data;
		Cursor.SourceSize = Aliases.Size;
		Cursor.Validated = 1u;
	}
	if ( Cursor.Offset == Aliases.Size ) {
		memcpy(pAlias, &Alias, sizeof(Alias));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_END;
	}
	Alias = __xrtHttpProxyAliasAt(
		Aliases, Cursor.Offset, &iNext
	);
	Cursor.Offset = iNext;
	memcpy(pAlias, &Alias, sizeof(Alias));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 解码一个已经验证的别名，同时保留展示形式转义。 */
XRT_API bool xrtHttpProxyAliasRead(
	xstrview Alias,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtHttpViewValid(Alias) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pSize, sizeof(*pSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpProxyAliasValid(Alias) ) {
		__xrtErrorSetValue();
		return false;
	}
	return __xrtPercentDecodeCore(
		Alias, false, pOutput, iCapacity, pSize,
		"read-http-proxy-alias"
	);
}

#endif
