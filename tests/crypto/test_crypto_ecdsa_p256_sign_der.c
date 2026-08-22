#include "../test.h"
#include "test_crypto_ecdsa_sign_der.h"



int main(void)
{
	testCryptoEcdsaSignDer(
		32,
		70,
		XCRYPTO_HASH_SHA256,
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		"0000000000000000000000000000000000000000000000000000000000000001",
		"7ab1611d194dce64adde9e1c5148764c8606de60701a7b0e62737c8bb6e20fc2"
		"474642168ca2c2af9fec9838ff0c9ba20da046f3a2a733aec75e6315ffb858ab",
		"304402207ab1611d194dce64adde9e1c5148764c8606de60701a7b0e62737c8b"
		"b6e20fc20220474642168ca2c2af9fec9838ff0c9ba20da046f3a2a733aec75e"
		"6315ffb858ab",
		xrtEcdsaP256SignDer
	);
	printf("[PASS] crypto_ecdsa_p256_sign_der\n");
	return 0;
}
