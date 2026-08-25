#include "../internal/xrt_codec.h"



#if defined(XRT_FEATURE_CODEC_BASE64)

#define XRT_BASE64_VALID_FLAGS \
	((uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING | \
	 (uint32)XBASE64_IGNORE_SPACE | (uint32)XBASE64_OPTIONAL_PADDING)



static const char __xrtBase64Standard[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char __xrtBase64Url[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";



/* 内置字母表直接换算字符值，自定义字母表才查询局部反查表。 */
static int8 __xrtBase64Value(
	uint8 iCharacter,
	const int8* pReverse,
	uint32 iFlags,
	bool bCustom
)
{
	if ( bCustom ) {
		return iCharacter < 128u ? pReverse[iCharacter] : -1;
	}
	if ( (iCharacter >= (uint8)'A') && (iCharacter <= (uint8)'Z') ) {
		return (int8)(iCharacter - (uint8)'A');
	}
	if ( (iCharacter >= (uint8)'a') && (iCharacter <= (uint8)'z') ) {
		return (int8)(iCharacter - (uint8)'a' + 26u);
	}
	if ( (iCharacter >= (uint8)'0') && (iCharacter <= (uint8)'9') ) {
		return (int8)(iCharacter - (uint8)'0' + 52u);
	}
	if ( (iFlags & (uint32)XBASE64_URL) != 0 ) {
		if ( iCharacter == (uint8)'-' ) {
			return 62;
		}
		return iCharacter == (uint8)'_' ? 63 : -1;
	}
	if ( iCharacter == (uint8)'+' ) {
		return 62;
	}
	return iCharacter == (uint8)'/' ? 63 : -1;
}



/* PEM 和 MIME 路径只忽略明确允许的 ASCII 横向空白与换行。 */
static bool __xrtBase64Space(uint8 iValue)
{
	return (iValue == (uint8)' ') || (iValue == (uint8)'\t') ||
		(iValue == UINT8_C(0x0B)) || (iValue == UINT8_C(0x0C)) ||
		(iValue == (uint8)'\r') || (iValue == (uint8)'\n');
}



/* 验证配置并生成正向字母表和可选的反向表。 */
static bool __xrtBase64Prepare(
	const xbase64config* pConfig,
	bool bEncoding,
	const char** ppAlphabet,
	int8* pReverse,
	bool* pCustom,
	uint32* pFlags,
	cstr sOperation
)
{
	const char* sAlphabet;
	uint64 Seen[2] = { 0, 0 };
	uint32 iFlags = 0;
	bool bCustom = false;
	size_t i;

	if ( (pConfig != NULL) &&
		!__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pConfig != NULL ) {
		iFlags = pConfig->Flags;
		bCustom = pConfig->Alphabet != NULL;
	}
	if ( (iFlags & ~XRT_BASE64_VALID_FLAGS) != 0 ||
		(((iFlags & (uint32)XBASE64_NO_PADDING) != 0) &&
		 ((iFlags & (uint32)XBASE64_OPTIONAL_PADDING) != 0)) ||
		(bEncoding && ((iFlags & (uint32)XBASE64_IGNORE_SPACE) != 0)) ) {
		__xrtCodecError(
			XERR_VALUE, XCODEC_ERROR_BASE64_CONFIG, sOperation,
			"invalid Base64 configuration flags"
		);
		return false;
	}
	if ( bCustom &&
		((iFlags & (uint32)XBASE64_URL) != 0) ) {
		__xrtCodecError(
			XERR_VALUE, XCODEC_ERROR_BASE64_CONFIG, sOperation,
			"a custom Base64 alphabet cannot be combined with URL mode"
		);
		return false;
	}
	sAlphabet = bCustom ? pConfig->Alphabet :
		(((iFlags & (uint32)XBASE64_URL) != 0) ?
			__xrtBase64Url : __xrtBase64Standard);
	if ( bCustom && !__xrtRangeValid(sAlphabet, 65u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !bCustom ) {
		if ( ppAlphabet != NULL ) {
			*ppAlphabet = sAlphabet;
		}
		if ( pCustom != NULL ) {
			*pCustom = false;
		}
		*pFlags = iFlags;
		return true;
	}

	if ( pReverse != NULL ) {
		for ( i = 0; i < 128u; i++ ) {
			pReverse[i] = -1;
		}
	}
	for ( i = 0; i < 64u; i++ ) {
		uint8 iCharacter = (uint8)sAlphabet[i];
		uint64 iMask;
		size_t iWord;

		if ( !bCustom ) {
			if ( pReverse != NULL ) {
				pReverse[iCharacter] = (int8)i;
			}
			continue;
		}
		if ( (iCharacter < 0x21u) || (iCharacter > 0x7Eu) ||
			(iCharacter == (uint8)'=') ) {
			__xrtCodecError(
				XERR_VALUE, XCODEC_ERROR_BASE64_CONFIG, sOperation,
				"Base64 alphabet must contain 64 unique visible ASCII characters"
			);
			return false;
		}
		iWord = (size_t)iCharacter >> 6u;
		iMask = UINT64_C(1) << (iCharacter & 63u);
		if ( (Seen[iWord] & iMask) != 0 ) {
			__xrtCodecError(
				XERR_VALUE, XCODEC_ERROR_BASE64_CONFIG, sOperation,
				"Base64 alphabet must contain 64 unique visible ASCII characters"
			);
			return false;
		}
		Seen[iWord] |= iMask;
		if ( pReverse != NULL ) {
			pReverse[iCharacter] = (int8)i;
		}
	}
	if ( bCustom && (sAlphabet[64] != '\0') ) {
		__xrtCodecError(
			XERR_VALUE, XCODEC_ERROR_BASE64_CONFIG, sOperation,
			"Base64 alphabet must contain exactly 64 characters"
		);
		return false;
	}
	if ( ppAlphabet != NULL ) {
		*ppAlphabet = sAlphabet;
	}
	if ( pCustom != NULL ) {
		*pCustom = true;
	}
	*pFlags = iFlags;
	return true;
}



/* 计算编码字符数，不把末尾零字节计入结果。 */
static bool __xrtBase64EncodedSize(
	size_t iSize,
	uint32 iFlags,
	size_t* pEncodedSize
)
{
	size_t iGroups = iSize / 3u;
	size_t iRemain = iSize % 3u;
	size_t iTail = iRemain == 0 ? 0 :
		(((iFlags & (uint32)XBASE64_NO_PADDING) != 0) ? iRemain + 1u : 4u);

	if ( iGroups > ((SIZE_MAX - iTail) / 4u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pEncodedSize = (iGroups * 4u) + iTail;
	if ( *pEncodedSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 从后向前编码，使输出与输入从同一地址开始时不会覆盖未读取字节。 */
static void __xrtBase64EncodeBody(
	const uint8* pData,
	size_t iSize,
	char* sOutput,
	size_t iOutputSize,
	const char* sAlphabet,
	bool bPadding
)
{
	size_t iGroups = (iSize / 3u) + ((iSize % 3u) != 0 ? 1u : 0u);

	sOutput[iOutputSize] = '\0';
	while ( iGroups != 0 ) {
		size_t iGroup;
		size_t iInput;
		size_t iCount;
		size_t iOutput;
		uint32 iValue;

		iGroups--;
		iGroup = iGroups;
		iInput = iGroup * 3u;
		iCount = iSize - iInput;
		if ( iCount > 3u ) {
			iCount = 3u;
		}
		iOutput = iGroup * 4u;
		iValue = (uint32)pData[iInput] << 16u;

		if ( iCount >= 2u ) {
			iValue |= (uint32)pData[iInput + 1u] << 8u;
		}
		if ( iCount == 3u ) {
			iValue |= (uint32)pData[iInput + 2u];
		}
		sOutput[iOutput] = sAlphabet[(iValue >> 18u) & 0x3Fu];
		sOutput[iOutput + 1u] = sAlphabet[(iValue >> 12u) & 0x3Fu];
		if ( iCount >= 2u ) {
			sOutput[iOutput + 2u] = sAlphabet[(iValue >> 6u) & 0x3Fu];
		} else if ( bPadding ) {
			sOutput[iOutput + 2u] = '=';
		}
		if ( iCount == 3u ) {
			sOutput[iOutput + 3u] = sAlphabet[iValue & 0x3Fu];
		} else if ( bPadding ) {
			sOutput[iOutput + 3u] = '=';
		}
	}
}



/* 把字节编码为规范 Base64，并支持同址原地扩张。 */
XRT_API bool xrtBase64Encode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
)
{
	const char* sAlphabet;
	uint32 iFlags;
	size_t iRequired;

	if ( !__xrtRangeValid(pData, iSize) ||
		!__xrtRangeValid(sOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtBase64Prepare(
		pConfig, true, &sAlphabet, NULL, NULL, &iFlags, "base64-encode"
	) || !__xrtBase64EncodedSize(iSize, iFlags, &iRequired) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pData, iSize
	) || ((sOutput != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
	)) ) {
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
	if ( __xrtRangesOverlap(
		sOutput, iRequired + 1u, pData, iSize
	) && ((const void*)sOutput != pData) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtBase64EncodeBody(
		(const uint8*)pData, iSize, sOutput, iRequired, sAlphabet,
		(iFlags & (uint32)XBASE64_NO_PADDING) == 0
	);
	*pOutputSize = iRequired;
	return true;
}



/* 验证完整输入并计算解码长度，确保后续写入不会遇到格式失败。 */
static bool __xrtBase64DecodedSize(
	cstr sText,
	size_t iTextSize,
	const int8* pReverse,
	uint32 iFlags,
	bool bCustom,
	size_t* pDecodedSize
)
{
	size_t iSymbols = 0;
	size_t iData = 0;
	size_t iPadding = 0;
	int8 iLast = 0;
	bool bSeenPadding = false;
	size_t i;

	for ( i = 0; i < iTextSize; i++ ) {
		uint8 iCharacter = (uint8)sText[i];
		int8 iValue;

		if ( __xrtBase64Space(iCharacter) &&
			((iFlags & (uint32)XBASE64_IGNORE_SPACE) != 0) ) {
			continue;
		}
		iSymbols++;
		if ( iCharacter == (uint8)'=' ) {
			if ( ((iFlags & (uint32)XBASE64_NO_PADDING) != 0) ||
				(++iPadding > 2u) ) {
				return false;
			}
			bSeenPadding = true;
			continue;
		}
		iValue = __xrtBase64Value(
			iCharacter, pReverse, iFlags, bCustom
		);
		if ( bSeenPadding || (iValue < 0) ) {
			return false;
		}
		iLast = iValue;
		iData++;
	}

	if ( (iFlags & (uint32)XBASE64_OPTIONAL_PADDING) != 0 ) {
		size_t iRemain = iData % 4u;
		size_t iRequiredPadding;

		if ( iRemain == 1u ) {
			return false;
		}
		iRequiredPadding = iRemain == 0 ? 0 : 4u - iRemain;
		if ( iPadding > iRequiredPadding ) {
			return false;
		}
		*pDecodedSize = (iData / 4u) * 3u +
			(iRemain == 0 ? 0 : iRemain - 1u);
	} else if ( (iFlags & (uint32)XBASE64_NO_PADDING) == 0 ) {
		if ( (iSymbols % 4u) != 0 ||
			((iPadding == 0) && ((iData % 4u) != 0)) ||
			((iPadding == 1u) && ((iData % 4u) != 3u)) ||
			((iPadding == 2u) && ((iData % 4u) != 2u)) ) {
			return false;
		}
		*pDecodedSize = (iSymbols / 4u) * 3u - iPadding;
	} else {
		size_t iRemain = iData % 4u;

		if ( iRemain == 1u ) {
			return false;
		}
		*pDecodedSize = (iData / 4u) * 3u +
			(iRemain == 0 ? 0 : iRemain - 1u);
	}

	if ( ((iData % 4u) == 2u) && ((iLast & 0x0F) != 0) ) {
		return false;
	}
	if ( ((iData % 4u) == 3u) && ((iLast & 0x03) != 0) ) {
		return false;
	}
	return true;
}



/* 在已经完整校验的输入上执行前向解码，支持同址原地收缩。 */
static void __xrtBase64DecodeBody(
	cstr sText,
	size_t iTextSize,
	const int8* pReverse,
	uint32 iFlags,
	bool bCustom,
	uint8* pOutput,
	size_t iOutputSize
)
{
	uint8 Values[4];
	size_t iValueCount = 0;
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < iTextSize; i++ ) {
		uint8 iCharacter = (uint8)sText[i];
		uint32 iValue;

		if ( __xrtBase64Space(iCharacter) &&
			((iFlags & (uint32)XBASE64_IGNORE_SPACE) != 0) ) {
			continue;
		}
		Values[iValueCount++] = iCharacter == (uint8)'=' ?
			0 : (uint8)__xrtBase64Value(
				iCharacter, pReverse, iFlags, bCustom
			);
		if ( iValueCount != 4u ) {
			continue;
		}
		iValue = ((uint32)Values[0] << 18u) |
			((uint32)Values[1] << 12u) |
			((uint32)Values[2] << 6u) | (uint32)Values[3];
		if ( iOutput < iOutputSize ) {
			pOutput[iOutput++] = (uint8)(iValue >> 16u);
		}
		if ( iOutput < iOutputSize ) {
			pOutput[iOutput++] = (uint8)(iValue >> 8u);
		}
		if ( iOutput < iOutputSize ) {
			pOutput[iOutput++] = (uint8)iValue;
		}
		iValueCount = 0;
	}
	if ( iValueCount >= 2u ) {
		uint32 iValue = ((uint32)Values[0] << 18u) |
			((uint32)Values[1] << 12u) |
			(iValueCount == 3u ? (uint32)Values[2] << 6u : 0);

		pOutput[iOutput++] = (uint8)(iValue >> 16u);
		if ( iValueCount == 3u ) {
			pOutput[iOutput] = (uint8)(iValue >> 8u);
		}
	}
}



/* 严格解码 Base64，并在格式与容量失败时保持输出不变。 */
XRT_API bool xrtBase64Decode(
	cstr sText,
	size_t iTextSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
)
{
	int8 Reverse[128];
	const int8* pReverse;
	uint32 iFlags;
	size_t iRequired;
	bool bCustom;

	if ( !__xrtRangeValid(sText, iTextSize) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtBase64Prepare(
		pConfig, false, NULL, Reverse, &bCustom, &iFlags, "base64-decode"
	) ) {
		return false;
	}
	pReverse = bCustom ? Reverse : NULL;
	if ( !__xrtBase64DecodedSize(
		sText, iTextSize, pReverse, iFlags, bCustom, &iRequired
	) ) {
		__xrtCodecError(
			XERR_PROTOCOL, XCODEC_ERROR_BASE64_FORMAT, "base64-decode",
			"input is not canonical Base64 text"
		);
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sText, iTextSize
	) || ((pOutput != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pOutput, iCapacity
	)) ) {
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
	if ( __xrtRangesOverlap(
		pOutput, iRequired, sText, iTextSize
	) && (pOutput != (const void*)sText) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iRequired != 0 ) {
		__xrtBase64DecodeBody(
			sText, iTextSize, pReverse, iFlags, bCustom,
			(uint8*)pOutput, iRequired
		);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 编码并返回独立的末尾补零字符串。 */
XRT_API str xrtBase64EncodeNew(
	const void* pData,
	size_t iSize,
	const xbase64config* pConfig
)
{
	str sOutput;
	size_t iOutputSize;

	if ( !xrtBase64Encode(
		pData, iSize, NULL, 0, &iOutputSize, pConfig
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iOutputSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtBase64Encode(
		pData, iSize, sOutput, iOutputSize + 1u, &iOutputSize, pConfig
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	return sOutput;
}



/* 解码并返回独立字节，同时保留一个不计入长度的末尾零字节。 */
XRT_API bytes xrtBase64DecodeNew(
	cstr sText,
	size_t iTextSize,
	size_t* pOutputSize,
	const xbase64config* pConfig
)
{
	bytes pOutput;
	size_t iOutputSize;

	if ( !__xrtRangeValid(sText, iTextSize) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		__xrtRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sText, iTextSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtBase64Decode(
		sText, iTextSize, NULL, 0, &iOutputSize, pConfig
	) ) {
		return NULL;
	}
	if ( iOutputSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iOutputSize + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtBase64Decode(
		sText, iTextSize, pOutput, iOutputSize, &iOutputSize, pConfig
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iOutputSize] = 0;
	*pOutputSize = iOutputSize;
	return pOutput;
}

#endif
