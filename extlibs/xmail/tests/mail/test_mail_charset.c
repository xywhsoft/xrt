#include "../test.h"



/* 验证内置字符集别名和 UTF-8 转换。 */
static void testMailCharsetConvert(void)
{
	static const unsigned char arrLatin[] = { 'c', 'a', 'f', 0xE9u };
	static const unsigned char arrWindows[] = { 0x93u, 'x', 0x94u, 0x80u };
	char arrOutput[32];
	size_t iSize;

	testRequire(xrtMailCharsetSupported(XRT_STR_LITERAL("latin1")) &&
		xrtMailCharsetSupported(XRT_STR_LITERAL("CP1252")) &&
		!xrtMailCharsetSupported(XRT_STR_LITERAL("GB18030")),
		"mail character set support query mismatch");
	testRequire(xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("ISO-8859-1"),
		(xbytesview) { arrLatin, sizeof(arrLatin) },
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "caf\xC3\xA9") == 0),
		"mail Latin-1 conversion mismatch");
	testRequire(xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("windows-1252"),
		(xbytesview) { arrWindows, sizeof(arrWindows) },
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(
		arrOutput,
		"\xE2\x80\x9Cx\xE2\x80\x9D\xE2\x82\xAC"
	) == 0), "mail Windows-1252 conversion mismatch");
}



/* 失败路径必须保持目标不变并提供稳定错误。 */
static void testMailCharsetErrors(void)
{
	static const unsigned char arrInvalid[] = { 0xFFu };
	static const unsigned char arrUndefinedWindows[] = { 0x81u };
	char arrOutput[8] = "keep";
	size_t iSize = 0;

	testRequire(!xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("UTF-8"),
		(xbytesview) { arrInvalid, sizeof(arrInvalid) },
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"invalid mail UTF-8 modified output");
	xrtClearError();
	testRequire(!xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("KOI8-R"),
		(xbytesview) { arrInvalid, sizeof(arrInvalid) },
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "unsupported mail character set was accepted");
	xrtClearError();
	testRequire(!xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("Windows-1252"),
		(xbytesview) {
			arrUndefinedWindows,
			sizeof(arrUndefinedWindows)
		},
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "keep") == 0),
		"undefined Windows-1252 byte was accepted");
}



/* 运行邮件字符集全部契约测试。 */
int main(void)
{
	testMailCharsetConvert();
	testMailCharsetErrors();
	return 0;
}
