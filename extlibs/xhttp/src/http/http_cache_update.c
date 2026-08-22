#include "../internal/xrt_http_cache_validate.h"

#include <xrt/http_forward.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断候选是否与 304 中任一强验证器匹配。 */
static bool __xrtHttpCache304StrongMatch(
	const xrt_http_cache_validator* pResponse,
	const xrt_http_cache_validator* pStored
)
{
	if ( pResponse->ETagValid &&
		!pResponse->ETag.Weak &&
		pStored->ETagValid &&
		xrtHttpETagStrongEqual(
			&pResponse->ETag,
			&pStored->ETag
		) ) {
		return true;
	}
	return pResponse->LastModifiedValid &&
		pResponse->LastModifiedStrong &&
		pStored->LastModifiedValid &&
		pStored->LastModifiedStrong &&
		__xrtHttpCacheDateEqual(
			pResponse->LastModified,
			pStored->LastModified
		);
}



/* 判断候选是否与 304 中任一弱验证器匹配。 */
static bool __xrtHttpCache304WeakMatch(
	const xrt_http_cache_validator* pResponse,
	const xrt_http_cache_validator* pStored
)
{
	if ( pResponse->ETagValid &&
		pStored->ETagValid &&
		xrtHttpETagWeakEqual(
			&pResponse->ETag,
			&pStored->ETag
		) ) {
		return true;
	}
	return pResponse->LastModifiedValid &&
		pStored->LastModifiedValid &&
		__xrtHttpCacheDateEqual(
			pResponse->LastModified,
			pStored->LastModified
		);
}



/* 判断验证元数据是否包含任何有效强验证器。 */
static bool __xrtHttpCacheValidatorStrong(
	const xrt_http_cache_validator* pValidator
)
{
	return (pValidator->ETagValid &&
		!pValidator->ETag.Weak) ||
		(pValidator->LastModifiedValid &&
		 pValidator->LastModifiedStrong);
}



/* 判断验证元数据是否包含任何有效验证器。 */
static bool __xrtHttpCacheValidatorAny(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagValid ||
		pValidator->LastModifiedValid;
}



/* 判断验证元数据是否在线路上出现了验证器字段。 */
static bool __xrtHttpCacheValidatorPresent(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagPresent ||
		pValidator->LastModifiedPresent;
}



/* 统计与 304 强验证器匹配的全部候选。 */
static size_t __xrtHttpCache304StrongCount(
	const xrt_http_cache_validator* pResponse,
	const xhttpcacheentry* pEntries,
	size_t iCount
)
{
	xrt_http_cache_validator Stored;
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Stored
		);
		if ( __xrtHttpCache304StrongMatch(
			pResponse, &Stored
		) ) {
			iMatches++;
		}
	}
	return iMatches;
}



/* 选择与弱验证器匹配且 Date 最新的唯一候选。 */
static bool __xrtHttpCache304WeakLatest(
	const xrt_http_cache_validator* pResponse,
	const xhttpcacheentry* pEntries,
	size_t iCount,
	size_t* pIndex
)
{
	xrt_http_cache_validator Stored;
	xtime iNewest = 0;
	xtime iDate;
	size_t iNewestIndex = 0;
	bool bFound = false;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Stored
		);
		if ( !__xrtHttpCache304WeakMatch(
			pResponse, &Stored
		) ) {
			continue;
		}
		iDate = __xrtHttpCacheEntryDate(
			&pEntries[i], &Stored
		);
		if ( !bFound || (iDate > iNewest) ||
			((iDate == iNewest) &&
			 (i > iNewestIndex)) ) {
			bFound = true;
			iNewest = iDate;
			iNewestIndex = i;
		}
	}
	if ( bFound ) {
		*pIndex = iNewestIndex;
	}
	return bFound;
}



