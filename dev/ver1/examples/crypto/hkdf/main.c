/*
 * XRT Example - HKDF
 * XRT 范例 - HKDF 密钥派生
 *
 * Description / 说明:
 *   EN: Demonstrates HKDF extract/expand with SHA-256 and SHA-384.
 *   CN: 演示基于 SHA-256 与 SHA-384 的 HKDF 提取与扩展。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_hkdf.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_hkdf -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	const uint8 arrSalt[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
	const uint8 arrIKM[] = "input key material";
	const uint8 arrInfo[] = "hkdf-demo";
	uint8 arrPrk256[32];
	uint8 arrOkm256[42];
	uint8 arrPrk384[48];
	uint8 arrOkm384[64];

	xrtInit();

	xrtHKDFExtract(arrPrk256, arrSalt, sizeof(arrSalt), arrIKM, strlen((str)arrIKM));
	xrtHKDFExpand(arrOkm256, sizeof(arrOkm256), arrPrk256, sizeof(arrPrk256), arrInfo, strlen((str)arrInfo));

	xrtHKDFExtract_SHA384(arrPrk384, arrSalt, sizeof(arrSalt), arrIKM, strlen((str)arrIKM));
	xrtHKDFExpand_SHA384(arrOkm384, sizeof(arrOkm384), arrPrk384, sizeof(arrPrk384), arrInfo, strlen((str)arrInfo));

	printf("hkdf_sha256_prk = ");
	exPrintHex(arrPrk256, sizeof(arrPrk256));
	printf("\n");

	printf("hkdf_sha256_okm = ");
	exPrintHex(arrOkm256, sizeof(arrOkm256));
	printf("\n");

	printf("hkdf_sha384_prk = ");
	exPrintHex(arrPrk384, sizeof(arrPrk384));
	printf("\n");

	printf("hkdf_sha384_okm = ");
	exPrintHex(arrOkm384, sizeof(arrOkm384));
	printf("\n");

	xrtUnit();
	return 0;
}
