#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HELLO_WRITE)

/* 检查 writer 结构与调用方缓冲是否一致。 */
static bool __xrtTlsWriterValid(const xtlswriter* pWriter)
{
	return (pWriter != NULL) &&
		((pWriter->Data != NULL) || (pWriter->Capacity == 0)) &&
		(pWriter->Size <= pWriter->Capacity);
}



/* 检查新扩展类型是否尚未出现在 writer 中。 */
static bool __xrtTlsWriterTypeFree(
	const xtlswriter* pWriter,
	xtlsextensiontype Type
)
{
	xtlsextension Existing;
	xtlsitemresult Result;

	if ( (uint32)Type > UINT16_MAX ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_EXTENSION, "write-extension",
			"TLS extension type does not fit the wire format", SIZE_MAX
		);
		return false;
	}
	Result = xrtTlsExtensionsFind(
		(xbytesview) { pWriter->Data, pWriter->Size }, Type, &Existing
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		__xrtTlsError(
			XERR_EXISTS, XTLS_ERROR_EXTENSION, "write-extension",
			"TLS extension type already exists in the writer", pWriter->Size
		);
		return false;
	}
	return true;
}



/* 预检一次完整扩展追加并返回负载写入位置。 */
static bool __xrtTlsWriterBegin(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	size_t iDataSize,
	uint8** ppData
)
{
	size_t iSize;

	if ( !__xrtTlsWriterValid(pWriter) || (ppData == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-extension",
			"TLS writer or output position is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterTypeFree(pWriter, Type) ) {
		return false;
	}
	iSize = xrtTlsExtensionSize(iDataSize);
	if ( iSize == 0 ) {
		return false;
	}
	if ( iSize > pWriter->Capacity - pWriter->Size ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "write-extension",
			"TLS writer capacity is too small", pWriter->Size
		);
		return false;
	}
	*ppData = pWriter->Data + pWriter->Size + XTLS_EXTENSION_HEADER_SIZE;
	return true;
}



/* 在负载完成后提交扩展头和 writer 长度。 */
static void __xrtTlsWriterCommit(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	size_t iDataSize
)
{
	uint8* pHeader = pWriter->Data + pWriter->Size;

	__xrtTlsWrite16(pHeader, (uint16)Type);
	__xrtTlsWrite16(pHeader + 2u, (uint16)iDataSize);
	pWriter->Size += XTLS_EXTENSION_HEADER_SIZE + iDataSize;
}



/* 判断输入区域是否与本次尚未写入的目标区域重叠。 */
static bool __xrtTlsWriterOverlap(
	const xtlswriter* pWriter,
	size_t iWriteSize,
	const void* pInput,
	size_t iInputSize
)
{
	uintptr_t iOutput;
	uintptr_t iInput;

	if ( iInputSize == 0 ) {
		return false;
	}
	iOutput = (uintptr_t)(pWriter->Data + pWriter->Size);
	iInput = (uintptr_t)pInput;
	if ( (iOutput > UINTPTR_MAX - iWriteSize) ||
		(iInput > UINTPTR_MAX - iInputSize) ) {
		return true;
	}
	return (iOutput < iInput + iInputSize) &&
		(iInput < iOutput + iWriteSize);
}



/* 检查调用方 16 位标识数组是否存在重复。 */
static bool __xrtTlsWriterIdsUnique(
	const uint16* pValues,
	size_t iCount,
	cstr sOperation
)
{
	uint64 Seen[4] = { 0, 0, 0, 0 };

	for ( size_t i = 0; i < iCount; i++ ) {
		uint16 iValue = pValues[i];
		uint8 iBucket = (uint8)iValue;
		uint64 iBit = __xrtTlsHelloSeenBit(iValue);

		if ( (Seen[iBucket >> 6u] & iBit) != 0 ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( pValues[j] == iValue ) {
					return __xrtTlsHelloError(
						XTLS_ERROR_EXTENSION, sOperation,
						"TLS writer list contains a duplicate", i
					);
				}
			}
		}
		Seen[iBucket >> 6u] |= iBit;
	}
	return true;
}



/* 初始化一个空 writer。 */
XRT_API bool xrtTlsWriterInit(
	xtlswriter* pWriter,
	void* pData,
	size_t iCapacity
)
{
	xtlswriter Writer;

	if ( (pWriter == NULL) || ((pData == NULL) && (iCapacity != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "init-tls-writer",
			"TLS writer or buffer is invalid", SIZE_MAX
		);
		return false;
	}
	Writer.Data = (bytes)pData;
	Writer.Capacity = iCapacity;
	Writer.Size = 0;
	*pWriter = Writer;
	return true;
}



/* 清空 writer 的逻辑内容。 */
XRT_API bool xrtTlsWriterReset(xtlswriter* pWriter)
{
	if ( !__xrtTlsWriterValid(pWriter) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "reset-tls-writer",
			"TLS writer is invalid", SIZE_MAX
		);
		return false;
	}
	pWriter->Size = 0;
	return true;
}



