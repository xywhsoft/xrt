#include "../test.h"
#include "test_crypto_digest.h"



static const uint8 TestAeadKey[XRT_CHACHA20_POLY1305_KEY_SIZE] = {
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
};

static const uint8 TestAeadNonce[XRT_CHACHA20_POLY1305_NONCE_SIZE] = {
	0x07, 0x00, 0x00, 0x00, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47
};

static const uint8 TestAeadAad[] = {
	0x50, 0x51, 0x52, 0x53, 0xC0, 0xC1,
	0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7
};

static const char TestAeadPlain[] =
	"Ladies and Gentlemen of the class of '99: If I could offer you only "
	"one tip for the future, sunscreen would be it.";



/* 核对 RFC 8439 2.8.2 的完整 detached AEAD 向量。 */
static void testAeadVector(void)
{
	uint8 Cipher[sizeof(TestAeadPlain) - 1u];
	uint8 Tag[XRT_CHACHA20_POLY1305_TAG_SIZE];
	uint8 Plain[sizeof(TestAeadPlain) - 1u];

	testRequire(xrtChaCha20Poly1305Encrypt(
			TestAeadKey,
			TestAeadNonce,
			TestAeadAad,
			sizeof(TestAeadAad),
			TestAeadPlain,
			sizeof(Cipher),
			Cipher,
			Tag
		), "ChaCha20-Poly1305 RFC encryption failed");
	testCryptoDigest(Cipher, sizeof(Cipher),
		"d31a8d34648e60db7b86afbc53ef7ec2"
		"a4aded51296e08fea9e2b5a736ee62d6"
		"3dbea45e8ca9671282fafb69da92728b"
		"1a71de0a9e060b2905d6a5b67ecd3b36"
		"92ddbd7f2d778b8c9803aee328091b58"
		"fab324e4fad675945585808b4831d7bc"
		"3ff4def08e4b7a9de576d26586cec64b"
		"6116",
		"ChaCha20-Poly1305 RFC ciphertext mismatch");
	testCryptoDigest(Tag, sizeof(Tag),
		"1ae10b594f09e26a7e902ecbd0600691",
		"ChaCha20-Poly1305 RFC tag mismatch");
	testRequire(xrtChaCha20Poly1305Decrypt(
			TestAeadKey,
			TestAeadNonce,
			TestAeadAad,
			sizeof(TestAeadAad),
			Cipher,
			sizeof(Cipher),
			Tag,
			Plain
		) && xrtConstTimeEqual(Plain, TestAeadPlain, sizeof(Plain)),
		"ChaCha20-Poly1305 RFC decryption failed");
}



/* 验证 packed 便捷层、空消息和原位 Seal/Open。 */
static void testAeadPacked(void)
{
	uint8 Packed[sizeof(TestAeadPlain) - 1u + XRT_CHACHA20_POLY1305_OVERHEAD];
	uint8 Plain[sizeof(TestAeadPlain) - 1u];
	uint8 Empty[XRT_CHACHA20_POLY1305_OVERHEAD];

	testRequire(xrtChaCha20Poly1305Seal(
			TestAeadKey,
			TestAeadNonce,
			TestAeadAad,
			sizeof(TestAeadAad),
			TestAeadPlain,
			sizeof(Plain),
			Packed,
			sizeof(Packed)
		), "ChaCha20-Poly1305 Seal failed");
	testRequire(xrtChaCha20Poly1305Open(
			TestAeadKey,
			TestAeadNonce,
			TestAeadAad,
			sizeof(TestAeadAad),
			Packed,
			sizeof(Packed),
			Plain,
			sizeof(Plain)
		) && xrtConstTimeEqual(Plain, TestAeadPlain, sizeof(Plain)),
		"ChaCha20-Poly1305 Open failed");
	memcpy(Packed, TestAeadPlain, sizeof(Plain));
	testRequire(xrtChaCha20Poly1305Seal(
			TestAeadKey, TestAeadNonce, TestAeadAad, sizeof(TestAeadAad),
			Packed, sizeof(Plain), Packed, sizeof(Packed)
		) && xrtChaCha20Poly1305Open(
			TestAeadKey, TestAeadNonce, TestAeadAad, sizeof(TestAeadAad),
			Packed, sizeof(Packed), Packed, sizeof(Packed)
		) && xrtConstTimeEqual(Packed, TestAeadPlain, sizeof(Plain)),
		"ChaCha20-Poly1305 in-place packed round trip failed");
	testRequire(xrtChaCha20Poly1305Seal(
			TestAeadKey, TestAeadNonce, NULL, 0,
			NULL, 0, Empty, sizeof(Empty)
		) && xrtChaCha20Poly1305Open(
			TestAeadKey, TestAeadNonce, NULL, 0,
			Empty, sizeof(Empty), NULL, 0
		), "ChaCha20-Poly1305 empty packed message failed");
}



