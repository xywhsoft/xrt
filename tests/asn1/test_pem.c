#include "../test.h"



/* 验证规范编码、查询模式、分配型便捷路径和空正文。 */
static void testPemEncode(void)
{
	static const char Expected[] =
		"-----BEGIN TEST-----\n"
		"TWFu\n"
		"-----END TEST-----\n";
	static const char Empty[] =
		"-----BEGIN EMPTY-----\n"
		"-----END EMPTY-----\n";
	char Output[128];
	size_t iSize = SIZE_MAX;
	str sAllocated;

	testRequire(xrtPemEncode("TEST", "Man", 3, NULL, 0, &iSize) &&
		(iSize == sizeof(Expected) - 1u), "PEM encode query mismatch");
	testRequire(xrtPemEncode(
		"TEST", "Man", 3, Output, sizeof(Output), &iSize
	) && (iSize == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, sizeof(Expected)) == 0),
		"PEM canonical encode mismatch");

	sAllocated = xrtPemEncodeNew("EMPTY", NULL, 0);
	testRequire((sAllocated != NULL) && (strcmp(sAllocated, Empty) == 0),
		"PEM empty encode mismatch");
	xrtFree(sAllocated);
}



/* 验证 Base64 正文严格按 64 字符换行，并且最后一行不补空白。 */
static void testPemWrap(void)
{
	uint8 Input[49];
	char Output[160];
	cstr sBody;
	cstr sEnd;
	size_t iSize;

	memset(Input, 0, sizeof(Input));
	testRequire(xrtPemEncode(
		"DATA", Input, sizeof(Input), Output, sizeof(Output), &iSize
	), "PEM wrapped encode failed");
	sBody = strchr(Output, '\n') + 1;
	sEnd = strstr(sBody, "-----END DATA-----");
	testRequire((sEnd != NULL) && ((size_t)(sEnd - sBody) == 70u) &&
		(sBody[64] == '\n') && (sBody[69] == '\n'),
		"PEM Base64 line wrapping mismatch");
}



/* 验证说明文本、多块遍历、三种换行和精确标签查找。 */
static void testPemReadAndFind(void)
{
	static const char Text[] =
		"explanation\r\n"
		"-----BEGIN ONE-----\r\nT25l\r\n-----END ONE-----\r\n"
		"between\r"
		"-----BEGIN TWO-----\rVHdvbw==\r-----END TWO-----\r"
		"after";
	xpemcursor Cursor;
	xpemblock Block;
	uint8 Decoded[8];
	size_t iSize;

	testRequire(xrtPemInit(&Cursor, Text, sizeof(Text) - 1u),
		"PEM cursor initialization failed");
	testRequire((xrtPemRead(&Cursor, &Block) == XPEM_BLOCK) &&
		(Block.Label.Size == 3u) &&
		(memcmp(Block.Label.Data, "ONE", 3) == 0) &&
		(Block.Raw.Size >= 2u) &&
		(memcmp(Block.Raw.Data + Block.Raw.Size - 2u, "\r\n", 2) == 0) &&
		xrtPemDecode(&Block, Decoded, sizeof(Decoded), &iSize) &&
		(iSize == 3u) && (memcmp(Decoded, "One", 3) == 0),
		"PEM first block mismatch");
	testRequire((xrtPemRead(&Cursor, &Block) == XPEM_BLOCK) &&
		(Block.Label.Size == 3u) &&
		(memcmp(Block.Label.Data, "TWO", 3) == 0) &&
		(Block.Raw.Size >= 1u) && (Block.Raw.Data[Block.Raw.Size - 1u] == '\r') &&
		xrtPemDecode(&Block, Decoded, sizeof(Decoded), &iSize) &&
		(iSize == 4u) && (memcmp(Decoded, "Twoo", 4) == 0),
		"PEM second block mismatch");
	testRequire(xrtPemRead(&Cursor, &Block) == XPEM_DONE,
		"PEM cursor did not finish cleanly");
	testRequire(xrtPemFind(
		Text, sizeof(Text) - 1u, "TWO", &Block
	) && (Block.Label.Size == 3u), "PEM exact label find failed");
}



/* 验证解析严格受显式长度约束，不依赖输入末尾零字节。 */
static void testPemBoundedInput(void)
{
	static const char Text[] = {
		'-', '-', '-', '-', '-', 'B', 'E', 'G', 'I', 'N', ' ', 'X',
		'-', '-', '-', '-', '-', '\n', 'W', 'A', '=', '=', '\n',
		'-', '-', '-', '-', '-', 'E', 'N', 'D', ' ', 'X',
		'-', '-', '-', '-', '-', 'X'
	};
	xpemcursor Cursor;
	xpemblock Block;
	uint8 Output[2];
	size_t iSize;

	testRequire(xrtPemInit(&Cursor, Text, sizeof(Text) - 1u) &&
		(xrtPemRead(&Cursor, &Block) == XPEM_BLOCK) &&
		xrtPemDecode(&Block, Output, sizeof(Output), &iSize) &&
		(iSize == 1u) && (Output[0] == (uint8)'X'),
		"PEM bounded non-NUL input failed");
}



