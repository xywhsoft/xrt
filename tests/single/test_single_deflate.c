#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件保留确定性 gzip 编码和拥有型结果。 */
int main(void)
{
	xdeflateconfig Config;
	bytes pOutput;
	size_t iSize;
	bool bPass;

	xrtDeflateConfigInit(&Config);
	pOutput = xrtDeflateAll(
		XRT_BYTES_LITERAL("hello"),
		&Config,
		&iSize
	);
	bPass = (pOutput != NULL) &&
		(iSize > 18u) &&
		(memcmp(
			pOutput,
			"\x1f\x8b\x08\x00",
			4
		 ) == 0) &&
		(pOutput[iSize] == 0);
	xrtFree(pOutput);
	if ( !bPass ) {
		return 1;
	}
	printf("[PASS] single-deflate\n");
	return 0;
}
