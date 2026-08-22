/*
 * XRT Example - SHA256
 * XRT 范例 - SHA256 摘要
 *
 * Description / 说明:
 *   EN: Demonstrates one-shot and incremental SHA-256 hashing.
 *   CN: 演示一次性与分块式 SHA-256 哈希计算。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_sha256.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_sha256 -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	str sText = "XRT SHA256 Example";
	uint8 arrOneShot[32];
	uint8 arrStream[32];
	xsha256_ctx tCtx;

	xrtInit();

	xrtSHA256(sText, strlen(sText), arrOneShot);

	xrtSHA256Init(&tCtx);
	xrtSHA256Update(&tCtx, sText, 4);
	xrtSHA256Update(&tCtx, sText + 4, strlen(sText) - 4);
	xrtSHA256Final(&tCtx, arrStream);

	printf("sha256_one_shot = ");
	exPrintHex(arrOneShot, sizeof(arrOneShot));
	printf("\n");

	printf("sha256_stream   = ");
	exPrintHex(arrStream, sizeof(arrStream));
	printf("\n");

	printf("match = %s\n", exBoolText(exBytesEqual(arrOneShot, arrStream, sizeof(arrOneShot))));

	xrtUnit();
	return 0;
}
