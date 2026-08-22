#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 PBKDF2-HMAC-SHA256 的单轮、双轮和高迭代标准向量。 */
static void testPbkdf2Sha256Vectors(void)
{
	uint8 arrOutput[40];

	testRequire(xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 1, arrOutput, 32
		), "PBKDF2-SHA256 one-iteration vector failed");
	testCryptoDigest(arrOutput, 32,
		"120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
		"PBKDF2-SHA256 one-iteration mismatch");
	testRequire(xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 2, arrOutput, 32
		), "PBKDF2-SHA256 two-iteration vector failed");
	testCryptoDigest(arrOutput, 32,
		"ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43",
		"PBKDF2-SHA256 two-iteration mismatch");
	testRequire(xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 4096, arrOutput, 32
		), "PBKDF2-SHA256 4096-iteration vector failed");
	testCryptoDigest(arrOutput, 32,
		"c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a",
		"PBKDF2-SHA256 4096-iteration mismatch");
	testRequire(xrtPbkdf2Sha256(
			"passwordPASSWORDpassword", 24,
			"saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
			4096, arrOutput, sizeof(arrOutput)
		), "PBKDF2-SHA256 multi-block vector failed");
	testCryptoDigest(arrOutput, sizeof(arrOutput),
		"348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9",
		"PBKDF2-SHA256 multi-block mismatch");
}



/* 验证空密码、空 salt 和跨两个完整摘要块的输出。 */
static void testPbkdf2Sha256Empty(void)
{
	uint8 arrOutput[64];

	testRequire(xrtPbkdf2Sha256(
			NULL, 0, NULL, 0, 2, arrOutput, sizeof(arrOutput)
		), "PBKDF2-SHA256 empty input failed");
	testCryptoDigest(arrOutput, sizeof(arrOutput),
		"97398411d6aea43a77acef92226ab8278d4db0668bd1d7a76a725f7680ac45c504d715e13d3c31a6c1539b9713ea6916da72027e1b2f37962f8bf0306ae25840",
		"PBKDF2-SHA256 empty input mismatch");
}



/* 验证所有参数失败都发生在输出写入之前。 */
static void testPbkdf2Sha256Edges(void)
{
	uint8 arrOutput[64];
	uint8 arrBefore[64];

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			NULL, 1, "salt", 4, 2, arrOutput, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrOutput, arrBefore, sizeof(arrOutput)),
		"PBKDF2-SHA256 accepted an invalid password range");
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			"password", 8, NULL, 1, 2, arrOutput, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrOutput, arrBefore, sizeof(arrOutput)),
		"PBKDF2-SHA256 accepted an invalid salt range");
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 0, arrOutput, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrOutput, arrBefore, sizeof(arrOutput)),
		"PBKDF2-SHA256 accepted zero iterations");
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 2, NULL, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"PBKDF2-SHA256 accepted a null output");
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 2, NULL, 0
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"PBKDF2-SHA256 accepted an empty output");
	memcpy(arrOutput, "password", 8);
	memcpy(arrBefore, arrOutput, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			arrOutput, 8, "salt", 4, 2, arrOutput, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrOutput, arrBefore, sizeof(arrOutput)),
		"PBKDF2-SHA256 accepted password/output overlap");
	memcpy(arrOutput, "salt", 4);
	memcpy(arrBefore, arrOutput, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			"password", 8, arrOutput, 4, 2, arrOutput, 32
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrOutput, arrBefore, sizeof(arrOutput)),
		"PBKDF2-SHA256 accepted salt/output overlap");
}



/* 在 64 位目标上验证第 2^32 个块在解引用输出前被拒绝。 */
static void testPbkdf2Sha256Limit(void)
{
#if SIZE_MAX > UINT32_MAX
	size_t iTooLarge = ((size_t)UINT32_MAX * XRT_SHA256_SIZE) + 1u;

	xrtClearError();
	testRequire(!xrtPbkdf2Sha256(
			NULL, 0, NULL, 0, 1, (void*)(uintptr_t)1, iTooLarge
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"PBKDF2-SHA256 accepted a 2^32nd output block");
#endif
}



/* 执行 PBKDF2-SHA256 向量、边界和块上限测试。 */
int main(void)
{
	testPbkdf2Sha256Vectors();
	testPbkdf2Sha256Empty();
	testPbkdf2Sha256Edges();
	testPbkdf2Sha256Limit();
	return 0;
}
