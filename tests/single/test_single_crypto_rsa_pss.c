#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../crypto/rsa_fixture.h"



/* 验证单头文件能够独立提供 RSA-PSS 验签。 */
int main(void)
{
	xrsapublickey Key;
	uint8 Modulus[128];
	uint8 Hash[32];
	uint8 Signature[128];

	return !__xrtTestRsaFixture(&Key, Modulus, Hash) ||
		!__xrtTestRsaHex(__xrtTestRsaPssHex, Signature, sizeof(Signature)) ||
		!xrtRsaPssVerify(
			&Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			32u,
			Hash,
			Signature,
			sizeof(Signature)
		);
}
