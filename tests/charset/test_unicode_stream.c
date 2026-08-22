#include "../test.h"



/* 每个可能的 UTF-8 分块位置都必须和整块校验一致。 */
static void testEverySplit(void)
{
	static const char sText[] = "A\xE4\xBD\xA0\xF0\x9F\x98\x80Z";
	const size_t iSize = sizeof(sText) - 1u;

	for ( size_t i = 0; i <= iSize; i++ ) {
		xutf8state State;
		xutfstatus First;
		xutfstatus Second;

		xrtUtf8StateInit(&State);
		First = xrtUtf8StateFeed(&State, (xstrview){ sText, i }, false);
		Second = xrtUtf8StateFeed(&State,
			(xstrview){ sText + i, iSize - i }, true);
		testRequire((First == XUTF_OK) || (First == XUTF_MORE),
			"valid first UTF-8 chunk failed");
		testRequire(Second == XUTF_OK, "valid split UTF-8 failed");
		testRequire(xrtUtf8StateError(&State) == XRT_NPOS,
			"valid stream recorded an error");
	}
}



/* 跨分块错误和最终截断必须返回原始流中的绝对位置。 */
static void testErrors(void)
{
	xutf8state State;
	static const char arrFirst[] = { 'A', (char)0xE1, (char)0x80 };
	static const char arrBadTail[] = { 'B' };

	xrtUtf8StateInit(&State);
	testRequire(xrtUtf8StateFeed(&State,
		(xstrview){ arrFirst, sizeof(arrFirst) }, false) == XUTF_MORE,
		"trailing UTF-8 prefix was not retained");
	testRequire(xrtUtf8StateFeed(&State,
		(xstrview){ arrBadTail, sizeof(arrBadTail) }, true) == XUTF_INVALID,
		"cross-chunk invalid UTF-8 accepted");
	testRequire(xrtUtf8StateError(&State) == 1,
		"cross-chunk UTF-8 error offset is wrong");

	xrtUtf8StateInit(&State);
	testRequire(xrtUtf8StateFeed(&State,
		(xstrview){ arrFirst, sizeof(arrFirst) }, true) == XUTF_INVALID,
		"final truncated UTF-8 accepted");
	testRequire(xrtUtf8StateError(&State) == 1,
		"truncated UTF-8 error offset is wrong");
}



/* 执行流式 UTF-8 边界测试。 */
int main(void)
{
	testEverySplit();
	testErrors();
	return 0;
}
