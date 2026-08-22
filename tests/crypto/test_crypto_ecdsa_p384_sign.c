#include "../test.h"
#include "test_crypto_ecdsa_sign.h"



int main(void)
{
	testCryptoEcdsaSign(
		48,
		XCRYPTO_HASH_SHA384,
		XCRYPTO_HASH_SHA256,
		"ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf"
		"581a0db248b0a77aecec196accc52973",
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f",
		"0000000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000001",
		"93a4739b2254c160d696f213a3caf263520b9806cceef5338c7b0f36f24327c6"
		"6b9525c46059b62453d440ec15c8a0422e8fe98b0ba0aa5f5f7b753030d4ee22"
		"1f6989549c5a03a1578e90f58e38ac1db5d55d8656be64c735c6cdd6ba8f55ba",
		"ecdsa-p384-sign",
		xrtEcdsaP384Sign
	);
	printf("[PASS] crypto_ecdsa_p384_sign\n");
	return 0;
}
