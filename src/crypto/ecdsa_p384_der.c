#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_DER)

/* 严格解码 DER 后验证任意非空摘要上的 P-384 ECDSA 签名。 */
XRT_API bool xrtEcdsaP384VerifyDer(
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic
)
{
	return __xrtEcdsaVerifyDer(
		XRT_NIST_P384,
		pHash,
		iHashSize,
		pDer,
		iDerSize,
		pPublic,
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		"ecdsa-p384-verify"
	);
}

#endif
