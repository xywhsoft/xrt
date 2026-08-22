#include "../internal/xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_X448)

/*
	X448 域运算保留旧版 XRT 的 14 x 32 位实现骨架。
	新版把归约和减法改为固定轮次无符号运算，并补齐公开契约与敏感数据清理。
*/

#define XRT_X448_LIMBS 14u
#define XRT_X448_LIMB_BITS 32u
#define XRT_X448_FOLD_ROUNDS 3u

typedef uint32 __xrt_x448_limb;
typedef uint64 __xrt_x448_double_limb;
typedef __xrt_x448_limb __xrt_x448_field[XRT_X448_LIMBS];

static const uint8 __xrtX448Base[XRT_X448_PUBLIC_SIZE] = { 5 };
static const __xrt_x448_limb __xrtX448Prime[XRT_X448_LIMBS] = {
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFE),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF),
	UINT32_C(0xFFFFFFFF), UINT32_C(0xFFFFFFFF)
};
static const __xrt_x448_limb __xrtX448A24[1] = { 39081u };



/* 执行一个 32 x 32 位乘加并返回低半部。 */
static __xrt_x448_limb __xrtX448MultiplyAdd(
	__xrt_x448_limb* pCarry,
	__xrt_x448_limb iAdd,
	__xrt_x448_limb iLeft,
	__xrt_x448_limb iRight
)
{
	__xrt_x448_double_limb iValue =
		((__xrt_x448_double_limb)iLeft * iRight) + iAdd + *pCarry;

	*pCarry = (__xrt_x448_limb)(iValue >> XRT_X448_LIMB_BITS);
	return (__xrt_x448_limb)iValue;
}



/* 从指定 limb 起以固定循环长度加入一个外部进位。 */
static __xrt_x448_double_limb __xrtX448AddScalarAt(
	__xrt_x448_field Value,
	size_t iStart,
	__xrt_x448_double_limb iCarry
)
{
	for ( size_t i = iStart; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_double_limb iValue =
			(__xrt_x448_double_limb)Value[i] + (__xrt_x448_limb)iCarry;

		Value[i] = (__xrt_x448_limb)iValue;
		iCarry = (iCarry >> XRT_X448_LIMB_BITS) +
			(iValue >> XRT_X448_LIMB_BITS);
	}
	return iCarry;
}



/* 按 2^448 = 2^224 + 1 固定执行三轮外部进位折叠。 */
static void __xrtX448Fold(
	__xrt_x448_field Value,
	__xrt_x448_double_limb iCarry
)
{
	for ( size_t i = 0; i < XRT_X448_FOLD_ROUNDS; i++ ) {
		__xrt_x448_double_limb iLow =
			__xrtX448AddScalarAt(Value, 0, iCarry);
		__xrt_x448_double_limb iHigh =
			__xrtX448AddScalarAt(Value, 7, iCarry);

		iCarry = iLow + iHigh;
	}
}



/* 以固定次数把小于 2^448 的域元素规范化到 [0, p)。 */
static void __xrtX448Canonical(__xrt_x448_field Value)
{
	__xrt_x448_field Reduced;
	__xrt_x448_limb iBorrow = 0;

	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_limb iLeft = Value[i];
		__xrt_x448_double_limb iSubtract =
			(__xrt_x448_double_limb)__xrtX448Prime[i] + iBorrow;
		__xrt_x448_limb iSubtractLow = (__xrt_x448_limb)iSubtract;

		Reduced[i] = iLeft - iSubtractLow;
		iBorrow = (__xrt_x448_limb)(iSubtract >> XRT_X448_LIMB_BITS) +
			(iLeft < iSubtractLow ? 1u : 0u);
	}

	{
		__xrt_x448_limb iUseReduced = 0u - (1u - iBorrow);

		for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
			Value[i] = (Reduced[i] & iUseReduced) |
				(Value[i] & ~iUseReduced);
		}
	}
}



