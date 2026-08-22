#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证 RSA-PSS 的固定向量、参数约束和损坏拒绝。 */
int main(void)
{
	xrsapublickey Key;
	uint8 Modulus[128];
	uint8 Hash[32];
	uint8 Hash224[28];
	uint8 Signature[128];
	uint8 Damaged[128];

	if ( !__xrtTestRsaFixture(&Key, Modulus, Hash) ||
		 !__xrtTestRsaHex(__xrtTestRsaPssHex, Signature, sizeof(Signature)) ) {
		return 1;
	}
	if ( !xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		32u,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 2;
	}
	if ( !xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		XRT_RSA_PSS_SALT_ANY,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 3;
	}
	if ( xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		31u,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 4;
	}
	if ( xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA384,
		32u,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 5;
	}
	if ( !__xrtTestRsaHex(
		__xrtTestRsaSha224HashHex, Hash224, sizeof(Hash224)
	) || !__xrtTestRsaHex(
		__xrtTestRsaPssSha224Hex, Signature, sizeof(Signature)
	) || !xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA224,
		XCRYPTO_HASH_SHA224,
		28u,
		Hash224,
		Signature,
		sizeof(Signature)
	) ) {
		return 6;
	}
	if ( !__xrtTestRsaHex(
		__xrtTestRsaPssHex, Signature, sizeof(Signature)
	) ) {
		return 7;
	}

	memcpy(Damaged, Signature, sizeof(Damaged));
	Damaged[63] ^= 1u;
	if ( xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		32u,
		Hash,
		Damaged,
		sizeof(Damaged)
	) ) {
		return 8;
	}
	Hash[0] ^= 1u;
	if ( xrtRsaPssVerify(
		&Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		32u,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 9;
	}
	if ( xrtGetError() == NULL ) {
		return 10;
	}

	printf("[PASS] crypto_rsa_pss\n");
	return 0;
}
