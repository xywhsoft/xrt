#include "rsa_fixture.h"

#include <stdio.h>
#include <string.h>



/* 验证 RSA 原始公钥运算、重叠和失败原子性。 */
int main(void)
{
	xrsapublickey Key;
	xrsapublickey Invalid;
	uint8 Modulus[128];
	uint8 Hash[32];
	uint8 Input[128] = { 0 };
	uint8 Expected[128];
	uint8 Output[128];
	uint8 Snapshot[128];
	uint8 EvenExponent[1] = { 2 };

	if ( !__xrtTestRsaFixture(&Key, Modulus, Hash) ||
		 !__xrtTestRsaHex(__xrtTestRsaRawHex, Expected, sizeof(Expected)) ) {
		return 1;
	}
	Input[127] = 2u;
	if ( !xrtRsaPublic(&Key, Input, sizeof(Input), Output) ||
		 (memcmp(Output, Expected, sizeof(Output)) != 0) ) {
		return 2;
	}
	if ( !xrtRsaPublic(&Key, Input, sizeof(Input), Input) ||
		 (memcmp(Input, Expected, sizeof(Input)) != 0) ) {
		return 3;
	}

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Snapshot, Output, sizeof(Output));
	if ( xrtRsaPublic(&Key, Modulus, sizeof(Modulus), Output) ||
		 (memcmp(Output, Snapshot, sizeof(Output)) != 0) ) {
		return 4;
	}
	Invalid = Key;
	Invalid.Exponent = EvenExponent;
	Invalid.ExponentSize = sizeof(EvenExponent);
	if ( xrtRsaPublic(&Invalid, Expected, sizeof(Expected), Output) ) {
		return 5;
	}
	Invalid = Key;
	Invalid.ModulusSize = 127u;
	if ( xrtRsaPublic(&Invalid, Expected, 127u, Output) ) {
		return 6;
	}
	if ( xrtRsaPublic(NULL, Expected, sizeof(Expected), Output) ||
		 (xrtGetError() == NULL) ) {
		return 7;
	}

	printf("[PASS] crypto_rsa\n");
	return 0;
}
