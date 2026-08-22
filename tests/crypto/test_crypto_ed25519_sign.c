#include "../test.h"
#include "test_crypto_digest.h"



/* 验证纯 Ed25519 的 RFC 8032 空消息向量。 */
static void testEd25519SignPure(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Overlap[XRT_ED25519_SIGNATURE_SIZE];
	xed25519key Key;

	testCryptoDecode(
		Seed, sizeof(Seed),
		"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
		"Ed25519 signing seed vector has the wrong size"
	);
	testRequire(xrtEd25519Sign(Seed, NULL, 0, Signature),
		"Ed25519 empty-message signing failed");
	testCryptoDigest(
		Signature, sizeof(Signature),
		"e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
		"5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
		"Ed25519 empty-message signature mismatch"
	);
	memset(Overlap, 0, sizeof(Overlap));
	memcpy(Overlap, Seed, sizeof(Seed));
	testRequire(xrtEd25519Sign(Overlap, NULL, 0, Overlap) &&
		xrtConstTimeEqual(Overlap, Signature, sizeof(Signature)),
		"Ed25519 seed/signature overlap failed");
	testRequire(xrtEd25519KeyInit(&Key, Seed),
		"Ed25519 signing key initialization failed");
	testRequire(xrtEd25519SignKey(&Key, NULL, 0, Signature),
		"Ed25519 prepared-key signing failed");
	xrtEd25519KeyClear(&Key);
}



/* 验证 Ed25519ctx 与 Ed25519ph 的 RFC 8032 向量。 */
static void testEd25519SignModes(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Message[16];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Prehash[XRT_ED25519_PREHASH_SIZE];
	xed25519key Key;
	static const uint8 Context[] = { 'f', 'o', 'o' };

	testCryptoDecode(
		Seed, sizeof(Seed),
		"0305334e381af78f141cb666f6199f57bc3495335a256a95bd2a55bf546663f6",
		"Ed25519ctx seed vector has the wrong size"
	);
	testCryptoDecode(
		Message, sizeof(Message), "f726936d19c800494e3fdaff20b276a8",
		"Ed25519ctx message vector has the wrong size"
	);
	testRequire(xrtEd25519KeyInit(&Key, Seed) &&
		xrtEd25519SignMode(
			&Key, XED25519_CONTEXT, Context, sizeof(Context),
			Message, sizeof(Message), Signature
		), "Ed25519ctx signing failed");
	testCryptoDigest(
		Signature, sizeof(Signature),
		"55a4cc2f70a54e04288c5f4cd1e45a7bb520b36292911876cada7323198dd87a"
		"8b36950b95130022907a7fb7c4e9b2d5f6cca685a587b4b21f4b888e4e7edb0d",
		"Ed25519ctx signature mismatch"
	);
	xrtEd25519KeyClear(&Key);

	testCryptoDecode(
		Seed, sizeof(Seed),
		"833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
		"Ed25519ph seed vector has the wrong size"
	);
	testRequire(xrtSha512("abc", 3, Prehash),
		"Ed25519ph prehash failed");
	testRequire(xrtEd25519KeyInit(&Key, Seed) &&
		xrtEd25519SignMode(
			&Key, XED25519_PREHASH, NULL, 0,
			Prehash, sizeof(Prehash), Signature
		), "Ed25519ph signing failed");
	testCryptoDigest(
		Signature, sizeof(Signature),
		"98a70222f0b8121aa9d30f813d683f809e462b469c7ff87639499bb94e6dae41"
		"31f85042463c2a355a2003d062adf5aaa10b8c61e636062aaad11c2a26083406",
		"Ed25519ph signature mismatch"
	);
	xrtEd25519KeyClear(&Key);
}



/* 验证模式参数错误保持签名输出不变。 */
static void testEd25519SignEdges(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Context[256] = { 0 };
	uint8 Message[XRT_ED25519_PREHASH_SIZE] = { 0 };
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Before[XRT_ED25519_SIGNATURE_SIZE];
	xed25519key Key;

	testRequire(xrtEd25519KeyInit(&Key, Seed),
		"Ed25519 edge key initialization failed");
	memset(Signature, 0xA5, sizeof(Signature));
	memcpy(Before, Signature, sizeof(Before));
	testRequire(!xrtEd25519SignMode(
			&Key, XED25519_CONTEXT, Context, sizeof(Context),
			Message, sizeof(Message), Signature
		) && xrtConstTimeEqual(Signature, Before, sizeof(Signature)),
		"Ed25519 accepted an oversized context or changed output");
	testRequire(!xrtEd25519SignMode(
			&Key, XED25519_PREHASH, NULL, 0,
			Message, sizeof(Message) - 1u, Signature
		) && xrtConstTimeEqual(Signature, Before, sizeof(Signature)),
		"Ed25519 accepted a short prehash or changed output");
	testRequire(!xrtEd25519SignMode(
			&Key, (xed25519mode)99, NULL, 0,
			Message, sizeof(Message), Signature
		) && xrtConstTimeEqual(Signature, Before, sizeof(Signature)),
		"Ed25519 accepted an invalid mode or changed output");
	Key.Guard = 0;
	testRequire(!xrtEd25519SignKey(&Key, Message, sizeof(Message), Signature) &&
		xrtConstTimeEqual(Signature, Before, sizeof(Signature)),
		"Ed25519 accepted an invalid prepared key or changed output");
	xrtEd25519KeyClear(&Key);
}



/* 执行 Ed25519 签名向量和失败原子性测试。 */
int main(void)
{
	testEd25519SignPure();
	testEd25519SignModes();
	testEd25519SignEdges();
	printf("[PASS] crypto_ed25519_sign\n");
	return 0;
}