/* 验证边界、标签和嵌套错误不会推进游标或发布半个块。 */
static void testPemRejectsMalformedBoundaries(void)
{
	static const cstr Cases[] = {
		"-----BEGIN A-----\nQQ==\n-----END B-----\n",
		"-----BEGIN A-----\nQQ==\n",
		"-----BEGIN A-----\n-----BEGIN B-----\n-----END B-----\n",
		"-----BEGIN A  B-----\nQQ==\n-----END A  B-----\n",
		"-----BEGIN A- B-----\nQQ==\n-----END A- B-----\n",
		"-----BEGIN A -B-----\nQQ==\n-----END A -B-----\n",
		"-----BEGIN A-----tail\nQQ==\n-----END A-----\n",
		"-----BEGIN A-----"
	};

	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xpemcursor Cursor;
		xpemcursor BeforeCursor;
		xpemblock Block;
		xpemblock BeforeBlock;

		testRequire(xrtPemInit(&Cursor, Cases[i], strlen(Cases[i])),
			"PEM malformed cursor initialization failed");
		BeforeCursor = Cursor;
		memset(&Block, 0xA5, sizeof(Block));
		BeforeBlock = Block;
		testRequire((xrtPemRead(&Cursor, &Block) == XPEM_ERROR) &&
			(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0) &&
			(memcmp(&Block, &BeforeBlock, sizeof(Block)) == 0) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.pem") == 0),
			"PEM malformed boundary was accepted or changed outputs");
	}
}



/* 验证正文错误被包装为 PEM 错误，并保留底层 Codec 原因。 */
static void testPemBodyError(void)
{
	static const char Text[] =
		"-----BEGIN DATA-----\nTQ===\n-----END DATA-----\n";
	xpemcursor Cursor;
	xpemblock Block;
	uint8 Output[8];
	size_t iSize = 77;
	const xerror* pError;
	const xerror* pCause;

	testRequire(xrtPemInit(&Cursor, Text, sizeof(Text) - 1u) &&
		(xrtPemRead(&Cursor, &Block) == XPEM_BLOCK),
		"PEM malformed body block parse failed unexpectedly");
	testRequire(!xrtPemDecode(
		&Block, Output, sizeof(Output), &iSize
	) && (iSize == 77), "PEM malformed Base64 was accepted");
	pError = xrtGetError();
	pCause = xrtErrorCause(pError);
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.pem") == 0) &&
		(xrtErrorCode(pError) == XPEM_ERROR_BODY) &&
		(pCause != NULL) &&
		(strcmp(xrtErrorDomain(pCause), "xrt.codec") == 0) &&
		(xrtErrorCode(pCause) == XCODEC_ERROR_BASE64_FORMAT),
		"PEM Base64 cause chain mismatch");
}



/* 验证容量、重叠和未找到路径保持输出缓冲与对象原子性。 */
static void testPemFailureAtomicity(void)
{
	char Output[96];
	char Before[96];
	size_t iRequired;
	size_t iSize;
	xpemblock Block;
	xpemblock BeforeBlock;

	testRequire(xrtPemEncode("TEST", "Man", 3, NULL, 0, &iRequired),
		"PEM atomicity query failed");
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 99;
	testRequire(!xrtPemEncode(
		"TEST", "Man", 3, Output, iRequired, &iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"PEM capacity failure changed output");

	memcpy(Output, "Man", 4);
	memcpy(Before, Output, sizeof(Output));
	iSize = 99;
	testRequire(!xrtPemEncode(
		"TEST", Output, 3, Output, sizeof(Output), &iSize
	) && (iSize == 99) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"PEM overlapping encode was not rejected atomically");

	memset(&Block, 0xA5, sizeof(Block));
	BeforeBlock = Block;
	testRequire(!xrtPemFind("plain text", 10, "DATA", &Block) &&
		(memcmp(&Block, &BeforeBlock, sizeof(Block)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XPEM_ERROR_NOT_FOUND),
		"PEM not-found path changed output");
	testRequire(!xrtPemFind("plain text", 10, "A- B", &Block) &&
		(memcmp(&Block, &BeforeBlock, sizeof(Block)) == 0) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.pem") == 0) &&
		(xrtErrorCode(xrtGetError()) == XPEM_ERROR_LABEL),
		"PEM invalid requested label preserved a stale error");
}



/* 执行 PEM 编码、解析、严格正文、失败原子性和长度边界测试。 */
int main(void)
{
	testPemEncode();
	testPemWrap();
	testPemReadAndFind();
	testPemBoundedInput();
	testPemRejectsMalformedBoundaries();
	testPemBodyError();
	testPemFailureAtomicity();
	printf("[PASS] pem\n");
	return 0;
}
