#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件保留 gzip 校验和整块便捷路径。 */
int main(void)
{
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07,
		0x00, 0x86, 0xA6, 0x10, 0x36, 0x05, 0x00, 0x00,
		0x00
	};
	xinflateconfig Config;
	bytes pOutput;
	size_t iSize;

	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	pOutput = xrtInflateAll(
		(xbytesview){ Gzip, sizeof(Gzip) },
		&Config,
		&iSize
	);
	if ( (pOutput == NULL) || (iSize != 5) ||
		(memcmp(pOutput, "hello", 5) != 0) ) {
		xrtFree(pOutput);
		return 1;
	}
	xrtFree(pOutput);
	printf("[PASS] single-inflate\n");
	return 0;
}
