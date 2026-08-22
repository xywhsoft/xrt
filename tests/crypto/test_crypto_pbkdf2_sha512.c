#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 PBKDF2-HMAC-SHA384 的单块和跨块向量。 */
static void testPbkdf2Sha384Vectors(void)
{
	uint8 arrOutput[64];

	testRequire(xrtPbkdf2Sha384(
			"password", 8, "salt", 4, 1, arrOutput, 48
		), "PBKDF2-SHA384 one-iteration vector failed");
	testCryptoDigest(arrOutput, 48,
		"c0e14f06e49e32d73f9f52ddf1d0c5c7191609233631dadd76a567db42b78676b38fc800cc53ddb642f5c74442e62be4",
		"PBKDF2-SHA384 one-iteration mismatch");
	testRequire(xrtPbkdf2Sha384(
			"password", 8, "salt", 4, 2, arrOutput, sizeof(arrOutput)
		), "PBKDF2-SHA384 multi-block vector failed");
	testCryptoDigest(arrOutput, sizeof(arrOutput),
		"54f775c6d790f21930459162fc535dbf04a939185127016a04176a0730c6f1f4fb48832ad1261baadd2cedd50814b1c806ad1bbf43ebdc9d047904bf7ceafe1e",
		"PBKDF2-SHA384 multi-block mismatch");
}



/* 验证 PBKDF2-HMAC-SHA512 的单块、跨块和空输入向量。 */
static void testPbkdf2Sha512Vectors(void)
{
	uint8 arrOutput[128];

	testRequire(xrtPbkdf2Sha512(
			"password", 8, "salt", 4, 1, arrOutput, 64
		), "PBKDF2-SHA512 one-iteration vector failed");
	testCryptoDigest(arrOutput, 64,
		"867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce",
		"PBKDF2-SHA512 one-iteration mismatch");
	testRequire(xrtPbkdf2Sha512(
			"password", 8, "salt", 4, 2, arrOutput, 80
		), "PBKDF2-SHA512 multi-block vector failed");
	testCryptoDigest(arrOutput, 80,
		"e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53cf76cab2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e473e311ad827b68945f4e2dddb204c78",
		"PBKDF2-SHA512 multi-block mismatch");
	testRequire(xrtPbkdf2Sha512(
			NULL, 0, NULL, 0, 2, arrOutput, sizeof(arrOutput)
		), "PBKDF2-SHA512 empty input failed");
	testCryptoDigest(arrOutput, sizeof(arrOutput),
		"a422663fda8609a1e2fd53541260edf886ec636605814c2e17b4c78d8f9e233266f223f65cb2b7440b12c099ce1aa3279e29aa848e29825103d2bc5a5f4f4ad373dfdb2360460d9823b578d103ed18b7e1f168e9a6d425e8f8c9c3f068c1b9f50b10188eb36dcf94e060756b5329917c0f216aec85e93778275d6c693f12ec6e",
		"PBKDF2-SHA512 empty input mismatch");
}



/* 验证共享 SHA-384/512 路径统一拒绝非法参数和输入输出重叠。 */
static void testPbkdf2Sha512Edges(void)
{
	uint8 arrBuffer[128];
	uint8 arrBefore[128];

	memset(arrBuffer, 0x5A, sizeof(arrBuffer));
	memcpy(arrBefore, arrBuffer, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha384(
			NULL, 1, "salt", 4, 2, arrBuffer, 48
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrBuffer, arrBefore, sizeof(arrBuffer)),
		"PBKDF2-SHA384 accepted invalid password input");
	xrtClearError();
	testRequire(!xrtPbkdf2Sha512(
			"password", 8, "salt", 4, 0, arrBuffer, 64
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrBuffer, arrBefore, sizeof(arrBuffer)),
		"PBKDF2-SHA512 accepted zero iterations");
	memcpy(arrBuffer, "password", 8);
	memcpy(arrBefore, arrBuffer, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha512(
			arrBuffer, 8, "salt", 4, 2, arrBuffer, 64
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrBuffer, arrBefore, sizeof(arrBuffer)),
		"PBKDF2-SHA512 accepted password/output overlap");
	memcpy(arrBuffer, "salt", 4);
	memcpy(arrBefore, arrBuffer, sizeof(arrBefore));
	xrtClearError();
	testRequire(!xrtPbkdf2Sha384(
			"password", 8, arrBuffer, 4, 2, arrBuffer, 48
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 xrtConstTimeEqual(arrBuffer, arrBefore, sizeof(arrBuffer)),
		"PBKDF2-SHA384 accepted salt/output overlap");
}



/* 执行 PBKDF2-SHA384/512 向量和共享边界测试。 */
int main(void)
{
	testPbkdf2Sha384Vectors();
	testPbkdf2Sha512Vectors();
	testPbkdf2Sha512Edges();
	return 0;
}
