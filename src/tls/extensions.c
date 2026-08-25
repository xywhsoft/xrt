#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HELLO)
/* 报告 Hello 与扩展语义层的协议错误。 */
bool __xrtTlsHelloError(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
)
{
	__xrtTlsError(
		XERR_PROTOCOL, Code, sOperation, sMessage, iOffset
	);
	return false;
}



/* 返回 256 个精确桶中某个标识对应的位。 */
uint64 __xrtTlsHelloSeenBit(uint16 iValue)
{
	return UINT64_C(1) << ((uint8)iValue & 63u);
}



/* 检查扩展类型是否已经出现在游标此前消费的区域。 */
static bool __xrtTlsExtensionSeen(
	const xtlsextensioncursor* pCursor,
	uint16 iType
)
{
	size_t iOffset = 0;
	uint8 iBucket = (uint8)iType;
	uint64 iBit = __xrtTlsHelloSeenBit(iType);

	if ( (pCursor->Seen[iBucket >> 6u] & iBit) == 0 ) {
		return false;
	}
	while ( iOffset < pCursor->Offset ) {
		size_t iRemaining = pCursor->Offset - iOffset;
		size_t iSize;

		if ( iRemaining < XTLS_EXTENSION_HEADER_SIZE ) {
			return false;
		}
		iSize = XTLS_EXTENSION_HEADER_SIZE +
			__xrtTlsRead16(pCursor->Data.Data + iOffset + 2u);
		if ( iSize > iRemaining ) {
			return false;
		}

		if ( __xrtTlsRead16(pCursor->Data.Data + iOffset) == iType ) {
			return true;
		}
		iOffset += iSize;
	}
	return false;
}



/* 标记一个已经成功读取的扩展类型桶。 */
static void __xrtTlsExtensionMark(
	xtlsextensioncursor* pCursor,
	uint16 iType
)
{
	uint8 iBucket = (uint8)iType;

	pCursor->Seen[iBucket >> 6u] |= __xrtTlsHelloSeenBit(iType);
}



/* 初始化借用完整扩展向量的游标。 */
XRT_API bool xrtTlsExtensionsInit(
	xtlsextensioncursor* pCursor,
	xbytesview Extensions
)
{
	xtlsextensioncursor Cursor;

	if ( (pCursor == NULL) || !__xrtTlsViewValid(Extensions) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "init-extensions",
			"TLS extension cursor input or output is invalid", SIZE_MAX
		);
		return false;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Data = Extensions;
	*pCursor = Cursor;
	return true;
}



/* 读取下一扩展并拒绝任何重复类型。 */
XRT_API xtlsitemresult xrtTlsExtensionsRead(
	xtlsextensioncursor* pCursor,
	xtlsextension* pExtension
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xbytesview Remaining;
	xtlsresult Result;
	uint16 iType;

	if ( (pCursor == NULL) || (pExtension == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) && (pCursor->Offset > pCursor->Data.Size)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-extensions",
			"TLS extension cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	Remaining.Data = Cursor.Data.Data + Cursor.Offset;
	Remaining.Size = Cursor.Data.Size - Cursor.Offset;
	Result = xrtTlsExtensionParse(Remaining, &Extension, NULL);
	if ( Result != XTLS_OK ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION, "read-extensions",
			"TLS extension vector contains a truncated item", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	iType = (uint16)Extension.Type;
	if ( __xrtTlsExtensionSeen(&Cursor, iType) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION, "read-extensions",
			"TLS extension type appears more than once", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	__xrtTlsExtensionMark(&Cursor, iType);
	Cursor.Offset += Extension.EncodedSize;
	*pCursor = Cursor;
	*pExtension = Extension;
	return XTLS_ITEM_VALUE;
}



/* 完整验证扩展向量。 */
XRT_API bool xrtTlsExtensionsValidate(xbytesview Extensions)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;

	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	do {
		Result = xrtTlsExtensionsRead(&Cursor, &Extension);
	} while ( Result == XTLS_ITEM_VALUE );
	return Result == XTLS_ITEM_DONE;
}



/* 完整验证后查找唯一扩展。 */
XRT_API xtlsitemresult xrtTlsExtensionsFind(
	xbytesview Extensions,
	xtlsextensiontype Type,
	xtlsextension* pExtension
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsextension Found = {0};
	xtlsitemresult Result;
	bool bFound = false;

	if ( (pExtension == NULL) || ((uint32)Type > UINT16_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "find-extension",
			"TLS extension type or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return XTLS_ITEM_ERROR;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( Extension.Type == Type ) {
			Found = Extension;
			bFound = true;
		}
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return Result;
	}
	if ( !bFound ) {
		return XTLS_ITEM_DONE;
	}
	*pExtension = Found;
	return XTLS_ITEM_VALUE;
}



/* 返回 16 位标识列表的元素数量。 */

#endif
