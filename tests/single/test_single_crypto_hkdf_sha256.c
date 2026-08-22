#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 HKDF-SHA256 组合入口。 */
int main(void)
{
	uint8 arrIkm[22];
	uint8 arrSalt[13];
	uint8 arrInfo[10];
	uint8 arrOkm[42];

	memset(arrIkm, 0x0B, sizeof(arrIkm));
	for ( size_t i = 0; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(arrInfo); i++ ) {
		arrInfo[i] = (uint8)(0xF0u + i);
	}
	return (!xrtHkdfSha256(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm),
			arrInfo, sizeof(arrInfo), arrOkm, sizeof(arrOkm)
		) || (arrOkm[0] != 0x3Cu) || (arrOkm[41] != 0x65u)) ? 1 : 0;
}
