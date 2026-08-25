#include <stdio.h>

#include <xmail.h>



/* 把旧邮件常见的 Windows-1252 字节转换成 UTF-8。 */
int main(void)
{
	static const unsigned char arrText[] = {
		0x93u, 'x', 'm', 'a', 'i', 'l', 0x94u
	};
	str sText = xrtMailCharsetToUtf8(
		XRT_STR_LITERAL("windows-1252"),
		(xbytesview) { arrText, sizeof(arrText) },
		NULL
	);

	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
