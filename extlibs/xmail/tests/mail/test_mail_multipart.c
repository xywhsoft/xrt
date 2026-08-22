#include "../test.h"



/* 比较借用视图与常量文本。 */
static bool testMailMultipartEqual(xstrview Text, const char* sValue)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) && (memcmp(Text.Data, sValue, iSize) == 0);
}



/* 验证 preamble、part、关闭分隔线和 epilogue。 */
static void testMailMultipartCursor(void)
{
	static const char sBody[] =
		"preamble\r\n"
		"--sample\r\n"
		"Content-Type: text/plain\r\n"
		"X-Test: one\r\n"
		"\r\n"
		"first\r\n"
		"not--sample\r\n"
		"--sample\r\n"
		"\r\n"
		"second\r\n"
		"--sample--\r\n"
		"epilogue";
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;

	testRequire(xrtMailMultipartCursorInit(
		&Cursor,
		XRT_STR_LITERAL(sBody),
		XRT_STR_LITERAL("sample"),
		0
	) && testMailMultipartEqual(Cursor.Preamble, "preamble"),
		"mail multipart preamble mismatch");
	testRequire(xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_ITEM &&
		testMailMultipartEqual(
			Part.Headers,
			"Content-Type: text/plain\r\nX-Test: one"
		) && testMailMultipartEqual(Part.Body, "first\r\nnot--sample"),
		"mail multipart first part mismatch");
	testRequire(xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_ITEM &&
		(Part.Headers.Size == 0) && testMailMultipartEqual(Part.Body, "second") &&
		Cursor.Closed && testMailMultipartEqual(Cursor.Epilogue, "epilogue"),
		"mail multipart second part or epilogue mismatch");
	testRequire(xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_END,
		"mail multipart cursor did not reach end");
}



/* 验证流式构建片段。 */
static void testMailMultipartWrite(void)
{
	char arrOutput[64];
	size_t iSize = 0;

	testRequire(xrtMailMultipartMarkWrite(
		XRT_STR_LITERAL("b"),
		XMAIL_MULTIPART_FIRST,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "--b\r\n") == 0),
		"mail multipart first marker mismatch");
	testRequire(xrtMailMultipartMarkWrite(
		XRT_STR_LITERAL("b"),
		XMAIL_MULTIPART_NEXT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "\r\n--b\r\n") == 0),
		"mail multipart next marker mismatch");
	testRequire(xrtMailMultipartMarkWrite(
		XRT_STR_LITERAL("b"),
		XMAIL_MULTIPART_CLOSE,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "\r\n--b--\r\n") == 0),
		"mail multipart close marker mismatch");
}



/* 验证预算与缺失关闭分隔线。 */
static void testMailMultipartErrors(void)
{
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;
	xmailmultipartview Before;

	testRequire(xrtMailMultipartCursorInit(
		&Cursor,
		XRT_STR_LITERAL("--b\r\n\r\none\r\n--b\r\n\r\ntwo\r\n--b--\r\n"),
		XRT_STR_LITERAL("b"),
		1u
	), "mail multipart limit cursor init failed");
	testRequire(xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_ITEM,
		"mail multipart limit first part failed");
	Before = Part;
	testRequire(xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_ERROR &&
		(Part.Source.Data == Before.Source.Data),
		"mail multipart part limit did not preserve output");
	testRequire(xrtMailMultipartCursorInit(
		&Cursor,
		XRT_STR_LITERAL("--b\r\n\r\nunterminated"),
		XRT_STR_LITERAL("b"),
		0
	) && (xrtMailMultipartNext(&Cursor, &Part) == XMAIL_NEXT_ERROR),
		"mail multipart accepted a missing closing boundary");
}



/* 运行 multipart 全部契约测试。 */
int main(void)
{
	testMailMultipartCursor();
	testMailMultipartWrite();
	testMailMultipartErrors();
	return 0;
}
