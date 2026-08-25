#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HELLO)
/* 验证偶数字节的 16 位标识列表中不存在重复项。 */
static bool __xrtTlsIdsUnique(
	xbytesview Data,
	cstr sOperation,
	cstr sMessage,
	xerrkind Kind
)
{
	uint64 Seen[4] = { 0, 0, 0, 0 };

	for ( size_t i = 0; i < Data.Size; i += 2u ) {
		uint16 iValue = __xrtTlsRead16(Data.Data + i);
		uint8 iBucket = (uint8)iValue;
		uint64 iBit = __xrtTlsHelloSeenBit(iValue);

		if ( (Seen[iBucket >> 6u] & iBit) != 0 ) {
			for ( size_t j = 0; j < i; j += 2u ) {
				if ( __xrtTlsRead16(Data.Data + j) == iValue ) {
					__xrtTlsError(
						Kind, XTLS_ERROR_EXTENSION, sOperation,
						sMessage, i
					);
					return false;
				}
			}
		}
		Seen[iBucket >> 6u] |= iBit;
	}
	return true;
}



/* 验证去除线路长度前缀后的非空偶数标识列表。 */
bool __xrtTlsIdsDataValid(
	xbytesview Data,
	cstr sOperation,
	xerrkind Kind
)
{
	if ( !__xrtTlsViewValid(Data) || (sOperation == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-tls-ids",
			"TLS identifier list is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Data.Size < 2u) || ((Data.Size & 1u) != 0) ) {
		__xrtTlsError(
			Kind, XTLS_ERROR_EXTENSION, sOperation,
			"TLS identifier list must contain complete 16-bit values", 0
		);
		return false;
	}
	return __xrtTlsIdsUnique(
		Data, sOperation, "TLS identifier list contains a duplicate", Kind
	);
}



/* 解析带 16 位字节长度的非空偶数标识列表。 */
static bool __xrtTlsIds16(
	xbytesview Data,
	xtlsids* pIds,
	cstr sOperation
)
{
	xtlsids Ids;
	size_t iSize;

	if ( (pIds == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS identifier list input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 4u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, sOperation,
			"TLS identifier list is empty or truncated", Data.Size
		);
	}
	iSize = __xrtTlsRead16(Data.Data);
	if ( iSize != Data.Size - 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, sOperation,
			"TLS identifier list length is inconsistent", 0
		);
	}
	Ids.Data.Data = Data.Data + 2u;
	Ids.Data.Size = iSize;
	if ( !__xrtTlsIdsDataValid(Ids.Data, sOperation, XERR_PROTOCOL) ) {
		return false;
	}
	*pIds = Ids;
	return true;
}



