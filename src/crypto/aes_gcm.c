/*
	GHASH 常量时间乘法改编自 BearSSL 0.6 的 ghash_ctmul64 实现。
	Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>, MIT License.
*/

#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_AES_GCM) && !defined(__TINYC__) && \
	(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
	 defined(_M_X64) || defined(_M_AMD64)) && \
	(defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
	#define XRT_GHASH_X86_HARDWARE 1
	#include <tmmintrin.h>
	#include <wmmintrin.h>
	#if defined(_MSC_VER)
		#define XRT_GHASH_TARGET
	#else
		#define XRT_GHASH_TARGET __attribute__((target("ssse3,pclmul")))
	#endif
#else
	#define XRT_GHASH_X86_HARDWARE 0
	#define XRT_GHASH_TARGET
#endif



#if defined(XRT_FEATURE_CRYPTO_AES_GCM) && !defined(__TINYC__) && \
	defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
	#define XRT_GHASH_ARM_HARDWARE 1
	#include <arm_neon.h>
	#define XRT_GHASH_ARM_TARGET __attribute__((target("+crypto")))
#else
	#define XRT_GHASH_ARM_HARDWARE 0
	#define XRT_GHASH_ARM_TARGET
#endif



#if defined(XRT_FEATURE_CRYPTO_AES_GCM)

#define XRT_AES_GCM_GUARD UINT32_C(0x4147434D)



/* 保存一次 GHASH 计算的累加值和预处理哈希子密钥。 */
typedef struct __xrtghash {
	uint8 HardwareValue[XRT_AES_BLOCK_SIZE];
	uint8 HardwareHash[XRT_AES_BLOCK_SIZE];
	uint64 Y0;
	uint64 Y1;
	uint64 H0;
	uint64 H1;
	uint64 H2;
	uint64 H0Reverse;
	uint64 H1Reverse;
	uint64 H2Reverse;
	uint32 Backend;
} __xrtghash;



/* 判断标签长度是否属于 NIST SP 800-38D 允许的七个值。 */
static bool __xrtAesGcmValidTagSize(size_t iTagSize)
{
	return (iTagSize == 4u) || (iTagSize == 8u) ||
		((iTagSize >= 12u) && (iTagSize <= 16u));
}



/* 查询当前状态是否选择了已经验证的 PCLMUL GHASH 后端。 */
static uint32 __xrtAesGcmHardwareHash(const xaesgcm* pState)
{
	#if XRT_GHASH_X86_HARDWARE
		if ( (pState->Cipher.Backend &
			 XRT_INTERNAL_AES_BACKEND_PCLMUL) != 0 ) {
			return XRT_INTERNAL_AES_BACKEND_PCLMUL;
		}
	#endif
	#if XRT_GHASH_ARM_HARDWARE
		if ( (pState->Cipher.Backend &
			 XRT_INTERNAL_AES_BACKEND_ARM_PMULL) != 0 ) {
			return XRT_INTERNAL_AES_BACKEND_ARM_PMULL;
		}
	#endif
	(void)pState;
	return 0;
}



/* 反转一个 64 位字的全部位，供 GHASH 的反序位定义使用。 */
static uint64 __xrtGhashReverse64(uint64 iValue)
{
	iValue = ((iValue & UINT64_C(0x5555555555555555)) << 1u) |
		((iValue >> 1u) & UINT64_C(0x5555555555555555));
	iValue = ((iValue & UINT64_C(0x3333333333333333)) << 2u) |
		((iValue >> 2u) & UINT64_C(0x3333333333333333));
	iValue = ((iValue & UINT64_C(0x0F0F0F0F0F0F0F0F)) << 4u) |
		((iValue >> 4u) & UINT64_C(0x0F0F0F0F0F0F0F0F));
	iValue = ((iValue & UINT64_C(0x00FF00FF00FF00FF)) << 8u) |
		((iValue >> 8u) & UINT64_C(0x00FF00FF00FF00FF));
	iValue = ((iValue & UINT64_C(0x0000FFFF0000FFFF)) << 16u) |
		((iValue >> 16u) & UINT64_C(0x0000FFFF0000FFFF));
	return (iValue << 32u) | (iValue >> 32u);
}



/* 以普通整数乘法中的空洞位计算一个常量时间无进位乘积。 */
static uint64 __xrtGhashMultiply64(uint64 x, uint64 y)
{
	uint64 x0 = x & UINT64_C(0x1111111111111111);
	uint64 x1 = x & UINT64_C(0x2222222222222222);
	uint64 x2 = x & UINT64_C(0x4444444444444444);
	uint64 x3 = x & UINT64_C(0x8888888888888888);
	uint64 y0 = y & UINT64_C(0x1111111111111111);
	uint64 y1 = y & UINT64_C(0x2222222222222222);
	uint64 y2 = y & UINT64_C(0x4444444444444444);
	uint64 y3 = y & UINT64_C(0x8888888888888888);
	uint64 z0 = (x0 * y0) ^ (x1 * y3) ^ (x2 * y2) ^ (x3 * y1);
	uint64 z1 = (x0 * y1) ^ (x1 * y0) ^ (x2 * y3) ^ (x3 * y2);
	uint64 z2 = (x0 * y2) ^ (x1 * y1) ^ (x2 * y0) ^ (x3 * y3);
	uint64 z3 = (x0 * y3) ^ (x1 * y2) ^ (x2 * y1) ^ (x3 * y0);

	z0 &= UINT64_C(0x1111111111111111);
	z1 &= UINT64_C(0x2222222222222222);
	z2 &= UINT64_C(0x4444444444444444);
	z3 &= UINT64_C(0x8888888888888888);
	return z0 | z1 | z2 | z3;
}



#if XRT_GHASH_ARM_HARDWARE

/* 在一次 ARM Crypto 分派内完成整个字段的 PMULL GHASH。 */
static XRT_GHASH_ARM_TARGET void __xrtGhashHardwareArm(
	__xrtghash* pState,
	const void* pData,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pData;

	while ( iSize != 0 ) {
		uint8 Tail[XRT_AES_BLOCK_SIZE];
		const uint8* pBlock = pRead;
		uint64 y0;
		uint64 y1;
		uint64 y2;
		poly128_t Product0;
		poly128_t Product1;
		poly128_t Product2;
		uint64 z0;
		uint64 z1;
		uint64 z2;
		uint64 z0High;
		uint64 z1High;
		uint64 z2High;
		uint64 v0;
		uint64 v1;
		uint64 v2;
		uint64 v3;

		if ( iSize >= XRT_AES_BLOCK_SIZE ) {
			pRead += XRT_AES_BLOCK_SIZE;
			iSize -= XRT_AES_BLOCK_SIZE;
		} else {
			memset(Tail, 0, sizeof(Tail));
			memcpy(Tail, pRead, iSize);
			pBlock = Tail;
			iSize = 0;
		}

		y1 = pState->Y1 ^ __xrtCryptoLoadBe64(pBlock);
		y0 = pState->Y0 ^ __xrtCryptoLoadBe64(pBlock + 8u);
		y2 = y0 ^ y1;
		Product0 = vmull_p64((poly64_t)y0, (poly64_t)pState->H0);
		Product1 = vmull_p64((poly64_t)y1, (poly64_t)pState->H1);
		Product2 = vmull_p64((poly64_t)y2, (poly64_t)pState->H2);
		z0 = (uint64)Product0;
		z1 = (uint64)Product1;
		z2 = (uint64)Product2 ^ z0 ^ z1;
		z0High = (uint64)(Product0 >> 64u);
		z1High = (uint64)(Product1 >> 64u);
		z2High = (uint64)(Product2 >> 64u) ^ z0High ^ z1High;
		v0 = z0;
		v1 = z0High ^ z2;
		v2 = z1 ^ z2High;
		v3 = z1High;
		v3 = (v3 << 1u) | (v2 >> 63u);
		v2 = (v2 << 1u) | (v1 >> 63u);
		v1 = (v1 << 1u) | (v0 >> 63u);
		v0 <<= 1u;
		v2 ^= v0 ^ (v0 >> 1u) ^ (v0 >> 2u) ^ (v0 >> 7u);
		v1 ^= (v0 << 63u) ^ (v0 << 62u) ^ (v0 << 57u);
		v3 ^= v1 ^ (v1 >> 1u) ^ (v1 >> 2u) ^ (v1 >> 7u);
		v2 ^= (v1 << 63u) ^ (v1 << 62u) ^ (v1 << 57u);
		pState->Y0 = v2;
		pState->Y1 = v3;
		xrtSecureZero(Tail, sizeof(Tail));
	}
}

#endif



#if XRT_GHASH_X86_HARDWARE

/* 把四个 64 位半字组成的 256 位无进位乘积整体左移一位。 */
static XRT_GHASH_TARGET void __xrtGhashShift256(
	__m128i* pX0,
	__m128i* pX1,
	__m128i* pX2,
	__m128i* pX3
)
{
	*pX0 = _mm_or_si128(
		_mm_slli_epi64(*pX0, 1), _mm_srli_epi64(*pX1, 63)
	);
	*pX1 = _mm_or_si128(
		_mm_slli_epi64(*pX1, 1), _mm_srli_epi64(*pX2, 63)
	);
	*pX2 = _mm_or_si128(
		_mm_slli_epi64(*pX2, 1), _mm_srli_epi64(*pX3, 63)
	);
	*pX3 = _mm_slli_epi64(*pX3, 1);
}



/* 按 GCM 多项式把 256 位乘积约简为 128 位。 */
static XRT_GHASH_TARGET void __xrtGhashReduce(
	__m128i* pX0,
	__m128i* pX1,
	__m128i* pX2,
	__m128i* pX3
)
{
	*pX1 = _mm_xor_si128(
		*pX1,
		_mm_xor_si128(
			_mm_xor_si128(*pX3, _mm_srli_epi64(*pX3, 1)),
			_mm_xor_si128(
				_mm_srli_epi64(*pX3, 2), _mm_srli_epi64(*pX3, 7)
			)
		)
	);
	*pX2 = _mm_xor_si128(
		_mm_xor_si128(*pX2, _mm_slli_epi64(*pX3, 63)),
		_mm_xor_si128(
			_mm_slli_epi64(*pX3, 62), _mm_slli_epi64(*pX3, 57)
		)
	);
	*pX0 = _mm_xor_si128(
		*pX0,
		_mm_xor_si128(
			_mm_xor_si128(*pX2, _mm_srli_epi64(*pX2, 1)),
			_mm_xor_si128(
				_mm_srli_epi64(*pX2, 2), _mm_srli_epi64(*pX2, 7)
			)
		)
	);
	*pX1 = _mm_xor_si128(
		_mm_xor_si128(*pX1, _mm_slli_epi64(*pX2, 63)),
		_mm_xor_si128(
			_mm_slli_epi64(*pX2, 62), _mm_slli_epi64(*pX2, 57)
		)
	);
}



/* 使用 PCLMULQDQ 对一个独立补零字段执行 GHASH。 */
static XRT_GHASH_TARGET void __xrtGhashHardware(
	uint8 pValue[XRT_AES_BLOCK_SIZE],
	const uint8 pHash[XRT_AES_BLOCK_SIZE],
	const void* pData,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pData;
	const __m128i Reverse = _mm_set_epi8(
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	);
	__m128i Y = _mm_shuffle_epi8(
		_mm_loadu_si128((const __m128i*)pValue), Reverse
	);
	__m128i H = _mm_shuffle_epi8(
		_mm_loadu_si128((const __m128i*)pHash), Reverse
	);
	__m128i Hx = _mm_xor_si128(H, _mm_shuffle_epi32(H, 0x0E));

	while ( iSize != 0 ) {
		uint8 Tail[XRT_AES_BLOCK_SIZE];
		__m128i A;
		__m128i Ax;
		__m128i x0;
		__m128i x1;
		__m128i x2;
		__m128i x3;

		if ( iSize >= XRT_AES_BLOCK_SIZE ) {
			A = _mm_loadu_si128((const __m128i*)pRead);
			pRead += XRT_AES_BLOCK_SIZE;
			iSize -= XRT_AES_BLOCK_SIZE;
		} else {
			memset(Tail, 0, sizeof(Tail));
			memcpy(Tail, pRead, iSize);
			A = _mm_loadu_si128((const __m128i*)Tail);
			iSize = 0;
		}
		A = _mm_xor_si128(_mm_shuffle_epi8(A, Reverse), Y);
		Ax = _mm_xor_si128(A, _mm_shuffle_epi32(A, 0x0E));
		x1 = _mm_clmulepi64_si128(A, H, 0x11);
		x3 = _mm_clmulepi64_si128(A, H, 0x00);
		x2 = _mm_xor_si128(
			_mm_clmulepi64_si128(Ax, Hx, 0x00),
			_mm_xor_si128(x1, x3)
		);
		x0 = _mm_shuffle_epi32(x1, 0x0E);
		x1 = _mm_xor_si128(x1, _mm_shuffle_epi32(x2, 0x0E));
		x2 = _mm_xor_si128(x2, _mm_shuffle_epi32(x3, 0x0E));
		__xrtGhashShift256(&x0, &x1, &x2, &x3);
		__xrtGhashReduce(&x0, &x1, &x2, &x3);
		Y = _mm_unpacklo_epi64(x1, x0);
		xrtSecureZero(Tail, sizeof(Tail));
	}
	Y = _mm_shuffle_epi8(Y, Reverse);
	_mm_storeu_si128((__m128i*)pValue, Y);
}

#endif



/* 用一个 128 位哈希子密钥初始化 GHASH 累加器。 */
static void __xrtGhashInit(
	__xrtghash* pState,
	const uint8 pHash[XRT_AES_BLOCK_SIZE],
	uint32 iBackend
)
{
	memset(pState, 0, sizeof(*pState));
	pState->Backend = iBackend;
	if ( iBackend == XRT_INTERNAL_AES_BACKEND_PCLMUL ) {
		memcpy(pState->HardwareHash, pHash, XRT_AES_BLOCK_SIZE);
		return;
	}
	pState->H1 = __xrtCryptoLoadBe64(pHash);
	pState->H0 = __xrtCryptoLoadBe64(pHash + 8u);
	pState->H0Reverse = __xrtGhashReverse64(pState->H0);
	pState->H1Reverse = __xrtGhashReverse64(pState->H1);
	pState->H2 = pState->H0 ^ pState->H1;
	pState->H2Reverse = pState->H0Reverse ^ pState->H1Reverse;
}



/* 把一个 128 位块乘以哈希子密钥并约简到 GCM 域。 */
static void __xrtGhashBlock(
	__xrtghash* pState,
	const uint8 pBlock[XRT_AES_BLOCK_SIZE]
)
{
	uint64 y0Reverse;
	uint64 y1Reverse;
	uint64 y2;
	uint64 y2Reverse;
	uint64 z0;
	uint64 z1;
	uint64 z2;
	uint64 z0High;
	uint64 z1High;
	uint64 z2High;
	uint64 v0;
	uint64 v1;
	uint64 v2;
	uint64 v3;

	pState->Y1 ^= __xrtCryptoLoadBe64(pBlock);
	pState->Y0 ^= __xrtCryptoLoadBe64(pBlock + 8u);
	y0Reverse = __xrtGhashReverse64(pState->Y0);
	y1Reverse = __xrtGhashReverse64(pState->Y1);
	y2 = pState->Y0 ^ pState->Y1;
	y2Reverse = y0Reverse ^ y1Reverse;
	z0 = __xrtGhashMultiply64(pState->Y0, pState->H0);
	z1 = __xrtGhashMultiply64(pState->Y1, pState->H1);
	z2 = __xrtGhashMultiply64(y2, pState->H2);
	z0High = __xrtGhashMultiply64(
		y0Reverse, pState->H0Reverse
	);
	z1High = __xrtGhashMultiply64(
		y1Reverse, pState->H1Reverse
	);
	z2High = __xrtGhashMultiply64(
		y2Reverse, pState->H2Reverse
	);
	z2 ^= z0 ^ z1;
	z2High ^= z0High ^ z1High;
	z0High = __xrtGhashReverse64(z0High) >> 1u;
	z1High = __xrtGhashReverse64(z1High) >> 1u;
	z2High = __xrtGhashReverse64(z2High) >> 1u;
	v0 = z0;
	v1 = z0High ^ z2;
	v2 = z1 ^ z2High;
	v3 = z1High;
	v3 = (v3 << 1u) | (v2 >> 63u);
	v2 = (v2 << 1u) | (v1 >> 63u);
	v1 = (v1 << 1u) | (v0 >> 63u);
	v0 <<= 1u;
	v2 ^= v0 ^ (v0 >> 1u) ^ (v0 >> 2u) ^ (v0 >> 7u);
	v1 ^= (v0 << 63u) ^ (v0 << 62u) ^ (v0 << 57u);
	v3 ^= v1 ^ (v1 >> 1u) ^ (v1 >> 2u) ^ (v1 >> 7u);
	v2 ^= (v1 << 63u) ^ (v1 << 62u) ^ (v1 << 57u);
	pState->Y0 = v2;
	pState->Y1 = v3;
}



/* 向 GHASH 追加一个独立、按 16 字节补零的字段。 */
static void __xrtGhashUpdate(
	__xrtghash* pState,
	const void* pData,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pData;

	#if XRT_GHASH_X86_HARDWARE
		if ( pState->Backend == XRT_INTERNAL_AES_BACKEND_PCLMUL ) {
			__xrtGhashHardware(
				pState->HardwareValue,
				pState->HardwareHash,
				pData,
				iSize
			);
			return;
		}
	#endif
	#if XRT_GHASH_ARM_HARDWARE
		if ( pState->Backend == XRT_INTERNAL_AES_BACKEND_ARM_PMULL ) {
			__xrtGhashHardwareArm(pState, pData, iSize);
			return;
		}
	#endif

	while ( iSize >= XRT_AES_BLOCK_SIZE ) {
		__xrtGhashBlock(pState, pRead);
		pRead += XRT_AES_BLOCK_SIZE;
		iSize -= XRT_AES_BLOCK_SIZE;
	}
	if ( iSize != 0 ) {
		uint8 Block[XRT_AES_BLOCK_SIZE];

		memset(Block, 0, sizeof(Block));
		memcpy(Block, pRead, iSize);
		__xrtGhashBlock(pState, Block);
		xrtSecureZero(Block, sizeof(Block));
	}
}



/* 输出当前 GHASH 累加值。 */
static void __xrtGhashFinal(
	const __xrtghash* pState,
	uint8 pOutput[XRT_AES_BLOCK_SIZE]
)
{
	#if XRT_GHASH_X86_HARDWARE
		if ( pState->Backend == XRT_INTERNAL_AES_BACKEND_PCLMUL ) {
			memcpy(pOutput, pState->HardwareValue, XRT_AES_BLOCK_SIZE);
			return;
		}
	#endif
	__xrtCryptoStoreBe64(pOutput, pState->Y1);
	__xrtCryptoStoreBe64(pOutput + 8u, pState->Y0);
}



/* 设置稳定的认证失败错误，不暴露标签差异位置。 */
static void __xrtAesGcmAuthError(const char* pOperation)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = XCRYPTO_ERROR_AUTHENTICATION;
	Desc.Operation = pOperation;
	Desc.Message = "authentication tag verification failed";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证 AES-GCM 状态和 NIST 字节长度边界。 */
static bool __xrtAesGcmValidateState(
	const xaesgcm* pState,
	size_t iNonceSize,
	size_t iAadSize,
	size_t iDataSize
)
{
	if ( (pState == NULL) || (pState->Guard != XRT_AES_GCM_GUARD) ||
		 !__xrtAesGcmValidTagSize(pState->TagSize) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( iNonceSize == 0 ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if SIZE_MAX > UINT32_MAX
		if ( ((uint64)iNonceSize > (UINT64_MAX / 8u)) ||
			 ((uint64)iAadSize > (UINT64_MAX / 8u)) ||
			 ((uint64)iDataSize > XRT_AES_GCM_MAX_SIZE) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
	#else
		(void)iAadSize;
		(void)iDataSize;
	#endif
	return true;
}



/* 验证 detached AEAD 指针、重叠关系和失败原子性前提。 */
static bool __xrtAesGcmValidate(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iDataSize,
	void* pOutput,
	const void* pTag
)
{
	if ( !__xrtAesGcmValidateState(
		pState, iNonceSize, iAadSize, iDataSize
	) ) {
		return false;
	}
	if ( (pNonce == NULL) || (pTag == NULL) ||
		 ((pAad == NULL) && (iAadSize != 0)) ||
		 ((pInput == NULL) && (iDataSize != 0)) ||
		 ((pOutput == NULL) && (iDataSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pNonce, iNonceSize
		)) || (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pAad, iAadSize
		)) || (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pInput, iDataSize
		)) || (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pOutput, iDataSize
		)) || (__xrtCryptoRangesOverlap(
			pState, sizeof(*pState), pTag, pState->TagSize
		)) || (__xrtCryptoRangesOverlap(
			pOutput, iDataSize, pNonce, iNonceSize
		)) || (__xrtCryptoRangesOverlap(
			pOutput, iDataSize, pAad, iAadSize
		)) || ((pOutput != pInput) && (__xrtCryptoRangesOverlap(
			pOutput, iDataSize, pInput, iDataSize
		))) || (__xrtCryptoRangesOverlap(
			pTag, pState->TagSize, pNonce, iNonceSize
		)) || (__xrtCryptoRangesOverlap(
			pTag, pState->TagSize, pAad, iAadSize
		)) || (__xrtCryptoRangesOverlap(
			pTag, pState->TagSize, pInput, iDataSize
		)) || (__xrtCryptoRangesOverlap(
			pTag, pState->TagSize, pOutput, iDataSize
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 按 GCM 的 32 位大端计数规则递增一个计数块。 */
static void __xrtAesGcmIncrement(uint8 pCounter[XRT_AES_BLOCK_SIZE])
{
	uint32 iValue = __xrtCryptoLoadBe32(pCounter + 12u);

	__xrtCryptoStoreBe32(pCounter + 12u, iValue + 1u);
}



/* 由 96 位快路径或通用 GHASH 路径构造初始计数块 J0。 */
static void __xrtAesGcmCounter(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	uint8 pCounter[XRT_AES_BLOCK_SIZE]
)
{
	if ( iNonceSize == XRT_AES_GCM_NONCE_DEFAULT_SIZE ) {
		memcpy(pCounter, pNonce, XRT_AES_GCM_NONCE_DEFAULT_SIZE);
		pCounter[12] = 0;
		pCounter[13] = 0;
		pCounter[14] = 0;
		pCounter[15] = 1;
	} else {
		__xrtghash Hash;
		uint8 Length[XRT_AES_BLOCK_SIZE];

		memset(Length, 0, sizeof(Length));
		__xrtCryptoStoreBe64(Length + 8u, (uint64)iNonceSize * 8u);
		__xrtGhashInit(
			&Hash, pState->Hash, __xrtAesGcmHardwareHash(pState)
		);
		__xrtGhashUpdate(&Hash, pNonce, iNonceSize);
		__xrtGhashUpdate(&Hash, Length, sizeof(Length));
		__xrtGhashFinal(&Hash, pCounter);
		xrtSecureZero(Length, sizeof(Length));
		xrtSecureZero(&Hash, sizeof(Hash));
	}
}



/* 计算 AAD、密文和长度块的完整 16 字节 GCM 标签。 */
static void __xrtAesGcmTag(
	const xaesgcm* pState,
	const uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS],
	const uint8 pCounter[XRT_AES_BLOCK_SIZE],
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	uint8 pTag[XRT_AES_BLOCK_SIZE]
)
{
	__xrtghash Hash;
	uint8 Lengths[XRT_AES_BLOCK_SIZE];
	uint8 Mask[XRT_AES_BLOCK_SIZE];

	__xrtGhashInit(
		&Hash, pState->Hash, __xrtAesGcmHardwareHash(pState)
	);
	__xrtGhashUpdate(&Hash, pAad, iAadSize);
	__xrtGhashUpdate(&Hash, pCipher, iCipherSize);
	__xrtCryptoStoreBe64(Lengths, (uint64)iAadSize * 8u);
	__xrtCryptoStoreBe64(Lengths + 8u, (uint64)iCipherSize * 8u);
	__xrtGhashUpdate(&Hash, Lengths, sizeof(Lengths));
	__xrtGhashFinal(&Hash, pTag);
	__xrtAesEncryptBlocks(
		&pState->Cipher, pExpanded, pCounter, Mask, 1u
	);
	for ( size_t i = 0; i < XRT_AES_BLOCK_SIZE; i++ ) {
		pTag[i] ^= Mask[i];
	}
	xrtSecureZero(Mask, sizeof(Mask));
	xrtSecureZero(Lengths, sizeof(Lengths));
	xrtSecureZero(&Hash, sizeof(Hash));
}



/* 使用四块并行 AES 批次执行 GCM 的 GCTR。 */
static void __xrtAesGcmCrypt(
	const xaesgcm* pState,
	const uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS],
	const uint8 pCounter[XRT_AES_BLOCK_SIZE],
	const void* pInput,
	void* pOutput,
	size_t iSize
)
{
	const uint8* pRead = (const uint8*)pInput;
	uint8* pWrite = (uint8*)pOutput;
	uint8 Current[XRT_AES_BLOCK_SIZE];

	memcpy(Current, pCounter, sizeof(Current));

	while ( iSize != 0 ) {
		uint8 Counter[4u * XRT_AES_BLOCK_SIZE];
		uint8 Stream[4u * XRT_AES_BLOCK_SIZE];
		size_t iBytes = iSize < sizeof(Stream) ? iSize : sizeof(Stream);
		size_t iBlocks = (iBytes + XRT_AES_BLOCK_SIZE - 1u) /
			XRT_AES_BLOCK_SIZE;

		for ( size_t i = 0; i < iBlocks; i++ ) {
			__xrtAesGcmIncrement(Current);
			memcpy(
				Counter + (i * XRT_AES_BLOCK_SIZE),
				Current,
				XRT_AES_BLOCK_SIZE
			);
		}
		__xrtAesEncryptBlocks(
			&pState->Cipher,
			pExpanded,
			Counter,
			Stream,
			iBlocks
		);
		for ( size_t i = 0; i < iBytes; i++ ) {
			pWrite[i] = pRead[i] ^ Stream[i];
		}
		pRead += iBytes;
		pWrite += iBytes;
		iSize -= iBytes;
		xrtSecureZero(Stream, sizeof(Stream));
		xrtSecureZero(Counter, sizeof(Counter));
	}
	xrtSecureZero(Current, sizeof(Current));
}



/* 为软件 AES 展开轮密钥；AES-NI 路径直接使用状态内的标准轮密钥。 */
static bool __xrtAesGcmPrepare(
	const xaesgcm* pState,
	uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS]
)
{
	if ( !__xrtAesHasBackend(&pState->Cipher, 0) ) {
		return false;
	}
	if ( (pState->Cipher.Backend &
		 (XRT_INTERNAL_AES_BACKEND_AESNI |
		  XRT_INTERNAL_AES_BACKEND_ARM_AES)) != 0 ) {
		return true;
	}
	return __xrtAesExpand(&pState->Cipher, pExpanded);
}



/* 初始化固定密钥、固定标签长度的 AES-GCM 状态。 */
XRT_API bool xrtAesGcmInit(
	xaesgcm* pState,
	const void* pKey,
	size_t iKeySize,
	size_t iTagSize
)
{
	xaesgcm Next;
	uint8 Zero[XRT_AES_BLOCK_SIZE];
	bool bResult = false;

	if ( (pState == NULL) || (pKey == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtAesGcmValidTagSize(iTagSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Next, 0, sizeof(Next));
	memset(Zero, 0, sizeof(Zero));
	if ( !xrtAesInit(&Next.Cipher, pKey, iKeySize) ||
		 !xrtAesEncrypt(&Next.Cipher, Zero, Next.Hash) ) {
		goto cleanup;
	}
	Next.Guard = XRT_AES_GCM_GUARD;
	Next.TagSize = (uint32)iTagSize;
	*pState = Next;
	bResult = true;

cleanup:
	xrtSecureZero(Zero, sizeof(Zero));
	xrtSecureZero(&Next, sizeof(Next));
	return bResult;
}



/* 清除调用方持有的 AES-GCM 密钥状态。 */
XRT_API void xrtAesGcmClear(xaesgcm* pState)
{
	if ( pState != NULL ) {
		xrtSecureZero(pState, sizeof(*pState));
	}
}



/* 返回状态固定绑定的标签长度。 */
XRT_API size_t xrtAesGcmTagSize(const xaesgcm* pState)
{
	if ( (pState == NULL) || (pState->Guard != XRT_AES_GCM_GUARD) ||
		 !__xrtAesGcmValidTagSize(pState->TagSize) ) {
		__xrtErrorSetInvalidState();
		return 0;
	}
	return pState->TagSize;
}



/* 加密并生成分离的 GCM 标签。 */
XRT_API bool xrtAesGcmEncrypt(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pCipher,
	void* pTag
)
{
	uint64 Expanded[XRT_INTERNAL_AES_EXPANDED_WORDS];
	uint8 Counter[XRT_AES_BLOCK_SIZE];
	uint8 Tag[XRT_AES_BLOCK_SIZE];
	bool bResult = false;

	if ( !__xrtAesGcmValidate(
		pState, pNonce, iNonceSize, pAad, iAadSize,
		pPlain, iPlainSize, pCipher, pTag
	) || !__xrtAesGcmPrepare(pState, Expanded) ) {
		goto cleanup;
	}
	__xrtAesGcmCounter(pState, pNonce, iNonceSize, Counter);
	__xrtAesGcmCrypt(
		pState, Expanded, Counter, pPlain, pCipher, iPlainSize
	);
	__xrtAesGcmTag(
		pState, Expanded, Counter, pAad, iAadSize,
		pCipher, iPlainSize, Tag
	);
	memcpy(pTag, Tag, pState->TagSize);
	bResult = true;

cleanup:
	xrtSecureZero(Tag, sizeof(Tag));
	xrtSecureZero(Counter, sizeof(Counter));
	xrtSecureZero(Expanded, sizeof(Expanded));
	return bResult;
}



/* 先认证密文，再以 GCTR 写入明文。 */
XRT_API bool xrtAesGcmDecrypt(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	const void* pTag,
	void* pPlain
)
{
	uint64 Expanded[XRT_INTERNAL_AES_EXPANDED_WORDS];
	uint8 Counter[XRT_AES_BLOCK_SIZE];
	uint8 Tag[XRT_AES_BLOCK_SIZE];
	bool bResult = false;

	if ( !__xrtAesGcmValidate(
		pState, pNonce, iNonceSize, pAad, iAadSize,
		pCipher, iCipherSize, pPlain, pTag
	) || !__xrtAesGcmPrepare(pState, Expanded) ) {
		goto cleanup;
	}
	__xrtAesGcmCounter(pState, pNonce, iNonceSize, Counter);
	__xrtAesGcmTag(
		pState, Expanded, Counter, pAad, iAadSize,
		pCipher, iCipherSize, Tag
	);
	if ( !xrtConstTimeEqual(Tag, pTag, pState->TagSize) ) {
		__xrtAesGcmAuthError("aes-gcm-open");
		goto cleanup;
	}
	__xrtAesGcmCrypt(
		pState, Expanded, Counter, pCipher, pPlain, iCipherSize
	);
	bResult = true;

cleanup:
	xrtSecureZero(Tag, sizeof(Tag));
	xrtSecureZero(Counter, sizeof(Counter));
	xrtSecureZero(Expanded, sizeof(Expanded));
	return bResult;
}



/* 把 detached 输出映射为连续的 cipher || tag。 */
XRT_API bool xrtAesGcmSeal(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
)
{
	size_t iTagSize = xrtAesGcmTagSize(pState);
	size_t iRequired;

	if ( iTagSize == 0 ) {
		return false;
	}
	if ( iPlainSize > (SIZE_MAX - iTagSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = iPlainSize + iTagSize;
	if ( pOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	return xrtAesGcmEncrypt(
		pState, pNonce, iNonceSize, pAad, iAadSize,
		pPlain, iPlainSize, pOutput, (uint8*)pOutput + iPlainSize
	);
}



/* 从连续输入末尾分离标签并复用 detached 认证路径。 */
XRT_API bool xrtAesGcmOpen(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
)
{
	size_t iTagSize = xrtAesGcmTagSize(pState);
	size_t iCipherSize;

	if ( iTagSize == 0 ) {
		return false;
	}
	if ( (pInput == NULL) && (iInputSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iInputSize < iTagSize ) {
		__xrtAesGcmAuthError("aes-gcm-open");
		return false;
	}
	iCipherSize = iInputSize - iTagSize;
	if ( (pPlain == NULL) && (iCipherSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iPlainSize < iCipherSize ) {
		__xrtErrorSetRange();
		return false;
	}
	return xrtAesGcmDecrypt(
		pState, pNonce, iNonceSize, pAad, iAadSize,
		pInput, iCipherSize,
		(const uint8*)pInput + iCipherSize,
		pPlain
	);
}



/* 以空明文的 GCM 路径生成 GMAC 标签。 */
XRT_API bool xrtAesGmac(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pData,
	size_t iSize,
	void* pTag
)
{
	return xrtAesGcmEncrypt(
		pState, pNonce, iNonceSize, pData, iSize,
		NULL, 0, NULL, pTag
	);
}



/* 以空密文的 GCM 路径验证 GMAC 标签。 */
XRT_API bool xrtAesGmacVerify(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pData,
	size_t iSize,
	const void* pTag
)
{
	return xrtAesGcmDecrypt(
		pState, pNonce, iNonceSize, pData, iSize,
		NULL, 0, pTag, NULL
	);
}



#undef XRT_AES_GCM_GUARD

#endif



#undef XRT_GHASH_TARGET
#undef XRT_GHASH_X86_HARDWARE
#undef XRT_GHASH_ARM_TARGET
#undef XRT_GHASH_ARM_HARDWARE
