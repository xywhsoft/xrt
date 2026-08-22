#include "../test.h"
#include "test_crypto_digest.h"



/* 读取纯 Ed25519 空消息验证向量。 */
static void testEd25519VerifyFixture(
	uint8 Public[XRT_ED25519_PUBLIC_SIZE],
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE]
)
{
	testCryptoDecode(
		Public, XRT_ED25519_PUBLIC_SIZE,
		"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
		"Ed25519 verification public vector has the wrong size"
	);
	testCryptoDecode(
		Signature, XRT_ED25519_SIGNATURE_SIZE,
		"e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
		"5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
		"Ed25519 verification signature vector has the wrong size"
	);
}



/* 验证纯 Ed25519、Ed25519ctx 与 Ed25519ph 向量。 */
static void testEd25519VerifyVectors(void)
{
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Message[16];
	uint8 Prehash[XRT_ED25519_PREHASH_SIZE];
	static const uint8 Context[] = { 'f', 'o', 'o' };

	testEd25519VerifyFixture(Public, Signature);
	testRequire(xrtEd25519Verify(Public, NULL, 0, Signature),
		"Ed25519 empty-message verification failed");

	testCryptoDecode(
		Public, sizeof(Public),
		"dfc9425e4f968f7f0c29f0259cf5f9aed6851c2bb4ad8bfb860cfee0ab248292",
		"Ed25519ctx public vector has the wrong size"
	);
	testCryptoDecode(
		Message, sizeof(Message), "f726936d19c800494e3fdaff20b276a8",
		"Ed25519ctx verification message has the wrong size"
	);
	testCryptoDecode(
		Signature, sizeof(Signature),
		"55a4cc2f70a54e04288c5f4cd1e45a7bb520b36292911876cada7323198dd87a"
		"8b36950b95130022907a7fb7c4e9b2d5f6cca685a587b4b21f4b888e4e7edb0d",
		"Ed25519ctx verification signature has the wrong size"
	);
	testRequire(xrtEd25519VerifyMode(
			Public, XED25519_CONTEXT, Context, sizeof(Context),
			Message, sizeof(Message), Signature
		), "Ed25519ctx verification failed");
	testRequire(!xrtEd25519VerifyMode(
			Public, XED25519_CONTEXT, "bar", 3,
			Message, sizeof(Message), Signature
		), "Ed25519ctx accepted the wrong context");

	testCryptoDecode(
		Public, sizeof(Public),
		"ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf",
		"Ed25519ph public vector has the wrong size"
	);
	testCryptoDecode(
		Signature, sizeof(Signature),
		"98a70222f0b8121aa9d30f813d683f809e462b469c7ff87639499bb94e6dae41"
		"31f85042463c2a355a2003d062adf5aaa10b8c61e636062aaad11c2a26083406",
		"Ed25519ph verification signature has the wrong size"
	);
	testRequire(xrtSha512("abc", 3, Prehash) &&
		xrtEd25519VerifyMode(
			Public, XED25519_PREHASH, NULL, 0,
			Prehash, sizeof(Prehash), Signature
		), "Ed25519ph verification failed");
}



/* 验证规范标量、点编码和主子群公钥边界。 */
static void testEd25519VerifyStrict(void)
{
	static const uint8 Order[32] = {
		0xED, 0xD3, 0xF5, 0x5C, 0x1A, 0x63, 0x12, 0x58,
		0xD6, 0x9C, 0xF7, 0xA2, 0xDE, 0xF9, 0xDE, 0x14,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
	};
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Corrupt[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Identity[XRT_ED25519_PUBLIC_SIZE] = { 1 };
	uint8 NonCanonical[XRT_ED25519_PUBLIC_SIZE];

	testEd25519VerifyFixture(Public, Signature);
	memcpy(Corrupt, Signature, sizeof(Corrupt));
	Corrupt[0] ^= 1u;
	testRequire(!xrtEd25519Verify(Public, NULL, 0, Corrupt),
		"Ed25519 accepted a modified R value");
	memcpy(Corrupt, Signature, sizeof(Corrupt));
	memcpy(Corrupt + 32u, Order, sizeof(Order));
	testRequire(!xrtEd25519Verify(Public, NULL, 0, Corrupt),
		"Ed25519 accepted S equal to L");
	testRequire(!xrtEd25519Verify(Identity, NULL, 0, Signature) &&
		xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY,
		"Ed25519 accepted the identity public key");
	memset(NonCanonical, 0xFF, sizeof(NonCanonical));
	NonCanonical[0] = 0xED;
	NonCanonical[31] = 0x7F;
	testRequire(!xrtEd25519Verify(NonCanonical, NULL, 0, Signature),
		"Ed25519 accepted a non-canonical public key");
	memcpy(Corrupt, Signature, sizeof(Corrupt));
	memcpy(Corrupt, NonCanonical, sizeof(NonCanonical));
	testRequire(!xrtEd25519Verify(Public, NULL, 0, Corrupt),
		"Ed25519 accepted a non-canonical R encoding");
	Identity[31] = 0x80;
	testRequire(!xrtEd25519Verify(Identity, NULL, 0, Signature),
		"Ed25519 accepted x=0 with a set sign bit");
}



/* 验证模式参数边界。 */
static void testEd25519VerifyEdges(void)
{
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	uint8 Context[256] = { 0 };
	uint8 Prehash[XRT_ED25519_PREHASH_SIZE] = { 0 };

	testEd25519VerifyFixture(Public, Signature);
	testRequire(!xrtEd25519VerifyMode(
			Public, XED25519_CONTEXT, Context, sizeof(Context),
			NULL, 0, Signature
		), "Ed25519 verification accepted an oversized context");
	testRequire(!xrtEd25519VerifyMode(
			Public, XED25519_PREHASH, NULL, 0,
			Prehash, sizeof(Prehash) - 1u, Signature
		), "Ed25519 verification accepted a short prehash");
	testRequire(!xrtEd25519Verify(NULL, NULL, 0, Signature),
		"Ed25519 verification accepted a null public key");
	testRequire(!xrtEd25519Verify(Public, NULL, 0, NULL),
		"Ed25519 verification accepted a null signature");
}



/* 执行 Ed25519 严格验证与模式测试。 */
int main(void)
{
	testEd25519VerifyVectors();
	testEd25519VerifyStrict();
	testEd25519VerifyEdges();
	printf("[PASS] crypto_ed25519_verify\n");
	return 0;
}
