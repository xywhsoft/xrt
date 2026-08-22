/*
 * XRT Example - HMAC
 * XRT 范例 - HMAC 消息认证
 *
 * Description / 说明:
 *   EN: Demonstrates HMAC-SHA256, HMAC-SHA384 and HMAC-SHA512.
 *   CN: 演示 HMAC-SHA256、HMAC-SHA384 与 HMAC-SHA512。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_hmac.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_hmac -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	const uint8 arrKey[] = "demo-key";
	const uint8 arrMsg[] = "authenticated message";
	uint8 arr256[32];
	uint8 arr384[48];
	uint8 arr512[64];

	xrtInit();

	xrtHMAC_SHA256(arrKey, strlen((str)arrKey), arrMsg, strlen((str)arrMsg), arr256);
	xrtHMAC_SHA384(arrKey, strlen((str)arrKey), arrMsg, strlen((str)arrMsg), arr384);
	xrtHMAC_SHA512(arrKey, strlen((str)arrKey), arrMsg, strlen((str)arrMsg), arr512);

	printf("hmac_sha256 = ");
	exPrintHex(arr256, sizeof(arr256));
	printf("\n");

	printf("hmac_sha384 = ");
	exPrintHex(arr384, sizeof(arr384));
	printf("\n");

	printf("hmac_sha512 = ");
	exPrintHex(arr512, sizeof(arr512));
	printf("\n");

	xrtUnit();
	return 0;
}
