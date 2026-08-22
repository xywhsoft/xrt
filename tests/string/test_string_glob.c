#include "../test.h"



/* 验证严格 UTF-8 通配、字符类、转义、大小写和错误边界。 */
int main(void)
{
	static const char arrInvalid[] = { (char)0xE0, (char)0x80, (char)0x80 };

	testRequire(xrtStrGlob(XRT_STR_LITERAL("hello.txt"), XRT_STR_LITERAL("*.txt"), 0),
		"basic glob mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL("a你😀z"), XRT_STR_LITERAL("a??z"), 0),
		"UTF-8 question mark mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL("file7.c"), XRT_STR_LITERAL("file[0-9].[ch]"), 0),
		"glob character range mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL("filex.c"), XRT_STR_LITERAL("file[!0-9].c"), 0),
		"negated glob class mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL("a*b"), XRT_STR_LITERAL("a\\*b"), 0),
		"escaped glob star mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL("README.MD"), XRT_STR_LITERAL("readme.[mM][dD]"),
		XSTR_GLOB_CASE_ASCII), "ASCII case-insensitive glob mismatch");
	testRequire(!xrtStrGlob(XRT_STR_LITERAL("README.MD"), XRT_STR_LITERAL("readme.md"), 0),
		"case-sensitive glob mismatch");
	testRequire(xrtStrGlob(XRT_STR_LITERAL(""), XRT_STR_LITERAL("***"), 0),
		"empty glob mismatch");
	testRequire(!xrtStrGlob(XRT_STR_LITERAL(""), XRT_STR_LITERAL("?"), 0),
		"empty question glob mismatch");

	xrtClearError();
	testRequire(!xrtStrGlob(XRT_STR_LITERAL("a"), XRT_STR_LITERAL("[z-a]"), 0),
		"reversed glob range must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XSTR_ERROR_PATTERN),
		"reversed glob range error mismatch");
	xrtClearError();
	testRequire(!xrtStrGlob(XRT_STR_LITERAL("a"), XRT_STR_LITERAL("a\\"), 0),
		"incomplete glob escape must fail");
	testRequire(xrtErrorCode(xrtGetError()) == XSTR_ERROR_PATTERN,
		"incomplete glob escape error mismatch");
	xrtClearError();
	testRequire(!xrtStrGlob((xstrview){ arrInvalid, sizeof(arrInvalid) },
		XRT_STR_LITERAL("*"), 0), "invalid UTF-8 glob text must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_INVALID),
		"invalid UTF-8 glob error mismatch");
	xrtClearError();
	testRequire(!xrtStrGlob(XRT_STR_LITERAL("a"), XRT_STR_LITERAL("*"), 2u),
		"unknown glob flag must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"unknown glob flag error mismatch");
	xrtClearError();

	printf("[PASS] string-glob\n");
	return 0;
}
