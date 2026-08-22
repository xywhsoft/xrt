#include "../test.h"



/* 验证换行规范化、明确长度和事务式容量失败。 */
static void testMailCrlf(void)
{
	static const char arrInput[] = { 'a', '\n', 'b', '\r', 'c', '\r', '\n', 0 };
	static const char arrExpected[] = {
		'a', '\r', '\n', 'b', '\r', '\n', 'c', '\r', '\n', 0
	};
	char arrOutput[32];
	size_t iSize = 0;
	str sOutput;

	testRequire(xrtMailCrlfWrite(
		testMailViewN(arrInput, sizeof(arrInput)),
		NULL,
		0,
		&iSize
	) && (iSize == sizeof(arrExpected)), "mail CRLF query mismatch");
	testRequire(xrtMailCrlfWrite(
		testMailViewN(arrInput, sizeof(arrInput)),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == sizeof(arrExpected)) &&
		(memcmp(arrOutput, arrExpected, sizeof(arrExpected)) == 0) &&
		(arrOutput[iSize] == 0), "mail CRLF output mismatch");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtMailCrlfWrite(
		XRT_STR_LITERAL("a\nb"),
		arrOutput,
		4u,
		&iSize
	) && (iSize == 4u) && (memcmp(arrOutput, "keep", 5u) == 0),
		"mail CRLF short buffer published partial output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"mail CRLF short buffer error mismatch");

	sOutput = xrtMailCrlf(XRT_STR_LITERAL("a\rb"), &iSize);
	testRequire((sOutput != NULL) && (iSize == 4u) &&
		(strcmp(sOutput, "a\r\nb") == 0), "allocated mail CRLF mismatch");
	xrtFree(sOutput);
}



/* 验证 multipart 边界语法属于无依赖邮件核心能力。 */
static void testMailBoundary(void)
{
	testRequire(xrtMailBoundaryValid(XRT_STR_LITERAL("xrt-boundary_01")),
		"mail boundary rejected valid token");
	testRequire(!xrtMailBoundaryValid(XRT_STR_LITERAL("bad boundary ")),
		"mail boundary accepted trailing space");
	testRequire(!xrtMailBoundaryValid(XRT_STR_LITERAL("bad\r\nboundary")),
		"mail boundary accepted control characters");
}



/* 运行邮件内容核心测试。 */
int main(void)
{
	testMailCrlf();
	testMailBoundary();
	return 0;
}
