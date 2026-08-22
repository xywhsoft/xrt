#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_HASH32)

/*
 * nmhash32x v2.0
 * Copyright (c) 2021, James Z.M. Gao
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
 * Source: https://github.com/rurban/smhasher
 * XRT 保留旧版裁剪后的 nmhash32x，并修复标量轮次的严格别名访问。
 */

#if defined(__GNUC__) && defined(__AVX2__)
	#include <immintrin.h>
#elif defined(__GNUC__) && defined(__SSE2__)
	#include <emmintrin.h>
#elif defined(_MSC_VER)
	#include <intrin.h>
#endif



#if defined(__GNUC__) || defined(__clang__)
	#define XRT_NMH_LIKELY(v) __builtin_expect((v), 1)
#else
	#define XRT_NMH_LIKELY(v) (v)
#endif

#if defined(__has_builtin)
	#if __has_builtin(__builtin_rotateleft32)
		#define XRT_NMH_ROTATE(v, b) __builtin_rotateleft32((v), (b))
	#endif
#endif

#if !defined(XRT_NMH_ROTATE)
	#if defined(_MSC_VER)
		#define XRT_NMH_ROTATE(v, b) _rotl((v), (b))
	#else
		#define XRT_NMH_ROTATE(v, b) (((v) << (b)) | ((v) >> (32u - (b))))
	#endif
#endif



#if defined(__TINYC__)
	#define XRT_NMH_RESTRICT restrict
#elif defined(__cplusplus) && (defined(__GNUC__) || defined(__clang__))
	#define XRT_NMH_RESTRICT __restrict__
#elif defined(__cplusplus) && defined(_MSC_VER)
	#define XRT_NMH_RESTRICT __restrict
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
	#define XRT_NMH_RESTRICT restrict
#else
	#define XRT_NMH_RESTRICT
#endif



#define XRT_NMH_SCALAR 0
#define XRT_NMH_SSE2 1
#define XRT_NMH_AVX2 2
#define XRT_NMH_AVX512 3

#if defined(__TINYC__)
	#define XRT_NMH_VECTOR XRT_NMH_SCALAR
#elif defined(__AVX512BW__)
	#define XRT_NMH_VECTOR XRT_NMH_AVX512
#elif defined(__AVX2__)
	#define XRT_NMH_VECTOR XRT_NMH_AVX2
#elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || \
	(defined(_M_IX86_FP) && (_M_IX86_FP == 2))
	#define XRT_NMH_VECTOR XRT_NMH_SSE2
#else
	#define XRT_NMH_VECTOR XRT_NMH_SCALAR
#endif



#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
	#include <stdalign.h>
	#define XRT_NMH_ALIGN(n) alignas(n)
#elif defined(__GNUC__) || defined(__clang__)
	#define XRT_NMH_ALIGN(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
	#define XRT_NMH_ALIGN(n) __declspec(align(n))
#else
	#define XRT_NMH_ALIGN(n)
#endif

#if XRT_NMH_VECTOR > XRT_NMH_SCALAR
	#define XRT_NMH_ACC_ALIGN 64
#elif defined(__BIGGEST_ALIGNMENT__)
	#define XRT_NMH_ACC_ALIGN __BIGGEST_ALIGNMENT__
#else
	#define XRT_NMH_ACC_ALIGN 16
#endif



#define XRT_NMH_PRIME1 UINT32_C(0x9E3779B1)
#define XRT_NMH_PRIME2 UINT32_C(0x85EBCA77)
#define XRT_NMH_PRIME3 UINT32_C(0xC2B2AE3D)
#define XRT_NMH_PRIME4 UINT32_C(0x27D4EB2F)
#define XRT_NMH_M1 UINT32_C(0xF0D9649B)
#define XRT_NMH_M2 UINT32_C(0x29A7935D)
#define XRT_NMH_M3 UINT32_C(0x55D35831)



