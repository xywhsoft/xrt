#include "../test.h"
#include "test_crypto_digest.h"



/* 验证空密钥、RFC 4231 和超过块长密钥的 HMAC-SHA256 向量。 */
static void testHmacSha256Vectors(void)
{
	uint8 arrKey[131];
	uint8 arrMac[XRT_SHA256_SIZE];

	testRequire(xrtHmacSha256(NULL, 0, NULL, 0, arrMac),
		"HMAC-SHA256 empty vector failed");
	testCryptoDigest(arrMac, sizeof(arrMac),
		"b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad",
		"HMAC-SHA256 empty vector mismatch");
	testRequire(xrtHmacSha256(
			"Jefe", 4, "what do ya want for nothing?", 28, arrMac
		), "HMAC-SHA256 RFC 4231 case 2 failed");
	testCryptoDigest(arrMac, sizeof(arrMac),
		"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
		"HMAC-SHA256 RFC 4231 case 2 mismatch");
	memset(arrKey, 0xAA, sizeof(arrKey));
	testRequire(xrtHmacSha256(
			arrKey, sizeof(arrKey),
			"Test Using Larger Than Block-Size Key - Hash Key First", 54,
			arrMac
		), "HMAC-SHA256 long key vector failed");
	testCryptoDigest(arrMac, sizeof(arrMac),
		"60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
		"HMAC-SHA256 long key vector mismatch");
}



/* 流式 Final 必须可重复，并允许随后继续追加数据。 */
static void testHmacSha256Streaming(void)
{
	xhmacsha256 State;
	uint8 arrFirst[XRT_SHA256_SIZE];
	uint8 arrSecond[XRT_SHA256_SIZE];
	uint8 arrExpected[XRT_SHA256_SIZE];

	testRequire(xrtHmacSha256Init(&State, "secret", 6) &&
		xrtHmacSha256Update(&State, "hello ", 6) &&
		xrtHmacSha256Update(&State, "world", 5) &&
		xrtHmacSha256Final(&State, arrFirst) &&
		xrtHmacSha256Final(&State, arrSecond),
		"HMAC-SHA256 streaming final failed");
	testRequire(xrtHmacSha256(
			"secret", 6, "hello world", 11, arrExpected
		) && xrtConstTimeEqual(arrFirst, arrExpected, sizeof(arrFirst)) &&
		xrtConstTimeEqual(arrSecond, arrExpected, sizeof(arrSecond)),
		"HMAC-SHA256 streaming result mismatch");
	testRequire(xrtHmacSha256Update(&State, "!", 1) &&
		xrtHmacSha256Final(&State, arrFirst) &&
		xrtHmacSha256("secret", 6, "hello world!", 12, arrExpected) &&
		xrtConstTimeEqual(arrFirst, arrExpected, sizeof(arrFirst)),
		"HMAC-SHA256 continuation after final mismatch");
}



/* 参数和损坏状态必须在修改输出或状态前被拒绝。 */
static void testHmacSha256Invalid(void)
{
	xhmacsha256 State;
	xhmacsha256 Before;
	uint8 arrMac[XRT_SHA256_SIZE];

	xrtClearError();
	testRequire(!xrtHmacSha256Init(NULL, NULL, 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA256 null state contract failed");
	memset(&State, 0xA5, sizeof(State));
	Before = State;
	xrtClearError();
	testRequire(!xrtHmacSha256Init(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA256 invalid key modified state");
	xrtClearError();
	testRequire(!xrtHmacSha256Update(&State, NULL, 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HMAC-SHA256 accepted uninitialized state");
	testRequire(xrtHmacSha256Init(&State, "key", 3),
		"HMAC-SHA256 valid init failed");
	Before = State;
	xrtClearError();
	testRequire(!xrtHmacSha256Update(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA256 invalid update modified state");
	xrtClearError();
	testRequire(!xrtHmacSha256Final(&State, NULL) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA256 null output modified state");
	xrtClearError();
	testRequire(!xrtHmacSha256("key", 3, "data", 4, NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA256 one-shot null output contract failed");
	memset(arrMac, 0, sizeof(arrMac));
}



/* 执行 HMAC-SHA256 向量、流式和失败契约测试。 */
int main(void)
{
	testHmacSha256Vectors();
	testHmacSha256Streaming();
	testHmacSha256Invalid();
	return 0;
}
