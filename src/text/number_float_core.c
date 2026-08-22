/*
	浮点转换核心精炼自 yyjson 0.12.0 的数值读取与写出实现。
	Copyright (c) 2020 YaoYuan <ibireme@gmail.com>
	SPDX-License-Identifier: MIT

	XRT 只保留整数运算、BigInt 后备、Schubfach 和共用十进制幂表。
	公开语法、显式长度、错误和内存契约由 number_float.c 负责。
*/
#include "../internal/xrt_number.h"
#include "../third_party/yyjson/yyjson_number_table.h"



#if defined(XRT_FEATURE_NUMBER_FLOAT)

#define XRT_NUMBER_F64_BITS			64
#define XRT_NUMBER_F64_EXP_BITS		11
#define XRT_NUMBER_F64_SIG_BITS		52
#define XRT_NUMBER_F64_FULL_BITS	53
#define XRT_NUMBER_F64_EXP_BIAS		1023
#define XRT_NUMBER_F64_MAX_BIN_EXP	1024
#define XRT_NUMBER_F64_MIN_BIN_EXP	(-1021)
#define XRT_NUMBER_POWER10_MIN		(-343)
#define XRT_NUMBER_POWER10_MAX		324
#define XRT_NUMBER_U64_MAX			UINT64_MAX
#define XRT_NUMBER_F64_INF			UINT64_C(0x7FF0000000000000)
#define XRT_NUMBER_F64_SIG_MASK		UINT64_C(0x000FFFFFFFFFFFFF)
#define XRT_NUMBER_F64_EXP_MASK		UINT64_C(0x7FF0000000000000)



/* BigInt 只服务 double 精确舍入；当前上限最多使用 58 个字。 */
typedef struct xrt_number_bigint {
	uint32 Used;
	uint64 Bits[64];
} xrt_number_bigint;



/* 二进制近似数用于精确路径的候选值计算。 */
typedef struct xrt_number_diy {
	uint64 Significand;
	int32 Exponent;
} xrt_number_diy;



/* double 解析与写出共用的小十进制幂。 */
static const uint64 __xrtNumberUIntPower10[20] = {
	UINT64_C(1),
	UINT64_C(10),
	UINT64_C(100),
	UINT64_C(1000),
	UINT64_C(10000),
	UINT64_C(100000),
	UINT64_C(1000000),
	UINT64_C(10000000),
	UINT64_C(100000000),
	UINT64_C(1000000000),
	UINT64_C(10000000000),
	UINT64_C(100000000000),
	UINT64_C(1000000000000),
	UINT64_C(10000000000000),
	UINT64_C(100000000000000),
	UINT64_C(1000000000000000),
	UINT64_C(10000000000000000),
	UINT64_C(100000000000000000),
	UINT64_C(1000000000000000000),
	UINT64_C(10000000000000000000)
};



/* 在不违反别名规则的情况下取得 double 位模式。 */
static uint64 __xrtNumberFloatBits(double fValue)
{
	uint64 iBits;

	memcpy(&iBits, &fValue, sizeof(iBits));
	return iBits;
}



/* 返回非零 64 位整数的前导零位数。 */
static uint32 __xrtNumberLeadingZeros(uint64 iValue)
{
#if defined(__GNUC__) || defined(__clang__)
	return (uint32)__builtin_clzll((unsigned long long)iValue);
#else
	uint32 iCount = 0;

	if ( iValue <= UINT64_C(0x00000000FFFFFFFF) ) {
		iCount += 32u;
		iValue <<= 32u;
	}
	if ( iValue <= UINT64_C(0x0000FFFFFFFFFFFF) ) {
		iCount += 16u;
		iValue <<= 16u;
	}
	if ( iValue <= UINT64_C(0x00FFFFFFFFFFFFFF) ) {
		iCount += 8u;
		iValue <<= 8u;
	}
	if ( iValue <= UINT64_C(0x0FFFFFFFFFFFFFFF) ) {
		iCount += 4u;
		iValue <<= 4u;
	}
	if ( iValue <= UINT64_C(0x3FFFFFFFFFFFFFFF) ) {
		iCount += 2u;
		iValue <<= 2u;
	}
	if ( iValue <= UINT64_C(0x7FFFFFFFFFFFFFFF) ) {
		iCount++;
	}
	return iCount;
#endif
}