/* 计算两个规范域元素之和。 */
static void __xrtX448Add(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pLeft,
	const __xrt_x448_field pRight
)
{
	__xrt_x448_double_limb iCarry = 0;

	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_double_limb iValue =
			(__xrt_x448_double_limb)pLeft[i] + pRight[i] + iCarry;

		pOutput[i] = (__xrt_x448_limb)iValue;
		iCarry = iValue >> XRT_X448_LIMB_BITS;
	}
	__xrtX448Fold(pOutput, iCarry);
	__xrtX448Canonical(pOutput);
}



/* 计算两个规范域元素之差，并避免旧实现的无符号下溢传播。 */
static void __xrtX448Subtract(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pLeft,
	const __xrt_x448_field pRight
)
{
	__xrt_x448_field Difference;
	__xrt_x448_limb iBorrow = 0;

	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_limb iLeft = pLeft[i];
		__xrt_x448_double_limb iSubtract =
			(__xrt_x448_double_limb)pRight[i] + iBorrow;
		__xrt_x448_limb iSubtractLow = (__xrt_x448_limb)iSubtract;

		Difference[i] = iLeft - iSubtractLow;
		iBorrow = (__xrt_x448_limb)(iSubtract >> XRT_X448_LIMB_BITS) +
			(iLeft < iSubtractLow ? 1u : 0u);
	}

	{
		__xrt_x448_limb iAddPrime = 0u - iBorrow;
		__xrt_x448_double_limb iCarry = 0;

		for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
			__xrt_x448_double_limb iValue =
				(__xrt_x448_double_limb)Difference[i] +
				(__xrtX448Prime[i] & iAddPrime) + iCarry;

			pOutput[i] = (__xrt_x448_limb)iValue;
			iCarry = iValue >> XRT_X448_LIMB_BITS;
		}
	}
}



/* 计算域元素乘积，允许输出与任一输入重叠。 */
static void __xrtX448Multiply(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pLeft,
	const __xrt_x448_limb* pRight,
	size_t iRightLimbs
)
{
	__xrt_x448_limb Accumulator[XRT_X448_LIMBS * 2u] = { 0 };
	__xrt_x448_double_limb Folded[21] = { 0 };
	__xrt_x448_double_limb iCarry = 0;

	for ( size_t i = 0; i < iRightLimbs; i++ ) {
		__xrt_x448_limb iMultiplyCarry = 0;

		for ( size_t j = 0; j < XRT_X448_LIMBS; j++ ) {
			Accumulator[i + j] = __xrtX448MultiplyAdd(
				&iMultiplyCarry,
				Accumulator[i + j],
				pRight[i],
				pLeft[j]
			);
		}
		Accumulator[i + XRT_X448_LIMBS] = iMultiplyCarry;
	}

	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		Folded[i] += Accumulator[i];
		Folded[i] += Accumulator[i + XRT_X448_LIMBS];
		Folded[i + 7u] += Accumulator[i + XRT_X448_LIMBS];
	}
	for ( size_t i = 21u; i > XRT_X448_LIMBS; i-- ) {
		size_t j = i - 1u;

		Folded[j - 14u] += Folded[j];
		Folded[j - 7u] += Folded[j];
	}

	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_double_limb iValue = Folded[i] + iCarry;

		pOutput[i] = (__xrt_x448_limb)iValue;
		iCarry = iValue >> XRT_X448_LIMB_BITS;
	}
	__xrtX448Fold(pOutput, iCarry);
	__xrtX448Canonical(pOutput);
}



/* 计算域元素平方。 */
static void __xrtX448Square(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pInput
)
{
	__xrtX448Multiply(pOutput, pInput, pInput, XRT_X448_LIMBS);
}



/* 把 pOutput 的旧值作为右乘数。 */
static void __xrtX448MultiplySelf(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pLeft
)
{
	__xrtX448Multiply(pOutput, pLeft, pOutput, XRT_X448_LIMBS);
}



/* 原位计算域元素平方。 */
static void __xrtX448SquareSelf(__xrt_x448_field Value)
{
	__xrtX448MultiplySelf(Value, Value);
}



/* 按全零或全一掩码交换两个域元素。 */
static void __xrtX448Swap(
	__xrt_x448_field pLeft,
	__xrt_x448_field pRight,
	__xrt_x448_limb iMask
)
{
	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrt_x448_limb iValue = (pLeft[i] ^ pRight[i]) & iMask;

		pLeft[i] ^= iValue;
		pRight[i] ^= iValue;
	}
}



