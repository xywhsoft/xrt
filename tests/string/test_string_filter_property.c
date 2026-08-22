#include "../test.h"



/* 生成可复现的属性测试字节。 */
static uint32 testNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 用独立字节位图计算过滤参考结果。 */
static size_t testFilterReference(const unsigned char* pText, size_t iTextSize,
	const unsigned char* pSet, size_t iSetSize, unsigned char* pOutput)
{
	bool arrRemove[256] = { false };
	size_t iWrite = 0;

	for ( size_t i = 0; i < iSetSize; i++ ) {
		arrRemove[pSet[i]] = true;
	}
	for ( size_t i = 0; i < iTextSize; i++ ) {
		if ( !arrRemove[pText[i]] ) {
			pOutput[iWrite++] = pText[i];
		}
	}
	return iWrite;
}



/* 随机验证二进制字节过滤与独立参考实现完全一致。 */
int main(void)
{
	unsigned char arrActual[130];
	unsigned char arrExpected[129];
	unsigned char arrInPlace[130];
	unsigned char arrSet[64];
	unsigned char arrText[128];
	uint32 iState = UINT32_C(0x89ABCDEF);

	for ( size_t iCase = 0; iCase < 200000u; iCase++ ) {
		size_t iTextSize = (size_t)(testNext(&iState) % 129u);
		size_t iSetSize = (size_t)(testNext(&iState) % 65u);
		size_t iExpected;
		size_t iActual = SIZE_MAX;

		for ( size_t i = 0; i < iTextSize; i++ ) {
			arrText[i] = (unsigned char)testNext(&iState);
		}
		for ( size_t i = 0; i < iSetSize; i++ ) {
			arrSet[i] = (unsigned char)testNext(&iState);
		}
		iExpected = testFilterReference(arrText, iTextSize,
			arrSet, iSetSize, arrExpected);

		testRequire(xrtStrFilterTo(
			(xstrview){ (cstr)arrText, iTextSize },
			(xstrview){ (cstr)arrSet, iSetSize },
			NULL, 0, &iActual) && (iActual == iExpected),
			"byte filter property query mismatch");
		testRequire(xrtStrFilterTo(
			(xstrview){ (cstr)arrText, iTextSize },
			(xstrview){ (cstr)arrSet, iSetSize },
			(char*)arrActual, sizeof(arrActual), &iActual) &&
			(iActual == iExpected) &&
			(memcmp(arrActual, arrExpected, iExpected) == 0) &&
			(arrActual[iExpected] == 0),
			"byte filter property output mismatch");

		if ( (iCase & 3u) == 0 ) {
			memcpy(arrInPlace, arrText, iTextSize);
			testRequire(xrtStrFilterTo(
				(xstrview){ (cstr)arrInPlace, iTextSize },
				(xstrview){ (cstr)arrSet, iSetSize },
				(char*)arrInPlace, sizeof(arrInPlace), &iActual) &&
				(iActual == iExpected) &&
				(memcmp(arrInPlace, arrExpected, iExpected) == 0) &&
				(arrInPlace[iExpected] == 0),
				"byte filter property in-place mismatch");
		}
	}
	printf("[PASS] string-filter-property\n");
	return 0;
}
