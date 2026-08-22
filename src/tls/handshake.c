#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* 读取网络字节序 24 位整数。 */
uint32 __xrtTlsRead24(const uint8* pData)
{
	return ((uint32)pData[0] << 16u) |
		((uint32)pData[1] << 8u) |
		(uint32)pData[2];
}



/* 写入网络字节序 24 位整数。 */
void __xrtTlsWrite24(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)(iValue >> 16u);
	pData[1] = (uint8)(iValue >> 8u);
	pData[2] = (uint8)iValue;
}



/* 返回握手类型的稳定英文名称。 */
XRT_API cstr xrtTlsHandshakeName(xtlshandshaketype Type)
{
	switch ( Type ) {
		case XTLS_HANDSHAKE_HELLO_REQUEST: return "hello_request";
		case XTLS_HANDSHAKE_CLIENT_HELLO: return "client_hello";
		case XTLS_HANDSHAKE_SERVER_HELLO: return "server_hello";
		case XTLS_HANDSHAKE_NEW_SESSION_TICKET: return "new_session_ticket";
		case XTLS_HANDSHAKE_END_OF_EARLY_DATA: return "end_of_early_data";
		case XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS:
			return "encrypted_extensions";
		case XTLS_HANDSHAKE_CERTIFICATE: return "certificate";
		case XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE:
			return "server_key_exchange";
		case XTLS_HANDSHAKE_CERTIFICATE_REQUEST:
			return "certificate_request";
		case XTLS_HANDSHAKE_SERVER_HELLO_DONE: return "server_hello_done";
		case XTLS_HANDSHAKE_CERTIFICATE_VERIFY: return "certificate_verify";
		case XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE:
			return "client_key_exchange";
		case XTLS_HANDSHAKE_FINISHED: return "finished";
		case XTLS_HANDSHAKE_CERTIFICATE_STATUS: return "certificate_status";
		case XTLS_HANDSHAKE_SUPPLEMENTAL_DATA: return "supplemental_data";
		case XTLS_HANDSHAKE_KEY_UPDATE: return "key_update";
		case XTLS_HANDSHAKE_COMPRESSED_CERTIFICATE:
			return "compressed_certificate";
		case XTLS_HANDSHAKE_MESSAGE_HASH: return "message_hash";
		default: return "unknown_handshake";
	}
}



/* 返回扩展类型的稳定英文名称。 */
XRT_API cstr xrtTlsExtensionName(xtlsextensiontype Type)
{
	switch ( Type ) {
		case XTLS_EXTENSION_SERVER_NAME: return "server_name";
		case XTLS_EXTENSION_MAX_FRAGMENT_LENGTH: return "max_fragment_length";
		case XTLS_EXTENSION_STATUS_REQUEST: return "status_request";
		case XTLS_EXTENSION_SUPPORTED_GROUPS: return "supported_groups";
		case XTLS_EXTENSION_EC_POINT_FORMATS: return "ec_point_formats";
		case XTLS_EXTENSION_SIGNATURE_ALGORITHMS:
			return "signature_algorithms";
		case XTLS_EXTENSION_USE_SRTP: return "use_srtp";
		case XTLS_EXTENSION_HEARTBEAT: return "heartbeat";
		case XTLS_EXTENSION_ALPN: return "application_layer_protocol_negotiation";
		case XTLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP:
			return "signed_certificate_timestamp";
		case XTLS_EXTENSION_CLIENT_CERTIFICATE_TYPE:
			return "client_certificate_type";
		case XTLS_EXTENSION_SERVER_CERTIFICATE_TYPE:
			return "server_certificate_type";
		case XTLS_EXTENSION_PADDING: return "padding";
		case XTLS_EXTENSION_ENCRYPT_THEN_MAC: return "encrypt_then_mac";
		case XTLS_EXTENSION_EXTENDED_MASTER_SECRET:
			return "extended_master_secret";
		case XTLS_EXTENSION_COMPRESS_CERTIFICATE: return "compress_certificate";
		case XTLS_EXTENSION_RECORD_SIZE_LIMIT: return "record_size_limit";
		case XTLS_EXTENSION_SESSION_TICKET: return "session_ticket";
		case XTLS_EXTENSION_PRE_SHARED_KEY: return "pre_shared_key";
		case XTLS_EXTENSION_EARLY_DATA: return "early_data";
		case XTLS_EXTENSION_SUPPORTED_VERSIONS: return "supported_versions";
		case XTLS_EXTENSION_COOKIE: return "cookie";
		case XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES:
			return "psk_key_exchange_modes";
		case XTLS_EXTENSION_CERTIFICATE_AUTHORITIES:
			return "certificate_authorities";
		case XTLS_EXTENSION_OID_FILTERS: return "oid_filters";
		case XTLS_EXTENSION_POST_HANDSHAKE_AUTH: return "post_handshake_auth";
		case XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT:
			return "signature_algorithms_cert";
		case XTLS_EXTENSION_KEY_SHARE: return "key_share";
		case XTLS_EXTENSION_RENEGOTIATION_INFO: return "renegotiation_info";
		default: return "unknown_extension";
	}
}



