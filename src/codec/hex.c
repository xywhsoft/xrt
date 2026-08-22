#include "../internal/xrt_codec.h"



#if defined(XRT_FEATURE_CODEC_HEX)

#define XRT_HEX_ENCODE_FLAGS ((uint32)XHEX_UPPER)
#define XRT_HEX_DECODE_FLAGS ((uint32)XHEX_IGNORE_SPACE)



/* 判断一个字节是否属于解码器允许忽略的 ASCII 空白。 */
static bool __xrtHexSpace(uint8 iByte)
{
	return (iByte == (uint8)' ') || (iByte == (uint8)'\t') ||
		(iByte == UINT8_C(0x0B)) || (iByte == UINT8_C(0x0C)) ||
		(iByte == (uint8)'\r') || (iByte == (uint8)'\n');
}



/* 把一个 HEX 数字转换为四位值。 */
static bool __xrtHexNibble(uint8 iByte, uint8* pValue)
{
	if ( (iByte >= (uint8)'0') && (iByte <= (uint8)'9') ) {
		*pValue = (uint8)(iByte - (uint8)'0');
		return true;
	}
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'F') ) {
		*pValue = (uint8)(iByte - (uint8)'A' + 10u);
		return true;
	}
	if ( (iByte >= (uint8)'a') && (iByte <= (uint8)'f') ) {
		*pValue = (uint8)(iByte - (uint8)'a' + 10u);
		return true;
	}
	return false;
}



/* 校验全部输入并计算解码字节数。 */
static bool __xrtHexDecodedSize(xstrview Text, uint32 iFlags,
	size_t* pDecodedSize)
{
	size_t iDigits = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iValue;
		uint8 iByte = (uint8)Text.Data[i];

		if ( __xrtHexSpace(iByte) &&
			((iFlags & (uint32)XHEX_IGNORE_SPACE) != 0) ) {
			continue;
		}
		if ( !__xrtHexNibble(iByte, &iValue) ) {
			__xrtCodecError(XERR_PROTOCOL, XCODEC_ERROR_HEX_FORMAT,
				"hex-decode", "HEX text contains a non-hexadecimal character");
			return false;
		}
		iDigits++;
	}
	if ( (iDigits & 1u) != 0 ) {
		__xrtCodecError(XERR_PROTOCOL, XCODEC_ERROR_HEX_FORMAT,
			"hex-decode", "HEX text must contain an even number of digits");
		return false;
	}
	*pDecodedSize = iDigits / 2u;
	return true;
}



/* 从后向前编码，从而支持输出与输入同址扩张。 */
static void __xrtHexEncodeBody(const uint8* pData, size_t iSize,
	char* sOutput, bool bUpper)
{
	static const char sLower[] = "0123456789abcdef";
	static const char sUpper[] = "0123456789ABCDEF";
	const char* sDigits = bUpper ? sUpper : sLower;

	sOutput[iSize * 2u] = 0;
	while ( iSize != 0 ) {
		uint8 iByte;
		size_t iOutput;

		iSize--;
		iByte = pData[iSize];
		iOutput = iSize * 2u;
		sOutput[iOutput] = sDigits[iByte >> 4u];
		sOutput[iOutput + 1u] = sDigits[iByte & 0x0Fu];
	}
}



/* 把任意字节编码为 HEX 文本。 */
XRT_API bool xrtHexEncode(const void* pData, size_t iSize,
	char* sOutput, size_t iCapacity, size_t* pOutputSize, uint32 iFlags)
{
	size_t iRequired;

	if ( !__xrtRangeValid(pData, iSize) ||
		!__xrtRangeValid(sOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iFlags & ~XRT_HEX_ENCODE_FLAGS) != 0 ) {
		__xrtCodecError(XERR_VALUE, XCODEC_ERROR_HEX_CONFIG,
			"hex-encode", "invalid HEX encode flags");
		return false;
	}
	if ( iSize > ((SIZE_MAX - 1u) / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iRequired = iSize * 2u;
	if ( __xrtRangesOverlap(pOutputSize, sizeof(*pOutputSize), pData, iSize) ||
		 ((sOutput != NULL) && __xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(sOutput, iRequired + 1u, pData, iSize) &&
		 ((const void*)sOutput != pData) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtHexEncodeBody((const uint8*)pData, iSize, sOutput,
		(iFlags & (uint32)XHEX_UPPER) != 0);
	*pOutputSize = iRequired;
	return true;
}



/* 在已经预检的输入上执行向前原地安全解码。 */
static void __xrtHexDecodeBody(xstrview Text, uint32 iFlags,
	uint8* pOutput)
{
	size_t iOutput = 0;
	uint8 iHigh = 0;
	bool bHigh = true;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint8 iValue = 0;
		uint8 iByte = (uint8)Text.Data[i];

		if ( __xrtHexSpace(iByte) &&
			((iFlags & (uint32)XHEX_IGNORE_SPACE) != 0) ) {
			continue;
		}
		(void)__xrtHexNibble(iByte, &iValue);
		if ( bHigh ) {
			iHigh = iValue;
			bHigh = false;
		} else {
			pOutput[iOutput++] = (uint8)((iHigh << 4u) | iValue);
			bHigh = true;
		}
	}
}



/* 严格解码 HEX 文本。 */
XRT_API bool xrtHexDecode(xstrview Text, void* pOutput,
	size_t iCapacity, size_t* pOutputSize, uint32 iFlags)
{
	size_t iRequired;

	if ( !__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iFlags & ~XRT_HEX_DECODE_FLAGS) != 0 ) {
		__xrtCodecError(XERR_VALUE, XCODEC_ERROR_HEX_CONFIG,
			"hex-decode", "invalid HEX decode flags");
		return false;
	}
	if ( !__xrtHexDecodedSize(Text, iFlags, &iRequired) ) {
		return false;
	}
	if ( __xrtRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Text.Data, Text.Size) ||
		 ((pOutput != NULL) && __xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(pOutput, iRequired, Text.Data, Text.Size) &&
		 (pOutput != (const void*)Text.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtHexDecodeBody(Text, iFlags, (uint8*)pOutput);
	*pOutputSize = iRequired;
	return true;
}



/* 编码并创建末尾补零的独立文本。 */
XRT_API str xrtHexEncodeNew(const void* pData, size_t iSize, uint32 iFlags)
{
	size_t iOutputSize;
	str sOutput;

	if ( !xrtHexEncode(pData, iSize, NULL, 0, &iOutputSize, iFlags) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iOutputSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHexEncode(pData, iSize, sOutput, iOutputSize + 1u,
		&iOutputSize, iFlags) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 解码并创建带额外末尾零哨兵的独立字节。 */
XRT_API bytes xrtHexDecodeNew(xstrview Text, size_t* pOutputSize,
	uint32 iFlags)
{
	size_t iRequired;
	bytes pOutput;

	if ( !__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		__xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHexDecode(Text, NULL, 0, &iRequired, iFlags) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHexDecode(Text, pOutput, iRequired, &iRequired, iFlags) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return pOutput;
}

#undef XRT_HEX_ENCODE_FLAGS
#undef XRT_HEX_DECODE_FLAGS

#endif
