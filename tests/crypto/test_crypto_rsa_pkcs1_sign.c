#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证 PKCS#1 固定向量、规范验签、重叠和失败原子性。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	uint8 Expected[128];
	uint8 Signature[128];
	uint8 Snapshot[128];
	uint8 Overlap[128];
	uint8 SavedExponent;

	if ( !__xrtTestRsaPrivateFixture(&Fixture) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPkcs1Hex,
			Expected,
			sizeof(Expected)
		 ) ) {
		return 1;
	}
	if ( !xrtRsaPkcs1Sign(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		Fixture.Hash,
		Signature
	) || (memcmp(Signature, Expected, sizeof(Signature)) != 0) ) {
		return 2;
	}
	if ( !xrtRsaPkcs1Verify(
		&Fixture.Key.Public,
		XCRYPTO_HASH_SHA256,
		Fixture.Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 3;
	}

	memset(Overlap, 0, sizeof(Overlap));
	memcpy(Overlap, Fixture.Hash, sizeof(Fixture.Hash));
	if ( !xrtRsaPkcs1Sign(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		Overlap,
		Overlap
	) || (memcmp(Overlap, Expected, sizeof(Overlap)) != 0) ) {
		return 4;
	}

	memset(Signature, 0xA5, sizeof(Signature));
	memcpy(Snapshot, Signature, sizeof(Signature));
	SavedExponent = Fixture.Exponent1[0];
	Fixture.Exponent1[0] ^= 1u;
	if ( xrtRsaPkcs1Sign(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		Fixture.Hash,
		Signature
	) || (memcmp(Signature, Snapshot, sizeof(Signature)) != 0) ) {
		return 5;
	}
	Fixture.Exponent1[0] = SavedExponent;
	if ( xrtRsaPkcs1Sign(
		&Fixture.Key,
		(xcryptohash)0,
		Fixture.Hash,
		Signature
	) || (xrtGetError() == NULL) ) {
		return 6;
	}

	printf("[PASS] crypto_rsa_pkcs1_sign\n");
	return 0;
}
