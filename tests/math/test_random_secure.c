#include "../test.h"



/* 操作系统随机源必须填满缓冲，并连续给出不同的非零结果。 */
static void testSecureRandomFill(void)
{
	uint8 arrFirst[64];
	uint8 arrSecond[64];
	uint8 arrZero[64];

	memset(arrFirst, 0, sizeof(arrFirst));
	memset(arrSecond, 0, sizeof(arrSecond));
	memset(arrZero, 0, sizeof(arrZero));
	testRequire(xrtSecureRandom(arrFirst, sizeof(arrFirst)),
		"first secure random fill failed");
	testRequire(xrtSecureRandom(arrSecond, sizeof(arrSecond)),
		"second secure random fill failed");
	testRequire(memcmp(arrFirst, arrZero, sizeof(arrFirst)) != 0,
		"secure random returned an all-zero block");
	testRequire(memcmp(arrFirst, arrSecond, sizeof(arrFirst)) != 0,
		"two secure random blocks were identical");
}



/* 空区间允许空指针，非空区间必须严格拒绝空指针。 */
static void testSecureRandomInvalid(void)
{
	testRequire(xrtSecureRandom(NULL, 0),
		"secure random rejected an empty range");

	xrtClearError();
	testRequire(!xrtSecureRandom(NULL, 1),
		"secure random accepted a null output");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"secure random null output reported the wrong error");
}



/* 执行密码安全随机源的成功与参数边界测试。 */
int main(void)
{
	testSecureRandomFill();
	testSecureRandomInvalid();
	return 0;
}
