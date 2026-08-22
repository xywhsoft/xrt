#include "../test.h"



/* 比较无零结尾线缆片段。 */
static bool testHttpRangeMultipartBytes(
	const void* pData,
	size_t iSize,
	cstr sExpected
)
{
	size_t iExpected = strlen(sExpected);

	return (iSize == iExpected) &&
		(memcmp(pData, sExpected, iSize) == 0);
}



/* 验证 Part 头、段尾、关闭边界和完整长度使用同一线缆格式。 */
static void testHttpRangeMultipartWire(void)
{
	xhttpbyterange Ranges[2] = {
		{ 10, 19 },
		{ 30, 39 }
	};
	xstrview Type = XRT_STR_LITERAL("image/png");
	xstrview Boundary = XRT_STR_LITERAL("xrt-range-17");
	char Output[256];
	uint64 iLength;
	size_t iRequired;
	size_t iSize;

	testRequire(xrtHttpRangeMultipartLength(
		Ranges,
		2,
		100,
		Type,
		Boundary,
		&iLength
	) && (iLength == 192),
		"HTTP range multipart length mismatch");

	testRequire(xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		100,
		Type,
		Boundary,
		NULL,
		0,
		&iRequired
	) && (iRequired == 75),
		"HTTP range multipart head measure failed");
	testRequire(xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		100,
		Type,
		Boundary,
		Output,
		sizeof(Output),
		&iSize
	) && (iSize == iRequired) &&
		testHttpRangeMultipartBytes(
			Output,
			iSize,
			"--xrt-range-17\r\n"
			"Content-Type: image/png\r\n"
			"Content-Range: bytes 10-19/100\r\n"
			"\r\n"
		),
		"HTTP range multipart head bytes mismatch");
	testRequire(xrtHttpRangeMultipartEndWrite(
		Output,
		sizeof(Output),
		&iSize
	) && testHttpRangeMultipartBytes(
		Output,
		iSize,
		"\r\n"
	), "HTTP range multipart end bytes mismatch");
	testRequire(xrtHttpRangeMultipartCloseWrite(
		Boundary,
		Output,
		sizeof(Output),
		&iSize
	) && testHttpRangeMultipartBytes(
		Output,
		iSize,
		"--xrt-range-17--\r\n"
	), "HTTP range multipart close bytes mismatch");
}



/* 验证默认媒体类型和 64 位范围值不会经过窄整数格式化。 */
static void testHttpRangeMultipartDefaults(void)
{
	xhttpbyterange Range = {
		UINT64_C(4294967296),
		UINT64_C(4294967300)
	};
	char Output[256];
	size_t iSize;

	testRequire(xrtHttpRangeMultipartHeadWrite(
		&Range,
		UINT64_C(8589934592),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("b"),
		Output,
		sizeof(Output),
		&iSize
	) && testHttpRangeMultipartBytes(
		Output,
		iSize,
		"--b\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Range: bytes 4294967296-4294967300/8589934592\r\n"
		"\r\n"
	), "HTTP range multipart default or uint64 mismatch");
}



/* 验证范围顺序、协议值、总长度溢出和输出别名边界。 */
static void testHttpRangeMultipartEdges(void)
{
	xhttpbyterange Ranges[2] = {
		{ 10, 19 },
		{ 18, 29 }
	};
	xhttpbyterange Huge = {
		0,
		UINT64_MAX - UINT64_C(1)
	};
	char Output[128];
	char Before[128];
	uint64 iLength = 77;
	size_t iSize = 0;
	size_t iRequired;

	testRequire(!xrtHttpRangeMultipartLength(
		Ranges,
		2,
		100,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("safe"),
		&iLength
	) && (iLength == 77),
		"HTTP range multipart accepted overlapping ranges");
	xrtClearError();

	Ranges[1].First = 30;
	Ranges[1].Last = 39;
	testRequire(!xrtHttpRangeMultipartLength(
		Ranges,
		2,
		100,
		XRT_STR_LITERAL("text/plain\r\nx: y"),
		XRT_STR_LITERAL("safe"),
		&iLength
	), "HTTP range multipart accepted field injection");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartLength(
		Ranges,
		2,
		100,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("unsafe boundary"),
		&iLength
	), "HTTP range multipart accepted an unsafe boundary");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartLength(
		&Huge,
		1,
		UINT64_MAX,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("safe"),
		&iLength
	), "HTTP range multipart length overflow succeeded");
	xrtClearError();

	testRequire(xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		100,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("safe"),
		NULL,
		0,
		&iRequired
	), "HTTP range multipart edge measure failed");
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		100,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("safe"),
		Output,
		iRequired - 1u,
		&iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"HTTP range multipart capacity failure was not atomic");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		100,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("safe"),
		&Ranges[0],
		sizeof(Ranges[0]),
		&iSize
	), "HTTP range multipart accepted aliased output");
	xrtClearError();
}



