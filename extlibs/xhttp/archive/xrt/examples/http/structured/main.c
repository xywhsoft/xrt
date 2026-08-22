#include <stdio.h>

#include <xrt/http_structured.h>



/* 读取 Priority 风格 Dictionary，同时保留未知扩展成员。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL("u=2, i, future=(a b);v=1");
	xhttpstructuredmapcursor Cursor;
	xhttpstructureddictionarymember Member;
	xhttpnext Next;

	xrtHttpStructuredMapCursorInit(&Cursor);
	while ( (Next = xrtHttpStructuredDictionaryMapNext(
		Value, &Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		printf(
			"member = %.*s\n",
			(int)Member.Key.Size, Member.Key.Data
		);
	}
	return Next == XHTTP_NEXT_END ? 0 : 1;
}
