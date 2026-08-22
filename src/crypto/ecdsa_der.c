#include "../internal/xrt_crypto_ecdsa.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_DER)

#define XRT_ECDSA_DER_SCALAR_MAX_SIZE 66u
#define XRT_ECDSA_DER_MAX_SIZE 141u



/* 计算定宽无符号整数的规范 DER INTEGER 内容范围。 */
static size_t __xrtEcdsaIntegerInfo(
	const uint8* pValue,
	size_t iScalarSize,
	size_t* pOffset,
	bool* pPrefix
)
{
	size_t iOffset = 0;

	while ( (iOffset + 1u < iScalarSize) && (pValue[iOffset] == 0) ) {
		iOffset++;
	}
	*pOffset = iOffset;
	*pPrefix = (pValue[iOffset] & 0x80u) != 0;
	return iScalarSize - iOffset + (*pPrefix ? 1u : 0u);
}



/* 写入长度不超过 255 的规范 DER 长度字段。 */
static size_t __xrtEcdsaWriteLength(uint8* pOutput, size_t iLength)
{
	if ( iLength < 128u ) {
		pOutput[0] = (uint8)iLength;
		return 1;
	}
	pOutput[0] = 0x81;
	pOutput[1] = (uint8)iLength;
	return 2;
}



/* 读取规范 DER 长度字段，并拒绝非最短形式。 */
static bool __xrtEcdsaReadLength(
	const uint8* pInput,
	size_t iInputSize,
	size_t* pOffset,
	size_t* pLength
)
{
	uint8 iFirst;

	if ( *pOffset >= iInputSize ) {
		return false;
	}
	iFirst = pInput[(*pOffset)++];
	if ( iFirst < 0x80u ) {
		*pLength = iFirst;
		return true;
	}
	if ( (iFirst != 0x81u) || (*pOffset >= iInputSize) ) {
		return false;
	}
	*pLength = pInput[(*pOffset)++];
	return *pLength >= 128u;
}



/* 编码一个规范正数 DER INTEGER。 */
static size_t __xrtEcdsaEncodeInteger(
	uint8* pOutput,
	const uint8* pValue,
	size_t iScalarSize
)
{
	size_t iOffset;
	size_t iLength;
	bool bPrefix;

	iLength = __xrtEcdsaIntegerInfo(
		pValue, iScalarSize, &iOffset, &bPrefix
	);
	pOutput[0] = 0x02;
	pOutput[1] = (uint8)iLength;
	if ( bPrefix ) {
		pOutput[2] = 0;
		memcpy(pOutput + 3, pValue + iOffset, iScalarSize - iOffset);
	} else {
		memcpy(pOutput + 2, pValue + iOffset, iScalarSize - iOffset);
	}
	return iLength + 2u;
}



