#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR)

/* 使用操作系统安全随机源生成 P-256 私钥和未压缩公钥。 */
XRT_API bool xrtP256KeyPair(void* pPrivate, void* pPublic)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistKeyPairApi(
		XRT_NIST_P256,
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		pPrivate,
		pPublic,
		"p256-keypair"
	);
}

#endif
