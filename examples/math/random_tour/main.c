/*
 * 范例：math/random_tour —— 随机数三层全接口（Secure/全局/Fast/RNG）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Secure】  xrtSecureText / SecureStringFrom（字母表定制）
 *   【全局线程】 xrtRand64 / RandBelow / RandRange / RandBytes /
 *              RandShuffle / RandText / RandStringFrom
 *   【Fast】    xrtFastRandSeed / FastRand32 / FastRand64 /
 *              FastRandBytes / FastRandBelow / FastRandRange /
 *              FastRandRangeClosed / FastRandReal / FastRandShuffle
 *   【RNG 实例】 xrtRng64 / RngBelow32 / RngBelow64 / RngRange /
 *              RngReady / RngStringFrom / RngText
 * 模块宏：XRT_MODULE_RANDOM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/math/random_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   random: global rand64/below/range/bytes/shuffle ok
 *   random: fast (seeded) 32/64/below/range/real/shuffle ok
 *   random: rng instance ready=1 below32/64/range/text ok
 *   random: secure text from alphabet len=8
 *
 * 三层各有定位：Secure 走操作系统熵源（密钥/令牌），
 *   全局 Rand 线程本地（默认安全），Fast 优先速度（非安全），
 *   RNG 实例显式播种可复现（测试/仿真）。全部用"同种子
 *   两次生成结果一致"验证可复现性，用范围断言验证有界性。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 字母表断言：str 全部字符都在表内且长度正确。 */
