#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HELLO_WRITE)

/* 检查 ClientHello 视图是否可以无损编码。 */
static bool __xrtTlsClientHelloWritable(
	const xtlsclienthello* pHello
)
{
	bool bNullCompression = false;

	if ( (pHello == NULL) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->Random : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->SessionId : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->CipherSuites.Data : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->CompressionMethods : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->Extensions : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-hello",
			"TLS ClientHello view is invalid", SIZE_MAX
		);
		return false;
	}
	if ( pHello->LegacyVersion != XTLS_VERSION_12 ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_VERSION, "write-client-hello",
			"TLS ClientHello legacy_version must be 0x0303", 0
		);
	}
	if ( (pHello->Random.Size != XTLS_RANDOM_SIZE) ||
		(pHello->SessionId.Size > XTLS_SESSION_ID_MAX) ||
		(pHello->CipherSuites.Data.Size < 2u) ||
		(pHello->CipherSuites.Data.Size > UINT16_MAX) ||
		((pHello->CipherSuites.Data.Size & 1u) != 0) ||
		(pHello->CompressionMethods.Size == 0) ||
		(pHello->CompressionMethods.Size > UINT8_MAX) ||
		(pHello->Extensions.Size > UINT16_MAX) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "write-client-hello",
			"TLS ClientHello field length is not encodable", SIZE_MAX
		);
	}
	for ( size_t i = 0; i < pHello->CompressionMethods.Size; i++ ) {
		if ( pHello->CompressionMethods.Data[i] == 0 ) {
			bNullCompression = true;
		}
	}
	if ( !bNullCompression ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "write-client-hello",
			"TLS ClientHello must offer null compression", SIZE_MAX
		);
	}
	return __xrtTlsHelloExtensions(
		pHello->Extensions, false, false
	) && __xrtTlsClientCompressionValid(pHello);
}



/* 检查 ServerHello 视图是否可以无损编码。 */
static bool __xrtTlsServerHelloWritable(
	const xtlsserverhello* pHello
)
{
	if ( (pHello == NULL) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->Random : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->SessionId : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pHello != NULL ?
			pHello->Extensions : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-server-hello",
			"TLS ServerHello view is invalid", SIZE_MAX
		);
		return false;
	}
	if ( pHello->LegacyVersion != XTLS_VERSION_12 ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_VERSION, "write-server-hello",
			"TLS ServerHello legacy_version must be 0x0303", 0
		);
	}
	if ( (pHello->Random.Size != XTLS_RANDOM_SIZE) ||
		(pHello->SessionId.Size > XTLS_SESSION_ID_MAX) ||
		(pHello->CompressionMethod != 0) ||
		(pHello->Extensions.Size > UINT16_MAX) ||
		(pHello->Retry != __xrtTlsHelloRetry(pHello->Random)) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "write-server-hello",
			"TLS ServerHello field value is not encodable", SIZE_MAX
		);
	}
	return __xrtTlsHelloExtensions(
		pHello->Extensions, true, pHello->Retry
	);
}



/* 返回编码 ClientHello 正文所需的精确长度。 */
XRT_API size_t xrtTlsClientHelloSize(const xtlsclienthello* pHello)
{
	size_t iSize;

	if ( !__xrtTlsClientHelloWritable(pHello) ) {
		return 0;
	}
	iSize = 2u + XTLS_RANDOM_SIZE + 1u + pHello->SessionId.Size +
		2u + pHello->CipherSuites.Data.Size +
		1u + pHello->CompressionMethods.Size;
	if ( pHello->Extensions.Size != 0 ) {
		iSize += 2u + pHello->Extensions.Size;
	}
	return iSize;
}