/* FARSH 的固定初始向量是 nmhash32x 输出契约的一部分。 */
XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) static const uint32 __xrtNmhInitial[32] = {
	UINT32_C(0xB8FE6C39), UINT32_C(0x23A44BBE), UINT32_C(0x7C01812C), UINT32_C(0xF721AD1C),
	UINT32_C(0xDED46DE9), UINT32_C(0x839097DB), UINT32_C(0x7240A4A4), UINT32_C(0xB7B3671F),
	UINT32_C(0xCB79E64E), UINT32_C(0xCCC0E578), UINT32_C(0x825AD07D), UINT32_C(0xCCFF7221),
	UINT32_C(0xB8084674), UINT32_C(0xF743248E), UINT32_C(0xE03590E6), UINT32_C(0x813A264C),
	UINT32_C(0x3C2852BB), UINT32_C(0x91C300CB), UINT32_C(0x88D0658B), UINT32_C(0x1B532EA3),
	UINT32_C(0x71644897), UINT32_C(0xA20DF94E), UINT32_C(0x3819EF46), UINT32_C(0xA9DEACD8),
	UINT32_C(0xA8FA763F), UINT32_C(0xE39C343F), UINT32_C(0xF9DCBBC7), UINT32_C(0xC70B4F1D),
	UINT32_C(0x8A51E04B), UINT32_C(0xCDB45931), UINT32_C(0xC89F7EC9), UINT32_C(0xD9787364)
};



/* 未对齐读取 32 位小端字。 */
static inline uint32 __xrtNmhRead32(const void* pData)
{
	return __xrtReadLe32(pData);
}



/* 未对齐读取 16 位小端字。 */
static inline uint16 __xrtNmhRead16(const void* pData)
{
	return __xrtReadLe16(pData);
}



#if XRT_NMH_VECTOR == XRT_NMH_SCALAR

/* 分别乘两个 16 位 lane，避免通过 uint16 指针破坏严格别名规则。 */
static inline uint32 __xrtNmhMultiply16(uint32 iValue, uint32 iFactor)
{
	uint32 iLow = ((iValue & UINT32_C(0xFFFF)) *
		(iFactor & UINT32_C(0xFFFF))) & UINT32_C(0xFFFF);
	uint32 iHigh = (((iValue >> 16) * (iFactor >> 16)) &
		UINT32_C(0xFFFF)) << 16;

	return iLow | iHigh;
}



/* 标量长输入轮次保留旧算法顺序，便于编译器自动向量化。 */
static void __xrtNmhLongRound(uint32* XRT_NMH_RESTRICT pX,
	uint32* XRT_NMH_RESTRICT pY, const unsigned char* XRT_NMH_RESTRICT pData)
{
	size_t i;

	for ( i = 0; i < 32u; i++ ) {
		pX[i] ^= __xrtNmhRead32(pData + (i * 4u));
	}
	for ( i = 0; i < 32u; i++ ) {
		pY[i] ^= __xrtNmhRead32(pData + 128u + (i * 4u));
	}
	for ( i = 0; i < 32u; i++ ) {
		pX[i] += pY[i];
		pY[i] ^= pX[i] >> 1;
		pX[i] = __xrtNmhMultiply16(pX[i], XRT_NMH_M1);
		pX[i] ^= (pX[i] << 5) ^ (pX[i] >> 13);
		pX[i] = __xrtNmhMultiply16(pX[i], XRT_NMH_M2);
		pX[i] ^= pY[i];
		pX[i] ^= (pX[i] << 11) ^ (pX[i] >> 9);
		pX[i] = __xrtNmhMultiply16(pX[i], XRT_NMH_M3);
		pX[i] ^= (pX[i] >> 10) ^ (pX[i] >> 20);
	}
}

#else

/* SIMD 路径使用对齐的 lane 乘数。 */
XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) static const uint32 __xrtNmhFactor1[32] = {
	XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1,
	XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1,
	XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1,
	XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1, XRT_NMH_M1
};
XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) static const uint32 __xrtNmhFactor2[32] = {
	XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2,
	XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2,
	XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2,
	XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2, XRT_NMH_M2
};
XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) static const uint32 __xrtNmhFactor3[32] = {
	XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3,
	XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3,
	XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3,
	XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3, XRT_NMH_M3
};

#if XRT_NMH_VECTOR == XRT_NMH_SSE2
	#define XRT_NMH_MM(name) _mm_ ## name
	#define XRT_NMH_MMW(name) _mm_ ## name ## 128
	typedef __m128i __xrt_nmh_vector;
#elif XRT_NMH_VECTOR == XRT_NMH_AVX2
	#define XRT_NMH_MM(name) _mm256_ ## name
	#define XRT_NMH_MMW(name) _mm256_ ## name ## 256
	typedef __m256i __xrt_nmh_vector;
