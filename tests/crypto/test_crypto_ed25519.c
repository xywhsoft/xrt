#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 RFC 8032 种子、公钥派生和展开密钥生命周期。 */
static void testEd25519Public(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 InPlace[XRT_ED25519_PUBLIC_SIZE];
	xed25519key Key;
	uint8 Zero[sizeof(Key)];

	testCryptoDecode(
		Seed, sizeof(Seed),
		"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
		"Ed25519 seed vector has the wrong size"
	);
	testRequire(xrtEd25519Public(Seed, Public),
		"Ed25519 public derivation failed");
	testCryptoDigest(
		Public, sizeof(Public),
		"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
		"Ed25519 public vector mismatch"
	);
	memcpy(InPlace, Seed, sizeof(InPlace));
	testRequire(xrtEd25519Public(InPlace, InPlace) &&
		xrtConstTimeEqual(InPlace, Public, sizeof(Public)),
		"Ed25519 in-place public derivation failed");
	testRequire(xrtEd25519KeyInit(&Key, Seed) &&
		xrtConstTimeEqual(Key.Public, Public, sizeof(Public)),
		"Ed25519 expanded key mismatch");
	memset(Zero, 0, sizeof(Zero));
	xrtEd25519KeyClear(&Key);
	testRequire(xrtConstTimeEqual(&Key, Zero, sizeof(Key)),
		"Ed25519 expanded key clear failed");
}



/* 验证无效参数不会发布部分公钥或密钥状态。 */
static void testEd25519PublicEdges(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Output[XRT_ED25519_PUBLIC_SIZE];
	uint8 Before[XRT_ED25519_PUBLIC_SIZE];
	xed25519key Key;
	xed25519key KeyBefore;

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Before));
	testRequire(!xrtEd25519Public(NULL, Output) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)),
		"Ed25519 null seed changed public output");
	memset(&Key, 0x5A, sizeof(Key));
	KeyBefore = Key;
	testRequire(!xrtEd25519KeyInit(&Key, NULL) &&
		xrtConstTimeEqual(&Key, &KeyBefore, sizeof(Key)),
		"Ed25519 key init failure changed target state");
	testRequire(!xrtEd25519Public(Seed, NULL),
		"Ed25519 public derivation accepted null output");
	xrtEd25519KeyClear(NULL);
}



/* 执行 Ed25519 公钥与展开密钥测试。 */
int main(void)
{
	testEd25519Public();
	testEd25519PublicEdges();
	printf("[PASS] crypto_ed25519\n");
	return 0;
}
