#include <stdio.h>
#include <xmail.h>



/* 逐段读取 multipart，不复制正文。 */
int main(void)
{
	static const char sBody[] =
		"--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n--b--\r\n";
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;

	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		XRT_STR_LITERAL(sBody),
		XRT_STR_LITERAL("b"),
		0
	) || (xrtMailMultipartNext(&Cursor, &Part) != XMAIL_NEXT_ITEM) ) {
		return 1;
	}
	printf("body=%.*s\n", (int)Part.Body.Size, Part.Body.Data);
	return 0;
}
