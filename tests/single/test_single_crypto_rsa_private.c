#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../crypto/rsa_fixture.h"

#include <string.h>



/* 验证单头文件能够独立提供带 CRT 的 RSA 私钥运算。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	uint8 Cipher[128];
	uint8 Plain[128] = { 0 };

	Plain[127] = 2u;
	return !__xrtTestRsaPrivateFixture(&Fixture) ||
		!__xrtTestRsaHex(__xrtTestRsaRawHex, Cipher, sizeof(Cipher)) ||
		!xrtRsaPrivate(&Fixture.Key, Cipher, sizeof(Cipher), Cipher) ||
		(memcmp(Cipher, Plain, sizeof(Cipher)) != 0);
}
