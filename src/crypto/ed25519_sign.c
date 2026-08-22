#include "../internal/xrt_crypto_ed25519.h"



#if defined(XRT_FEATURE_CRYPTO_ED25519_SIGN)

/* 验证消息指针和预哈希模式的固定输入长度。 */
static bool __xrtEd25519SignMessage(
	xed25519mode iMode,
	const void* pMessage,
	size_t iMessageSize
)
{
	if ( ((pMessage == NULL) && (iMessageSize != 0)) ||
		 ((iMode == XED25519_PREHASH) &&
		  (iMessageSize != XRT_ED25519_PREHASH_SIZE)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 使用展开密钥签署 RFC 8032 指定模式的数据。 */
XRT_API bool xrtEd25519SignMode(
	const xed25519key* pKey,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
)
{
	xsha512 Hash;
	uint8 NonceHash[XRT_SHA512_SIZE];
	uint8 Nonce[32];
	uint8 ChallengeHash[XRT_SHA512_SIZE];
	uint8 Challenge[32];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	__xrted25519point NoncePoint;
	bool bResult = false;

	if ( pSignature == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtEd25519ValidateKey(pKey) ||
		 !__xrtEd25519SignMessage(iMode, pMessage, iMessageSize) ) {
		return false;
	}
	if ( !__xrtEd25519HashInit(&Hash, iMode, pContext, iContextSize) ||
		 !xrtSha512Update(&Hash, pKey->Prefix, sizeof(pKey->Prefix)) ||
		 !xrtSha512Update(&Hash, pMessage, iMessageSize) ||
		 !xrtSha512Final(&Hash, NonceHash) ) {
		goto cleanup;
	}
	__xrtEd25519ScalarReduce(Nonce, NonceHash);
	__xrtEd25519PointMultiplyBase(&NoncePoint, Nonce);
	__xrtEd25519PointEncode(Signature, &NoncePoint);

	if ( !__xrtEd25519HashInit(&Hash, iMode, pContext, iContextSize) ||
		 !xrtSha512Update(&Hash, Signature, 32u) ||
		 !xrtSha512Update(&Hash, pKey->Public, sizeof(pKey->Public)) ||
		 !xrtSha512Update(&Hash, pMessage, iMessageSize) ||
		 !xrtSha512Final(&Hash, ChallengeHash) ) {
		goto cleanup;
	}
	__xrtEd25519ScalarReduce(Challenge, ChallengeHash);
	__xrtEd25519ScalarMultiplyAdd(
		Signature + 32u, Nonce, Challenge, pKey->Scalar
	);
	memcpy(pSignature, Signature, sizeof(Signature));
	bResult = true;

cleanup:
	xrtSecureZero(&Hash, sizeof(Hash));
	xrtSecureZero(NonceHash, sizeof(NonceHash));
	xrtSecureZero(Nonce, sizeof(Nonce));
	xrtSecureZero(ChallengeHash, sizeof(ChallengeHash));
	xrtSecureZero(Challenge, sizeof(Challenge));
	xrtSecureZero(Signature, sizeof(Signature));
	xrtSecureZero(&NoncePoint, sizeof(NoncePoint));
	return bResult;
}



/* 使用展开密钥签署纯 Ed25519 消息。 */
XRT_API bool xrtEd25519SignKey(
	const xed25519key* pKey,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
)
{
	return xrtEd25519SignMode(
		pKey, XED25519_PURE, NULL, 0,
		pMessage, iMessageSize, pSignature
	);
}



/* 使用种子签署纯 Ed25519 消息。 */
XRT_API bool xrtEd25519Sign(
	const void* pSeed,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
)
{
	xed25519key Key;
	bool bResult;

	if ( (pSeed == NULL) || (pSignature == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtEd25519KeyInit(&Key, pSeed) ) {
		return false;
	}
	bResult = xrtEd25519SignKey(
		&Key, pMessage, iMessageSize, pSignature
	);
	xrtEd25519KeyClear(&Key);
	return bResult;
}

#endif
