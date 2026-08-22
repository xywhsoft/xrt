#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../crypto/rsa_fixture.h"



/* 验证单头文件能够生成并验证 RSA PKCS#1 v1.5 签名。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	uint8 Signature[128];

	return !__xrtTestRsaPrivateFixture(&Fixture) ||
		!xrtRsaPkcs1Sign(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature
		) ||
		!xrtRsaPkcs1Verify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		);
}
