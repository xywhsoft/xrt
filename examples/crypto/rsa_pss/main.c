#include "../rsa_fixture.h"

#include <stdio.h>



/* 展示 RSA 原始运算、显式盐 PSS 和随机盐便利入口。 */
int main(void)
{
	__xrt_example_rsa_fixture Fixture;
	uint8 Raw[128] = { 0 };
	uint8 Cipher[128];
	uint8 Plain[128];
	uint8 Signature[128];
	bool bValid = false;

	/* 原始运算只用于说明底层层次，普通消息不得直接这样加密。 */
	Raw[sizeof(Raw) - 1u] = 2u;
	if ( !__xrtExampleRsaInit(&Fixture) ||
		 !xrtRsaPublic(
			&Fixture.Key.Public, Raw, sizeof(Raw), Cipher
		 ) || !xrtRsaPrivate(
			&Fixture.Key, Cipher, sizeof(Cipher), Plain
		 ) || !xrtConstTimeEqual(Raw, Plain, sizeof(Raw)) ) {
		goto cleanup;
	}

	/* 显式盐入口适合协议层，随机盐入口覆盖常见签名路径。 */
	if ( !xrtRsaPssSignSalt(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			Fixture.Salt,
			sizeof(Fixture.Salt),
			Fixture.Hash,
			Signature
		) || !xrtRsaPssVerify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			sizeof(Fixture.Salt),
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		) || !xrtRsaPssSign(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature
		) || !xrtRsaPssVerify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			XRT_RSA_PSS_SALT_ANY,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		) ) {
		goto cleanup;
	}
	bValid = true;

cleanup:
	xrtSecureZero(&Fixture, sizeof(Fixture));
	xrtSecureZero(Plain, sizeof(Plain));
	xrtSecureZero(Signature, sizeof(Signature));
	printf("RSA-PSS: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