/* 返回 writer 已完成区域。 */
XRT_API xbytesview xrtTlsWriterData(const xtlswriter* pWriter)
{
	xbytesview Data = { NULL, 0 };

	if ( !__xrtTlsWriterValid(pWriter) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "get-tls-writer-data",
			"TLS writer is invalid", SIZE_MAX
		);
		return Data;
	}
	Data.Data = pWriter->Data;
	Data.Size = pWriter->Size;
	return Data;
}



/* 失败原子地追加一个原始扩展。 */
XRT_API bool xrtTlsWriterExtension(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	xbytesview Data
)
{
	uint8* pWrite;

	if ( !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-extension",
			"TLS extension data is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(pWriter, Type, Data.Size, &pWrite) ) {
		return false;
	}
	if ( Data.Size != 0 ) {
		memmove(pWrite, Data.Data, Data.Size);
	}
	__xrtTlsWriterCommit(pWriter, Type, Data.Size);
	return true;
}



/* 追加只包含一个 host_name 的 SNI 扩展。 */
XRT_API bool xrtTlsWriterHostName(
	xtlswriter* pWriter,
	xbytesview Host
)
{
	uint8* pWrite;
	size_t iDataSize;
	size_t iWriteSize;

	if ( !__xrtTlsViewValid(Host) || (Host.Size == 0) ||
		(Host.Size > XTLS_EXTENSION_DATA_MAX - 5u) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_EXTENSION, "write-host-name",
			"TLS SNI host name is empty or too long", SIZE_MAX
		);
		return false;
	}
	iDataSize = 5u + Host.Size;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( !__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
		pWriter, iWriteSize, Host.Data, Host.Size
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-host-name",
			"TLS SNI input overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_SERVER_NAME, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)(3u + Host.Size));
	pWrite[2] = 0;
	__xrtTlsWrite16(pWrite + 3u, (uint16)Host.Size);
	memcpy(pWrite + 5u, Host.Data, Host.Size);
	__xrtTlsWriterCommit(
		pWriter, XTLS_EXTENSION_SERVER_NAME, iDataSize
	);
	return true;
}



/* 追加完整 ALPN 协议列表。 */
XRT_API bool xrtTlsWriterProtocols(
	xtlswriter* pWriter,
	const xbytesview* pProtocols,
	size_t iCount
)
{
	uint8* pWrite;
	size_t iListSize = 0;
	size_t iDataSize;
	size_t iWriteSize;
	size_t iOffset = 2u;

	if ( (pProtocols == NULL) || (iCount == 0) ||
		(iCount > (XTLS_EXTENSION_DATA_MAX - 2u) / 2u) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-protocols",
			"TLS ALPN protocol array is empty or invalid", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtTlsViewValid(pProtocols[i]) ||
			(pProtocols[i].Size == 0) ||
			(pProtocols[i].Size > UINT8_MAX) ||
			(iListSize > XTLS_EXTENSION_DATA_MAX - 3u -
				pProtocols[i].Size) ) {
			__xrtTlsError(
				XERR_RANGE, XTLS_ERROR_EXTENSION, "write-protocols",
				"TLS ALPN protocol list is invalid or too long", i
			);
			return false;
		}
		iListSize += 1u + pProtocols[i].Size;
	}
	iDataSize = 2u + iListSize;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( !__xrtTlsWriterValid(pWriter) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-protocols",
			"TLS writer is invalid", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pProtocols,
		iCount * sizeof(xbytesview)
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-protocols",
			"TLS ALPN view array overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtTlsWriterOverlap(
			pWriter, iWriteSize, pProtocols[i].Data, pProtocols[i].Size
		) ) {
			__xrtTlsError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-protocols",
				"TLS ALPN input overlaps its writer destination", i
			);
			return false;
		}
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_ALPN, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)iListSize);
	for ( size_t i = 0; i < iCount; i++ ) {
		pWrite[iOffset++] = (uint8)pProtocols[i].Size;
		memcpy(pWrite + iOffset, pProtocols[i].Data, pProtocols[i].Size);
		iOffset += pProtocols[i].Size;
	}
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_ALPN, iDataSize);
	return true;
}



