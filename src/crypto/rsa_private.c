#include "../internal/xrt_crypto_rsa.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

/* 去除大端整数不参与数值的前导零。 */
static const uint8* __xrtRsaTrimInteger(
	const void* pValue,
	size_t* pSize
)
{
	const uint8* pBytes = (const uint8*)pValue;

	while ( (*pSize > 0) && (*pBytes == 0) ) {
		pBytes++;
		(*pSize)--;
	}
	return pBytes;
}



/* 常数时间判断两个同宽大端整数是否满足左值小于右值。 */
static uint32 __xrtRsaBytesLess(
	const uint8* pLeft,
	const uint8* pRight,
	size_t iSize
)
{
	uint32 iBorrow = 0;

	while ( iSize > 0 ) {
		uint32 iValue;

		iSize--;
		iValue = (uint32)pLeft[iSize] - pRight[iSize] - iBorrow;
		iBorrow = iValue >> 31u;
	}
	return iBorrow;
}



/* 判断一个可选大端整数的指针和长度是否同时存在或同时为空。 */
static bool __xrtRsaOptionalIntegerValid(const void* pValue, size_t iSize)
{
	return ((pValue == NULL) && (iSize == 0)) ||
		((pValue != NULL) && (iSize != 0));
}



/* 验证 CRT 五参数是否完整，并区分完全未提供的完整指数路径。 */
static bool __xrtRsaCrtMode(const xrsaprivatekey* pKey, bool* pUseCrt)
{
	bool bPrime1 = (pKey->Prime1 != NULL) && (pKey->Prime1Size != 0);
	bool bPrime2 = (pKey->Prime2 != NULL) && (pKey->Prime2Size != 0);
	bool bExponent1 = (pKey->Exponent1 != NULL) &&
		(pKey->Exponent1Size != 0);
	bool bExponent2 = (pKey->Exponent2 != NULL) &&
		(pKey->Exponent2Size != 0);
	bool bCoefficient = (pKey->Coefficient != NULL) &&
		(pKey->CoefficientSize != 0);
	bool bAny = bPrime1 || bPrime2 || bExponent1 || bExponent2 ||
		bCoefficient;
	bool bAll = bPrime1 && bPrime2 && bExponent1 && bExponent2 &&
		bCoefficient;

	if ( !__xrtRsaOptionalIntegerValid(pKey->Prime1, pKey->Prime1Size) ||
		 !__xrtRsaOptionalIntegerValid(pKey->Prime2, pKey->Prime2Size) ||
		 !__xrtRsaOptionalIntegerValid(
			pKey->Exponent1,
			pKey->Exponent1Size
		 ) ||
		 !__xrtRsaOptionalIntegerValid(
			pKey->Exponent2,
			pKey->Exponent2Size
		 ) ||
		 !__xrtRsaOptionalIntegerValid(
			pKey->Coefficient,
			pKey->CoefficientSize
		 ) ||
		 (bAny && !bAll) ) {
		return false;
	}
	*pUseCrt = bAll;
	return true;
}



/* 返回 int31 编码头对应的真实比特长度。 */
static size_t __xrtRsaI31Bits(const uint32* pValue)
{
	return (size_t)pValue[0] - (pValue[0] >> 5u);
}



