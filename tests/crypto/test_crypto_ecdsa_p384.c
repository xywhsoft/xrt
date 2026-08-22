#include "../test.h"
#include "test_crypto_ecdsa_verify.h"



/* 验证 P-384 对短摘要和长摘要执行标准 bits2int 转换。 */
static void testVariableHashes(void)
{
	static cstr sPublic =
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a3"
		"85502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc2"
		"9f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f";
	static const cstr Signatures[] = {
		"bbae572b64409c9ad9e52f18decfee2107e7a8737613a6e469e4c2a04922eb85"
		"102e1eedd6b1bbcfba5b68cb7951ac83417ab449e404068dd4f65def57191fccfb"
		"dbfb72ad5cb8584430eaa2d4f9d57f4cf26f69515816c80a468b2690d9a069",
		"3cfa05a609bb11606cf592c1764498ab363c8b0b79ff36f8a5b21d0b6d659940"
		"2521652f8fbcb34703f574f18d90010efdc672eefa2ea97277a862415f1edab60"
		"5d12f0bb53252c38fc0fef7c0d7f8fc47aea0f72ed823e15a8eed2b12451755",
		"e16bc22bfb268e070caf4ff9c8c163e4acfe0c574988cd373181444c85f4b59c"
		"0f10db2dcff61239b782f105c86015b0f0f7296504a4e296e125b9b139e05182"
		"79eed980753160d829a2f1b426e6cf9cb2ed98104e6626fd0f7eb16a887178cb"
	};
	static const size_t HashSizes[] = { 28u, 32u, 64u };
	uint8 Hash[64];
	uint8 Public[97];
	uint8 Signature[96];

	for ( size_t i = 0; i < sizeof(Hash); i++ ) {
		Hash[i] = (uint8)i;
	}
	testCryptoDecode(Public, sizeof(Public), sPublic,
		"P-384 variable-hash public key size mismatch");
	for ( size_t i = 0; i < sizeof(HashSizes) / sizeof(HashSizes[0]); i++ ) {
		testCryptoDecode(
			Signature, sizeof(Signature), Signatures[i],
			"P-384 variable-hash signature size mismatch"
		);
		testRequire(xrtEcdsaP384Verify(
			Hash, HashSizes[i], Signature, Public
		), "P-384 variable-hash signature was rejected");
	}

	/* 超出 P-384 群阶位数的摘要尾部不参与 ECDSA 整数转换。 */
	Hash[63] ^= UINT8_C(1);
	testCryptoDecode(
		Signature, sizeof(Signature), Signatures[2],
		"P-384 SHA-512 signature restore failed"
	);
	testRequire(xrtEcdsaP384Verify(Hash, 64u, Signature, Public),
		"P-384 long-hash tail affected verification");
	Hash[63] ^= UINT8_C(1);
	Hash[0] ^= UINT8_C(1);
	testRequire(!xrtEcdsaP384Verify(Hash, 64u, Signature, Public),
		"P-384 long-hash prefix mismatch was accepted");
}



int main(void)
{
	testCryptoEcdsaVerify(
		48,
		97,
		"ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf"
		"581a0db248b0a77aecec196accc52973",
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f",
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a3"
		"85502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc2"
		"9f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"5be6e34e41e8d9d8241d92df587ecf10288676aaacf8cf2291924663e5ec0b1b"
		"0051aa8c8039b024b5eab30fed3710751b7f67536ab38d9b9c2dc32d51a4dab1"
		"43cf333a2a477668d1c8e57890e7e6bb368c0f9ebb0edcd6ee923d21b09254b7",
		"3a62d85a12efd324ecbe58f4927a3794644de33aeebbaf70b5a5f9b21bc4f159"
		"4f0904cb7af0c887aed8c66809f9bd49a29c3ff142bd965d04b187b6ffa9a207"
		"86ad38769c46113d18bc7d2097bde5f964f7acd83f06495bd1bc904a5ac3e745",
		"ecdsa-p384-verify",
		xrtEcdsaP384Verify
	);
	testVariableHashes();
	printf("[PASS] crypto_ecdsa_p384\n");
	return 0;
}
