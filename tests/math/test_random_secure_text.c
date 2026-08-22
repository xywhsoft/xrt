#include "../test.h"



/* 安全文本必须均匀约束在字母表内，并提供可释放的默认字符串。 */
static void testSecureTextSuccess(void)
{
	char arrFirst[4097];
	char arrSecond[4097];
	str sToken;

	testRequire(xrtSecureText(XRT_STR_LITERAL("ABC"), arrFirst,
		sizeof(arrFirst), sizeof(arrFirst) - 1u),
		"first secure text fill failed");
	testRequire(xrtSecureText(XRT_STR_LITERAL("ABC"), arrSecond,
		sizeof(arrSecond), sizeof(arrSecond) - 1u),
		"second secure text fill failed");
	testRequire(memcmp(arrFirst, arrSecond, sizeof(arrFirst) - 1u) != 0,
		"two secure text outputs were identical");
	for ( size_t i = 0; i < (sizeof(arrFirst) - 1u); i++ ) {
		testRequire((arrFirst[i] == 'A') || (arrFirst[i] == 'B') ||
			(arrFirst[i] == 'C'), "secure text escaped its alphabet");
	}

	sToken = xrtSecureString(43);
	testRequire((sToken != NULL) && (strlen(sToken) == 43),
		"default secure token length mismatch");
	for ( size_t i = 0; i < 43; i++ ) {
		testRequire(((sToken[i] >= '0') && (sToken[i] <= '9')) ||
			((sToken[i] >= 'A') && (sToken[i] <= 'Z')) ||
			((sToken[i] >= 'a') && (sToken[i] <= 'z')) ||
			(sToken[i] == '-') || (sToken[i] == '_'),
			"default secure token alphabet mismatch");
	}
	xrtSecureZero(sToken, 44);
	xrtFree(sToken);

	sToken = xrtSecureString(0);
	testRequire((sToken != NULL) && (sToken[0] == 0),
		"empty secure token ownership mismatch");
	xrtFree(sToken);
}



/* 参数、容量和重叠错误必须在写入输出前被拒绝。 */
static void testSecureTextFailures(void)
{
	char arrOutput[16];
	char arrBefore[16];
	char arrAlphabet[] = "abc";

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	testRequire(!xrtSecureText(XRT_STR_LITERAL("aba"), arrOutput,
		sizeof(arrOutput), 8), "duplicate secure alphabet was accepted");
	testRequire(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0,
		"invalid secure alphabet changed output");

	xrtClearError();
	testRequire(!xrtSecureText(XRT_STR_LITERAL("abc"), arrOutput,
		8, 8), "short secure text buffer was accepted");
	testRequire(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0,
		"short secure text buffer changed output");

	xrtClearError();
	testRequire(!xrtSecureText(
		(xstrview){ arrAlphabet, sizeof(arrAlphabet) - 1u },
		arrAlphabet, sizeof(arrAlphabet), 2),
		"secure text output/alphabet overlap was accepted");

	xrtClearError();
	testRequire(!xrtSecureText(XRT_STR_LITERAL("abc"), NULL, 4, 3),
		"secure text accepted a null output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"secure text null output reported the wrong error");
}



/* 执行密码安全随机文本的成功与失败边界测试。 */
int main(void)
{
	testSecureTextSuccess();
	testSecureTextFailures();
	return 0;
}
