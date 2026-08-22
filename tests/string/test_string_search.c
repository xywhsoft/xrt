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



/* 按 XRT 的 ASCII 规则折叠一个字节。 */
static unsigned char testFold(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 使用朴素算法查找明确长度字节串。 */
static size_t testFind(xstrview Text, xstrview Part, size_t iStart, bool bCase)
{
	if ( iStart > Text.Size ) {
		return XRT_NPOS;
	}
	if ( Part.Size == 0 ) {
		return iStart;
	}
	if ( Part.Size > (Text.Size - iStart) ) {
		return XRT_NPOS;
	}
	for ( size_t i = iStart; i <= (Text.Size - Part.Size); i++ ) {
		size_t j = 0;

		while ( j < Part.Size ) {
			unsigned char iText = (unsigned char)Text.Data[i + j];
			unsigned char iPart = (unsigned char)Part.Data[j];

			if ( bCase ) {
				iText = testFold(iText);
				iPart = testFold(iPart);
			}
			if ( iText != iPart ) {
				break;
			}
			j++;
		}
		if ( j == Part.Size ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 使用朴素算法从右侧查找明确长度字节串。 */
static size_t testRFind(xstrview Text, xstrview Part, bool bCase)
{
	if ( Part.Size == 0 ) {
		return Text.Size;
	}
	if ( Part.Size > Text.Size ) {
		return XRT_NPOS;
	}
	for ( size_t i = (Text.Size - Part.Size) + 1u; i > 0; i-- ) {
		if ( testFind(Text, Part, i - 1u, bCase) == (i - 1u) ) {
			return i - 1u;
		}
	}
	return XRT_NPOS;
}



/* 使用朴素算法统计不重叠匹配。 */
static size_t testCount(xstrview Text, xstrview Part, bool bCase)
{
	size_t iCount = 0;
	size_t iPosition = 0;

	if ( Part.Size == 0 ) {
		return 0;
	}
	while ( iPosition <= Text.Size ) {
		iPosition = testFind(Text, Part, iPosition, bCase);
		if ( iPosition == XRT_NPOS ) {
			break;
		}
		iCount++;
		iPosition += Part.Size;
	}
	return iCount;
}



/* 对随机二进制输入交叉验证全部搜索路径。 */
int main(void)
{
	char arrText[96];
	char arrPart[24];
	char arrSet[16];
	uint32 iState = UINT32_C(0xC001D00D);

	for ( size_t iRound = 0; iRound < 200000u; iRound++ ) {
		size_t iTextSize = testNext(&iState) % sizeof(arrText);
		size_t iPartSize = testNext(&iState) % sizeof(arrPart);
		size_t iSetSize = testNext(&iState) % sizeof(arrSet);
		size_t iStart = testNext(&iState) % (sizeof(arrText) + 1u);
		xstrview Text;
		xstrview Part;
		xstrview Set;
		size_t iAny = XRT_NPOS;

		for ( size_t i = 0; i < iTextSize; i++ ) {
			arrText[i] = (char)testNext(&iState);
		}
		for ( size_t i = 0; i < iPartSize; i++ ) {
			arrPart[i] = (char)testNext(&iState);
		}
		for ( size_t i = 0; i < iSetSize; i++ ) {
			arrSet[i] = (char)testNext(&iState);
		}
		Text = xrtStrViewN(arrText, iTextSize);
		Part = xrtStrViewN(arrPart, iPartSize);
		Set = xrtStrViewN(arrSet, iSetSize);
		testRequire(xrtStrFind(Text, Part, iStart) ==
			testFind(Text, Part, iStart, false), "random exact find mismatch");
		testRequire(xrtStrCaseFind(Text, Part, iStart) ==
			testFind(Text, Part, iStart, true), "random case find mismatch");
		testRequire(xrtStrRFind(Text, Part) ==
			testRFind(Text, Part, false), "random exact reverse-find mismatch");
		testRequire(xrtStrCaseRFind(Text, Part) ==
			testRFind(Text, Part, true), "random case reverse-find mismatch");
		testRequire(xrtStrCount(Text, Part) ==
			testCount(Text, Part, false), "random exact count mismatch");
		testRequire(xrtStrCaseCount(Text, Part) ==
			testCount(Text, Part, true), "random case count mismatch");

		if ( iStart <= iTextSize ) {
			for ( size_t i = iStart; i < iTextSize; i++ ) {
				for ( size_t j = 0; j < iSetSize; j++ ) {
					if ( (unsigned char)arrText[i] == (unsigned char)arrSet[j] ) {
						iAny = i;
						break;
					}
				}
				if ( iAny != XRT_NPOS ) {
					break;
				}
			}
		}
		testRequire(xrtStrFindAny(Text, Set, iStart) == iAny,
			"random byte-set find mismatch");
	}
	printf("[PASS] string-search\n");
	return 0;
}
