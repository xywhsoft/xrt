#include "../internal/xrt_crypto_ed25519.h"



#if defined(XRT_FEATURE_CRYPTO_ED25519_KEYPAIR)

/* 生成随机 Ed25519 种子和对应公钥，并保持失败原子性。 */
XRT_API bool xrtEd25519KeyPair(void* pSeed, void* pPublic)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	bool bResult = false;

	if ( (pSeed == NULL) || (pPublic == NULL) ||
		 __xrtCryptoRangesOverlap(
			pSeed, XRT_ED25519_SEED_SIZE,
			pPublic, XRT_ED25519_PUBLIC_SIZE
		 ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSecureRandom(Seed, sizeof(Seed)) ||
		 !xrtEd25519Public(Seed, Public) ) {
		goto cleanup;
	}
	memcpy(pSeed, Seed, sizeof(Seed));
	memcpy(pPublic, Public, sizeof(Public));
	bResult = true;

cleanup:
	xrtSecureZero(Seed, sizeof(Seed));
	xrtSecureZero(Public, sizeof(Public));
	return bResult;
}

#endif
