#include "../test.h"



/* 旧版 rapidhash v3.0 公开结果必须逐值保持。 */
static void testLegacyVectors(void)
{
	testRequire(xrtHash64(NULL, 0) == UINT64_C(0x0338DC4BE2CECDAE),
		"hash64 empty vector changed");
	testRequire(xrtHash64("Hello", 5) == UINT64_C(0x341C16AEF48B463D),
		"hash64 Hello vector changed");
	testRequire(xrtHash64("The quick brown fox jumps over the lazy dog", 43) ==
		UINT64_C(0x91722DC8D52A3F7B), "hash64 phrase vector changed");
	testRequire(xrtHash64Seed("Hello World", 11,
		UINT64_C(0xDEADBEEFCAFEBABE)) == UINT64_C(0x81D5D2BCD26F0472),
		"hash64 seeded vector changed");
}



/* 短输入、长输入和 112 字节轮次边界都必须支持未对齐地址。 */
static void testLengthsAndAlignment(void)
{
	static const struct {
		size_t Size;
		uint64 Hash;
	} arrLegacy[] = {
		{ 255u, UINT64_C(0x0EF3662377D6029E) },
		{ 256u, UINT64_C(0x54A870B8E2B36A23) },
		{ 257u, UINT64_C(0x4D56EB2CE5E0D964) },
		{ 511u, UINT64_C(0xF247F91E20FC2586) },
		{ 512u, UINT64_C(0x559EE2C3910429C4) },
		{ 1024u, UINT64_C(0x38FA3E942DFA978F) }
	};
	unsigned char arrAligned[1024];
	unsigned char arrUnaligned[1025];

	for ( size_t i = 0; i < sizeof(arrAligned); i++ ) {
		arrAligned[i] = (unsigned char)((i * 53u) ^ (i >> 1));
		arrUnaligned[i + 1u] = arrAligned[i];
	}

	for ( size_t i = 0; i <= sizeof(arrAligned); i++ ) {
		uint64 iExpected = xrtHash64Seed(arrAligned, i,
			UINT64_C(0x0123456789ABCDEF));
		uint64 iActual = xrtHash64Seed(arrUnaligned + 1, i,
			UINT64_C(0x0123456789ABCDEF));

		testRequire(iActual == iExpected, "hash64 depends on input alignment");
	}
	for ( size_t i = 0; i < (sizeof(arrLegacy) / sizeof(arrLegacy[0])); i++ ) {
		testRequire(xrtHash64Seed(arrAligned, arrLegacy[i].Size,
			UINT64_C(0x0123456789ABCDEF)) == arrLegacy[i].Hash,
			"hash64 long-input legacy vector changed");
	}
}



/* 空输入接受空指针，非空输入必须报告参数错误。 */
static void testArguments(void)
{
	xrtClearError();
	(void)xrtHash64(NULL, 0);
	testRequire(xrtGetError() == NULL, "hash64 empty input set an error");

	xrtClearError();
	testRequire(xrtHash64(NULL, 1) == 0, "hash64 invalid input did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"hash64 invalid input reported the wrong error");

	xrtClearError();
	testRequire(xrtHash64Seed(NULL, 1, 7) == 0,
		"seeded hash64 invalid input did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"seeded hash64 invalid input reported the wrong error");
}



/* 执行旧输出、长度分支和参数边界测试。 */
int main(void)
{
	testLegacyVectors();
	testLengthsAndAlignment();
	testArguments();
	return 0;
}
