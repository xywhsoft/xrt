#include "../internal/xrt_crypto_ecdsa.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER)

/* 生成固定曲线的确定性签名并编码为规范 DER。 */
bool __xrtEcdsaSignDer(
	int iCurve,
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
)
{
	uint8 Signature[XRT_ECDSA_SCALAR_MAX_SIZE * 2u];
	bool bResult;

	if ( pSize == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bResult = __xrtEcdsaSign(
		iCurve,
		Hash,
		pHash,
		pPrivate,
		Signature,
		iScalarSize,
		iPublicSize,
		sOperation
	) && xrtEcdsaDerEncode(
		Signature, iScalarSize, pDer, iCapacity, pSize
	);
	xrtSecureZero(Signature, sizeof(Signature));
	return bResult;
}

#endif
