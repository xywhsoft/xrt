#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_DER)

/* 严格解码 DER 后验证任意非空摘要上的 P-256 ECDSA 签名。 */
XRT_API bool xrtEcdsaP256VerifyDer(
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic
)
{
	return __xrtEcdsaVerifyDer(
		XRT_NIST_P256,
		pHash,
		iHashSize,
		pDer,
		iDerSize,
		pPublic,
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		"ecdsa-p256-verify"
	);
}

#endif
