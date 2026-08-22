#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN)

typedef struct xrt_ecdsa_nonce_state {
	uint8 Key[XRT_SHA512_SIZE];
	uint8 Value[XRT_SHA512_SIZE];
	size_t HmacSize;
	size_t ScalarSize;
	xrt_ecdsa_hmac_fn Hmac;
} xrt_ecdsa_nonce_state;



/* 选择当前裁剪构建中可用的 RFC 6979 HMAC 后端。 */
static bool __xrtEcdsaNonceAlgorithm(
	xcryptohash Hash,
	xrt_ecdsa_hmac_fn* pHmac,
	size_t* pHmacSize
)
{
	(void)pHmac;
	(void)pHmacSize;
	switch ( Hash ) {
		#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
		case XCRYPTO_HASH_SHA256:
			*pHmac = xrtHmacSha256;
			*pHmacSize = XRT_SHA256_SIZE;
			return true;
		#endif

		#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)
		case XCRYPTO_HASH_SHA384:
			*pHmac = xrtHmacSha384;
			*pHmacSize = XRT_SHA384_SIZE;
			return true;
		case XCRYPTO_HASH_SHA512:
			*pHmac = xrtHmacSha512;
			*pHmacSize = XRT_SHA512_SIZE;
			return true;
		#endif

		default:
			return false;
	}
}



/* 完成一次 HMAC，并只在成功后发布新的状态字节。 */
static bool __xrtEcdsaNonceHmac(
	xrt_ecdsa_nonce_state* pState,
	const void* pData,
	size_t iDataSize,
	uint8* pOutput
)
{
	uint8 Next[XRT_SHA512_SIZE];
	bool bResult = pState->Hmac(
		pState->Key, pState->HmacSize, pData, iDataSize, Next
	);

	if ( bResult ) {
		memcpy(pOutput, Next, pState->HmacSize);
	}
	xrtSecureZero(Next, sizeof(Next));
	return bResult;
}



