#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只启用 crypto core 时必须提供安全清零和常量时间比较。 */
int main(void)
{
	uint8 arrData[16];
	uint8 arrZero[16] = {0};

	memset(arrData, 0xA5, sizeof(arrData));
	xrtSecureZero(arrData, sizeof(arrData));
	return (xrtCryptoHashSize(XCRYPTO_HASH_SHA512) == XRT_SHA512_SIZE) &&
		xrtConstTimeEqual(arrData, arrZero, sizeof(arrData)) ? 0 : 1;
}
