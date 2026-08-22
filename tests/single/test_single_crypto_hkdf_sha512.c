#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 HKDF-SHA384 与 HKDF-SHA512 组合入口。 */
int main(void)
{
	uint8 arrOkm384[42];
	uint8 arrOkm512[42];

	return (!xrtHkdfSha384(NULL, 0, "ikm", 3, NULL, 0,
			arrOkm384, sizeof(arrOkm384)) ||
		!xrtHkdfSha512(NULL, 0, "ikm", 3, NULL, 0,
			arrOkm512, sizeof(arrOkm512)) ||
		xrtConstTimeEqual(arrOkm384, arrOkm512, sizeof(arrOkm384))) ? 1 : 0;
}
