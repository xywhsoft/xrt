#include "../test.h"



/* 安全清零必须覆盖完整区间，并接受空区间。 */
static void testSecureZero(void)
{
	uint8 arrData[64];

	memset(arrData, 0xA5, sizeof(arrData));
	xrtSecureZero(arrData, sizeof(arrData));
	for ( size_t i = 0; i < sizeof(arrData); i++ ) {
		testRequire(arrData[i] == 0, "secure zero left data behind");
	}
	xrtSecureZero(NULL, 0);
}



/* 常量时间比较必须正确处理相同、不同和空区间。 */
static void testConstTimeEqual(void)
{
	uint8 arrLeft[32];
	uint8 arrRight[32];

	memset(arrLeft, 0x5A, sizeof(arrLeft));
	memcpy(arrRight, arrLeft, sizeof(arrRight));
	testRequire(xrtConstTimeEqual(arrLeft, arrRight, sizeof(arrLeft)),
		"constant-time equal rejected equal data");
	arrRight[0] ^= 1;
	testRequire(!xrtConstTimeEqual(arrLeft, arrRight, sizeof(arrLeft)),
		"constant-time equal missed the first byte");
	memcpy(arrRight, arrLeft, sizeof(arrRight));
	arrRight[sizeof(arrRight) - 1] ^= 1;
	testRequire(!xrtConstTimeEqual(arrLeft, arrRight, sizeof(arrLeft)),
		"constant-time equal missed the last byte");
	testRequire(xrtConstTimeEqual(NULL, NULL, 0),
		"constant-time equal rejected an empty range");
}



/* 摘要元数据必须独立于具体摘要实现的裁剪状态。 */
static void testCryptoHashSize(void)
{
	testRequire(
		(xrtCryptoHashSize(XCRYPTO_HASH_SHA1) == XRT_SHA1_SIZE) &&
		(xrtCryptoHashSize(XCRYPTO_HASH_SHA224) == XRT_SHA224_SIZE) &&
		(xrtCryptoHashSize(XCRYPTO_HASH_SHA256) == XRT_SHA256_SIZE) &&
		(xrtCryptoHashSize(XCRYPTO_HASH_SHA384) == XRT_SHA384_SIZE) &&
		(xrtCryptoHashSize(XCRYPTO_HASH_SHA512) == XRT_SHA512_SIZE) &&
		(xrtCryptoHashSize((xcryptohash)0) == 0),
		"cryptographic hash size metadata mismatch"
	);
}



/* 非空区间的空指针必须设置参数错误，不得静默成功。 */
static void testCryptoCoreInvalid(void)
{
	xrtClearError();
	xrtSecureZero(NULL, 1);
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"secure zero null input reported the wrong error");

	xrtClearError();
	testRequire(!xrtConstTimeEqual(NULL, "x", 1),
		"constant-time equal accepted a null input");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"constant-time equal null input reported the wrong error");
}



/* 执行密码学基础内存原语测试。 */
int main(void)
{
	testSecureZero();
	testConstTimeEqual();
	testCryptoHashSize();
	testCryptoCoreInvalid();
	return 0;
}
