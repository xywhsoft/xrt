#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN)

/* 使用指定摘要算法的 RFC 6979 路径生成定宽 low-S P-384 签名。 */
XRT_API bool xrtEcdsaP384Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
)
{
	return __xrtEcdsaSign(
		XRT_NIST_P384,
		Hash,
		pHash,
		pPrivate,
		pSignature,
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		"ecdsa-p384-sign"
	);
}

#endif