/* 验证并执行标准双素数 RSA CRT 私钥运算。 */
static __xrt_rsa_result __xrtRsaCrtPower(
	const xrsaprivatekey* pKey,
	const void* pInput,
	void* pOutput
)
{
	uint32 Prime1[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Prime2[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Value1[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Value2[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Square[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Left[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Right[XRT_RSA_MAX_FACTOR_I31_WORDS] = { 0 };
	uint32 Product[XRT_RSA_MAX_I31_WORDS] = { 0 };
	const uint8* pPrime1;
	const uint8* pPrime2;
	const uint8* pExponent1;
	const uint8* pExponent2;
	const uint8* pCoefficient;
	size_t iPrime1Size = pKey->Prime1Size;
	size_t iPrime2Size = pKey->Prime2Size;
	size_t iExponent1Size = pKey->Exponent1Size;
	size_t iExponent2Size = pKey->Exponent2Size;
	size_t iCoefficientSize = pKey->CoefficientSize;
	size_t iModulusSize = pKey->Public.ModulusSize;
	size_t iProductBits;
	size_t iValue2Words;
	uint32 iInverse;
	uint32 iBorrow;
	__xrt_rsa_result iResult = XRT_RSA_RESULT_KEY;

	/* 在读取前导零前先限制调用方声明的外部区间。 */
	if ( (iPrime1Size > (XRT_RSA_MAX_FACTOR_SIZE + 1u)) ||
		 (iPrime2Size > (XRT_RSA_MAX_FACTOR_SIZE + 1u)) ||
		 (iExponent1Size > (XRT_RSA_MAX_FACTOR_SIZE + 1u)) ||
		 (iExponent2Size > (XRT_RSA_MAX_FACTOR_SIZE + 1u)) ||
		 (iCoefficientSize > (XRT_RSA_MAX_FACTOR_SIZE + 1u)) ) {
		goto cleanup;
	}
	pPrime1 = __xrtRsaTrimInteger(pKey->Prime1, &iPrime1Size);
	pPrime2 = __xrtRsaTrimInteger(pKey->Prime2, &iPrime2Size);
	pExponent1 = __xrtRsaTrimInteger(pKey->Exponent1, &iExponent1Size);
	pExponent2 = __xrtRsaTrimInteger(pKey->Exponent2, &iExponent2Size);
	pCoefficient = __xrtRsaTrimInteger(
		pKey->Coefficient,
		&iCoefficientSize
	);

	/* 仅接受标准平衡双素数参数，避免异常因子把固定栈空间放大。 */
	if ( (iPrime1Size == 0) || (iPrime2Size == 0) ||
		 (iPrime1Size > XRT_RSA_MAX_FACTOR_SIZE) ||
		 (iPrime2Size > XRT_RSA_MAX_FACTOR_SIZE) ||
		 (iPrime1Size > ((iModulusSize + 1u) >> 1u)) ||
		 (iPrime2Size > ((iModulusSize + 1u) >> 1u)) ||
		 ((iPrime1Size + iPrime2Size) < iModulusSize) ||
		 ((iPrime1Size + iPrime2Size) > (iModulusSize + 1u)) ||
		 ((pPrime1[iPrime1Size - 1u] & 1u) == 0) ||
		 ((pPrime2[iPrime2Size - 1u] & 1u) == 0) ||
		 (iPrime1Size == iPrime2Size &&
		  xrtConstTimeEqual(pPrime1, pPrime2, iPrime1Size)) ||
		 (iExponent1Size == 0) || (iExponent1Size > iPrime1Size) ||
		 (iExponent2Size == 0) || (iExponent2Size > iPrime2Size) ||
		 !__xrtRsaExponentValid(pExponent1, iExponent1Size) ||
		 !__xrtRsaExponentValid(pExponent2, iExponent2Size) ||
		 (iCoefficientSize == 0) ||
		 (iCoefficientSize > iPrime1Size) ) {
		goto cleanup;
	}
	if ( (iCoefficientSize == iPrime1Size) &&
		 !__xrtRsaBytesLess(pCoefficient, pPrime1, iPrime1Size) ) {
		goto cleanup;
	}

	__xrtI31Decode(Prime1, pPrime1, iPrime1Size);
	__xrtI31Decode(Prime2, pPrime2, iPrime2Size);
	if ( (((Prime1[0] + 31u) >> 5u) + 1u) >
		 XRT_RSA_MAX_FACTOR_I31_WORDS ||
		 (((Prime2[0] + 31u) >> 5u) + 1u) >
		 XRT_RSA_MAX_FACTOR_I31_WORDS ) {
		goto cleanup;
	}

	/* p * q 必须与公开模数完全一致，不能只依赖最终签名复核。 */
	__xrtI31MultiplyAdd(Product, Prime1, Prime2);
	iProductBits = __xrtRsaI31Bits(Product);
	if ( (iProductBits == 0) ||
		 (((iProductBits + 7u) >> 3u) > iModulusSize) ) {
		goto cleanup;
	}
	__xrtI31Encode(pOutput, iModulusSize, Product);
	if ( !xrtConstTimeEqual(
		pOutput,
		pKey->Public.Modulus,
		iModulusSize
	) ) {
		goto cleanup;
	}

	/* 分别计算 m1 = m^dP mod p 与 m2 = m^dQ mod q。 */
	__xrtI31ReduceBytes(Value2, pInput, iModulusSize, Prime2);
	__xrtI31MontgomerySquare(Square, Prime2);
	iInverse = __xrtI31NegativeInverse(Prime2[1]);
	if ( iInverse == 0 ) {
		goto cleanup;
	}
	__xrtI31ModPower(
		Value2,
		pExponent2,
		iExponent2Size,
		Prime2,
		Square,
		iInverse,
		Left,
		Right
	);

	__xrtI31ReduceBytes(Value1, pInput, iModulusSize, Prime1);
	__xrtI31MontgomerySquare(Square, Prime1);
	iInverse = __xrtI31NegativeInverse(Prime1[1]);
	if ( iInverse == 0 ) {
		goto cleanup;
	}
	__xrtI31ModPower(
		Value1,
		pExponent1,
		iExponent1Size,
		Prime1,
		Square,
		iInverse,
		Left,
		Right
	);

	/* h = (m1 - m2) * qInv mod p。 */
	__xrtI31Encode(pOutput, iPrime2Size, Value2);
	__xrtI31ReduceBytes(Left, pOutput, iPrime2Size, Prime1);
	iBorrow = __xrtI31Subtract(Value1, Left, 1u);
	__xrtI31Add(Value1, Prime1, iBorrow);
	__xrtI31ReduceBytes(Right, pCoefficient, iCoefficientSize, Prime1);
	__xrtI31MontgomeryMultiply(
		Left,
		Value1,
		Square,
		Prime1,
		iInverse
	);
	__xrtI31MontgomeryMultiply(
		Value1,
		Left,
		Right,
		Prime1,
		iInverse
	);

	/* 以 s = m2 + q * h 重组完整定宽结果。 */
	memset(Product, 0, sizeof(Product));
	iValue2Words = (Value2[0] + 31u) >> 5u;
	memcpy(
		Product + 1,
		Value2 + 1,
		iValue2Words * sizeof(uint32)
	);
	__xrtI31MultiplyAdd(Product, Prime2, Value1);
	if ( ((Product[0] + 31u) >> 5u) >= XRT_RSA_MAX_I31_WORDS ) {
		goto cleanup;
	}
	__xrtI31Encode(pOutput, iModulusSize, Product);
	iResult = XRT_RSA_RESULT_OK;

cleanup:
	xrtSecureZero(Prime1, sizeof(Prime1));
	xrtSecureZero(Prime2, sizeof(Prime2));
	xrtSecureZero(Value1, sizeof(Value1));
	xrtSecureZero(Value2, sizeof(Value2));
	xrtSecureZero(Square, sizeof(Square));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	xrtSecureZero(Product, sizeof(Product));
	return iResult;
}



/* 执行完整指数或 CRT 私钥运算，并以公开指数复核故障结果。 */
__xrt_rsa_result __xrtRsaPrivateCore(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
)
{
	uint8 Result[XRT_RSA_MAX_MODULUS_SIZE];
	uint8 Verified[XRT_RSA_MAX_MODULUS_SIZE];
	xrsapublickey PrivatePower;
	size_t iModulusSize;
	bool bUseCrt;
	__xrt_rsa_result iResult = XRT_RSA_RESULT_ARGUMENT;

	if ( (pKey == NULL) || (pInput == NULL) || (pOutput == NULL) ) {
		return XRT_RSA_RESULT_ARGUMENT;
	}
	iModulusSize = pKey->Public.ModulusSize;
	if ( (iInputSize != iModulusSize) || !__xrtRsaKeyValid(&pKey->Public) ||
		 !__xrtRsaCrtMode(pKey, &bUseCrt) ||
		 !__xrtRsaOptionalIntegerValid(
			pKey->PrivateExponent,
			pKey->PrivateExponentSize
		 ) ) {
		iResult = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	if ( !__xrtRsaBytesLess(
		(const uint8*)pInput,
		(const uint8*)pKey->Public.Modulus,
		iModulusSize
	) ) {
		iResult = XRT_RSA_RESULT_INPUT;
		goto cleanup;
	}

	if ( bUseCrt ) {
		iResult = __xrtRsaCrtPower(pKey, pInput, Result);
	} else {
		if ( pKey->PrivateExponent == NULL ) {
			iResult = XRT_RSA_RESULT_KEY;
			goto cleanup;
		}
		PrivatePower = pKey->Public;
		PrivatePower.Exponent = pKey->PrivateExponent;
		PrivatePower.ExponentSize = pKey->PrivateExponentSize;
		iResult = __xrtRsaPower(
			&PrivatePower,
			pInput,
			iInputSize,
			Result
		);
	}
	if ( iResult != XRT_RSA_RESULT_OK ) {
		goto cleanup;
	}

	/* 公钥复核同时检测 CRT 参数不一致和计算故障。 */
	iResult = __xrtRsaPower(
		&pKey->Public,
		Result,
		iModulusSize,
		Verified
	);
	if ( (iResult != XRT_RSA_RESULT_OK) ||
		 !xrtConstTimeEqual(Verified, pInput, iModulusSize) ) {
		iResult = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	memcpy(pOutput, Result, iModulusSize);

cleanup:
	xrtSecureZero(Result, sizeof(Result));
	xrtSecureZero(Verified, sizeof(Verified));
	return iResult;
}



/* 执行原始 RSA 私钥运算，并把内部失败归一到结构化错误。 */
bool xrtRsaPrivate(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
)
{
	__xrt_rsa_result iResult = __xrtRsaPrivatePower(
		pKey,
		pInput,
		iInputSize,
		pOutput
	);

	if ( iResult == XRT_RSA_RESULT_OK ) {
		return true;
	}
	if ( iResult == XRT_RSA_RESULT_RANDOM ) {
		return false;
	}
	if ( (iResult == XRT_RSA_RESULT_ARGUMENT) ||
		 (iResult == XRT_RSA_RESULT_INPUT) ) {
		__xrtRsaError(
			"crypto.rsa.private",
			"the RSA private input or output is invalid",
			XCRYPTO_ERROR_KEY
		);
	} else {
		__xrtRsaError(
			"crypto.rsa.private",
			"the RSA private key or CRT result is invalid",
			XCRYPTO_ERROR_KEY
		);
	}
	return false;
}

#endif
