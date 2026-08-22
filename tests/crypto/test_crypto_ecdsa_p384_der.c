#include "../test.h"
#include "test_crypto_ecdsa_der_verify.h"



/* 验证 DER 便利入口不会把 P-384 错误限制为 SHA-384。 */
static void testSha256(void)
{
	uint8 Hash[32];
	uint8 Public[97];
	uint8 Der[103];

	for ( size_t i = 0; i < sizeof(Hash); i++ ) {
		Hash[i] = (uint8)i;
	}
	testCryptoDecode(
		Public, sizeof(Public),
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a3"
		"85502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc2"
		"9f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"P-384 SHA-256 DER public key size mismatch"
	);
	testCryptoDecode(
		Der, sizeof(Der),
		"306502303cfa05a609bb11606cf592c1764498ab363c8b0b79ff36f8a5b21d0b"
		"6d6599402521652f8fbcb34703f574f18d90010e023100fdc672eefa2ea97277a8"
		"62415f1edab605d12f0bb53252c38fc0fef7c0d7f8fc47aea0f72ed823e15a8e"
		"ed2b12451755",
		"P-384 SHA-256 DER signature size mismatch"
	);
	testRequire(xrtEcdsaP384VerifyDer(
		Hash, sizeof(Hash), Der, sizeof(Der), Public
	), "P-384 SHA-256 DER signature was rejected");
}



int main(void)
{
	testCryptoEcdsaVerifyDer(
		48,
		97,
		102,
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f",
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a3"
		"85502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc2"
		"9f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"306402305be6e34e41e8d9d8241d92df587ecf10288676aaacf8cf2291924663"
		"e5ec0b1b0051aa8c8039b024b5eab30fed37107502301b7f67536ab38d9b9c2d"
		"c32d51a4dab143cf333a2a477668d1c8e57890e7e6bb368c0f9ebb0edcd6ee92"
		"3d21b09254b7",
		"ecdsa-p384-verify",
		xrtEcdsaP384VerifyDer
	);
	testSha256();
	printf("[PASS] crypto_ecdsa_p384_der\n");
	return 0;
}
