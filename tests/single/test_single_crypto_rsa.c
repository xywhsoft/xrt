#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../crypto/rsa_fixture.h"

#include <string.h>



/* 验证单头文件能够独立提供 RSA 公钥运算。 */
int main(void)
{
	xrsapublickey Key;
	uint8 Modulus[128];
	uint8 Hash[32];
	uint8 Input[128] = { 0 };
	uint8 Expected[128];

	Input[127] = 2u;
	return !__xrtTestRsaFixture(&Key, Modulus, Hash) ||
		!__xrtTestRsaHex(__xrtTestRsaRawHex, Expected, sizeof(Expected)) ||
		!xrtRsaPublic(&Key, Input, sizeof(Input), Input) ||
		(memcmp(Input, Expected, sizeof(Input)) != 0);
}