/* 返回完整握手消息长度。 */
XRT_API size_t xrtTlsHandshakeSize(size_t iBodySize)
{
	if ( iBodySize > XTLS_HANDSHAKE_BODY_MAX ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE, "size-handshake",
			"TLS handshake body exceeds the wire limit", SIZE_MAX
		);
		return 0;
	}
	return XTLS_HANDSHAKE_HEADER_SIZE + iBodySize;
}



/* 分片感知地解析输入开头的一条握手消息。 */
XRT_API xtlsresult xrtTlsHandshakeParse(
	xbytesview Input,
	xtlshandshake* pHandshake,
	size_t* pRequired
)
{
	xtlshandshake Handshake;
	size_t iBodySize;
	size_t iEncodedSize;

	if ( pRequired != NULL ) {
		*pRequired = 0;
	}
	if ( (pHandshake == NULL) ||
		 ((Input.Data == NULL) && (Input.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-handshake",
			"TLS handshake input or output is invalid", SIZE_MAX
		);
		return XTLS_ERROR;
	}
	if ( Input.Size < XTLS_HANDSHAKE_HEADER_SIZE ) {
		if ( pRequired != NULL ) {
			*pRequired = XTLS_HANDSHAKE_HEADER_SIZE;
		}
		return XTLS_AGAIN;
	}

	iBodySize = __xrtTlsRead24(Input.Data + 1u);
	iEncodedSize = XTLS_HANDSHAKE_HEADER_SIZE + iBodySize;
	if ( pRequired != NULL ) {
		*pRequired = iEncodedSize;
	}
	if ( Input.Size < iEncodedSize ) {
		return XTLS_AGAIN;
	}

	memset(&Handshake, 0, sizeof(Handshake));
	Handshake.Type = (xtlshandshaketype)Input.Data[0];
	Handshake.Body.Data = Input.Data + XTLS_HANDSHAKE_HEADER_SIZE;
	Handshake.Body.Size = iBodySize;
	Handshake.EncodedSize = iEncodedSize;
	*pHandshake = Handshake;
	return XTLS_OK;
}



/* 把握手类型和正文编码到调用方缓冲。 */
XRT_API bool xrtTlsHandshakeEncode(
	xtlshandshaketype Type,
	xbytesview Body,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iEncodedSize;

	if ( (pOutput == NULL) ||
		 ((Body.Data == NULL) && (Body.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "encode-handshake",
			"TLS handshake input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (uint32)Type > UINT8_MAX ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_HANDSHAKE, "encode-handshake",
			"TLS handshake type does not fit the wire format", SIZE_MAX
		);
		return false;
	}
	iEncodedSize = xrtTlsHandshakeSize(Body.Size);
	if ( iEncodedSize == 0 ) {
		return false;
	}
	if ( iOutputSize < iEncodedSize ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE, "encode-handshake",
			"TLS handshake output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size != 0 ) {
		memmove(
			pWrite + XTLS_HANDSHAKE_HEADER_SIZE,
			Body.Data,
			Body.Size
		);
	}
	pWrite[0] = (uint8)Type;
	__xrtTlsWrite24(pWrite + 1u, (uint32)Body.Size);
	return true;
}



/* 返回 TLS 1.3 CertificateVerify 待签内容的精确长度。 */
XRT_API size_t xrtTls13CertificateVerifyContentSize(
	xtlsrole Signer,
	size_t iTranscriptHashSize
)
{
	static const char ServerContext[] =
		"TLS 1.3, server CertificateVerify";
	static const char ClientContext[] =
		"TLS 1.3, client CertificateVerify";
	size_t iContextSize;

	_Static_assert(
		sizeof(ServerContext) == sizeof(ClientContext),
		"TLS CertificateVerify context sizes must match"
	);
	if ( ((Signer != XTLS_CLIENT) && (Signer != XTLS_SERVER)) ||
		((iTranscriptHashSize != 32u) && (iTranscriptHashSize != 48u)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"size-tls13-certificate-verify-content",
			"TLS 1.3 CertificateVerify signer or transcript hash size is invalid",
			SIZE_MAX
		);
		return 0;
	}
	iContextSize = Signer == XTLS_SERVER ?
		sizeof(ServerContext) - 1u : sizeof(ClientContext) - 1u;
	return 65u + iContextSize + iTranscriptHashSize;
}



/* 编码由 64 个空格、角色上下文、零分隔符和 transcript 摘要组成的待签内容。 */
XRT_API bool xrtTls13CertificateVerifyContentEncode(
	xtlsrole Signer,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
)
{
	static const char ServerContext[] =
		"TLS 1.3, server CertificateVerify";
	static const char ClientContext[] =
		"TLS 1.3, client CertificateVerify";
	const char* sContext;
	size_t iContextSize;
	size_t iRequired = xrtTls13CertificateVerifyContentSize(
		Signer, TranscriptHash.Size
	);
	uint8* pWrite = (uint8*)pOutput;

	if ( (iRequired == 0) || (TranscriptHash.Data == NULL) ||
		(pOutput == NULL) ) {
		if ( iRequired != 0 ) {
			__xrtTlsError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
				"write-tls13-certificate-verify-content",
				"TLS 1.3 CertificateVerify transcript or output is invalid",
				SIZE_MAX
			);
		}
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_VERIFY,
			"write-tls13-certificate-verify-content",
			"TLS 1.3 CertificateVerify output buffer is too small",
			SIZE_MAX
		);
		return false;
	}
	sContext = Signer == XTLS_SERVER ? ServerContext : ClientContext;
	iContextSize = Signer == XTLS_SERVER ?
		sizeof(ServerContext) - 1u : sizeof(ClientContext) - 1u;
	memset(pWrite, 0x20, 64u);
	memcpy(pWrite + 64u, sContext, iContextSize);
	pWrite[64u + iContextSize] = 0;
	memmove(
		pWrite + 65u + iContextSize,
		TranscriptHash.Data, TranscriptHash.Size
	);
	return true;
}



