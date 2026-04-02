/*
 * XRT Example - ChaCha20
 * XRT 范例 - ChaCha20 与 Poly1305
 *
 * Description / 说明:
 *   EN: Demonstrates ChaCha20 stream encryption and AEAD round-trip.
 *   CN: 演示 ChaCha20 流加密与 AEAD 往返验证。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_chacha20.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_chacha20 -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	const uint8 arrAAD[] = "chacha-demo";
	const uint8 arrPlain[] = "stream cipher payload";
	uint8 arrKey[32];
	uint8 arrNonce[12];
	uint8 arrStreamCipher[128] = { 0 };
	uint8 arrStreamPlain[128] = { 0 };
	uint8 arrAeadCipher[128] = { 0 };
	uint8 arrAeadPlain[128] = { 0 };
	bool bEnc;
	bool bDec;

	xrtInit();

	memset(arrKey, 0x33, sizeof(arrKey));
	xrtRandomBytes(arrNonce, sizeof(arrNonce));

	xrtChaCha20(arrStreamCipher, arrKey, arrNonce, 1, arrPlain, strlen((str)arrPlain));
	xrtChaCha20(arrStreamPlain, arrKey, arrNonce, 1, arrStreamCipher, strlen((str)arrPlain));

	bEnc = xrtChaCha20Poly1305Encrypt(arrAeadCipher, arrKey, arrNonce, arrAAD, strlen((str)arrAAD), arrPlain, strlen((str)arrPlain));
	bDec = xrtChaCha20Poly1305Decrypt(arrAeadPlain, arrKey, arrNonce, arrAAD, strlen((str)arrAAD), arrAeadCipher, strlen((str)arrPlain) + 16u);

	printf("stream_round_trip = %s\n", exBoolText(exBytesEqual(arrPlain, arrStreamPlain, strlen((str)arrPlain))));
	printf("aead_round_trip   = %s\n", exBoolText(bEnc && bDec && exBytesEqual(arrPlain, arrAeadPlain, strlen((str)arrPlain))));

	xrtUnit();
	return 0;
}