/* 验证 multipart 固定描述符和标量输出支持未对齐存储并拒绝地址回绕。 */
static void testHttpRangeMultipartMemory(void)
{
	uint8 RangeStorage[sizeof(xhttpbyterange) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 LengthStorage[sizeof(uint64) + 2u];
	xhttpbyterange Range = { 0, 1 };
	char Output[160];
	size_t iSize;
	uint64 iLength;

	memset(RangeStorage, 0xA5, sizeof(RangeStorage));
	memcpy(RangeStorage + 1u, &Range, sizeof(Range));
	memset(SizeStorage, 0xB6, sizeof(SizeStorage));
	memset(LengthStorage, 0xC7, sizeof(LengthStorage));
	testRequire(xrtHttpRangeMultipartLength(
		(const xhttpbyterange*)(RangeStorage + 1u),
		1u,
		10u,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("part"),
		(uint64*)(LengthStorage + 1u)
	), "HTTP range multipart length rejected unaligned storage");
	memcpy(&iLength, LengthStorage + 1u, sizeof(iLength));
	testRequire((iLength != 0) &&
		(RangeStorage[0] == 0xA5u) &&
		(RangeStorage[sizeof(RangeStorage) - 1u] == 0xA5u) &&
		(LengthStorage[0] == 0xC7u) &&
		(LengthStorage[sizeof(LengthStorage) - 1u] == 0xC7u),
		"HTTP range multipart unaligned length mismatch");

	testRequire(xrtHttpRangeMultipartHeadWrite(
		(const xhttpbyterange*)(RangeStorage + 1u),
		10u,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("part"),
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u)
	), "HTTP range multipart head rejected unaligned storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize != 0) &&
		(SizeStorage[0] == 0xB6u) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xB6u) &&
		testHttpRangeMultipartBytes(
			Output,
			iSize,
			"--part\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Range: bytes 0-1/10\r\n"
			"\r\n"
		),
		"HTTP range multipart unaligned head mismatch");
	testRequire(xrtHttpRangeMultipartEndWrite(
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u)
	), "HTTP range multipart end rejected unaligned size");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 2u) &&
		(memcmp(Output, "\r\n", 2u) == 0),
		"HTTP range multipart unaligned end mismatch");
	testRequire(xrtHttpRangeMultipartCloseWrite(
		XRT_STR_LITERAL("part"),
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u)
	), "HTTP range multipart close rejected unaligned size");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == 10u) &&
		(memcmp(Output, "--part--\r\n", iSize) == 0),
		"HTTP range multipart unaligned close mismatch");

	xrtClearError();
	testRequire(!xrtHttpRangeMultipartHeadWrite(
		(const xhttpbyterange*)(uintptr_t)(UINTPTR_MAX - 1u),
		10u,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("part"),
		Output,
		sizeof(Output),
		&iSize
	), "HTTP range multipart head accepted wrapping input");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartHeadWrite(
		&Range,
		10u,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("part"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		sizeof(Output),
		&iSize
	), "HTTP range multipart head accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartEndWrite(
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP range multipart end accepted wrapping size");
	xrtClearError();
	testRequire(!xrtHttpRangeMultipartCloseWrite(
		XRT_STR_LITERAL("part"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		sizeof(Output),
		&iSize
	), "HTTP range multipart close accepted wrapping output");
	xrtClearError();
}



/* 执行字节多范围线缆层测试。 */
int main(void)
{
	testHttpRangeMultipartWire();
	testHttpRangeMultipartDefaults();
	testHttpRangeMultipartEdges();
	testHttpRangeMultipartMemory();
	printf("[PASS] http_range_multipart\n");
	return 0;
}
