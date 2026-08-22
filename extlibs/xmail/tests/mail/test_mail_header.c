#include "../test.h"



/* 验证字段名、字段值和注入边界。 */
static void testMailHeaderValidation(void)
{
	testRequire(xrtMailHeaderNameValid(XRT_STR_LITERAL("X-Mail-Test")),
		"valid mail header name rejected");
	testRequire(!xrtMailHeaderNameValid(XRT_STR_LITERAL("Bad:Name")),
		"mail header name with colon accepted");
	testRequire(xrtMailHeaderValueValid(XRT_STR_LITERAL("one\r\n two")),
		"valid folded mail header value rejected");
	testRequire(!xrtMailHeaderValueValid(XRT_STR_LITERAL("one\r\nInjected: yes")),
		"mail header injection value accepted");
	testRequire(!xrtMailHeaderValueValid(XRT_STR_LITERAL("one\n two")),
		"bare LF mail header value accepted");
}



/* 验证字段折叠、展开和容量失败不发布半包。 */
static void testMailHeaderWrite(void)
{
	static const char sExpected[] =
		"Subject: one\r\n two three\r\n";
	char arrOutput[128];
	char arrInPlace[64] = "one\r\n  two";
	size_t iSize = 0;
	str sHeader;

	testRequire(xrtMailHeaderWrite(
		XRT_STR_LITERAL("Subject"),
		XRT_STR_LITERAL("one two three"),
		12u,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, sExpected) == 0),
		"mail header folding mismatch");
	testRequire(xrtMailHeaderUnfoldWrite(
		testMailView(arrInPlace),
		arrInPlace,
		sizeof(arrInPlace),
		&iSize
	) && (strcmp(arrInPlace, "one two") == 0),
		"mail header in-place unfold mismatch");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtMailHeaderWrite(
		XRT_STR_LITERAL("Subject"),
		XRT_STR_LITERAL("one two"),
		0,
		arrOutput,
		5u,
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0),
		"short mail header buffer published partial output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"short mail header buffer error mismatch");

	sHeader = xrtMailHeader(
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("value"),
		0,
		&iSize
	);
	testRequire((sHeader != NULL) && (strcmp(sHeader, "X-Test: value\r\n") == 0),
		"allocated mail header mismatch");
	xrtFree(sHeader);
}



/* 验证零分配字段游标保留重复字段和折叠值。 */
static void testMailHeaderCursor(void)
{
	static const char sBlock[] =
		"Subject: one\r\n two\r\n"
		"Received: first\r\n"
		"Received: second\r\n"
		"\r\nbody";
	xmailheadercursor Cursor;
	xmailheaderview Header;
	char arrValue[32];
	size_t iSize;

	testRequire(xrtMailHeaderCursorInit(&Cursor, XRT_STR_LITERAL(sBlock)),
		"mail header cursor init failed");
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ITEM,
		"mail header cursor missed first field");
	testRequire(testMailViewCaseEqual(Header.Name, XRT_STR_LITERAL("subject")),
		"mail header cursor first name mismatch");
	testRequire(xrtMailHeaderUnfoldWrite(
		Header.Value,
		arrValue,
		sizeof(arrValue),
		&iSize
	) && (strcmp(arrValue, "one two") == 0),
		"mail header cursor folded value mismatch");
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ITEM &&
		testMailViewEqual(Header.Value, XRT_STR_LITERAL("first")),
		"mail header cursor first repeated value mismatch");
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ITEM &&
		testMailViewEqual(Header.Value, XRT_STR_LITERAL("second")),
		"mail header cursor second repeated value mismatch");
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_END,
		"mail header cursor did not stop at the empty line");
}



/* 验证严格线路语法和 998 字节硬限制。 */
static void testMailHeaderLimits(void)
{
	char arrValue[1600];
	char arrOutput[1800];
	char arrLine[1024];
	xmailheadercursor Cursor;
	xmailheaderview Header;
	size_t iOutputSize;

	for ( size_t i = 0; i < sizeof(arrValue); i++ ) {
		arrValue[i] = (i % 40u) == 39u ? ' ' : 'a';
	}
	testRequire(xrtMailHeaderValueValid(
		testMailViewN(arrValue, sizeof(arrValue))
	), "long logical mail header value was rejected before folding");
	testRequire(xrtMailHeaderWrite(
		XRT_STR_LITERAL("X-Long"),
		testMailViewN(arrValue, sizeof(arrValue)),
		78u,
		arrOutput,
		sizeof(arrOutput),
		&iOutputSize
	) && (strstr(arrOutput, "\r\n ") != NULL),
		"long logical mail header value was not folded");

	memset(arrLine, 'a', sizeof(arrLine));
	memcpy(arrLine, "X: ", 3u);
	arrLine[1002] = '\r';
	arrLine[1003] = '\n';
	testRequire(xrtMailHeaderCursorInit(
		&Cursor,
		testMailViewN(arrLine, 1004u)
	), "long mail header cursor init failed");
	xrtClearError();
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ERROR,
		"mail header line over 998 bytes was accepted");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_HEADER
	) != NULL, "mail header hard limit error mismatch");

	testRequire(xrtMailHeaderCursorInit(
		&Cursor,
		XRT_STR_LITERAL("X: one\n")
	), "bare LF cursor init failed");
	Header.Name = XRT_STR_LITERAL("unchanged-name");
	Header.Value = XRT_STR_LITERAL("unchanged-value");
	xrtClearError();
	testRequire(xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ERROR,
		"bare LF mail header block was accepted");
	testRequire(testMailViewEqual(Header.Name, XRT_STR_LITERAL("unchanged-name")) &&
		testMailViewEqual(Header.Value, XRT_STR_LITERAL("unchanged-value")),
		"mail header parse error modified the output view");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_LINE
	) != NULL, "mail header line error metadata mismatch");
}



/* 运行邮件字段全部契约测试。 */
int main(void)
{
	testMailHeaderValidation();
	testMailHeaderWrite();
	testMailHeaderCursor();
	testMailHeaderLimits();
	return 0;
}
