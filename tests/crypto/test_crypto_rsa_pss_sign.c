#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证显式盐固定向量、随机盐、重叠和失败原子性。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	uint8 Expected[128];
	uint8 Signature[128];
	uint8 Snapshot[128];
	uint8 Overlap[128];
	uint8 LongSalt[65] = { 0 };
	uint8 TooLongSalt[95] = { 0 };

	if ( !__xrtTestRsaPrivateFixture(&Fixture) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPssHex,
			Expected,
			sizeof(Expected)
		 ) ) {
		return 1;
	}
	if ( !xrtRsaPssSignSalt(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		Fixture.Salt,
		sizeof(Fixture.Salt),
		Fixture.Hash,
		Signature
	) || (memcmp(Signature, Expected, sizeof(Signature)) != 0) ) {
		return 2;
	}
	if ( !xrtRsaPssVerify(
		&Fixture.Key.Public,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		32u,
		Fixture.Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 3;
	}

	if ( !xrtRsaPssSign(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		Fixture.Hash,
		Signature
	) || !xrtRsaPssVerify(
		&Fixture.Key.Public,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		32u,
		Fixture.Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 4;
	}

	memset(Overlap, 0, sizeof(Overlap));
	memcpy(Overlap, Fixture.Hash, sizeof(Fixture.Hash));
	if ( !xrtRsaPssSignSalt(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		Fixture.Salt,
		sizeof(Fixture.Salt),
		Overlap,
		Overlap
	) || (memcmp(Overlap, Expected, sizeof(Overlap)) != 0) ) {
		return 5;
	}

	/* 显式盐不受摘要长度限制，只受当前模数的 PSS 编码容量限制。 */
	if ( !xrtRsaPssSignSalt(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		LongSalt,
		sizeof(LongSalt),
		Fixture.Hash,
		Signature
	) || !xrtRsaPssVerify(
		&Fixture.Key.Public,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		sizeof(LongSalt),
		Fixture.Hash,
		Signature,
		sizeof(Signature)
	) ) {
		return 6;
	}

	memset(Signature, 0xA5, sizeof(Signature));
	memcpy(Snapshot, Signature, sizeof(Signature));
	if ( xrtRsaPssSignSalt(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		TooLongSalt,
		sizeof(TooLongSalt),
		Fixture.Hash,
		Signature
	) || (memcmp(Signature, Snapshot, sizeof(Signature)) != 0) ) {
		return 7;
	}
	if ( xrtRsaPssSignSalt(
		&Fixture.Key,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256,
		LongSalt,
		SIZE_MAX,
		Fixture.Hash,
		Signature
	) || (memcmp(Signature, Snapshot, sizeof(Signature)) != 0) ) {
		return 8;
	}
	if ( xrtRsaPssSignSalt(
		&Fixture.Key,
		(xcryptohash)0,
		XCRYPTO_HASH_SHA256,
		NULL,
		0,
		Fixture.Hash,
		Signature
	) || (xrtGetError() == NULL) ) {
		return 9;
	}

	printf("[PASS] crypto_rsa_pss_sign\n");
	return 0;
}
