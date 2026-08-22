#include "../test.h"



/* 生成可重复的轻量伪随机序列。 */
static uint32 testCookieRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 随机构建再解析，验证数量和全部借用字节保持一致。 */
int main(void)
{
	static const char Alphabet[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
	uint32 iState = UINT32_C(0xC001C0DE);
	size_t iRound;

	for ( iRound = 0; iRound < 6000; iRound++ ) {
		char Names[8][17];
		char Values[8][33];
		xcookiepair Input[8];
		xcookiepair Output[8];
		char Text[512];
		size_t iCount = (testCookieRandom(&iState) % 8u) + 1u;
		size_t iParsed;
		size_t iSize;
		size_t i;

		for ( i = 0; i < iCount; i++ ) {
			size_t iName = (testCookieRandom(&iState) % 16u) + 1u;
			size_t iValue = testCookieRandom(&iState) % 32u;
			size_t j;

			for ( j = 0; j < iName; j++ ) {
				Names[i][j] = Alphabet[
					testCookieRandom(&iState) % (sizeof(Alphabet) - 1u)
				];
			}
			for ( j = 0; j < iValue; j++ ) {
				Values[i][j] = Alphabet[
					testCookieRandom(&iState) % (sizeof(Alphabet) - 1u)
				];
			}
			Input[i].Name = (xstrview){ Names[i], iName };
			Input[i].Value = (xstrview){ Values[i], iValue };
		}
		testRequire(xrtCookieWrite(
			Input, iCount, Text, sizeof(Text), &iSize
		), "cookie mutation write failed");
		testRequire(xrtCookieParse(
			(xstrview){ Text, iSize }, Output, 8, &iParsed, NULL
		) && (iParsed == iCount), "cookie mutation parse failed");
		for ( i = 0; i < iCount; i++ ) {
			testRequire((Output[i].Name.Size == Input[i].Name.Size) &&
				(memcmp(Output[i].Name.Data, Input[i].Name.Data,
				 Input[i].Name.Size) == 0) &&
				(Output[i].Value.Size == Input[i].Value.Size) &&
				(memcmp(Output[i].Value.Data, Input[i].Value.Data,
				 Input[i].Value.Size) == 0),
				"cookie mutation roundtrip mismatch");
		}
	}
	printf("[PASS] cookie_mutation\n");
	return 0;
}

