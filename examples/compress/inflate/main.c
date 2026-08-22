#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示一次性解码带完整 trailer 校验的 gzip 数据。 */
int main(void)
{
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07,
		0x00, 0x86, 0xA6, 0x10, 0x36, 0x05, 0x00, 0x00,
		0x00
	};
	xinflateconfig Config;
	bytes pText;
	size_t iSize;

	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	pText = xrtInflateAll(
		(xbytesview){ Gzip, sizeof(Gzip) },
		&Config,
		&iSize
	);
	if ( pText == NULL ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, (const char*)pText);
	xrtFree(pText);
	return 0;
}