#else
	#define XRT_NMH_MM(name) _mm512_ ## name
	#define XRT_NMH_MMW(name) _mm512_ ## name ## 512
	typedef __m512i __xrt_nmh_vector;
#endif



/* 使用编译目标已经允许的 SIMD 宽度执行长输入轮次。 */
static void __xrtNmhLongRound(uint32* XRT_NMH_RESTRICT pX,
	uint32* XRT_NMH_RESTRICT pY, const unsigned char* XRT_NMH_RESTRICT pData)
{
	const __xrt_nmh_vector* pFactor1 = (const __xrt_nmh_vector*)__xrtNmhFactor1;
	const __xrt_nmh_vector* pFactor2 = (const __xrt_nmh_vector*)__xrtNmhFactor2;
	const __xrt_nmh_vector* pFactor3 = (const __xrt_nmh_vector*)__xrtNmhFactor3;
	__xrt_nmh_vector* pVectorX = (__xrt_nmh_vector*)pX;
	__xrt_nmh_vector* pVectorY = (__xrt_nmh_vector*)pY;
	const __xrt_nmh_vector* pVectorData = (const __xrt_nmh_vector*)pData;
	size_t iCount =
		(sizeof(__xrtNmhInitial) / sizeof(__xrtNmhInitial[0])) /
		(sizeof(__xrt_nmh_vector) / sizeof(uint32));
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		pVectorX[i] = XRT_NMH_MMW(xor_si)(pVectorX[i], XRT_NMH_MMW(loadu_si)(pVectorData + i));
		pVectorY[i] = XRT_NMH_MMW(xor_si)(pVectorY[i], XRT_NMH_MMW(loadu_si)(pVectorData + i + iCount));
		pVectorX[i] = XRT_NMH_MM(add_epi32)(pVectorX[i], pVectorY[i]);
		pVectorY[i] = XRT_NMH_MMW(xor_si)(pVectorY[i], XRT_NMH_MM(srli_epi32)(pVectorX[i], 1));
		pVectorX[i] = XRT_NMH_MM(mullo_epi16)(pVectorX[i], *pFactor1);
		pVectorX[i] = XRT_NMH_MMW(xor_si)(
			XRT_NMH_MMW(xor_si)(pVectorX[i], XRT_NMH_MM(slli_epi32)(pVectorX[i], 5)),
			XRT_NMH_MM(srli_epi32)(pVectorX[i], 13));
		pVectorX[i] = XRT_NMH_MM(mullo_epi16)(pVectorX[i], *pFactor2);
		pVectorX[i] = XRT_NMH_MMW(xor_si)(pVectorX[i], pVectorY[i]);
		pVectorX[i] = XRT_NMH_MMW(xor_si)(
			XRT_NMH_MMW(xor_si)(pVectorX[i], XRT_NMH_MM(slli_epi32)(pVectorX[i], 11)),
			XRT_NMH_MM(srli_epi32)(pVectorX[i], 9));
		pVectorX[i] = XRT_NMH_MM(mullo_epi16)(pVectorX[i], *pFactor3);
		pVectorX[i] = XRT_NMH_MMW(xor_si)(
			XRT_NMH_MMW(xor_si)(pVectorX[i], XRT_NMH_MM(srli_epi32)(pVectorX[i], 10)),
			XRT_NMH_MM(srli_epi32)(pVectorX[i], 20));
	}
}

#undef XRT_NMH_MM
#undef XRT_NMH_MMW

#endif



/* 处理至少 256 字节的输入并合并 32 条累加 lane。 */
static uint32 __xrtNmhLong(const unsigned char* XRT_NMH_RESTRICT pData,
	size_t iSize, uint32 iSeed)
{
	XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) uint32 arrX[32];
	XRT_NMH_ALIGN(XRT_NMH_ACC_ALIGN) uint32 arrY[32];
	size_t iRounds = (iSize - 1u) / 256u;
	uint32 iSum = 0;
	size_t i;

	for ( i = 0; i < 32u; i++ ) {
		arrX[i] = __xrtNmhInitial[i];
		arrY[i] = iSeed;
	}
	for ( i = 0; i < iRounds; i++ ) {
		__xrtNmhLongRound(arrX, arrY, pData + (i * 256u));
	}
	__xrtNmhLongRound(arrX, arrY, pData + iSize - 256u);
	for ( i = 0; i < 32u; i++ ) {
		iSum += arrX[i] ^ __xrtNmhInitial[i];
	}
