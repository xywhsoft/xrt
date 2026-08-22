#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供密码安全随机和安全清零。 */
int main(void)
{
	uint8 arrData[32];
	uint8 arrZero[32] = {0};

	if ( !xrtSecureRandom(arrData, sizeof(arrData)) ||
		 (memcmp(arrData, arrZero, sizeof(arrData)) == 0) ) {
		return 1;
	}
	xrtSecureZero(arrData, sizeof(arrData));
	return memcmp(arrData, arrZero, sizeof(arrData)) == 0 ? 0 : 1;
}
