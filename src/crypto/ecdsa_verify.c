#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY)

/* 清除 ECDSA 验证过程中的全部临时标量与点。 */
static void __xrtEcdsaVerifyClear(
	uint32 Values[][XRT_ECDSA_I31_MAX_WORDS],
	size_t iValueCount,
	uint8* pBytes,
	size_t iByteCount
)
{
	xrtSecureZero(
		Values, iValueCount * XRT_ECDSA_I31_MAX_WORDS * sizeof(uint32)
	);
	xrtSecureZero(pBytes, iByteCount);
}



/* 验证固定 NIST 曲线和任意非空摘要上的 raw r||s 签名。 */
bool __xrtEcdsaVerify(
	int iCurve,
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
)
{
	const uint8* pRaw = (const uint8*)pSignature;
	const uint8* pOrderBytes;
	const uint32* pSquare;
	uint32 Values[9][XRT_ECDSA_I31_MAX_WORDS] = { { 0 } };
	uint32* pOrder = Values[0];
	uint32* pR = Values[1];
	uint32* pS = Values[2];
	uint32* pHashValue = Values[3];
	uint32* pU1 = Values[4];
	uint32* pU2 = Values[5];
	uint32* pTemporaryLeft = Values[6];
	uint32* pTemporaryRight = Values[7];
	uint32* pX = Values[8];
	uint8 Bytes[XRT_ECDSA_PUBLIC_MAX_SIZE +
		(XRT_ECDSA_SCALAR_MAX_SIZE * 4u)];
	uint8* pPoint = Bytes;
	uint8* pU1Bytes = pPoint + iPublicSize;
	uint8* pU2Bytes = pU1Bytes + iScalarSize;
	uint8* pExponent = pU2Bytes + iScalarSize;
	uint8* pReducedX = pExponent + iScalarSize;
	uint32 iModulusInverse;
	uint32 iValid;
	bool bResult;

	if ( (pHash == NULL) || (iHashSize == 0) ||
		(pSignature == NULL) || (pPublic == NULL) ||
		((iCurve == XRT_NIST_P256) &&
		 ((iScalarSize != 32u) || (iPublicSize != 65u))) ||
		((iCurve == XRT_NIST_P384) &&
		 ((iScalarSize != 48u) || (iPublicSize != 97u))) ||
		((iCurve != XRT_NIST_P256) && (iCurve != XRT_NIST_P384)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pOrderBytes = __xrtNistOrder(iCurve, NULL);
	pSquare = __xrtEcdsaSquare(iCurve);
	__xrtI31Decode(pOrder, pOrderBytes, iScalarSize);
	iValid = __xrtI31DecodeMod(pR, pRaw, iScalarSize, pOrder);
	iValid &= __xrtI31DecodeMod(
		pS, pRaw + iScalarSize, iScalarSize, pOrder
	);
	iValid &= __xrtI31Not(__xrtI31IsZero(pR));
	iValid &= __xrtI31Not(__xrtI31IsZero(pS));
	iValid &= __xrtNistPointValid(iCurve, pPublic, iPublicSize);
	if ( iValid == 0 ) {
		__xrtEcdsaVerifyClear(Values, 9, Bytes, sizeof(Bytes));
		__xrtEcdsaError(sOperation, "the ECDSA signature or public key is invalid");
		return false;
	}

	/* ECDSA bits2int 取摘要左侧 qlen 位，短摘要按大端整数左补零。 */
	memset(pReducedX, 0, iScalarSize);
	if ( iHashSize >= iScalarSize ) {
		memcpy(pReducedX, pHash, iScalarSize);
	} else {
		memcpy(
			pReducedX + (iScalarSize - iHashSize), pHash, iHashSize
		);
	}

	/* s^-1、z/s 与 r/s 全部在曲线群阶上计算。 */
	__xrtI31Decode(pHashValue, pReducedX, iScalarSize);
	pHashValue[0] = pOrder[0];
	__xrtEcdsaReduce(pHashValue, pOrder);
	memcpy(pExponent, pOrderBytes, iScalarSize);
	pExponent[iScalarSize - 1u] -= 2u;
	iModulusInverse = __xrtI31NegativeInverse(pOrder[1]);
	__xrtI31ModPower(
		pS,
		pExponent,
		iScalarSize,
		pOrder,
		pSquare,
		iModulusInverse,
		pTemporaryLeft,
		pTemporaryRight
	);
	__xrtEcdsaMultiply(
		pU1,
		pHashValue,
		pS,
		pOrder,
		pSquare,
		iModulusInverse,
		pTemporaryLeft
	);
	__xrtEcdsaMultiply(
		pU2,
		pR,
		pS,
		pOrder,
		pSquare,
		iModulusInverse,
		pTemporaryLeft
	);
	__xrtI31Encode(pU1Bytes, iScalarSize, pU1);
	__xrtI31Encode(pU2Bytes, iScalarSize, pU2);

	/* 当 z 为零时只计算 u2*Q，避免双标量接口接收零标量。 */
	memcpy(pPoint, pPublic, iPublicSize);
	if ( __xrtI31IsZero(pU1) != 0 ) {
		iValid = __xrtNistPointMultiply(
			iCurve, pPoint, iPublicSize, pU2Bytes, iScalarSize
		);
	} else {
		iValid = __xrtNistPointMultiplyAdd(
			iCurve,
			pPoint,
			NULL,
			iPublicSize,
			pU2Bytes,
			iScalarSize,
			pU1Bytes,
			iScalarSize
		);
	}

	/* 比较 (u1*G + u2*Q).x mod n 与签名 r。 */
	if ( iValid != 0 ) {
		__xrtI31Decode(pX, pPoint + 1, iScalarSize);
		pX[0] = pOrder[0];
		__xrtEcdsaReduce(pX, pOrder);
		__xrtI31Encode(pReducedX, iScalarSize, pX);
		bResult = xrtConstTimeEqual(pReducedX, pRaw, iScalarSize);
	} else {
		bResult = false;
	}
	__xrtEcdsaVerifyClear(Values, 9, Bytes, sizeof(Bytes));
	if ( !bResult ) {
		__xrtEcdsaError(sOperation, "the ECDSA signature verification failed");
	}
	return bResult;
}

#endif