#if SIZE_MAX > UINT32_MAX
	iSum += (uint32)(iSize >> 32);
#endif
	return iSum ^ (uint32)iSize;
}



/* 完成零到四字节路径的 avalanche。 */
static inline uint32 __xrtNmhSmall(uint32 iValue, uint32 iSeed)
{
	iValue ^= iSeed;
	iValue *= UINT32_C(0xBDAB1EA9);
	iValue += XRT_NMH_ROTATE(iSeed, 31);
	iValue ^= iValue >> 18;
	iValue *= UINT32_C(0xA7896A1B);
	iValue ^= iValue >> 12;
	iValue *= UINT32_C(0x83796A2D);
	iValue ^= iValue >> 16;
	return iValue;
}



/* 处理五到八字节输入。 */
static inline uint32 __xrtNmh5To8(const unsigned char* pData,
	size_t iSize, uint32 iSeed)
{
	uint32 iX = __xrtNmhRead32(pData) ^ XRT_NMH_PRIME3;
	uint32 iY = __xrtNmhRead32(pData + iSize - 4u) ^ iSeed;

	iX += iY;
	iX ^= iX >> iSize;
	iX *= UINT32_C(0x11049A7D);
	iX ^= iX >> 23;
	iX *= UINT32_C(0xBCCCDC7B);
	iX ^= XRT_NMH_ROTATE(iY, 3);
	iX ^= iX >> 12;
	iX *= UINT32_C(0x065E9DAD);
	iX ^= iX >> 12;
	return iX;
}



/* 对一个八字节 lane 对执行完整主体混合并保留旋转后的第二 lane。 */
static inline uint32 __xrtNmhLane(uint32 iX, uint32* pY, unsigned int iRotate)
{
	iX ^= *pY;
	iX *= UINT32_C(0x11049A7D);
	iX ^= iX >> 23;
	iX *= UINT32_C(0xBCCCDC7B);
	*pY = XRT_NMH_ROTATE(*pY, iRotate);
	iX ^= *pY;
	iX ^= iX >> 12;
	iX *= UINT32_C(0x065E9DAD);
	iX ^= iX >> 12;
	return iX;
}



/* 处理九到 255 字节输入。 */
static uint32 __xrtNmh9To255(const unsigned char* pData,
	size_t iSize, uint32 iSeed)
{
	uint32 iX = XRT_NMH_PRIME3;
	uint32 iY = iSeed;
	uint32 iA = XRT_NMH_PRIME4;
	uint32 iB = iSeed;
	size_t iRounds = (iSize - 1u) / 16u;
	size_t i;

	for ( i = 0; i < iRounds; i++ ) {
		iX ^= __xrtNmhRead32(pData + (i * 16u));
		iY ^= __xrtNmhRead32(pData + (i * 16u) + 4u);
		iX = __xrtNmhLane(iX, &iY, 4);
		iA ^= __xrtNmhRead32(pData + (i * 16u) + 8u);
		iB ^= __xrtNmhRead32(pData + (i * 16u) + 12u);
		iA = __xrtNmhLane(iA, &iB, 3);
	}

	if ( XRT_NMH_LIKELY((((uint8)iSize - 1u) & 8u) != 0) ) {
		if ( XRT_NMH_LIKELY((((uint8)iSize - 1u) & 4u) != 0) ) {
			iA ^= __xrtNmhRead32(pData + (iRounds * 16u));
			iB ^= __xrtNmhRead32(pData + (iRounds * 16u) + 4u);
			iA ^= iB;
			iA *= UINT32_C(0x11049A7D);
			iA ^= iA >> 23;
			iA *= UINT32_C(0xBCCCDC7B);
			iA ^= XRT_NMH_ROTATE(iB, 4);
			iA ^= iA >> 12;
			iA *= UINT32_C(0x065E9DAD);
		} else {
			iA ^= __xrtNmhRead32(pData + (iRounds * 16u)) + iB;
			iA ^= iA >> 16;
			iA *= UINT32_C(0xA52FB2CD);
			iA ^= iA >> 15;
			iA *= UINT32_C(0x551E4D49);
		}
		iX ^= __xrtNmhRead32(pData + iSize - 8u);
		iY ^= __xrtNmhRead32(pData + iSize - 4u);
		iX ^= iY;
		iX *= UINT32_C(0x11049A7D);
		iX ^= iX >> 23;
		iX *= UINT32_C(0xBCCCDC7B);
		iX ^= XRT_NMH_ROTATE(iY, 3);
		iX ^= iX >> 12;
		iX *= UINT32_C(0x065E9DAD);
	} else {
		if ( XRT_NMH_LIKELY((((uint8)iSize - 1u) & 4u) != 0) ) {
			iA ^= __xrtNmhRead32(pData + (iRounds * 16u)) + iB;
			iA ^= iA >> 16;
			iA *= UINT32_C(0xA52FB2CD);
			iA ^= iA >> 15;
			iA *= UINT32_C(0x551E4D49);
		}
		iX ^= __xrtNmhRead32(pData + iSize - 4u) + iY;
		iX ^= iX >> 16;
		iX *= UINT32_C(0xA52FB2CD);
		iX ^= iX >> 15;
		iX *= UINT32_C(0x551E4D49);
	}

	iX ^= (uint32)iSize;
	iX ^= XRT_NMH_ROTATE(iA, 27);
	iX ^= iX >> 14;
	iX *= UINT32_C(0x141CC535);
	return iX;
}