/* 返回非零 64 位整数的尾随零位数。 */
static uint32 __xrtNumberTrailingZeros(uint64 iValue)
{
#if defined(__GNUC__) || defined(__clang__)
	return (uint32)__builtin_ctzll((unsigned long long)iValue);
#else
	uint32 iCount = 0;

	if ( (iValue & UINT64_C(0x00000000FFFFFFFF)) == 0 ) {
		iCount += 32u;
		iValue >>= 32u;
	}
	if ( (iValue & UINT64_C(0x000000000000FFFF)) == 0 ) {
		iCount += 16u;
		iValue >>= 16u;
	}
	if ( (iValue & UINT64_C(0x00000000000000FF)) == 0 ) {
		iCount += 8u;
		iValue >>= 8u;
	}
	if ( (iValue & UINT64_C(0x000000000000000F)) == 0 ) {
		iCount += 4u;
		iValue >>= 4u;
	}
	if ( (iValue & UINT64_C(0x0000000000000003)) == 0 ) {
		iCount += 2u;
		iValue >>= 2u;
	}
	if ( (iValue & UINT64_C(0x0000000000000001)) == 0 ) {
		iCount++;
	}
	return iCount;
#endif
}



/* 计算两个 64 位无符号整数的完整 128 位乘积。 */
static void __xrtNumberMultiply128(
	uint64 iLeft,
	uint64 iRight,
	uint64* pHigh,
	uint64* pLow
)
{
#if defined(__SIZEOF_INT128__)
	__uint128_t iResult = (__uint128_t)iLeft * (__uint128_t)iRight;

	*pHigh = (uint64)(iResult >> 64u);
	*pLow = (uint64)iResult;
#else
	uint32 iLeftLow = (uint32)iLeft;
	uint32 iLeftHigh = (uint32)(iLeft >> 32u);
	uint32 iRightLow = (uint32)iRight;
	uint32 iRightHigh = (uint32)(iRight >> 32u);
	uint64 iProduct00 = (uint64)iLeftLow * (uint64)iRightLow;
	uint64 iProduct01 = (uint64)iLeftLow * (uint64)iRightHigh;
	uint64 iProduct10 = (uint64)iLeftHigh * (uint64)iRightLow;
	uint64 iProduct11 = (uint64)iLeftHigh * (uint64)iRightHigh;
	uint64 iMiddle0 = iProduct01 + (iProduct00 >> 32u);
	uint32 iMiddle00 = (uint32)iMiddle0;
	uint32 iMiddle01 = (uint32)(iMiddle0 >> 32u);
	uint64 iMiddle1 = iProduct10 + (uint64)iMiddle00;
	uint32 iMiddle10 = (uint32)iMiddle1;
	uint32 iMiddle11 = (uint32)(iMiddle1 >> 32u);

	*pHigh = iProduct11 + (uint64)iMiddle01 + (uint64)iMiddle11;
	*pLow = ((uint64)iMiddle10 << 32u) | (uint64)(uint32)iProduct00;
#endif
}



/* 计算两个 64 位整数的乘积并向完整 128 位结果加一个 64 位值。 */
static void __xrtNumberMultiplyAdd128(
	uint64 iLeft,
	uint64 iRight,
	uint64 iAdd,
	uint64* pHigh,
	uint64* pLow
)
{
#if defined(__SIZEOF_INT128__)
	__uint128_t iResult = ((__uint128_t)iLeft * (__uint128_t)iRight) +
		(__uint128_t)iAdd;

	*pHigh = (uint64)(iResult >> 64u);
	*pLow = (uint64)iResult;
#else
	uint64 iHigh;
	uint64 iLow;
	uint64 iSum;

	__xrtNumberMultiply128(iLeft, iRight, &iHigh, &iLow);
	iSum = iLow + iAdd;
	iHigh += (uint64)((iSum < iLow) || (iSum < iAdd));
	*pHigh = iHigh;
	*pLow = iSum;
#endif
}



/* 从共用表读取 10 的指定幂所对应的高低 64 位有效数。 */
static void __xrtNumberPower10(
	int32 iExponent,
	uint64* pHigh,
	uint64* pLow
)
{
	int32 iIndex = iExponent - XRT_NUMBER_POWER10_MIN;

	*pHigh = __xrtNumberPower10Table[iIndex * 2];
	*pLow = __xrtNumberPower10Table[(iIndex * 2) + 1];
}



/* 计算共用十进制幂表高 64 位有效数对应的二进制指数。 */
static int32 __xrtNumberPower10Exponent(int32 iExponent)
{
	return ((iExponent * 217706) - 4128768) >> 16;
}



/* BigInt 加一个 64 位无符号整数。 */
static void __xrtNumberBigAdd(xrt_number_bigint* pBig, uint64 iValue)
{
	uint32 iIndex;
	uint32 iMaximum;
	uint64 iNumber = pBig->Bits[0];
	uint64 iSum = iNumber + iValue;

	pBig->Bits[0] = iSum;
	if ( (iSum >= iNumber) || (iSum >= iValue) ) {
		return;
	}
	iMaximum = pBig->Used;
	for ( iIndex = 1; iIndex < iMaximum; iIndex++ ) {
		if ( pBig->Bits[iIndex] != XRT_NUMBER_U64_MAX ) {
			pBig->Bits[iIndex]++;
			return;
		}
		pBig->Bits[iIndex] = 0;
	}
	pBig->Bits[pBig->Used++] = 1;
}