/* 按 RFC 6979 3.2 初始化确定性 nonce 状态。 */
static bool __xrtEcdsaNonceInit(
	xrt_ecdsa_nonce_state* pState,
	const uint8* pPrivate,
	const uint8* pHash,
	size_t iScalarSize,
	xrt_ecdsa_hmac_fn pHmac,
	size_t iHmacSize
)
{
	uint8 Data[XRT_SHA512_SIZE + (XRT_ECDSA_SCALAR_MAX_SIZE * 2u) + 1u];
	size_t iPrefixSize;
	bool bResult = false;

	memset(pState, 0, sizeof(*pState));
	pState->HmacSize = iHmacSize;
	pState->ScalarSize = iScalarSize;
	pState->Hmac = pHmac;
	memset(pState->Value, 0x01, iHmacSize);
	iPrefixSize = iHmacSize + 1u;

	memcpy(Data, pState->Value, iHmacSize);
	Data[iHmacSize] = 0;
	memcpy(Data + iPrefixSize, pPrivate, iScalarSize);
	memcpy(Data + iPrefixSize + iScalarSize, pHash, iScalarSize);
	if ( !__xrtEcdsaNonceHmac(
		pState, Data, iPrefixSize + (iScalarSize * 2u), pState->Key
	) || !__xrtEcdsaNonceHmac(
		pState, pState->Value, iHmacSize, pState->Value
	) ) {
		goto cleanup;
	}

	memcpy(Data, pState->Value, iHmacSize);
	Data[iHmacSize] = 1;
	memcpy(Data + iPrefixSize, pPrivate, iScalarSize);
	memcpy(Data + iPrefixSize + iScalarSize, pHash, iScalarSize);
	if ( !__xrtEcdsaNonceHmac(
		pState, Data, iPrefixSize + (iScalarSize * 2u), pState->Key
	) || !__xrtEcdsaNonceHmac(
		pState, pState->Value, iHmacSize, pState->Value
	) ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(Data, sizeof(Data));
	return bResult;
}



/* 按 RFC 6979 拒绝步骤推进到下一条候选序列。 */
static bool __xrtEcdsaNonceReject(xrt_ecdsa_nonce_state* pState)
{
	uint8 Data[XRT_SHA512_SIZE + 1u];
	bool bResult;

	memcpy(Data, pState->Value, pState->HmacSize);
	Data[pState->HmacSize] = 0;
	bResult = __xrtEcdsaNonceHmac(
		pState, Data, pState->HmacSize + 1u, pState->Key
	) && __xrtEcdsaNonceHmac(
		pState, pState->Value, pState->HmacSize, pState->Value
	);
	xrtSecureZero(Data, sizeof(Data));
	return bResult;
}



/* 生成下一条严格位于 [1, n) 的 RFC 6979 候选标量。 */
static bool __xrtEcdsaNonceNext(
	xrt_ecdsa_nonce_state* pState,
	const uint8* pOrder,
	uint8* pCandidate
)
{
	for ( ;; ) {
		size_t iOffset = 0;

		while ( iOffset < pState->ScalarSize ) {
			size_t iCopy;

			if ( !__xrtEcdsaNonceHmac(
				pState, pState->Value,
				pState->HmacSize, pState->Value
			) ) {
				return false;
			}
			iCopy = pState->ScalarSize - iOffset;
			if ( iCopy > pState->HmacSize ) {
				iCopy = pState->HmacSize;
			}
			memcpy(pCandidate + iOffset, pState->Value, iCopy);
			iOffset += iCopy;
		}
		if ( __xrtNistScalarValid(
			pCandidate, pOrder, pState->ScalarSize
		) != 0 ) {
			return true;
		}
		if ( !__xrtEcdsaNonceReject(pState) ) {
			return false;
		}
	}
}



/* 把签名 s 规范化到群阶下半区，消除 ECDSA 可延展性。 */
static void __xrtEcdsaLowS(
	uint32* pS,
	const uint32* pOrder,
	uint32* pHalf,
	uint32* pDifference,
	uint32* pTemporary
)
{
	size_t iIntegerSize = ((pOrder[0] + 63u) >> 5u) * sizeof(uint32);
	uint32 iBorrow;
	uint32 iGreater;

	memcpy(pHalf, pOrder, iIntegerSize);
	__xrtI31RightShift(pHalf, 1);
	memcpy(pDifference, pS, iIntegerSize);
	iBorrow = __xrtI31Subtract(pDifference, pHalf, 1);
	iGreater = __xrtI31Not(iBorrow) &
		__xrtI31Not(__xrtI31IsZero(pDifference));
	memcpy(pTemporary, pOrder, iIntegerSize);
	(void)__xrtI31Subtract(pTemporary, pS, 1);
	__xrtI31Copy(iGreater, pS, pTemporary, iIntegerSize);
}



/* 使用 RFC 6979 nonce 生成固定曲线的 low-S raw ECDSA 签名。 */
bool __xrtEcdsaSign(
	int iCurve,
	xcryptohash HashAlgorithm,
	const void* pHash,
	const void* pPrivate,
	void* pSignature,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
)
{
	const uint8* pOrderBytes;
	const uint32* pSquare;
	xrt_ecdsa_nonce_state Nonce;
	uint32 Values[11][XRT_ECDSA_I31_MAX_WORDS] = { { 0 } };
	uint32* pOrder = Values[0];
	uint32* pPrivateValue = Values[1];
	uint32* pHashValue = Values[2];
	uint32* pNonceValue = Values[3];
	uint32* pR = Values[4];
	uint32* pS = Values[5];
	uint32* pProduct = Values[6];
	uint32* pTemporaryLeft = Values[7];
	uint32* pTemporaryRight = Values[8];
	uint32* pHalf = Values[9];
	uint32* pDifference = Values[10];
	uint8 Hash[XRT_ECDSA_SCALAR_MAX_SIZE];
	uint8 Private[XRT_ECDSA_SCALAR_MAX_SIZE];
	uint8 ReducedHash[XRT_ECDSA_SCALAR_MAX_SIZE];
	uint8 Candidate[XRT_ECDSA_SCALAR_MAX_SIZE];
	uint8 Exponent[XRT_ECDSA_SCALAR_MAX_SIZE];
	uint8 Point[XRT_ECDSA_PUBLIC_MAX_SIZE];
	uint8 Signature[XRT_ECDSA_SCALAR_MAX_SIZE * 2u];
	xrt_ecdsa_hmac_fn pHmac = NULL;
	size_t iHashSize;
	size_t iHmacSize = 0;
	uint32 iModulusInverse;
	bool bResult = false;

	iHashSize = xrtCryptoHashSize(HashAlgorithm);
	if ( (pHash == NULL) || (pPrivate == NULL) || (pSignature == NULL) ||
		(iHashSize == 0) ||
		((iCurve == XRT_NIST_P256) &&
		 ((iScalarSize != 32u) || (iPublicSize != 65u))) ||
		((iCurve == XRT_NIST_P384) &&
		 ((iScalarSize != 48u) || (iPublicSize != 97u))) ||
		((iCurve != XRT_NIST_P256) && (iCurve != XRT_NIST_P384)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtEcdsaNonceAlgorithm(
		HashAlgorithm, &pHmac, &iHmacSize
	) ) {
		__xrtErrorSetUnsupported();
		return false;
	}
	memset(Hash, 0, iScalarSize);
	if ( iHashSize >= iScalarSize ) {
		memcpy(Hash, pHash, iScalarSize);
	} else {
		memcpy(Hash + (iScalarSize - iHashSize), pHash, iHashSize);
	}
	memcpy(Private, pPrivate, iScalarSize);
	pOrderBytes = __xrtNistOrder(iCurve, NULL);
	pSquare = __xrtEcdsaSquare(iCurve);
	__xrtI31Decode(pOrder, pOrderBytes, iScalarSize);
	if ( (__xrtI31DecodeMod(
		pPrivateValue, Private, iScalarSize, pOrder
	) == 0) || (__xrtI31IsZero(pPrivateValue) != 0) ) {
		__xrtNistError(
			sOperation, "the ECDSA private key is invalid", XCRYPTO_ERROR_KEY
		);
		goto cleanup;
	}

	/* RFC 6979 的 bits2octets 与 ECDSA 的 z 使用同一份群阶归约摘要。 */
	__xrtI31Decode(pHashValue, Hash, iScalarSize);
	pHashValue[0] = pOrder[0];
	__xrtEcdsaReduce(pHashValue, pOrder);
	__xrtI31Encode(ReducedHash, iScalarSize, pHashValue);
	if ( !__xrtEcdsaNonceInit(
		&Nonce, Private, ReducedHash, iScalarSize, pHmac, iHmacSize
	) ) {
		goto cleanup;
	}
	memcpy(Exponent, pOrderBytes, iScalarSize);
	Exponent[iScalarSize - 1u] -= 2u;
	iModulusInverse = __xrtI31NegativeInverse(pOrder[1]);

	for ( ;; ) {
		if ( !__xrtEcdsaNonceNext(&Nonce, pOrderBytes, Candidate) ) {
			goto cleanup;
		}
		(void)__xrtI31DecodeMod(
			pNonceValue, Candidate, iScalarSize, pOrder
		);
		if ( __xrtNistPointMultiplyBase(
			iCurve, Point, Candidate, iScalarSize
		) != iPublicSize ) {
			__xrtEcdsaError(sOperation, "ECDSA nonce point derivation failed");
			goto cleanup;
		}
		__xrtI31Decode(pR, Point + 1, iScalarSize);
		pR[0] = pOrder[0];
		__xrtEcdsaReduce(pR, pOrder);
		if ( __xrtI31IsZero(pR) != 0 ) {
			if ( !__xrtEcdsaNonceReject(&Nonce) ) {
				goto cleanup;
			}
			continue;
		}

		/* s = k^-1 * (z + r*d) mod n。 */
		__xrtEcdsaMultiply(
			pProduct,
			pR,
			pPrivateValue,
			pOrder,
			pSquare,
			iModulusInverse,
			pTemporaryLeft
		);
		__xrtEcdsaAdd(pProduct, pHashValue, pOrder);
		memcpy(
			pS,
			pNonceValue,
			((pOrder[0] + 63u) >> 5u) * sizeof(uint32)
		);
		__xrtI31ModPower(
			pS,
			Exponent,
			iScalarSize,
			pOrder,
			pSquare,
			iModulusInverse,
			pTemporaryLeft,
			pTemporaryRight
		);
		__xrtEcdsaMultiply(
			pS,
			pS,
			pProduct,
			pOrder,
			pSquare,
			iModulusInverse,
			pTemporaryLeft
		);
		if ( __xrtI31IsZero(pS) != 0 ) {
			if ( !__xrtEcdsaNonceReject(&Nonce) ) {
				goto cleanup;
			}
			continue;
		}

		__xrtEcdsaLowS(
			pS, pOrder, pHalf, pDifference, pTemporaryRight
		);
		__xrtI31Encode(Signature, iScalarSize, pR);
		__xrtI31Encode(Signature + iScalarSize, iScalarSize, pS);
		memcpy(pSignature, Signature, iScalarSize * 2u);
		bResult = true;
		break;
	}

cleanup:
	xrtSecureZero(Signature, sizeof(Signature));
	xrtSecureZero(Point, sizeof(Point));
	xrtSecureZero(Exponent, sizeof(Exponent));
	xrtSecureZero(Candidate, sizeof(Candidate));
	xrtSecureZero(ReducedHash, sizeof(ReducedHash));
	xrtSecureZero(Private, sizeof(Private));
	xrtSecureZero(Hash, sizeof(Hash));
	xrtSecureZero(Values, sizeof(Values));
	xrtSecureZero(&Nonce, sizeof(Nonce));
	return bResult;
}

#endif
