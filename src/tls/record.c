#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_RECORD)

/* 返回 TLS 1.3 内层允许携带的真实内容类型。 */
static bool __xrtTlsRecordInnerTypeValid(xtlsrecordtype Type)
{
	return (Type == XTLS_RECORD_ALERT) ||
		(Type == XTLS_RECORD_HANDSHAKE) ||
		(Type == XTLS_RECORD_APPLICATION_DATA);
}



/* 统一调用当前密钥绑定的 AEAD 加密后端。 */
static bool __xrtTlsRecordAeadSeal(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
)
{
	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		if ( pKey->Aead == XTLS_AEAD_AES_GCM ) {
			return __xrtTlsRecordAesSeal(
				pKey, pNonce, pAad, iAadSize,
				pPlain, iPlainSize, pOutput, iOutputSize
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		if ( pKey->Aead == XTLS_AEAD_CHACHA20_POLY1305 ) {
			return __xrtTlsRecordChaChaSeal(
				pKey, pNonce, pAad, iAadSize,
				pPlain, iPlainSize, pOutput, iOutputSize
			);
		}
	#endif
	(void)pKey;
	(void)pNonce;
	(void)pAad;
	(void)iAadSize;
	(void)pPlain;
	(void)iPlainSize;
	(void)pOutput;
	(void)iOutputSize;
	__xrtTlsError(
		XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "seal-record",
		"TLS record cipher backend is not enabled", SIZE_MAX
	);
	return false;
}



/* 统一调用当前密钥绑定的 AEAD 解密后端。 */
static bool __xrtTlsRecordAeadOpen(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
)
{
	bool bResult = false;

	(void)pKey;
	(void)pNonce;
	(void)pAad;
	(void)iAadSize;
	(void)pInput;
	(void)iInputSize;
	(void)pPlain;
	(void)iPlainSize;

	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		if ( pKey->Aead == XTLS_AEAD_AES_GCM ) {
			bResult = __xrtTlsRecordAesOpen(
				pKey, pNonce, pAad, iAadSize,
				pInput, iInputSize, pPlain, iPlainSize
			);
		} else
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		if ( pKey->Aead == XTLS_AEAD_CHACHA20_POLY1305 ) {
			bResult = __xrtTlsRecordChaChaOpen(
				pKey, pNonce, pAad, iAadSize,
				pInput, iInputSize, pPlain, iPlainSize
			);
		} else
	#endif
	{
		__xrtTlsError(
			XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "open-record",
			"TLS record cipher backend is not enabled", SIZE_MAX
		);
		return false;
	}
	if ( !bResult ) {
		const xerror* pCause = xrtGetError();

		__xrtTlsErrorCause(
			XERR_PROTOCOL, XTLS_ERROR_CIPHER, "open-record",
			"TLS record authentication failed", SIZE_MAX, pCause
		);
	}
	return bResult;
}



/* 返回当前构建中是否支持指定版本与密码套件。 */
bool __xrtTlsRecordCipherSupported(xtlsversion Version, xtlscipher Cipher)
{
	const xtlscipherinfo* pInfo = xrtTlsCipherInfo(Cipher);

	if ( (pInfo == NULL) || (pInfo->Version != Version) ) {
		return false;
	}
	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		if ( pInfo->Aead == XTLS_AEAD_AES_GCM ) {
			return true;
		}
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		if ( pInfo->Aead == XTLS_AEAD_CHACHA20_POLY1305 ) {
			return true;
		}
	#endif
	return false;
}



/* 返回单组记录密钥允许处理的记录数量。 */
uint64 __xrtTlsRecordKeyLimit(const xtlsrecordkey* pKey)
{
	if ( (pKey != NULL) && (pKey->Aead == XTLS_AEAD_AES_GCM) ) {
		return XTLS_AES_GCM_RECORD_LIMIT;
	}
	return UINT64_MAX;
}



/* 返回记录密钥是否必须更新或关闭。 */
bool __xrtTlsRecordKeyExhausted(const xtlsrecordkey* pKey)
{
	return (pKey == NULL) || !pKey->Ready ||
		(pKey->Sequence >= __xrtTlsRecordKeyLimit(pKey));
}



/* 初始化单向记录密钥。 */
bool __xrtTlsRecordKeyInit(
	xtlsrecordkey* pKey,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
)
{
	xtlsrecordkey Next;
	const xtlscipherinfo* pInfo;
	bool bReady = false;

	if ( (pKey == NULL) || (Key.Data == NULL) || (Iv.Data == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "initialize-record-key",
			"TLS record key input is invalid", SIZE_MAX
		);
		return false;
	}
	pInfo = xrtTlsCipherInfo(Cipher);
	if ( !__xrtTlsRecordCipherSupported(Version, Cipher) ) {
		__xrtTlsError(
			XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "initialize-record-key",
			"TLS version and cipher suite are not supported together", SIZE_MAX
		);
		return false;
	}
	if ( (Key.Size != pInfo->KeySize) || (Iv.Size != pInfo->IvSize) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_CIPHER, "initialize-record-key",
			"TLS record key or IV has the wrong size", SIZE_MAX
		);
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	Next.Version = Version;
	Next.Cipher = Cipher;
	Next.Aead = pInfo->Aead;
	Next.IvSize = pInfo->IvSize;
	Next.ExplicitNonceSize = pInfo->ExplicitNonceSize;
	Next.TagSize = pInfo->TagSize;
	memcpy(Next.Iv, Iv.Data, Iv.Size);
	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		if ( Next.Aead == XTLS_AEAD_AES_GCM ) {
			bReady = __xrtTlsRecordAesInit(&Next, Key.Data, Key.Size);
		} else
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		if ( Next.Aead == XTLS_AEAD_CHACHA20_POLY1305 ) {
			bReady = __xrtTlsRecordChaChaInit(&Next, Key.Data, Key.Size);
		} else
	#endif
	{
		bReady = false;
	}
	if ( !bReady ) {
		const xerror* pCause = xrtGetError();

		xrtSecureZero(&Next, sizeof(Next));
		__xrtTlsErrorCause(
			XERR_INTERNAL, XTLS_ERROR_CIPHER, "initialize-record-key",
			"TLS record cipher key initialization failed", SIZE_MAX,
			pCause
		);
		return false;
	}
	Next.Ready = true;
	xrtSecureZero(pKey, sizeof(*pKey));
	*pKey = Next;
	xrtSecureZero(&Next, sizeof(Next));
	return true;
}



/* 清除记录密钥、IV 和序列号。 */
void __xrtTlsRecordKeyClear(xtlsrecordkey* pKey)
{
	if ( pKey == NULL ) {
		return;
	}
	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		if ( pKey->Ready && (pKey->Aead == XTLS_AEAD_AES_GCM) ) {
			__xrtTlsRecordAesClear(pKey);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		if ( pKey->Ready &&
			(pKey->Aead == XTLS_AEAD_CHACHA20_POLY1305) ) {
			__xrtTlsRecordChaChaClear(pKey);
		}
	#endif
	xrtSecureZero(pKey, sizeof(*pKey));
}



/* 返回保护一条明文记录所需的完整线路长度。 */
size_t __xrtTlsRecordSealSize(
	const xtlsrecordkey* pKey,
	size_t iPlainSize,
	size_t iPadding
)
{
	size_t iPayloadSize;

	if ( (pKey == NULL) || !pKey->Ready ||
		(iPlainSize > XTLS_RECORD_PLAINTEXT_MAX) ) {
		return 0;
	}
	if ( pKey->Version == XTLS_VERSION_13 ) {
		if ( (iPadding > (SIZE_MAX - iPlainSize - 1u)) ||
			((iPlainSize + 1u + iPadding) > XTLS13_INNER_PLAINTEXT_MAX) ) {
			return 0;
		}
		iPayloadSize = iPlainSize + 1u + iPadding + pKey->TagSize;
		return iPayloadSize <= XTLS13_RECORD_CIPHERTEXT_MAX ?
			XTLS_RECORD_HEADER_SIZE + iPayloadSize : 0;
	}
	if ( pKey->Version == XTLS_VERSION_12 ) {
		iPayloadSize = iPlainSize + pKey->TagSize +
			pKey->ExplicitNonceSize;
		return (iPadding == 0) &&
			(iPayloadSize <= XTLS12_RECORD_CIPHERTEXT_MAX) ?
			XTLS_RECORD_HEADER_SIZE + iPayloadSize : 0;
	}
	return 0;
}



/* 保护一条 TLS 1.3 记录。 */
static bool __xrtTlsRecordSeal13(
	xtlsrecordkey* pKey,
	xtlsrecordtype Type,
	xbytesview Plain,
	size_t iPadding,
	uint8* pOutput,
	size_t iOutputSize,
	size_t* pWritten
)
{
	uint8 Header[XTLS_RECORD_HEADER_SIZE];
	uint8 Nonce[12];
	uint8* pInner = pOutput + XTLS_RECORD_HEADER_SIZE;
	size_t iInnerSize = Plain.Size + 1u + iPadding;
	size_t iPayloadSize = iInnerSize + pKey->TagSize;

	Header[0] = XTLS_RECORD_APPLICATION_DATA;
	__xrtTlsWrite16(Header + 1u, UINT16_C(0x0303));
	__xrtTlsWrite16(Header + 3u, (uint16)iPayloadSize);
	if ( Plain.Size != 0 ) {
		memmove(pInner, Plain.Data, Plain.Size);
	}
	pInner[Plain.Size] = (uint8)Type;
	memset(pInner + Plain.Size + 1u, 0, iPadding);
	__xrtTlsRecordNonce(Nonce, pKey->Iv, pKey->Sequence);
	if ( !__xrtTlsRecordAeadSeal(
		pKey, Nonce, Header, sizeof(Header),
		pInner, iInnerSize, pInner, iOutputSize - XTLS_RECORD_HEADER_SIZE
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		return false;
	}
	memcpy(pOutput, Header, sizeof(Header));
	xrtSecureZero(Nonce, sizeof(Nonce));
	pKey->Sequence++;
	*pWritten = XTLS_RECORD_HEADER_SIZE + iPayloadSize;
	return true;
}



/* 保护一条 TLS 1.2 记录。 */
static bool __xrtTlsRecordSeal12(
	xtlsrecordkey* pKey,
	xtlsrecordtype Type,
	xbytesview Plain,
	uint8* pOutput,
	size_t iOutputSize,
	size_t* pWritten
)
{
	uint8 Header[XTLS_RECORD_HEADER_SIZE];
	uint8 Aad[13];
	uint8 Nonce[12] = { 0 };
	uint8* pCipher;
	size_t iExplicit = pKey->ExplicitNonceSize;
	size_t iPayloadSize = iExplicit + Plain.Size + pKey->TagSize;

	Header[0] = (uint8)Type;
	__xrtTlsWrite16(Header + 1u, UINT16_C(0x0303));
	__xrtTlsWrite16(Header + 3u, (uint16)iPayloadSize);
	__xrtTlsWrite64(Aad, pKey->Sequence);
	Aad[8] = (uint8)Type;
	__xrtTlsWrite16(Aad + 9u, UINT16_C(0x0303));
	__xrtTlsWrite16(Aad + 11u, (uint16)Plain.Size);
	if ( iExplicit != 0 ) {
		memcpy(Nonce, pKey->Iv, 4u);
		__xrtTlsWrite64(Nonce + 4u, pKey->Sequence);
	} else {
		__xrtTlsRecordNonce(Nonce, pKey->Iv, pKey->Sequence);
	}
	pCipher = pOutput + XTLS_RECORD_HEADER_SIZE + iExplicit;
	if ( Plain.Size != 0 ) {
		memmove(pCipher, Plain.Data, Plain.Size);
	}
	if ( !__xrtTlsRecordAeadSeal(
		pKey, Nonce, Aad, sizeof(Aad),
		pCipher, Plain.Size, pCipher,
		iOutputSize - XTLS_RECORD_HEADER_SIZE - iExplicit
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		xrtSecureZero(Aad, sizeof(Aad));
		return false;
	}
	memcpy(pOutput, Header, sizeof(Header));
	if ( iExplicit != 0 ) {
		__xrtTlsWrite64(pOutput + XTLS_RECORD_HEADER_SIZE, pKey->Sequence);
	}
	xrtSecureZero(Nonce, sizeof(Nonce));
	xrtSecureZero(Aad, sizeof(Aad));
	pKey->Sequence++;
	*pWritten = XTLS_RECORD_HEADER_SIZE + iPayloadSize;
	return true;
}



/* 保护一条记录并在成功后递增发送序列号。 */
bool __xrtTlsRecordSeal(
	xtlsrecordkey* pKey,
	xtlsrecordtype Type,
	xbytesview Plain,
	size_t iPadding,
	void* pOutput,
	size_t iOutputSize,
	size_t* pWritten
)
{
	size_t iRequired;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (pKey == NULL) || (pOutput == NULL) || (pWritten == NULL) ||
		((Plain.Data == NULL) && (Plain.Size != 0)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "seal-record",
			"TLS protected record input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !pKey->Ready ) {
		__xrtTlsError(
			XERR_STATE, XTLS_ERROR_STATE, "seal-record",
			"TLS record key is not initialized", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsRecordInnerTypeValid(Type) ) {
		__xrtTlsError(
			XERR_VALUE, XTLS_ERROR_RECORD_TYPE, "seal-record",
			"TLS protected record type is invalid", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsRecordKeyExhausted(pKey) ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "seal-record",
			"TLS record key usage limit is exhausted", SIZE_MAX
		);
		return false;
	}
	iRequired = __xrtTlsRecordSealSize(pKey, Plain.Size, iPadding);
	if ( iRequired == 0 ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_SIZE, "seal-record",
			"TLS protected record exceeds its version limit", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_BUFFER, "seal-record",
			"TLS protected record output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( pKey->Version == XTLS_VERSION_13 ) {
		return __xrtTlsRecordSeal13(
			pKey, Type, Plain, iPadding,
			(uint8*)pOutput, iOutputSize, pWritten
		);
	}
	return __xrtTlsRecordSeal12(
		pKey, Type, Plain, (uint8*)pOutput, iOutputSize, pWritten
	);
}



/* 打开一条 TLS 1.3 记录并提取内层类型与零填充。 */
static bool __xrtTlsRecordOpen13(
	xtlsrecordkey* pKey,
	const xtlsrecord* pRecord,
	uint8* pOutput,
	size_t iOutputSize,
	xtlsrecordtype* pType,
	size_t* pWritten
)
{
	uint8 Header[XTLS_RECORD_HEADER_SIZE];
	uint8 Nonce[12];
	size_t iInnerSize = pRecord->Payload.Size - pKey->TagSize;
	size_t iTypeOffset;

	Header[0] = XTLS_RECORD_APPLICATION_DATA;
	__xrtTlsWrite16(Header + 1u, UINT16_C(0x0303));
	__xrtTlsWrite16(Header + 3u, (uint16)pRecord->Payload.Size);
	__xrtTlsRecordNonce(Nonce, pKey->Iv, pKey->Sequence);
	if ( !__xrtTlsRecordAeadOpen(
		pKey, Nonce, Header, sizeof(Header),
		pRecord->Payload.Data, pRecord->Payload.Size,
		pOutput, iOutputSize
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		return false;
	}
	xrtSecureZero(Nonce, sizeof(Nonce));
	iTypeOffset = iInnerSize;
	while ( (iTypeOffset != 0) && (pOutput[iTypeOffset - 1u] == 0) ) {
		iTypeOffset--;
	}
	if ( (iTypeOffset == 0) || !__xrtTlsRecordInnerTypeValid(
		(xtlsrecordtype)pOutput[iTypeOffset - 1u]
	) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_RECORD_TYPE, "open-record",
			"TLS 1.3 inner plaintext has no valid content type", SIZE_MAX
		);
		return false;
	}
	*pType = (xtlsrecordtype)pOutput[iTypeOffset - 1u];
	*pWritten = iTypeOffset - 1u;
	pKey->Sequence++;
	return true;
}



/* 打开一条 TLS 1.2 记录。 */
static bool __xrtTlsRecordOpen12(
	xtlsrecordkey* pKey,
	const xtlsrecord* pRecord,
	uint8* pOutput,
	size_t iOutputSize,
	xtlsrecordtype* pType,
	size_t* pWritten
)
{
	uint8 Aad[13];
	uint8 Nonce[12] = { 0 };
	size_t iExplicit = pKey->ExplicitNonceSize;
	size_t iPlainSize = pRecord->Payload.Size - iExplicit - pKey->TagSize;

	__xrtTlsWrite64(Aad, pKey->Sequence);
	Aad[8] = (uint8)pRecord->Type;
	__xrtTlsWrite16(Aad + 9u, pRecord->LegacyVersion);
	__xrtTlsWrite16(Aad + 11u, (uint16)iPlainSize);
	if ( iExplicit != 0 ) {
		memcpy(Nonce, pKey->Iv, 4u);
		memcpy(Nonce + 4u, pRecord->Payload.Data, 8u);
	} else {
		__xrtTlsRecordNonce(Nonce, pKey->Iv, pKey->Sequence);
	}
	if ( !__xrtTlsRecordAeadOpen(
		pKey, Nonce, Aad, sizeof(Aad),
		pRecord->Payload.Data + iExplicit,
		pRecord->Payload.Size - iExplicit,
		pOutput, iOutputSize
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		xrtSecureZero(Aad, sizeof(Aad));
		return false;
	}
	xrtSecureZero(Nonce, sizeof(Nonce));
	xrtSecureZero(Aad, sizeof(Aad));
	*pType = pRecord->Type;
	*pWritten = iPlainSize;
	pKey->Sequence++;
	return true;
}



/* 打开一条完整记录并在成功后递增接收序列号。 */
bool __xrtTlsRecordOpen(
	xtlsrecordkey* pKey,
	const xtlsrecord* pRecord,
	void* pOutput,
	size_t iOutputSize,
	xtlsrecordtype* pType,
	size_t* pWritten
)
{
	size_t iPlainCapacity;
	size_t iMinimum;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (pKey == NULL) || (pRecord == NULL) || (pOutput == NULL) ||
		(pType == NULL) || (pWritten == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "open-record",
			"TLS protected record input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (pRecord->Payload.Data == NULL) &&
		(pRecord->Payload.Size != 0) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "open-record",
			"TLS protected record payload is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !pKey->Ready ) {
		__xrtTlsError(
			XERR_STATE, XTLS_ERROR_STATE, "open-record",
			"TLS record key is not initialized", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsRecordKeyExhausted(pKey) ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "open-record",
			"TLS record key usage limit is exhausted", SIZE_MAX
		);
		return false;
	}
	if ( (pRecord->LegacyVersion != UINT16_C(0x0303)) ||
		!__xrtTlsRecordInnerTypeValid(pRecord->Type) ) {
		__xrtTlsError(
			XERR_PROTOCOL, XTLS_ERROR_RECORD_TYPE, "open-record",
			"TLS protected record header is invalid", SIZE_MAX
		);
		return false;
	}
	if ( pKey->Version == XTLS_VERSION_13 ) {
		if ( pRecord->Type != XTLS_RECORD_APPLICATION_DATA ) {
			__xrtTlsError(
				XERR_PROTOCOL, XTLS_ERROR_RECORD_TYPE, "open-record",
				"TLS 1.3 protected record has an invalid outer type", SIZE_MAX
			);
			return false;
		}
		if ( (pRecord->Payload.Size < ((size_t)pKey->TagSize + 1u)) ||
			(pRecord->Payload.Size > XTLS13_RECORD_CIPHERTEXT_MAX) ||
			((pRecord->Payload.Size - pKey->TagSize) >
			 XTLS13_INNER_PLAINTEXT_MAX) ) {
			__xrtTlsError(
				XERR_PROTOCOL, XTLS_ERROR_RECORD_SIZE, "open-record",
				"TLS 1.3 protected record size is invalid", SIZE_MAX
			);
			return false;
		}
		iPlainCapacity = pRecord->Payload.Size - pKey->TagSize;
	} else {
		iMinimum = (size_t)pKey->ExplicitNonceSize + pKey->TagSize;
		if ( (pRecord->Payload.Size < iMinimum) ||
			(pRecord->Payload.Size > XTLS12_RECORD_CIPHERTEXT_MAX) ) {
			__xrtTlsError(
				XERR_PROTOCOL, XTLS_ERROR_RECORD_SIZE, "open-record",
				"TLS 1.2 protected record size is invalid", SIZE_MAX
			);
			return false;
		}
		iPlainCapacity = pRecord->Payload.Size - iMinimum;
	}
	if ( iOutputSize < iPlainCapacity ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RECORD_BUFFER, "open-record",
			"TLS plaintext output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( pKey->Version == XTLS_VERSION_13 ) {
		return __xrtTlsRecordOpen13(
			pKey, pRecord, (uint8*)pOutput,
			iOutputSize, pType, pWritten
		);
	}
	return __xrtTlsRecordOpen12(
		pKey, pRecord, (uint8*)pOutput,
		iOutputSize, pType, pWritten
	);
}

#endif
