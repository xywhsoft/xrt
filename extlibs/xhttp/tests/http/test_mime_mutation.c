#include "../test.h"



/* 生成可复现的轻量伪随机序列。 */
static uint32 testMimeRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 随机验证媒体类型的解析写出闭环。 */
int main(void)
{
	static const char Token[] =
		"abcdefghijklmnopqrstuvwxyz0123456789-_.";
	uint32 iState = UINT32_C(0x4D494D45);
	size_t iRound;

	for ( iRound = 0; iRound < 5000; iRound++ ) {
		char TypeText[160];
		char Output[160];
		char MainType[13];
		char Subtype[17];
		char ParamValue[17];
		xmediatype Type;
		size_t iType = (testMimeRandom(&iState) % 12u) + 1u;
		size_t iSubtype = (testMimeRandom(&iState) % 16u) + 1u;
		size_t iParam = (testMimeRandom(&iState) % 16u) + 1u;
		size_t iText;
		size_t iOutput;
		size_t i;

		for ( i = 0; i < iType; i++ ) {
			MainType[i] = Token[
				testMimeRandom(&iState) % (sizeof(Token) - 1u)
			];
		}
		for ( i = 0; i < iSubtype; i++ ) {
			Subtype[i] = Token[
				testMimeRandom(&iState) % (sizeof(Token) - 1u)
			];
		}
		for ( i = 0; i < iParam; i++ ) {
			ParamValue[i] = Token[
				testMimeRandom(&iState) % (sizeof(Token) - 1u)
			];
		}
		iText = 0;
		memcpy(TypeText + iText, MainType, iType);
		iText += iType;
		TypeText[iText++] = '/';
		memcpy(TypeText + iText, Subtype, iSubtype);
		iText += iSubtype;
		memcpy(TypeText + iText, "; p=", 4);
		iText += 4;
		memcpy(TypeText + iText, ParamValue, iParam);
		iText += iParam;
		testRequire(xrtHttpMediaTypeParse(
			(xstrview){ TypeText, iText }, &Type
		) && xrtHttpMediaTypeWrite(
			&Type, Output, sizeof(Output), &iOutput
		) && (iOutput == iText) &&
			(memcmp(Output, TypeText, iText) == 0),
			"MIME media type mutation roundtrip mismatch");
	}
	printf("[PASS] mime_mutation\n");
	return 0;
}