/* BigInt 乘一个非零 64 位无符号整数。 */
static void __xrtNumberBigMultiply(xrt_number_bigint* pBig, uint64 iValue)
{
	uint32 iIndex = 0;
	uint32 iMaximum = pBig->Used;
	uint64 iCarry = 0;

	while ( (iIndex < iMaximum) && (pBig->Bits[iIndex] == 0) ) {
		iIndex++;
	}
	while ( iIndex < iMaximum ) {
		uint64 iHigh;
		uint64 iLow;

		__xrtNumberMultiplyAdd128(
			pBig->Bits[iIndex], iValue, iCarry, &iHigh, &iLow);
		pBig->Bits[iIndex] = iLow;
		iCarry = iHigh;
		iIndex++;
	}
	if ( iCarry != 0 ) {
		pBig->Bits[pBig->Used++] = iCarry;
	}
}



/* BigInt 左移指定二进制位数。 */
static void __xrtNumberBigMultiplyPower2(
	xrt_number_bigint* pBig,
	uint32 iExponent
)
{
	uint32 iShift = iExponent % 64u;
	uint32 iMove = iExponent / 64u;
	uint32 iIndex = pBig->Used;

	if ( iShift == 0 ) {
		while ( iIndex > 0 ) {
			pBig->Bits[iIndex + iMove - 1] = pBig->Bits[iIndex - 1];
			iIndex--;
		}
		pBig->Used += iMove;
		while ( iMove > 0 ) {
			pBig->Bits[--iMove] = 0;
		}
		return;
	}

	pBig->Bits[iIndex] = 0;
	while ( iIndex > 0 ) {
		uint64 iNumber = pBig->Bits[iIndex] << iShift;

		iNumber |= pBig->Bits[iIndex - 1] >> (64u - iShift);
		pBig->Bits[iIndex + iMove] = iNumber;
		iIndex--;
	}
	pBig->Bits[iMove] = pBig->Bits[0] << iShift;
	pBig->Used += iMove + (pBig->Bits[pBig->Used + iMove] > 0);
	while ( iMove > 0 ) {
		pBig->Bits[--iMove] = 0;
	}
}



/* BigInt 乘 10 的非负整数幂。 */
static void __xrtNumberBigMultiplyPower10(
	xrt_number_bigint* pBig,
	int32 iExponent
)
{
	while ( iExponent >= 19 ) {
		__xrtNumberBigMultiply(pBig, __xrtNumberUIntPower10[19]);
		iExponent -= 19;
	}
	if ( iExponent != 0 ) {
		__xrtNumberBigMultiply(
			pBig, __xrtNumberUIntPower10[iExponent]);
	}
}



/* 比较两个非负 BigInt。 */
static int32 __xrtNumberBigCompare(
	const xrt_number_bigint* pLeft,
	const xrt_number_bigint* pRight
)
{
	uint32 iIndex = pLeft->Used;

	if ( pLeft->Used < pRight->Used ) {
		return -1;
	}
	if ( pLeft->Used > pRight->Used ) {
		return 1;
	}
	while ( iIndex > 0 ) {
		uint64 iLeft;
		uint64 iRight;

		iIndex--;
		iLeft = pLeft->Bits[iIndex];
		iRight = pRight->Bits[iIndex];
		if ( iLeft < iRight ) {
			return -1;
		}
		if ( iLeft > iRight ) {
			return 1;
		}
	}
	return 0;
}



/* 用一个 64 位值初始化 BigInt。 */
static void __xrtNumberBigSet(xrt_number_bigint* pBig, uint64 iValue)
{
	pBig->Used = 1;
	pBig->Bits[0] = iValue;
}



/* 从最多 769 个十进制有效数字构造精确比较所需的 BigInt。 */
static void __xrtNumberBigSetDigits(
	xrt_number_bigint* pBig,
	const uint8* pDigits,
	uint32 iDigitCount
)
{
	uint32 iIndex = 0;
	uint32 iChunkSize = 0;
	uint64 iChunk = 0;

	__xrtNumberBigSet(pBig, 0);
	while ( iIndex < iDigitCount ) {
		iChunk = (iChunk * UINT64_C(10)) +
			(uint64)(pDigits[iIndex] - (uint8)'0');
		iChunkSize++;
		iIndex++;
		if ( (iChunkSize == 19u) || (iIndex == iDigitCount) ) {
			__xrtNumberBigMultiplyPower10(pBig, (int32)iChunkSize);
			__xrtNumberBigAdd(pBig, iChunk);
			iChunk = 0;
			iChunkSize = 0;
		}
	}
}



/* 从共用表取得已舍入的 10 的幂近似数。 */
static xrt_number_diy __xrtNumberDiyPower10(int32 iExponent)
{
	xrt_number_diy Result;
	uint64 iLow;

	__xrtNumberPower10(iExponent, &Result.Significand, &iLow);
	Result.Exponent = __xrtNumberPower10Exponent(iExponent);
	Result.Significand += iLow >> 63u;
	return Result;
}



