#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER)

/* 生成确定性 low-S P-384 ECDSA 签名并编码为规范 DER。 */
XRT_API bool xrtEcdsaP384SignDer(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtEcdsaSignDer(
		XRT_NIST_P384,
		Hash,
		pHash,
		pPrivate,
		pDer,
		iCapacity,
		pSize,
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		"ecdsa-p384-sign"
	);
}

#endif