/* 验证 304 下标输出不会覆盖输入或数量输出。 */
static bool __xrtHttpCache304OutputValid(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iSelected,
	size_t* pCount
)
{
	size_t iBytes;

	if ( iSelected > (SIZE_MAX / sizeof(*pIndices)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = iSelected * sizeof(*pIndices);
	if ( __xrtHttpFieldArrayOverlap(
		pFields, iFieldCount,
		pIndices, iBytes
	) || __xrtHttpCacheEntriesOverlap(
		pEntries, iEntryCount,
		pIndices, iBytes
	) || __xrtRangesOverlap(
		pIndices, iBytes,
		pCount, sizeof(*pCount)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 按 304 中的验证器选择应更新的候选下标。 */
XRT_API xhttpcacheupdatematch xrtHttpCache304Select(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iCapacity,
	size_t* pCount
)
{
	xrt_http_cache_validator Response;
	xrt_http_cache_validator Stored;
	xhttpcacheupdatematch Match =
		XHTTP_CACHE_UPDATE_MATCH_NONE;
	size_t iSelected = 0;
	size_t iWeak = 0;
	size_t iWrite = 0;
	size_t iOutputBytes;
	size_t i;

	if ( (pCount == NULL) ||
		((pIndices == NULL) && (iCapacity != 0)) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iFieldCount
		) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iEntryCount
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iFieldCount,
			pCount, sizeof(*pCount)
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iEntryCount,
			pCount, sizeof(*pCount)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(*pIndices)) ) {
		__xrtErrorSetSizeOverflow();
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	iOutputBytes = iCapacity * sizeof(*pIndices);
	if ( __xrtRangesOverlap(
		pIndices, iOutputBytes,
		pCount, sizeof(*pCount)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	*pCount = 0;
	(void)__xrtHttpCacheValidatorRead(
		pFields, iFieldCount, &Response
	);

	/* 强验证器优先并选择全部匹配候选。 */
	if ( __xrtHttpCacheValidatorStrong(&Response) ) {
		iSelected = __xrtHttpCache304StrongCount(
			&Response, pEntries, iEntryCount
		);
		if ( iSelected != 0 ) {
			Match = XHTTP_CACHE_UPDATE_MATCH_STRONG;
		}
	} else if ( __xrtHttpCacheValidatorAny(&Response) ) {
		if ( __xrtHttpCache304WeakLatest(
			&Response,
			pEntries,
			iEntryCount,
			&iWeak
		) ) {
			iSelected = 1;
			Match = XHTTP_CACHE_UPDATE_MATCH_WEAK;
		}
	} else if (
		!__xrtHttpCacheValidatorPresent(&Response) &&
		(iEntryCount == 1) ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[0].Fields,
			pEntries[0].FieldCount,
			&Stored
		);
		if ( !__xrtHttpCacheValidatorPresent(
			&Stored
		) ) {
			iSelected = 1;
			Match = XHTTP_CACHE_UPDATE_MATCH_SINGLE;
		}
	}
	*pCount = iSelected;
	if ( (iSelected == 0) || (pIndices == NULL) ) {
		return Match;
	}
	if ( iCapacity < iSelected ) {
		__xrtErrorSetRange();
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	if ( !__xrtHttpCache304OutputValid(
		pFields, iFieldCount,
		pEntries, iEntryCount,
		pIndices, iSelected, pCount
	) ) {
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}

	/* 容量已经确认后再写下标，保持短缓冲原子性。 */
	if ( Match == XHTTP_CACHE_UPDATE_MATCH_STRONG ) {
		for ( i = 0; i < iEntryCount; i++ ) {
			(void)__xrtHttpCacheValidatorRead(
				pEntries[i].Fields,
				pEntries[i].FieldCount,
				&Stored
			);
			if ( __xrtHttpCache304StrongMatch(
				&Response, &Stored
			) ) {
				pIndices[iWrite++] = i;
			}
		}
	} else if ( Match == XHTTP_CACHE_UPDATE_MATCH_WEAK ) {
		pIndices[0] = iWeak;
		iWrite = 1;
	} else if ( Match == XHTTP_CACHE_UPDATE_MATCH_SINGLE ) {
		pIndices[0] = 0;
		iWrite = 1;
	}
	return (iWrite == iSelected) ?
		Match :
		XHTTP_CACHE_UPDATE_MATCH_ERROR;
}



/* 判断 HEAD 中出现的 ETag 是否与已保存响应精确相同。 */
static bool __xrtHttpCacheHeadETagMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->ETagPresent ) {
		return true;
	}
	return pHead->ETagValid &&
		pStored->ETagValid &&
		__xrtHttpCacheETagExact(
			&pHead->ETag,
			&pStored->ETag
		);
}



/* 判断 HEAD 中出现的 Last-Modified 是否与已保存响应相同。 */
static bool __xrtHttpCacheHeadDateMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->LastModifiedPresent ) {
		return true;
	}
	return pHead->LastModifiedValid &&
		pStored->LastModifiedValid &&
		__xrtHttpCacheDateEqual(
			pHead->LastModified,
			pStored->LastModified
		);
}



/* 判断 HEAD 中出现的 Content-Length 是否与已保存响应相同。 */
static bool __xrtHttpCacheHeadLengthMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->ContentLengthPresent ) {
		return true;
	}
	return pHead->ContentLengthValid &&
		pStored->ContentLengthPresent &&
		pStored->ContentLengthValid &&
		(pHead->ContentLength ==
		 pStored->ContentLength);
}



