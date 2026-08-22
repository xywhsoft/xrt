#include "../internal/xrt_crypto_rsa.h"



#if defined(XRT_FEATURE_CRYPTO_RSA)

/* 判断大端字节串是否表示大于一的奇数。 */
bool __xrtRsaExponentValid(const uint8* pExponent, size_t iSize)
{
	size_t i = 0;

	while ( (i < iSize) && (pExponent[i] == 0) ) {
		i++;
	}
	if ( (i == iSize) || ((pExponent[iSize - 1u] & 1u) == 0) ) {
		return false;
	}
	if ( ((iSize - i) == 1u) && (pExponent[i] <= 1u) ) {
		return false;
	}
	return true;
}



/* 验证 RSA 公钥视图的尺寸、奇模数和奇指数。 */
bool __xrtRsaKeyValid(const xrsapublickey* pKey)
{
	const uint8* pModulus;

	if ( (pKey == NULL) || (pKey->Modulus == NULL) ||
		 (pKey->Exponent == NULL) ) {
		return false;
	}
	pModulus = (const uint8*)pKey->Modulus;
	return (pKey->ModulusSize >= XRT_RSA_MIN_MODULUS_SIZE) &&
		(pKey->ModulusSize <= XRT_RSA_MAX_MODULUS_SIZE) &&
		(pKey->ExponentSize > 0) &&
		(pKey->ExponentSize <= pKey->ModulusSize) &&
		(pModulus[0] != 0) &&
		((pModulus[pKey->ModulusSize - 1u] & 1u) != 0) &&
		__xrtRsaExponentValid(
			(const uint8*)pKey->Exponent,
			pKey->ExponentSize
		);
}



/* 执行严格输入检查后的 RSA 模幂，不直接修改线程错误。 */
__xrt_rsa_result __xrtRsaPower(
	const xrsapublickey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
)
{
	uint32 Modulus[XRT_RSA_MAX_I31_WORDS] = { 0 };
	uint32 Value[XRT_RSA_MAX_I31_WORDS] = { 0 };
	uint32 Square[XRT_RSA_MAX_I31_WORDS] = { 0 };
	uint32 Left[XRT_RSA_MAX_I31_WORDS] = { 0 };
	uint32 Right[XRT_RSA_MAX_I31_WORDS] = { 0 };
	size_t iWords;
	size_t iModulusSize;
	uint32 iInverse;
	__xrt_rsa_result iResult = XRT_RSA_RESULT_OK;

	if ( (pKey == NULL) || (pInput == NULL) || (pOutput == NULL) ) {
		return XRT_RSA_RESULT_ARGUMENT;
	}
	iModulusSize = pKey->ModulusSize;
	if ( iInputSize != iModulusSize ) {
		return XRT_RSA_RESULT_ARGUMENT;
	}
	if ( !__xrtRsaKeyValid(pKey) ) {
		return XRT_RSA_RESULT_KEY;
	}
	__xrtI31Decode(Modulus, pKey->Modulus, iModulusSize);
	iWords = (Modulus[0] + 31u) >> 5u;
	if ( (iWords < 2u) || ((iWords + 1u) > XRT_RSA_MAX_I31_WORDS) ) {
		iResult = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	if ( __xrtI31DecodeMod(Value, pInput, iInputSize, Modulus) == 0 ) {
		iResult = XRT_RSA_RESULT_INPUT;
		goto cleanup;
	}

	__xrtI31MontgomerySquare(Square, Modulus);
	iInverse = __xrtI31NegativeInverse(Modulus[1]);
	if ( iInverse == 0 ) {
		iResult = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	__xrtI31ModPower(
		Value,
		(const uint8*)pKey->Exponent,
		pKey->ExponentSize,
		Modulus,
		Square,
		iInverse,
		Left,
		Right
	);
	__xrtI31Encode(pOutput, iModulusSize, Value);

cleanup:
	xrtSecureZero(Modulus, sizeof(Modulus));
	xrtSecureZero(Value, sizeof(Value));
	xrtSecureZero(Square, sizeof(Square));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	return iResult;
}



/* 执行原始 RSA 公钥运算，所有输入输出均为定宽大端整数。 */
bool xrtRsaPublic(
	const xrsapublickey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
)
{
	__xrt_rsa_result iResult = __xrtRsaPower(
		pKey,
		pInput,
		iInputSize,
		pOutput
	);

	if ( iResult == XRT_RSA_RESULT_OK ) {
		return true;
	}
	if ( (iResult == XRT_RSA_RESULT_ARGUMENT) ||
		 (iResult == XRT_RSA_RESULT_INPUT) ) {
		__xrtRsaError(
			"crypto.rsa.public",
			"the RSA input or output is invalid",
			XCRYPTO_ERROR_KEY
		);
	} else {
		__xrtRsaError(
			"crypto.rsa.public",
			"the RSA modulus or exponent is invalid",
			XCRYPTO_ERROR_KEY
		);
	}
	return false;
}

#endif