#if defined(XRT_FEATURE_TLS_PSK_WRITE)

/* 追加非空且不重复的 PSK 密钥交换模式列表。 */
XRT_API bool xrtTlsWriterPskModes(
	xtlswriter* pWriter,
	const uint8* pModes,
	size_t iCount
)
{
	uint64 Seen[4] = { 0, 0, 0, 0 };
	uint8* pWrite;

	if ( (pModes == NULL) || (iCount == 0) || (iCount > UINT8_MAX) ||
		!__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
			pWriter, XTLS_EXTENSION_HEADER_SIZE + 1u + iCount,
			pModes, iCount
		) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-psk-modes", "TLS PSK mode array or writer is invalid",
			SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		uint8 iMode = pModes[i];
		uint64 iBit = UINT64_C(1) << (iMode & 63u);

		if ( (Seen[iMode >> 6u] & iBit) != 0 ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_EXTENSION, "write-psk-modes",
				"TLS PSK mode appears more than once", i
			);
		}
		Seen[iMode >> 6u] |= iBit;
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES,
		1u + iCount, &pWrite
	) ) {
		return false;
	}
	pWrite[0] = (uint8)iCount;
	memcpy(pWrite + 1u, pModes, iCount);
	__xrtTlsWriterCommit(
		pWriter, XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES, 1u + iCount
	);
	return true;
}



/* 追加等量 PSK identity 与 binder 列表。 */
XRT_API bool xrtTlsWriterClientPsks(
	xtlswriter* pWriter,
	const xtlspsk* pPsks,
	size_t iCount
)
{
	size_t iIdentities = 0;
	size_t iBinders = 0;
	size_t iDataSize;
	size_t iWriteSize;
	size_t iOffset;
	uint8* pWrite;

	if ( (pPsks == NULL) || (iCount == 0) ||
		(iCount > ((UINT16_MAX - 4u) / 40u)) ||
		!__xrtTlsWriterValid(pWriter) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-client-psks", "TLS PSK array or writer is invalid", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtTlsViewValid(pPsks[i].Identity) ||
			!__xrtTlsViewValid(pPsks[i].Binder) ||
			(pPsks[i].Identity.Size == 0) ||
			(pPsks[i].Identity.Size > UINT16_MAX) ||
			(pPsks[i].Binder.Size < 32u) ||
			(pPsks[i].Binder.Size > UINT8_MAX) ||
			(iIdentities > UINT16_MAX - 6u - pPsks[i].Identity.Size) ||
			(iBinders > UINT16_MAX - 1u - pPsks[i].Binder.Size) ) {
			__xrtTlsError(
				XERR_RANGE, XTLS_ERROR_EXTENSION,
				"write-client-psks", "TLS PSK entry is invalid or too long", i
			);
			return false;
		}
		iIdentities += 6u + pPsks[i].Identity.Size;
		iBinders += 1u + pPsks[i].Binder.Size;
	}
	iDataSize = 4u + iIdentities + iBinders;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( iDataSize > UINT16_MAX ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION,
			"write-client-psks", "TLS PSK extension is too long", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pPsks, iCount * sizeof(xtlspsk)
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-client-psks", "TLS PSK array overlaps its writer destination",
			SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtTlsWriterOverlap(
			pWriter, iWriteSize,
			pPsks[i].Identity.Data, pPsks[i].Identity.Size
		) || __xrtTlsWriterOverlap(
			pWriter, iWriteSize,
			pPsks[i].Binder.Data, pPsks[i].Binder.Size
		) ) {
			__xrtTlsError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"write-client-psks", "TLS PSK input overlaps its writer destination", i
			);
			return false;
		}
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_PRE_SHARED_KEY, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)iIdentities);
	iOffset = 2u;
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite16(pWrite + iOffset, (uint16)pPsks[i].Identity.Size);
		memcpy(
			pWrite + iOffset + 2u,
			pPsks[i].Identity.Data, pPsks[i].Identity.Size
		);
		__xrtTlsWrite32(
			pWrite + iOffset + 2u + pPsks[i].Identity.Size,
			pPsks[i].ObfuscatedAge
		);
		iOffset += 6u + pPsks[i].Identity.Size;
	}
	__xrtTlsWrite16(pWrite + iOffset, (uint16)iBinders);
	iOffset += 2u;
	for ( size_t i = 0; i < iCount; i++ ) {
		pWrite[iOffset++] = (uint8)pPsks[i].Binder.Size;
		memcpy(pWrite + iOffset, pPsks[i].Binder.Data, pPsks[i].Binder.Size);
		iOffset += pPsks[i].Binder.Size;
	}
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_PRE_SHARED_KEY, iDataSize);
	return true;
}



