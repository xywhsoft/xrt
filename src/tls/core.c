#include "../internal/xrt_tls.h"

#include <stdio.h>



#if defined(XRT_FEATURE_TLS)

/* 设置带可选输入偏移的 TLS 结构化错误。 */
void __xrtTlsError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
)
{
	__xrtTlsErrorCause(
		Kind, Code, sOperation, sMessage, iOffset, NULL
	);
}



/* 设置带原因链和可选输入偏移的 TLS 结构化错误。 */
void __xrtTlsErrorCause(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
)
{
	char Data[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.tls";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	if ( iOffset != SIZE_MAX ) {
		(void)snprintf(
			Data, sizeof(Data), "offset=%llu",
			(unsigned long long)iOffset
		);
		Desc.Data = Data;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 读取网络字节序 16 位整数。 */
uint16 __xrtTlsRead16(const uint8* pData)
{
	return (uint16)(((uint16)pData[0] << 8u) | (uint16)pData[1]);
}



/* 写入网络字节序 16 位整数。 */
void __xrtTlsWrite16(uint8* pData, uint16 iValue)
{
	pData[0] = (uint8)(iValue >> 8u);
	pData[1] = (uint8)iValue;
}



/* 读取网络字节序 32 位整数。 */
uint32 __xrtTlsRead32(const uint8* pData)
{
	return ((uint32)pData[0] << 24u) |
		((uint32)pData[1] << 16u) |
		((uint32)pData[2] << 8u) |
		(uint32)pData[3];
}



/* 写入网络字节序 32 位整数。 */
void __xrtTlsWrite32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)(iValue >> 24u);
	pData[1] = (uint8)(iValue >> 16u);
	pData[2] = (uint8)(iValue >> 8u);
	pData[3] = (uint8)iValue;
}



/* 检查借用字节视图是否具有一致的空值语义。 */
bool __xrtTlsViewValid(xbytesview View)
{
	return (View.Data != NULL) || (View.Size == 0);
}



/* 判断借用输入是否与给定输出区域重叠。 */
bool __xrtTlsViewOverlap(
	const void* pOutput,
	size_t iOutputSize,
	xbytesview Input
)
{
	uintptr_t iOutput;
	uintptr_t iInput;

	if ( Input.Size == 0 ) {
		return false;
	}
	iOutput = (uintptr_t)pOutput;
	iInput = (uintptr_t)Input.Data;
	if ( (iOutput > UINTPTR_MAX - iOutputSize) ||
		(iInput > UINTPTR_MAX - Input.Size) ) {
		return true;
	}
	return (iOutput < iInput + Input.Size) &&
		(iInput < iOutput + iOutputSize);
}



/* 判断版本是否属于当前 TLS 协议范围。 */
bool __xrtTlsVersionSupported(xtlsversion Version)
{
	return (Version == XTLS_VERSION_12) ||
		(Version == XTLS_VERSION_13);
}



#if defined(XRT_FEATURE_TLS_RECORD)

