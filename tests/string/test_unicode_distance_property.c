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



/* 使用完整二维动态规划计算短 ASCII 文本的参考距离。 */
static size_t testDistance(cstr sLeft, size_t iLeftSize,
	cstr sRight, size_t iRightSize)
{
	size_t arrDistance[17][17];

	for ( size_t i = 0; i <= iLeftSize; i++ ) {
		arrDistance[i][0] = i;
	}
	for ( size_t j = 0; j <= iRightSize; j++ ) {
		arrDistance[0][j] = j;
	}
	for ( size_t i = 1; i <= iLeftSize; i++ ) {
		for ( size_t j = 1; j <= iRightSize; j++ ) {
			size_t iDelete = arrDistance[i - 1u][j] + 1u;
			size_t iInsert = arrDistance[i][j - 1u] + 1u;
			size_t iReplace = arrDistance[i - 1u][j - 1u] +
				(sLeft[i - 1u] == sRight[j - 1u] ? 0u : 1u);
			size_t iValue = iDelete < iInsert ? iDelete : iInsert;

			if ( iReplace < iValue ) {
				iValue = iReplace;
			}
			arrDistance[i][j] = iValue;
		}
	}
	return arrDistance[iLeftSize][iRightSize];
}



/* 随机交叉验证精确距离和全部小阈值带状路径。 */
int main(void)
{
	char arrLeft[16];
	char arrRight[16];
	uint32 iState = UINT32_C(0x51A17EED);

	for ( size_t iRound = 0; iRound < 100000u; iRound++ ) {
		size_t iLeftSize = testNext(&iState) % (sizeof(arrLeft) + 1u);
		size_t iRightSize = testNext(&iState) % (sizeof(arrRight) + 1u);
		size_t iExpected;
		xstrview Left;
		xstrview Right;

		for ( size_t i = 0; i < iLeftSize; i++ ) {
			arrLeft[i] = (char)('a' + (testNext(&iState) % 5u));
		}
		for ( size_t i = 0; i < iRightSize; i++ ) {
			arrRight[i] = (char)('a' + (testNext(&iState) % 5u));
		}
		Left = (xstrview){ arrLeft, iLeftSize };
		Right = (xstrview){ arrRight, iRightSize };
		iExpected = testDistance(arrLeft, iLeftSize, arrRight, iRightSize);
		testRequire(xrtUtf8Distance(Left, Right, XRT_NPOS) == iExpected,
			"random exact Unicode distance mismatch");
		for ( size_t iLimit = 0; iLimit <= 6u; iLimit++ ) {
			size_t iActual = xrtUtf8Distance(Left, Right, iLimit);
			size_t iLimited = iExpected <= iLimit ? iExpected : XRT_NPOS;

			testRequire(iActual == iLimited,
				"random limited Unicode distance mismatch");
		}
	}
	printf("[PASS] unicode-distance-property\n");
	return 0;
}
