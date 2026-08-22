#include "../internal/xrt_crypto_ed25519.h"



#if defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY)

/* 验证消息指针和预哈希模式的固定输入长度。 */
static bool __xrtEd25519VerifyMessage(
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



/* 严格验证 RFC 8032 指定模式的签名。 */
XRT_API bool xrtEd25519VerifyMode(
	const void* pPublic,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
)
{
	const uint8* pPublicBytes = (const uint8*)pPublic;
	const uint8* pSignatureBytes = (const uint8*)pSignature;
	xsha512 Hash;
	uint8 ChallengeHash[XRT_SHA512_SIZE];
	uint8 Challenge[32];
	uint8 Left[32];
	uint8 Right[32];
	__xrted25519point Public;
	__xrted25519point Nonce;
	__xrted25519point LeftPoint;
	__xrted25519point Product;
	__xrted25519point RightPoint;
	bool bResult = false;

	if ( (pPublic == NULL) || (pSignature == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtEd25519VerifyMessage(iMode, pMessage, iMessageSize) ||
		 !__xrtEd25519HashInit(&Hash, iMode, pContext, iContextSize) ) {
		return false;
	}
	if ( !__xrtEd25519PointDecode(&Public, pPublicBytes) ||
		 __xrtEd25519PointIdentity(&Public) ||
		 !__xrtEd25519PointMainSubgroup(&Public) ) {
		__xrtEd25519Error(
			"ed25519-verify", "the Ed25519 public key is not a canonical main-subgroup point",
			XCRYPTO_ERROR_KEY
		);
		goto cleanup;
	}
	if ( !__xrtEd25519ScalarCanonical(pSignatureBytes + 32u) ||
		 !__xrtEd25519PointDecode(&Nonce, pSignatureBytes) ) {
		__xrtEd25519Error(
			"ed25519-verify", "the Ed25519 signature encoding is invalid",
			XCRYPTO_ERROR_SIGNATURE
		);
		goto cleanup;
	}
	if ( !xrtSha512Update(&Hash, pSignatureBytes, 32u) ||
		 !xrtSha512Update(&Hash, pPublicBytes, XRT_ED25519_PUBLIC_SIZE) ||
		 !xrtSha512Update(&Hash, pMessage, iMessageSize) ||
		 !xrtSha512Final(&Hash, ChallengeHash) ) {
		goto cleanup;
	}
	__xrtEd25519ScalarReduce(Challenge, ChallengeHash);
	__xrtEd25519PointMultiplyBase(&LeftPoint, pSignatureBytes + 32u);
	__xrtEd25519PointMultiplyPublic(&Product, &Public, Challenge);
	__xrtEd25519PointAdd(&RightPoint, &Nonce, &Product);
	__xrtEd25519PointEncode(Left, &LeftPoint);
	__xrtEd25519PointEncode(Right, &RightPoint);
	bResult = xrtConstTimeEqual(Left, Right, sizeof(Left));
	if ( !bResult ) {
		__xrtEd25519Error(
			"ed25519-verify", "the Ed25519 signature does not match the message and public key",
			XCRYPTO_ERROR_SIGNATURE
		);
	}

cleanup:
	xrtSecureZero(&Hash, sizeof(Hash));
	xrtSecureZero(ChallengeHash, sizeof(ChallengeHash));
	xrtSecureZero(Challenge, sizeof(Challenge));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	xrtSecureZero(&Public, sizeof(Public));
	xrtSecureZero(&Nonce, sizeof(Nonce));
	xrtSecureZero(&LeftPoint, sizeof(LeftPoint));
	xrtSecureZero(&Product, sizeof(Product));
	xrtSecureZero(&RightPoint, sizeof(RightPoint));
	return bResult;
}



/* 严格验证纯 Ed25519 签名。 */
XRT_API bool xrtEd25519Verify(
	const void* pPublic,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
)
{
	return xrtEd25519VerifyMode(
		pPublic, XED25519_PURE, NULL, 0,
		pMessage, iMessageSize, pSignature
	);
}

#endif
