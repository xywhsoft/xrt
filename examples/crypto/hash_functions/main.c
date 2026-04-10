/*
 * XRT Example - Hash / HMAC / HKDF
 * XRT 范例 - 摘要 / HMAC / HKDF
 *
 * Description / 说明:
 *   EN: Demonstrates real XRT crypto primitives for SHA-256, incremental
 *       SHA-384, HMAC-SHA256, and HKDF-SHA256.
 *   CN: 演示当前 XRT 真实公开的加密原语：SHA-256、增量 SHA-384、
 *       HMAC-SHA256 和 HKDF-SHA256。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_hash_functions.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/crypto_hash_functions -lws2_32 -lshell32 -liphlpapi
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdio.h>
#include <string.h>


static void PrintHex(const uint8* pData, size_t iLen)
{
	size_t i;

	for ( i = 0; i < iLen; i++ ) {
		printf("%02x", pData[i]);
	}
}


static bool BufferEqual(const uint8* pLeft, const uint8* pRight, size_t iLen)
{
	return memcmp(pLeft, pRight, iLen) == 0;
}


int main(void)
{
	char sMsg[] = "xrt crypto demo";
	const uint8 aucSalt[] = { 0x10, 0x22, 0x34, 0x48, 0x5a, 0x6c, 0x7e, 0x80 };
	const uint8 aucInfo[] = "demo/hash-hmac-hkdf";
	uint8 aucSha256[32];
	uint8 aucSha384OneShot[48];
	uint8 aucSha384Stream[48];
	uint8 aucHmac[32];
	uint8 aucPrk[32];
	uint8 aucOkm[48];
	xsha512_ctx tSha384;
	bool bSha384Same = false;

	xrtInit();

	xrtSHA256(sMsg, strlen(sMsg), aucSha256);
	xrtSHA384(sMsg, strlen(sMsg), aucSha384OneShot);

	xrtSHA384Init(&tSha384);
	xrtSHA512Update(&tSha384, sMsg, 4);
	xrtSHA512Update(&tSha384, sMsg + 4, strlen(sMsg) - 4);
	xrtSHA384Final(&tSha384, aucSha384Stream);

	xrtHMAC_SHA256((const uint8*)"demo-key", 8, (const uint8*)sMsg, strlen(sMsg), aucHmac);
	xrtHKDFExtract(aucPrk, aucSalt, sizeof(aucSalt), (const uint8*)sMsg, strlen(sMsg));
	xrtHKDFExpand(aucOkm, sizeof(aucOkm), aucPrk, sizeof(aucPrk), aucInfo, strlen((const char*)aucInfo));

	bSha384Same = BufferEqual(aucSha384OneShot, aucSha384Stream, sizeof(aucSha384OneShot));

	printf("message = %s\n", sMsg);
	printf("sha256 = ");
	PrintHex(aucSha256, sizeof(aucSha256));
	printf("\n");

	printf("sha384_one_shot = ");
	PrintHex(aucSha384OneShot, sizeof(aucSha384OneShot));
	printf("\n");

	printf("sha384_stream = ");
	PrintHex(aucSha384Stream, sizeof(aucSha384Stream));
	printf("\n");

	printf("sha384_stream_matches = %s\n", bSha384Same ? "true" : "false");

	printf("hmac_sha256 = ");
	PrintHex(aucHmac, sizeof(aucHmac));
	printf("\n");

	printf("hkdf_prk = ");
	PrintHex(aucPrk, sizeof(aucPrk));
	printf("\n");

	printf("hkdf_okm = ");
	PrintHex(aucOkm, sizeof(aucOkm));
	printf("\n");

	xrtUnit();
	return bSha384Same ? 0 : 1;
}
