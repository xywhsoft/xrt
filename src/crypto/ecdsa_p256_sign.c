#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)

/* 使用指定摘要算法的 RFC 6979 路径生成定宽 low-S P-256 签名。 */
XRT_API bool xrtEcdsaP256Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
)
{
	return __xrtEcdsaSign(
		XRT_NIST_P256,
		Hash,
		pHash,
		pPrivate,
		pSignature,
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		"ecdsa-p256-sign"
	);
}

#endif