XRT_API size_t xrtTlsIdsCount(const xtlsids* pIds)
{
	if ( (pIds == NULL) || !__xrtTlsViewValid(
		pIds != NULL ? pIds->Data : (xbytesview) { NULL, 1u }
	) || ((pIds != NULL) && ((pIds->Data.Size & 1u) != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "count-tls-ids",
			"TLS identifier list is invalid", SIZE_MAX
		);
		return 0;
	}
	return pIds->Data.Size / 2u;
}



/* 按索引读取 16 位标识。 */
XRT_API bool xrtTlsIdsGet(
	const xtlsids* pIds,
	size_t iIndex,
	uint16* pValue
)
{
	size_t iCount;

	if ( (pIds == NULL) || (pValue == NULL) ||
		!__xrtTlsViewValid(pIds != NULL ?
			pIds->Data : (xbytesview) { NULL, 1u }) ||
		((pIds != NULL) && ((pIds->Data.Size & 1u) != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-id",
			"TLS identifier list or output is invalid", SIZE_MAX
		);
		return false;
	}
	iCount = pIds->Data.Size / 2u;
	if ( iIndex >= iCount ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "get-tls-id",
			"TLS identifier index is out of range", iIndex
		);
		return false;
	}
	*pValue = __xrtTlsRead16(pIds->Data.Data + (iIndex * 2u));
	return true;
}



/* 判断标识列表是否包含给定线路值。 */
XRT_API bool xrtTlsIdsContain(const xtlsids* pIds, uint16 iValue)
{
	if ( (pIds == NULL) || !__xrtTlsViewValid(
		pIds != NULL ? pIds->Data : (xbytesview) { NULL, 1u }
	) || ((pIds != NULL) && ((pIds->Data.Size & 1u) != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "contain-tls-id",
			"TLS identifier list is invalid", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < pIds->Data.Size; i += 2u ) {
		if ( __xrtTlsRead16(pIds->Data.Data + i) == iValue ) {
			return true;
		}
	}
	return false;
}



/* 严格解析客户端支持版本列表。 */
XRT_API bool xrtTlsClientVersions(
	xbytesview Data,
	xtlsids* pVersions
)
{
	xtlsids Versions;
	size_t iSize;

	if ( (pVersions == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-client-versions",
			"TLS version list input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 3u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-client-versions",
			"TLS client version list is empty or truncated", Data.Size
		);
	}
	iSize = Data.Data[0];
	if ( (iSize != Data.Size - 1u) || ((iSize & 1u) != 0) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-client-versions",
			"TLS client version list length is inconsistent", 0
		);
	}
	Versions.Data.Data = Data.Data + 1u;
	Versions.Data.Size = iSize;
	if ( !__xrtTlsIdsUnique(
		Versions.Data, "parse-client-versions",
		"TLS client version list contains a duplicate", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pVersions = Versions;
	return true;
}



/* 严格解析服务端选择版本。 */
XRT_API bool xrtTlsServerVersion(xbytesview Data, uint16* pVersion)
{
	if ( (pVersion == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-server-version",
			"TLS selected version input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size != 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-server-version",
			"TLS selected version must contain exactly two bytes", 0
		);
	}
	*pVersion = __xrtTlsRead16(Data.Data);
	return true;
}



/* 严格解析命名组列表。 */
XRT_API bool xrtTlsGroups(xbytesview Data, xtlsids* pGroups)
{
	return __xrtTlsIds16(Data, pGroups, "parse-supported-groups");
}



/* 严格解析签名方案列表。 */
XRT_API bool xrtTlsSignatures(xbytesview Data, xtlsids* pSignatures)
{
	return __xrtTlsIds16(
		Data, pSignatures, "parse-signature-algorithms"
	);
}



/* 读取下一 SNI 名称。 */
XRT_API xtlsitemresult xrtTlsServerNamesRead(
	xtlsservernamecursor* pCursor,
	xtlsservername* pName
)
{
	xtlsservernamecursor Cursor;
	xtlsservername Name;
	size_t iRemaining;
	size_t iNameSize;
	uint64 iBit;

	if ( (pCursor == NULL) || (pName == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) && (pCursor->Offset > pCursor->Data.Size)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-server-names",
			"TLS server-name cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	iRemaining = Cursor.Data.Size - Cursor.Offset;
	if ( iRemaining < 3u ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-server-names",
			"TLS server-name entry is truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Name.Type = Cursor.Data.Data[Cursor.Offset];
	iNameSize = __xrtTlsRead16(Cursor.Data.Data + Cursor.Offset + 1u);
	if ( (iNameSize == 0) || (iNameSize > iRemaining - 3u) ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-server-names",
			"TLS server-name length is invalid", Cursor.Offset + 1u
		);
		return XTLS_ITEM_ERROR;
	}
	iBit = __xrtTlsHelloSeenBit(Name.Type);
	if ( (Cursor.Seen[Name.Type >> 6u] & iBit) != 0 ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-server-names",
			"TLS server-name type appears more than once", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Name.Name.Data = Cursor.Data.Data + Cursor.Offset + 3u;
	Name.Name.Size = iNameSize;
	Cursor.Seen[Name.Type >> 6u] |= iBit;
	Cursor.Offset += 3u + iNameSize;
	*pCursor = Cursor;
	*pName = Name;
	return XTLS_ITEM_VALUE;
}



/* 严格解析 SNI 名称列表。 */
XRT_API bool xrtTlsServerNames(
	xbytesview Data,
	xtlsservernamecursor* pCursor
)
{
	xtlsservernamecursor Cursor;
	xtlsservername Name;
	xtlsitemresult Result;
	size_t iSize;

	if ( (pCursor == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-server-names",
			"TLS server-name input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 6u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-server-names",
			"TLS server-name list is empty or truncated", Data.Size
		);
	}
	iSize = __xrtTlsRead16(Data.Data);
	if ( iSize != Data.Size - 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-server-names",
			"TLS server-name list length is inconsistent", 0
		);
	}
	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Data.Data = Data.Data + 2u;
	Cursor.Data.Size = iSize;
	do {
		Result = xrtTlsServerNamesRead(&Cursor, &Name);
	} while ( Result == XTLS_ITEM_VALUE );
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	Cursor.Offset = 0;
	memset(Cursor.Seen, 0, sizeof(Cursor.Seen));
	*pCursor = Cursor;
	return true;
}



/* 查找 SNI host_name。 */
XRT_API xtlsitemresult xrtTlsHostName(
	xbytesview Data,
	xbytesview* pHost
)
{
	xtlsservernamecursor Cursor;
	xtlsservername Name;
	xtlsitemresult Result;
	xbytesview Host;
	bool bFound = false;

	if ( pHost == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "find-host-name",
			"TLS host-name output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( !xrtTlsServerNames(Data, &Cursor) ) {
		return XTLS_ITEM_ERROR;
	}
	while ( (Result = xrtTlsServerNamesRead(
		&Cursor, &Name
	)) == XTLS_ITEM_VALUE ) {
		if ( Name.Type == 0 ) {
			Host = Name.Name;
			bFound = true;
		}
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return Result;
	}
	if ( !bFound ) {
		return XTLS_ITEM_DONE;
	}
	*pHost = Host;
	return XTLS_ITEM_VALUE;
}



/* 读取下一项 ALPN 协议名称。 */
XRT_API xtlsitemresult xrtTlsProtocolsRead(
	xtlsprotocolcursor* pCursor,
	xbytesview* pProtocol
)
{
	xtlsprotocolcursor Cursor;
	xbytesview Protocol;
	size_t iRemaining;
	size_t iSize;

	if ( (pCursor == NULL) || (pProtocol == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) && (pCursor->Offset > pCursor->Data.Size)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-protocols",
			"TLS ALPN cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	iRemaining = Cursor.Data.Size - Cursor.Offset;
	iSize = Cursor.Data.Data[Cursor.Offset];
	if ( (iSize == 0) || (iSize > iRemaining - 1u) ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-protocols",
			"TLS ALPN protocol name is empty or truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Protocol.Data = Cursor.Data.Data + Cursor.Offset + 1u;
	Protocol.Size = iSize;
	Cursor.Offset += 1u + iSize;
	*pCursor = Cursor;
	*pProtocol = Protocol;
	return XTLS_ITEM_VALUE;
}



/* 严格解析 ALPN ProtocolNameList。 */
XRT_API bool xrtTlsProtocols(
	xbytesview Data,
	xtlsprotocolcursor* pCursor
)
{
	xtlsprotocolcursor Cursor;
	xbytesview Protocol;
	xtlsitemresult Result;
	size_t iSize;

	if ( (pCursor == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-protocols",
			"TLS ALPN input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 4u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-protocols",
			"TLS ALPN list is empty or truncated", Data.Size
		);
	}
	iSize = __xrtTlsRead16(Data.Data);
	if ( iSize != Data.Size - 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-protocols",
			"TLS ALPN list length is inconsistent", 0
		);
	}
	Cursor.Data.Data = Data.Data + 2u;
	Cursor.Data.Size = iSize;
	Cursor.Offset = 0;
	do {
		Result = xrtTlsProtocolsRead(&Cursor, &Protocol);
	} while ( Result == XTLS_ITEM_VALUE );
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	Cursor.Offset = 0;
	*pCursor = Cursor;
	return true;
}



/* 严格读取服务端唯一选择的 ALPN 协议。 */
XRT_API bool xrtTlsProtocolSelected(
	xbytesview Data,
	xbytesview* pProtocol
)
{
	xtlsprotocolcursor Cursor;
	xbytesview Protocol;
	xbytesview Extra;

	if ( pProtocol == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "selected-protocol",
			"TLS ALPN selected-protocol output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !xrtTlsProtocols(Data, &Cursor) ||
		(xrtTlsProtocolsRead(&Cursor, &Protocol) != XTLS_ITEM_VALUE) ) {
		return false;
	}
	if ( xrtTlsProtocolsRead(&Cursor, &Extra) != XTLS_ITEM_DONE ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "selected-protocol",
			"TLS server selected more than one ALPN protocol", Cursor.Offset
		);
	}
	*pProtocol = Protocol;
	return true;
}



/* 在完整 ALPN 列表中查找一个不透明协议名称。 */
XRT_API xtlsitemresult xrtTlsProtocolFind(
	xbytesview Data,
	xbytesview Protocol
)
{
	xtlsprotocolcursor Cursor;
	xbytesview Current;
	xtlsitemresult Result;

	if ( !__xrtTlsViewValid(Protocol) || (Protocol.Size == 0) ||
		(Protocol.Size > UINT8_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "find-protocol",
			"TLS ALPN protocol name is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( !xrtTlsProtocols(Data, &Cursor) ) {
		return XTLS_ITEM_ERROR;
	}
	while ( (Result = xrtTlsProtocolsRead(
		&Cursor, &Current
	)) == XTLS_ITEM_VALUE ) {
		if ( (Current.Size == Protocol.Size) &&
			(memcmp(Current.Data, Protocol.Data, Protocol.Size) == 0) ) {
			return XTLS_ITEM_VALUE;
		}
	}
	return Result;
}



/* 按服务端偏好顺序选择双方 ALPN 列表的第一个交集。 */
XRT_API xtlsitemresult xrtTlsProtocolSelect(
	xbytesview Offered,
	xbytesview Preferred,
	xbytesview* pProtocol
)
{
	xtlsprotocolcursor OfferedCursor;
	xtlsprotocolcursor PreferredCursor;
	xbytesview Protocol;
	xtlsitemresult Result;

	if ( pProtocol == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "select-protocol",
			"TLS ALPN selection output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( !xrtTlsProtocols(Offered, &OfferedCursor) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !xrtTlsProtocols(Preferred, &PreferredCursor) ) {
		return XTLS_ITEM_ERROR;
	}
	while ( (Result = xrtTlsProtocolsRead(
		&PreferredCursor, &Protocol
	)) == XTLS_ITEM_VALUE ) {
		xtlsitemresult Match = xrtTlsProtocolFind(Offered, Protocol);

		if ( Match == XTLS_ITEM_ERROR ) {
			return Match;
		}
		if ( Match == XTLS_ITEM_VALUE ) {
			*pProtocol = Protocol;
			return XTLS_ITEM_VALUE;
		}
	}
	return Result;
}



/* 检查密钥共享组是否已经出现在此前消费的区域。 */
static bool __xrtTlsKeyShareSeen(
	const xtlskeysharecursor* pCursor,
	uint16 iGroup
)
{
	size_t iOffset = 0;
	uint8 iBucket = (uint8)iGroup;
	uint64 iBit = __xrtTlsHelloSeenBit(iGroup);

	if ( (pCursor->Seen[iBucket >> 6u] & iBit) == 0 ) {
		return false;
	}
	while ( iOffset < pCursor->Offset ) {
		size_t iRemaining = pCursor->Offset - iOffset;
		size_t iKeySize;

		if ( iRemaining < 4u ) {
			return false;
		}
		iKeySize = __xrtTlsRead16(
			pCursor->Data.Data + iOffset + 2u
		);
		if ( iKeySize > iRemaining - 4u ) {
			return false;
		}

		if ( __xrtTlsRead16(pCursor->Data.Data + iOffset) == iGroup ) {
			return true;
		}
		iOffset += 4u + iKeySize;
	}
	return false;
}



/* 读取下一项客户端密钥共享。 */
XRT_API xtlsitemresult xrtTlsKeySharesRead(
	xtlskeysharecursor* pCursor,
	xtlskeyshare* pShare
)
{
	xtlskeysharecursor Cursor;
	xtlskeyshare Share;
	size_t iRemaining;
	size_t iKeySize;
	uint8 iBucket;

	if ( (pCursor == NULL) || (pShare == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) && (pCursor->Offset > pCursor->Data.Size)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-key-shares",
			"TLS key-share cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	iRemaining = Cursor.Data.Size - Cursor.Offset;
	if ( iRemaining < 4u ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-key-shares",
			"TLS key-share entry is truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Share.Group = __xrtTlsRead16(Cursor.Data.Data + Cursor.Offset);
	iKeySize = __xrtTlsRead16(Cursor.Data.Data + Cursor.Offset + 2u);
	if ( (iKeySize == 0) || (iKeySize > iRemaining - 4u) ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-key-shares",
			"TLS key-share key length is invalid", Cursor.Offset + 2u
		);
		return XTLS_ITEM_ERROR;
	}
	if ( __xrtTlsKeyShareSeen(&Cursor, Share.Group) ) {
		__xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "read-key-shares",
			"TLS key-share group appears more than once", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Share.Key.Data = Cursor.Data.Data + Cursor.Offset + 4u;
	Share.Key.Size = iKeySize;
	iBucket = (uint8)Share.Group;
	Cursor.Seen[iBucket >> 6u] |= __xrtTlsHelloSeenBit(Share.Group);
	Cursor.Offset += 4u + iKeySize;
	*pCursor = Cursor;
	*pShare = Share;
	return XTLS_ITEM_VALUE;
}



/* 严格解析 ClientHello key_share 列表。 */
XRT_API bool xrtTlsClientKeyShares(
	xbytesview Data,
	xtlskeysharecursor* pCursor
)
{
	xtlskeysharecursor Cursor;
	xtlskeyshare Share;
	xtlsitemresult Result;
	size_t iSize;

	if ( (pCursor == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-client-key-shares",
			"TLS key-share input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-client-key-shares",
			"TLS key-share list is truncated", Data.Size
		);
	}
	iSize = __xrtTlsRead16(Data.Data);
	if ( iSize != Data.Size - 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-client-key-shares",
			"TLS key-share list length is inconsistent", 0
		);
	}
	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Data.Data = Data.Data + 2u;
	Cursor.Data.Size = iSize;
	do {
		Result = xrtTlsKeySharesRead(&Cursor, &Share);
	} while ( Result == XTLS_ITEM_VALUE );
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	Cursor.Offset = 0;
	memset(Cursor.Seen, 0, sizeof(Cursor.Seen));
	*pCursor = Cursor;
	return true;
}



/* 严格解析普通 ServerHello 中唯一的 key_share。 */
XRT_API bool xrtTlsServerKeyShare(
	xbytesview Data,
	xtlskeyshare* pShare
)
{
	xtlskeyshare Share;
	size_t iKeySize;

	if ( (pShare == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-server-key-share",
			"TLS server key-share input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 5u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-server-key-share",
			"TLS server key-share is empty or truncated", Data.Size
		);
	}
	iKeySize = __xrtTlsRead16(Data.Data + 2u);
	if ( iKeySize != Data.Size - 4u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-server-key-share",
			"TLS server key-share length is inconsistent", 2
		);
	}
	Share.Group = __xrtTlsRead16(Data.Data);
	Share.Key.Data = Data.Data + 4u;
	Share.Key.Size = iKeySize;
	*pShare = Share;
	return true;
}



/* 严格解析 HelloRetryRequest 选择的命名组。 */
XRT_API bool xrtTlsRetryGroup(xbytesview Data, uint16* pGroup)
{
	if ( (pGroup == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-retry-group",
			"TLS retry-group input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size != 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-retry-group",
			"TLS retry key-share must contain exactly one group", 0
		);
	}
	*pGroup = __xrtTlsRead16(Data.Data);
	return true;
}



/* 严格解析 HelloRetryRequest cookie 的 16 位非空字节向量。 */
XRT_API bool xrtTlsRetryCookie(xbytesview Data, xbytesview* pCookie)
{
	xbytesview Cookie;

	if ( (pCookie == NULL) || !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-retry-cookie",
			"TLS retry-cookie input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Data.Size < 3u) ||
		((size_t)__xrtTlsRead16(Data.Data) != (Data.Size - 2u)) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, "parse-retry-cookie",
			"TLS retry cookie is empty or has an inconsistent length", 0
		);
	}
	Cookie.Data = Data.Data + 2u;
	Cookie.Size = Data.Size - 2u;
	*pCookie = Cookie;
	return true;
}



/* 验证 Hello 中当前公开支持的核心扩展语义。 */

#endif