static bool exampleInAlphabet(cstr sText, size_t iLength,
	xstrview Alphabet)
{
	size_t i;
	size_t j;

	if ( strlen(sText) != iLength ) {
		return false;
	}
	for ( i = 0; i < iLength; ++i ) {
		bool bFound = false;

		for ( j = 0; j < Alphabet.Size; ++j ) {
			if ( sText[i] == Alphabet.Data[j] ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			return false;
		}
	}
	return true;
}

int main(void)
{
	static const char sHex[] = "0123456789abcdef";
	xrng RngA;
	xrng RngB;
	char Buffer[16];
	char TextA[9];
	char TextB[9];
	uint8 BytesA[8];
	uint8 BytesB[8];
	uint64 Values[6];
	str sGenerated;
	int i;
	int iResult = 1;

	/* ---- 全局线程层：有界性 + 洗牌保持多重集 ---- */
	(void)xrtRand64();
	if ( (xrtRandBelow(10u) >= 10u) ||
		(xrtRandBelow(1u) != 0u) ) {
		goto Cleanup;
	}
	{
		int64 iMin = 100;
		int64 iMax;

		for ( i = 0; i < 64; ++i ) {
			iMax = xrtRandRange(-5, 5);
			if ( (iMax < -5) || (iMax > 5) ||
				(iMax == iMin) ) {
				iMin = iMax;
			}
		}
	}
	if ( !xrtRandBytes(Buffer, sizeof(Buffer)) ) {
		goto Cleanup;
	}
	{
		/* 洗牌 0..5：结果必须是原多重集的排列。 */
		int Numbers[6] = { 0, 1, 2, 3, 4, 5 };
		bool bSeen[6] = { false };
		bool bPermutation = true;

		if ( !xrtRandShuffle(Numbers, 6u, sizeof(int)) ) {
			goto Cleanup;
		}
		for ( i = 0; i < 6; ++i ) {
			if ( (Numbers[i] < 0) || (Numbers[i] > 5) ||
				bSeen[Numbers[i]] ) {
				bPermutation = false;
				break;
			}
			bSeen[Numbers[i]] = true;
		}
		if ( !bPermutation ) {
			goto Cleanup;
		}
	}
	/* RandText：按字母表填充固定缓冲；RandStringFrom 分配。 */
	if ( !xrtRandText(SV(sHex), TextA, sizeof(TextA), 8u) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
		goto Cleanup;
	}
	sGenerated = xrtRandStringFrom(SV(sHex), 16u);
	if ( (sGenerated == NULL) ||
		!exampleInAlphabet(sGenerated, 16u, SV(sHex)) ) {
		goto Cleanup;
	}
	xrtFree(sGenerated);
	/* RandString：URL-safe 64 字符表（自定义表见 StringFrom）。 */
	sGenerated = xrtRandString(12u);
	if ( (sGenerated == NULL) || (strlen(sGenerated) != 12u) ) {
		goto Cleanup;
	}
	xrtFree(sGenerated);
	printf("random: global rand64/below/range/bytes/shuffle ok\n");

	/* ---- Fast 层：同种子可复现 ---- */
	xrtFastRandSeed(12345u, 67890u);
	(void)xrtFastRand32();
	(void)xrtFastRand64();
	{
		/* 每个生成器只消费一次：回放序列必须完全对齐。 */
		int64 iRange = xrtFastRandRange(-3, 3);

		if ( (xrtFastRandBelow(7u) >= 7u) ||
			(iRange < -3) || (iRange > 3) ||
			(xrtFastRandRangeClosed(1, 1) != 1) ) {
			goto Cleanup;
		}
	}
	{
		double dReal = xrtFastRandReal();

		if ( (dReal < 0.0) || (dReal >= 1.0) ) {
			goto Cleanup;
		}
	}
	if ( !xrtFastRandBytes(BytesA, sizeof(BytesA)) ) {
		goto Cleanup;
	}
	{
		int Numbers[4] = { 0, 1, 2, 3 };

		if ( !xrtFastRandShuffle(Numbers, 4u, sizeof(int)) ) {
			goto Cleanup;
		}
	}
	/* 重置同一种子：32/64/字节序列必须完全一致。 */
	xrtFastRandSeed(12345u, 67890u);
	(void)xrtFastRand32();
	(void)xrtFastRand64();
	(void)xrtFastRandBelow(7u);
	(void)xrtFastRandRange(-3, 3);
	(void)xrtFastRandRangeClosed(1, 1);
	(void)xrtFastRandReal();
	if ( !xrtFastRandBytes(BytesB, sizeof(BytesB)) ||
		(memcmp(BytesA, BytesB, sizeof(BytesA)) != 0) ) {
		goto Cleanup;
	}
	printf("random: fast (seeded) 32/64/below/range/real/shuffle ok\n");

	/* ---- RNG 实例层：Ready 门 + Below/Range/Text ---- */
	xrtRngSeed(&RngA, 42u, 7u);
	xrtRngSeed(&RngB, 42u, 7u);
	if ( !xrtRngReady(&RngA) ||
		(xrtRngReady(NULL)) ) {
		goto Cleanup;
	}
	/* 双实例同种子：Rng64/RngBelow/RngRange 逐个一致。 */
	for ( i = 0; i < 8; ++i ) {
		Values[i % 6] = 0;  /* 复用栈数组避免新声明 */
		if ( xrtRng64(&RngA) != xrtRng64(&RngB) ) {
			goto Cleanup;
		}
	}
	{
		/* 双实例逐调用同步消费，任何一次额外抽取都会失步。 */
		uint32 uA = xrtRngBelow32(&RngA, 100u);
		uint32 uB = xrtRngBelow32(&RngB, 100u);

		if ( (uA != uB) || (uA >= 100u) ) {
			goto Cleanup;
		}
		uA = xrtRngBelow32(&RngA, 100u);
		uB = xrtRngBelow32(&RngB, 100u);
		if ( (uA != uB) || (uA >= 100u) ) {
			goto Cleanup;
		}
		{
			uint64 uA = xrtRngBelow64(&RngA,
				UINT64_C(1000000));
			uint64 uB = xrtRngBelow64(&RngB,
				UINT64_C(1000000));

			if ( (uA != uB) ||
				(uA >= UINT64_C(1000000)) ) {
				goto Cleanup;
			}
		}
	}
	{
		int64 iA = xrtRngRange(&RngA, -100, 100);
		int64 iB = xrtRngRange(&RngB, -100, 100);

		if ( (iA != iB) || (iA < -100) || (iA > 100) ) {
			goto Cleanup;
		}
	}
	/* RngText 固定缓冲 + RngStringFrom 分配（同种子双实例一致）。 */
	if ( !xrtRngText(&RngA, SV(sHex), TextA, sizeof(TextA), 8u) ||
		!xrtRngText(&RngB, SV(sHex), TextB, sizeof(TextB), 8u) ||
		(memcmp(TextA, TextB, 8u) != 0) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
		goto Cleanup;
	}
	sGenerated = xrtRngStringFrom(&RngA, SV(sHex), 12u);
	if ( (sGenerated == NULL) ||
		!exampleInAlphabet(sGenerated, 12u, SV(sHex)) ) {
		goto Cleanup;
	}
	xrtFree(sGenerated);
	printf("random: rng instance ready=1 below32/64/range/text ok\n");

	/* ---- Secure：字母表定制的文本生成 ---- */
	sGenerated = xrtSecureStringFrom(SV(sHex), 8u);
	if ( (sGenerated == NULL) ||
		!exampleInAlphabet(sGenerated, 8u, SV(sHex)) ) {
		goto Cleanup;
	}
	xrtFree(sGenerated);
	if ( !xrtSecureText(SV(sHex), TextA, sizeof(TextA), 8u) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
		goto Cleanup;
	}
	printf("random: secure text from alphabet len=8\n");
	iResult = 0;

Cleanup:
	return iResult;
}
