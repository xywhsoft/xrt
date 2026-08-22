#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256)

/* 验证任意非空摘要上的定宽 P-256 ECDSA raw r||s 签名。 */
XRT_API bool xrtEcdsaP256Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
)
{
	return __xrtEcdsaVerify(
		XRT_NIST_P256,
		pHash,
		iHashSize,
		pSignature,
		pPublic,
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		"ecdsa-p256-verify"
	);
}

#endif
