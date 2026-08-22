#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证 CRT、完整指数、原位运算、故障复核和失败原子性。 */
int main(void)
{
	__xrt_test_rsa_private_fixture Fixture;
	xrsaprivatekey Key;
	uint8 Cipher[128];
	uint8 Plain[128] = { 0 };
	uint8 Output[128];
	uint8 Snapshot[128];
	uint8 FaultInput[128];
	uint8 Verified[128];
	uint8 SavedCoefficient;

	Plain[127] = 2u;
	if ( !__xrtTestRsaPrivateFixture(&Fixture) ||
		 !__xrtTestRsaHex(__xrtTestRsaRawHex, Cipher, sizeof(Cipher)) ) {
		return 1;
	}
	if ( !xrtRsaPrivate(
		&Fixture.Key,
		Cipher,
		sizeof(Cipher),
		Output
	) || (memcmp(Output, Plain, sizeof(Output)) != 0) ) {
		return 2;
	}

	memcpy(Output, Cipher, sizeof(Output));
	if ( !xrtRsaPrivate(
		&Fixture.Key,
		Output,
		sizeof(Output),
		Output
	) || (memcmp(Output, Plain, sizeof(Output)) != 0) ) {
		return 3;
	}

	/* 清空全部 CRT 字段后必须走完整私有指数路径。 */
	Key = Fixture.Key;
	Key.Prime1 = NULL;
	Key.Prime1Size = 0;
	Key.Prime2 = NULL;
	Key.Prime2Size = 0;
	Key.Exponent1 = NULL;
	Key.Exponent1Size = 0;
	Key.Exponent2 = NULL;
	Key.Exponent2Size = 0;
	Key.Coefficient = NULL;
	Key.CoefficientSize = 0;
	if ( !xrtRsaPrivate(&Key, Cipher, sizeof(Cipher), Output) ||
		 (memcmp(Output, Plain, sizeof(Output)) != 0) ) {
		return 4;
	}

	/* CRT 系数故障必须被公开指数复核捕获，且不能发布半成品。 */
	memcpy(FaultInput, Cipher, sizeof(FaultInput));
	FaultInput[127] ^= 1u;
	if ( !xrtRsaPrivate(
		&Fixture.Key,
		FaultInput,
		sizeof(FaultInput),
		Output
	) || !xrtRsaPublic(
		&Fixture.Key.Public,
		Output,
		sizeof(Output),
		Verified
	) || (memcmp(Verified, FaultInput, sizeof(Verified)) != 0) ) {
		return 5;
	}
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Snapshot, Output, sizeof(Output));
	SavedCoefficient = Fixture.Coefficient[0];
	Fixture.Coefficient[0] ^= 1u;
	if ( xrtRsaPrivate(
		&Fixture.Key,
		FaultInput,
		sizeof(FaultInput),
		Output
	) || (memcmp(Output, Snapshot, sizeof(Output)) != 0) ) {
		return 6;
	}
	Fixture.Coefficient[0] = SavedCoefficient;

	/* 部分 CRT、等于模数的代表元和空参数均必须失败。 */
	Key = Fixture.Key;
	Key.Exponent2 = NULL;
	Key.Exponent2Size = 0;
	if ( xrtRsaPrivate(&Key, Cipher, sizeof(Cipher), Output) ) {
		return 7;
	}
	Key = Fixture.Key;
	Key.Prime1Size = SIZE_MAX;
	if ( xrtRsaPrivate(&Key, Cipher, sizeof(Cipher), Output) ) {
		return 8;
	}
	if ( xrtRsaPrivate(
		&Fixture.Key,
		Fixture.Modulus,
		sizeof(Fixture.Modulus),
		Output
	) ) {
		return 9;
	}
	if ( xrtRsaPrivate(NULL, Cipher, sizeof(Cipher), Output) ||
		 (xrtGetError() == NULL) ) {
		return 10;
	}

	printf("[PASS] crypto_rsa_private\n");
	return 0;
}
