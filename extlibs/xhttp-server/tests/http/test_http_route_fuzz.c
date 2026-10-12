#include "../test.h"

#include "../../fuzz/http_route.c"



#ifndef XRT_HTTP_ROUTE_FUZZ_ROUNDS
	#define XRT_HTTP_ROUTE_FUZZ_ROUNDS 3000u
#endif

#define XRT_HTTP_ROUTE_FUZZ_TEST_MAX 4096u



/* 生成可重复的路由协议噪声。 */
static uint32 testHttpRouteFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 把固定模板与路径编码为 fuzz 入口的长度前缀格式。 */
static void testHttpRouteFuzzSeed(cstr sPattern, cstr sPath)
{
	uint8 Data[512];
	size_t iPattern = strlen(sPattern);
	size_t iPath = strlen(sPath);

	testRequire(
		(iPattern <= UINT16_MAX) &&
		((iPattern + iPath + 2u) <= sizeof(Data)),
		"HTTP route fixed fuzz seed is too large"
	);
	Data[0] = (uint8)(iPattern >> 8u);
	Data[1] = (uint8)iPattern;
	memcpy(Data + 2u, sPattern, iPattern);
	memcpy(Data + 2u + iPattern, sPath, iPath);
	testRequire(
		xrtHttpRouteFuzzerTestOneInput(
			Data, iPattern + iPath + 2u
		) == 0,
		"HTTP route fixed fuzz seed failed"
	);
}



/* 先执行结构种子，再运行固定随机种子的变长任意字节输入。 */
int main(void)
{
	uint8 Data[XRT_HTTP_ROUTE_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x243F6A88);
	size_t iRound;

	testRequire(
		xrtHttpRouteFuzzerTestOneInput(NULL, 0) == 0,
		"HTTP route empty fuzz seed failed"
	);
	testHttpRouteFuzzSeed("/", "/");
	testHttpRouteFuzzSeed("/users/{id}", "/users/42");
	testHttpRouteFuzzSeed("/{tail...}", "/");
	testHttpRouteFuzzSeed("/files/{path...}", "/files/a/b%2Fc");
	testHttpRouteFuzzSeed("/{a}/{a}", "/one/two");
	testHttpRouteFuzzSeed("/{tail...}/bad", "/a/bad");
	testHttpRouteFuzzSeed("/%Q0", "/%Q0");
	for ( iRound = 0;
		iRound < XRT_HTTP_ROUTE_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = 2u + (size_t)(
			testHttpRouteFuzzNext(&iState) %
			(XRT_HTTP_ROUTE_FUZZ_TEST_MAX - 1u)
		);
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(
				testHttpRouteFuzzNext(&iState) >> 24u
			);
		}
		testRequire(
			xrtHttpRouteFuzzerTestOneInput(Data, iSize) == 0,
			"HTTP route deterministic fuzz round failed"
		);
	}
	printf(
		"[PASS] HTTP route protocol fuzz (%u rounds)\n",
		(unsigned int)XRT_HTTP_ROUTE_FUZZ_ROUNDS
	);
	return 0;
}
