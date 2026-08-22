#include <stdio.h>

#include <xrt.h>



/* 演示按 Unicode 标量反转和过滤严格 UTF-8 文本。 */
int main(void)
{
	str sText = xrtUtf8Reverse(XRT_STR_LITERAL("XRT 你好 😀"));
	str sFiltered;
	str sPadded;
	xstrview Range;

	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);

	sFiltered = xrtUtf8Filter(XRT_STR_LITERAL("你好，XRT"),
		XRT_STR_LITERAL("，好"));
	if ( sFiltered == NULL ) {
		return 2;
	}
	printf("%s\n", sFiltered);
	xrtFree(sFiltered);

	if ( !xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), -2, 1, &Range) ) {
		return 3;
	}
	printf("%.*s\n", (int)Range.Size, Range.Data);

	sPadded = xrtUtf8PadCenter(XRT_STR_LITERAL("XRT"), 7,
		XRT_STR_LITERAL("好😀"));
	if ( sPadded == NULL ) {
		return 4;
	}
	printf("%s\n", sPadded);
	xrtFree(sPadded);
	return 0;
}
