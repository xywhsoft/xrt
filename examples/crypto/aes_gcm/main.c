/*
 * XRT Example - AES GCM
 * XRT 范例 - AES GCM 加解密
 *
 * Description / 说明:
 *   EN: Demonstrates AES-128-GCM and AES-256-GCM round-trip encryption.
 *   CN: 演示 AES-128-GCM 与 AES-256-GCM 的往返加解密。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_aes_gcm.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_aes_gcm -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	const uint8 arrAAD[] = "aes-gcm-demo";
	const uint8 arrPlain[] = "confidential payload";
	uint8 arrKey128[16] = { 0 };
	uint8 arrKey256[32] = { 0 };
	uint8 arrNonce[12];
	uint8 arrCipher128[128] = { 0 };
	uint8 arrPlain128[128] = { 0 };
	uint8 arrCipher256[128] = { 0 };
	uint8 arrPlain256[128] = { 0 };
	bool bEnc128;
	bool bDec128;
	bool bEnc256;
	bool bDec256;

	xrtInit();

	memset(arrKey128, 0x11, sizeof(arrKey128));
	memset(arrKey256, 0x22, sizeof(arrKey256));
	xrtRandomBytes(arrNonce, sizeof(arrNonce));

	bEnc128 = xrtAES128GCMEncrypt(arrCipher128, arrKey128, arrNonce, sizeof(arrNonce), arrAAD, strlen((str)arrAAD), arrPlain, strlen((str)arrPlain));
	bDec128 = xrtAES128GCMDecrypt(arrPlain128, arrKey128, arrNonce, sizeof(arrNonce), arrAAD, strlen((str)arrAAD), arrCipher128, strlen((str)arrPlain) + 16u);

	bEnc256 = xrtAES256GCMEncrypt(arrCipher256, arrKey256, arrNonce, sizeof(arrNonce), arrAAD, strlen((str)arrAAD), arrPlain, strlen((str)arrPlain));
	bDec256 = xrtAES256GCMDecrypt(arrPlain256, arrKey256, arrNonce, sizeof(arrNonce), arrAAD, strlen((str)arrAAD), arrCipher256, strlen((str)arrPlain) + 16u);

	printf("aes128_round_trip = %s\n", exBoolText(bEnc128 && bDec128 && exBytesEqual(arrPlain, arrPlain128, strlen((str)arrPlain))));
	printf("aes256_round_trip = %s\n", exBoolText(bEnc256 && bDec256 && exBytesEqual(arrPlain, arrPlain256, strlen((str)arrPlain))));

	xrtUnit();
	return 0;
}
