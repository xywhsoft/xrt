#include "../test.h"
#include "test_crypto_ecdsa_sign.h"



int main(void)
{
	testCryptoEcdsaSign(
		32,
		XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA384,
		"ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		"0000000000000000000000000000000000000000000000000000000000000001",
		"7ab1611d194dce64adde9e1c5148764c8606de60701a7b0e62737c8bb6e20fc2"
		"474642168ca2c2af9fec9838ff0c9ba20da046f3a2a733aec75e6315ffb858ab",
		"ecdsa-p256-sign",
		xrtEcdsaP256Sign
	);
	printf("[PASS] crypto_ecdsa_p256_sign\n");
	return 0;
}
