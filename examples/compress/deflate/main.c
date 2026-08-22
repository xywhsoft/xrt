#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示一次性生成确定性 gzip 数据。 */
int main(void)
{
	static const char Text[] =
		"repeat repeat repeat repeat repeat";
	xdeflateconfig Config;
	bytes pGzip;
	size_t iSize;

	xrtDeflateConfigInit(&Config);
	pGzip = xrtDeflateAll(
		XRT_BYTES_LITERAL(Text),
		&Config,
		&iSize
	);
	if ( pGzip == NULL ) {
		return 1;
	}
	printf(
		"plain=%u gzip=%u\n",
		(unsigned int)(sizeof(Text) - 1u),
		(unsigned int)iSize
	);
	xrtFree(pGzip);
	return 0;
}
