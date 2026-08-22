#include "../test.h"

#include "../../fuzz/websocket_protocol.c"



#ifndef XRT_WS_FUZZ_ROUNDS
	#define XRT_WS_FUZZ_ROUNDS 2000u
#endif

#define XRT_WS_FUZZ_TEST_MAX 4096u



/* 生成可重复的协议噪声，保证普通测试环境也持续执行 fuzz 入口。 */
static uint32 testWsFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 先执行固定协议种子，再运行固定种子的变长随机输入。 */
int main(void)
{
	static const uint8 Seeds[][32] = {
		{ 0x81, 0x00 },
		{ 0x81, 0x80, 0x12, 0x34, 0x56, 0x78 },
		{ 0x88, 0x02, 0x03, 0xE8 },
		{ 0x89, 0x00 },
		{ 0xC1, 0x00 },
		{ 0xFF, 0xFF, 0xFF, 0xFF }
	};
	static const size_t SeedSizes[] = {
		2,
		6,
		4,
		2,
		2,
		4
	};
	uint8 Data[XRT_WS_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x7F4A7C15);

	testRequire(
		LLVMFuzzerTestOneInput(NULL, 0) == 0,
		"WebSocket empty fuzz seed failed"
	);
	for ( size_t i = 0;
		i < sizeof(Seeds) / sizeof(Seeds[0]);
		i++ ) {
		testRequire(
			LLVMFuzzerTestOneInput(
				Seeds[i],
				SeedSizes[i]
			) == 0,
			"WebSocket fixed fuzz seed failed"
		);
	}
	for ( size_t iRound = 0;
		iRound < XRT_WS_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = (size_t)(
			testWsFuzzNext(&iState) %
			(XRT_WS_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(
				testWsFuzzNext(&iState) >> 24u
			);
		}
		testRequire(
			LLVMFuzzerTestOneInput(Data, iSize) == 0,
			"WebSocket deterministic fuzz round failed"
		);
	}
	printf(
		"[PASS] WebSocket protocol fuzz (%u rounds)\n",
		(unsigned int)XRT_WS_FUZZ_ROUNDS
	);
	return 0;
}