/* 计算 z^(p-2)；循环与分支只依赖固定公开指数。 */
static void __xrtX448Inverse(
	__xrt_x448_field pOutput,
	const __xrt_x448_field pInput
)
{
	__xrt_x448_field Base;
	__xrt_x448_field Result = { 0 };

	memcpy(Base, pInput, sizeof(Base));
	Result[0] = 1;
	for ( size_t i = 0; i < 448u; i++ ) {
		if ( (i != 1u) && (i != 224u) ) {
			__xrtX448Multiply(Result, Result, Base, XRT_X448_LIMBS);
		}
		__xrtX448Square(Base, Base);
	}
	memcpy(pOutput, Result, sizeof(Result));
	xrtSecureZero(Base, sizeof(Base));
	xrtSecureZero(Result, sizeof(Result));
}



/* 执行 Montgomery ladder 的前半组域运算。 */
static void __xrtX448LadderFirst(__xrt_x448_field Values[5])
{
	__xrt_x448_limb* pX2 = Values[0];
	__xrt_x448_limb* pZ2 = Values[1];
	__xrt_x448_limb* pX3 = Values[2];
	__xrt_x448_limb* pZ3 = Values[3];
	__xrt_x448_limb* pTemp = Values[4];

	__xrtX448Add(pTemp, pX2, pZ2);
	__xrtX448Subtract(pZ2, pX2, pZ2);
	__xrtX448Add(pX2, pX3, pZ3);
	__xrtX448Subtract(pZ3, pX3, pZ3);
	__xrtX448MultiplySelf(pZ3, pTemp);
	__xrtX448MultiplySelf(pX2, pZ2);
	__xrtX448Add(pX3, pZ3, pX2);
	__xrtX448Subtract(pZ3, pZ3, pX2);
	__xrtX448SquareSelf(pTemp);
	__xrtX448SquareSelf(pZ2);
	__xrtX448Subtract(pX2, pTemp, pZ2);
	__xrtX448Multiply(
		pZ2, pX2, __xrtX448A24,
		sizeof(__xrtX448A24) / sizeof(__xrtX448A24[0])
	);
	__xrtX448Add(pZ2, pZ2, pTemp);
}



/* 执行 Montgomery ladder 的后半组域运算。 */
static void __xrtX448LadderSecond(
	__xrt_x448_field Values[5],
	const __xrt_x448_field pPoint
)
{
	__xrt_x448_limb* pX2 = Values[0];
	__xrt_x448_limb* pZ2 = Values[1];
	__xrt_x448_limb* pX3 = Values[2];
	__xrt_x448_limb* pZ3 = Values[3];
	__xrt_x448_limb* pTemp = Values[4];

	__xrtX448SquareSelf(pZ3);
	__xrtX448MultiplySelf(pZ3, pPoint);
	__xrtX448SquareSelf(pX3);
	__xrtX448MultiplySelf(pZ2, pX2);
	__xrtX448Subtract(pX2, pTemp, pX2);
	__xrtX448MultiplySelf(pX2, pTemp);
}



/* 固定执行 448 轮 X448 Montgomery ladder。 */
static void __xrtX448Ladder(
	__xrt_x448_field Values[5],
	const uint8 pScalar[XRT_X448_PRIVATE_SIZE],
	const __xrt_x448_field pPoint
)
{
	__xrt_x448_limb iSwap = 0;
	__xrt_x448_limb* pX2 = Values[0];
	__xrt_x448_limb* pZ2 = Values[1];
	__xrt_x448_limb* pX3 = Values[2];
	__xrt_x448_limb* pZ3 = Values[3];

	memset(Values, 0, sizeof(__xrt_x448_field) * 5u);
	pX2[0] = 1;
	memcpy(pX3, pPoint, sizeof(__xrt_x448_field));
	pZ3[0] = 1;
	for ( int i = 447; i >= 0; i-- ) {
		__xrt_x448_limb iBit =
			(__xrt_x448_limb)((pScalar[i / 8] >> (i % 8)) & 1u);
		__xrt_x448_limb iMask = 0u - iBit;

		__xrtX448Swap(pX2, pX3, iSwap ^ iMask);
		__xrtX448Swap(pZ2, pZ3, iSwap ^ iMask);
		iSwap = iMask;
		__xrtX448LadderFirst(Values);
		__xrtX448LadderSecond(Values, pPoint);
	}
	__xrtX448Swap(pX2, pX3, iSwap);
	__xrtX448Swap(pZ2, pZ3, iSwap);
}



