#include "../bench_common.h"

#define XRT_MODULE_CRYPTO_AES_GCM
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 递增 96 位 nonce 的低 64 位，保证同一基准密钥下每次调用使用不同 nonce。 */
static void benchAesGcmNonce(uint8* pNonce, uint64 iValue)
{
	for ( size_t i = 0u; i < 8u; i++ ) {
		pNonce[11u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 测量复用密钥状态时的大块 AES-GCM 加密吞吐，并在计时后验证最终结果。 */
int main(int argc, char** argv)
{
	uint32 iCount = xbenchArgU32(argc, argv, 1, 32768u);
	uint32 iSize = xbenchArgU32(argc, argv, 2, 16384u);
	static const uint8 Key[XRT_AES256_KEY_SIZE] = {
		0x6a, 0x4f, 0x2d, 0x19, 0x93, 0xc1, 0x77, 0xe5,
		0x08, 0x64, 0xb2, 0xda, 0x31, 0xaf, 0x55, 0x9c,
		0xf0, 0x43, 0x28, 0x7e, 0xb5, 0x0d, 0x91, 0x66,
		0x3b, 0xcc, 0x14, 0xa8, 0x72, 0xe9, 0x05, 0xdf
	};
	static const uint8 Aad[] = "xrt-aes-gcm-performance";
	xaesgcm State;
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Tag[XRT_AES_GCM_TAG_MAX_SIZE];
	uint8* pPlain;
	uint8* pCipher;
	uint8* pVerify;
	xbenchtimer Timer;
	uint64 iElapsed;
	double fMebibytes;

	if (
		(iCount == 0u) ||
		(iSize == 0u)
	) {
		fprintf(stderr, "invalid AES-GCM benchmark arguments.\n");
		return 1;
	}
	pPlain = (uint8*)malloc(iSize);
	pCipher = (uint8*)malloc(iSize);
	pVerify = (uint8*)malloc(iSize);
	if ( (pPlain == NULL) || (pCipher == NULL) || (pVerify == NULL) ) {
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 2;
	}
	for ( uint32 i = 0u; i < iSize; i++ ) {
		pPlain[i] = (uint8)((i * 29u) + 17u);
	}
	if (
		!xrtAesGcmInit(
			&State,
			Key,
			sizeof(Key),
			XRT_AES_GCM_TAG_DEFAULT_SIZE
		)
	) {
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 3;
	}

	xbenchApplyCpuPinFromEnv();
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iCount; i++ ) {
		benchAesGcmNonce(Nonce, (uint64)i + 1u);
		if (
			!xrtAesGcmEncrypt(
				&State,
				Nonce,
				sizeof(Nonce),
				Aad,
				sizeof(Aad) - 1u,
				pPlain,
				iSize,
				pCipher,
				Tag
			)
		) {
			xrtAesGcmClear(&State);
			free(pPlain);
			free(pCipher);
			free(pVerify);
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	if (
		!xrtAesGcmDecrypt(
			&State,
			Nonce,
			sizeof(Nonce),
			Aad,
			sizeof(Aad) - 1u,
			pCipher,
			iSize,
			Tag,
			pVerify
		) ||
		(memcmp(pPlain, pVerify, iSize) != 0)
	) {
		xrtAesGcmClear(&State);
		free(pPlain);
		free(pCipher);
		free(pVerify);
		return 5;
	}
	fMebibytes =
		((double)iCount * (double)iSize) /
		(1024.0 * 1024.0);
	printf("aes_gcm_operations: %" PRIu32 "\n", iCount);
	printf("aes_gcm_message_bytes: %" PRIu32 "\n", iSize);
	printf("aes_gcm_elapsed_ns: %" PRIu64 "\n", iElapsed);
	printf(
		"aes_gcm_encrypt_mib_per_sec: %.3f\n",
		iElapsed == 0u
			? 0.0
			: fMebibytes / xbenchNsToSec(iElapsed)
	);

	xrtAesGcmClear(&State);
	xrtSecureZero(pPlain, iSize);
	xrtSecureZero(pCipher, iSize);
	xrtSecureZero(pVerify, iSize);
	free(pPlain);
	free(pCipher);
	free(pVerify);
	return 0;
}
