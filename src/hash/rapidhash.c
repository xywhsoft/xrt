#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_HASH64)

/*
 * rapidhash v3.0, compact profile.
 * Copyright (C) 2024 Nicolas De Carli
 * Based on wyhash by Wang Yi.
 *
 * BSD 2-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Source: https://github.com/Nicoshev/rapidhash
 * XRT 保留旧版已经使用的 v3.0 输出，只移除了未公开的 Micro/Nano 变体。
 */

#if defined(_MSC_VER)
	#include <intrin.h>
#endif



#if defined(__GNUC__) || defined(__clang__)
	#define XRT_RAPID_INLINE static inline __attribute__((__always_inline__))
	#define XRT_RAPID_LIKELY(v) __builtin_expect((v), 1)
#elif defined(_MSC_VER)
	#define XRT_RAPID_INLINE static __forceinline
	#define XRT_RAPID_LIKELY(v) (v)
#else
	#define XRT_RAPID_INLINE static inline
	#define XRT_RAPID_LIKELY(v) (v)
#endif



/* 固定 secret 是确定性算法契约的一部分。 */
static const uint64 __xrtRapidSecret[8] = {
	UINT64_C(0x2D358DCCAA6C78A5),
	UINT64_C(0x8BB84B93962EACC9),
	UINT64_C(0x4B33A62ED433D4A3),
	UINT64_C(0x4D5A2DA51DE1AA47),
	UINT64_C(0xA0761D6478BD642F),
	UINT64_C(0xE7037ED1A0B428DB),
	UINT64_C(0x90ED1765281C388C),
	UINT64_C(0xAAAAAAAAAAAAAAAA)
};



/* 计算 64 x 64 的完整 128 位乘积。 */
XRT_RAPID_INLINE void __xrtRapidMultiply(uint64* pLow, uint64* pHigh)
{
#if defined(__SIZEOF_INT128__)
	__uint128_t iProduct = (__uint128_t)(*pLow) * (*pHigh);

	*pLow = (uint64)iProduct;
	*pHigh = (uint64)(iProduct >> 64);
#elif defined(_MSC_VER) && defined(_M_X64) && !defined(_M_ARM64EC)
	*pLow = _umul128(*pLow, *pHigh, pHigh);
#elif defined(_MSC_VER) && defined(_M_ARM64)
	uint64 iUpper = __umulh(*pLow, *pHigh);

	*pLow *= *pHigh;
	*pHigh = iUpper;
#else
	uint64 iHighA = *pLow >> 32;
	uint64 iHighB = *pHigh >> 32;
	uint64 iLowA = (uint32)(*pLow);
	uint64 iLowB = (uint32)(*pHigh);
	uint64 iHigh = iHighA * iHighB;
	uint64 iMiddle0 = iHighA * iLowB;
	uint64 iMiddle1 = iHighB * iLowA;
	uint64 iLow = iLowA * iLowB;
	uint64 iTemp = iLow + (iMiddle0 << 32);
	uint64 iCarry = iTemp < iLow;
	uint64 iResultLow = iTemp + (iMiddle1 << 32);

	iCarry += iResultLow < iTemp;
	*pLow = iResultLow;
	*pHigh = iHigh + (iMiddle0 >> 32) + (iMiddle1 >> 32) + iCarry;
#endif
}



/* 乘法混合后折叠高低半部。 */
XRT_RAPID_INLINE uint64 __xrtRapidMix(uint64 iLeft, uint64 iRight)
{
	__xrtRapidMultiply(&iLeft, &iRight);
	return iLeft ^ iRight;
}



/* 未对齐读取 64 位小端字。 */
XRT_RAPID_INLINE uint64 __xrtRapidRead64(const unsigned char* pData)
{
	return __xrtReadLe64(pData);
}



/* 未对齐读取 32 位小端字。 */
XRT_RAPID_INLINE uint64 __xrtRapidRead32(const unsigned char* pData)
{
	return __xrtReadLe32(pData);
}



