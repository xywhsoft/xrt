#include <xmail.h>



/* 逐项读取 multipart 的直接子部分，不创建对象树。 */
static bool inspectBody(xstrview Text)
{
	ximapbodyview Body;
	ximapbodyview Child;
	ximapbodycursor Cursor;
	xmailnext Next;

	if ( !xrtImapBodyParse(Text, &Body) ) {
		return false;
	}
	if ( Body.Kind != XIMAP_BODY_MULTIPART ) {
		return true;
	}
	if ( !xrtImapBodyChildCursorInit(&Cursor, &Body) ) {
		return false;
	}
	while ( (Next = xrtImapBodyChildNext(&Cursor, &Child)) ==
		XMAIL_NEXT_ITEM ) {
		(void)Child;
	}
	return Next == XMAIL_NEXT_END;
}



/* 示例输入通常来自 FETCH BODYSTRUCTURE 属性值。 */
int main(void)
{
	return inspectBody(XRT_STR_LITERAL(
		"((\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 5 1) "
		"\"ALTERNATIVE\")"
	)) ? 0 : 1;
}
