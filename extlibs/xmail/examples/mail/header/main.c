#include <stdio.h>

#include <xmail.h>



/* 遍历原始字段并只在需要时展开值。 */
int main(void)
{
	static const char sHeaders[] =
		"Subject: xmail content layer\r\n"
		"X-Long: one\r\n two\r\n"
		"\r\n";
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;

	if ( !xrtMailHeaderCursorInit(&Cursor, XRT_STR_LITERAL(sHeaders)) ) {
		return 1;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		str sValue = xrtMailHeaderUnfold(Header.Value, NULL);

		if ( sValue == NULL ) {
			return 2;
		}
		printf("%.*s = %s\n", (int)Header.Name.Size, Header.Name.Data, sValue);
		xrtFree(sValue);
	}
	return Next == XMAIL_NEXT_END ? 0 : 3;
}
