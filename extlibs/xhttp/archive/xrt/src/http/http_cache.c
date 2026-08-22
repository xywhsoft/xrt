#include "../internal/xrt_http_cache.h"

#include <xrt/http_cache.h>



#if defined(XRT_FEATURE_HTTP_CACHE)

#define XRT_HTTP_CACHE_DIRECTIVES \
	((uint32)XHTTP_CACHE_MAX_AGE | \
	 (uint32)XHTTP_CACHE_MAX_STALE | \
	 (uint32)XHTTP_CACHE_MIN_FRESH | \
	 (uint32)XHTTP_CACHE_NO_CACHE | \
	 (uint32)XHTTP_CACHE_NO_STORE | \
	 (uint32)XHTTP_CACHE_NO_TRANSFORM | \
	 (uint32)XHTTP_CACHE_ONLY_IF_CACHED | \
	 (uint32)XHTTP_CACHE_MUST_REVALIDATE | \
	 (uint32)XHTTP_CACHE_MUST_UNDERSTAND | \
	 (uint32)XHTTP_CACHE_PRIVATE | \
	 (uint32)XHTTP_CACHE_PROXY_REVALIDATE | \
	 (uint32)XHTTP_CACHE_PUBLIC | \
	 (uint32)XHTTP_CACHE_S_MAXAGE)

#define XRT_HTTP_CACHE_FLAGS \
	(XRT_HTTP_CACHE_DIRECTIVES | \
	 (uint32)XHTTP_CACHE_PRESENT | \
	 (uint32)XHTTP_CACHE_EXTENSION | \
	 (uint32)XHTTP_CACHE_DUPLICATE | \
	 (uint32)XHTTP_CACHE_INVALID | \
	 (uint32)XHTTP_CACHE_MAX_STALE_ANY | \
	 (uint32)XHTTP_CACHE_NO_CACHE_FIELDS | \
	 (uint32)XHTTP_CACHE_PRIVATE_FIELDS | \
	 (uint32)XHTTP_CACHE_CONFLICT)



/* 判断字段是否承载 Cache-Control 指令列表。 */
static bool __xrtHttpCacheField(const xhttpfield* pField)
{
	return xrtHttpFieldNameEqual(
		pField->Name,
		XRT_STR_LITERAL("Cache-Control")
	);
}



/* 无错误副作用地解析并饱和一个 delta-seconds。 */
bool __xrtHttpCacheDeltaParse(
	xstrview Text,
	bool Quoted,
	bool TrimOWS,
	uint64* pSeconds
)
{
	uint64 iValue = 0;
	size_t iOffset = 0;
	bool bDigit = false;

	if ( TrimOWS ) {
		Text = xrtHttpOwsTrim(Text);
	}
	while ( iOffset < Text.Size ) {
		uint8 iByte = (uint8)Text.Data[iOffset];
		uint64 iDigit;

		iOffset++;
		if ( Quoted && (iByte == (uint8)'\\') ) {
			if ( iOffset == Text.Size ) {
				return false;
			}
			iByte = (uint8)Text.Data[iOffset];
			iOffset++;
		}
		if ( (iByte < (uint8)'0') ||
			(iByte > (uint8)'9') ) {
			return false;
		}
		bDigit = true;
		iDigit = (uint64)(iByte - (uint8)'0');
		if ( iValue >= XHTTP_CACHE_DELTA_MAX ) {
			iValue = XHTTP_CACHE_DELTA_MAX;
		} else if ( iValue >
			((XHTTP_CACHE_DELTA_MAX - iDigit) /
			 UINT64_C(10)) ) {
			iValue = XHTTP_CACHE_DELTA_MAX;
		} else {
			iValue = (iValue * UINT64_C(10)) + iDigit;
		}
	}
	if ( !bDigit ) {
		return false;
	}
	*pSeconds = iValue;
	return true;
}



