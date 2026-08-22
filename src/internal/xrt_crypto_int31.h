#ifndef XRT_INTERNAL_CRYPTO_INT31_H
#define XRT_INTERNAL_CRYPTO_INT31_H

#include "xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_INT31)

#define XRT_I31_WORD_MASK UINT32_C(0x7FFFFFFF)



/* 对一个 0 或 1 控制位取反。 */
static inline uint32 __xrtI31Not(uint32 iControl)
{
	return iControl ^ 1u;
}



/* 按控制位选择左值或右值，不对数据分支。 */
static inline uint32 __xrtI31Select(
	uint32 iControl,
	uint32 iLeft,
	uint32 iRight
)
{
	return iRight ^ ((0u - iControl) & (iLeft ^ iRight));
}



/* 比较两个无符号字是否相等。 */
static inline uint32 __xrtI31Equal(uint32 iLeft, uint32 iRight)
{
	uint32 iValue = iLeft ^ iRight;

	return __xrtI31Not((iValue | (0u - iValue)) >> 31u);
}



/* 比较两个无符号字是否不相等。 */
static inline uint32 __xrtI31NotEqual(uint32 iLeft, uint32 iRight)
{
	uint32 iValue = iLeft ^ iRight;

	return (iValue | (0u - iValue)) >> 31u;
}



/* 比较两个无符号字的大小。 */
static inline uint32 __xrtI31Greater(uint32 iLeft, uint32 iRight)
{
	uint32 iValue = iRight - iLeft;

	return (iValue ^ ((iLeft ^ iRight) & (iLeft ^ iValue))) >> 31u;
}



/* 返回 -1、0 或 1，表示左值小于、等于或大于右值。 */
static inline int32 __xrtI31Compare(uint32 iLeft, uint32 iRight)
{
	return (int32)__xrtI31Greater(iLeft, iRight) |
		(0 - (int32)__xrtI31Greater(iRight, iLeft));
}



/* 按控制位复制一段内存，访问轨迹不依赖数据内容。 */
void __xrtI31Copy(
	uint32 iControl,
	void* pDestination,
	const void* pSource,
	size_t iSize
);



/* 清空一个整数，并写入编码后的公告位数。 */
void __xrtI31Zero(uint32* pValue, uint32 iBitLength);



/* 从大端字节串解码一个无符号整数。 */
void __xrtI31Decode(uint32* pValue, const void* pSource, size_t iSize);



/* 把整数右移 0 到 30 位。 */
void __xrtI31RightShift(uint32* pValue, int iBits);



/* 条件执行同长度整数加法，并返回最高进位。 */
uint32 __xrtI31Add(uint32* pLeft, const uint32* pRight, uint32 iControl);



/* 条件执行同长度整数减法，并返回最高借位。 */
uint32 __xrtI31Subtract(uint32* pLeft, const uint32* pRight, uint32 iControl);



/* 判断一个整数是否为零。 */
uint32 __xrtI31IsZero(const uint32* pValue);



/* 从大端字节串解码一个严格小于模数的整数。 */
uint32 __xrtI31DecodeMod(
	uint32* pValue,
	const void* pSource,
	size_t iSize,
	const uint32* pModulus
);



/* 把任意长度的大端整数归约到给定奇模数，不使用数据相关除法。 */
void __xrtI31ReduceBytes(
	uint32* pValue,
	const void* pSource,
	size_t iSize,
	const uint32* pModulus
);



/* 把整数编码为指定长度的大端字节串。 */
void __xrtI31Encode(void* pDestination, size_t iSize, const uint32* pValue);



/* 把两个整数的乘积累加到已清零或预置低位值的目标整数。 */
void __xrtI31MultiplyAdd(
	uint32* pDestination,
	const uint32* pLeft,
	const uint32* pRight
);



/* 计算模数最低字在 2^31 下的负逆元。 */
uint32 __xrtI31NegativeInverse(uint32 iValue);



/* 通过固定次数模加倍计算 Montgomery R^2。 */
void __xrtI31MontgomerySquare(
	uint32* pSquare,
	const uint32* pModulus
);



/* 执行 31 位字 Montgomery 模乘。 */
void __xrtI31MontgomeryMultiply(
	uint32* pDestination,
	const uint32* pLeft,
	const uint32* pRight,
	const uint32* pModulus,
	uint32 iModulusInverse
);



/* 从 Montgomery 表示转换回普通模整数。 */
void __xrtI31FromMontgomery(
	uint32* pValue,
	const uint32* pModulus,
	uint32 iModulusInverse
);



/* 使用预计算 R^2 执行固定轨迹模幂。 */
void __xrtI31ModPower(
	uint32* pValue,
	const uint8* pExponent,
	size_t iExponentSize,
	const uint32* pModulus,
	const uint32* pMontgomerySquare,
	uint32 iModulusInverse,
	uint32* pTemporaryLeft,
	uint32* pTemporaryRight
);

#endif

#endif
