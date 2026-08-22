#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR)

/* 使用安全随机源生成规范化私钥，并在成功后一次发布两个输出。 */
XRT_API bool xrtX25519KeyPair(void* pPrivate, void* pPublic)
{
	uint8 Private[XRT_X25519_PRIVATE_SIZE];
	uint8 Public[XRT_X25519_PUBLIC_SIZE];

	if ( (pPrivate == NULL) || (pPublic == NULL) ||
		 __xrtCryptoRangesOverlap(
			pPrivate, sizeof(Private), pPublic, sizeof(Public)
		 ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSecureRandom(Private, sizeof(Private)) ) {
		return false;
	}
	Private[0] &= 248u;
	Private[31] &= 127u;
	Private[31] |= 64u;
	if ( !xrtX25519Public(Private, Public) ) {
		xrtSecureZero(Private, sizeof(Private));
		xrtSecureZero(Public, sizeof(Public));
		return false;
	}

	memcpy(pPrivate, Private, sizeof(Private));
	memcpy(pPublic, Public, sizeof(Public));
	xrtSecureZero(Private, sizeof(Private));
	xrtSecureZero(Public, sizeof(Public));
	return true;
}

#endif
