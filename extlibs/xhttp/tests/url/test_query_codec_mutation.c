#include "../test.h"



/* 生成可重复的伪随机测试字节。 */
static uint32 testQueryCodecRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 随机验证二进制字段的 Query 编码、扫描和解码闭环。 */
int main(void)
{
	uint32 iState = UINT32_C(0x51434F44);
	size_t iCase;

	for ( iCase = 0; iCase < 6000; iCase++ ) {
		uint8 Keys[8][16];
		uint8 Values[8][16];
		xquerypair Pairs[8];
		char Text[1024];
		size_t iCount = (size_t)(testQueryCodecRandom(&iState) % 8u) + 1u;
		size_t iText;
		size_t iOffset = 0;
		size_t i;

		for ( i = 0; i < iCount; i++ ) {
			size_t iKey = (size_t)(testQueryCodecRandom(&iState) % 17u);
			size_t iValue = (size_t)(testQueryCodecRandom(&iState) % 17u);
			bool bValue = (testQueryCodecRandom(&iState) & 1u) != 0;
			size_t j;

			if ( !bValue && (iKey == 0) ) {
				iKey = 1;
			}
			for ( j = 0; j < iKey; j++ ) {
				Keys[i][j] = (uint8)testQueryCodecRandom(&iState);
			}
			for ( j = 0; j < iValue; j++ ) {
				Values[i][j] = (uint8)testQueryCodecRandom(&iState);
			}
			Pairs[i].Flags = bValue ? XQUERY_HAS_VALUE : 0;
			Pairs[i].Key = (xstrview){ (cstr)Keys[i], iKey };
			Pairs[i].Value = bValue ?
				(xstrview){ (cstr)Values[i], iValue } :
				(xstrview){ NULL, 0 };
		}
		testRequire(xrtQueryWrite(
			Pairs, iCount, Text, sizeof(Text), &iText
		), "query codec mutation write failed");
		for ( i = 0; i < iCount; i++ ) {
			uint8 Key[16];
			uint8 Value[16];
			xquerypair Parsed;
			size_t iKey;
			size_t iValue;

			testRequire(xrtQueryNext(
				(xstrview){ Text, iText }, &iOffset, &Parsed
			) == XQUERY_NEXT_ITEM, "query codec mutation item missing");
			testRequire(xrtPercentDecode(
				Parsed.Key, Key, sizeof(Key), &iKey
			) && (iKey == Pairs[i].Key.Size) &&
				((iKey == 0) || (memcmp(Key, Pairs[i].Key.Data, iKey) == 0)),
				"query codec mutation key mismatch");
			testRequire(
				((Parsed.Flags & XQUERY_HAS_VALUE) != 0) ==
				((Pairs[i].Flags & XQUERY_HAS_VALUE) != 0),
				"query codec mutation value state mismatch");
			if ( (Pairs[i].Flags & XQUERY_HAS_VALUE) != 0 ) {
				testRequire(xrtPercentDecode(
					Parsed.Value, Value, sizeof(Value), &iValue
				) && (iValue == Pairs[i].Value.Size) &&
					((iValue == 0) ||
					 (memcmp(Value, Pairs[i].Value.Data, iValue) == 0)),
					"query codec mutation value mismatch");
			}
		}
		{
			xquerypair Parsed;

			testRequire(xrtQueryNext(
				(xstrview){ Text, iText }, &iOffset, &Parsed
			) == XQUERY_NEXT_END, "query codec mutation had extra items");
		}
	}
	printf("[PASS] query_codec_mutation\n");
	return 0;
}
