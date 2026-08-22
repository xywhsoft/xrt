#include "../test.h"



/* 生成可重复的伪随机测试数据。 */
static uint32 testNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 使用完整动态规划匹配只含 ASCII 字面量、问号和星号的参考模式。 */
static bool testGlob(cstr sText, size_t iTextSize,
	cstr sPattern, size_t iPatternSize)
{
	bool arrMatch[25][25];

	memset(arrMatch, 0, sizeof(arrMatch));
	arrMatch[0][0] = true;
	for ( size_t j = 1; j <= iPatternSize; j++ ) {
		if ( sPattern[j - 1u] == '*' ) {
			arrMatch[0][j] = arrMatch[0][j - 1u];
		}
	}
	for ( size_t i = 1; i <= iTextSize; i++ ) {
		for ( size_t j = 1; j <= iPatternSize; j++ ) {
			char iPattern = sPattern[j - 1u];

			if ( iPattern == '*' ) {
				arrMatch[i][j] = arrMatch[i][j - 1u] ||
					arrMatch[i - 1u][j];
			} else if ( (iPattern == '?') ||
				(iPattern == sText[i - 1u]) ) {
				arrMatch[i][j] = arrMatch[i - 1u][j - 1u];
			}
		}
	}
	return arrMatch[iTextSize][iPatternSize];
}



/* 随机交叉验证贪婪匹配器的星号回溯组合。 */
int main(void)
{
	char arrText[24];
	char arrPattern[24];
	uint32 iState = UINT32_C(0x610BCA5E);

	for ( size_t iRound = 0; iRound < 200000u; iRound++ ) {
		size_t iTextSize = testNext(&iState) % (sizeof(arrText) + 1u);
		size_t iPatternSize = testNext(&iState) % (sizeof(arrPattern) + 1u);
		bool bExpected;
		bool bActual;

		for ( size_t i = 0; i < iTextSize; i++ ) {
			arrText[i] = (char)('a' + (testNext(&iState) % 3u));
		}
		for ( size_t i = 0; i < iPatternSize; i++ ) {
			uint32 iToken = testNext(&iState) % 7u;

			arrPattern[i] = iToken == 0 ? '*' :
				(iToken == 1 ? '?' : (char)('a' + (iToken % 3u)));
		}
		bExpected = testGlob(arrText, iTextSize, arrPattern, iPatternSize);
		bActual = xrtStrGlob(xrtStrViewN(arrText, iTextSize),
			xrtStrViewN(arrPattern, iPatternSize), 0);
		testRequire(bActual == bExpected, "random glob mismatch");
	}
	printf("[PASS] string-glob-property\n");
	return 0;
}
