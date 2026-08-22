#include "../test.h"

#include "../../fuzz/http_sse.c"



#ifndef XRT_HTTP_SSE_FUZZ_ROUNDS
	#define XRT_HTTP_SSE_FUZZ_ROUNDS 3000u
#endif

#define XRT_HTTP_SSE_FUZZ_TEST_MAX 512u



/* 生成可重复的 SSE 协议噪声。 */
static uint32 testHttpSseFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 执行一个固定 SSE 语料。 */
static void testHttpSseFuzzSeed(cstr sInput)
{
	testRequire(
		xrtHttpSseFuzzerTestOneInput(
			(const uint8*)sInput, strlen(sInput)
		) == 0,
		"HTTP SSE fixed fuzz seed failed"
	);
}



/* 先覆盖协议语料，再运行固定随机种子的任意字节流。 */
int main(void)
{
	uint8 Data[XRT_HTTP_SSE_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x9E3779B9);
	size_t iRound;

	testRequire(
		xrtHttpSseFuzzerTestOneInput(NULL, 0) == 0,
		"HTTP SSE empty fuzz seed failed"
	);
	testHttpSseFuzzSeed("data: hello\n\n");
	testHttpSseFuzzSeed("id: 42\rretry: 1000\rdata: x\r\r");
	testHttpSseFuzzSeed(": ping\r\nevent: update\r\ndata:\r\n\r\n");
	testHttpSseFuzzSeed("retry: invalid\ndata: incomplete");
	for ( iRound = 0; iRound < XRT_HTTP_SSE_FUZZ_ROUNDS; iRound++ ) {
		size_t iSize = (size_t)(
			testHttpSseFuzzNext(&iState) %
			(sizeof(Data) + 1u)
		);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(
				testHttpSseFuzzNext(&iState) >> 24u
			);
		}
		testRequire(
			xrtHttpSseFuzzerTestOneInput(Data, iSize) == 0,
			"HTTP SSE deterministic fuzz round failed"
		);
	}
	printf(
		"[PASS] HTTP SSE differential fuzz (%u rounds)\n",
		(unsigned int)XRT_HTTP_SSE_FUZZ_ROUNDS
	);
	return 0;
}