/* 固定的 compact profile 与旧版 xrtHash64 输出一致。 */
static uint64 __xrtRapidHash64(const void* pData, size_t iSize, uint64 iSeed)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
	uint64 iLeft = 0;
	uint64 iRight = 0;
	size_t iRemain = iSize;

	iSeed ^= __xrtRapidMix(iSeed ^ __xrtRapidSecret[2], __xrtRapidSecret[1]);
	if ( XRT_RAPID_LIKELY(iSize <= 16u) ) {
		if ( iSize >= 4u ) {
			iSeed ^= iSize;
			if ( iSize >= 8u ) {
				iLeft = __xrtRapidRead64(pBytes);
				iRight = __xrtRapidRead64(pBytes + iSize - 8u);
			} else {
				iLeft = __xrtRapidRead32(pBytes);
				iRight = __xrtRapidRead32(pBytes + iSize - 4u);
			}
		} else if ( iSize != 0 ) {
			iLeft = ((uint64)pBytes[0] << 45) | pBytes[iSize - 1u];
			iRight = pBytes[iSize >> 1];
		}
	} else {
		uint64 iSee1 = iSeed;
		uint64 iSee2 = iSeed;
		uint64 iSee3 = iSeed;
		uint64 iSee4 = iSeed;
		uint64 iSee5 = iSeed;
		uint64 iSee6 = iSeed;

		if ( iRemain > 112u ) {
			do {
				iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes) ^ __xrtRapidSecret[0],
					__xrtRapidRead64(pBytes + 8) ^ iSeed);
				iSee1 = __xrtRapidMix(__xrtRapidRead64(pBytes + 16) ^ __xrtRapidSecret[1],
					__xrtRapidRead64(pBytes + 24) ^ iSee1);
				iSee2 = __xrtRapidMix(__xrtRapidRead64(pBytes + 32) ^ __xrtRapidSecret[2],
					__xrtRapidRead64(pBytes + 40) ^ iSee2);
				iSee3 = __xrtRapidMix(__xrtRapidRead64(pBytes + 48) ^ __xrtRapidSecret[3],
					__xrtRapidRead64(pBytes + 56) ^ iSee3);
				iSee4 = __xrtRapidMix(__xrtRapidRead64(pBytes + 64) ^ __xrtRapidSecret[4],
					__xrtRapidRead64(pBytes + 72) ^ iSee4);
				iSee5 = __xrtRapidMix(__xrtRapidRead64(pBytes + 80) ^ __xrtRapidSecret[5],
					__xrtRapidRead64(pBytes + 88) ^ iSee5);
				iSee6 = __xrtRapidMix(__xrtRapidRead64(pBytes + 96) ^ __xrtRapidSecret[6],
					__xrtRapidRead64(pBytes + 104) ^ iSee6);
				pBytes += 112;
				iRemain -= 112;
			} while ( iRemain > 112u );

			iSeed ^= iSee1;
			iSee2 ^= iSee3;
			iSee4 ^= iSee5;
			iSeed ^= iSee6;
			iSee2 ^= iSee4;
			iSeed ^= iSee2;
		}

		if ( iRemain > 16u ) {
			iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes) ^ __xrtRapidSecret[2],
				__xrtRapidRead64(pBytes + 8) ^ iSeed);
			if ( iRemain > 32u ) {
				iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes + 16) ^ __xrtRapidSecret[2],
					__xrtRapidRead64(pBytes + 24) ^ iSeed);
				if ( iRemain > 48u ) {
					iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes + 32) ^ __xrtRapidSecret[1],
						__xrtRapidRead64(pBytes + 40) ^ iSeed);
					if ( iRemain > 64u ) {
						iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes + 48) ^ __xrtRapidSecret[1],
							__xrtRapidRead64(pBytes + 56) ^ iSeed);
						if ( iRemain > 80u ) {
							iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes + 64) ^ __xrtRapidSecret[2],
								__xrtRapidRead64(pBytes + 72) ^ iSeed);
							if ( iRemain > 96u ) {
								iSeed = __xrtRapidMix(__xrtRapidRead64(pBytes + 80) ^ __xrtRapidSecret[1],
									__xrtRapidRead64(pBytes + 88) ^ iSeed);
							}
						}
					}
				}
			}
		}
		iLeft = __xrtRapidRead64(pBytes + iRemain - 16u) ^ iRemain;
		iRight = __xrtRapidRead64(pBytes + iRemain - 8u);
	}

	iLeft ^= __xrtRapidSecret[1];
	iRight ^= iSeed;
	__xrtRapidMultiply(&iLeft, &iRight);
	return __xrtRapidMix(iLeft ^ __xrtRapidSecret[7],
		iRight ^ __xrtRapidSecret[1] ^ iRemain);
}



/* 计算使用明确 seed 的确定性 64 位哈希。 */
XRT_API uint64 xrtHash64Seed(const void* pData, size_t iSize, uint64 iSeed)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	return __xrtRapidHash64(pData, iSize, iSeed);
}



/* 默认 seed 固定为零，使旧数据和跨进程结果继续稳定。 */
XRT_API uint64 xrtHash64(const void* pData, size_t iSize)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	return __xrtRapidHash64(pData, iSize, 0);
}



#undef XRT_RAPID_INLINE
#undef XRT_RAPID_LIKELY

#endif
