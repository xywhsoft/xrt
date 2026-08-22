#ifndef XRT_INTERNAL_CRYPTO_RSA_PKCS1_H
#define XRT_INTERNAL_CRYPTO_RSA_PKCS1_H

#include "xrt_crypto_rsa.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1)

/* 选择摘要对应的规范 DigestInfo 前缀和长度。 */
bool __xrtRsaDigestInfo(
	xcryptohash iHash,
	const uint8** pPrefix,
	size_t* pPrefixSize,
	size_t* pHashSize
);

#endif

#endif