/* 乘两个 DIY 浮点数并保留经过舍入的高 64 位。 */
static xrt_number_diy __xrtNumberDiyMultiply(
	xrt_number_diy Left,
	xrt_number_diy Right
)
{
	uint64 iHigh;
	uint64 iLow;

	__xrtNumberMultiply128(
		Left.Significand, Right.Significand, &iHigh, &iLow);
	Left.Significand = iHigh + (iLow >> 63u);
	Left.Exponent += Right.Exponent + 64;
	return Left;
}



/* 把 DIY 候选值编码为 IEEE-754 double 正数位模式。 */
static uint64 __xrtNumberDiyBits(xrt_number_diy Value)
{
	uint64 iSignificand = Value.Significand;
	int32 iExponent = Value.Exponent;
	uint32 iLeading;

	if ( iSignificand == 0 ) {
		return 0;
	}
	iLeading = __xrtNumberLeadingZeros(iSignificand);
	iSignificand <<= iLeading;
	iSignificand >>= XRT_NUMBER_F64_BITS - XRT_NUMBER_F64_FULL_BITS;
	iExponent -= (int32)iLeading;
	iExponent += XRT_NUMBER_F64_BITS - XRT_NUMBER_F64_FULL_BITS;
	iExponent += XRT_NUMBER_F64_SIG_BITS;

	if ( iExponent >= XRT_NUMBER_F64_MAX_BIN_EXP ) {
		return XRT_NUMBER_F64_INF;
	}
	if ( iExponent >= (XRT_NUMBER_F64_MIN_BIN_EXP - 1) ) {
		iExponent += XRT_NUMBER_F64_EXP_BIAS;
		return ((uint64)iExponent << XRT_NUMBER_F64_SIG_BITS) |
			(iSignificand & XRT_NUMBER_F64_SIG_MASK);
	}
	if ( iExponent >=
		(XRT_NUMBER_F64_MIN_BIN_EXP - XRT_NUMBER_F64_FULL_BITS) ) {
		return iSignificand >>
			(XRT_NUMBER_F64_MIN_BIN_EXP - iExponent - 1);
	}
	return 0;
}



/*
	尝试 Eisel-Lemire 风格的纯整数快速路径。
	只有舍入方向能够由已知高位唯一确定时才发布结果。
*/
static bool __xrtNumberFloatFast(
	uint64 iSignificand,
	int32 iExponent,
	uint64* pBits
)
{
	uint64 iPowerHigh;
	uint64 iPowerLow;
	uint64 iInput;
	uint64 iHigh;
	uint64 iLow;
	uint64 iHigh2;
	uint64 iLow2;
	uint64 iAdd;
	uint64 iKnown;
	int32 iBinaryExponent;
	uint32 iLeading;
	bool bExact = false;
	bool bCarry;
	bool bRoundUp;

	__xrtNumberPower10(iExponent, &iPowerHigh, &iPowerLow);
	iBinaryExponent = __xrtNumberPower10Exponent(iExponent);
	iLeading = __xrtNumberLeadingZeros(iSignificand);
	iInput = iSignificand << iLeading;
	iBinaryExponent -= (int32)iLeading;
	__xrtNumberMultiply128(iInput, iPowerHigh, &iHigh, &iLow);

	iKnown = iHigh & ((UINT64_C(1) << 9u) - UINT64_C(1));
	if ( (iKnown - UINT64_C(1)) <
		((UINT64_C(1) << 9u) - UINT64_C(2)) ) {
		bExact = true;
	} else {
		__xrtNumberMultiply128(iInput, iPowerLow, &iHigh2, &iLow2);
		iAdd = iLow + iHigh2;
		if ( (iAdd + UINT64_C(1)) > UINT64_C(1) ) {
			bCarry = (iAdd < iLow) || (iAdd < iHigh2);
			iHigh += (uint64)bCarry;
			bExact = true;
		}
	}
	if ( !bExact ) {
		return false;
	}

	iLeading = iHigh < (UINT64_C(1) << 63u);
	iHigh <<= iLeading;
	iBinaryExponent -= (int32)iLeading;
	iBinaryExponent += 64;
	bRoundUp = (iHigh & (UINT64_C(1) << 10u)) != 0;
	if ( bRoundUp ) {
		iHigh += UINT64_C(1) << 10u;
	}
	if ( iHigh < (UINT64_C(1) << 10u) ) {
		iHigh = UINT64_C(1) << 63u;
		iBinaryExponent++;
	}

	iHigh >>= XRT_NUMBER_F64_BITS - XRT_NUMBER_F64_FULL_BITS;
	iBinaryExponent +=
		XRT_NUMBER_F64_BITS - XRT_NUMBER_F64_FULL_BITS +
		XRT_NUMBER_F64_SIG_BITS + XRT_NUMBER_F64_EXP_BIAS;
	*pBits = ((uint64)iBinaryExponent << XRT_NUMBER_F64_SIG_BITS) |
		(iHigh & XRT_NUMBER_F64_SIG_MASK);
	return true;
}



