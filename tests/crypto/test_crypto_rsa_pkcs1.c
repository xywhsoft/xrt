#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证 RSA PKCS#1 v1.5 的固定向量和损坏拒绝。 */
int main(void)
{
	xrsapublickey Key;
	uint8 Modulus[128];
	uint8 Hash[32];
	uint8 Hash224[28];
	uint8 Signature[128];
	uint8 Damaged[128];

	if ( !__xrtTestRsaFixture(&Key, Modulus, Hash) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPkcs1Hex,
			Signature,
			sizeof(Signature)
		) ) {
		return 1;
	}
	if ( !xrtRsaPkcs1Verify(
		&Key,
		XCRYPTO_HASH_SHA256,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 2;
	}
	if ( xrtRsaPkcs1Verify(
		&Key,
		XCRYPTO_HASH_SHA384,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 3;
	}
	if ( !__xrtTestRsaHex(
		__xrtTestRsaSha224HashHex, Hash224, sizeof(Hash224)
	) || !__xrtTestRsaHex(
		__xrtTestRsaPkcs1Sha224Hex, Signature, sizeof(Signature)
	) || !xrtRsaPkcs1Verify(
		&Key,
		XCRYPTO_HASH_SHA224,
		Hash224,
		Signature,
		sizeof(Signature)
	) ) {
		return 4;
	}
	if ( !__xrtTestRsaHex(
		__xrtTestRsaPkcs1Hex, Signature, sizeof(Signature)
	) ) {
		return 5;
	}
	memcpy(Damaged, Signature, sizeof(Damaged));
	Damaged[17] ^= 0x80u;
	if ( xrtRsaPkcs1Verify(
		&Key,
		XCRYPTO_HASH_SHA256,
		Hash,
		Damaged,
		sizeof(Damaged)
	) ) {
		return 6;
	}
	Hash[31] ^= 1u;
	if ( xrtRsaPkcs1Verify(
		&Key,
		XCRYPTO_HASH_SHA256,
		Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 7;
	}
	if ( xrtGetError() == NULL ) {
		return 8;
	}

	printf("[PASS] crypto_rsa_pkcs1\n");
	return 0;
}