/* 写入网络字节序 64 位整数。 */
void __xrtTlsWrite64(uint8* pData, uint64 iValue)
{
	for ( size_t i = 0; i < 8u; i++ ) {
		pData[7u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 以 TLS 规定的方式把序列号异或进 12 字节静态 IV。 */
void __xrtTlsRecordNonce(uint8* pNonce, const uint8* pIv, uint64 iSequence)
{
	uint8 Sequence[12] = { 0 };

	__xrtTlsWrite64(Sequence + 4u, iSequence);
	for ( size_t i = 0; i < sizeof(Sequence); i++ ) {
		pNonce[i] = pIv[i] ^ Sequence[i];
	}
	xrtSecureZero(Sequence, sizeof(Sequence));
}

#endif



/* 检查是否是 XRT 接受的 TLS 记录类型。 */
bool __xrtTlsRecordTypeValid(xtlsrecordtype Type)
{
	return (Type == XTLS_RECORD_CHANGE_CIPHER_SPEC) ||
		(Type == XTLS_RECORD_ALERT) ||
		(Type == XTLS_RECORD_HANDSHAKE) ||
		(Type == XTLS_RECORD_APPLICATION_DATA);
}



/*
	记录层兼容版本允许 0301 到 0303；真正协商版本由握手层单独验证。
	TLS 1.3 首个 ClientHello 为兼容旧中间件可以使用 0301。
*/
static bool __xrtTlsRecordVersionValid(uint16 iVersion)
{
	return (iVersion >= UINT16_C(0x0301)) &&
		(iVersion <= UINT16_C(0x0303));
}



/* 返回协议版本的稳定英文名称。 */
XRT_API cstr xrtTlsVersionName(uint16 iVersion)
{
	switch ( iVersion ) {
		case XTLS_VERSION_12:
			return "TLS 1.2";
		case XTLS_VERSION_13:
			return "TLS 1.3";
		default:
			return "unknown";
	}
}



/* 返回密码套件的稳定英文名称。 */
XRT_API cstr xrtTlsCipherName(xtlscipher Cipher)
{
	switch ( Cipher ) {
		case XTLS_AES_128_GCM_SHA256:
			return "TLS_AES_128_GCM_SHA256";
		case XTLS_AES_256_GCM_SHA384:
			return "TLS_AES_256_GCM_SHA384";
		case XTLS_CHACHA20_POLY1305_SHA256:
			return "TLS_CHACHA20_POLY1305_SHA256";
		case XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256:
			return "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
		case XTLS_ECDHE_RSA_AES_128_GCM_SHA256:
			return "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
		case XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384:
			return "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
		case XTLS_ECDHE_RSA_AES_256_GCM_SHA384:
			return "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
		case XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
			return "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
		case XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
			return "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";
		default:
			return "unknown";
	}
}



/* 返回密码套件唯一的协议和记录保护参数。 */
XRT_API const xtlscipherinfo* xrtTlsCipherInfo(xtlscipher Cipher)
{
	static const xtlscipherinfo Infos[] = {
		{
			XTLS_AES_128_GCM_SHA256,
			XTLS_VERSION_13,
			XTLS_HASH_SHA256,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_INDEPENDENT,
			32, 16, 12, 0, 16
		},
		{
			XTLS_AES_256_GCM_SHA384,
			XTLS_VERSION_13,
			XTLS_HASH_SHA384,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_INDEPENDENT,
			48, 32, 12, 0, 16
		},
		{
			XTLS_CHACHA20_POLY1305_SHA256,
			XTLS_VERSION_13,
			XTLS_HASH_SHA256,
			XTLS_AEAD_CHACHA20_POLY1305,
			XTLS_CIPHER_AUTH_INDEPENDENT,
			32, 32, 12, 0, 16
		},
		{
			XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
			XTLS_VERSION_12,
			XTLS_HASH_SHA256,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_ECDSA,
			32, 16, 4, 8, 16
		},
		{
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_VERSION_12,
			XTLS_HASH_SHA256,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_RSA,
			32, 16, 4, 8, 16
		},
		{
			XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
			XTLS_VERSION_12,
			XTLS_HASH_SHA384,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_ECDSA,
			48, 32, 4, 8, 16
		},
		{
			XTLS_ECDHE_RSA_AES_256_GCM_SHA384,
			XTLS_VERSION_12,
			XTLS_HASH_SHA384,
			XTLS_AEAD_AES_GCM,
			XTLS_CIPHER_AUTH_RSA,
			48, 32, 4, 8, 16
		},
		{
			XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256,
			XTLS_VERSION_12,
			XTLS_HASH_SHA256,
			XTLS_AEAD_CHACHA20_POLY1305,
			XTLS_CIPHER_AUTH_RSA,
			32, 32, 12, 0, 16
		},
		{
			XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256,
			XTLS_VERSION_12,
			XTLS_HASH_SHA256,
			XTLS_AEAD_CHACHA20_POLY1305,
			XTLS_CIPHER_AUTH_ECDSA,
			32, 32, 12, 0, 16
		}
	};

	for ( size_t i = 0; i < (sizeof(Infos) / sizeof(Infos[0])); i++ ) {
		if ( Infos[i].Cipher == Cipher ) {
			return &Infos[i];
		}
	}
	return NULL;
}



/* 返回记录内容类型的稳定英文名称。 */
XRT_API cstr xrtTlsRecordName(xtlsrecordtype Type)
{
	switch ( Type ) {
		case XTLS_RECORD_CHANGE_CIPHER_SPEC:
			return "change_cipher_spec";
		case XTLS_RECORD_ALERT:
			return "alert";
		case XTLS_RECORD_HANDSHAKE:
			return "handshake";
		case XTLS_RECORD_APPLICATION_DATA:
			return "application_data";
		default:
			return "unknown";
	}
}



/* 返回 Alert 的稳定英文名称。 */
XRT_API cstr xrtTlsAlertName(xtlsalert Alert)
{
	switch ( Alert ) {
		case XTLS_ALERT_CLOSE_NOTIFY: return "close_notify";
		case XTLS_ALERT_UNEXPECTED_MESSAGE: return "unexpected_message";
		case XTLS_ALERT_BAD_RECORD_MAC: return "bad_record_mac";
		case XTLS_ALERT_RECORD_OVERFLOW: return "record_overflow";
		case XTLS_ALERT_HANDSHAKE_FAILURE: return "handshake_failure";
		case XTLS_ALERT_BAD_CERTIFICATE: return "bad_certificate";
		case XTLS_ALERT_UNSUPPORTED_CERTIFICATE: return "unsupported_certificate";
		case XTLS_ALERT_CERTIFICATE_REVOKED: return "certificate_revoked";
		case XTLS_ALERT_CERTIFICATE_EXPIRED: return "certificate_expired";
		case XTLS_ALERT_CERTIFICATE_UNKNOWN: return "certificate_unknown";
		case XTLS_ALERT_ILLEGAL_PARAMETER: return "illegal_parameter";
		case XTLS_ALERT_UNKNOWN_CA: return "unknown_ca";
		case XTLS_ALERT_ACCESS_DENIED: return "access_denied";
		case XTLS_ALERT_DECODE_ERROR: return "decode_error";
		case XTLS_ALERT_DECRYPT_ERROR: return "decrypt_error";
		case XTLS_ALERT_PROTOCOL_VERSION: return "protocol_version";
		case XTLS_ALERT_INSUFFICIENT_SECURITY: return "insufficient_security";
		case XTLS_ALERT_INTERNAL_ERROR: return "internal_error";
		case XTLS_ALERT_INAPPROPRIATE_FALLBACK: return "inappropriate_fallback";
		case XTLS_ALERT_USER_CANCELED: return "user_canceled";
		case XTLS_ALERT_MISSING_EXTENSION: return "missing_extension";
		case XTLS_ALERT_UNSUPPORTED_EXTENSION: return "unsupported_extension";
		case XTLS_ALERT_UNRECOGNIZED_NAME: return "unrecognized_name";
		case XTLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE:
			return "bad_certificate_status_response";
		case XTLS_ALERT_UNKNOWN_PSK_IDENTITY: return "unknown_psk_identity";
		case XTLS_ALERT_CERTIFICATE_REQUIRED: return "certificate_required";
		case XTLS_ALERT_NO_APPLICATION_PROTOCOL: return "no_application_protocol";
		default:
			return "unknown_alert";
	}
}



/* 返回给定负载所需的完整记录长度。 */
XRT_API size_t xrtTlsRecordSize(size_t iPayloadSize)
{
	if ( iPayloadSize > XTLS12_RECORD_CIPHERTEXT_MAX ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_SIZE, "size-record",
			"TLS record payload exceeds the protocol limit", SIZE_MAX
		);
		return 0;
	}
	return XTLS_RECORD_HEADER_SIZE + iPayloadSize;
}



/* 解析输入开头的一条完整记录。 */
XRT_API xtlsresult xrtTlsRecordParse(
	xbytesview Input,
	xtlsrecord* pRecord,
	size_t* pRequired
)
{
	xtlsrecord Record;
	uint16 iPayloadSize;
	size_t iEncodedSize;

	if ( pRequired != NULL ) {
		*pRequired = 0;
	}
	if ( (pRecord == NULL) || ((Input.Data == NULL) && (Input.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-record",
			"TLS record input or output is invalid", SIZE_MAX
		);
		return XTLS_ERROR;
	}
	if ( Input.Size < XTLS_RECORD_HEADER_SIZE ) {
		if ( pRequired != NULL ) {
			*pRequired = XTLS_RECORD_HEADER_SIZE;
		}
		return XTLS_AGAIN;
	}
	memset(&Record, 0, sizeof(Record));
	Record.Type = (xtlsrecordtype)Input.Data[0];
	Record.LegacyVersion = __xrtTlsRead16(Input.Data + 1u);
	if ( !__xrtTlsRecordTypeValid(Record.Type) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_RECORD_TYPE, "parse-record",
			"TLS record has an unknown content type", 0
		);
		return XTLS_ERROR;
	}
	if ( !__xrtTlsRecordVersionValid(Record.LegacyVersion) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_RECORD_VERSION, "parse-record",
			"TLS record has an invalid legacy version", 1
		);
		return XTLS_ERROR;
	}
	iPayloadSize = __xrtTlsRead16(Input.Data + 3u);
	if ( iPayloadSize > XTLS12_RECORD_CIPHERTEXT_MAX ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_RECORD_SIZE, "parse-record",
			"TLS record payload exceeds the protocol limit", 3
		);
		return XTLS_ERROR;
	}
	iEncodedSize = XTLS_RECORD_HEADER_SIZE + (size_t)iPayloadSize;
	if ( pRequired != NULL ) {
		*pRequired = iEncodedSize;
	}
	if ( Input.Size < iEncodedSize ) {
		return XTLS_AGAIN;
	}
	Record.Payload.Data = Input.Data + XTLS_RECORD_HEADER_SIZE;
	Record.Payload.Size = iPayloadSize;
	Record.EncodedSize = iEncodedSize;
	*pRecord = Record;
	return XTLS_OK;
}



