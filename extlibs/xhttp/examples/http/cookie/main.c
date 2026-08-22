#include <stdio.h>

#include <xhttp.h>



/* 展示 Cookie 请求字段的直接查找、遍历和规范写出。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL("sid=abc123; theme=dark; sid=backup");
	xcookiepair Pair;
	xcookiepair Output[2];
	char Buffer[64];
	size_t iOffset = 0;
	size_t iSize;

	if ( xrtCookieFind(
		Text, XRT_STR_LITERAL("sid"), &iOffset, &Pair
	) != XCOOKIE_NEXT_ITEM ) {
		return 1;
	}
	printf("sid=%.*s\n", (int)Pair.Value.Size, Pair.Value.Data);

	Output[0].Name = XRT_STR_LITERAL("sid");
	Output[0].Value = XRT_STR_LITERAL("new-session");
	Output[1].Name = XRT_STR_LITERAL("theme");
	Output[1].Value = XRT_STR_LITERAL("dark");
	if ( !xrtCookieWrite(
		Output, 2, Buffer, sizeof(Buffer), &iSize
	) ) {
		return 2;
	}
	printf("Cookie: %.*s\n", (int)iSize, Buffer);
	return 0;
}