/* 追加 ServerHello 的单一 selected_identity。 */
XRT_API bool xrtTlsWriterServerPsk(
	xtlswriter* pWriter,
	uint16 iSelected
)
{
	uint8* pWrite;

	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_PRE_SHARED_KEY, 2u, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, iSelected);
	__xrtTlsWriterCommit(
		pWriter, XTLS_EXTENSION_PRE_SHARED_KEY, 2u
	);
	return true;
}

#endif



/* 追加带 16 位长度前缀的标识列表扩展。 */
XRT_API bool xrtTlsWriterIds(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	const uint16* pValues,
	size_t iCount
)
{
	uint8* pWrite;
	size_t iListSize;
	size_t iDataSize;
	size_t iWriteSize;

	if ( (pValues == NULL) || (iCount == 0) ||
		(iCount > (XTLS_EXTENSION_DATA_MAX - 2u) / 2u) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-tls-ids",
			"TLS identifier array is empty, invalid or too long", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterIdsUnique(
		pValues, iCount, "write-tls-ids"
	) ) {
		return false;
	}
	iListSize = iCount * 2u;
	iDataSize = 2u + iListSize;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( !__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pValues, iListSize
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-tls-ids",
			"TLS identifier input overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(pWriter, Type, iDataSize, &pWrite) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)iListSize);
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite16(pWrite + 2u + (i * 2u), pValues[i]);
	}
	__xrtTlsWriterCommit(pWriter, Type, iDataSize);
	return true;
}



/* 追加 ClientHello supported_versions 扩展。 */
XRT_API bool xrtTlsWriterClientVersions(
	xtlswriter* pWriter,
	const uint16* pVersions,
	size_t iCount
)
{
	uint8* pWrite;
	size_t iListSize;
	size_t iDataSize;
	size_t iWriteSize;

	if ( (pVersions == NULL) || (iCount == 0) ||
		(iCount > UINT8_MAX / 2u) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-versions",
			"TLS client version array is empty, invalid or too long", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterIdsUnique(
		pVersions, iCount, "write-client-versions"
	) ) {
		return false;
	}
	iListSize = iCount * 2u;
	iDataSize = 1u + iListSize;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( !__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pVersions, iListSize
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-versions",
			"TLS version input overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_SUPPORTED_VERSIONS, iDataSize, &pWrite
	) ) {
		return false;
	}
	pWrite[0] = (uint8)iListSize;
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite16(pWrite + 1u + (i * 2u), pVersions[i]);
	}
	__xrtTlsWriterCommit(
		pWriter, XTLS_EXTENSION_SUPPORTED_VERSIONS, iDataSize
	);
	return true;
}



/* 追加 ServerHello 选择单一版本的 supported_versions 扩展。 */
XRT_API bool xrtTlsWriterServerVersion(
	xtlswriter* pWriter,
	uint16 iVersion
)
{
	uint8* pWrite;

	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_SUPPORTED_VERSIONS, 2u, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, iVersion);
	__xrtTlsWriterCommit(
		pWriter, XTLS_EXTENSION_SUPPORTED_VERSIONS, 2u
	);
	return true;
}



