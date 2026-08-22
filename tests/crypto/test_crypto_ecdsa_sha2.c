#include "../test.h"
#include "test_crypto_digest.h"



typedef bool (*test_ecdsa_public_fn)(const void* pPrivate, void* pPublic);

typedef bool (*test_ecdsa_sha2_sign_fn)(
	xcryptohash Hash,
	const void* pDigest,
	const void* pPrivate,
	void* pSignature
);

typedef bool (*test_ecdsa_sha2_verify_fn)(
	const void* pDigest,
	size_t iDigestSize,
	const void* pSignature,
	const void* pPublic
);

typedef bool (*test_ecdsa_hash_fn)(
	const void* pData,
	size_t iSize,
	void* pDigest
);



/* 用 RFC 6979 向量验证曲线宽度与摘要宽度不同的确定性签名。 */
static void testEcdsaSha2Vector(
	size_t iScalarSize,
	size_t iPublicSize,
	xcryptohash Hash,
	cstr sPrivate,
	cstr sExpected,
	test_ecdsa_hash_fn pHash,
	test_ecdsa_public_fn pPublic,
	test_ecdsa_sha2_sign_fn pSign,
	test_ecdsa_sha2_verify_fn pVerify
)
{
	static const char Message[] = "sample";
	uint8 Digest[XRT_SHA512_SIZE];
	uint8 Private[XRT_P384_PRIVATE_SIZE];
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Signature[XRT_ECDSA_P384_SIGNATURE_SIZE];
	uint8 Expected[XRT_ECDSA_P384_SIGNATURE_SIZE];
	size_t iHashSize = xrtCryptoHashSize(Hash);

	testCryptoDecode(
		Private, iScalarSize, sPrivate, "RFC 6979 private key mismatch"
	);
	testCryptoDecode(
		Expected, iScalarSize * 2u, sExpected,
		"RFC 6979 expected signature mismatch"
	);
	testRequire(pHash(Message, sizeof(Message) - 1u, Digest) &&
		pPublic(Private, Public) &&
		pSign(Hash, Digest, Private, Signature) &&
		xrtConstTimeEqual(Signature, Expected, iScalarSize * 2u) &&
		pVerify(Digest, iHashSize, Signature, Public),
		"ECDSA cross-width RFC 6979 vector mismatch");
	xrtSecureZero(Signature, sizeof(Signature));
	xrtSecureZero(Public, iPublicSize);
	xrtSecureZero(Private, sizeof(Private));
	xrtSecureZero(Digest, sizeof(Digest));
}



/* 覆盖 TLS 1.2 会使用的 P-256/SHA-384 与 P-256/SHA-512。 */
static void testEcdsaP256Sha2(void)
{
	static const char Private[] =
		"c9afa9d845ba75166b5c215767b1d693"
		"4e50c3db36e89b127b8a622b120f6721";

	testEcdsaSha2Vector(
		XRT_P256_PRIVATE_SIZE, XRT_P256_PUBLIC_SIZE,
		XCRYPTO_HASH_SHA384, Private,
		"0eafea039b20e9b42309fb1d89e213057cbf973dc0cfc8f129edddc800ef7719"
		"4861f0491e6998b9455193e34e7b0d284ddd7149a74b95b9261f13abde940954",
		xrtSha384, xrtP256Public, xrtEcdsaP256Sign, xrtEcdsaP256Verify
	);
	testEcdsaSha2Vector(
		XRT_P256_PRIVATE_SIZE, XRT_P256_PUBLIC_SIZE,
		XCRYPTO_HASH_SHA512, Private,
		"8496a60b5e9b47c825488827e0495b0e3fa109ec4568fd3f8d1097678eb97f00"
		"2362ab1adbe2b8adf9cb9edab740ea6049c028114f2460f96554f61fae3302fe",
		xrtSha512, xrtP256Public, xrtEcdsaP256Sign, xrtEcdsaP256Verify
	);
}



/* 覆盖 RFC 6979 需要两个 HMAC 块的 P-384/SHA-256 及 SHA-512。 */
static void testEcdsaP384Sha2(void)
{
	static const char Private[] =
		"6b9d3dad2e1b8c1c05b19875b6659f4de23c3b667bf297ba9aa47740787137d8"
		"96d5724e4c70a825f872c9ea60d2edf5";

	testEcdsaSha2Vector(
		XRT_P384_PRIVATE_SIZE, XRT_P384_PUBLIC_SIZE,
		XCRYPTO_HASH_SHA256, Private,
		"21b13d1e013c7fa1392d03c5f99af8b30c570c6f98d4ea8e354b63a21d3daa33"
		"bde1e888e63355d92fa2b3c36d8fb2cd"
		"0c55bbc04ef88ba40b428834c76e98b9cdf975ef35981c2b69b127124c652ef3"
		"683da9c57b95e34c2c20930227cb1ec3",
		xrtSha256, xrtP384Public, xrtEcdsaP384Sign, xrtEcdsaP384Verify
	);
	testEcdsaSha2Vector(
		XRT_P384_PRIVATE_SIZE, XRT_P384_PUBLIC_SIZE,
		XCRYPTO_HASH_SHA512, Private,
		"ed0959d5880ab2d869ae7f6c2915c6d60f96507f9cb3e047c0046861da4a799c"
		"fe30f35cc900056d7c99cd7882433709"
		"512c8cceee3890a84058ce1e22dbc2198f42323ce8aca9135329f03c068e5112"
		"dc7cc3ef3446defceb01a45c2667fdd5",
		xrtSha512, xrtP384Public, xrtEcdsaP384Sign, xrtEcdsaP384Verify
	);
}



/* 执行 ECDSA 完整 SHA-2 组合门禁。 */
int main(void)
{
	testEcdsaP256Sha2();
	testEcdsaP384Sha2();
	printf("[PASS] crypto_ecdsa_sha2\n");
	return 0;
}