/* 用 BigInt 比较候选值上边界，完成罕见的精确舍入判定。 */
static bool __xrtNumberFloatExact(
	const uint8* pDigits,
	uint32 iDigitCount,
	int32 iDigitExponent,
	uint64 iSignificand,
	int32 iSignificandExponent,
	uint64* pBits
)
{
	const int32 iErrorLog = 3;
	const uint64 iErrorUnit = UINT64_C(1) << 3u;
	uint64 iError;
	uint32 iLeading;
	int32 iOrder;
	int32 iEffectiveBits;
	int32 iPrecisionCount;
	uint64 iPrecision;
	uint64 iHalf;
	uint64 iRaw;
	xrt_number_diy Value;
	xrt_number_diy Upper;
	xrt_number_bigint Full;
	xrt_number_bigint Compare;
	int32 iComparison;

	Value.Significand = iSignificand;
	Value.Exponent = 0;
	iError = (iDigitCount > 19u) ? (iErrorUnit / 2u) : 0;

	iLeading = __xrtNumberLeadingZeros(Value.Significand);
	Value.Significand <<= iLeading;
	Value.Exponent -= (int32)iLeading;
	iError <<= iLeading;

	Value = __xrtNumberDiyMultiply(
		Value, __xrtNumberDiyPower10(iSignificandExponent));
	iError += (iErrorUnit / 2u) + (iError != 0) + (iErrorUnit / 2u);

	iLeading = __xrtNumberLeadingZeros(Value.Significand);
	Value.Significand <<= iLeading;
	Value.Exponent -= (int32)iLeading;
	iError <<= iLeading;

	iOrder = 64 + Value.Exponent;
	if ( iOrder >=
		(-1074 + XRT_NUMBER_F64_FULL_BITS) ) {
		iEffectiveBits = XRT_NUMBER_F64_FULL_BITS;
	} else if ( iOrder <= -1074 ) {
		iEffectiveBits = 0;
	} else {
		iEffectiveBits = iOrder + 1074;
	}

	iPrecisionCount = 64 - iEffectiveBits;
	if ( (iPrecisionCount + iErrorLog) >= 64 ) {
		int32 iShift =
			(iPrecisionCount + iErrorLog) - 64 + 1;

		Value.Significand >>= iShift;
		Value.Exponent += iShift;
		iError = (iError >> iShift) + UINT64_C(1) + iErrorUnit;
		iPrecisionCount -= iShift;
	}

	iPrecision = Value.Significand &
		((UINT64_C(1) << iPrecisionCount) - UINT64_C(1));
	iPrecision *= iErrorUnit;
	iHalf = (UINT64_C(1) << (iPrecisionCount - 1)) * iErrorUnit;

	Value.Significand >>= iPrecisionCount;
	Value.Significand += (iPrecision >= (iHalf + iError));
	Value.Exponent += iPrecisionCount;
	iRaw = __xrtNumberDiyBits(Value);
	if ( iRaw == XRT_NUMBER_F64_INF ) {
		return false;
	}
	if ( (iPrecision <= (iHalf - iError)) ||
		(iPrecision >= (iHalf + iError)) ) {
		*pBits = iRaw;
		return true;
	}

	if ( (iRaw & XRT_NUMBER_F64_EXP_MASK) != 0 ) {
		Upper.Significand =
			(iRaw & XRT_NUMBER_F64_SIG_MASK) +
			(UINT64_C(1) << XRT_NUMBER_F64_SIG_BITS);
		Upper.Exponent = (int32)(
			(iRaw & XRT_NUMBER_F64_EXP_MASK) >>
			XRT_NUMBER_F64_SIG_BITS);
	} else {
		Upper.Significand = iRaw & XRT_NUMBER_F64_SIG_MASK;
		Upper.Exponent = 1;
	}
	Upper.Exponent -=
		XRT_NUMBER_F64_EXP_BIAS + XRT_NUMBER_F64_SIG_BITS;
	Upper.Significand <<= 1u;
	Upper.Exponent--;
	Upper.Significand++;

	__xrtNumberBigSetDigits(&Full, pDigits, iDigitCount);
	__xrtNumberBigSet(&Compare, Upper.Significand);
	if ( iDigitExponent >= 0 ) {
		__xrtNumberBigMultiplyPower10(&Full, iDigitExponent);
	} else {
		__xrtNumberBigMultiplyPower10(&Compare, -iDigitExponent);
	}
	if ( Upper.Exponent > 0 ) {
		__xrtNumberBigMultiplyPower2(
			&Compare, (uint32)Upper.Exponent);
	} else {
		__xrtNumberBigMultiplyPower2(
			&Full, (uint32)-Upper.Exponent);
	}
	iComparison = __xrtNumberBigCompare(&Full, &Compare);
	if ( iComparison != 0 ) {
		iRaw += (iComparison > 0);
	} else {
		iRaw += iRaw & UINT64_C(1);
	}
	if ( iRaw == XRT_NUMBER_F64_INF ) {
		return false;
	}
	*pBits = iRaw;
	return true;
}



