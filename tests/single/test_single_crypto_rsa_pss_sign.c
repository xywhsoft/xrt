#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../crypto/rsa_fixture.h"



/* 验证单头文件能够以显式盐生成并验证 RSA-PSS 签名。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	uint8 Signature[128];

	return !__xrtTestRsaPrivateFixture(&Fixture) ||
		!xrtRsaPssSignSalt(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			Fixture.Salt,
			sizeof(Fixture.Salt),
			Fixture.Hash,
			Signature
		) ||
		!xrtRsaPssVerify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			32u,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		);
}
