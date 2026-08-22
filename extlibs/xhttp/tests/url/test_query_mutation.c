#include "../test.h"



/* 生成可复现随机数。 */
static uint32 testQueryRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 填充不会改变查询结构的随机原始字段。 */
static size_t testQueryBytes(
	char* sOutput,
	size_t iCapacity,
	uint32* pState,
	bool bValue
)
{
	static const char KeyAlphabet[] =
		"abcdefghijklmnopqrstuvwxyz0123456789%_-~";
	static const char ValueAlphabet[] =
		"abcdefghijklmnopqrstuvwxyz0123456789%_-=~";
	cstr sAlphabet = bValue ? ValueAlphabet : KeyAlphabet;
	size_t iAlphabet = strlen(sAlphabet);
	size_t iSize = (size_t)(testQueryRandom(pState) % (iCapacity + 1u));
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		sOutput[i] = sAlphabet[testQueryRandom(pState) % iAlphabet];
	}
	return iSize;
}



/* 验证大量原始查询项经 Write 和 Next 后保持结构稳定。 */
int main(void)
{
	xquerypair Pairs[24];
	char Keys[24][16];
	char Values[24][16];
	char Query[1024];
	uint32 iState = UINT32_C(0xC001D00D);
	size_t iCase;

	for ( iCase = 0; iCase < 6000u; iCase++ ) {
		size_t iCount = (size_t)(testQueryRandom(&iState) % 25u);
		size_t iQuery;
		size_t iOffset = 0;
		size_t i;

		for ( i = 0; i < iCount; i++ ) {
			bool bHasValue = (testQueryRandom(&iState) & 1u) != 0;
			size_t iKey = testQueryBytes(
				Keys[i], sizeof(Keys[i]), &iState, false
			);
			size_t iValue = bHasValue ? testQueryBytes(
				Values[i], sizeof(Values[i]), &iState, true
			) : 0;

			if ( !bHasValue && (iKey == 0) ) {
				Keys[i][0] = 'k';
				iKey = 1;
			}
			Pairs[i].Flags = bHasValue ? XQUERY_HAS_VALUE : 0;
			Pairs[i].Key = (xstrview){ Keys[i], iKey };
			Pairs[i].Value = bHasValue ?
				(xstrview){ Values[i], iValue } :
				(xstrview){ NULL, 0 };
		}
		testRequire(xrtQueryRawWrite(
			Pairs, iCount, Query, sizeof(Query), &iQuery
		), "query mutation write failed");
		for ( i = 0; i < iCount; i++ ) {
			xquerypair Parsed;

			testRequire(xrtQueryNext(
				(xstrview){ Query, iQuery }, &iOffset, &Parsed
			) == XQUERY_NEXT_ITEM, "query mutation item missing");
			testRequire((Parsed.Flags == Pairs[i].Flags) &&
				(Parsed.Key.Size == Pairs[i].Key.Size) &&
				(Parsed.Value.Size == Pairs[i].Value.Size) &&
				((Parsed.Key.Size == 0) || (memcmp(
					Parsed.Key.Data, Pairs[i].Key.Data, Parsed.Key.Size
				) == 0)) && ((Parsed.Value.Size == 0) || (memcmp(
					Parsed.Value.Data, Pairs[i].Value.Data, Parsed.Value.Size
				) == 0)), "query mutation item mismatch");
		}
		{
			xquerypair Parsed;

			testRequire(xrtQueryNext(
				(xstrview){ Query, iQuery }, &iOffset, &Parsed
			) == XQUERY_NEXT_END, "query mutation had extra items");
		}
	}
	printf("[PASS] query_mutation\n");
	return 0;
}
