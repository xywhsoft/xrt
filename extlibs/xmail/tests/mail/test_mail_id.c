#include "../test.h"



/* 从调用方缓冲构造不依赖 string 模块的视图。 */
static xstrview testMailIdView(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 验证 Message-ID 解析和随机生成。 */
static void testMailMessageId(void)
{
	char arrOutput[128];
	xmailmessageidview MessageId;
	size_t iSize = 0;

	testRequire(xrtMailMessageIdParse(
		XRT_STR_LITERAL(" <abc.123@example.com> "),
		XMAIL_ID_DEFAULT,
		&MessageId
	) && (MessageId.Left.Size == 7u) && (MessageId.Right.Size == 11u),
		"mail Message-ID parse mismatch");
	testRequire(xrtMailMessageIdWrite(
		XRT_STR_LITERAL("example.com"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && xrtMailMessageIdParse(
		testMailIdView(arrOutput, iSize),
		XMAIL_ID_DEFAULT,
		&MessageId
	) && (MessageId.Left.Size == 32u),
		"generated mail Message-ID is invalid");
	testRequire(!xrtMailMessageIdParse(
		XRT_STR_LITERAL("<a..b@example.com>"),
		XMAIL_ID_DEFAULT,
		&MessageId
	), "mail Message-ID accepted an empty atom");
	testRequire(xrtMailMessageIdParse(
		XRT_STR_LITERAL("<a@[tag@value]>"),
		XMAIL_ID_DEFAULT,
		&MessageId
	), "mail Message-ID rejected at sign inside id-right literal");
	{
		static const char sInvalidUtf8[] = { '<', (char)0xC0, '@', 'x', '>', 0 };

		testRequire(!xrtMailMessageIdParse(
			XRT_STR_LITERAL(sInvalidUtf8),
			XMAIL_ID_UTF8,
			&MessageId
		), "mail Message-ID accepted invalid UTF-8");
	}
}



/* 验证 boundary 语法、随机生成和短缓冲。 */
static void testMailBoundary(void)
{
	char arrOutput[128];
	char arrSmall[8] = "keep";
	size_t iSize = 0;

	testRequire(xrtMailBoundaryValid(XRT_STR_LITERAL("simple-boundary")) &&
		!xrtMailBoundaryValid(XRT_STR_LITERAL("bad boundary ")) &&
		!xrtMailBoundaryValid(XRT_STR_LITERAL("bad@boundary")),
		"mail boundary validation mismatch");
	testRequire(xrtMailBoundaryWrite(
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && xrtMailBoundaryValid(testMailIdView(arrOutput, iSize)),
		"generated mail boundary is invalid");
	testRequire(!xrtMailBoundaryWrite(arrSmall, sizeof(arrSmall), &iSize) &&
		(memcmp(arrSmall, "keep", 5u) == 0) && (iSize > sizeof(arrSmall)),
		"mail boundary short buffer published partial output");
}



/* 运行邮件标识全部契约测试。 */
int main(void)
{
	testMailMessageId();
	testMailBoundary();
	return 0;
}
