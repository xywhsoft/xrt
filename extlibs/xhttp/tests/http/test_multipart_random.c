#include "../test.h"



/* 判断一个字节是否为小写十六进制字符。 */
static bool testHexLower(char ch)
{
	return ((ch >= '0') && (ch <= '9')) ||
		((ch >= 'a') && (ch <= 'f'));
}



/* 验证安全随机 boundary 的格式、未对齐发布和解析闭环。 */
int main(void)
{
	unsigned char Storage[sizeof(xmultipartboundary) + 1u];
	xmultipartboundary Boundary;
	xmultipartboundary Parsed;
	size_t i;

	memset(Storage, 0xA5, sizeof(Storage));
	testRequire(xrtMultipartBoundaryRandom(
		(xmultipartboundary*)(void*)(Storage + 1u)
	), "multipart random unaligned output failed");
	memcpy(&Boundary, Storage + 1u, sizeof(Boundary));
	testRequire((Boundary.Size == 45u) &&
		(memcmp(Boundary.Data, "----xrt-form-", 13u) == 0) &&
		(Boundary.Data[Boundary.Size] == '\0'),
		"multipart random boundary shape mismatch");
	for ( i = 13u; i < Boundary.Size; i++ ) {
		testRequire(testHexLower(Boundary.Data[i]),
			"multipart random boundary contains a non-hex byte");
	}
	testRequire(xrtMultipartBoundaryParse(
		(xstrview){ Boundary.Data, Boundary.Size }, &Parsed
	) && (Parsed.Size == Boundary.Size) &&
		(memcmp(Parsed.Data, Boundary.Data, Boundary.Size + 1u) == 0),
		"multipart random boundary parse roundtrip failed");

	testRequire(!xrtMultipartBoundaryRandom(NULL),
		"multipart random accepted a null output");
	xrtClearError();
	printf("[PASS] multipart_random\n");
	return 0;
}