/* 判断 200 HEAD 是否能更新一个已保存 GET 响应。 */
XRT_API xhttpcacheheaddecision xrtHttpCacheHeadPlan(
	uint16 iStatus,
	const xhttpcacheentry* pEntry,
	const xhttpfield* pFields,
	size_t iFieldCount
)
{
	xrt_http_cache_validator Head;
	xrt_http_cache_validator Stored;

	if ( (iStatus < 200u) || (iStatus > 599u) ||
		!xrtHttpCacheEntryValid(pEntry) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iFieldCount
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_HEAD_ERROR;
	}
	if ( iStatus != XHTTP_STATUS_OK ) {
		return XHTTP_CACHE_HEAD_IGNORE;
	}
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Stored
	);
	(void)__xrtHttpCacheValidatorRead(
		pFields, iFieldCount, &Head
	);
	if ( !__xrtHttpCacheHeadETagMatch(
		&Head, &Stored
	) || !__xrtHttpCacheHeadDateMatch(
		&Head, &Stored
	) || !__xrtHttpCacheHeadLengthMatch(
		&Head, &Stored
	) ) {
		return XHTTP_CACHE_HEAD_STALE;
	}
	return XHTTP_CACHE_HEAD_UPDATE;
}



/* 从 quoted-string 正文读取一个已经解码 quoted-pair 的字节。 */
static bool __xrtHttpCacheQuotedByte(
	xstrview Value,
	size_t* pOffset,
	unsigned char* pByte
)
{
	unsigned char iByte;

	if ( *pOffset >= Value.Size ) {
		return false;
	}
	iByte = (unsigned char)Value.Data[*pOffset];
	(*pOffset)++;
	if ( iByte == (unsigned char)'\\' ) {
		if ( *pOffset >= Value.Size ) {
			return false;
		}
		iByte = (unsigned char)Value.Data[*pOffset];
		(*pOffset)++;
	}
	*pByte = iByte;
	return true;
}



/* 判断解码后的字节是否为 OWS。 */
static bool __xrtHttpCacheQualifiedOws(unsigned char iByte)
{
	return (iByte == (unsigned char)' ') ||
		(iByte == (unsigned char)'\t');
}



