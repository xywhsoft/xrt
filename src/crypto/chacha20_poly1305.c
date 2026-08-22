#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305)

/* 设置稳定的 AEAD 认证失败错误，不暴露标签比较位置。 */
static void __xrtChaCha20Poly1305AuthError(void)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = XCRYPTO_ERROR_AUTHENTICATION;
	Desc.Operation = "chacha20-poly1305-open";
	Desc.Message = "authentication tag verification failed";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 拒绝危险的部分重叠，同时保留密文与明文同起点的原位路径。 */
static bool __xrtChaCha20Poly1305Validate(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iSize,
	void* pOutput,
	const void* pTag
)
{
	if ( (pKey == NULL) || (pNonce == NULL) || (pTag == NULL) ||
		 ((pAad == NULL) && (iAadSize != 0)) ||
		 ((pInput == NULL) && (iSize != 0)) ||
		 ((pOutput == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (uint64)iSize > XRT_CHACHA20_POLY1305_MAX_SIZE ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( (__xrtCryptoRangesOverlap(
			pOutput, iSize, pKey, XRT_CHACHA20_POLY1305_KEY_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pOutput, iSize, pNonce, XRT_CHACHA20_POLY1305_NONCE_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pOutput, iSize, pAad, iAadSize
		)) || ((pOutput != pInput) && (__xrtCryptoRangesOverlap(
			pOutput, iSize, pInput, iSize
		))) || (__xrtCryptoRangesOverlap(
			pTag, XRT_CHACHA20_POLY1305_TAG_SIZE,
			pKey, XRT_CHACHA20_POLY1305_KEY_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pTag, XRT_CHACHA20_POLY1305_TAG_SIZE,
			pNonce, XRT_CHACHA20_POLY1305_NONCE_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pTag, XRT_CHACHA20_POLY1305_TAG_SIZE, pAad, iAadSize
		)) || (__xrtCryptoRangesOverlap(
			pTag, XRT_CHACHA20_POLY1305_TAG_SIZE, pInput, iSize
		)) || (__xrtCryptoRangesOverlap(
			pTag, XRT_CHACHA20_POLY1305_TAG_SIZE, pOutput, iSize
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 按 RFC 8439 顺序认证 AAD、密文、补齐和两个 64 位长度。 */
static bool __xrtChaCha20Poly1305Tag(
	const uint8 pKey[XRT_CHACHA20_POLY1305_KEY_SIZE],
	const uint8 pNonce[XRT_CHACHA20_POLY1305_NONCE_SIZE],
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	uint8 pTag[XRT_CHACHA20_POLY1305_TAG_SIZE]
)
{
	xpoly1305 State;
	uint8 Stream[XRT_CHACHA20_BLOCK_SIZE];
	uint8 PolyKey[XRT_POLY1305_KEY_SIZE];
	uint8 Lengths[16];
	uint8 Zeros[16];
	size_t iAadPad = (16u - (iAadSize & 15u)) & 15u;
	size_t iCipherPad = (16u - (iCipherSize & 15u)) & 15u;
	bool bResult;

	memset(&State, 0, sizeof(State));
	memset(Zeros, 0, sizeof(Zeros));
	__xrtChaCha20Block(pKey, pNonce, 0, Stream);
	memcpy(PolyKey, Stream, sizeof(PolyKey));
	__xrtCryptoStoreLe64(Lengths, (uint64)iAadSize);
	__xrtCryptoStoreLe64(Lengths + 8, (uint64)iCipherSize);
	bResult = xrtPoly1305Init(&State, PolyKey) &&
		xrtPoly1305Update(&State, pAad, iAadSize) &&
		xrtPoly1305Update(&State, Zeros, iAadPad) &&
		xrtPoly1305Update(&State, pCipher, iCipherSize) &&
		xrtPoly1305Update(&State, Zeros, iCipherPad) &&
		xrtPoly1305Update(&State, Lengths, sizeof(Lengths)) &&
		xrtPoly1305Final(&State, pTag);
	xrtSecureZero(Lengths, sizeof(Lengths));
	xrtSecureZero(PolyKey, sizeof(PolyKey));
	xrtSecureZero(Stream, sizeof(Stream));
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 先完成 ChaCha20 加密，再认证生成的密文并提交标签。 */
XRT_API bool xrtChaCha20Poly1305Encrypt(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pCipher,
	void* pTag
)
{
	uint8 Tag[XRT_CHACHA20_POLY1305_TAG_SIZE];
	bool bResult = false;

	if ( !__xrtChaCha20Poly1305Validate(
		pKey, pNonce, pAad, iAadSize,
		pPlain, iPlainSize, pCipher, pTag
	) ) {
		return false;
	}
	if ( !xrtChaCha20(
		pKey, pNonce, 1, pPlain, pCipher, iPlainSize
	) ) {
		goto cleanup;
	}
	if ( !__xrtChaCha20Poly1305Tag(
		(const uint8*)pKey,
		(const uint8*)pNonce,
		pAad,
		iAadSize,
		pCipher,
		iPlainSize,
		Tag
	) ) {
		goto cleanup;
	}
	memcpy(pTag, Tag, sizeof(Tag));
	bResult = true;

cleanup:
	xrtSecureZero(Tag, sizeof(Tag));
	return bResult;
}



/* 先以常量时间验证标签，成功后才写入明文。 */
XRT_API bool xrtChaCha20Poly1305Decrypt(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	const void* pTag,
	void* pPlain
)
{
	uint8 Tag[XRT_CHACHA20_POLY1305_TAG_SIZE];
	bool bResult = false;

	if ( !__xrtChaCha20Poly1305Validate(
		pKey, pNonce, pAad, iAadSize,
		pCipher, iCipherSize, pPlain, pTag
	) ) {
		return false;
	}
	if ( !__xrtChaCha20Poly1305Tag(
		(const uint8*)pKey,
		(const uint8*)pNonce,
		pAad,
		iAadSize,
		pCipher,
		iCipherSize,
		Tag
	) ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(Tag, pTag, sizeof(Tag)) ) {
		__xrtChaCha20Poly1305AuthError();
		goto cleanup;
	}
	bResult = xrtChaCha20(
		pKey, pNonce, 1, pCipher, pPlain, iCipherSize
	);

cleanup:
	xrtSecureZero(Tag, sizeof(Tag));
	return bResult;
}



/* 检查容量后把 detached 输出映射为连续的 cipher || tag。 */
XRT_API bool xrtChaCha20Poly1305Seal(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
)
{
	size_t iRequired;

	if ( iPlainSize > (SIZE_MAX - XRT_CHACHA20_POLY1305_OVERHEAD) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = iPlainSize + XRT_CHACHA20_POLY1305_OVERHEAD;
	if ( pOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	return xrtChaCha20Poly1305Encrypt(
		pKey,
		pNonce,
		pAad,
		iAadSize,
		pPlain,
		iPlainSize,
		pOutput,
		(uint8*)pOutput + iPlainSize
	);
}



/* 分离连续输入末尾标签，并复用认证后写入的 detached 解密路径。 */
XRT_API bool xrtChaCha20Poly1305Open(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
)
{
	size_t iCipherSize;

	if ( (pInput == NULL) && (iInputSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iInputSize < XRT_CHACHA20_POLY1305_OVERHEAD ) {
		__xrtChaCha20Poly1305AuthError();
		return false;
	}
	iCipherSize = iInputSize - XRT_CHACHA20_POLY1305_OVERHEAD;
	if ( (iCipherSize != 0) && (pPlain == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iPlainSize < iCipherSize ) {
		__xrtErrorSetRange();
		return false;
	}
	return xrtChaCha20Poly1305Decrypt(
		pKey,
		pNonce,
		pAad,
		iAadSize,
		pInput,
		iCipherSize,
		(const uint8*)pInput + iCipherSize,
		pPlain
	);
}

#endif
