#include "../test.h"



/* 生成可复现的伪随机字节，避免随机测试依赖平台运行库。 */
static uint32 testPercentRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 验证大量任意字节和大小写混合转义都能稳定往返。 */
int main(void)
{
	uint8 Input[128];
	char Encoded[385];
	uint8 Decoded[128];
	uint32 iState = UINT32_C(0x9E3779B9);
	size_t iCase;

	for ( iCase = 0; iCase < 6000u; iCase++ ) {
		size_t iInput = (size_t)(testPercentRandom(&iState) % 129u);
		size_t iEncoded;
		size_t iDecoded;
		size_t i;

		for ( i = 0; i < iInput; i++ ) {
			Input[i] = (uint8)testPercentRandom(&iState);
		}
		testRequire(xrtPercentEncode(
			Input, iInput, XRT_STR_LITERAL(""),
			Encoded, sizeof(Encoded), &iEncoded
		), "percent mutation encode failed");
		for ( i = 1; i < iEncoded; i++ ) {
			if ( (Encoded[i - 1u] == '%') &&
				(Encoded[i] >= 'A') && (Encoded[i] <= 'F') &&
				((testPercentRandom(&iState) & 1u) != 0) ) {
				Encoded[i] = (char)(Encoded[i] + ('a' - 'A'));
			}
		}
		testRequire(xrtPercentDecode(
			(xstrview){ Encoded, iEncoded }, Decoded, sizeof(Decoded), &iDecoded
		) && (iDecoded == iInput) && (memcmp(Decoded, Input, iInput) == 0),
			"percent mutation round trip mismatch");
	}
	printf("[PASS] codec_percent_mutation\n");
	return 0;
}
