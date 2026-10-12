#include "../test.h"

#include "../../fuzz/http_router.c"



#ifndef XRT_HTTP_ROUTER_FUZZ_ROUNDS
	#define XRT_HTTP_ROUTER_FUZZ_ROUNDS 3000u
#endif

#define XRT_HTTP_ROUTER_FUZZ_TEST_MAX 512u



/* 生成可重复的 Router 协议噪声。 */
static uint32 testHttpRouterFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 执行一个指定请求方法编号和原始路径的固定种子。 */
static void testHttpRouterFuzzSeed(uint8 iMethod, cstr sPath)
{
	uint8 Data[XRT_HTTP_ROUTER_FUZZ_TEST_MAX];
	size_t iPath = strlen(sPath);

	testRequire(
		(iPath + 1u) <= sizeof(Data),
		"HTTP router fixed fuzz seed is too large"
	);
	Data[0] = iMethod;
	memcpy(Data + 1u, sPath, iPath);
	testRequire(
		xrtHttpRouterFuzzerTestOneInput(
			Data, iPath + 1u
		) == 0,
		"HTTP router fixed fuzz seed failed"
	);
}



/* 先覆盖结构回溯种子，再运行固定随机种子的合法与任意字节路径。 */
int main(void)
{
	static const char Alphabet[] =
		"abcdefghijklmnopqrstuvwxyz0123456789/-._~%20";
	uint8 Data[XRT_HTTP_ROUTER_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x13198A2E);
	size_t iRound;

	testRequire(
		xrtHttpRouterFuzzerTestOneInput(NULL, 0) == 0,
		"HTTP router empty fuzz seed failed"
	);
	testHttpRouterFuzzSeed(0u, "/");
	testHttpRouterFuzzSeed(0u, "/users/me/detail");
	testHttpRouterFuzzSeed(1u, "/users/alice/detail");
	testHttpRouterFuzzSeed(2u, "/users/me/other");
	testHttpRouterFuzzSeed(3u, "/files/static");
	testHttpRouterFuzzSeed(2u, "/files/name");
	testHttpRouterFuzzSeed(6u, "/files/a/b%2Fc");
	testHttpRouterFuzzSeed(1u, "/users//last");
	testHttpRouterFuzzSeed(8u, "/fallback/a/b");
	testHttpRouterFuzzSeed(5u, "/not-found");
	testHttpRouterFuzzSeed(9u, "/users/42/detail");
	for ( iRound = 0;
		iRound < XRT_HTTP_ROUTER_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = 2u + (size_t)(
			testHttpRouterFuzzNext(&iState) %
			(XRT_HTTP_ROUTER_FUZZ_TEST_MAX - 1u)
		);
		size_t i;

		Data[0] = (uint8)testHttpRouterFuzzNext(&iState);
		Data[1] = '/';
		for ( i = 2u; i < iSize; i++ ) {
			Data[i] = (uint8)Alphabet[
				testHttpRouterFuzzNext(&iState) %
				(sizeof(Alphabet) - 1u)
			];
		}
		testRequire(
			xrtHttpRouterFuzzerTestOneInput(
				Data, iSize
			) == 0,
			"HTTP router valid deterministic fuzz round failed"
		);
		if ( (iRound % 4u) == 0 ) {
			for ( i = 1u; i < iSize; i++ ) {
				Data[i] = (uint8)(
					testHttpRouterFuzzNext(&iState) >> 24u
				);
			}
			testRequire(
				xrtHttpRouterFuzzerTestOneInput(
					Data, iSize
				) == 0,
				"HTTP router raw deterministic fuzz round failed"
			);
		}
	}
	printf(
		"[PASS] HTTP router differential fuzz (%u rounds)\n",
		(unsigned int)XRT_HTTP_ROUTER_FUZZ_ROUNDS
	);
	return 0;
}
