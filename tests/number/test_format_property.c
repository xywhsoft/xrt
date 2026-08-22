#include "../test.h"



/* 在不违反别名规则的情况下读取 double 位模式。 */
static uint64 testFormatBits(double fValue)
{
	uint64 iBits;

	memcpy(&iBits, &fValue, sizeof(iBits));
	return iBits;
}



/* 从指定位模式构造 double。 */
static double testFormatFromBits(uint64 iBits)
{
	double fValue;

	memcpy(&fValue, &iBits, sizeof(fValue));
	return fValue;
}



/* 生成可复现的随机位模式。 */
static uint64 testFormatNext(uint64* pState)
{
	uint64 iValue = *pState;

	iValue ^= iValue >> 12u;
	iValue ^= iValue << 25u;
	iValue ^= iValue >> 27u;
	*pState = iValue;
	return iValue * UINT64_C(2685821657736338717);
}



/* 验证 17 位一般格式和 16 位科学小数可精确往返全部有限值。 */
int main(void)
{
	char sGeneral[64];
	char sScientific[64];
	uint64 iState = UINT64_C(0xA0761D6478BD642F);

	for ( size_t i = 0; i < 500000u; i++ ) {
		uint64 iBits = testFormatNext(&iState);
		uint64 iExponent = iBits & UINT64_C(0x7FF0000000000000);
		double fValue;
		double fParsed;
		size_t iSize;

		if ( iExponent == UINT64_C(0x7FF0000000000000) ) {
			continue;
		}
		fValue = testFormatFromBits(iBits);
		testRequire(xrtNumFormatTo(fValue, XRT_STR_LITERAL(".17g"),
			sGeneral, sizeof(sGeneral), &iSize),
			"17-digit general format failed");
		testRequire(xrtNumParse(
			(xstrview){ sGeneral, iSize }, 0, &fParsed) &&
			(testFormatBits(fParsed) == iBits),
			"17-digit general format did not round trip");

		testRequire(xrtNumFormatTo(fValue, XRT_STR_LITERAL(".16e"),
			sScientific, sizeof(sScientific), &iSize),
			"17-digit scientific format failed");
		testRequire(xrtNumParse(
			(xstrview){ sScientific, iSize }, 0, &fParsed) &&
			(testFormatBits(fParsed) == iBits),
			"17-digit scientific format did not round trip");
	}
	printf("[PASS] number-format-property\n");
	return 0;
}
