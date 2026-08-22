#include "../test.h"

#include <limits.h>



/* PCG 官方向量和旧版 PCG32 路径必须逐值保持。 */
static void testPcgVectors(void)
{
	static const uint32 arrExpected[] = {
		UINT32_C(0xA15C02B7), UINT32_C(0x7B47F409),
		UINT32_C(0xBA1D3330), UINT32_C(0x83D2F293),
		UINT32_C(0xBFA4784B), UINT32_C(0xCBED606E)
	};
	static const uint32 arrLegacy[] = {
		UINT32_C(2280515124), UINT32_C(875822104), UINT32_C(2165132003),
		UINT32_C(3444695176), UINT32_C(1217744654)
	};
	xrng Rng;

	xrtRngSeed(&Rng, 42, 54);
	for ( size_t i = 0; i < (sizeof(arrExpected) / sizeof(arrExpected[0])); i++ ) {
		testRequire(xrtRng32(&Rng) == arrExpected[i], "PCG32 vector changed");
	}

	xrtRngSeed(&Rng, 42, 54);
	testRequire(xrtRng64(&Rng) == UINT64_C(0x7B47F409A15C02B7),
		"PCG64 composition changed");

	xrtRngSeed(&Rng, 12345, 1);
	for ( size_t i = 0; i < (sizeof(arrLegacy) / sizeof(arrLegacy[0])); i++ ) {
		testRequire(xrtRng32(&Rng) == arrLegacy[i],
			"legacy PCG32 vector changed");
	}
}



/* 静态初始化与显式播种都必须产生稳定、可复制状态。 */
static void testStateCopy(void)
{
	xrng Left = XRT_RNG_INITIALIZER;
	xrng Right = Left;

	for ( int i = 0; i < 32; i++ ) {
		testRequire(xrtRng32(&Left) == xrtRng32(&Right),
			"copied RNG states diverged");
	}

	xrtRngSeed(&Left, UINT64_C(0x123456789ABCDEF0), 7);
	Right = Left;
	for ( int i = 0; i < 32; i++ ) {
		testRequire(xrtRng64(&Left) == xrtRng64(&Right),
			"seeded RNG states diverged");
	}

	xrtRngSeed(&Left, 99, 5);
	xrtRngSeed(&Right, 99, UINT64_C(0x8000000000000005));
	testRequire(memcmp(&Left, &Right, sizeof(Left)) == 0,
		"PCG stream high-bit contract changed");
}



/* 字节填充必须冻结小端布局，并与消费同样数量的 PCG32 字一致。 */
static void testBytes(void)
{
	static const uint8 arrExpected[] = {
		0xB7u, 0x02u, 0x5Cu, 0xA1u,
		0x09u, 0xF4u, 0x47u, 0x7Bu,
		0x30u
	};
	xrng Bytes;
	xrng Words;
	uint8 arrOutput[sizeof(arrExpected)];

	xrtRngSeed(&Bytes, 42, 54);
	Words = Bytes;
	testRequire(xrtRngBytes(&Bytes, arrOutput, sizeof(arrOutput)),
		"random byte fill failed");
	testRequire(memcmp(arrOutput, arrExpected, sizeof(arrOutput)) == 0,
		"random byte layout changed");
	(void)xrtRng32(&Words);
	(void)xrtRng32(&Words);
	(void)xrtRng32(&Words);
	testRequire(memcmp(&Bytes, &Words, sizeof(Bytes)) == 0,
		"random byte fill consumed the wrong state");

	Words = Bytes;
	testRequire(xrtRngBytes(&Bytes, NULL, 0),
		"empty random byte fill failed");
	testRequire(memcmp(&Bytes, &Words, sizeof(Bytes)) == 0,
		"empty random byte fill advanced state");
}



/* 有界采样覆盖 32/64 位拒绝路径、幂宽度和单值边界。 */
static void testBounds(void)
{
	xrng Rng;

	xrtRngSeed(&Rng, 2026, 17);
	for ( int i = 0; i < 100000; i++ ) {
		uint32 iSmall = xrtRngBelow32(&Rng, 10);
		uint64 iLarge = xrtRngBelow64(&Rng, UINT64_C(0x100000001));

		testRequire(iSmall < 10, "bounded 32-bit RNG escaped its range");
		testRequire(iLarge < UINT64_C(0x100000001),
			"bounded 64-bit RNG escaped its range");
	}
	testRequire(xrtRngBelow32(&Rng, 1) == 0, "bound one did not return zero");
	testRequire(xrtRngBelow64(&Rng, 1) == 0, "64-bit bound one did not return zero");
}



