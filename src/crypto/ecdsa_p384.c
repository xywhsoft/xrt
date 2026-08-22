#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384)

/* 验证任意非空摘要上的定宽 P-384 ECDSA raw r||s 签名。 */
XRT_API bool xrtEcdsaP384Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
)
{
	return __xrtEcdsaVerify(
		XRT_NIST_P384,
		pHash,
		iHashSize,
		pSignature,
		pPublic,
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		"ecdsa-p384-verify"
	);
}

#endif
