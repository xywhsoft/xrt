/*
 * XRT Example - Random Bytes
 * XRT 范例 - 加密安全随机数
 *
 * Description / 说明:
 *   EN: Demonstrates cryptographically secure random byte generation.
 *   CN: 演示加密安全随机字节生成。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_random_bytes.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/crypto_random_bytes -lm -lpthread
 *
 * Note / 注意:
 *   - Use xrtRandomBytes for cryptographic material, not xrtRand32/xrtRand64.
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	uint8 arrA[32];
	uint8 arrB[32];

	xrtInit();

	xrtRandomBytes(arrA, sizeof(arrA));
	xrtRandomBytes(arrB, sizeof(arrB));

	printf("random_a = ");
	exPrintHex(arrA, sizeof(arrA));
	printf("\n");

	printf("random_b = ");
	exPrintHex(arrB, sizeof(arrB));
	printf("\n");

	printf("buffers_equal = %s\n", exBoolText(exBytesEqual(arrA, arrB, sizeof(arrA))));

	xrtUnit();
	return 0;
}
