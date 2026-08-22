#include "../test.h"
#include "test_crypto_digest.h"



/* 核对 RFC 8439 2.4.2 的完整 114 字节 ChaCha20 向量。 */
static void testChaCha20Vector(void)
{
	uint8 Key[XRT_CHACHA20_KEY_SIZE];
	const uint8 Nonce[XRT_CHACHA20_NONCE_SIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x4A, 0x00, 0x00, 0x00, 0x00
	};
	cstr sPlain =
		"Ladies and Gentlemen of the class of '99: If I could offer you only "
		"one tip for the future, sunscreen would be it.";
	uint8 Output[114];

	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		Key[i] = (uint8)i;
	}
	testRequire(strlen(sPlain) == sizeof(Output),
		"ChaCha20 vector input size mismatch");
	testRequire(xrtChaCha20(
			Key, Nonce, 1, sPlain, Output, sizeof(Output)
		), "ChaCha20 vector encryption failed");
	testCryptoDigest(Output, sizeof(Output),
		"6e2e359a2568f98041ba0728dd0d6981"
		"e97e7aec1d4360c20a27afccfd9fae0b"
		"f91b65c5524733ab8f593dabcd62b357"
		"1639d624e65152ab8f530c359f0861d8"
		"07ca0dbf500d6a6156a38e088a22b65e"
		"52bc514d16ccf806818ce91ab7793736"
		"5af90bbf74a35be6b40b8eedf2785e42"
		"874d",
		"ChaCha20 RFC vector mismatch");
}



/* 验证完整块、尾块和同起点原位异或保持对称。 */
static void testChaCha20InPlace(void)
{
	uint8 Key[XRT_CHACHA20_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_NONCE_SIZE];
	uint8 Plain[129];
	uint8 Cipher[129];
	uint8 InPlace[129];

	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		Key[i] = (uint8)(0x80u + i);
	}
	for ( size_t i = 0; i < sizeof(Nonce); i++ ) {
		Nonce[i] = (uint8)(0x20u + i);
	}
	for ( size_t i = 0; i < sizeof(Plain); i++ ) {
		Plain[i] = (uint8)((i * 37u) + 11u);
	}
	memcpy(InPlace, Plain, sizeof(Plain));
	testRequire(xrtChaCha20(
			Key, Nonce, 7, Plain, Cipher, sizeof(Cipher)
		), "ChaCha20 disjoint encryption failed");
	testRequire(xrtChaCha20(
			Key, Nonce, 7, InPlace, InPlace, sizeof(InPlace)
		) && xrtConstTimeEqual(Cipher, InPlace, sizeof(Cipher)),
		"ChaCha20 in-place encryption mismatch");
	testRequire(xrtChaCha20(
			Key, Nonce, 7, InPlace, InPlace, sizeof(InPlace)
		) && xrtConstTimeEqual(Plain, InPlace, sizeof(Plain)),
		"ChaCha20 in-place round trip failed");
}



/* 压实空输入、计数器末块和所有危险重叠参数。 */
static void testChaCha20Edges(void)
{
	uint8 Key[XRT_CHACHA20_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_CHACHA20_NONCE_SIZE] = { 0 };
	uint8 Input[66] = { 0 };
	uint8 Output[66];
	uint8 Before[66];

	testRequire(xrtChaCha20(Key, Nonce, 0, NULL, NULL, 0),
		"ChaCha20 rejected an empty transform");
	testRequire(xrtChaCha20(
			Key, Nonce, UINT32_MAX, Input, Output, 64
		), "ChaCha20 rejected the final counter block");
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	xrtClearError();
	testRequire(!xrtChaCha20(
			Key, Nonce, UINT32_MAX, Input, Output, 65
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)),
		"ChaCha20 counter overflow changed output");
	xrtClearError();
	testRequire(!xrtChaCha20(
			Key, Nonce, 0, Input, Input + 1, 64
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20 accepted partial overlap");
	xrtClearError();
	testRequire(!xrtChaCha20(
			Key, Nonce, 0, Input, Key, 1
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20 accepted output over the key");
	xrtClearError();
	testRequire(!xrtChaCha20(
			NULL, Nonce, 0, Input, Output, 1
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20 accepted a null key");
	xrtClearError();
	testRequire(!xrtChaCha20(
			Key, Nonce, 0, NULL, Output, 1
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"ChaCha20 accepted a null non-empty input");
}



/* 执行 ChaCha20 标准向量、原位与计数器边界测试。 */
int main(void)
{
	testChaCha20Vector();
	testChaCha20InPlace();
	testChaCha20Edges();
	return 0;
}