/* 把定宽 raw r||s 签名编码为规范 DER SEQUENCE。 */
XRT_API bool xrtEcdsaDerEncode(
	const void* pRaw,
	size_t iScalarSize,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
)
{
	const uint8* pSignature = (const uint8*)pRaw;
	uint8 Output[XRT_ECDSA_DER_MAX_SIZE];
	size_t iLeftOffset;
	size_t iLeftLength;
	size_t iRightLength;
	size_t iContentLength;
	size_t iLengthSize;
	size_t iRequired;
	bool bPrefix;

	if ( (pRaw == NULL) || (pSize == NULL) || (iScalarSize == 0) ||
		(iScalarSize > XRT_ECDSA_DER_SCALAR_MAX_SIZE) ||
		((pDer == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iLeftLength = __xrtEcdsaIntegerInfo(
		pSignature, iScalarSize, &iLeftOffset, &bPrefix
	);
	iRightLength = __xrtEcdsaIntegerInfo(
		pSignature + iScalarSize, iScalarSize, &iLeftOffset, &bPrefix
	);
	iContentLength = 4u + iLeftLength + iRightLength;
	iLengthSize = (iContentLength < 128u) ? 1u : 2u;
	iRequired = 1u + iLengthSize + iContentLength;
	if ( __xrtCryptoRangesOverlap(
		pSize, sizeof(*pSize), pRaw, iScalarSize * 2u
	) || ((pDer != NULL) && __xrtCryptoRangesOverlap(
		pSize, sizeof(*pSize), pDer, iRequired
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pDer == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}

	Output[0] = 0x30;
	(void)__xrtEcdsaWriteLength(Output + 1, iContentLength);
	iLeftOffset = 1u + iLengthSize;
	iLeftLength = __xrtEcdsaEncodeInteger(
		Output + iLeftOffset, pSignature, iScalarSize
	);
	(void)__xrtEcdsaEncodeInteger(
		Output + iLeftOffset + iLeftLength,
		pSignature + iScalarSize,
		iScalarSize
	);
	*pSize = iRequired;
	memcpy(pDer, Output, iRequired);
	xrtSecureZero(Output, sizeof(Output));
	return true;
}



/* 读取并左侧补零一个规范正数 DER INTEGER。 */
static bool __xrtEcdsaDecodeInteger(
	const uint8* pInput,
	size_t iInputSize,
	size_t* pOffset,
	uint8* pOutput,
	size_t iScalarSize
)
{
	size_t iLength;
	const uint8* pValue;

	if ( (*pOffset >= iInputSize) || (pInput[(*pOffset)++] != 0x02u) ||
		!__xrtEcdsaReadLength(pInput, iInputSize, pOffset, &iLength) ||
		(iLength == 0) || (iLength > iInputSize - *pOffset) ) {
		return false;
	}
	pValue = pInput + *pOffset;
	*pOffset += iLength;
	if ( (pValue[0] & 0x80u) != 0 ) {
		return false;
	}
	if ( (iLength > 1u) && (pValue[0] == 0) &&
		((pValue[1] & 0x80u) == 0) ) {
		return false;
	}
	if ( (pValue[0] == 0) && (iLength > 1u) ) {
		pValue++;
		iLength--;
	}
	if ( iLength > iScalarSize ) {
		return false;
	}
	memset(pOutput, 0, iScalarSize);
	memcpy(pOutput + iScalarSize - iLength, pValue, iLength);
	return true;
}



/* 严格解码规范 DER ECDSA 签名为定宽 raw r||s。 */
XRT_API bool xrtEcdsaDerDecode(
	const void* pDer,
	size_t iDerSize,
	void* pRaw,
	size_t iScalarSize
)
{
	const uint8* pInput = (const uint8*)pDer;
	uint8 Output[XRT_ECDSA_DER_SCALAR_MAX_SIZE * 2u];
	size_t iOffset = 0;
	size_t iSequenceSize;
	bool bValid;

	if ( (pDer == NULL) || (pRaw == NULL) || (iScalarSize == 0) ||
		(iScalarSize > XRT_ECDSA_DER_SCALAR_MAX_SIZE) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bValid = (iOffset < iDerSize) && (pInput[iOffset++] == 0x30u) &&
		__xrtEcdsaReadLength(pInput, iDerSize, &iOffset, &iSequenceSize) &&
		(iSequenceSize == iDerSize - iOffset) &&
		__xrtEcdsaDecodeInteger(
			pInput, iDerSize, &iOffset, Output, iScalarSize
		) &&
		__xrtEcdsaDecodeInteger(
			pInput, iDerSize, &iOffset, Output + iScalarSize, iScalarSize
		) &&
		(iOffset == iDerSize);
	if ( !bValid ) {
		xrtSecureZero(Output, sizeof(Output));
		__xrtEcdsaError("ecdsa-der-decode", "the ECDSA signature is not canonical DER");
		return false;
	}
	memcpy(pRaw, Output, iScalarSize * 2u);
	xrtSecureZero(Output, sizeof(Output));
	return true;
}

#endif
