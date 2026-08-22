#include "../internal/xrt_crypto_25519.h"



#if defined(XRT_FEATURE_CRYPTO_CURVE25519)

/*
	25519 有限域算术保留自旧版 XRT 使用的 Mike Hamburg / STROBE 实现。
	Copyright (c) 2015-2016 Cryptography Research, Inc.
	MIT License。
*/

#define XRT_25519_LIMB_BITS 32u

typedef uint64 __xrt25519double;



/* 执行一个 32 x 32 位乘加并返回低半部。 */
static uint32 __xrt25519MultiplyAdd(
	uint32* pCarry,
	uint32 iAdd,
	uint32 iLeft,
	uint32 iRight
)
{
	__xrt25519double iValue =
		((__xrt25519double)iLeft * iRight) + iAdd + *pCarry;

	*pCarry = (uint32)(iValue >> XRT_25519_LIMB_BITS);
	return (uint32)iValue;
}



/* 执行两个 32 位字和进位的加法。 */
static uint32 __xrt25519AddCarry(
	uint32* pCarry,
	uint32 iLeft,
	uint32 iRight
)
{
	__xrt25519double iValue =
		(__xrt25519double)*pCarry + iLeft + iRight;

	*pCarry = (uint32)(iValue >> XRT_25519_LIMB_BITS);
	return (uint32)iValue;
}



/* 执行一个 32 位字和进位的加法。 */
static uint32 __xrt25519Carry(uint32* pCarry, uint32 iValue)
{
	__xrt25519double iTotal = (__xrt25519double)*pCarry + iValue;

	*pCarry = (uint32)(iTotal >> XRT_25519_LIMB_BITS);
	return (uint32)iTotal;
}



/* 按 2^255 = 19 折叠最高位和外部进位。 */
static void __xrt25519Propagate(
	__xrt25519field Value,
	uint32 iOverflow
)
{
	uint32 iCarry;

	iOverflow = (Value[XRT_INTERNAL_25519_LIMBS - 1u] >> 31u) |
		(iOverflow << 1u);
	Value[XRT_INTERNAL_25519_LIMBS - 1u] &= UINT32_C(0x7FFFFFFF);
	iCarry = iOverflow * 19u;
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		Value[i] = __xrt25519Carry(&iCarry, Value[i]);
	}
}



/* 计算完整或单 limb 右乘数的乘积。 */
static void __xrt25519MultiplyLimbs(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const uint32* pRight,
	size_t iRightLimbs
)
{
	uint32 Accumulator[XRT_INTERNAL_25519_LIMBS * 2u] = { 0 };
	uint32 iCarry;

	for ( size_t i = 0; i < iRightLimbs; i++ ) {
		iCarry = 0;
		for ( size_t j = 0; j < XRT_INTERNAL_25519_LIMBS; j++ ) {
			Accumulator[i + j] = __xrt25519MultiplyAdd(
				&iCarry, Accumulator[i + j], pRight[i], pLeft[j]
			);
		}
		Accumulator[i + XRT_INTERNAL_25519_LIMBS] = iCarry;
	}

	iCarry = 0;
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		pOutput[i] = __xrt25519MultiplyAdd(
			&iCarry,
			Accumulator[i],
			38u,
			Accumulator[i + XRT_INTERNAL_25519_LIMBS]
		);
	}
	__xrt25519Propagate(pOutput, iCarry);
	xrtSecureZero(Accumulator, sizeof(Accumulator));
}



/* 把有限域元素清零。 */
void __xrt25519Zero(__xrt25519field Value)
{
	memset(Value, 0, sizeof(__xrt25519field));
}



/* 把有限域元素设为一。 */
void __xrt25519One(__xrt25519field Value)
{
	__xrt25519Zero(Value);
	Value[0] = 1u;
}



/* 复制一个有限域元素。 */
void __xrt25519Copy(
	__xrt25519field pOutput,
	const __xrt25519field pInput
)
{
	memcpy(pOutput, pInput, sizeof(__xrt25519field));
}



