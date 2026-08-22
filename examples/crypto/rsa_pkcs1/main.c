#include "../rsa_fixture.h"

#include <stdio.h>



/* 展示严格的 RSA PKCS#1 v1.5 签名与验签。 */
int main(void)
{
	__xrt_example_rsa_fixture Fixture;
	uint8 Signature[128];
	bool bValid = false;

	/* 新协议通常应优先选择 PSS，本入口用于既有协议互操作。 */
	if ( __xrtExampleRsaInit(&Fixture) &&
		 xrtRsaPkcs1Sign(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature
		 ) && xrtRsaPkcs1Verify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		 ) ) {
		bValid = true;
	}
	xrtSecureZero(&Fixture, sizeof(Fixture));
	xrtSecureZero(Signature, sizeof(Signature));

	printf("RSA PKCS#1 v1.5: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
