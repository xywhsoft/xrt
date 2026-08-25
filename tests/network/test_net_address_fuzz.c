#include "../test.h"

#include "../../fuzz/net_address.c"



#ifndef XRT_NET_ADDRESS_FUZZ_ROUNDS
	#define XRT_NET_ADDRESS_FUZZ_ROUNDS 2000u
#endif

#define XRT_NET_ADDRESS_FUZZ_TEST_MAX 4096u



/* 生成可重复的网络地址文本与结构噪声。 */
static uint32 testNetAddressFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 执行规范地址种子和确定性随机输入，全程不发起外部查询。 */
int main(void)
{
	static const cstr Seeds[] = {
		"",
		"127.0.0.1",
		"192.0.2.1:443",
		"::1",
		"[2001:db8::1]:65535",
		"::ffff:192.0.2.1",
		"[fe80::1%3]:80",
		"01.2.3.4",
		"[::1",
		"2001:db8:::1"
	};
	uint8 Data[XRT_NET_ADDRESS_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x13198A2E);

	testRequire(xrtNetAddressFuzzerTestOneInput(NULL, 0) == 0,
		"network address empty fuzz seed failed");
	for ( size_t i = 0; i < (sizeof(Seeds) / sizeof(Seeds[0])); i++ ) {
		testRequire(xrtNetAddressFuzzerTestOneInput(
			(const uint8*)Seeds[i], strlen(Seeds[i])
		) == 0, "network address fixed fuzz seed failed");
	}
	for ( size_t iRound = 0;
		iRound < XRT_NET_ADDRESS_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = (size_t)(
			testNetAddressFuzzNext(&iState) %
			(XRT_NET_ADDRESS_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(
				testNetAddressFuzzNext(&iState) >> 24u
			);
		}
		testRequire(xrtNetAddressFuzzerTestOneInput(
			Data, iSize
		) == 0, "network address deterministic fuzz round failed");
	}
	printf(
		"[PASS] network address fuzz (%u rounds)\n",
		(unsigned int)XRT_NET_ADDRESS_FUZZ_ROUNDS
	);
	return 0;
}