/* 篡改密文、标签或 AAD 时必须返回结构化错误且不写明文。 */
static void testAeadAuthentication(void)
{
	uint8 Cipher[sizeof(TestAeadPlain) - 1u];
	uint8 Tag[XRT_CHACHA20_POLY1305_TAG_SIZE];
	uint8 Plain[sizeof(TestAeadPlain) - 1u];
	uint8 Before[sizeof(Plain)];
	uint8 Aad[sizeof(TestAeadAad)];

	testRequire(xrtChaCha20Poly1305Encrypt(
			TestAeadKey, TestAeadNonce, TestAeadAad, sizeof(TestAeadAad),
			TestAeadPlain, sizeof(Cipher), Cipher, Tag
		), "ChaCha20-Poly1305 tamper setup failed");
	memset(Plain, 0xA5, sizeof(Plain));
	memcpy(Before, Plain, sizeof(Before));
	Cipher[0] ^= 1u;
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Decrypt(
			TestAeadKey, TestAeadNonce, TestAeadAad, sizeof(TestAeadAad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_AUTHENTICATION) &&
		xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"ChaCha20-Poly1305 ciphertext tamper changed output");
	Cipher[0] ^= 1u;
	Tag[15] ^= 1u;
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Decrypt(
			TestAeadKey, TestAeadNonce, TestAeadAad, sizeof(TestAeadAad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"ChaCha20-Poly1305 tag tamper changed output");
	Tag[15] ^= 1u;
	memcpy(Aad, TestAeadAad, sizeof(Aad));
	Aad[0] ^= 1u;
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Decrypt(
			TestAeadKey, TestAeadNonce, Aad, sizeof(Aad),
			Cipher, sizeof(Cipher), Tag, Plain
		) && xrtConstTimeEqual(Plain, Before, sizeof(Plain)),
		"ChaCha20-Poly1305 AAD tamper changed output");
}



/* 检查容量、短输入、部分重叠和 RFC 消息长度上限。 */
static void testAeadEdges(void)
{
	uint8 Buffer[64];
	uint8 Tag[XRT_CHACHA20_POLY1305_TAG_SIZE];
	uint8 One = 0;

	memset(Buffer, 0x5A, sizeof(Buffer));
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Seal(
			TestAeadKey, TestAeadNonce, NULL, 0,
			"x", 1, Buffer, XRT_CHACHA20_POLY1305_OVERHEAD
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"ChaCha20-Poly1305 accepted a short seal buffer");
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Open(
			TestAeadKey, TestAeadNonce, NULL, 0,
			Buffer, 15, NULL, 0
		) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"ChaCha20-Poly1305 accepted input without a full tag");
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Encrypt(
			TestAeadKey, TestAeadNonce, NULL, 0,
			Buffer, 16, Buffer + 1, Tag
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20-Poly1305 accepted partial plaintext overlap");
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Encrypt(
			TestAeadKey, TestAeadNonce, Buffer, 8,
			Buffer + 16, 8, Buffer, Tag
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20-Poly1305 accepted output over AAD");
	xrtClearError();
	testRequire(!xrtChaCha20Poly1305Encrypt(
			TestAeadKey, TestAeadNonce, NULL, 0,
			Buffer, 16, Buffer, Buffer + 8
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20-Poly1305 accepted tag over ciphertext");
	#if SIZE_MAX > UINT32_MAX
		xrtClearError();
		testRequire(!xrtChaCha20Poly1305Encrypt(
				TestAeadKey, TestAeadNonce, NULL, 0,
				&One,
				(size_t)(XRT_CHACHA20_POLY1305_MAX_SIZE + 1u),
				&One,
				Tag
			) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"ChaCha20-Poly1305 accepted a message past P_MAX");
	#else
		(void)One;
	#endif
}



/* 执行 AEAD 向量、便捷层、认证原子性与边界测试。 */
int main(void)
{
	testAeadVector();
	testAeadPacked();
	testAeadAuthentication();
	testAeadEdges();
	return 0;
}