/* 把归一化十进制有效数字转换成正确舍入的正 double 位模式。 */
bool __xrtNumberFloatConvert(
	const uint8* pDigits,
	uint32 iDigitCount,
	int32 iDigitExponent,
	uint64 iSignificand,
	int32 iSignificandExponent,
	uint64* pBits
)
{
	if ( (pDigits == NULL) || (pBits == NULL) ||
		(iDigitCount == 0) || (iDigitCount > 769u) ||
		(iSignificand == 0) ||
		(iSignificandExponent < XRT_NUMBER_POWER10_MIN) ||
		(iSignificandExponent > XRT_NUMBER_POWER10_MAX) ) {
		return false;
	}
	if ( (iDigitCount <= 19u) &&
		(iSignificandExponent > -307) &&
		(iSignificandExponent < 288) &&
		__xrtNumberFloatFast(
			iSignificand, iSignificandExponent, pBits) ) {
		return true;
	}
	return __xrtNumberFloatExact(
		pDigits,
		iDigitCount,
		iDigitExponent,
		iSignificand,
		iSignificandExponent,
		pBits
	);
}



/* 64 位乘法后取最高 64 位，并把被丢弃部分合并为奇数舍入位。 */
static uint64 __xrtNumberRoundOdd128(
	uint64 iHigh,
	uint64 iLow,
	uint64 iMultiplier
)
{
	uint64 iXHigh;
	uint64 iXLow;
	uint64 iYHigh;
	uint64 iYLow;

	__xrtNumberMultiply128(iMultiplier, iLow, &iXHigh, &iXLow);
	__xrtNumberMultiplyAdd128(
		iMultiplier, iHigh, iXHigh, &iYHigh, &iYLow);
	return iYHigh | (uint64)(iYLow > UINT64_C(1));
}



