#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的严格 Base64 编解码主路径。 */
int main(void)
{
	char Text[16];
	uint8 Data[8];
	size_t iSize;

	if ( !xrtBase64Encode("single", 6, Text, sizeof(Text), &iSize, NULL) ||
		(strcmp(Text, "c2luZ2xl") != 0) ||
		!xrtBase64Decode(Text, iSize, Data, sizeof(Data), &iSize, NULL) ||
		(iSize != 6) || (memcmp(Data, "single", 6) != 0) ) {
		return 1;
	}
	printf("[PASS] single-codec-base64\n");
	return 0;
}