/* 失败原子地编码 ClientHello 正文。 */
XRT_API bool xrtTlsClientHelloEncode(
	const xtlsclienthello* pHello,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsClientHelloSize(pHello);
	size_t iOffset = 0;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-hello",
			"TLS ClientHello output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE, "write-client-hello",
			"TLS ClientHello output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->Random
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->SessionId
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->CipherSuites.Data
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->CompressionMethods
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->Extensions
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-client-hello",
			"TLS ClientHello input overlaps its output", SIZE_MAX
		);
		return false;
	}

	__xrtTlsWrite16(pWrite + iOffset, pHello->LegacyVersion);
	iOffset += 2u;
	memcpy(pWrite + iOffset, pHello->Random.Data, XTLS_RANDOM_SIZE);
	iOffset += XTLS_RANDOM_SIZE;
	pWrite[iOffset++] = (uint8)pHello->SessionId.Size;
	if ( pHello->SessionId.Size != 0 ) {
		memcpy(
			pWrite + iOffset,
			pHello->SessionId.Data,
			pHello->SessionId.Size
		);
		iOffset += pHello->SessionId.Size;
	}
	__xrtTlsWrite16(
		pWrite + iOffset, (uint16)pHello->CipherSuites.Data.Size
	);
	iOffset += 2u;
	memcpy(
		pWrite + iOffset,
		pHello->CipherSuites.Data.Data,
		pHello->CipherSuites.Data.Size
	);
	iOffset += pHello->CipherSuites.Data.Size;
	pWrite[iOffset++] = (uint8)pHello->CompressionMethods.Size;
	memcpy(
		pWrite + iOffset,
		pHello->CompressionMethods.Data,
		pHello->CompressionMethods.Size
	);
	iOffset += pHello->CompressionMethods.Size;
	if ( pHello->Extensions.Size != 0 ) {
		__xrtTlsWrite16(
			pWrite + iOffset, (uint16)pHello->Extensions.Size
		);
		iOffset += 2u;
		memcpy(
			pWrite + iOffset,
			pHello->Extensions.Data,
			pHello->Extensions.Size
		);
	}
	return true;
}



/* 返回编码 ServerHello 正文所需的精确长度。 */
XRT_API size_t xrtTlsServerHelloSize(const xtlsserverhello* pHello)
{
	size_t iSize;

	if ( !__xrtTlsServerHelloWritable(pHello) ) {
		return 0;
	}
	iSize = 2u + XTLS_RANDOM_SIZE + 1u + pHello->SessionId.Size + 3u;
	if ( pHello->Extensions.Size != 0 ) {
		iSize += 2u + pHello->Extensions.Size;
	}
	return iSize;
}



/* 失败原子地编码 ServerHello 或 HelloRetryRequest 正文。 */
XRT_API bool xrtTlsServerHelloEncode(
	const xtlsserverhello* pHello,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsServerHelloSize(pHello);
	size_t iOffset = 0;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-server-hello",
			"TLS ServerHello output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE, "write-server-hello",
			"TLS ServerHello output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->Random
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->SessionId
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, pHello->Extensions
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-server-hello",
			"TLS ServerHello input overlaps its output", SIZE_MAX
		);
		return false;
	}

	__xrtTlsWrite16(pWrite + iOffset, pHello->LegacyVersion);
	iOffset += 2u;
	memcpy(pWrite + iOffset, pHello->Random.Data, XTLS_RANDOM_SIZE);
	iOffset += XTLS_RANDOM_SIZE;
	pWrite[iOffset++] = (uint8)pHello->SessionId.Size;
	if ( pHello->SessionId.Size != 0 ) {
		memcpy(
			pWrite + iOffset,
			pHello->SessionId.Data,
			pHello->SessionId.Size
		);
		iOffset += pHello->SessionId.Size;
	}
	__xrtTlsWrite16(pWrite + iOffset, pHello->CipherSuite);
	iOffset += 2u;
	pWrite[iOffset++] = pHello->CompressionMethod;
	if ( pHello->Extensions.Size != 0 ) {
		__xrtTlsWrite16(
			pWrite + iOffset, (uint16)pHello->Extensions.Size
		);
		iOffset += 2u;
		memcpy(
			pWrite + iOffset,
			pHello->Extensions.Data,
			pHello->Extensions.Size
		);
	}
	return true;
}

#endif
