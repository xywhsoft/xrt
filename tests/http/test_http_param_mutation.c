#include "../test.h"



/* 生成可复现的轻量伪随机序列。 */
static uint32 testHttpParamRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 随机构建后重新解析，验证 quoted-string 转义和参数边界。 */
int main(void)
{
	static const char NameAlphabet[] =
		"abcdefghijklmnopqrstuvwxyz0123456789_-";
	static const char ValueAlphabet[] =
		"abcdefghijklmnopqrstuvwxyz ;\\\"0123456789";
	uint32 iState = UINT32_C(0x50415241);
	size_t iRound;

	for ( iRound = 0; iRound < 6000; iRound++ ) {
		char Names[6][13];
		char Values[6][25];
		char Text[512];
		size_t NameSizes[6];
		size_t ValueSizes[6];
		bool Quoted[6];
		size_t iCount = (testHttpParamRandom(&iState) % 6u) + 1u;
		size_t iText = 0;
		size_t iOffset = 0;
		size_t i;

		for ( i = 0; i < iCount; i++ ) {
			size_t j;
			size_t iWritten;

			NameSizes[i] =
				(testHttpParamRandom(&iState) % 12u) + 1u;
			ValueSizes[i] =
				testHttpParamRandom(&iState) % 24u;
			Quoted[i] = (ValueSizes[i] == 0) ||
				((testHttpParamRandom(&iState) & 1u) != 0);
			for ( j = 0; j < NameSizes[i]; j++ ) {
				Names[i][j] = NameAlphabet[
					testHttpParamRandom(&iState) %
					(sizeof(NameAlphabet) - 1u)
				];
			}
			for ( j = 0; j < ValueSizes[i]; j++ ) {
				char ch = ValueAlphabet[
					testHttpParamRandom(&iState) %
					(sizeof(ValueAlphabet) - 1u)
				];

				if ( !Quoted[i] && ((ch == ' ') ||
					(ch == ';') || (ch == '\\') ||
					(ch == '\"')) ) {
					ch = 'x';
				}
				Values[i][j] = ch;
			}
			if ( i != 0 ) {
				Text[iText++] = ';';
				Text[iText++] = ' ';
			}
			testRequire(xrtHttpParamWrite(
				(xstrview){ Names[i], NameSizes[i] },
				(xstrview){ Values[i], ValueSizes[i] },
				XHTTP_PARAM_HAS_VALUE |
					(Quoted[i] ? XHTTP_PARAM_QUOTED : 0),
				Text + iText, sizeof(Text) - iText, &iWritten
			), "HTTP parameter mutation write failed");
			iText += iWritten;
		}
		for ( i = 0; i < iCount; i++ ) {
			xhttpparam Param;
			char Decoded[32];
			size_t iDecoded;

			testRequire(xrtHttpParamNext(
				(xstrview){ Text, iText }, &iOffset, &Param
			) == XHTTP_NEXT_ITEM,
				"HTTP parameter mutation parse failed");
			testRequire((Param.Name.Size == NameSizes[i]) &&
				(memcmp(Param.Name.Data, Names[i],
				 NameSizes[i]) == 0) &&
				xrtHttpParamValueWrite(
					&Param, Decoded, sizeof(Decoded), &iDecoded
				) && (iDecoded == ValueSizes[i]) &&
				(memcmp(Decoded, Values[i], ValueSizes[i]) == 0),
				"HTTP parameter mutation roundtrip mismatch");
		}
		{
			xhttpparam Param;

			testRequire(xrtHttpParamNext(
				(xstrview){ Text, iText }, &iOffset, &Param
			) == XHTTP_NEXT_END,
				"HTTP parameter mutation list did not end");
		}
	}
	printf("[PASS] http_param_mutation\n");
	return 0;
}