/*
	在限定字段名列表中查找目标名称。
	函数同时完整验证解码后的 #field-name，避免命中前缀后忽略坏尾部。
*/
static bool __xrtHttpCacheQualifiedHas(
	const xhttpcacheitem* pItem,
	xstrview Name,
	bool* pValid
)
{
	size_t iOffset = 0;
	size_t iName;
	size_t iItems = 0;
	unsigned char iByte;
	bool bHaveByte = false;
	bool bMatch = false;
	bool bItemMatch;

	*pValid = true;
	if ( (pItem->Flags &
		 XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	if ( (pItem->Flags &
		 XHTTP_PARAM_QUOTED) == 0 ) {
		if ( !xrtHttpTokenValid(pItem->Value) ) {
			*pValid = false;
			return false;
		}
		return xrtHttpFieldNameEqual(
			pItem->Value, Name
		);
	}

	/* quoted-string 先按 quoted-pair 解码，再按逗号拆分 field-name。 */
	while ( iOffset < pItem->Value.Size ) {
		bHaveByte = false;
		while ( iOffset < pItem->Value.Size ) {
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
			if ( !__xrtHttpCacheQualifiedOws(iByte) &&
				(iByte != (unsigned char)',') ) {
				bHaveByte = true;
				break;
			}
		}
		if ( !bHaveByte ) {
			break;
		}
		iName = 0;
		bItemMatch = true;
		do {
			if ( !__xrtHttpTokenByte(iByte) ) {
				*pValid = false;
				return false;
			}
			if ( (iName >= Name.Size) ||
				(__xhttpAsciiLower(iByte) !=
				 __xhttpAsciiLower(
					(unsigned char)Name.Data[iName]
				 )) ) {
				bItemMatch = false;
			}
			iName++;
			if ( iOffset >= pItem->Value.Size ) {
				bHaveByte = false;
				break;
			}
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
		} while ( !__xrtHttpCacheQualifiedOws(iByte) &&
			(iByte != (unsigned char)',') );
		if ( bItemMatch && (iName == Name.Size) ) {
			bMatch = true;
		}
		iItems++;

		/* token 后只允许 OWS、逗号或输入结束。 */
		while ( bHaveByte &&
			__xrtHttpCacheQualifiedOws(iByte) ) {
			if ( iOffset >= pItem->Value.Size ) {
				bHaveByte = false;
				break;
			}
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
		}
		if ( bHaveByte &&
			(iByte != (unsigned char)',') ) {
			*pValid = false;
			return false;
		}
	}
	if ( iItems == 0 ) {
		*pValid = false;
		return false;
	}
	return bMatch;
}



/* 判断字段是否被 Cache-Control 的限定 no-cache/private 排除。 */
static xhttpcachefieldupdate __xrtHttpCacheQualifiedUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool Shared
)
{
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	xhttpnext Next;
	bool bValid;
	bool bFound;

	xrtHttpCacheCursorInit(&Cursor);
	while ( (Next = xrtHttpCacheNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Directive != XHTTP_CACHE_NO_CACHE) &&
			(!Shared ||
			 (Item.Directive != XHTTP_CACHE_PRIVATE)) ) {
			continue;
		}
		bFound = __xrtHttpCacheQualifiedHas(
			&Item, Name, &bValid
		);
		if ( !bValid ) {
			__xrtErrorSetValue();
			return XHTTP_CACHE_FIELD_UPDATE_ERROR;
		}
		if ( bFound ) {
			return XHTTP_CACHE_FIELD_UPDATE_SKIP;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	return XHTTP_CACHE_FIELD_UPDATE_REPLACE;
}



/* 判断字段是否被 Connection 字段提名为逐跳字段。 */
static xhttpcachefieldupdate __xrtHttpCacheConnectionUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	xhttpnext Next = xrtHttpConnectionFind(
		pFields, iCount, Name
	);

	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	return (Next == XHTTP_NEXT_ITEM) ?
		XHTTP_CACHE_FIELD_UPDATE_SKIP :
		XHTTP_CACHE_FIELD_UPDATE_REPLACE;
}



/* 判断字段是否属于代理连接身份专用元数据。 */
static bool __xrtHttpCacheProxyField(xstrview Name)
{
	return xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Proxy-Authenticate")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Proxy-Authentication-Info")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Proxy-Authorization")
	);
}



