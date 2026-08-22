/*
 * XRT Example - Number to String
 * XRT 范例 - 数字转字符串
 *
 * Description / 说明:
 *   EN: Demonstrates signed decimal, unsigned hexadecimal, and double
 *       formatting with JNUM.
 *   CN: 演示 JNUM 里的有符号十进制、无符号十六进制和 double 格式化。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/jnum_num_to_str.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/jnum_num_to_str              (Linux, GCC)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static void TestSignedIntegers(void)
{
	int32_t arrI32[] = {0, 1, -1, 42, -42, 2147483647, -2147483647 - 1};
	int64_t arrI64[] = {
		0,
		1,
		-1,
		9223372036854775807LL,
		-9223372036854775807LL,
		12345678901234LL,
		-98765432109876LL,
	};
	char sBuffer[64];
	int i;

	printf("=== Signed Decimal ===\n");
	printf("=== 有符号十进制 ===\n");

	for ( i = 0; i < (int)(sizeof(arrI32) / sizeof(arrI32[0])); i++ ) {
		int iLen = xrtI32ToStr(arrI32[i], sBuffer);
		printf("  i32 %-12d -> \"%s\" (len=%d)\n", arrI32[i], sBuffer, iLen);
	}

	for ( i = 0; i < (int)(sizeof(arrI64) / sizeof(arrI64[0])); i++ ) {
		int iLen = xrtI64ToStr(arrI64[i], sBuffer);
		printf("  i64 %-20lld -> \"%s\" (len=%d)\n", (long long)arrI64[i], sBuffer, iLen);
	}

	printf("\n");
}


static void TestUnsignedHex(void)
{
	uint32_t arrU32[] = {0u, 1u, 255u, 4096u, 0xffffffffu};
	uint64_t arrU64[] = {
		0ULL,
		1ULL,
		255ULL,
		0xfeedbeefULL,
		0xffffffffffffffffULL,
	};
	char sBuffer[64];
	int i;

	printf("=== Unsigned Hex With 0x Prefix ===\n");
	printf("=== 无符号十六进制（带 0x 前缀） ===\n");

	for ( i = 0; i < (int)(sizeof(arrU32) / sizeof(arrU32[0])); i++ ) {
		int iLen = xrtU32ToStr(arrU32[i], sBuffer);
		printf("  u32 %-10u -> \"%s\" (len=%d)\n", arrU32[i], sBuffer, iLen);
	}

	for ( i = 0; i < (int)(sizeof(arrU64) / sizeof(arrU64[0])); i++ ) {
		int iLen = xrtU64ToStr(arrU64[i], sBuffer);
		printf("  u64 %-20llu -> \"%s\" (len=%d)\n", (unsigned long long)arrU64[i], sBuffer, iLen);
	}

	printf("\n");
}


static void TestDoubles(void)
{
	double arrValues[] = {
		0.0,
		1.0,
		-1.0,
		3.14159265358979,
		-2.71828182845904,
		1.23456789e10,
		1.23456789e-10,
		1e15,
		1e-15,
	};
	char sBuffer[64];
	int i;

	printf("=== Double Formatting ===\n");
	printf("=== Double 格式化 ===\n");

	for ( i = 0; i < (int)(sizeof(arrValues) / sizeof(arrValues[0])); i++ ) {
		int iLen = xrtNumToStr(arrValues[i], sBuffer);
		printf("  %-20.15g -> \"%s\" (len=%d)\n", arrValues[i], sBuffer, iLen);
	}

	printf("\n");
}


int main(void)
{
	xrtInit();

	printf("XRT JNUM - Number to String Demo\n");
	printf("XRT JNUM - 数字转字符串演示\n");
	printf("================================\n\n");

	TestSignedIntegers();
	TestUnsignedHex();
	TestDoubles();

	xrtUnit();
	return 0;
}
