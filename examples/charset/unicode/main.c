#include <stdio.h>

#include <xrt.h>



/* 演示 UTF-8 主线与 UTF-16 平台边界之间的严格往返。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL("XRT \xE4\xBD\xA0\xE5\xA5\xBD \xF0\x9F\x98\x80");
	uint16* pUtf16;
	uint16* pUtf16Copy;
	str sUtf8;
	size_t iUnits = 0;
	xstrview Word;

	pUtf16 = xrtUtf8ViewTo16(Text, XUTF_STRICT, &iUnits);
	if ( pUtf16 == NULL ) {
		return 1;
	}
	pUtf16Copy = xrtUtf16DupView(xrtUtf16View(pUtf16, iUnits));
	if ( pUtf16Copy == NULL ) {
		xrtFree(pUtf16);
		return 1;
	}
	sUtf8 = xrtUtf16ViewTo8(xrtUtf16View(pUtf16, iUnits), XUTF_STRICT, NULL);
	if ( sUtf8 == NULL ) {
		xrtFree(pUtf16Copy);
		xrtFree(pUtf16);
		return 1;
	}
	printf("UTF-16 units: %llu\n%s\n", (unsigned long long)iUnits, sUtf8);
	if ( !xrtUtf8Slice(Text, 4, 2, &Word) ) {
		xrtFree(sUtf8);
		xrtFree(pUtf16Copy);
		xrtFree(pUtf16);
		return 1;
	}
	printf("scalar slice: %.*s\n", (int)Word.Size, Word.Data);
	xrtFree(sUtf8);
	xrtFree(pUtf16Copy);
	xrtFree(pUtf16);
	return 0;
}