/* 计算两个有限域元素之和。 */
void __xrt25519Add(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
)
{
	uint32 iCarry = 0;

	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		pOutput[i] = __xrt25519AddCarry(
			&iCarry, pLeft[i], pRight[i]
		);
	}
	__xrt25519Propagate(pOutput, iCarry);
}



/* 计算两个有限域元素之差。 */
void __xrt25519Subtract(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
)
{
	uint32 iBorrow = 38u;

	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		uint32 iLeft = pLeft[i];
		__xrt25519double iSubtract =
			(__xrt25519double)pRight[i] + iBorrow;
		uint32 iSubtractLow = (uint32)iSubtract;

		pOutput[i] = iLeft - iSubtractLow;
		iBorrow = (uint32)(iSubtract >> XRT_25519_LIMB_BITS) +
			(iLeft < iSubtractLow ? 1u : 0u);
	}
	__xrt25519Propagate(pOutput, 1u - iBorrow);
}



/* 计算两个有限域元素之积。 */
void __xrt25519Multiply(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
)
{
	__xrt25519MultiplyLimbs(
		pOutput, pLeft, pRight, XRT_INTERNAL_25519_LIMBS
	);
}



/* 计算有限域元素与一个 32 位小整数之积。 */
void __xrt25519MultiplySmall(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	uint32 iRight
)
{
	__xrt25519MultiplyLimbs(pOutput, pLeft, &iRight, 1u);
}



/* 计算有限域元素的平方。 */
void __xrt25519Square(
	__xrt25519field pOutput,
	const __xrt25519field pInput
)
{
	__xrt25519Multiply(pOutput, pInput, pInput);
}



/* 以常数时间掩码选择两个有限域元素。 */
void __xrt25519Select(
	__xrt25519field pOutput,
	const __xrt25519field pFalse,
	const __xrt25519field pTrue,
	uint32 iMask
)
{
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		pOutput[i] = pFalse[i] ^ ((pFalse[i] ^ pTrue[i]) & iMask);
	}
}



/* 以全零或全一掩码交换两个有限域元素。 */
void __xrt25519Swap(
	__xrt25519field pLeft,
	__xrt25519field pRight,
	uint32 iMask
)
{
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		uint32 iDifference = (pLeft[i] ^ pRight[i]) & iMask;

		pLeft[i] ^= iDifference;
		pRight[i] ^= iDifference;
	}
}



/* 从 32 字节小端序列读取元素，并清除编码最高位。 */
void __xrt25519Load(
	__xrt25519field pOutput,
	const uint8 pInput[32]
)
{
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		pOutput[i] = __xrtCryptoLoadLe32(pInput + (i * 4u));
	}
	pOutput[XRT_INTERNAL_25519_LIMBS - 1u] &= UINT32_C(0x7FFFFFFF);
}



/* 把元素归约为唯一的 32 字节小端编码。 */
void __xrt25519Store(
	uint8 pOutput[32],
	const __xrt25519field pInput
)
{
	__xrt25519field Value;

	__xrt25519Copy(Value, pInput);
	(void)__xrt25519Canonical(Value);
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		__xrtCryptoStoreLe32(pOutput + (i * 4u), Value[i]);
	}
	pOutput[31] &= 0x7Fu;
	xrtSecureZero(Value, sizeof(Value));
}



/* 原位归约元素，并返回归约结果是否非零。 */
bool __xrt25519Canonical(__xrt25519field Value)
{
	uint32 iCarry = 19u;
	uint32 iBorrow = 19u;
	uint32 iNonZero = 0;

	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		Value[i] = __xrt25519Carry(&iCarry, Value[i]);
	}
	__xrt25519Propagate(Value, iCarry);
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		uint32 iValue = Value[i];

		Value[i] = iValue - iBorrow;
		iBorrow = iValue < iBorrow ? 1u : 0u;
		iNonZero |= Value[i];
	}
	return iNonZero != 0;
}



#undef XRT_25519_LIMB_BITS

#endif
