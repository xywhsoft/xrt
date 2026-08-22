/*
 * XRT Example - SHA512
 * XRT 范例 - SHA512 与 SHA384
 *
 * Description / 说明:
 *   EN: Demonstrates SHA-512 and SHA-384 hashing APIs.
 *   CN: 演示 SHA-512 与 SHA-384 哈希接口。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_sha512.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_sha512 -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	str sText = "XRT SHA512 Example";
	uint8 arrSHA512[64];
	uint8 arrSHA384OneShot[48];
	uint8 arrSHA384Stream[48];
	xsha512_ctx tCtx;

	xrtInit();

	xrtSHA512(sText, strlen(sText), arrSHA512);
	xrtSHA384(sText, strlen(sText), arrSHA384OneShot);

	xrtSHA512Init(&tCtx);
	xrtSHA512Update(&tCtx, sText, 5);
	xrtSHA512Update(&tCtx, sText + 5, strlen(sText) - 5);
	xrtSHA384Final(&tCtx, arrSHA384Stream);

	printf("sha512 = ");
	exPrintHex(arrSHA512, sizeof(arrSHA512));
	printf("\n");

	printf("sha384_one_shot = ");
	exPrintHex(arrSHA384OneShot, sizeof(arrSHA384OneShot));
	printf("\n");

	printf("sha384_stream   = ");
	exPrintHex(arrSHA384Stream, sizeof(arrSHA384Stream));
	printf("\n");

	printf("sha384_match = %s\n", exBoolText(exBytesEqual(arrSHA384OneShot, arrSHA384Stream, sizeof(arrSHA384OneShot))));

	xrtUnit();
	return 0;
}