/* 返回完整扩展长度。 */
XRT_API size_t xrtTlsExtensionSize(size_t iDataSize)
{
	if ( iDataSize > XTLS_EXTENSION_DATA_MAX ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "size-extension",
			"TLS extension data exceeds the wire limit", SIZE_MAX
		);
		return 0;
	}
	return XTLS_EXTENSION_HEADER_SIZE + iDataSize;
}



/* 分片感知地解析输入开头的一个扩展。 */
XRT_API xtlsresult xrtTlsExtensionParse(
	xbytesview Input,
	xtlsextension* pExtension,
	size_t* pRequired
)
{
	xtlsextension Extension;
	size_t iDataSize;
	size_t iEncodedSize;

	if ( pRequired != NULL ) {
		*pRequired = 0;
	}
	if ( (pExtension == NULL) ||
		 ((Input.Data == NULL) && (Input.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-extension",
			"TLS extension input or output is invalid", SIZE_MAX
		);
		return XTLS_ERROR;
	}
	if ( Input.Size < XTLS_EXTENSION_HEADER_SIZE ) {
		if ( pRequired != NULL ) {
			*pRequired = XTLS_EXTENSION_HEADER_SIZE;
		}
		return XTLS_AGAIN;
	}

	iDataSize = __xrtTlsRead16(Input.Data + 2u);
	iEncodedSize = XTLS_EXTENSION_HEADER_SIZE + iDataSize;
	if ( pRequired != NULL ) {
		*pRequired = iEncodedSize;
	}
	if ( Input.Size < iEncodedSize ) {
		return XTLS_AGAIN;
	}

	memset(&Extension, 0, sizeof(Extension));
	Extension.Type = (xtlsextensiontype)__xrtTlsRead16(Input.Data);
	Extension.Data.Data = Input.Data + XTLS_EXTENSION_HEADER_SIZE;
	Extension.Data.Size = iDataSize;
	Extension.EncodedSize = iEncodedSize;
	*pExtension = Extension;
	return XTLS_OK;
}



/* 把扩展类型和负载编码到调用方缓冲。 */
XRT_API bool xrtTlsExtensionEncode(
	xtlsextensiontype Type,
	xbytesview Data,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iEncodedSize;

	if ( (pOutput == NULL) ||
		 ((Data.Data == NULL) && (Data.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "encode-extension",
			"TLS extension input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (uint32)Type > UINT16_MAX ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_EXTENSION, "encode-extension",
			"TLS extension type does not fit the wire format", SIZE_MAX
		);
		return false;
	}
	iEncodedSize = xrtTlsExtensionSize(Data.Size);
	if ( iEncodedSize == 0 ) {
		return false;
	}
	if ( iOutputSize < iEncodedSize ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "encode-extension",
			"TLS extension output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size != 0 ) {
		memmove(
			pWrite + XTLS_EXTENSION_HEADER_SIZE,
			Data.Data,
			Data.Size
		);
	}
	__xrtTlsWrite16(pWrite, (uint16)Type);
	__xrtTlsWrite16(pWrite + 2u, (uint16)Data.Size);
	return true;
}

#endif
