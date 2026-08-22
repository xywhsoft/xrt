#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 HMAC-SHA384/512 的空密钥、RFC 4231 和长密钥向量。 */
static void testHmacSha512Vectors(void)
{
	uint8 arrKey[131];
	uint8 arrMac[XRT_SHA512_SIZE];

	testRequire(xrtHmacSha384(NULL, 0, NULL, 0, arrMac),
		"HMAC-SHA384 empty vector failed");
	testCryptoDigest(arrMac, XRT_SHA384_SIZE,
		"6c1f2ee938fad2e24bd91298474382ca218c75db3d83e114b3d4367776d14d3551289e75e8209cd4b792302840234adc",
		"HMAC-SHA384 empty vector mismatch");
	testRequire(xrtHmacSha512(NULL, 0, NULL, 0, arrMac),
		"HMAC-SHA512 empty vector failed");
	testCryptoDigest(arrMac, XRT_SHA512_SIZE,
		"b936cee86c9f87aa5d3c6f2e84cb5a4239a5fe50480a6ec66b70ab5b1f4ac6730c6c515421b327ec1d69402e53dfb49ad7381eb067b338fd7b0cb22247225d47",
		"HMAC-SHA512 empty vector mismatch");
	testRequire(xrtHmacSha384(
			"Jefe", 4, "what do ya want for nothing?", 28, arrMac
		), "HMAC-SHA384 RFC 4231 case 2 failed");
	testCryptoDigest(arrMac, XRT_SHA384_SIZE,
		"af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649",
		"HMAC-SHA384 RFC 4231 case 2 mismatch");
	testRequire(xrtHmacSha512(
			"Jefe", 4, "what do ya want for nothing?", 28, arrMac
		), "HMAC-SHA512 RFC 4231 case 2 failed");
	testCryptoDigest(arrMac, XRT_SHA512_SIZE,
		"164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",
		"HMAC-SHA512 RFC 4231 case 2 mismatch");
	memset(arrKey, 0xAA, sizeof(arrKey));
	testRequire(xrtHmacSha384(
			arrKey, sizeof(arrKey),
			"Test Using Larger Than Block-Size Key - Hash Key First", 54,
			arrMac
		), "HMAC-SHA384 long key vector failed");
	testCryptoDigest(arrMac, XRT_SHA384_SIZE,
		"4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952",
		"HMAC-SHA384 long key vector mismatch");
	testRequire(xrtHmacSha512(
			arrKey, sizeof(arrKey),
			"Test Using Larger Than Block-Size Key - Hash Key First", 54,
			arrMac
		), "HMAC-SHA512 long key vector failed");
	testCryptoDigest(arrMac, XRT_SHA512_SIZE,
		"80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598",
		"HMAC-SHA512 long key vector mismatch");
}



/* 两种 64 位 SHA-2 HMAC 的流式 Final 都必须是可继续的快照。 */
static void testHmacSha512Streaming(void)
{
	xhmacsha384 Sha384;
	xhmacsha512 Sha512;
	uint8 arrMac[XRT_SHA512_SIZE];
	uint8 arrExpected[XRT_SHA512_SIZE];

	testRequire(xrtHmacSha384Init(&Sha384, "secret", 6) &&
		xrtHmacSha384Update(&Sha384, "hello ", 6) &&
		xrtHmacSha384Update(&Sha384, "world", 5) &&
		xrtHmacSha384Final(&Sha384, arrMac) &&
		xrtHmacSha384("secret", 6, "hello world", 11, arrExpected) &&
		xrtConstTimeEqual(arrMac, arrExpected, XRT_SHA384_SIZE),
		"HMAC-SHA384 streaming mismatch");
	testRequire(xrtHmacSha512Init(&Sha512, "secret", 6) &&
		xrtHmacSha512Update(&Sha512, "hello", 5) &&
		xrtHmacSha512Final(&Sha512, arrMac) &&
		xrtHmacSha512Final(&Sha512, arrExpected) &&
		xrtConstTimeEqual(arrMac, arrExpected, XRT_SHA512_SIZE) &&
		xrtHmacSha512Update(&Sha512, " world", 6) &&
		xrtHmacSha512Final(&Sha512, arrMac) &&
		xrtHmacSha512("secret", 6, "hello world", 11, arrExpected) &&
		xrtConstTimeEqual(arrMac, arrExpected, XRT_SHA512_SIZE),
		"HMAC-SHA512 snapshot continuation mismatch");
}



/* 算法串用、空参数和未初始化状态必须被拒绝。 */
static void testHmacSha512Invalid(void)
{
	xhmacsha512 State;
	xhmacsha512 Before;

	xrtClearError();
	testRequire(!xrtHmacSha384Init(NULL, NULL, 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA384 null state contract failed");
	memset(&State, 0xA5, sizeof(State));
	Before = State;
	xrtClearError();
	testRequire(!xrtHmacSha512Init(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA512 invalid key modified state");
	testRequire(xrtHmacSha384Init(&State, "key", 3),
		"HMAC-SHA384 valid init failed");
	Before = State;
	xrtClearError();
	testRequire(!xrtHmacSha512Update(&State, NULL, 0) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HMAC-SHA512 accepted HMAC-SHA384 state");
	xrtClearError();
	testRequire(!xrtHmacSha384Update(&State, NULL, 1) &&
		 (memcmp(&State, &Before, sizeof(State)) == 0) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA384 invalid update modified state");
	xrtClearError();
	testRequire(!xrtHmacSha384Final(&State, NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA384 null output contract failed");
	xrtClearError();
	testRequire(!xrtHmacSha512("key", 3, "data", 4, NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HMAC-SHA512 one-shot null output contract failed");
}



/* 执行 HMAC-SHA384/512 向量、流式和失败契约测试。 */
int main(void)
{
	testHmacSha512Vectors();
	testHmacSha512Streaming();
	testHmacSha512Invalid();
	return 0;
}