/* 验证 no-cache 与 private 的可选字段名列表。 */
static bool __xrtHttpCacheFieldNames(
	const xhttpcacheitem* pItem
)
{
	size_t iOffset = 0;
	bool bAny = false;
	bool bToken = false;
	bool bAfter = false;

	if ( (pItem->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return true;
	}
	if ( (pItem->Flags & XHTTP_PARAM_QUOTED) == 0 ) {
		return xrtHttpTokenValid(pItem->Value);
	}
	while ( iOffset < pItem->Value.Size ) {
		uint8 iByte = (uint8)pItem->Value.Data[iOffset];

		iOffset++;
		if ( (pItem->Flags & XHTTP_PARAM_QUOTED) != 0 &&
			(iByte == (uint8)'\\') ) {
			iByte = (uint8)pItem->Value.Data[iOffset];
			iOffset++;
		}

		if ( iByte == (uint8)',' ) {
			if ( bToken ) {
				bAny = true;
			}
			bToken = false;
			bAfter = false;
		} else if ( (iByte == (uint8)' ') ||
			(iByte == (uint8)'\t') ) {
			if ( bToken ) {
				bAfter = true;
			}
		} else {
			if ( bAfter || !__xrtHttpTokenByte(iByte) ) {
				return false;
			}
			bToken = true;
		}
	}
	return bAny || bToken;
}



/* 验证一个公开缓存条目可安全用于数值读取。 */
static bool __xrtHttpCacheItemValid(
	const xhttpcacheitem* pItem
)
{
	xhttpparam Param;
	size_t iSize;

	if ( (pItem == NULL) ||
		((pItem->Flags & ~(uint32)(
			XHTTP_PARAM_HAS_VALUE |
			XHTTP_PARAM_QUOTED
		 )) != 0) ||
		((pItem->Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
		!__xrtHttpViewValid(pItem->Name) ||
		!__xrtHttpViewValid(pItem->Value) ||
		!xrtHttpTokenValid(pItem->Name) ||
		(xrtHttpCacheDirectiveParse(pItem->Name) !=
		 pItem->Directive) ||
		__xrtRangesOverlap(
			pItem, sizeof(*pItem),
			pItem->Name.Data, pItem->Name.Size
		) ||
		__xrtRangesOverlap(
			pItem, sizeof(*pItem),
			pItem->Value.Data, pItem->Value.Size
		) ) {
		return false;
	}
	Param.Name = pItem->Name;
	Param.Value = pItem->Value;
	Param.Flags = pItem->Flags;
	return xrtHttpParamValueWrite(
		&Param, NULL, 0, &iSize
	);
}



/* 安全增加汇总计数。 */
static bool __xrtHttpCacheCount(size_t* pCount)
{
	if ( *pCount == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	(*pCount)++;
	return true;
}



/* 记录一个已知指令及其参数事实。 */
static bool __xrtHttpCacheKnownAdd(
	xhttpcachecontrol* pControl,
	const xhttpcacheitem* pItem
)
{
	uint32 iDirective = (uint32)pItem->Directive;
	uint64 iDelta = 0;
	bool bFirst = (pControl->Flags & iDirective) == 0;
	bool bValid = true;

	if ( !bFirst ) {
		if ( !__xrtHttpCacheCount(
			&pControl->DuplicateCount
		) ) {
			return false;
		}
		pControl->DuplicateDirectives |= iDirective;
		pControl->Flags |= XHTTP_CACHE_DUPLICATE;
	}
	pControl->Flags |= iDirective;
	switch ( pItem->Directive ) {
		case XHTTP_CACHE_MAX_AGE:
		case XHTTP_CACHE_MIN_FRESH:
		case XHTTP_CACHE_S_MAXAGE:
			bValid =
				((pItem->Flags &
				  XHTTP_PARAM_HAS_VALUE) != 0) &&
				__xrtHttpCacheDeltaParse(
					pItem->Value,
					(pItem->Flags &
					 XHTTP_PARAM_QUOTED) != 0,
					false,
					&iDelta
				);
			break;

		case XHTTP_CACHE_MAX_STALE:
			if ( (pItem->Flags &
				  XHTTP_PARAM_HAS_VALUE) == 0 ) {
				if ( bFirst ) {
					pControl->Flags |=
						XHTTP_CACHE_MAX_STALE_ANY;
				}
			} else {
				bValid = __xrtHttpCacheDeltaParse(
					pItem->Value,
					(pItem->Flags &
					 XHTTP_PARAM_QUOTED) != 0,
					false,
					&iDelta
				);
			}
			break;

		case XHTTP_CACHE_NO_CACHE:
		case XHTTP_CACHE_PRIVATE:
			bValid = __xrtHttpCacheFieldNames(pItem);
			if ( bValid &&
				((pItem->Flags &
				  XHTTP_PARAM_HAS_VALUE) != 0) ) {
				pControl->Flags |=
					pItem->Directive ==
						XHTTP_CACHE_NO_CACHE ?
					XHTTP_CACHE_NO_CACHE_FIELDS :
					XHTTP_CACHE_PRIVATE_FIELDS;
			}
			break;

		default:
			bValid = (pItem->Flags &
				XHTTP_PARAM_HAS_VALUE) == 0;
			break;
	}
	if ( !bValid ) {
		if ( !__xrtHttpCacheCount(
			&pControl->InvalidCount
		) ) {
			return false;
		}
		pControl->InvalidDirectives |= iDirective;
		pControl->Flags |= XHTTP_CACHE_INVALID;
		return true;
	}
	if ( !bFirst ) {
		return true;
	}
	if ( pItem->Directive == XHTTP_CACHE_MAX_AGE ) {
		pControl->MaxAge = iDelta;
	} else if ( pItem->Directive ==
		XHTTP_CACHE_MAX_STALE ) {
		pControl->MaxStale = iDelta;
	} else if ( pItem->Directive ==
		XHTTP_CACHE_MIN_FRESH ) {
		pControl->MinFresh = iDelta;
	} else if ( pItem->Directive ==
		XHTTP_CACHE_S_MAXAGE ) {
		pControl->SMaxAge = iDelta;
	}
	return true;
}



/* 记录一个结构正确的缓存指令。 */
static bool __xrtHttpCacheItemAdd(
	xhttpcachecontrol* pControl,
	const xhttpcacheitem* pItem
)
{
	if ( !__xrtHttpCacheCount(
		&pControl->DirectiveCount
	) ) {
		return false;
	}
	if ( pItem->Directive == XHTTP_CACHE_UNKNOWN ) {
		if ( !__xrtHttpCacheCount(
			&pControl->UnknownCount
		) ) {
			return false;
		}
		pControl->Flags |= XHTTP_CACHE_EXTENSION;
		return true;
	}
	return __xrtHttpCacheKnownAdd(pControl, pItem);
}



/* 依据互斥公开事实更新冲突标志。 */
static void __xrtHttpCacheConflict(
	xhttpcachecontrol* pControl
)
{
	pControl->Flags &= ~(uint32)XHTTP_CACHE_CONFLICT;
	if ( ((pControl->Flags & XHTTP_CACHE_PUBLIC) != 0) &&
		((pControl->Flags & XHTTP_CACHE_PRIVATE) != 0) ) {
		pControl->Flags |= XHTTP_CACHE_CONFLICT;
	}
}



/* 统计已知指令出现位，用于验证公开汇总计数。 */
static size_t __xrtHttpCacheDirectiveCount(uint32 iFlags)
{
	size_t iCount = 0;

	iFlags &= XRT_HTTP_CACHE_DIRECTIVES;
	while ( iFlags != 0 ) {
		iCount += (size_t)(iFlags & UINT32_C(1));
		iFlags >>= 1;
	}
	return iCount;
}



/* 返回已知缓存指令的规范名称。 */
XRT_API xstrview xrtHttpCacheDirectiveName(
	xhttpcachedirective Directive
)
{
	switch ( Directive ) {
		case XHTTP_CACHE_MAX_AGE:
			return XRT_STR_LITERAL("max-age");
		case XHTTP_CACHE_MAX_STALE:
			return XRT_STR_LITERAL("max-stale");
		case XHTTP_CACHE_MIN_FRESH:
			return XRT_STR_LITERAL("min-fresh");
		case XHTTP_CACHE_NO_CACHE:
			return XRT_STR_LITERAL("no-cache");
		case XHTTP_CACHE_NO_STORE:
			return XRT_STR_LITERAL("no-store");
		case XHTTP_CACHE_NO_TRANSFORM:
			return XRT_STR_LITERAL("no-transform");
		case XHTTP_CACHE_ONLY_IF_CACHED:
			return XRT_STR_LITERAL("only-if-cached");
		case XHTTP_CACHE_MUST_REVALIDATE:
			return XRT_STR_LITERAL("must-revalidate");
		case XHTTP_CACHE_MUST_UNDERSTAND:
			return XRT_STR_LITERAL("must-understand");
		case XHTTP_CACHE_PRIVATE:
			return XRT_STR_LITERAL("private");
		case XHTTP_CACHE_PROXY_REVALIDATE:
			return XRT_STR_LITERAL("proxy-revalidate");
		case XHTTP_CACHE_PUBLIC:
			return XRT_STR_LITERAL("public");
		case XHTTP_CACHE_S_MAXAGE:
			return XRT_STR_LITERAL("s-maxage");
		default:
			return (xstrview){ NULL, 0 };
	}
}



/* 大小写不敏感地映射一个缓存指令名称。 */
XRT_API xhttpcachedirective xrtHttpCacheDirectiveParse(
	xstrview Name
)
{
	static const xhttpcachedirective Directives[] = {
		XHTTP_CACHE_MAX_AGE,
		XHTTP_CACHE_MAX_STALE,
		XHTTP_CACHE_MIN_FRESH,
		XHTTP_CACHE_NO_CACHE,
		XHTTP_CACHE_NO_STORE,
		XHTTP_CACHE_NO_TRANSFORM,
		XHTTP_CACHE_ONLY_IF_CACHED,
		XHTTP_CACHE_MUST_REVALIDATE,
		XHTTP_CACHE_MUST_UNDERSTAND,
		XHTTP_CACHE_PRIVATE,
		XHTTP_CACHE_PROXY_REVALIDATE,
		XHTTP_CACHE_PUBLIC,
		XHTTP_CACHE_S_MAXAGE
	};
	size_t i;

	for ( i = 0;
		i < (sizeof(Directives) / sizeof(Directives[0]));
		i++ ) {
		if ( xrtHttpTokenEqual(
			Name,
			xrtHttpCacheDirectiveName(Directives[i])
		) ) {
			return Directives[i];
		}
	}
	return XHTTP_CACHE_UNKNOWN;
}



/* 初始化 Cache-Control 前向游标。 */
XRT_API void xrtHttpCacheCursorInit(xhttpcachecursor* pCursor)
{
	if ( pCursor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pCursor, 0, sizeof(*pCursor));
}



/* 跨越重复字段读取下一个缓存指令。 */
XRT_API xhttpnext xrtHttpCacheNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecursor* pCursor,
	xhttpcacheitem* pItem
)
{
	xhttpcachecursor Cursor;

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
		const xhttpfield* pField = &pFields[Cursor.Field];

		if ( !__xrtHttpCacheField(pField) ) {
			if ( Cursor.Offset != 0 ) {
				__xrtErrorSetInvalidArgument();
				return XHTTP_NEXT_ERROR;
			}
			Cursor.Field++;
			continue;
		}
		if ( Cursor.Offset > pField->Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		{
			xhttpparam Param;
			xhttpnext Next = xrtHttpDirectiveNext(
				pField->Value,
				&Cursor.Offset,
				&Param
			);

			if ( Next == XHTTP_NEXT_ERROR ) {
				return XHTTP_NEXT_ERROR;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				pItem->Name = Param.Name;
				pItem->Value = Param.Value;
				pItem->Directive =
					xrtHttpCacheDirectiveParse(Param.Name);
				pItem->Flags = Param.Flags;
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



/* 严格解析一个完整 delta-seconds。 */
XRT_API bool xrtHttpCacheDeltaParse(
	xstrview Text,
	uint64* pSeconds
)
{
	uint64 iSeconds;

	if ( !__xrtHttpViewValid(Text) ||
		(pSeconds == NULL) ||
		__xrtRangesOverlap(
			pSeconds, sizeof(*pSeconds),
			Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheDeltaParse(
		Text, false, true, &iSeconds
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pSeconds = iSeconds;
	return true;
}



/* 读取并饱和一个已知数值缓存指令。 */
XRT_API bool xrtHttpCacheDeltaRead(
	const xhttpcacheitem* pItem,
	uint64* pSeconds
)
{
	uint64 iSeconds;
	bool bNumeric;

	bNumeric = (pItem != NULL) &&
		((pItem->Directive == XHTTP_CACHE_MAX_AGE) ||
		 (pItem->Directive == XHTTP_CACHE_MAX_STALE) ||
		 (pItem->Directive == XHTTP_CACHE_MIN_FRESH) ||
		 (pItem->Directive == XHTTP_CACHE_S_MAXAGE));
	if ( !bNumeric || (pSeconds == NULL) ||
		!__xrtHttpCacheItemValid(pItem) ||
		__xrtRangesOverlap(
			pSeconds, sizeof(*pSeconds),
			pItem, sizeof(*pItem)
		) ||
		__xrtRangesOverlap(
			pSeconds, sizeof(*pSeconds),
			pItem->Name.Data, pItem->Name.Size
		) ||
		__xrtRangesOverlap(
			pSeconds, sizeof(*pSeconds),
			pItem->Value.Data, pItem->Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheDeltaParse(
		pItem->Value,
		(pItem->Flags & XHTTP_PARAM_QUOTED) != 0,
		false,
		&iSeconds
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pSeconds = iSeconds;
	return true;
}



/* 初始化空缓存控制汇总。 */
XRT_API void xrtHttpCacheControlInit(
	xhttpcachecontrol* pControl
)
{
	if ( pControl == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pControl, 0, sizeof(*pControl));
}



/* 验证公开缓存控制汇总的内部一致性。 */
XRT_API bool xrtHttpCacheControlValid(
	const xhttpcachecontrol* pControl
)
{
	size_t iKnown;
	bool bPresent;
	bool bConflict;

	if ( (pControl == NULL) ||
		((pControl->Flags & ~XRT_HTTP_CACHE_FLAGS) != 0) ||
		((pControl->DuplicateDirectives &
		  ~XRT_HTTP_CACHE_DIRECTIVES) != 0) ||
		((pControl->InvalidDirectives &
		  ~XRT_HTTP_CACHE_DIRECTIVES) != 0) ||
		((pControl->DuplicateDirectives &
		  ~pControl->Flags) != 0) ||
		((pControl->InvalidDirectives &
		  ~pControl->Flags) != 0) ||
		(pControl->MaxAge > XHTTP_CACHE_DELTA_MAX) ||
		(pControl->MaxStale > XHTTP_CACHE_DELTA_MAX) ||
		(pControl->MinFresh > XHTTP_CACHE_DELTA_MAX) ||
		(pControl->SMaxAge > XHTTP_CACHE_DELTA_MAX) ||
		(pControl->UnknownCount >
		 pControl->DirectiveCount) ||
		(pControl->DuplicateCount >
		 pControl->DirectiveCount) ||
		(pControl->InvalidCount >
		 pControl->DirectiveCount) ) {
		return false;
	}
	iKnown = __xrtHttpCacheDirectiveCount(pControl->Flags);
	if ( (iKnown > (SIZE_MAX -
		  pControl->DuplicateCount)) ||
		((iKnown + pControl->DuplicateCount) >
		 (SIZE_MAX - pControl->UnknownCount)) ||
		(pControl->DirectiveCount !=
		 (iKnown + pControl->DuplicateCount +
		  pControl->UnknownCount)) ||
		(pControl->InvalidCount >
		 (iKnown + pControl->DuplicateCount)) ) {
		return false;
	}
	bPresent = pControl->FieldCount != 0;
	bConflict =
		((pControl->Flags & XHTTP_CACHE_PUBLIC) != 0) &&
		((pControl->Flags & XHTTP_CACHE_PRIVATE) != 0);
	if ( (((pControl->Flags &
		   XHTTP_CACHE_PRESENT) != 0) != bPresent) ||
		(((pControl->Flags &
		   XHTTP_CACHE_EXTENSION) != 0) !=
		 (pControl->UnknownCount != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_DUPLICATE) != 0) !=
		 (pControl->DuplicateCount != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_INVALID) != 0) !=
		 (pControl->InvalidCount != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_CONFLICT) != 0) != bConflict) ) {
		return false;
	}
	if ( ((pControl->DuplicateCount == 0) !=
		  (pControl->DuplicateDirectives == 0)) ||
		((pControl->InvalidCount == 0) !=
		  (pControl->InvalidDirectives == 0)) ||
		(__xrtHttpCacheDirectiveCount(
			pControl->DuplicateDirectives
		 ) > pControl->DuplicateCount) ||
		(__xrtHttpCacheDirectiveCount(
			pControl->InvalidDirectives
		 ) > pControl->InvalidCount) ) {
		return false;
	}
	if ( (!bPresent &&
		 ((pControl->DirectiveCount != 0) ||
		  (pControl->Flags != 0))) ||
		(((pControl->Flags &
		   XHTTP_CACHE_MAX_AGE) == 0) &&
		 (pControl->MaxAge != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_MAX_STALE) == 0) &&
		 (pControl->MaxStale != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_MIN_FRESH) == 0) &&
		 (pControl->MinFresh != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_S_MAXAGE) == 0) &&
		 (pControl->SMaxAge != 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_MAX_STALE_ANY) != 0) &&
		 (((pControl->Flags &
		    XHTTP_CACHE_MAX_STALE) == 0) ||
		  (pControl->MaxStale != 0))) ||
		(((pControl->Flags &
		   XHTTP_CACHE_NO_CACHE_FIELDS) != 0) &&
		 ((pControl->Flags &
		   XHTTP_CACHE_NO_CACHE) == 0)) ||
		(((pControl->Flags &
		   XHTTP_CACHE_PRIVATE_FIELDS) != 0) &&
		 ((pControl->Flags &
		   XHTTP_CACHE_PRIVATE) == 0)) ) {
		return false;
	}
	return true;
}



/* 失败原子地合并一个 Cache-Control 字段值。 */
XRT_API bool xrtHttpCacheControlAdd(
	xhttpcachecontrol* pControl,
	xstrview Value
)
{
	xhttpcachecontrol Control;
	size_t iOffset = 0;

	if ( (pControl == NULL) ||
		!xrtHttpCacheControlValid(pControl) ||
		!__xrtHttpViewValid(Value) ||
		__xrtRangesOverlap(
			pControl, sizeof(*pControl),
			Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Control = *pControl;
	if ( !__xrtHttpCacheCount(&Control.FieldCount) ) {
		return false;
	}
	Control.Flags |= XHTTP_CACHE_PRESENT;
	for ( ;; ) {
		xhttpparam Param;
		xhttpcacheitem Item;
		xhttpnext Next = xrtHttpDirectiveNext(
			Value, &iOffset, &Param
		);

		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		Item.Name = Param.Name;
		Item.Value = Param.Value;
		Item.Directive =
			xrtHttpCacheDirectiveParse(Param.Name);
		Item.Flags = Param.Flags;
		if ( !__xrtHttpCacheItemAdd(
			&Control, &Item
		) ) {
			return false;
		}
	}
	__xrtHttpCacheConflict(&Control);
	*pControl = Control;
	return true;
}



/* 扫描重复字段并建立缓存控制汇总。 */
XRT_API bool xrtHttpCacheControlParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecontrol* pControl
)
{
	xhttpcachecontrol Control;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iCount
	) || (pControl == NULL) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pControl, sizeof(*pControl)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Control, 0, sizeof(Control));
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheField(&pFields[i]) &&
			!xrtHttpCacheControlAdd(
				&Control, pFields[i].Value
			) ) {
			return false;
		}
	}
	*pControl = Control;
	return true;
}

#endif
