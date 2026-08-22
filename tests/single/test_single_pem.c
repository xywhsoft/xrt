#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 PEM 规范编码、解析和二进制解码。 */
int main(void)
{
	char Text[96];
	uint8 Data[4];
	xpemcursor Cursor;
	xpemblock Block;
	size_t iTextSize;
	size_t iDataSize;

	if ( !xrtPemEncode(
		"DATA", "Man", 3, Text, sizeof(Text), &iTextSize
	) || !xrtPemInit(&Cursor, Text, iTextSize) ||
		(xrtPemRead(&Cursor, &Block) != XPEM_BLOCK) ||
		!xrtPemDecode(&Block, Data, sizeof(Data), &iDataSize) ||
		(iDataSize != 3u) || (memcmp(Data, "Man", 3) != 0) ) {
		return 1;
	}
	printf("[PASS] single-pem\n");
	return 0;
}
