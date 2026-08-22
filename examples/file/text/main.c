#include <stdio.h>

#include <xrt.h>



/* 展示 UTF-8 文本与带 BOM UTF-16 文件之间的一次往返。 */
int main(void)
{
	static const char sPath[] = "xrt-file-text-example.tmp";
	str sText;
	size_t iSize;

	if ( !xrtFileWriteText(sPath, XRT_STR_LITERAL("Hello, XRT"),
		XENCODING_UTF16_LE, XUTF_STRICT, true) ) {
		return 1;
	}
	sText = xrtFileReadText(sPath, XENCODING_UNKNOWN, XUTF_STRICT, &iSize);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return xrtFileDelete(sPath) ? 0 : 1;
}
