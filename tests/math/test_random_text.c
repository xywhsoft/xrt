#include "../test.h"



/* 验证可复现文本、默认字母表、自定义字母表和失败原子性。 */
int main(void)
{
	xrng First;
	xrng Second;
	xrng Before;
	char arrFirst[65];
	char arrSecond[65];
	char arrOutput[16];
	char arrBefore[16];
	char arrAlphabet[] = "abc";
	str sText;

	xrtRngSeed(&First, 42, 54);
	xrtRngSeed(&Second, 42, 54);
	testRequire(xrtRngReady(&First), "seeded RNG must be ready");
	testRequire(xrtRngText(&First, XRT_STR_LITERAL("abc"), arrFirst,
		sizeof(arrFirst), 64), "random text write failed");
	testRequire(xrtRngText(&Second, XRT_STR_LITERAL("abc"), arrSecond,
		sizeof(arrSecond), 64), "second random text write failed");
	testRequire(strcmp(arrFirst, arrSecond) == 0,
		"same RNG seed did not reproduce text");
	for ( size_t i = 0; i < 64; i++ ) {
		testRequire((arrFirst[i] == 'a') || (arrFirst[i] == 'b') ||
			(arrFirst[i] == 'c'), "random text escaped its alphabet");
	}

	xrtRngSeed(&First, 1, 2);
	sText = xrtRngString(&First, 32);
	testRequire((sText != NULL) && (strlen(sText) == 32),
		"default random string length mismatch");
	for ( size_t i = 0; i < 32; i++ ) {
		testRequire(((sText[i] >= '0') && (sText[i] <= '9')) ||
			((sText[i] >= 'A') && (sText[i] <= 'Z')) ||
			((sText[i] >= 'a') && (sText[i] <= 'z')) ||
			(sText[i] == '-') || (sText[i] == '_'),
			"default random string alphabet mismatch");
	}
	xrtFree(sText);
	sText = xrtRngString(&First, 0);
	testRequire((sText != NULL) && (sText[0] == 0),
		"empty random string ownership mismatch");
	xrtFree(sText);

	/* 默认便捷层必须复用同一条 32 位无偏采样序列。 */
	xrtRngSeed(&First, 99, 7);
	xrtRngSeed(&Second, 99, 7);
	testRequire(xrtRngText(&First,
		XRT_STR_LITERAL("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"),
		arrFirst, sizeof(arrFirst), 64), "default alphabet base write failed");
	sText = xrtRngString(&Second, 64);
	testRequire((sText != NULL) && (strcmp(arrFirst, sText) == 0),
		"default random string diverged from base writer");
	xrtFree(sText);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	xrtRngSeed(&First, 7, 8);
	Before = First;
	testRequire(!xrtRngText(&First, XRT_STR_LITERAL("abc"), arrOutput,
		4, 4), "short random text buffer must fail");
	testRequire((memcmp(&First, &Before, sizeof(First)) == 0) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"short random text buffer changed state or output");
	xrtClearError();
	testRequire(!xrtRngText(&First, XRT_STR_LITERAL("aba"), arrOutput,
		sizeof(arrOutput), 4), "duplicate random alphabet must fail");
	testRequire(memcmp(&First, &Before, sizeof(First)) == 0,
		"invalid random alphabet advanced state");
	xrtClearError();
	testRequire(!xrtRngText(&First,
		(xstrview){ arrAlphabet, sizeof(arrAlphabet) - 1u },
		arrAlphabet, sizeof(arrAlphabet), 2),
		"random text output/alphabet overlap must fail");
	testRequire(memcmp(&First, &Before, sizeof(First)) == 0,
		"random text overlap advanced state");
	xrtClearError();

	printf("[PASS] random-text\n");
	return 0;
}
