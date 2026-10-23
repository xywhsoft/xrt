#include "../internal/xrt_crypto_ecdsa.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER)

#define XRT_ECDSA_VERIFY_DER_RAW_MAX_SIZE 96u



/* 严格解码 DER 后调用固定曲线的 raw ECDSA 验证层。 */
xrt_crypto_check __xrtEcdsaCheckDer(
	int iCurve,
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic,
	size_t iScalarSize,
	size_t iPublicSize
)
{
	uint8 Signature[XRT_ECDSA_VERIFY_DER_RAW_MAX_SIZE];
	xrt_crypto_check Result;

	if ( (pHash == NULL) || (iHashSize == 0) ||
		(pDer == NULL) || (pPublic == NULL) ||
		(iScalarSize > sizeof(Signature) / 2u) ) {
		return XRT_CRYPTO_CHECK_ARGUMENT;
	}
	Result = __xrtEcdsaDerDecodeCheck(pDer, iDerSize, Signature, iScalarSize);
	if ( Result != XRT_CRYPTO_CHECK_VALID ) {
		return Result;
	}
	Result = __xrtEcdsaCheck(
		iCurve,
		pHash,
		iHashSize,
		Signature,
		pPublic,
		iScalarSize,
		iPublicSize
	);
	xrtSecureZero(Signature, sizeof(Signature));
	return Result;
}

bool __xrtEcdsaVerifyDer(
	int iCurve, const void* pHash, size_t iHashSize,
	const void* pDer, size_t iDerSize, const void* pPublic,
	size_t iScalarSize, size_t iPublicSize, cstr sOperation
)
{
	xrt_crypto_check Result = __xrtEcdsaCheckDer(
		iCurve, pHash, iHashSize, pDer, iDerSize, pPublic, iScalarSize, iPublicSize
	);

	if ( Result == XRT_CRYPTO_CHECK_VALID ) {
		return true;
	}
	if ( Result == XRT_CRYPTO_CHECK_ARGUMENT ) {
		__xrtErrorSetInvalidArgument();
	} else if ( Result != XRT_CRYPTO_CHECK_ERROR ) {
		__xrtEcdsaError(sOperation, "the ECDSA signature verification failed");
	}
	return false;
}

#endif
