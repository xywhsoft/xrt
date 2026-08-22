#include "../test.h"
#include "test_crypto_ecdsa_verify.h"



/* 验证 P-256 对短摘要和长摘要执行标准 bits2int 转换。 */
static void testVariableHashes(void)
{
	static cstr sPublic =
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5";
	static const cstr Signatures[] = {
		"e156530fc73a52e96e18c33b998dfcdc66445064721641a41f500967050143c5"
		"e6787502ca784b36d21cce43b55d353f40d310d620261c97f523bdc4e10e0a86",
		"76a660acedf0ea45521f8bc9a0bb77765fc38be8968ee6d19282d0b966378657"
		"2cab907e54ebf8740345823061f69c52e34ceb290335a59319356a0880c2a1d1",
		"859e5d20fe3451f9c2a5a0995e74a93fb2d179dc54b1c49c20af1b397b2efe99"
		"66c89a79be802798d40868ada2aa41339a3562e79743fa351695de6d12336f14"
	};
	static const size_t HashSizes[] = { 28u, 48u, 64u };
	uint8 Hash[64];
	uint8 Public[65];
	uint8 Signature[64];

	for ( size_t i = 0; i < sizeof(Hash); i++ ) {
		Hash[i] = (uint8)i;
	}
	testCryptoDecode(Public, sizeof(Public), sPublic,
		"P-256 variable-hash public key size mismatch");
	for ( size_t i = 0; i < sizeof(HashSizes) / sizeof(HashSizes[0]); i++ ) {
		testCryptoDecode(
			Signature, sizeof(Signature), Signatures[i],
			"P-256 variable-hash signature size mismatch"
		);
		testRequire(xrtEcdsaP256Verify(
			Hash, HashSizes[i], Signature, Public
		), "P-256 variable-hash signature was rejected");
	}

	/* 超出 P-256 群阶位数的摘要尾部不参与 ECDSA 整数转换。 */
	Hash[47] ^= UINT8_C(1);
	testCryptoDecode(
		Signature, sizeof(Signature), Signatures[1],
		"P-256 SHA-384 signature restore failed"
	);
	testRequire(xrtEcdsaP256Verify(Hash, 48u, Signature, Public),
		"P-256 long-hash tail affected verification");
	Hash[47] ^= UINT8_C(1);
	Hash[0] ^= UINT8_C(1);
	testRequire(!xrtEcdsaP256Verify(Hash, 48u, Signature, Public),
		"P-256 long-hash prefix mismatch was accepted");
}



int main(void)
{
	testCryptoEcdsaVerify(
		32,
		65,
		"ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"8a774923f5243a9f3c1c0d22022ace804587447d084c16566bba66ba49f8bdba"
		"e387111aba3791e7f91d8c6bc97ea4b9eddeb8adace5056394fe73869b242bfb",
		"b63cf6f787a93638a0a6e8411e643c839461ee1556675dee078bcbe50341fa5f"
		"208a7543d917d533522a1737a9a1d065178c02fda24c5f4286a5427dd0e7a91f",
		"ecdsa-p256-verify",
		xrtEcdsaP256Verify
	);
	testVariableHashes();
	printf("[PASS] crypto_ecdsa_p256\n");
	return 0;
}