/* 完成长输入路径的最终 avalanche。 */
static inline uint32 __xrtNmhAvalanche(uint32 iValue)
{
	iValue ^= iValue >> 15;
	iValue *= UINT32_C(0xD168AAAD);
	iValue ^= iValue >> 15;
	iValue *= UINT32_C(0xAF723597);
	iValue ^= iValue >> 15;
	return iValue;
}



/* 保持 nmhash32x v2.0 所有长度分支的原始输出。 */
static uint32 __xrtNmHash32(const void* pData, size_t iSize, uint32 iSeed)
{
	const unsigned char* pBytes = (const unsigned char*)pData;

	if ( XRT_NMH_LIKELY(iSize <= 8u) ) {
		if ( XRT_NMH_LIKELY(iSize > 4u) ) {
			return __xrtNmh5To8(pBytes, iSize, iSeed);
		} else {
			union {
				uint32 Value;
			} Data = { 0 };

			switch ( iSize ) {
				case 0:
					iSeed += XRT_NMH_PRIME2;
					break;
				case 1:
					iSeed += XRT_NMH_PRIME2 + (UINT32_C(1) << 24) + 2u;
					Data.Value = pBytes[0];
					break;
				case 2:
					iSeed += XRT_NMH_PRIME2 + (UINT32_C(2) << 24) + 4u;
					Data.Value = __xrtNmhRead16(pBytes);
					break;
				case 3:
					iSeed += XRT_NMH_PRIME2 + (UINT32_C(3) << 24) + 6u;
					Data.Value = __xrtNmhRead16(pBytes) | ((uint32)pBytes[2] << 16);
					break;
				default:
					iSeed += XRT_NMH_PRIME1;
					Data.Value = __xrtNmhRead32(pBytes);
					break;
			}
			return __xrtNmhSmall(Data.Value, iSeed);
		}
	}
	if ( XRT_NMH_LIKELY(iSize < 256u) ) {
		return __xrtNmh9To255(pBytes, iSize, iSeed);
	}
	return __xrtNmhAvalanche(__xrtNmhLong(pBytes, iSize, iSeed));
}



/* 计算使用明确 seed 的确定性 32 位哈希。 */
XRT_API uint32 xrtHash32Seed(const void* pData, size_t iSize, uint32 iSeed)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	return __xrtNmHash32(pData, iSize, iSeed);
}



/* 默认 seed 固定为零，使旧数据和跨进程结果继续稳定。 */
XRT_API uint32 xrtHash32(const void* pData, size_t iSize)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	return __xrtNmHash32(pData, iSize, 0);
}



#undef XRT_NMH_LIKELY
#undef XRT_NMH_ROTATE
#undef XRT_NMH_RESTRICT
#undef XRT_NMH_SCALAR
#undef XRT_NMH_SSE2
#undef XRT_NMH_AVX2
#undef XRT_NMH_AVX512
#undef XRT_NMH_VECTOR
#undef XRT_NMH_ALIGN
#undef XRT_NMH_ACC_ALIGN
#undef XRT_NMH_PRIME1
#undef XRT_NMH_PRIME2
#undef XRT_NMH_PRIME3
#undef XRT_NMH_PRIME4
#undef XRT_NMH_M1
#undef XRT_NMH_M2
#undef XRT_NMH_M3

#endif
