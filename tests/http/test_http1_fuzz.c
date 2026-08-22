#include "../test.h"

#include "../../fuzz/http1_protocol.c"



#ifndef XRT_HTTP1_FUZZ_ROUNDS
	#define XRT_HTTP1_FUZZ_ROUNDS 2000u
#endif

#define XRT_HTTP1_FUZZ_TEST_MAX 4096u



/* 生成可重复的协议噪声，保证普通测试环境持续执行 fuzz 入口。 */
static uint32 testHttp1FuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 先执行协议边界种子，再运行固定种子的变长随机输入。 */
int main(void)
{
	static const cstr Seeds[] = {
		"",
		"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n",
		"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello",
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
			"4\r\ntest\r\n0\r\nDigest: ok\r\n\r\n",
		"POST / HTTP/1.1\r\nContent-Length: 1\r\n"
			"Transfer-Encoding: chunked\r\n\r\n",
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n"
			"0\r\n\r\n",
		"HTTP/1.1 204\r\n\r\n",
		"GET / HTTP/1.1\nHost: example.test\n\n",
		"gzip; q=\"quoted\\value\", chunked",
		"gzip,",
		"100-continue, feature=\"a,b\"; mode=fast",
		"100-continue, feature =on",
		"POST / HTTP/1.1\r\nHost: example.test\r\n"
			"Expect: 100-continue\r\nExpect: ,100-CONTINUE,\r\n\r\n",
		"trailers, gzip; level=9; q=0.500",
		"trailers, gzip;q=2",
		"GET / HTTP/1.1\r\nHost: example.test\r\n"
			"TE: gzip; note=\"a,b\";q=0.5\r\n"
			"TE: trailers\r\nConnection: keep-alive, TE\r\n\r\n",
		"GET / HTTP/1.1\r\nHost: example.test\r\n"
			"TE: trailers\r\nConnection: keep-alive\r\n\r\n",
		"GET / HTTP/1.1\r\nHost: example.test\r\n"
			"TE: gzip;q=2\r\nConnection: TE\r\n\r\n"
	};
	uint8 Data[XRT_HTTP1_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x6A09E667);

	testRequire(
		xrtHttp1FuzzerTestOneInput(NULL, 0) == 0,
		"HTTP/1 empty fuzz seed failed"
	);
	for ( size_t i = 0; i < (sizeof(Seeds) / sizeof(Seeds[0])); i++ ) {
		testRequire(
			xrtHttp1FuzzerTestOneInput(
				(const uint8*)Seeds[i], strlen(Seeds[i])
			) == 0,
			"HTTP/1 fixed fuzz seed failed"
		);
	}
	for ( size_t iRound = 0;
		iRound < XRT_HTTP1_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = (size_t)(
			testHttp1FuzzNext(&iState) %
			(XRT_HTTP1_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(testHttp1FuzzNext(&iState) >> 24u);
		}
		testRequire(
			xrtHttp1FuzzerTestOneInput(Data, iSize) == 0,
			"HTTP/1 deterministic fuzz round failed"
		);
	}
	printf(
		"[PASS] HTTP/1 protocol fuzz (%u rounds)\n",
		(unsigned int)XRT_HTTP1_FUZZ_ROUNDS
	);
	return 0;
}
