#include "../internal/xrt_random.h"



#if defined(XRT_FEATURE_RANDOM)

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#define XRT_RNG_GUARD UINT32_C(0x524E4731)
#define XRT_RNG_MULTIPLIER UINT64_C(6364136223846793005)



/* 不检查状态地执行一次 PCG XSH RR 变换。 */
static uint32 __xrtRngNext32(xrng* pRng)
{
	uint64 iOldState = pRng->State;
	uint32 iShifted;
	uint32 iRotate;

	pRng->State = (iOldState * XRT_RNG_MULTIPLIER) + pRng->Increment;
	iShifted = (uint32)(((iOldState >> 18u) ^ iOldState) >> 27u);
	iRotate = (uint32)(iOldState >> 59u);
	return (iShifted >> iRotate) | (iShifted << ((0u - iRotate) & 31u));
}



/* 不检查状态地连续取两个 32 位字并组合为 64 位字。 */
static uint64 __xrtRngNext64(xrng* pRng)
{
	uint64 iLow = __xrtRngNext32(pRng);
	uint64 iHigh = __xrtRngNext32(pRng);

	return (iHigh << 32) | iLow;
}



/* 验证状态标记和 PCG 奇数增量约束。 */
static bool __xrtRngValidate(const xrng* pRng)
{
	if ( pRng == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtRngReady(pRng) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 判断显式随机状态是否已初始化且内部约束自洽。 */
XRT_API bool xrtRngReady(const xrng* pRng)
{
	return (pRng != NULL) && (pRng->Guard == XRT_RNG_GUARD) &&
		((pRng->Increment & 1u) != 0);
}



/* 对已经验证的状态执行 32 位拒绝采样，供同模块组合层复用。 */
uint32 __xrtRngBelow32Ready(xrng* pRng, uint32 iBound)
{
	uint32 iThreshold = (0u - iBound) % iBound;

	for ( ;; ) {
		uint32 iValue = __xrtRngNext32(pRng);

		if ( iValue >= iThreshold ) {
			return iValue % iBound;
		}
	}
}



/* 不检查参数地执行 64 位拒绝采样。 */
static uint64 __xrtRngBelow64(xrng* pRng, uint64 iBound)
{
	uint64 iThreshold = (UINT64_C(0) - iBound) % iBound;

	for ( ;; ) {
		uint64 iValue = __xrtRngNext64(pRng);

		if ( iValue >= iThreshold ) {
			return iValue % iBound;
		}
	}
}



/* 根据区间宽度选择成本最低的无偏采样路径。 */
static uint64 __xrtRngOffset(xrng* pRng, uint64 iRange)
{
	if ( iRange <= UINT32_MAX ) {
		return __xrtRngBelow32Ready(pRng, (uint32)iRange);
	}
	if ( iRange == (UINT64_C(1) << 32) ) {
		return __xrtRngNext32(pRng);
	}
	return __xrtRngBelow64(pRng, iRange);
}



/* 把 int64 的二进制位模式无实现定义地还原为有符号值。 */
static int64 __xrtRngSigned(uint64 iBits)
{
	if ( iBits <= INT64_MAX ) {
		return (int64)iBits;
	}
	return -1 - (int64)(UINT64_MAX - iBits);
}



/* 使用 PCG 推荐的两步过程初始化状态。 */
XRT_API void xrtRngSeed(xrng* pRng, uint64 iSeed, uint64 iStream)
{
	if ( pRng == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}

	pRng->State = 0;
	pRng->Increment = (iStream << 1u) | 1u;
	pRng->Guard = XRT_RNG_GUARD;
	pRng->Reserved = 0;
	(void)__xrtRngNext32(pRng);
	pRng->State += iSeed;
	(void)__xrtRngNext32(pRng);
}



/* 验证后生成一个 32 位字。 */
XRT_API uint32 xrtRng32(xrng* pRng)
{
	return __xrtRngValidate(pRng) ? __xrtRngNext32(pRng) : 0;
}



/* 验证后从同一条 PCG 序列组合一个 64 位字。 */
XRT_API uint64 xrtRng64(xrng* pRng)
{
	return __xrtRngValidate(pRng) ? __xrtRngNext64(pRng) : 0;
}



/* 按固定小端顺序展开 PCG32 字，保证跨平台字节序列一致。 */
XRT_API bool xrtRngBytes(xrng* pRng, ptr pData, size_t iSize)
{
	uint8* pWrite = (uint8*)pData;
	size_t iOffset = 0;

	if ( !__xrtRngValidate(pRng) ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pData == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(pData, iSize, pRng, sizeof(*pRng)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	while ( iOffset < iSize ) {
		uint32 iValue = __xrtRngNext32(pRng);
		size_t iRemain = iSize - iOffset;
		size_t iChunk = iRemain < 4u ? iRemain : 4u;

		for ( size_t i = 0; i < iChunk; i++ ) {
			pWrite[iOffset + i] = (uint8)(iValue >> (i * 8u));
		}
		iOffset += iChunk;
	}
	return true;
}



/* 生成无模偏差的 32 位有界值。 */
XRT_API uint32 xrtRngBelow32(xrng* pRng, uint32 iBound)
{
	if ( !__xrtRngValidate(pRng) ) {
		return 0;
	}
	if ( iBound == 0 ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return __xrtRngBelow32Ready(pRng, iBound);
}



/* 生成无模偏差的 64 位有界值。 */
XRT_API uint64 xrtRngBelow64(xrng* pRng, uint64 iBound)
{
	if ( !__xrtRngValidate(pRng) ) {
		return 0;
	}
	if ( iBound == 0 ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return __xrtRngBelow64(pRng, iBound);
}



/* 在完整 int64 半开区间中生成无偏值。 */
XRT_API int64 xrtRngRange(xrng* pRng, int64 iMin, int64 iMax)
{
	uint64 iRange;
	uint64 iOffset;

	if ( !__xrtRngValidate(pRng) ) {
		return 0;
	}
	if ( iMin >= iMax ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	iRange = (uint64)iMax - (uint64)iMin;
	iOffset = __xrtRngOffset(pRng, iRange);
	return __xrtRngSigned((uint64)iMin + iOffset);
}



/* 在完整 int64 闭区间中生成无偏值。 */
XRT_API int64 xrtRngRangeClosed(xrng* pRng, int64 iMin, int64 iMax)
{
	uint64 iRange;
	uint64 iOffset;

	if ( !__xrtRngValidate(pRng) ) {
		return 0;
	}
	if ( iMin > iMax ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	iRange = ((uint64)iMax - (uint64)iMin) + 1u;
	iOffset = iRange == 0 ? __xrtRngNext64(pRng) : __xrtRngOffset(pRng, iRange);
	return __xrtRngSigned((uint64)iMin + iOffset);
}



/* 从随机 64 位字的高 53 位构造均匀双精度数。 */
XRT_API double xrtRngReal(xrng* pRng)
{
	if ( !__xrtRngValidate(pRng) ) {
		return 0.0;
	}
	return (double)(__xrtRngNext64(pRng) >> 11u) *
		(1.0 / 9007199254740992.0);
}



/* 验证完整数组后执行零分配 Fisher-Yates 洗牌。 */
XRT_API bool xrtRngShuffle(xrng* pRng,
	ptr pData, size_t iCount, size_t iItemSize)
{
	uint8* pBytes = (uint8*)pData;
	size_t iDataSize;

	if ( !__xrtRngValidate(pRng) ) {
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if ( (pData == NULL) || (iItemSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCount > (SIZE_MAX / iItemSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iDataSize = iCount * iItemSize;
	if ( __xrtRangesOverlap(pData, iDataSize, pRng, sizeof(*pRng)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	for ( size_t i = iCount; i > 1u; i-- ) {
		size_t iSwap = (size_t)__xrtRngOffset(pRng, (uint64)i);
		uint8* pLeft = pBytes + ((i - 1u) * iItemSize);
		uint8* pRight = pBytes + (iSwap * iItemSize);

		if ( pLeft == pRight ) {
			continue;
		}
		for ( size_t j = 0; j < iItemSize; j++ ) {
			uint8 iByte = pLeft[j];

			pLeft[j] = pRight[j];
			pRight[j] = iByte;
		}
	}
	return true;
}



#undef XRT_RNG_GUARD
#undef XRT_RNG_MULTIPLIER

#endif