/* 半开与闭区间必须在极值附近仍保持完整 int64 语义。 */
static void testRanges(void)
{
	xrng Rng;
	bool bSawNegative = false;
	bool bSawPositive = false;

	xrtRngSeed(&Rng, 98765, 3);
	for ( int i = 0; i < 100000; i++ ) {
		int64 iHalfOpen = xrtRngRange(&Rng, -9, 13);
		int64 iClosed = xrtRngRangeClosed(&Rng, INT32_MIN, INT32_MAX);
		int64 iFull = xrtRngRangeClosed(&Rng, INT64_MIN, INT64_MAX);

		testRequire((iHalfOpen >= -9) && (iHalfOpen < 13),
			"half-open range escaped its bounds");
		testRequire((iClosed >= INT32_MIN) && (iClosed <= INT32_MAX),
			"closed 32-bit domain escaped its bounds");
		bSawNegative = bSawNegative || (iFull < 0);
		bSawPositive = bSawPositive || (iFull >= 0);
	}
	testRequire(bSawNegative && bSawPositive,
		"full int64 range did not cover both signs");
	testRequire(xrtRngRangeClosed(&Rng, INT64_MIN, INT64_MIN) == INT64_MIN,
		"single-value closed range changed its value");
}



/* 洗牌必须可复现、保留完整排列并正确交换多字节元素。 */
static void testShuffle(void)
{
	uint32 arrLeft[32];
	uint32 arrRight[32];
	bool arrSeen[32] = { false };
	xrng Left;
	xrng Right;
	bool bChanged = false;

	for ( size_t i = 0; i < 32; i++ ) {
		arrLeft[i] = (uint32)i;
		arrRight[i] = (uint32)i;
	}
	xrtRngSeed(&Left, 1234, 19);
	Right = Left;
	testRequire(xrtRngShuffle(&Left, arrLeft, 32, sizeof(arrLeft[0])),
		"first random shuffle failed");
	testRequire(xrtRngShuffle(&Right, arrRight, 32, sizeof(arrRight[0])),
		"second random shuffle failed");
	testRequire(memcmp(arrLeft, arrRight, sizeof(arrLeft)) == 0,
		"same RNG state did not reproduce shuffle");

	for ( size_t i = 0; i < 32; i++ ) {
		testRequire(arrLeft[i] < 32, "shuffle produced an unknown element");
		testRequire(!arrSeen[arrLeft[i]], "shuffle duplicated an element");
		arrSeen[arrLeft[i]] = true;
		bChanged = bChanged || (arrLeft[i] != (uint32)i);
	}
	testRequire(bChanged, "shuffle did not change the tested sequence");
}



/* 无效参数和损坏状态必须报错，并且不得推进状态。 */
static void testFailuresAreAtomic(void)
{
	xrng Rng;
	xrng Before;
	uint8 iByte = 0xA5u;

	xrtRngSeed(&Rng, 7, 11);
	Before = Rng;
	xrtClearError();
	testRequire(xrtRngBelow32(&Rng, 0) == 0, "zero bound did not fail");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"zero bound advanced RNG state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"zero bound reported the wrong error");

	xrtClearError();
	testRequire(xrtRngRange(&Rng, 5, 5) == 0, "empty range did not fail");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"empty range advanced RNG state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"empty range reported the wrong error");

	xrtClearError();
	testRequire(!xrtRngBytes(&Rng, NULL, 1),
		"random bytes accepted a null output");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"invalid random byte fill advanced state");

	xrtClearError();
	testRequire(!xrtRngBytes(&Rng, &Rng, sizeof(Rng)),
		"random bytes accepted state/output overlap");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"overlapping random byte fill changed state");

	xrtClearError();
	testRequire(!xrtRngShuffle(&Rng, NULL, 1, 1),
		"shuffle accepted a null array");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"null shuffle advanced state");

	xrtClearError();
	testRequire(!xrtRngShuffle(&Rng, &iByte, 1, 0),
		"shuffle accepted zero-sized elements");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"zero-sized shuffle advanced state");

	xrtClearError();
	testRequire(!xrtRngShuffle(&Rng, &iByte, SIZE_MAX, 2),
		"shuffle accepted an overflowing array");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"overflowing shuffle advanced state");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"overflowing shuffle reported the wrong error");

	xrtClearError();
	testRequire(!xrtRngShuffle(&Rng, &Rng, sizeof(Rng), 1),
		"shuffle accepted state/array overlap");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"overlapping shuffle changed state");

	xrtClearError();
	Rng.Guard = 0;
	Before = Rng;
	testRequire(xrtRng32(&Rng) == 0, "damaged RNG state did not fail");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"damaged RNG state was modified");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"damaged RNG state reported the wrong error");

	xrtClearError();
	xrtRngSeed(NULL, 0, 0);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null RNG state reported the wrong error");
}



/* 单位实数必须可复现并严格位于半开区间。 */
static void testReal(void)
{
	xrng Left;
	xrng Right;

	xrtRngSeed(&Left, 314159, 271828);
	Right = Left;
	for ( int i = 0; i < 100000; i++ ) {
		double fLeft = xrtRngReal(&Left);
		double fRight = xrtRngReal(&Right);

		testRequire(fLeft == fRight, "unit real was not reproducible");
		testRequire((fLeft >= 0.0) && (fLeft < 1.0),
			"unit real escaped [0, 1)");
	}
}



/* 执行随机原语的向量、边界、极值和错误测试。 */
int main(void)
{
	testPcgVectors();
	testStateCopy();
	testBytes();
	testBounds();
	testRanges();
	testShuffle();
	testFailuresAreAtomic();
	testReal();
	return 0;
}
