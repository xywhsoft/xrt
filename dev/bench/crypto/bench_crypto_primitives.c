#include "../bench_common.h"

#define XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#define XRT_MODULE_CRYPTO_X25519
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 递增 96 位 nonce 的低 64 位，避免同一密钥下重复使用 nonce。 */
static void benchCryptoNonce(uint8* pNonce, uint64 iValue)
{
	for ( size_t i = 0u; i < 8u; i++ ) {
		pNonce[11u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 测量常用哈希、AEAD 和密钥协商公开路径，并在计时后验证最终结果。 */
int main(int argc, char** argv)
{
	uint32 iBlockCount = xbenchArgU32(argc, argv, 1, 32768u);
	uint32 iBlockSize = xbenchArgU32(argc, argv, 2, 16384u);
	uint32 iX25519Count = xbenchArgU32(argc, argv, 3, 10000u);
	static const uint8 Key[XRT_CHACHA20_KEY_SIZE] = {
		0x91, 0x43, 0xb7, 0x2d, 0xe6, 0x05, 0x68, 0xfa,
		0x3c, 0xd1, 0x7e, 0x89, 0x22, 0xac, 0x50, 0x6b,
		0x7d, 0xe8, 0x31, 0x14, 0xc9, 0x5f, 0xa2, 0x76,
		0x08, 0xbb, 0x64, 0xdf, 0x39, 0x10, 0x85, 0xce
	};
	static const uint8 Aad[] = "xrt-crypto-performance";
	uint8 Private[XRT_X25519_PRIVATE_SIZE];
	uint8 PeerPrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 PeerPublic[XRT_X25519_PUBLIC_SIZE];
	uint8 Shared[XRT_X25519_SHARED_SIZE];
	uint8 Digest[XRT_SHA256_SIZE];
	uint8 Nonce[XRT_CHACHA20_NONCE_SIZE] = { 0 };
	uint8 Tag[XRT_POLY1305_TAG_SIZE];
	uint8* pPlain;
	uint8* pCipher;
	uint8* pVerify;
	xbenchtimer Timer;
	uint64 iShaElapsed;
	uint64 iAeadElapsed;
	uint64 iX25519Elapsed;
	double fMebibytes;

	if (
		(iBlockCount == 0u) ||
		(iBlockSize == 0u) ||
		(iX25519Count == 0u)
	) {
		fprintf(stderr, "invalid crypto benchmark arguments.\n");
		return 1;
	}
	pPlain = (uint8*)malloc(iBlockSize);
	pCipher = (uint8*)malloc(iBlockSize);
	pVerify = (uint8*)malloc(iBlockSize);
	if ( (pPlain == NULL) || (pCipher == NULL) || (pVerify == NULL) ) {
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 2;
	}
	for ( uint32 i = 0u; i < iBlockSize; i++ ) {
		pPlain[i] = (uint8)((i * 37u) + 11u);
	}
	for ( size_t i = 0u; i < sizeof(Private); i++ ) {
		Private[i] = (uint8)((i * 13u) + 7u);
		PeerPrivate[i] = (uint8)((i * 29u) + 19u);
	}
	if ( !xrtX25519Public(PeerPrivate, PeerPublic) ) {
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 3;
	}

	xbenchApplyCpuPinFromEnv();
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iBlockCount; i++ ) {
		pPlain[0] = (uint8)i;
		if ( !xrtSha256(pPlain, iBlockSize, Digest) ) {
			free(pPlain);
			free(pCipher);
			free(pVerify);
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iShaElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iBlockCount; i++ ) {
		benchCryptoNonce(Nonce, (uint64)i + 1u);
		if ( !xrtChaCha20Poly1305Encrypt(
			Key,
			Nonce,
			Aad,
			sizeof(Aad) - 1u,
			pPlain,
			iBlockSize,
			pCipher,
			Tag
		) ) {
			free(pPlain);
			free(pCipher);
			free(pVerify);
			return 5;
		}
	}
	xbenchTimerStop(&Timer);
	iAeadElapsed = xbenchTimerElapsedNs(&Timer);
	if (
		!xrtChaCha20Poly1305Decrypt(
			Key,
			Nonce,
			Aad,
			sizeof(Aad) - 1u,
			pCipher,
			iBlockSize,
			Tag,
			pVerify
		) ||
		(memcmp(pPlain, pVerify, iBlockSize) != 0)
	) {
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 6;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iX25519Count; i++ ) {
		Private[0] = (uint8)i;
		if ( !xrtX25519Shared(Private, PeerPublic, Shared) ) {
			free(pPlain);
			free(pCipher);
			free(pVerify);
			return 7;
		}
	}
	xbenchTimerStop(&Timer);
	iX25519Elapsed = xbenchTimerElapsedNs(&Timer);

	fMebibytes =
		((double)iBlockCount * (double)iBlockSize) /
		(1024.0 * 1024.0);
	printf(
		"sha256_mib_per_sec: %.3f\n",
		iShaElapsed == 0u
			? 0.0
			: fMebibytes / xbenchNsToSec(iShaElapsed)
	);
	printf(
		"chacha20_poly1305_encrypt_mib_per_sec: %.3f\n",
		iAeadElapsed == 0u
			? 0.0
			: fMebibytes / xbenchNsToSec(iAeadElapsed)
	);
	printf(
		"x25519_shared_ops_per_sec: %.3f\n",
		xbenchSafeRate(iX25519Count, iX25519Elapsed)
	);

	xrtSecureZero(Private, sizeof(Private));
	xrtSecureZero(PeerPrivate, sizeof(PeerPrivate));
	xrtSecureZero(Shared, sizeof(Shared));
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(pPlain, iBlockSize);
	xrtSecureZero(pCipher, iBlockSize);
	xrtSecureZero(pVerify, iBlockSize);
	free(pPlain);
	free(pCipher);
	free(pVerify);
	return 0;
}
