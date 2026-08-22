#include "../internal/xrt_crypto_ecdsa.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER)

#define XRT_ECDSA_VERIFY_DER_RAW_MAX_SIZE 96u



/* 严格解码 DER 后调用固定曲线的 raw ECDSA 验证层。 */
bool __xrtEcdsaVerifyDer(
	int iCurve,
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
)
{
	uint8 Signature[XRT_ECDSA_VERIFY_DER_RAW_MAX_SIZE];
	bool bResult;

	if ( (pHash == NULL) || (iHashSize == 0) ||
		(pDer == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtEcdsaDerDecode(pDer, iDerSize, Signature, iScalarSize) ) {
		return false;
	}
	bResult = __xrtEcdsaVerify(
		iCurve,
		pHash,
		iHashSize,
		Signature,
		pPublic,
		iScalarSize,
		iPublicSize,
		sOperation
	);
	xrtSecureZero(Signature, sizeof(Signature));
	return bResult;
}

#endif
