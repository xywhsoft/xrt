#include "../test.h"



/* 旧版公开结果必须在新 API 中逐值保持。 */
static void testLegacyVectors(void)
{
	testRequire(xrtHash32(NULL, 0) == UINT32_C(0x09ADEC44),
		"hash32 empty vector changed");
	testRequire(xrtHash32("a", 1) == UINT32_C(0x019BECA0),
		"hash32 one-byte vector changed");
	testRequire(xrtHash32("Hello", 5) == UINT32_C(0x83B10C78),
		"hash32 Hello vector changed");
	testRequire(xrtHash32("The quick brown fox jumps over the lazy dog", 43) ==
		UINT32_C(0x7B30D24F), "hash32 phrase vector changed");
	testRequire(xrtHash32Seed("Hello World", 11, UINT32_C(0xDEADBEEF)) ==
		UINT32_C(0x572309DF), "hash32 seeded vector changed");
}



/* 所有长度分支都必须与输入地址对齐方式无关。 */
static void testLengthsAndAlignment(void)
{
	static const struct {
		size_t Size;
		uint32 Hash;
	} arrLegacy[] = {
		{ 255u, UINT32_C(0x22D8E045) },
		{ 256u, UINT32_C(0xCFA69BA0) },
		{ 257u, UINT32_C(0x911098E4) },
		{ 511u, UINT32_C(0x14AA4B44) },
		{ 512u, UINT32_C(0x4CDCB05C) },
		{ 1024u, UINT32_C(0x0E745385) }
	};
	unsigned char arrAligned[1024];
	unsigned char arrUnaligned[1025];

	for ( size_t i = 0; i < sizeof(arrAligned); i++ ) {
		arrAligned[i] = (unsigned char)((i * 37u) + (i >> 2));
		arrUnaligned[i + 1u] = arrAligned[i];
	}

	for ( size_t i = 0; i <= sizeof(arrAligned); i++ ) {
		uint32 iExpected = xrtHash32Seed(arrAligned, i, UINT32_C(0x12345678));
		uint32 iActual = xrtHash32Seed(arrUnaligned + 1, i, UINT32_C(0x12345678));

		testRequire(iActual == iExpected, "hash32 depends on input alignment");
	}
	for ( size_t i = 0; i < (sizeof(arrLegacy) / sizeof(arrLegacy[0])); i++ ) {
		testRequire(xrtHash32Seed(arrAligned, arrLegacy[i].Size,
			UINT32_C(0x12345678)) == arrLegacy[i].Hash,
			"hash32 long-input legacy vector changed");
	}
}



/* 空输入接受空指针，非空输入必须报告参数错误。 */
static void testArguments(void)
{
	xrtClearError();
	(void)xrtHash32(NULL, 0);
	testRequire(xrtGetError() == NULL, "hash32 empty input set an error");

	xrtClearError();
	testRequire(xrtHash32(NULL, 1) == 0, "hash32 invalid input did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"hash32 invalid input reported the wrong error");

	xrtClearError();
	testRequire(xrtHash32Seed(NULL, 1, 7) == 0,
		"seeded hash32 invalid input did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"seeded hash32 invalid input reported the wrong error");
}



/* 执行确定性、长度分支和参数边界测试。 */
int main(void)
{
	testLegacyVectors();
	testLengthsAndAlignment();
	testArguments();
	return 0;
}