/* 规范输入、执行标量乘法并返回结果是否非零。 */
static bool __xrtX448Compute(
	uint8 pOutput[XRT_X448_SHARED_SIZE],
	const uint8 pScalarInput[XRT_X448_PRIVATE_SIZE],
	const uint8 pPointInput[XRT_X448_PUBLIC_SIZE]
)
{
	uint8 Scalar[XRT_X448_PRIVATE_SIZE];
	__xrt_x448_field Point;
	__xrt_x448_field Values[5];
	__xrt_x448_field Inverse;
	__xrt_x448_field Result;
	uint8 iNonZero = 0;

	memcpy(Scalar, pScalarInput, sizeof(Scalar));
	Scalar[0] &= 252u;
	Scalar[55] |= 128u;
	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		Point[i] = __xrtCryptoLoadLe32(pPointInput + (i * 4u));
	}
	__xrtX448Canonical(Point);
	__xrtX448Ladder(Values, Scalar, Point);
	__xrtX448Inverse(Inverse, Values[1]);
	__xrtX448Multiply(Result, Values[0], Inverse, XRT_X448_LIMBS);
	__xrtX448Canonical(Result);
	for ( size_t i = 0; i < XRT_X448_LIMBS; i++ ) {
		__xrtCryptoStoreLe32(pOutput + (i * 4u), Result[i]);
	}
	for ( size_t i = 0; i < XRT_X448_SHARED_SIZE; i++ ) {
		iNonZero |= pOutput[i];
	}

	xrtSecureZero(Scalar, sizeof(Scalar));
	xrtSecureZero(Point, sizeof(Point));
	xrtSecureZero(Values, sizeof(Values));
	xrtSecureZero(Inverse, sizeof(Inverse));
	xrtSecureZero(Result, sizeof(Result));
	return iNonZero != 0;
}



/* 设置低阶对端公钥导致的密钥协商错误。 */
static void __xrtX448AgreementError(void)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = XCRYPTO_ERROR_KEY_AGREEMENT;
	Desc.Operation = "x448-shared";
	Desc.Message = "the peer X448 public key produced an all-zero shared secret";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 执行底层 X448 标量乘法，并允许输出覆盖任一输入。 */
XRT_API bool xrtX448(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
)
{
	uint8 Output[XRT_X448_SHARED_SIZE];

	if ( (pScalar == NULL) || (pPoint == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)__xrtX448Compute(
		Output, (const uint8*)pScalar, (const uint8*)pPoint
	);
	memcpy(pOutput, Output, sizeof(Output));
	xrtSecureZero(Output, sizeof(Output));
	return true;
}



/* 从调用方提供的私钥导出 X448 公钥。 */
XRT_API bool xrtX448Public(const void* pPrivate, void* pPublic)
{
	return xrtX448(pPrivate, __xrtX448Base, pPublic);
}



/* 计算并验证可用于密钥协商的非零共享秘密。 */
XRT_API bool xrtX448Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
)
{
	uint8 Shared[XRT_X448_SHARED_SIZE];
	bool bNonZero;

	if ( (pPrivate == NULL) || (pPeerPublic == NULL) || (pShared == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bNonZero = __xrtX448Compute(
		Shared, (const uint8*)pPrivate, (const uint8*)pPeerPublic
	);
	if ( !bNonZero ) {
		xrtSecureZero(Shared, sizeof(Shared));
		__xrtX448AgreementError();
		return false;
	}
	memcpy(pShared, Shared, sizeof(Shared));
	xrtSecureZero(Shared, sizeof(Shared));
	return true;
}

#undef XRT_X448_LIMBS
#undef XRT_X448_LIMB_BITS
#undef XRT_X448_FOLD_ROUNDS

#endif
