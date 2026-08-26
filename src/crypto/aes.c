/*
	AES 常量时间软件内核改编自 BearSSL 0.6 的 aes_ct64 实现。
	Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>, MIT License.
*/

#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_AES) && !defined(__TINYC__) && \
	(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
	 defined(_M_X64) || defined(_M_AMD64)) && \
	(defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
	#define XRT_AES_X86_HARDWARE 1
	/* clang-cl 同样定义 _MSC_VER，但需要显式 target 特性。 */
	#if defined(_MSC_VER) && !defined(__clang__)
		#include <intrin.h>
		#include <wmmintrin.h>
		#define XRT_AES_TARGET
	#else
		#include <cpuid.h>
		#include <wmmintrin.h>
		#define XRT_AES_TARGET __attribute__((target("sse2,aes")))
	#endif
#else
	#define XRT_AES_X86_HARDWARE 0
	#define XRT_AES_TARGET
#endif



#if defined(XRT_FEATURE_CRYPTO_AES) && !defined(__TINYC__) && \
	defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
	#define XRT_AES_ARM_HARDWARE 1
	#include <arm_neon.h>
	#define XRT_AES_ARM_TARGET __attribute__((target("+crypto")))
	#if defined(__linux__)
		#include <sys/auxv.h>
		#if defined(__has_include)
			#if __has_include(<asm/hwcap.h>)
				#include <asm/hwcap.h>
			#endif
		#endif
		#ifndef HWCAP_AES
			#define HWCAP_AES (UINT64_C(1) << 3u)
		#endif
		#ifndef HWCAP_PMULL
			#define HWCAP_PMULL (UINT64_C(1) << 4u)
		#endif
	#endif
#else
	#define XRT_AES_ARM_HARDWARE 0
	#define XRT_AES_ARM_TARGET
#endif



#if defined(XRT_FEATURE_CRYPTO_AES)

#define XRT_AES_GUARD UINT32_C(0x41455320)



/* 查询当前处理器可安全执行的 AES 与无进位乘法指令。 */
static uint32 __xrtAesHardwareFeatures(void)
{
	uint32 iFeatures = 0;

	#if XRT_AES_X86_HARDWARE
		#if defined(_MSC_VER)
			int Cpu[4];

			__cpuid(Cpu, 0);
			if ( Cpu[0] < 1 ) {
				return 0;
			}
			__cpuid(Cpu, 1);
			if ( (((uint32)Cpu[2] >> 25u) & 1u) != 0 ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_AESNI;
			}
			if ( ((((uint32)Cpu[2] >> 1u) & 1u) != 0) &&
				 ((((uint32)Cpu[2] >> 9u) & 1u) != 0) ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_PCLMUL;
			}
		#else
			unsigned int iMax;
			unsigned int iEax;
			unsigned int iEbx;
			unsigned int iEcx;
			unsigned int iEdx;

			iMax = __get_cpuid_max(0, NULL);
			if ( iMax < 1u ) {
				return 0;
			}
			__cpuid(1, iEax, iEbx, iEcx, iEdx);
			(void)iEax;
			(void)iEbx;
			(void)iEdx;
			if ( ((iEcx >> 25u) & 1u) != 0 ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_AESNI;
			}
			if ( (((iEcx >> 1u) & 1u) != 0) &&
				 (((iEcx >> 9u) & 1u) != 0) ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_PCLMUL;
			}
		#endif
	#endif
	#if XRT_AES_ARM_HARDWARE
		#if defined(__linux__)
			unsigned long iCapabilities = getauxval(AT_HWCAP);

			if ( (iCapabilities & HWCAP_AES) != 0 ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_ARM_AES;
			}
			if ( (iCapabilities & HWCAP_PMULL) != 0 ) {
				iFeatures |= XRT_INTERNAL_AES_BACKEND_ARM_PMULL;
			}
		#elif defined(__APPLE__)
			iFeatures |= XRT_INTERNAL_AES_BACKEND_ARM_AES |
				XRT_INTERNAL_AES_BACKEND_ARM_PMULL;
		#elif defined(__ARM_FEATURE_CRYPTO)
			iFeatures |= XRT_INTERNAL_AES_BACKEND_ARM_AES |
				XRT_INTERNAL_AES_BACKEND_ARM_PMULL;
		#endif
	#endif
	return iFeatures;
}



/* 处理器能力只探测一次；并发首次初始化允许重复探测，但只发布一个稳定结果。 */
static uint32 __xrtAesHardwareFeaturesCached(void)
{
	uint32 iFeatures;
	#if XRT_AES_X86_HARDWARE || XRT_AES_ARM_HARDWARE
		static uint32 iCached = UINT32_MAX;
	#endif

	#if defined(_MSC_VER) && XRT_AES_X86_HARDWARE
		iFeatures = (uint32)_InterlockedCompareExchange(
			(volatile long*)&iCached,
			(long)UINT32_MAX,
			(long)UINT32_MAX
		);
		if ( iFeatures == UINT32_MAX ) {
			iFeatures = __xrtAesHardwareFeatures();
			(void)_InterlockedCompareExchange(
				(volatile long*)&iCached,
				(long)iFeatures,
				(long)UINT32_MAX
			);
			iFeatures = (uint32)_InterlockedCompareExchange(
				(volatile long*)&iCached,
				(long)UINT32_MAX,
				(long)UINT32_MAX
			);
		}
	#elif (defined(__GNUC__) || defined(__clang__)) && \
		(XRT_AES_X86_HARDWARE || XRT_AES_ARM_HARDWARE)
		iFeatures = __atomic_load_n(&iCached, __ATOMIC_ACQUIRE);
		if ( iFeatures == UINT32_MAX ) {
			uint32 iExpected = UINT32_MAX;

			iFeatures = __xrtAesHardwareFeatures();
			(void)__atomic_compare_exchange_n(
				&iCached,
				&iExpected,
				iFeatures,
				false,
				__ATOMIC_RELEASE,
				__ATOMIC_ACQUIRE
			);
			if ( iExpected != UINT32_MAX ) {
				iFeatures = iExpected;
			}
		}
	#else
		iFeatures = __xrtAesHardwareFeatures();
	#endif
	return iFeatures;
}



/* 以固定逻辑门电路计算 64 路并行 AES S-box，避免密钥相关查表。 */
static void __xrtAesSbox(uint64 pQ[8])
{
	uint64 x0, x1, x2, x3, x4, x5, x6, x7;
	uint64 y1, y2, y3, y4, y5, y6, y7, y8, y9;
	uint64 y10, y11, y12, y13, y14, y15, y16, y17, y18, y19;
	uint64 y20, y21;
	uint64 z0, z1, z2, z3, z4, z5, z6, z7, z8, z9;
	uint64 z10, z11, z12, z13, z14, z15, z16, z17;
	uint64 t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
	uint64 t10, t11, t12, t13, t14, t15, t16, t17, t18, t19;
	uint64 t20, t21, t22, t23, t24, t25, t26, t27, t28, t29;
	uint64 t30, t31, t32, t33, t34, t35, t36, t37, t38, t39;
	uint64 t40, t41, t42, t43, t44, t45, t46, t47, t48, t49;
	uint64 t50, t51, t52, t53, t54, t55, t56, t57, t58, t59;
	uint64 t60, t61, t62, t63, t64, t65, t66, t67;
	uint64 s0, s1, s2, s3, s4, s5, s6, s7;

	/* 输入位按 Boyar-Peralta 电路的高位优先编号。 */
	x0 = pQ[7];
	x1 = pQ[6];
	x2 = pQ[5];
	x3 = pQ[4];
	x4 = pQ[3];
	x5 = pQ[2];
	x6 = pQ[1];
	x7 = pQ[0];

	/* 顶部线性变换。 */
	y14 = x3 ^ x5;
	y13 = x0 ^ x6;
	y9 = x0 ^ x3;
	y8 = x0 ^ x5;
	t0 = x1 ^ x2;
	y1 = t0 ^ x7;
	y4 = y1 ^ x3;
	y12 = y13 ^ y14;
	y2 = y1 ^ x0;
	y5 = y1 ^ x6;
	y3 = y5 ^ y8;
	t1 = x4 ^ y12;
	y15 = t1 ^ x5;
	y20 = t1 ^ x1;
	y6 = y15 ^ x7;
	y10 = y15 ^ t0;
	y11 = y20 ^ y9;
	y7 = x7 ^ y11;
	y17 = y10 ^ y11;
	y19 = y10 ^ y8;
	y16 = t0 ^ y11;
	y21 = y13 ^ y16;
	y18 = x0 ^ y16;

	/* 非线性逻辑门段。 */
	t2 = y12 & y15;
	t3 = y3 & y6;
	t4 = t3 ^ t2;
	t5 = y4 & x7;
	t6 = t5 ^ t2;
	t7 = y13 & y16;
	t8 = y5 & y1;
	t9 = t8 ^ t7;
	t10 = y2 & y7;
	t11 = t10 ^ t7;
	t12 = y9 & y11;
	t13 = y14 & y17;
	t14 = t13 ^ t12;
	t15 = y8 & y10;
	t16 = t15 ^ t12;
	t17 = t4 ^ t14;
	t18 = t6 ^ t16;
	t19 = t9 ^ t14;
	t20 = t11 ^ t16;
	t21 = t17 ^ y20;
	t22 = t18 ^ y19;
	t23 = t19 ^ y21;
	t24 = t20 ^ y18;
	t25 = t21 ^ t22;
	t26 = t21 & t23;
	t27 = t24 ^ t26;
	t28 = t25 & t27;
	t29 = t28 ^ t22;
	t30 = t23 ^ t24;
	t31 = t22 ^ t26;
	t32 = t31 & t30;
	t33 = t32 ^ t24;
	t34 = t23 ^ t33;
	t35 = t27 ^ t33;
	t36 = t24 & t35;
	t37 = t36 ^ t34;
	t38 = t27 ^ t36;
	t39 = t29 & t38;
	t40 = t25 ^ t39;
	t41 = t40 ^ t37;
	t42 = t29 ^ t33;
	t43 = t29 ^ t40;
	t44 = t33 ^ t37;
	t45 = t42 ^ t41;
	z0 = t44 & y15;
	z1 = t37 & y6;
	z2 = t33 & x7;
	z3 = t43 & y16;
	z4 = t40 & y1;
	z5 = t29 & y7;
	z6 = t42 & y11;
	z7 = t45 & y17;
	z8 = t41 & y10;
	z9 = t44 & y12;
	z10 = t37 & y3;
	z11 = t33 & y4;
	z12 = t43 & y13;
	z13 = t40 & y5;
	z14 = t29 & y2;
	z15 = t42 & y9;
	z16 = t45 & y14;
	z17 = t41 & y8;

	/* 底部线性变换并恢复 XRT 使用的低位优先编号。 */
	t46 = z15 ^ z16;
	t47 = z10 ^ z11;
	t48 = z5 ^ z13;
	t49 = z9 ^ z10;
	t50 = z2 ^ z12;
	t51 = z2 ^ z5;
	t52 = z7 ^ z8;
	t53 = z0 ^ z3;
	t54 = z6 ^ z7;
	t55 = z16 ^ z17;
	t56 = z12 ^ t48;
	t57 = t50 ^ t53;
	t58 = z4 ^ t46;
	t59 = z3 ^ t54;
	t60 = t46 ^ t57;
	t61 = z14 ^ t57;
	t62 = t52 ^ t58;
	t63 = t49 ^ t58;
	t64 = z4 ^ t59;
	t65 = t61 ^ t62;
	t66 = z1 ^ t63;
	s0 = t59 ^ t63;
	s6 = t56 ^ ~t62;
	s7 = t48 ^ ~t60;
	t67 = t64 ^ t65;
	s3 = t53 ^ t66;
	s4 = t51 ^ t66;
	s5 = t47 ^ t65;
	s1 = t64 ^ ~s3;
	s2 = t55 ^ ~t67;
	pQ[7] = s0;
	pQ[6] = s1;
	pQ[5] = s2;
	pQ[4] = s3;
	pQ[3] = s4;
	pQ[2] = s5;
	pQ[1] = s6;
	pQ[0] = s7;
}



/* 通过正向 S-box 前后的仿射变换计算逆 S-box。 */
static void __xrtAesInverseSbox(uint64 pQ[8])
{
	uint64 q0 = ~pQ[0];
	uint64 q1 = ~pQ[1];
	uint64 q2 = pQ[2];
	uint64 q3 = pQ[3];
	uint64 q4 = pQ[4];
	uint64 q5 = ~pQ[5];
	uint64 q6 = ~pQ[6];
	uint64 q7 = pQ[7];

	pQ[7] = q1 ^ q4 ^ q6;
	pQ[6] = q0 ^ q3 ^ q5;
	pQ[5] = q7 ^ q2 ^ q4;
	pQ[4] = q6 ^ q1 ^ q3;
	pQ[3] = q5 ^ q0 ^ q2;
	pQ[2] = q4 ^ q7 ^ q1;
	pQ[1] = q3 ^ q6 ^ q0;
	pQ[0] = q2 ^ q5 ^ q7;
	__xrtAesSbox(pQ);
	q0 = ~pQ[0];
	q1 = ~pQ[1];
	q2 = pQ[2];
	q3 = pQ[3];
	q4 = pQ[4];
	q5 = ~pQ[5];
	q6 = ~pQ[6];
	q7 = pQ[7];
	pQ[7] = q1 ^ q4 ^ q6;
	pQ[6] = q0 ^ q3 ^ q5;
	pQ[5] = q7 ^ q2 ^ q4;
	pQ[4] = q6 ^ q1 ^ q3;
	pQ[3] = q5 ^ q0 ^ q2;
	pQ[2] = q4 ^ q7 ^ q1;
	pQ[1] = q3 ^ q6 ^ q0;
	pQ[0] = q2 ^ q5 ^ q7;
}



/* 交换两个位平面中的交错字段。 */
static inline void __xrtAesSwap(
	uint64* pLeft,
	uint64* pRight,
	uint64 iLowMask,
	uint64 iHighMask,
	uint32 iShift
)
{
	uint64 iLeft = *pLeft;
	uint64 iRight = *pRight;

	*pLeft = (iLeft & iLowMask) | ((iRight & iLowMask) << iShift);
	*pRight = ((iLeft & iHighMask) >> iShift) | (iRight & iHighMask);
}



/* 在常规字节布局与 AES 位切片布局之间执行正交变换。 */
static void __xrtAesOrtho(uint64 pQ[8])
{
	for ( size_t i = 0; i < 8; i += 2 ) {
		__xrtAesSwap(
			&pQ[i], &pQ[i + 1],
			UINT64_C(0x5555555555555555),
			UINT64_C(0xAAAAAAAAAAAAAAAA), 1u
		);
	}
	__xrtAesSwap(&pQ[0], &pQ[2],
		UINT64_C(0x3333333333333333),
		UINT64_C(0xCCCCCCCCCCCCCCCC), 2u);
	__xrtAesSwap(&pQ[1], &pQ[3],
		UINT64_C(0x3333333333333333),
		UINT64_C(0xCCCCCCCCCCCCCCCC), 2u);
	__xrtAesSwap(&pQ[4], &pQ[6],
		UINT64_C(0x3333333333333333),
		UINT64_C(0xCCCCCCCCCCCCCCCC), 2u);
	__xrtAesSwap(&pQ[5], &pQ[7],
		UINT64_C(0x3333333333333333),
		UINT64_C(0xCCCCCCCCCCCCCCCC), 2u);
	for ( size_t i = 0; i < 4; i++ ) {
		__xrtAesSwap(
			&pQ[i], &pQ[i + 4],
			UINT64_C(0x0F0F0F0F0F0F0F0F),
			UINT64_C(0xF0F0F0F0F0F0F0F0), 4u
		);
	}
}



/* 把四个普通 32 位字交织为两个 64 位字。 */
static void __xrtAesInterleaveIn(
	uint64* pEven,
	uint64* pOdd,
	const uint32 pWords[4]
)
{
	uint64 x0 = pWords[0];
	uint64 x1 = pWords[1];
	uint64 x2 = pWords[2];
	uint64 x3 = pWords[3];

	x0 = (x0 | (x0 << 16u)) & UINT64_C(0x0000FFFF0000FFFF);
	x1 = (x1 | (x1 << 16u)) & UINT64_C(0x0000FFFF0000FFFF);
	x2 = (x2 | (x2 << 16u)) & UINT64_C(0x0000FFFF0000FFFF);
	x3 = (x3 | (x3 << 16u)) & UINT64_C(0x0000FFFF0000FFFF);
	x0 = (x0 | (x0 << 8u)) & UINT64_C(0x00FF00FF00FF00FF);
	x1 = (x1 | (x1 << 8u)) & UINT64_C(0x00FF00FF00FF00FF);
	x2 = (x2 | (x2 << 8u)) & UINT64_C(0x00FF00FF00FF00FF);
	x3 = (x3 | (x3 << 8u)) & UINT64_C(0x00FF00FF00FF00FF);
	*pEven = x0 | (x2 << 8u);
	*pOdd = x1 | (x3 << 8u);
}



/* 把两个 64 位交织字恢复为四个普通 32 位字。 */
static void __xrtAesInterleaveOut(
	uint32 pWords[4],
	uint64 iEven,
	uint64 iOdd
)
{
	uint64 x0 = iEven & UINT64_C(0x00FF00FF00FF00FF);
	uint64 x1 = iOdd & UINT64_C(0x00FF00FF00FF00FF);
	uint64 x2 = (iEven >> 8u) & UINT64_C(0x00FF00FF00FF00FF);
	uint64 x3 = (iOdd >> 8u) & UINT64_C(0x00FF00FF00FF00FF);

	x0 = (x0 | (x0 >> 8u)) & UINT64_C(0x0000FFFF0000FFFF);
	x1 = (x1 | (x1 >> 8u)) & UINT64_C(0x0000FFFF0000FFFF);
	x2 = (x2 | (x2 >> 8u)) & UINT64_C(0x0000FFFF0000FFFF);
	x3 = (x3 | (x3 >> 8u)) & UINT64_C(0x0000FFFF0000FFFF);
	pWords[0] = (uint32)x0 | (uint32)(x0 >> 16u);
	pWords[1] = (uint32)x1 | (uint32)(x1 >> 16u);
	pWords[2] = (uint32)x2 | (uint32)(x2 >> 16u);
	pWords[3] = (uint32)x3 | (uint32)(x3 >> 16u);
}



/* 对密钥扩展中的一个 32 位字执行常量时间 SubWord。 */
static uint32 __xrtAesSubWord(uint32 iValue)
{
	uint64 Q[8];
	uint32 iResult;

	memset(Q, 0, sizeof(Q));
	Q[0] = iValue;
	__xrtAesOrtho(Q);
	__xrtAesSbox(Q);
	__xrtAesOrtho(Q);
	iResult = (uint32)Q[0];
	xrtSecureZero(Q, sizeof(Q));
	return iResult;
}



/* 验证调用方持有的 AES 轮密钥状态。 */
static bool __xrtAesValidateState(const xaes* pState)
{
	if ( (pState == NULL) || (pState->Guard != XRT_AES_GUARD) ||
		 ((pState->Rounds != 10u) && (pState->Rounds != 12u) &&
		  (pState->Rounds != 14u)) ||
		 ((pState->Backend & ~__xrtAesHardwareFeaturesCached()) != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 把一轮普通 AES 密钥复制到四路位切片轮密钥。 */
static void __xrtAesExpandRound(const uint8* pRoundKey, uint64 pOutput[8])
{
	uint32 Words[4];

	for ( size_t i = 0; i < 4; i++ ) {
		Words[i] = __xrtCryptoLoadLe32(pRoundKey + (i * 4u));
	}
	__xrtAesInterleaveIn(&pOutput[0], &pOutput[4], Words);
	pOutput[1] = pOutput[0];
	pOutput[2] = pOutput[0];
	pOutput[3] = pOutput[0];
	pOutput[5] = pOutput[4];
	pOutput[6] = pOutput[4];
	pOutput[7] = pOutput[4];
	__xrtAesOrtho(pOutput);
	xrtSecureZero(Words, sizeof(Words));
}



/* 验证状态，并一次性展开本次批处理使用的全部位切片轮密钥。 */
bool __xrtAesExpand(
	const xaes* pState,
	uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS]
)
{
	if ( (pExpanded == NULL) || !__xrtAesValidateState(pState) ) {
		if ( pExpanded == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	for ( uint32 i = 0; i <= pState->Rounds; i++ ) {
		__xrtAesExpandRound(
			pState->RoundKey + ((size_t)i * XRT_AES_BLOCK_SIZE),
			pExpanded + ((size_t)i * 8u)
		);
	}
	return true;
}



/* 验证状态后查询指定硬件后端位。 */
bool __xrtAesHasBackend(const xaes* pState, uint32 iBackend)
{
	if ( !__xrtAesValidateState(pState) ) {
		return false;
	}
	return (pState->Backend & iBackend) == iBackend;
}



/* 把一轮位切片密钥异或到四个并行 AES 块。 */
static inline void __xrtAesAddRoundKey(uint64 pQ[8], const uint64* pKey)
{
	for ( size_t i = 0; i < 8; i++ ) {
		pQ[i] ^= pKey[i];
	}
}



/* 在位切片布局中执行 AES ShiftRows。 */
static void __xrtAesShiftRows(uint64 pQ[8])
{
	for ( size_t i = 0; i < 8; i++ ) {
		uint64 x = pQ[i];

		pQ[i] = (x & UINT64_C(0x000000000000FFFF)) |
			((x & UINT64_C(0x00000000FFF00000)) >> 4u) |
			((x & UINT64_C(0x00000000000F0000)) << 12u) |
			((x & UINT64_C(0x0000FF0000000000)) >> 8u) |
			((x & UINT64_C(0x000000FF00000000)) << 8u) |
			((x & UINT64_C(0xF000000000000000)) >> 12u) |
			((x & UINT64_C(0x0FFF000000000000)) << 4u);
	}
}



/* 在位切片布局中执行 AES 逆 ShiftRows。 */
static void __xrtAesInverseShiftRows(uint64 pQ[8])
{
	for ( size_t i = 0; i < 8; i++ ) {
		uint64 x = pQ[i];

		pQ[i] = (x & UINT64_C(0x000000000000FFFF)) |
			((x & UINT64_C(0x000000000FFF0000)) << 4u) |
			((x & UINT64_C(0x00000000F0000000)) >> 12u) |
			((x & UINT64_C(0x000000FF00000000)) << 8u) |
			((x & UINT64_C(0x0000FF0000000000)) >> 8u) |
			((x & UINT64_C(0x000F000000000000)) << 12u) |
			((x & UINT64_C(0xFFF0000000000000)) >> 4u);
	}
}



/* 循环右移一个 64 位字的两个 32 位半字。 */
static inline uint64 __xrtAesRotate32(uint64 iValue)
{
	return (iValue << 32u) | (iValue >> 32u);
}



/* 在位切片布局中执行 AES MixColumns。 */
static void __xrtAesMixColumns(uint64 pQ[8])
{
	uint64 q0 = pQ[0], q1 = pQ[1], q2 = pQ[2], q3 = pQ[3];
	uint64 q4 = pQ[4], q5 = pQ[5], q6 = pQ[6], q7 = pQ[7];
	uint64 r0 = (q0 >> 16u) | (q0 << 48u);
	uint64 r1 = (q1 >> 16u) | (q1 << 48u);
	uint64 r2 = (q2 >> 16u) | (q2 << 48u);
	uint64 r3 = (q3 >> 16u) | (q3 << 48u);
	uint64 r4 = (q4 >> 16u) | (q4 << 48u);
	uint64 r5 = (q5 >> 16u) | (q5 << 48u);
	uint64 r6 = (q6 >> 16u) | (q6 << 48u);
	uint64 r7 = (q7 >> 16u) | (q7 << 48u);

	pQ[0] = q7 ^ r7 ^ r0 ^ __xrtAesRotate32(q0 ^ r0);
	pQ[1] = q0 ^ r0 ^ q7 ^ r7 ^ r1 ^ __xrtAesRotate32(q1 ^ r1);
	pQ[2] = q1 ^ r1 ^ r2 ^ __xrtAesRotate32(q2 ^ r2);
	pQ[3] = q2 ^ r2 ^ q7 ^ r7 ^ r3 ^ __xrtAesRotate32(q3 ^ r3);
	pQ[4] = q3 ^ r3 ^ q7 ^ r7 ^ r4 ^ __xrtAesRotate32(q4 ^ r4);
	pQ[5] = q4 ^ r4 ^ r5 ^ __xrtAesRotate32(q5 ^ r5);
	pQ[6] = q5 ^ r5 ^ r6 ^ __xrtAesRotate32(q6 ^ r6);
	pQ[7] = q6 ^ r6 ^ r7 ^ __xrtAesRotate32(q7 ^ r7);
}



/* 在位切片布局中执行 AES 逆 MixColumns。 */
static void __xrtAesInverseMixColumns(uint64 pQ[8])
{
	uint64 q0 = pQ[0], q1 = pQ[1], q2 = pQ[2], q3 = pQ[3];
	uint64 q4 = pQ[4], q5 = pQ[5], q6 = pQ[6], q7 = pQ[7];
	uint64 r0 = (q0 >> 16u) | (q0 << 48u);
	uint64 r1 = (q1 >> 16u) | (q1 << 48u);
	uint64 r2 = (q2 >> 16u) | (q2 << 48u);
	uint64 r3 = (q3 >> 16u) | (q3 << 48u);
	uint64 r4 = (q4 >> 16u) | (q4 << 48u);
	uint64 r5 = (q5 >> 16u) | (q5 << 48u);
	uint64 r6 = (q6 >> 16u) | (q6 << 48u);
	uint64 r7 = (q7 >> 16u) | (q7 << 48u);

	pQ[0] = q5 ^ q6 ^ q7 ^ r0 ^ r5 ^ r7 ^
		__xrtAesRotate32(q0 ^ q5 ^ q6 ^ r0 ^ r5);
	pQ[1] = q0 ^ q5 ^ r0 ^ r1 ^ r5 ^ r6 ^ r7 ^
		__xrtAesRotate32(q1 ^ q5 ^ q7 ^ r1 ^ r5 ^ r6);
	pQ[2] = q0 ^ q1 ^ q6 ^ r1 ^ r2 ^ r6 ^ r7 ^
		__xrtAesRotate32(q0 ^ q2 ^ q6 ^ r2 ^ r6 ^ r7);
	pQ[3] = q0 ^ q1 ^ q2 ^ q5 ^ q6 ^ r0 ^ r2 ^ r3 ^ r5 ^
		__xrtAesRotate32(q0 ^ q1 ^ q3 ^ q5 ^ q6 ^ q7 ^ r0 ^ r3 ^ r5 ^ r7);
	pQ[4] = q1 ^ q2 ^ q3 ^ q5 ^ r1 ^ r3 ^ r4 ^ r5 ^ r6 ^ r7 ^
		__xrtAesRotate32(q1 ^ q2 ^ q4 ^ q5 ^ q7 ^ r1 ^ r4 ^ r5 ^ r6);
	pQ[5] = q2 ^ q3 ^ q4 ^ q6 ^ r2 ^ r4 ^ r5 ^ r6 ^ r7 ^
		__xrtAesRotate32(q2 ^ q3 ^ q5 ^ q6 ^ r2 ^ r5 ^ r6 ^ r7);
	pQ[6] = q3 ^ q4 ^ q5 ^ q7 ^ r3 ^ r5 ^ r6 ^ r7 ^
		__xrtAesRotate32(q3 ^ q4 ^ q6 ^ q7 ^ r3 ^ r6 ^ r7);
	pQ[7] = q4 ^ q5 ^ q6 ^ r4 ^ r6 ^ r7 ^
		__xrtAesRotate32(q4 ^ q5 ^ q7 ^ r4 ^ r7);
}



/* 对四个并行位切片块执行全部 AES 正向轮。 */
static void __xrtAesEncryptBitslice(
	const uint64* pExpanded,
	uint32 iRounds,
	uint64 pQ[8]
)
{
	__xrtAesAddRoundKey(pQ, pExpanded);
	for ( uint32 i = 1; i < iRounds; i++ ) {
		__xrtAesSbox(pQ);
		__xrtAesShiftRows(pQ);
		__xrtAesMixColumns(pQ);
		__xrtAesAddRoundKey(pQ, pExpanded + ((size_t)i * 8u));
	}
	__xrtAesSbox(pQ);
	__xrtAesShiftRows(pQ);
	__xrtAesAddRoundKey(pQ, pExpanded + ((size_t)iRounds * 8u));
}



/* 对四个并行位切片块执行全部 AES 逆向轮。 */
static void __xrtAesDecryptBitslice(
	const uint64* pExpanded,
	uint32 iRounds,
	uint64 pQ[8]
)
{
	__xrtAesAddRoundKey(pQ, pExpanded + ((size_t)iRounds * 8u));
	for ( uint32 i = iRounds - 1u; i != 0; i-- ) {
		__xrtAesInverseShiftRows(pQ);
		__xrtAesInverseSbox(pQ);
		__xrtAesAddRoundKey(pQ, pExpanded + ((size_t)i * 8u));
		__xrtAesInverseMixColumns(pQ);
	}
	__xrtAesInverseShiftRows(pQ);
	__xrtAesInverseSbox(pQ);
	__xrtAesAddRoundKey(pQ, pExpanded);
}



#if XRT_AES_X86_HARDWARE

/* 使用 AES-NI 并行加密一至四个完整块。 */
static XRT_AES_TARGET void __xrtAesEncryptHardware(
	const xaes* pState,
	const uint8* pInput,
	uint8* pOutput,
	size_t iBlocks
)
{
	while ( iBlocks != 0 ) {
		__m128i Blocks[4];
		__m128i Key;
		size_t iBatch = iBlocks < 4u ? iBlocks : 4u;

		Key = _mm_loadu_si128((const __m128i*)pState->RoundKey);
		for ( size_t i = 0; i < iBatch; i++ ) {
			Blocks[i] = _mm_xor_si128(
				_mm_loadu_si128((const __m128i*)(
					pInput + (i * XRT_AES_BLOCK_SIZE)
				)),
				Key
			);
		}
		for ( uint32 i = 1; i < pState->Rounds; i++ ) {
			Key = _mm_loadu_si128((const __m128i*)(
				pState->RoundKey + ((size_t)i * XRT_AES_BLOCK_SIZE)
			));
			for ( size_t j = 0; j < iBatch; j++ ) {
				Blocks[j] = _mm_aesenc_si128(Blocks[j], Key);
			}
		}
		Key = _mm_loadu_si128((const __m128i*)(
			pState->RoundKey + ((size_t)pState->Rounds * XRT_AES_BLOCK_SIZE)
		));
		for ( size_t i = 0; i < iBatch; i++ ) {
			Blocks[i] = _mm_aesenclast_si128(Blocks[i], Key);
			_mm_storeu_si128(
				(__m128i*)(pOutput + (i * XRT_AES_BLOCK_SIZE)),
				Blocks[i]
			);
		}
		pInput += iBatch * XRT_AES_BLOCK_SIZE;
		pOutput += iBatch * XRT_AES_BLOCK_SIZE;
		iBlocks -= iBatch;
	}
}



/* 使用正向轮密钥即时派生 AES-NI 逆轮密钥并解密一个块。 */
static XRT_AES_TARGET void __xrtAesDecryptHardware(
	const xaes* pState,
	const uint8* pInput,
	uint8* pOutput
)
{
	__m128i Block;
	__m128i Key;

	Key = _mm_loadu_si128((const __m128i*)(
		pState->RoundKey + ((size_t)pState->Rounds * XRT_AES_BLOCK_SIZE)
	));
	Block = _mm_xor_si128(_mm_loadu_si128((const __m128i*)pInput), Key);
	for ( uint32 i = pState->Rounds - 1u; i != 0; i-- ) {
		Key = _mm_aesimc_si128(_mm_loadu_si128((const __m128i*)(
			pState->RoundKey + ((size_t)i * XRT_AES_BLOCK_SIZE)
		)));
		Block = _mm_aesdec_si128(Block, Key);
	}
	Key = _mm_loadu_si128((const __m128i*)pState->RoundKey);
	Block = _mm_aesdeclast_si128(Block, Key);
	_mm_storeu_si128((__m128i*)pOutput, Block);
}

#endif



#if XRT_AES_ARM_HARDWARE

/* 使用 ARMv8 AES 指令并行加密一至四个完整块。 */
static XRT_AES_ARM_TARGET void __xrtAesEncryptArm(
	const xaes* pState,
	const uint8* pInput,
	uint8* pOutput,
	size_t iBlocks
)
{
	while ( iBlocks != 0 ) {
		uint8x16_t Blocks[4];
		uint8x16_t Key;
		size_t iBatch = iBlocks < 4u ? iBlocks : 4u;

		for ( size_t i = 0; i < iBatch; i++ ) {
			Blocks[i] = vld1q_u8(
				pInput + (i * XRT_AES_BLOCK_SIZE)
			);
		}
		for ( uint32 i = 0; i < (pState->Rounds - 1u); i++ ) {
			Key = vld1q_u8(
				pState->RoundKey + ((size_t)i * XRT_AES_BLOCK_SIZE)
			);
			for ( size_t j = 0; j < iBatch; j++ ) {
				Blocks[j] = vaesmcq_u8(vaeseq_u8(Blocks[j], Key));
			}
		}
		Key = vld1q_u8(
			pState->RoundKey +
			((size_t)(pState->Rounds - 1u) * XRT_AES_BLOCK_SIZE)
		);
		for ( size_t i = 0; i < iBatch; i++ ) {
			Blocks[i] = vaeseq_u8(Blocks[i], Key);
		}
		Key = vld1q_u8(
			pState->RoundKey +
			((size_t)pState->Rounds * XRT_AES_BLOCK_SIZE)
		);
		for ( size_t i = 0; i < iBatch; i++ ) {
			Blocks[i] = veorq_u8(Blocks[i], Key);
			vst1q_u8(
				pOutput + (i * XRT_AES_BLOCK_SIZE),
				Blocks[i]
			);
		}
		pInput += iBatch * XRT_AES_BLOCK_SIZE;
		pOutput += iBatch * XRT_AES_BLOCK_SIZE;
		iBlocks -= iBatch;
	}
}



/* 使用正向轮密钥和 ARMv8 AES 逆变换解密一个完整块。 */
static XRT_AES_ARM_TARGET void __xrtAesDecryptArm(
	const xaes* pState,
	const uint8* pInput,
	uint8* pOutput
)
{
	uint8x16_t Block = vld1q_u8(pInput);
	uint8x16_t Key;

	for ( uint32 i = pState->Rounds; i > 1u; i-- ) {
		Key = vld1q_u8(
			pState->RoundKey + ((size_t)i * XRT_AES_BLOCK_SIZE)
		);
		/* 首轮直接加入末轮密钥，其他逆序轮密钥必须先逆混列。 */
		if ( i != pState->Rounds ) {
			Key = vaesimcq_u8(Key);
		}
		Block = vaesimcq_u8(vaesdq_u8(Block, Key));
	}
	Key = vld1q_u8(pState->RoundKey + XRT_AES_BLOCK_SIZE);
	Key = vaesimcq_u8(Key);
	Block = vaesdq_u8(Block, Key);
	Block = veorq_u8(Block, vld1q_u8(pState->RoundKey));
	vst1q_u8(pOutput, Block);
}

#endif



/* 批量转换、处理并恢复最多四个 AES 块。 */
static void __xrtAesCryptBlocks(
	const uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS],
	uint32 iRounds,
	const uint8* pInput,
	uint8* pOutput,
	size_t iBlocks,
	bool bEncrypt
)
{
	while ( iBlocks != 0 ) {
		uint64 Q[8];
		uint32 Words[16];
		size_t iBatch = iBlocks < 4u ? iBlocks : 4u;

		memset(Words, 0, sizeof(Words));
		for ( size_t i = 0; i < iBatch; i++ ) {
			for ( size_t j = 0; j < 4; j++ ) {
				Words[(i * 4u) + j] = __xrtCryptoLoadLe32(
					pInput + (i * XRT_AES_BLOCK_SIZE) + (j * 4u)
				);
			}
			__xrtAesInterleaveIn(&Q[i], &Q[i + 4u], Words + (i * 4u));
		}
		for ( size_t i = iBatch; i < 4; i++ ) {
			Q[i] = 0;
			Q[i + 4u] = 0;
		}
		__xrtAesOrtho(Q);
		if ( bEncrypt ) {
			__xrtAesEncryptBitslice(pExpanded, iRounds, Q);
		} else {
			__xrtAesDecryptBitslice(pExpanded, iRounds, Q);
		}
		__xrtAesOrtho(Q);
		for ( size_t i = 0; i < iBatch; i++ ) {
			__xrtAesInterleaveOut(Words + (i * 4u), Q[i], Q[i + 4u]);
			for ( size_t j = 0; j < 4; j++ ) {
				__xrtCryptoStoreLe32(
					pOutput + (i * XRT_AES_BLOCK_SIZE) + (j * 4u),
					Words[(i * 4u) + j]
				);
			}
		}
		pInput += iBatch * XRT_AES_BLOCK_SIZE;
		pOutput += iBatch * XRT_AES_BLOCK_SIZE;
		iBlocks -= iBatch;
		xrtSecureZero(Words, sizeof(Words));
		xrtSecureZero(Q, sizeof(Q));
	}
}



/* 使用已经展开的位切片轮密钥批量加密完整块。 */
void __xrtAesEncryptBlocks(
	const xaes* pState,
	const uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS],
	const uint8* pInput,
	uint8* pOutput,
	size_t iBlocks
)
{
	#if XRT_AES_X86_HARDWARE
		if ( (pState->Backend & XRT_INTERNAL_AES_BACKEND_AESNI) != 0 ) {
			__xrtAesEncryptHardware(
				pState, pInput, pOutput, iBlocks
			);
			return;
		}
	#endif
	#if XRT_AES_ARM_HARDWARE
		if ( (pState->Backend & XRT_INTERNAL_AES_BACKEND_ARM_AES) != 0 ) {
			__xrtAesEncryptArm(pState, pInput, pOutput, iBlocks);
			return;
		}
	#endif
	__xrtAesCryptBlocks(
		pExpanded, pState->Rounds, pInput, pOutput, iBlocks, true
	);
}



/* 根据密钥长度生成标准 AES-128/192/256 正向轮密钥。 */
XRT_API bool xrtAesInit(xaes* pState, const void* pKey, size_t iKeySize)
{
	xaes Next;
	size_t iGenerated;
	size_t iRequired;
	uint32 iRounds;
	uint8 iRcon = 1u;

	if ( (pState == NULL) || (pKey == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iKeySize == XRT_AES128_KEY_SIZE ) {
		iRounds = 10u;
	} else if ( iKeySize == XRT_AES192_KEY_SIZE ) {
		iRounds = 12u;
	} else if ( iKeySize == XRT_AES256_KEY_SIZE ) {
		iRounds = 14u;
	} else {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	memcpy(Next.RoundKey, pKey, iKeySize);
	iGenerated = iKeySize;
	iRequired = ((size_t)iRounds + 1u) * XRT_AES_BLOCK_SIZE;
	while ( iGenerated < iRequired ) {
		uint32 iWord = __xrtCryptoLoadLe32(Next.RoundKey + iGenerated - 4u);
		size_t iOffset = iGenerated % iKeySize;

		if ( iOffset == 0 ) {
			iWord = (iWord << 24u) | (iWord >> 8u);
			iWord = __xrtAesSubWord(iWord) ^ iRcon;
			iRcon = (uint8)((iRcon << 1u) ^
				((uint8)(0u - (uint8)(iRcon >> 7u)) & 0x1Bu));
		} else if ( (iKeySize == XRT_AES256_KEY_SIZE) && (iOffset == 16u) ) {
			iWord = __xrtAesSubWord(iWord);
		}
		iWord ^= __xrtCryptoLoadLe32(Next.RoundKey + iGenerated - iKeySize);
		__xrtCryptoStoreLe32(Next.RoundKey + iGenerated, iWord);
		iGenerated += 4u;
	}
	Next.Guard = XRT_AES_GUARD;
	Next.Rounds = iRounds;
	Next.Backend = __xrtAesHardwareFeaturesCached();
	*pState = Next;
	xrtSecureZero(&Next, sizeof(Next));
	return true;
}



/* 清除调用方持有的 AES 轮密钥。 */
XRT_API void xrtAesClear(xaes* pState)
{
	if ( pState != NULL ) {
		xrtSecureZero(pState, sizeof(*pState));
	}
}



/* 校验块参数，并拒绝状态、输入和输出之间的危险重叠。 */
static bool __xrtAesValidateBlock(
	const xaes* pState,
	const void* pInput,
	void* pOutput
)
{
	if ( (pState == NULL) || (pInput == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pInput, XRT_AES_BLOCK_SIZE
		)) || (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pOutput, XRT_AES_BLOCK_SIZE
		)) || ((pInput != pOutput) && (__xrtCryptoRangesOverlap(
			pInput, XRT_AES_BLOCK_SIZE, pOutput, XRT_AES_BLOCK_SIZE
		))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 以失败不写输出的方式处理一个 AES 块。 */
static bool __xrtAesBlock(
	const xaes* pState,
	const void* pInput,
	void* pOutput,
	bool bEncrypt
)
{
	uint64 Expanded[XRT_INTERNAL_AES_EXPANDED_WORDS];
	uint8 Result[XRT_AES_BLOCK_SIZE];
	bool bResult = false;

	if ( !__xrtAesValidateBlock(pState, pInput, pOutput) ) {
		goto cleanup;
	}
	#if XRT_AES_X86_HARDWARE
		if ( __xrtAesHasBackend(
			pState, XRT_INTERNAL_AES_BACKEND_AESNI
		) ) {
			if ( bEncrypt ) {
				__xrtAesEncryptHardware(
					pState, (const uint8*)pInput, Result, 1u
				);
			} else {
				__xrtAesDecryptHardware(
					pState, (const uint8*)pInput, Result
				);
			}
		} else
	#endif
	#if XRT_AES_ARM_HARDWARE
		if ( __xrtAesHasBackend(
			pState, XRT_INTERNAL_AES_BACKEND_ARM_AES
		) ) {
			if ( bEncrypt ) {
				__xrtAesEncryptArm(
					pState, (const uint8*)pInput, Result, 1u
				);
			} else {
				__xrtAesDecryptArm(
					pState, (const uint8*)pInput, Result
				);
			}
		} else
	#endif
	{
		if ( !__xrtAesExpand(pState, Expanded) ) {
			goto cleanup;
		}
		__xrtAesCryptBlocks(
			Expanded,
			pState->Rounds,
			(const uint8*)pInput,
			Result,
			1u,
			bEncrypt
		);
	}
	memcpy(pOutput, Result, sizeof(Result));
	bResult = true;

cleanup:
	xrtSecureZero(Result, sizeof(Result));
	xrtSecureZero(Expanded, sizeof(Expanded));
	return bResult;
}



/* 加密一个 AES 块。 */
XRT_API bool xrtAesEncrypt(
	const xaes* pState,
	const void* pInput,
	void* pOutput
)
{
	return __xrtAesBlock(pState, pInput, pOutput, true);
}



/* 解密一个 AES 块。 */
XRT_API bool xrtAesDecrypt(
	const xaes* pState,
	const void* pInput,
	void* pOutput
)
{
	return __xrtAesBlock(pState, pInput, pOutput, false);
}



#undef XRT_AES_GUARD

#endif



#undef XRT_AES_TARGET
#undef XRT_AES_X86_HARDWARE
#undef XRT_AES_ARM_TARGET
#undef XRT_AES_ARM_HARDWARE
