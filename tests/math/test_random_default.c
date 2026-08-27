#include "../test.h"
#include "../test_thread.h"



#define TEST_RANDOM_THREAD_COUNT 4
#define TEST_RANDOM_VALUE_COUNT 128



/* 每个线程独立播种并保存同一条预期序列。 */
typedef struct test_random_context {
	uint32 Values[TEST_RANDOM_VALUE_COUNT];
} test_random_context;



/* 验证便捷状态不要求线程先附着到 XRT 运行时。 */
static int testRandomThreadRun(ptr pData)
{
	test_random_context* pContext = (test_random_context*)pData;

	xrtRandSeed(12345, 9);
	for ( int i = 0; i < TEST_RANDOM_VALUE_COUNT; i++ ) {
		pContext->Values[i] = xrtRand32();
	}
	return 0;
}



/* 便捷接口必须只是当前线程显式状态的薄组合层。 */
static void testConvenienceMatchesPrimitive(void)
{
	xrng Rng;
	uint8 arrExplicit[31];
	uint8 arrDefault[31];
	uint32 arrExplicitOrder[16];
	uint32 arrDefaultOrder[16];

	xrtRngSeed(&Rng, 42, 54);
	xrtRandSeed(42, 54);
	for ( int i = 0; i < 128; i++ ) {
		testRequire(xrtRand32() == xrtRng32(&Rng),
			"thread random diverged from explicit RNG");
	}
	xrtFastRandSeed(42, 54);
	xrtRngSeed(&Rng, 42, 54);
	for ( int i = 0; i < 128; i++ ) {
		testRequire(xrtFastRand32() == xrtRng32(&Rng),
			"fast thread random alias diverged from explicit RNG");
	}

	xrtRngSeed(&Rng, 88, 7);
	xrtRandSeed(88, 7);
	for ( int i = 0; i < 128; i++ ) {
		testRequire(xrtRandRange(-1000, 1000) == xrtRngRange(&Rng, -1000, 1000),
			"thread range diverged from explicit RNG");
	}

	xrtRngSeed(&Rng, 2026, 31);
	xrtRandSeed(2026, 31);
	testRequire(xrtRngBytes(&Rng, arrExplicit, sizeof(arrExplicit)) &&
		xrtRandBytes(arrDefault, sizeof(arrDefault)),
		"thread random byte fill failed");
	testRequire(memcmp(arrExplicit, arrDefault, sizeof(arrExplicit)) == 0,
		"thread random bytes diverged from explicit RNG");

	for ( size_t i = 0; i < 16; i++ ) {
		arrExplicitOrder[i] = (uint32)i;
		arrDefaultOrder[i] = (uint32)i;
	}
	xrtRngSeed(&Rng, 90, 13);
	xrtRandSeed(90, 13);
	testRequire(xrtRngShuffle(&Rng, arrExplicitOrder, 16,
		sizeof(arrExplicitOrder[0])) && xrtRandShuffle(arrDefaultOrder, 16,
		sizeof(arrDefaultOrder[0])), "thread random shuffle failed");
	testRequire(memcmp(arrExplicitOrder, arrDefaultOrder,
		sizeof(arrExplicitOrder)) == 0,
		"thread random shuffle diverged from explicit RNG");
}



/* 多个原生线程的便捷状态必须互不竞争且互不污染。 */
static void testThreadIsolation(void)
{
	test_random_context arrContext[TEST_RANDOM_THREAD_COUNT];
	testthread arrThread[TEST_RANDOM_THREAD_COUNT];

	memset(arrContext, 0, sizeof(arrContext));
	for ( int i = 0; i < TEST_RANDOM_THREAD_COUNT; i++ ) {
		arrThread[i].Proc = testRandomThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_RANDOM_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_RANDOM_THREAD_COUNT);
	for ( int i = 0; i < TEST_RANDOM_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "random worker failed");
		testRequire(memcmp(arrContext[0].Values, arrContext[i].Values,
			sizeof(arrContext[i].Values)) == 0,
			"thread-local random sequences polluted each other");
	}
}



/* 自动播种路径和全部便捷返回范围必须可直接使用。 */
static void testConvenienceBounds(void)
{
	for ( int i = 0; i < 10000; i++ ) {
		uint64 iBelow = xrtRandBelow(17);
		int64 iRange = xrtRandRange(-5, 8);
		int64 iClosed = xrtRandRangeClosed(1, 6);
		double fReal = xrtRandReal();

		testRequire(iBelow < 17, "thread bounded RNG escaped its range");
		testRequire((iRange >= -5) && (iRange < 8),
			"thread half-open range escaped its bounds");
		testRequire((iClosed >= 1) && (iClosed <= 6),
			"thread closed range escaped its bounds");
		testRequire((fReal >= 0.0) && (fReal < 1.0),
			"thread unit real escaped [0, 1)");
	}
}



/* 执行便捷层一致性、线程隔离和范围测试。 */
int main(void)
{
	testConvenienceMatchesPrimitive();
	testThreadIsolation();
	testConvenienceBounds();
	return 0;
}