/* 追加 ClientHello key_share 扩展。 */
XRT_API bool xrtTlsWriterClientKeyShares(
	xtlswriter* pWriter,
	const xtlskeyshare* pShares,
	size_t iCount
)
{
	uint8* pWrite;
	size_t iListSize = 0;
	size_t iDataSize;
	size_t iWriteSize;
	size_t iOffset = 2u;
	uint64 Seen[4] = { 0, 0, 0, 0 };

	if ( ((pShares == NULL) && (iCount != 0)) ||
		(iCount > (XTLS_EXTENSION_DATA_MAX - 2u) / 5u) ||
		!__xrtTlsWriterValid(pWriter) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-key-shares",
			"TLS key-share array or writer is invalid", SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		uint8 iBucket = (uint8)pShares[i].Group;
		uint64 iBit = __xrtTlsHelloSeenBit(pShares[i].Group);

		if ( !__xrtTlsViewValid(pShares[i].Key) ||
			(pShares[i].Key.Size == 0) ||
			(pShares[i].Key.Size > XTLS_EXTENSION_DATA_MAX - 6u) ||
			(iListSize > XTLS_EXTENSION_DATA_MAX - 6u -
				pShares[i].Key.Size) ) {
			__xrtTlsError(
				XERR_RANGE, XTLS_ERROR_EXTENSION,
				"write-client-key-shares",
				"TLS client key share is invalid or too long", i
			);
			return false;
		}
		if ( (Seen[iBucket >> 6u] & iBit) != 0 ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( pShares[j].Group == pShares[i].Group ) {
					return __xrtTlsHelloError(
						XTLS_ERROR_EXTENSION,
						"write-client-key-shares",
						"TLS key-share group appears more than once", i
					);
				}
			}
		}
		Seen[iBucket >> 6u] |= iBit;
		iListSize += 4u + pShares[i].Key.Size;
	}
	iDataSize = 2u + iListSize;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( (iCount != 0) && __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pShares,
		iCount * sizeof(xtlskeyshare)
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-client-key-shares",
			"TLS key-share view array overlaps its writer destination",
			SIZE_MAX
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtTlsWriterOverlap(
			pWriter, iWriteSize,
			pShares[i].Key.Data, pShares[i].Key.Size
		) ) {
			__xrtTlsError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"write-client-key-shares",
				"TLS key-share input overlaps its writer destination", i
			);
			return false;
		}
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_KEY_SHARE, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)iListSize);
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite16(pWrite + iOffset, pShares[i].Group);
		__xrtTlsWrite16(
			pWrite + iOffset + 2u, (uint16)pShares[i].Key.Size
		);
		memcpy(
			pWrite + iOffset + 4u,
			pShares[i].Key.Data,
			pShares[i].Key.Size
		);
		iOffset += 4u + pShares[i].Key.Size;
	}
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_KEY_SHARE, iDataSize);
	return true;
}



/* 追加普通 ServerHello 的单个 key_share 扩展。 */
XRT_API bool xrtTlsWriterServerKeyShare(
	xtlswriter* pWriter,
	const xtlskeyshare* pShare
)
{
	uint8* pWrite;
	size_t iDataSize;
	size_t iWriteSize;

	if ( (pShare == NULL) || !__xrtTlsViewValid(
		pShare != NULL ? pShare->Key : (xbytesview) { NULL, 1u }
	) || ((pShare != NULL) && ((pShare->Key.Size == 0) ||
		(pShare->Key.Size > XTLS_EXTENSION_DATA_MAX - 4u))) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-server-key-share",
			"TLS server key share is empty, invalid or too long", SIZE_MAX
		);
		return false;
	}
	iDataSize = 4u + pShare->Key.Size;
	iWriteSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( !__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
		pWriter, iWriteSize, pShare->Key.Data, pShare->Key.Size
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-server-key-share",
			"TLS key-share input overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_KEY_SHARE, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, pShare->Group);
	__xrtTlsWrite16(pWrite + 2u, (uint16)pShare->Key.Size);
	memcpy(pWrite + 4u, pShare->Key.Data, pShare->Key.Size);
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_KEY_SHARE, iDataSize);
	return true;
}



/* 追加 HelloRetryRequest 选择组形式的 key_share。 */
XRT_API bool xrtTlsWriterRetryGroup(
	xtlswriter* pWriter,
	uint16 iGroup
)
{
	uint8* pWrite;

	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_KEY_SHARE, 2u, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, iGroup);
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_KEY_SHARE, 2u);
	return true;
}



/* 追加 HelloRetryRequest 或重试 ClientHello 的非空 cookie。 */
XRT_API bool xrtTlsWriterRetryCookie(
	xtlswriter* pWriter,
	xbytesview Cookie
)
{
	uint8* pWrite;
	size_t iDataSize;

	if ( !__xrtTlsViewValid(Cookie) || (Cookie.Size == 0) ||
		(Cookie.Size > XTLS_EXTENSION_DATA_MAX - 2u) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_EXTENSION, "write-retry-cookie",
			"TLS retry cookie is empty or too long", SIZE_MAX
		);
		return false;
	}
	iDataSize = 2u + Cookie.Size;
	if ( !__xrtTlsWriterValid(pWriter) || __xrtTlsWriterOverlap(
		pWriter, XTLS_EXTENSION_HEADER_SIZE + iDataSize,
		Cookie.Data, Cookie.Size
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-retry-cookie",
			"TLS retry cookie overlaps its writer destination", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsWriterBegin(
		pWriter, XTLS_EXTENSION_COOKIE, iDataSize, &pWrite
	) ) {
		return false;
	}
	__xrtTlsWrite16(pWrite, (uint16)Cookie.Size);
	memcpy(pWrite + 2u, Cookie.Data, Cookie.Size);
	__xrtTlsWriterCommit(pWriter, XTLS_EXTENSION_COOKIE, iDataSize);
	return true;
}

#endif
