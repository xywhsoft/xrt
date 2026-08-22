#include "../test.h"



/* 线性同余序列提供可复现的输入字节。 */
static uint32 testUrlRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 成功解析的任意输入必须能稳定地构建、重解析并再次构建。 */
static void testUrlStable(xstrview Input)
{
	xurl First;
	xurl Second;
	str sFirst;
	str sSecond;
	size_t iFirst;
	size_t iSecond;

	if ( !xrtUrlParse(Input, &First) ) {
		xrtClearError();
		return;
	}
	sFirst = xrtUrlBuild(&First, &iFirst);
	testRequire((sFirst != NULL) && (iFirst == Input.Size) &&
		((iFirst == 0) || (memcmp(sFirst, Input.Data, iFirst) == 0)),
		"URL mutation first build did not preserve parsed text");
	testRequire(xrtUrlParse(
		(xstrview){ sFirst, iFirst }, &Second
	), "URL mutation rebuilt text did not parse");
	sSecond = xrtUrlBuild(&Second, &iSecond);
	testRequire((sSecond != NULL) && (iFirst == iSecond) &&
		(memcmp(sFirst, sSecond, iFirst + 1u) == 0),
		"URL mutation build was not stable");
	xrtFree(sSecond);
	xrtFree(sFirst);
}



/* 任意合法路径的测长、直接写出和分配写出必须完全一致。 */
static void testUrlNormalizeStable(xstrview Path)
{
	char Output[96];
	str sBuilt;
	size_t iMeasured;
	size_t iWritten;
	size_t iBuilt;

	testRequire(xrtUrlPathNormalize(
		Path, NULL, 0, &iMeasured
	), "URL mutation normalize size query failed");
	testRequire(xrtUrlPathNormalize(
		Path, Output, sizeof(Output), &iWritten
	) && (iWritten == iMeasured),
		"URL mutation normalize direct size diverged");
	sBuilt = xrtUrlPathNormalizeBuild(Path, &iBuilt);
	testRequire((sBuilt != NULL) &&
		(iBuilt == iMeasured) &&
		((iBuilt == 0) ||
		 (memcmp(Output, sBuilt, iBuilt) == 0)),
		"URL mutation normalize Build diverged");
	xrtFree(sBuilt);
}



/* 扫描随机输入及其全部前缀，覆盖分隔符与 percent 边界。 */
int main(void)
{
	static const char Alphabet[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789:/?#[]@!$&'()*+,;=.%_-~ \r\n";
	char Input[96];
	static const char PathAlphabet[] = "ab./";
	uint32 iState = UINT32_C(0x71A5C39D);
	size_t iCase;

	for ( iCase = 0; iCase < 6000; iCase++ ) {
		size_t iSize = testUrlRandom(&iState) % sizeof(Input);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			Input[i] = Alphabet[
				testUrlRandom(&iState) % (sizeof(Alphabet) - 1u)
			];
		}
		for ( i = 0; i <= iSize; i++ ) {
			testUrlStable((xstrview){ Input, i });
		}
	}
	for ( iCase = 0; iCase < 6000; iCase++ ) {
		size_t iSize = testUrlRandom(&iState) % sizeof(Input);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			Input[i] = PathAlphabet[
				testUrlRandom(&iState) %
				(sizeof(PathAlphabet) - 1u)
			];
		}
		testUrlNormalizeStable(
			(xstrview){ Input, iSize }
		);
	}
	printf("[PASS] url_mutation\n");
	return 0;
}