/*
	用 Schubfach 把非零有限 double 转成最短十进制有效数和指数。
	结果可能带尾随零，调用方在排版前统一裁掉。
*/
static void __xrtNumberBinaryToDecimal(
	uint64 iRawSignificand,
	uint32 iRawExponent,
	uint64 iBinarySignificand,
	int32 iBinaryExponent,
	uint64* pDecimalSignificand,
	int32* pDecimalExponent
)
{
	bool bEven;
	bool bIrregular;
	bool bRoundUp;
	bool bTrim;
	bool bU0Inside;
	bool bU1Inside;
	bool bW0Inside;
	bool bW1Inside;
	uint64 iValue;
	uint64 iShort;
	uint64 iCenter;
	uint64 iLeft;
	uint64 iRight;
	uint64 iScaled;
	uint64 iScaledLeft;
	uint64 iScaledRight;
	uint64 iPowerHigh;
	uint64 iPowerLow;
	uint64 iUpper;
	uint64 iLower;
	uint64 iMiddle;
	int32 iDecimalExponent;
	int32 iShift;

	while ( iRawSignificand != 0 ) {
		uint64 iRemainder;
		uint64 iDecimal;
		uint64 iAddOne;
		uint64 iAddTen;
		uint64 iScaledHigh;
		uint64 iScaledLow;
		uint64 iFraction;
		uint64 iHalfUlp;
		uint64 iTen;
		uint64 iSum;

		iDecimalExponent = (iBinaryExponent * 315653) >> 20;
		iShift = iBinaryExponent +
			((-iDecimalExponent * 217707) >> 16);
		__xrtNumberPower10(
			-iDecimalExponent, &iPowerHigh, &iPowerLow);

		iCenter = iBinarySignificand << (iShift + 1);
		__xrtNumberMultiply128(
			iCenter, iPowerLow, &iScaledHigh, &iScaledLow);
		__xrtNumberMultiplyAdd128(
			iCenter, iPowerHigh, iScaledHigh,
			&iScaledHigh, &iScaledLow);
		iRemainder = iScaledHigh % UINT64_C(10);
		iDecimal = iScaledHigh - iRemainder;

		iFraction = (iRemainder << 60u) | (iScaledLow >> 4u);
		iHalfUlp = iPowerHigh >> (4 - iShift);
		bW1Inside = iScaledLow >= (UINT64_C(1) << 63u);
		if ( iScaledLow == (UINT64_C(1) << 63u) ) {
			break;
		}
		bU0Inside = iHalfUlp >= iFraction;
		if ( iHalfUlp == iFraction ) {
			break;
		}
		iTen = UINT64_C(10) << 60u;
		iSum = iFraction + iHalfUlp;
		bW0Inside = iSum >= iTen;
		if ( (iTen - iSum) <= UINT64_C(1) ) {
			break;
		}

		bTrim = bU0Inside || bW0Inside;
		iAddTen = bW0Inside ? UINT64_C(10) : 0;
		iAddOne = iRemainder + (uint64)bW1Inside;
		*pDecimalSignificand =
			iDecimal + (bTrim ? iAddTen : iAddOne);
		*pDecimalExponent = iDecimalExponent;
		return;
	}

	bIrregular =
		(iRawSignificand == 0) && (iRawExponent > 1u);
	bEven = (iBinarySignificand & UINT64_C(1)) == 0;
	iLeft = (UINT64_C(4) * iBinarySignificand) -
		UINT64_C(2) + (uint64)bIrregular;
	iCenter = UINT64_C(4) * iBinarySignificand;
	iRight = (UINT64_C(4) * iBinarySignificand) + UINT64_C(2);

	iDecimalExponent =
		((iBinaryExponent * 315653) -
		(bIrregular ? 131237 : 0)) >> 20;
	iShift = iBinaryExponent +
		((-iDecimalExponent * 217707) >> 16) + 1;
	__xrtNumberPower10(
		-iDecimalExponent, &iPowerHigh, &iPowerLow);
	iPowerLow++;

	iScaledLeft = __xrtNumberRoundOdd128(
		iPowerHigh, iPowerLow, iLeft << iShift);
	iScaled = __xrtNumberRoundOdd128(
		iPowerHigh, iPowerLow, iCenter << iShift);
	iScaledRight = __xrtNumberRoundOdd128(
		iPowerHigh, iPowerLow, iRight << iShift);
	iLower = iScaledLeft + (uint64)!bEven;
	iUpper = iScaledRight - (uint64)!bEven;

	iValue = iScaled / UINT64_C(4);
	if ( iValue >= UINT64_C(10) ) {
		iShort = iValue / UINT64_C(10);
		bU0Inside = iLower <= (UINT64_C(40) * iShort);
		bW0Inside =
			iUpper >= ((UINT64_C(40) * iShort) + UINT64_C(40));
		if ( bU0Inside != bW0Inside ) {
			*pDecimalSignificand =
				(iShort * UINT64_C(10)) +
				(bW0Inside ? UINT64_C(10) : 0);
			*pDecimalExponent = iDecimalExponent;
			return;
		}
	}
	bU1Inside = iLower <= (UINT64_C(4) * iValue);
	bW1Inside =
		iUpper >= ((UINT64_C(4) * iValue) + UINT64_C(4));
	iMiddle = (UINT64_C(4) * iValue) + UINT64_C(2);
	bRoundUp = (iScaled > iMiddle) ||
		((iScaled == iMiddle) && ((iValue & UINT64_C(1)) != 0));
	*pDecimalSignificand = iValue +
		(uint64)((bU1Inside != bW1Inside) ? bW1Inside : bRoundUp);
	*pDecimalExponent = iDecimalExponent;
}



/* 把一个非零无符号整数写入临时十进制数字区并返回起点。 */
static char* __xrtNumberFloatDigits(uint64 iValue, char* sEnd)
{
	do {
		*--sEnd = (char)('0' + (char)(iValue % UINT64_C(10)));
		iValue /= UINT64_C(10);
	} while ( iValue != 0 );
	return sEnd;
}



/* 写出范围在 -324 到 308 之间的科学计数法指数。 */
static char* __xrtNumberFloatExponent(int32 iExponent, char* sOutput)
{
	char sDigits[4];
	char* sEnd = sDigits + sizeof(sDigits);
	char* sStart;
	uint32 iMagnitude;

	*sOutput++ = 'e';
	if ( iExponent < 0 ) {
		*sOutput++ = '-';
		iMagnitude = (uint32)-iExponent;
	} else {
		*sOutput++ = '+';
		iMagnitude = (uint32)iExponent;
	}
	sStart = __xrtNumberFloatDigits((uint64)iMagnitude, sEnd);
	memcpy(sOutput, sStart, (size_t)(sEnd - sStart));
	return sOutput + (sEnd - sStart);
}



