#include "../test.h"



/* 验证 Unicode 标量距离、带状阈值、相似度和非法输入。 */
int main(void)
{
	static const char arrInvalid[] = { (char)0xF0, (char)0x80, (char)0x80, (char)0x80 };
	double fSimilarity;

	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("kitten"), XRT_STR_LITERAL("sitting"),
		XRT_NPOS) == 3, "ASCII edit distance mismatch");
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("你好"), XRT_STR_LITERAL("你们"),
		XRT_NPOS) == 1, "Unicode scalar distance mismatch");
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("😀a"), XRT_STR_LITERAL("😀b"),
		XRT_NPOS) == 1, "supplementary scalar distance mismatch");
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("abcdef"),
		2) == XRT_NPOS, "distance limit mismatch");
	testRequire(xrtGetError() == NULL, "distance limit should not set an error");
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("kitten"), XRT_STR_LITERAL("sitting"),
		3) == 3, "banded edit distance mismatch");

	fSimilarity = xrtUtf8Similarity(XRT_STR_LITERAL("kitten"), XRT_STR_LITERAL("sitting"));
	testRequire((fSimilarity > 0.5714) && (fSimilarity < 0.5715),
		"normalized similarity mismatch");
	testRequire(xrtUtf8Similarity(XRT_STR_LITERAL(""), XRT_STR_LITERAL("")) == 1.0,
		"empty similarity mismatch");

	xrtClearError();
	testRequire(xrtUtf8Distance((xstrview){ arrInvalid, sizeof(arrInvalid) },
		XRT_STR_LITERAL("x"), XRT_NPOS) == XRT_NPOS,
		"invalid UTF-8 distance input must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_INVALID),
		"invalid UTF-8 distance error mismatch");
	xrtClearError();

	printf("[PASS] unicode-distance\n");
	return 0;
}
