#include "../test.h"



/* 生成可复现随机数。 */
static uint32 testFormRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 验证随机二进制字段经序列化和原地解析后保持顺序与字节。 */
int main(void)
{
	xformfield Input[24];
	xformfield Output[24];
	uint8 Names[24][16];
	uint8 Values[24][16];
	char Text[4096];
	uint32 iState = UINT32_C(0x51F15EED);
	size_t iCase;

	for ( iCase = 0; iCase < 6000u; iCase++ ) {
		size_t iCount = (size_t)(testFormRandom(&iState) % 25u);
		size_t iText;
		size_t iParsed;
		size_t iFindOffset = 0;
		size_t i;

		for ( i = 0; i < iCount; i++ ) {
			size_t iName = (size_t)(testFormRandom(&iState) % 17u);
			size_t iValue = (size_t)(testFormRandom(&iState) % 17u);
			size_t j;

			for ( j = 0; j < iName; j++ ) {
				Names[i][j] = (uint8)testFormRandom(&iState);
			}
			for ( j = 0; j < iValue; j++ ) {
				Values[i][j] = (uint8)testFormRandom(&iState);
			}
			Input[i].Name = (xbytesview){ Names[i], iName };
			Input[i].Value = (xbytesview){ Values[i], iValue };
		}
		testRequire(xrtFormWrite(
			Input, iCount, Text, sizeof(Text), &iText
		), "form mutation write failed");
		for ( i = 0; i < iCount; i++ ) {
			uint8 Value[16];
			size_t iValue;

			testRequire(xrtFormFind(
				(xstrview){ Text, iText }, Input[i].Name, &iFindOffset,
				Value, sizeof(Value), &iValue
			) == XFORM_FIND_FOUND && (iValue == Input[i].Value.Size) &&
				((iValue == 0) ||
				 (memcmp(Value, Input[i].Value.Data, iValue) == 0)),
				"form mutation direct lookup mismatch");
		}
		testRequire(xrtFormParse(
			Text, iText, Output,
			sizeof(Output) / sizeof(Output[0]), &iParsed, NULL
		) && (iParsed == iCount), "form mutation parse failed");
		for ( i = 0; i < iCount; i++ ) {
			testRequire((Output[i].Name.Size == Input[i].Name.Size) &&
				(Output[i].Value.Size == Input[i].Value.Size) &&
				((Input[i].Name.Size == 0) || (memcmp(
					Output[i].Name.Data, Input[i].Name.Data, Input[i].Name.Size
				) == 0)) && ((Input[i].Value.Size == 0) || (memcmp(
					Output[i].Value.Data, Input[i].Value.Data, Input[i].Value.Size
				) == 0)), "form mutation field mismatch");
		}
	}
	printf("[PASS] form_urlencoded_mutation\n");
	return 0;
}