/* 根据固定或科学计数法阈值排版已经裁掉尾零的十进制有效数。 */
static size_t __xrtNumberFloatLayout(
	uint64 iSignificand,
	int32 iExponent,
	char* sOutput,
	bool bCompact
)
{
	char sDigits[20];
	char* sEnd = sDigits + sizeof(sDigits);
	char* sStart = __xrtNumberFloatDigits(iSignificand, sEnd);
	int32 iDigitCount = (int32)(sEnd - sStart);
	int32 iDot = iDigitCount + iExponent;
	char* sCursor = sOutput;

	if ( (iDot > -6) && (iDot <= 21) ) {
		if ( iDot <= 0 ) {
			*sCursor++ = '0';
			*sCursor++ = '.';
			memset(sCursor, '0', (size_t)-iDot);
			sCursor += -iDot;
			memcpy(sCursor, sStart, (size_t)iDigitCount);
			sCursor += iDigitCount;
		} else if ( iDot >= iDigitCount ) {
			memcpy(sCursor, sStart, (size_t)iDigitCount);
			sCursor += iDigitCount;
			memset(sCursor, '0', (size_t)(iDot - iDigitCount));
			sCursor += iDot - iDigitCount;
			if ( !bCompact ) {
				*sCursor++ = '.';
				*sCursor++ = '0';
			}
		} else {
			memcpy(sCursor, sStart, (size_t)iDot);
			sCursor += iDot;
			*sCursor++ = '.';
			memcpy(sCursor, sStart + iDot,
				(size_t)(iDigitCount - iDot));
			sCursor += iDigitCount - iDot;
		}
		return (size_t)(sCursor - sOutput);
	}

	*sCursor++ = *sStart++;
	iDigitCount--;
	if ( iDigitCount > 0 ) {
		*sCursor++ = '.';
		memcpy(sCursor, sStart, (size_t)iDigitCount);
		sCursor += iDigitCount;
	}
	sCursor = __xrtNumberFloatExponent(
		iDot - 1, sCursor);
	return (size_t)(sCursor - sOutput);
}



/* 把 double 写成稳定、最短且能够精确往返的文本。 */
size_t __xrtNumberFloatFormat(
	double fValue,
	char* sOutput,
	bool bCompact
)
{
	uint64 iRaw = __xrtNumberFloatBits(fValue);
	bool bNegative = (iRaw >> 63u) != 0;
	uint64 iRawSignificand = iRaw & XRT_NUMBER_F64_SIG_MASK;
	uint32 iRawExponent = (uint32)(
		(iRaw & XRT_NUMBER_F64_EXP_MASK) >>
		XRT_NUMBER_F64_SIG_BITS);
	char* sCursor = sOutput;
	uint64 iBinarySignificand;
	int32 iBinaryExponent;
	uint64 iDecimalSignificand;
	int32 iDecimalExponent;
	size_t iSize;

	if ( iRawExponent == ((1u << XRT_NUMBER_F64_EXP_BITS) - 1u) ) {
		if ( iRawSignificand != 0 ) {
			memcpy(sCursor, "nan", 3);
			return 3;
		}
		if ( bNegative ) {
			*sCursor++ = '-';
		}
		memcpy(sCursor, "inf", 3);
		return (size_t)(sCursor - sOutput) + 3u;
	}
	if ( bNegative ) {
		*sCursor++ = '-';
	}
	if ( (iRaw << 1u) == 0 ) {
		if ( bCompact ) {
			*sCursor++ = '0';
		} else {
			memcpy(sCursor, "0.0", 3);
			sCursor += 3;
		}
		return (size_t)(sCursor - sOutput);
	}

	if ( iRawExponent != 0 ) {
		iBinarySignificand = iRawSignificand |
			(UINT64_C(1) << XRT_NUMBER_F64_SIG_BITS);
		iBinaryExponent = (int32)iRawExponent -
			XRT_NUMBER_F64_EXP_BIAS - XRT_NUMBER_F64_SIG_BITS;
		if ( (iBinaryExponent >= -XRT_NUMBER_F64_SIG_BITS) &&
			(iBinaryExponent <= 0) &&
			(__xrtNumberTrailingZeros(iBinarySignificand) >=
			(uint32)-iBinaryExponent) ) {
			iDecimalSignificand =
				iBinarySignificand >> -iBinaryExponent;
			iDecimalExponent = 0;
			iSize = __xrtNumberFloatLayout(
				iDecimalSignificand,
				iDecimalExponent,
				sCursor,
				bCompact
			);
			return (size_t)(sCursor - sOutput) + iSize;
		}
	} else {
		iBinarySignificand = iRawSignificand;
		iBinaryExponent =
			1 - XRT_NUMBER_F64_EXP_BIAS - XRT_NUMBER_F64_SIG_BITS;
	}

	__xrtNumberBinaryToDecimal(
		iRawSignificand,
		iRawExponent,
		iBinarySignificand,
		iBinaryExponent,
		&iDecimalSignificand,
		&iDecimalExponent
	);
	while ( (iDecimalSignificand % UINT64_C(10)) == 0 ) {
		iDecimalSignificand /= UINT64_C(10);
		iDecimalExponent++;
	}
	iSize = __xrtNumberFloatLayout(
		iDecimalSignificand,
		iDecimalExponent,
		sCursor,
		bCompact
	);
	return (size_t)(sCursor - sOutput) + iSize;
}

#endif