/* 判断新响应中的一个字段是否可替换已保存字段。 */
XRT_API xhttpcachefieldupdate xrtHttpCacheFieldUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iFlags
)
{
	xstrview Name;
	xhttpcachefieldupdate Result;

	if ( !__xrtHttpCacheFieldsValid(
		pFields, iCount
	) || (iIndex >= iCount) ||
		((iFlags & ~(
			XHTTP_CACHE_UPDATE_FIELD_DEPENDENT |
			XHTTP_CACHE_UPDATE_FIELD_PROCESSED
		 )) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	if ( (iFlags & (
		XHTTP_CACHE_UPDATE_FIELD_DEPENDENT |
		XHTTP_CACHE_UPDATE_FIELD_PROCESSED
	 )) != 0 ) {
		return XHTTP_CACHE_FIELD_UPDATE_SKIP;
	}
	Name = pFields[iIndex].Name;
	if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Content-Length")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Connection")
	) || __xrtHttpCacheProxyField(Name) ) {
		return XHTTP_CACHE_FIELD_UPDATE_SKIP;
	}
	Result = __xrtHttpCacheConnectionUpdate(
		pFields, iCount, Name
	);
	if ( Result != XHTTP_CACHE_FIELD_UPDATE_REPLACE ) {
		return Result;
	}
	return __xrtHttpCacheQualifiedUpdate(
		pFields, iCount, Name, Shared
	);
}



/* 按存储计划动作过滤初次保存的响应字段。 */
XRT_API xhttpcachefieldstore xrtHttpCacheFieldStore(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iActions
)
{
	xstrview Name;
	xhttpcachefieldupdate Result;
	uint32 iKnown =
		XHTTP_CACHE_STORE_REMOVE_CONNECTION |
		XHTTP_CACHE_STORE_REMOVE_PROXY |
		XHTTP_CACHE_STORE_SEPARATE_TRAILERS |
		XHTTP_CACHE_STORE_REMOVE_NO_CACHE |
		XHTTP_CACHE_STORE_REMOVE_PRIVATE |
		XHTTP_CACHE_STORE_MARK_INCOMPLETE |
		XHTTP_CACHE_STORE_AS_200 |
		XHTTP_CACHE_STORE_IGNORE_NO_STORE;

	if ( !__xrtHttpCacheFieldsValid(
		pFields, iCount
	) || (iIndex >= iCount) ||
		((iActions & ~iKnown) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_FIELD_STORE_ERROR;
	}
	Name = pFields[iIndex].Name;
	if ( (iActions &
		XHTTP_CACHE_STORE_REMOVE_CONNECTION) != 0 ) {
		if ( xrtHttpHopFieldKnown(Name) ||
			xrtHttpFieldNameEqual(
				Name, XRT_STR_LITERAL("Trailer")
			) ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
		Result = __xrtHttpCacheConnectionUpdate(
			pFields, iCount, Name
		);
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return XHTTP_CACHE_FIELD_STORE_ERROR;
		}
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
	}
	if ( ((iActions &
		  XHTTP_CACHE_STORE_REMOVE_PROXY) != 0) &&
		__xrtHttpCacheProxyField(Name) ) {
		return XHTTP_CACHE_FIELD_STORE_SKIP;
	}
	if ( (iActions & (
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE |
			XHTTP_CACHE_STORE_REMOVE_PRIVATE
		 )) != 0 ) {
		Result = __xrtHttpCacheQualifiedUpdate(
			pFields, iCount, Name, Shared
		);
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return XHTTP_CACHE_FIELD_STORE_ERROR;
		}
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
	}
	return XHTTP_CACHE_FIELD_STORE_KEEP;
}

#endif
