#include "../test.h"
#include "test_crypto_ecdsa_der_verify.h"



/* 验证 DER 便利入口不会把 P-256 错误限制为 SHA-256。 */
static void testSha384(void)
{
	uint8 Hash[48];
	uint8 Public[65];
	uint8 Der[70];

	for ( size_t i = 0; i < sizeof(Hash); i++ ) {
		Hash[i] = (uint8)i;
	}
	testCryptoDecode(
		Public, sizeof(Public),
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"P-256 SHA-384 DER public key size mismatch"
	);
	testCryptoDecode(
		Der, sizeof(Der),
		"3044022076a660acedf0ea45521f8bc9a0bb77765fc38be8968ee6d19282d0b966"
		"37865702202cab907e54ebf8740345823061f69c52e34ceb290335a59319356a08"
		"80c2a1d1",
		"P-256 SHA-384 DER signature size mismatch"
	);
	testRequire(xrtEcdsaP256VerifyDer(
		Hash, sizeof(Hash), Der, sizeof(Der), Public
	), "P-256 SHA-384 DER signature was rejected");
}



int main(void)
{
	testCryptoEcdsaVerifyDer(
		32,
		65,
		72,
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"30460221008a774923f5243a9f3c1c0d22022ace804587447d084c16566bba66ba"
		"49f8bdba022100e387111aba3791e7f91d8c6bc97ea4b9eddeb8adace5056394f"
		"e73869b242bfb",
		"ecdsa-p256-verify",
		xrtEcdsaP256VerifyDer
	);
	testSha384();
	printf("[PASS] crypto_ecdsa_p256_der\n");
	return 0;
}
