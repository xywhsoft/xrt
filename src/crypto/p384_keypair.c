#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_P384_KEYPAIR)

/* 使用操作系统安全随机源生成 P-384 私钥和未压缩公钥。 */
XRT_API bool xrtP384KeyPair(void* pPrivate, void* pPublic)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistKeyPairApi(
		XRT_NIST_P384,
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		pPrivate,
		pPublic,
		"p384-keypair"
	);
}

#endif
