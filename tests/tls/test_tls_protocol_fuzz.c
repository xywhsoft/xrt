#include "../test.h"

#include "../../fuzz/tls_protocol.c"



#ifndef XRT_TLS_FUZZ_ROUNDS
	#define XRT_TLS_FUZZ_ROUNDS 2000u
#endif

#define XRT_TLS_FUZZ_TEST_MAX 4096u



/* 生成可重复的 TLS 协议噪声。 */
static uint32 testTlsFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 执行固定边界种子和确定性变长随机输入。 */
int main(void)
{
	static const uint8 Record[] = {
		XTLS_RECORD_HANDSHAKE, 0x03, 0x03, 0x00, 0x04,
		XTLS_HANDSHAKE_CLIENT_HELLO, 0x00, 0x00, 0x00
	};
	static const uint8 Oversize[] = {
		XTLS_RECORD_HANDSHAKE, 0x03, 0x03, 0xFF, 0xFF
	};
	uint8 Data[XRT_TLS_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x243F6A88);

	testRequire(xrtTlsFuzzerTestOneInput(NULL, 0) == 0,
		"TLS empty fuzz seed failed");
	testRequire(xrtTlsFuzzerTestOneInput(
		Record, sizeof(Record)
	) == 0, "TLS record fuzz seed failed");
	testRequire(xrtTlsFuzzerTestOneInput(
		Oversize, sizeof(Oversize)
	) == 0, "TLS oversized record fuzz seed failed");
	for ( size_t iRound = 0; iRound < XRT_TLS_FUZZ_ROUNDS; iRound++ ) {
		size_t iSize = (size_t)(
			testTlsFuzzNext(&iState) %
			(XRT_TLS_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(testTlsFuzzNext(&iState) >> 24u);
		}
		testRequire(xrtTlsFuzzerTestOneInput(
			Data, iSize
		) == 0, "TLS deterministic fuzz round failed");
	}
	printf(
		"[PASS] TLS protocol fuzz (%u rounds)\n",
		(unsigned int)XRT_TLS_FUZZ_ROUNDS
	);
	return 0;
}
