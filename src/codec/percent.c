#include "../internal/xrt_codec.h"



#if defined(XRT_FEATURE_CODEC_PERCENT)

static const char __xrtPercentHex[] = "0123456789ABCDEF";
static const char __xrtPercentUnreservedText[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";



/* 判断字节是否属于 RFC 3986 无需转义字符。 */
bool __xrtPercentUnreserved(uint8 iValue)
{
	return ((iValue >= (uint8)'A') && (iValue <= (uint8)'Z')) ||
		((iValue >= (uint8)'a') && (iValue <= (uint8)'z')) ||
		((iValue >= (uint8)'0') && (iValue <= (uint8)'9')) ||
		(iValue == (uint8)'-') || (iValue == (uint8)'.') ||
		(iValue == (uint8)'_') || (iValue == (uint8)'~');
}



/* 判断字符是否属于允许按上下文保留的 RFC 3986 保留字符。 */
static bool __xrtPercentReserved(uint8 iValue)
{
	static const char Reserved[] = ":/?#[]@!$&'()*+,;=";

	return (iValue != 0) && (iValue < 128u) &&
		(strchr(Reserved, (int)iValue) != NULL);
}



/* 从零结尾 ASCII 字符集合构建完整安全位图。 */
void __xrtPercentMapInit(xrt_percent_map* pMap, cstr sSafe)
{
	const uint8* pText = (const uint8*)sSafe;

	pMap->Bits[0] = 0;
	pMap->Bits[1] = 0;
	while ( *pText != 0 ) {
		uint8 iValue = *pText++;

		pMap->Bits[iValue >> 6u] |=
			UINT64_C(1) << (iValue & 63u);
	}
}



/* 构建 RFC 3986 unreserved 字节位图。 */
void __xrtPercentUnreservedMap(xrt_percent_map* pMap)
{
	__xrtPercentMapInit(pMap, __xrtPercentUnreservedText);
}



/* 构建可跨字段复用的公开安全字符位图。 */
XRT_API bool xrtPercentMapInit(
	xpercentmap* pMap,
	xstrview Safe,
	bool bIncludeUnreserved
)
{
	xrt_percent_map Map;
	size_t i;

	if ( !__xrtRangeValid(pMap, sizeof(*pMap)) ||
		 !__xrtRangeValid(Safe.Data, Safe.Size) ||
		 __xrtRangesOverlap(
			pMap, sizeof(*pMap), Safe.Data, Safe.Size
		 ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( bIncludeUnreserved ) {
		__xrtPercentUnreservedMap(&Map);
	} else {
		Map.Bits[0] = 0;
		Map.Bits[1] = 0;
	}
	for ( i = 0; i < Safe.Size; i++ ) {
		uint8 iValue = (uint8)Safe.Data[i];

		if ( (iValue < 0x21u) || (iValue > 0x7Eu) ) {
			__xrtCodecError(
				XERR_VALUE, XCODEC_ERROR_PERCENT_CONFIG,
				"percent-map", "safe set contains a non-visible ASCII byte"
			);
			return false;
		}
		Map.Bits[iValue >> 6u] |=
			UINT64_C(1) << (iValue & 63u);
	}
	memcpy(pMap, &Map, sizeof(Map));
	return true;
}



/* 验证额外安全字符并追加到 RFC 3986 基础位图。 */
static bool __xrtPercentSafeMap(
	xstrview ExtraSafe,
	xrt_percent_map* pSafe
)
{
	size_t i;

	if (
		!__xrtRangeValid(ExtraSafe.Data, ExtraSafe.Size) ||
		!__xrtRangeValid(pSafe, sizeof(*pSafe))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtPercentUnreservedMap(pSafe);
	for ( i = 0; i < ExtraSafe.Size; i++ ) {
		uint8 iValue = (uint8)ExtraSafe.Data[i];

		if ( !__xrtPercentUnreserved(iValue) &&
			!__xrtPercentReserved(iValue) ) {
			__xrtCodecError(
				XERR_VALUE, XCODEC_ERROR_PERCENT_CONFIG,
				"percent-encode", "extra-safe contains a non-URI character"
			);
			return false;
		}
		pSafe->Bits[iValue >> 6u] |=
			UINT64_C(1) << (iValue & 63u);
	}
	return true;
}



/* 使用完整安全位图判断一个输入字节。 */
static bool __xrtPercentSafe(
	uint8 iValue,
	const xrt_percent_map* pSafe
)
{
	return (iValue < 128u) &&
		((pSafe->Bits[iValue >> 6u] &
		 (UINT64_C(1) << (iValue & 63u))) != 0);
}



/* 计算指定安全字符集和空格规则下的精确编码长度。 */
bool __xrtPercentEncodedSize(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	size_t* pEncodedSize
)
{
	size_t iRequired = 0;
	size_t i;

	if (
		!__xrtRangeValid(pData, iSize) ||
		!__xrtRangeValid(pSafe, sizeof(*pSafe)) ||
		!__xrtRangeValid(pEncodedSize, sizeof(*pEncodedSize))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iSize; i++ ) {
		uint8 iValue = pData[i];
		size_t iAdd = ((bSpaceAsPlus && (iValue == (uint8)' ')) ||
			__xrtPercentSafe(iValue, pSafe)) ? 1u : 3u;

		if ( iRequired > (SIZE_MAX - iAdd) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired += iAdd;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pEncodedSize = iRequired;
	return true;
}



/* 公开精确测量入口复用内部位图引擎。 */
XRT_API bool xrtPercentMeasure(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	size_t* pOutputSize
)
{
	return __xrtPercentEncodedSize(
		(const uint8*)pData,
		iSize,
		(const xrt_percent_map*)pMap,
		bSpaceAsPlus,
		pOutputSize
	);
}



/* 从后向前编码，保证同址扩张不会覆盖尚未读取的输入。 */
void __xrtPercentEncodeBody(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iOutputSize,
	bool bTerminate
)
{
	size_t iInput = iSize;
	size_t iOutput = iOutputSize;

	if ( bTerminate ) {
		sOutput[iOutputSize] = '\0';
	}
	while ( iInput != 0 ) {
		uint8 iValue = pData[--iInput];

		if ( bSpaceAsPlus && (iValue == (uint8)' ') ) {
			sOutput[--iOutput] = '+';
		} else if ( __xrtPercentSafe(iValue, pSafe) ) {
			sOutput[--iOutput] = (char)iValue;
		} else {
			iOutput -= 3u;
			sOutput[iOutput] = '%';
			sOutput[iOutput + 1u] = __xrtPercentHex[iValue >> 4u];
			sOutput[iOutput + 2u] = __xrtPercentHex[iValue & 0x0Fu];
		}
	}
}



/* 在不重叠输出上顺序执行 percent 编码。 */
size_t __xrtPercentEncodeForwardBody(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput
)
{
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		uint8 iValue = pData[i];

		if ( bSpaceAsPlus && (iValue == (uint8)' ') ) {
			sOutput[iOutput++] = '+';
		} else if ( __xrtPercentSafe(iValue, pSafe) ) {
			sOutput[iOutput++] = (char)iValue;
		} else {
			sOutput[iOutput++] = '%';
			sOutput[iOutput++] = __xrtPercentHex[iValue >> 4u];
			sOutput[iOutput++] = __xrtPercentHex[iValue & 0x0Fu];
		}
	}
	return iOutput;
}



/* 在调用方已经测量的输入上执行无分配顺序写入。 */
XRT_API size_t xrtPercentWriteMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput
)
{
	return __xrtPercentEncodeForwardBody(
		(const uint8*)pData,
		iSize,
		(const xrt_percent_map*)pMap,
		bSpaceAsPlus,
		sOutput
	);
}



/* 在调用方已经测量的输入上执行可同址扩张编码。 */
XRT_API void xrtPercentEncodeMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iOutputSize,
	bool bTerminate
)
{
	__xrtPercentEncodeBody(
		(const uint8*)pData,
		iSize,
		(const xrt_percent_map*)pMap,
		bSpaceAsPlus,
		sOutput,
		iOutputSize,
		bTerminate
	);
}



/* 执行共享的参数、容量、重叠检查和 percent 编码。 */
bool __xrtPercentEncodeCore(
	const void* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	bool bTerminate
)
{
	size_t iRequired;
	size_t iWriteSize;

	if (
		!__xrtRangeValid(pData, iSize) ||
		!__xrtRangeValid(pSafe, sizeof(*pSafe)) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		!__xrtRangeValid(sOutput, iCapacity)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPercentEncodedSize(
		(const uint8*)pData, iSize, pSafe, bSpaceAsPlus, &iRequired
	) ) {
		return false;
	}
	iWriteSize = iRequired + (bTerminate ? 1u : 0u);
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
	if ( iCapacity < iWriteSize ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		sOutput, iWriteSize, pData, iSize
	) && ((const void*)sOutput != pData) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtPercentEncodeBody(
		(const uint8*)pData, iSize, pSafe, bSpaceAsPlus,
		sOutput, iRequired, bTerminate
	);
	*pOutputSize = iRequired;
	return true;
}



/* 把一个十六进制字符转换为半字节，非法字符返回负数。 */
static int __xrtPercentHexValue(uint8 iValue)
{
	if ( (iValue >= (uint8)'0') && (iValue <= (uint8)'9') ) {
		return (int)(iValue - (uint8)'0');
	}
	if ( (iValue >= (uint8)'A') && (iValue <= (uint8)'F') ) {
		return 10 + (int)(iValue - (uint8)'A');
	}
	if ( (iValue >= (uint8)'a') && (iValue <= (uint8)'f') ) {
		return 10 + (int)(iValue - (uint8)'a');
	}
	return -1;
}



/* 无错误副作用地读取一个原始或 percent 转义字节。 */
XRT_API xpercentnext xrtPercentNext(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOffset,
	uint8* pValue
)
{
	size_t iOffset;
	uint8 iValue;

	if ( !__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pValue, sizeof(iValue)) ||
		__xrtRangesOverlap(
			pOffset, sizeof(iOffset), pValue, sizeof(iValue)
		) || __xrtRangesOverlap(
			Text.Data, Text.Size, pOffset, sizeof(iOffset)
		) || __xrtRangesOverlap(
			Text.Data, Text.Size, pValue, sizeof(iValue)
		) ) {
		return XPERCENT_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset == Text.Size ) {
		return XPERCENT_NEXT_END;
	}
	if ( iOffset > Text.Size ) {
		return XPERCENT_NEXT_ERROR;
	}
	iValue = (uint8)Text.Data[iOffset++];
	if ( iValue == (uint8)'%' ) {
		int iHigh;
		int iLow;

		if ( (Text.Size - iOffset) < 2u ) {
			return XPERCENT_NEXT_ERROR;
		}
		iHigh = __xrtPercentHexValue((uint8)Text.Data[iOffset++]);
		iLow = __xrtPercentHexValue((uint8)Text.Data[iOffset++]);
		if ( (iHigh < 0) || (iLow < 0) ) {
			return XPERCENT_NEXT_ERROR;
		}
		iValue = (uint8)(((uint32)iHigh << 4u) | (uint32)iLow);
	} else if ( bPlusAsSpace && (iValue == (uint8)'+') ) {
		iValue = (uint8)' ';
	}
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	memcpy(pValue, &iValue, sizeof(iValue));
	return XPERCENT_NEXT_BYTE;
}



/* 验证全部百分号转义并计算解码字节数。 */
bool __xrtPercentDecodedSize(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pDecodedSize,
	cstr sOperation
)
{
	size_t iRequired = 0;
	size_t i = 0;
	uint8 iValue;
	xpercentnext Next;

	if (
		!__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pDecodedSize, sizeof(*pDecodedSize)) ||
		(sOperation == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	do {
		Next = xrtPercentNext(
			Text, bPlusAsSpace, &i, &iValue
		);
		if ( Next == XPERCENT_NEXT_ERROR ) {
			__xrtCodecError(
				XERR_PROTOCOL, XCODEC_ERROR_PERCENT_FORMAT,
				sOperation, "input contains an invalid percent escape"
			);
			return false;
		}
		if ( Next == XPERCENT_NEXT_END ) {
			break;
		}
		if ( iRequired == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired++;
	} while ( Next == XPERCENT_NEXT_BYTE );
	*pDecodedSize = iRequired;
	return true;
}



/* 公开严格解码测量入口保留加号转换策略。 */
XRT_API bool xrtPercentDecodeMeasure(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOutputSize
)
{
	return __xrtPercentDecodedSize(
		Text,
		bPlusAsSpace,
		pOutputSize,
		"percent-decode"
	);
}



/* 在已验证文本上前向解码，支持同址收缩和可选加号转换。 */
size_t __xrtPercentDecodeBody(
	xstrview Text,
	bool bPlusAsSpace,
	uint8* pOutput
)
{
	size_t iInput = 0;
	size_t iOutput = 0;
	uint8 iValue;

	while ( xrtPercentNext(
		Text, bPlusAsSpace, &iInput, &iValue
	) == XPERCENT_NEXT_BYTE ) {
		pOutput[iOutput++] = iValue;
	}
	return iOutput;
}



/* 在调用方已经验证的文本上执行无分配顺序解码。 */
XRT_API size_t xrtPercentDecodeMeasured(
	xstrview Text,
	bool bPlusAsSpace,
	void* pOutput
)
{
	return __xrtPercentDecodeBody(
		Text,
		bPlusAsSpace,
		(uint8*)pOutput
	);
}



/* 在已经验证的文本上逐字节比较解码结果。 */
bool __xrtPercentDecodedEqualBody(
	xstrview Text,
	bool bPlusAsSpace,
	xbytesview Data
)
{
	size_t iInput = 0;
	size_t iData = 0;
	uint8 iValue;

	while ( xrtPercentNext(
		Text, bPlusAsSpace, &iInput, &iValue
	) == XPERCENT_NEXT_BYTE ) {
		if ( (iData == Data.Size) || (Data.Data[iData] != iValue) ) {
			return false;
		}
		iData++;
	}
	return iData == Data.Size;
}



/* 执行共享的参数、容量、重叠检查和严格 percent 解码。 */
bool __xrtPercentDecodeCore(
	xstrview Text,
	bool bPlusAsSpace,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	cstr sOperation
)
{
	size_t iRequired;

	if (
		!__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		(sOperation == NULL)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPercentDecodedSize(
		Text, bPlusAsSpace, &iRequired, sOperation
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
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
		pOutput, iRequired, Text.Data, Text.Size
	) && (pOutput != (const void*)Text.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iRequired != 0 ) {
		(void)__xrtPercentDecodeBody(
			Text, bPlusAsSpace, (uint8*)pOutput
		);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 共享公开编码入口的安全字符、别名与终止符策略。 */
static bool __xrtPercentWrite(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	bool bTerminate
)
{
	xrt_percent_map Safe;

	if ( !__xrtPercentSafeMap(ExtraSafe, &Safe) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutputSize, pOutputSize != NULL ? sizeof(*pOutputSize) : 0,
		ExtraSafe.Data, ExtraSafe.Size
	) || __xrtRangesOverlap(
		sOutput, sOutput != NULL ? iCapacity : 0,
		ExtraSafe.Data, ExtraSafe.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtPercentEncodeCore(
		pData, iSize, &Safe, false,
		sOutput, iCapacity, pOutputSize, bTerminate
	);
}



/* 按 RFC 3986 编码字节，并支持调用方缓冲和同址原地扩张。 */
XRT_API bool xrtPercentEncode(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	return __xrtPercentWrite(
		pData, iSize, ExtraSafe,
		sOutput, iCapacity, pOutputSize, true
	);
}



/* 写出不带零结尾的 RFC 3986 编码片段。 */
XRT_API bool xrtPercentWrite(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	return __xrtPercentWrite(
		pData, iSize, ExtraSafe,
		sOutput, iCapacity, pOutputSize, false
	);
}



/* 严格解码百分号转义，并在格式或容量失败时保持输出不变。 */
XRT_API bool xrtPercentDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	return __xrtPercentDecodeCore(
		Text, false, pOutput, iCapacity, pOutputSize, "percent-decode"
	);
}



/* 分配并返回零结尾的百分号编码文本。 */
XRT_API str xrtPercentEncodeNew(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	size_t* pOutputSize
)
{
	str sOutput;
	size_t iOutputSize;

	if (
		!__xrtRangeValid(pOutputSize, pOutputSize != NULL ?
			sizeof(*pOutputSize) : 0) ||
		((pOutputSize != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pData, iSize
	)) || ((pOutputSize != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), ExtraSafe.Data, ExtraSafe.Size
	)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtPercentEncode(
		pData, iSize, ExtraSafe, NULL, 0, &iOutputSize
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iOutputSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtPercentEncode(
		pData, iSize, ExtraSafe, sOutput, iOutputSize + 1u, &iOutputSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iOutputSize;
	}
	return sOutput;
}



/* 分配解码字节并保留一个不计入长度的末尾零哨兵。 */
XRT_API bytes xrtPercentDecodeNew(
	xstrview Text,
	size_t* pOutputSize
)
{
	bytes pOutput;
	size_t iOutputSize;

	if (
		!__xrtRangeValid(Text.Data, Text.Size) ||
		!__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		__xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtPercentDecode(Text, NULL, 0, &iOutputSize) ) {
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
	if ( !xrtPercentDecode(
		Text, pOutput, iOutputSize, &iOutputSize
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iOutputSize] = 0;
	*pOutputSize = iOutputSize;
	return pOutput;
}

#endif
