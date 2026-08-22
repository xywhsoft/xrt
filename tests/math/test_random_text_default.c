#include "../test.h"



/* 验证当前线程随机文本层可显式播种并复现结果。 */
int main(void)
{
	char arrFirst[33];
	char arrSecond[33];
	str sText;

	xrtRandSeed(2026, 7);
	testRequire(xrtRandText(XRT_STR_LITERAL("abcdef"), arrFirst,
		sizeof(arrFirst), 32), "default random text write failed");
	xrtRandSeed(2026, 7);
	testRequire(xrtRandText(XRT_STR_LITERAL("abcdef"), arrSecond,
		sizeof(arrSecond), 32), "default random text replay failed");
	testRequire(strcmp(arrFirst, arrSecond) == 0,
		"default random text seed was not reproducible");
	sText = xrtRandStringFrom(XRT_STR_LITERAL("01"), 16);
	testRequire((sText != NULL) && (strlen(sText) == 16),
		"default allocated random text failed");
	xrtFree(sText);
	sText = xrtRandString(16);
	testRequire((sText != NULL) && (strlen(sText) == 16),
		"default alphabet random text failed");
	xrtFree(sText);
	printf("[PASS] random-text-default\n");
	return 0;
}
