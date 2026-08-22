#include <stdio.h>

#include <xrt.h>



/* 演示直接生成带 BOM 的 UTF-16 LE 封包。 */
int main(void)
{
	xbytesview Source = XRT_BYTES_LITERAL("XRT \xE4\xBD\xA0\xE5\xA5\xBD");
	bytes pUtf16;
	size_t iSize = 0;

	pUtf16 = xrtTranscode(Source, XENCODING_UTF8, XENCODING_UTF16_LE,
		XUTF_STRICT, true, &iSize);
	if ( pUtf16 == NULL ) {
		return 1;
	}
	printf("UTF-16 LE bytes with BOM: %llu\n", (unsigned long long)iSize);
	xrtFree(pUtf16);
	return 0;
}