/* 把一条记录编码到调用方缓冲。 */
XRT_API bool xrtTlsRecordEncode(
	xtlsrecordtype Type,
	uint16 iLegacyVersion,
	xbytesview Payload,
	ptr pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iEncodedSize;

	if ( (pOutput == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "encode-record",
			"TLS record input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsRecordTypeValid(Type) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_RECORD_TYPE, "encode-record",
			"TLS record content type is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsRecordVersionValid(iLegacyVersion) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_RECORD_VERSION, "encode-record",
			"TLS record legacy version is invalid", SIZE_MAX
		);
		return false;
	}
	iEncodedSize = xrtTlsRecordSize(Payload.Size);
	if ( iEncodedSize == 0 ) {
		return false;
	}
	if ( iOutputSize < iEncodedSize ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_BUFFER, "encode-record",
			"TLS record output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( Payload.Size != 0 ) {
		memmove(
			pWrite + XTLS_RECORD_HEADER_SIZE,
			Payload.Data,
			Payload.Size
		);
	}
	pWrite[0] = (uint8)Type;
	__xrtTlsWrite16(pWrite + 1u, iLegacyVersion);
	__xrtTlsWrite16(pWrite + 3u, (uint16)Payload.Size);
	return true;
}



/* 解析恰好一个两字节 Alert 负载。 */
XRT_API bool xrtTlsAlertParse(
	xbytesview Payload,
	xtlsalertlevel* pLevel,
	xtlsalert* pAlert
)
{
	xtlsalertlevel Level;

	if ( (pLevel == NULL) || (pAlert == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-alert",
			"TLS alert input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Payload.Size != 2u ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_ALERT, "parse-alert",
			"TLS alert payload must contain exactly two bytes", 0
		);
		return false;
	}
	Level = (xtlsalertlevel)Payload.Data[0];
	if ( (Level != XTLS_ALERT_WARNING) && (Level != XTLS_ALERT_FATAL) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_ALERT, "parse-alert",
			"TLS alert level is invalid", 0
		);
		return false;
	}
	*pLevel = Level;
	*pAlert = (xtlsalert)Payload.Data[1];
	return true;
}



/* 编码一个两字节 Alert 负载。 */
XRT_API bool xrtTlsAlertEncode(
	xtlsalertlevel Level,
	xtlsalert Alert,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;

	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "encode-alert",
			"TLS alert output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Level != XTLS_ALERT_WARNING) && (Level != XTLS_ALERT_FATAL) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_ALERT, "encode-alert",
			"TLS alert level is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (uint32)Alert > UINT8_MAX ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_ALERT, "encode-alert",
			"TLS alert description does not fit the wire format", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < 2u ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_BUFFER, "encode-alert",
			"TLS alert output buffer is too small", SIZE_MAX
		);
		return false;
	}
	pWrite[0] = (uint8)Level;
	pWrite[1] = (uint8)Alert;
	return true;
}

#endif
