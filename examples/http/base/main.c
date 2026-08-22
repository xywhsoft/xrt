#include <stdio.h>

#include <xrt.h>



/* 展示传输无关的字段解析与 token-list 迭代。 */
int main(void)
{
	xhttpfield Field;
	xhttpnext Next;
	xstrview Token;
	char Output[128];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtHttpFieldParse(
		XRT_STR_LITERAL("Connection: keep-alive, Upgrade"), &Field
	) ) {
		return 1;
	}
	while ( (Next = xrtHttpTokenNext(
		Field.Value, &iOffset, &Token
	)) == XHTTP_NEXT_ITEM ) {
		printf("%.*s\n", (int)Token.Size, Token.Data);
	}
	if ( (Next != XHTTP_NEXT_END) || !xrtHttpFieldBlockWrite(
		&Field, 1, Output, sizeof(Output), &iSize
	) ) {
		return 2;
	}
	printf("%.*s", (int)iSize, Output);
	return 0;
}
