#include "../internal/xrt_charset.h"

#include <stdio.h>



#if defined(XRT_FEATURE_UNICODE)

/* 内部转换器使用统一的码元类型，避免维护六套近似循环。 */
typedef enum xrt_utf_unit {
	XRT_UTF_UNIT_8 = 1,
	XRT_UTF_UNIT_16 = 2,
	XRT_UTF_UNIT_32 = 4
} xrt_utf_unit;



/* 创建一个尚未读取和写入数据的转换结果。 */
static xutfresult __xrtUtfResult(void)
{
	xutfresult Result;

	Result.Status = XUTF_OK;
	Result.Read = 0;
	Result.Written = 0;
	Result.Error = XRT_NPOS;
	return Result;
}



/* 检查借用视图的指针和长度组合。 */
static bool __xrtUtfViewValid(const void* pData, size_t iSize)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 返回输入开头连续 ASCII 字节数，供常见纯 ASCII 路径跳过逐标量解码。 */
static size_t __xrtUtf8AsciiPrefix(const unsigned char* pText, size_t iSize)
{
	size_t i = 0;

	while ( (i < iSize) && (pText[i] <= 0x7Fu) ) {
		i++;
	}
	return i;
}



/* 根据源码元类型解码一个 Unicode 标量。 */
static xrt_utf_decode __xrtUtfDecodeUnit(const void* pSource, size_t iSize,
	xrt_utf_unit Unit)
{
	if ( Unit == XRT_UTF_UNIT_8 ) {
		return __xrtUtf8Decode((const unsigned char*)pSource, iSize);
	}
	if ( Unit == XRT_UTF_UNIT_16 ) {
		return __xrtUtf16Decode((const uint16*)pSource, iSize);
	}
	return __xrtUtf32Decode((const uint32*)pSource, iSize);
}



/* 根据目标码元类型返回编码长度并按需写出标量。 */
static size_t __xrtUtfEncodeUnit(uint32 iScalar, void* pTarget, xrt_utf_unit Unit)
{
	if ( Unit == XRT_UTF_UNIT_8 ) {
		return __xrtUtf8Encode(iScalar, (unsigned char*)pTarget);
	}
	if ( Unit == XRT_UTF_UNIT_16 ) {
		return __xrtUtf16Encode(iScalar, (uint16*)pTarget);
	}
	if ( pTarget != NULL ) {
		*(uint32*)pTarget = iScalar;
	}
	return 1;
}



/* 按目标码元宽度计算写入地址。 */
static void* __xrtUtfTargetAt(void* pTarget, size_t iPosition, xrt_utf_unit Unit)
{
	if ( Unit == XRT_UTF_UNIT_8 ) {
		return (unsigned char*)pTarget + iPosition;
	}
	if ( Unit == XRT_UTF_UNIT_16 ) {
		return (uint16*)pTarget + iPosition;
	}
	return (uint32*)pTarget + iPosition;
}



/* 按源码元宽度计算读取地址。 */
static const void* __xrtUtfSourceAt(const void* pSource, size_t iPosition,
	xrt_utf_unit Unit)
{
	if ( Unit == XRT_UTF_UNIT_8 ) {
		return (const unsigned char*)pSource + iPosition;
	}
	if ( Unit == XRT_UTF_UNIT_16 ) {
		return (const uint16*)pSource + iPosition;
	}
	return (const uint32*)pSource + iPosition;
}



/* 用一条标量管线完成任意 UTF 码元宽度之间的转换。 */
static xutfresult __xrtUtfConvert(const void* pSource, size_t iSourceSize,
	void* pTarget, size_t iCapacity, xrt_utf_unit SourceUnit,
	xrt_utf_unit TargetUnit, xutfpolicy Policy, cstr sOperation)
{
	xutfresult Result = __xrtUtfResult();
	bool bMeasure = pTarget == NULL;
	size_t iSourceBytes;
	size_t iTargetBytes = 0;

	if ( !__xrtUtfViewValid(pSource, iSourceSize) ||
		 ((pTarget == NULL) && (iCapacity != 0)) ||
		 ((Policy != XUTF_STRICT) && (Policy != XUTF_REPLACE)) ) {
		if ( ((pTarget == NULL) && (iCapacity != 0)) ||
			 ((Policy != XUTF_STRICT) && (Policy != XUTF_REPLACE)) ) {
			__xrtErrorSetInvalidArgument();
		}
		Result.Status = XUTF_INVALID;
		Result.Error = 0;
		return Result;
	}
	if ( (iSourceSize > (SIZE_MAX / (size_t)SourceUnit)) ||
		(!bMeasure && (iCapacity > (SIZE_MAX / (size_t)TargetUnit))) ) {
		Result.Status = XUTF_OVERFLOW;
		__xrtUtfSetOverflow(sOperation);
		return Result;
	}
	iSourceBytes = iSourceSize * (size_t)SourceUnit;
	if ( !bMeasure ) {
		iTargetBytes = iCapacity * (size_t)TargetUnit;
		if ( __xrtRangesOverlap(
			pSource, iSourceBytes, pTarget, iTargetBytes
		) ) {
			Result.Status = XUTF_INVALID;
			Result.Error = 0;
			__xrtErrorSetInvalidArgument();
			return Result;
		}
	}

	/* 每轮只处理一个标量，目标空间不足时保持源位置不变。 */
	while ( Result.Read < iSourceSize ) {
		xrt_utf_decode Decode;
		uint32 iScalar;
		size_t iNeed;

		if ( (SourceUnit == XRT_UTF_UNIT_8) &&
			(((const unsigned char*)pSource)[Result.Read] <= 0x7Fu) ) {
			Decode.Status = XUTF_OK;
			Decode.Scalar = ((const unsigned char*)pSource)[Result.Read];
			Decode.Read = 1;
		} else {
			Decode = __xrtUtfDecodeUnit(
				__xrtUtfSourceAt(pSource, Result.Read, SourceUnit),
				iSourceSize - Result.Read, SourceUnit);
		}
		iScalar = Decode.Scalar;

		if ( Decode.Status != XUTF_OK ) {
			if ( Policy == XUTF_STRICT ) {
				Result.Status = XUTF_INVALID;
				Result.Error = Result.Read;
				__xrtUtfSetInvalid(sOperation, Result.Error);
				return Result;
			}
			iScalar = 0xFFFDu;
		}

		iNeed = __xrtUtfEncodeUnit(iScalar, NULL, TargetUnit);
		if ( iNeed > (SIZE_MAX - Result.Written) ) {
			Result.Status = XUTF_OVERFLOW;
			__xrtUtfSetOverflow(sOperation);
			return Result;
		}
		if ( !bMeasure && (iNeed > (iCapacity - Result.Written)) ) {
			Result.Status = XUTF_NO_SPACE;
			return Result;
		}
		if ( !bMeasure ) {
			(void)__xrtUtfEncodeUnit(iScalar,
				__xrtUtfTargetAt(pTarget, Result.Written, TargetUnit), TargetUnit);
		}
		Result.Read += Decode.Read;
		Result.Written += iNeed;
	}
	return Result;
}



/* 分配目标字符串并复用缓冲区转换契约。 */
static ptr __xrtUtfConvertAlloc(const void* pSource, size_t iSourceSize,
	xrt_utf_unit SourceUnit, xrt_utf_unit TargetUnit, xutfpolicy Policy,
	size_t* pSize, cstr sOperation)
{
	xutfresult Measure;
	xutfresult Convert;
	size_t iBytes;
	size_t iSourceBytes;
	ptr pOutput;

	if ( iSourceSize > (SIZE_MAX / (size_t)SourceUnit) ) {
		__xrtUtfSetOverflow(sOperation);
		return NULL;
	}
	iSourceBytes = iSourceSize * (size_t)SourceUnit;
	if ( (pSize != NULL) && __xrtRangesOverlap(
		pSize, sizeof(*pSize), pSource, iSourceBytes
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSize != NULL ) {
		*pSize = 0;
	}
	Measure = __xrtUtfConvert(pSource, iSourceSize, NULL, 0, SourceUnit,
		TargetUnit, Policy, sOperation);
	if ( Measure.Status != XUTF_OK ) {
		return NULL;
	}
	if ( Measure.Written > ((SIZE_MAX / (size_t)TargetUnit) - 1u) ) {
		__xrtUtfSetOverflow(sOperation);
		return NULL;
	}
	iBytes = (Measure.Written + 1u) * (size_t)TargetUnit;
	pOutput = xrtMalloc(iBytes);
	if ( pOutput == NULL ) {
		return NULL;
	}

	/* 第二遍写入已经精确计量的缓冲区，并补一个完整零码元。 */
	Convert = __xrtUtfConvert(pSource, iSourceSize, pOutput, Measure.Written,
		SourceUnit, TargetUnit, Policy, sOperation);
	if ( Convert.Status != XUTF_OK ) {
		xrtFree(pOutput);
		return NULL;
	}
	memset(__xrtUtfTargetAt(pOutput, Convert.Written, TargetUnit), 0,
		(size_t)TargetUnit);
	if ( pSize != NULL ) {
		*pSize = Convert.Written;
	}
	return pOutput;
}



/* 严格解码一个 UTF-8 标量，并给出最大子部件长度。 */
xrt_utf_decode __xrtUtf8Decode(const unsigned char* pText, size_t iSize)
{
	xrt_utf_decode Decode;
	unsigned char iLead;
	size_t iNeed;
	unsigned char iSecondMin = 0x80u;
	unsigned char iSecondMax = 0xBFu;

	Decode.Status = XUTF_MORE;
	Decode.Scalar = 0;
	Decode.Read = iSize;
	if ( iSize == 0 ) {
		return Decode;
	}

	/* ASCII 是唯一的单字节 UTF-8 形式。 */
	iLead = pText[0];
	if ( iLead <= 0x7Fu ) {
		Decode.Status = XUTF_OK;
		Decode.Scalar = iLead;
		Decode.Read = 1;
		return Decode;
	}

	/* 首字节同时确定长度和第二字节的收紧范围。 */
	if ( (iLead >= 0xC2u) && (iLead <= 0xDFu) ) {
		iNeed = 2;
		Decode.Scalar = (uint32)(iLead & 0x1Fu);
	} else if ( (iLead >= 0xE0u) && (iLead <= 0xEFu) ) {
		iNeed = 3;
		Decode.Scalar = (uint32)(iLead & 0x0Fu);
		if ( iLead == 0xE0u ) {
			iSecondMin = 0xA0u;
		} else if ( iLead == 0xEDu ) {
			iSecondMax = 0x9Fu;
		}
	} else if ( (iLead >= 0xF0u) && (iLead <= 0xF4u) ) {
		iNeed = 4;
		Decode.Scalar = (uint32)(iLead & 0x07u);
		if ( iLead == 0xF0u ) {
			iSecondMin = 0x90u;
		} else if ( iLead == 0xF4u ) {
			iSecondMax = 0x8Fu;
		}
	} else {
		Decode.Status = XUTF_INVALID;
		Decode.Read = 1;
		return Decode;
	}

	/* 缺少第二字节时，现有内容仍可能是合法序列前缀。 */
	if ( iSize < 2u ) {
		Decode.Status = XUTF_MORE;
		Decode.Read = iSize;
		return Decode;
	}
	if ( (pText[1] < iSecondMin) || (pText[1] > iSecondMax) ) {
		Decode.Status = XUTF_INVALID;
		Decode.Read = 1;
		return Decode;
	}
	Decode.Scalar = (Decode.Scalar << 6) | (uint32)(pText[1] & 0x3Fu);

	/* 后续字节必须连续；错误前的合法前缀就是最大子部件。 */
	for ( size_t i = 2; i < iNeed; i++ ) {
		if ( i >= iSize ) {
			Decode.Status = XUTF_MORE;
			Decode.Read = iSize;
			return Decode;
		}
		if ( (pText[i] < 0x80u) || (pText[i] > 0xBFu) ) {
			Decode.Status = XUTF_INVALID;
			Decode.Read = i;
			return Decode;
		}
		Decode.Scalar = (Decode.Scalar << 6) | (uint32)(pText[i] & 0x3Fu);
	}
	Decode.Status = XUTF_OK;
	Decode.Read = iNeed;
	return Decode;
}



/* 严格解码一个 UTF-16 标量。 */
xrt_utf_decode __xrtUtf16Decode(const uint16* pText, size_t iSize)
{
	xrt_utf_decode Decode;
	uint32 iFirst;

	Decode.Status = XUTF_MORE;
	Decode.Scalar = 0;
	Decode.Read = iSize;
	if ( iSize == 0 ) {
		return Decode;
	}
	iFirst = pText[0];
	if ( (iFirst < 0xD800u) || (iFirst > 0xDFFFu) ) {
		Decode.Status = XUTF_OK;
		Decode.Scalar = iFirst;
		Decode.Read = 1;
		return Decode;
	}
	if ( iFirst >= 0xDC00u ) {
		Decode.Status = XUTF_INVALID;
		Decode.Read = 1;
		return Decode;
	}
	if ( iSize < 2u ) {
		Decode.Status = XUTF_MORE;
		Decode.Read = 1;
		return Decode;
	}
	if ( (pText[1] < 0xDC00u) || (pText[1] > 0xDFFFu) ) {
		Decode.Status = XUTF_INVALID;
		Decode.Read = 1;
		return Decode;
	}
	Decode.Status = XUTF_OK;
	Decode.Scalar = 0x10000u + ((iFirst - 0xD800u) << 10) +
		((uint32)pText[1] - 0xDC00u);
	Decode.Read = 2;
	return Decode;
}



/* 严格解码一个 UTF-32 标量。 */
xrt_utf_decode __xrtUtf32Decode(const uint32* pText, size_t iSize)
{
	xrt_utf_decode Decode;

	Decode.Status = XUTF_MORE;
	Decode.Scalar = 0;
	Decode.Read = iSize;
	if ( iSize == 0 ) {
		return Decode;
	}
	Decode.Read = 1;
	Decode.Scalar = pText[0];
	Decode.Status = xrtUnicodeScalar(Decode.Scalar) ? XUTF_OK : XUTF_INVALID;
	return Decode;
}



/* 无错误副作用地编码一个 UTF-8 标量。 */
size_t __xrtUtf8Encode(uint32 iScalar, unsigned char* pOutput)
{
	if ( !xrtUnicodeScalar(iScalar) ) {
		return 0;
	}
	if ( iScalar <= 0x7Fu ) {
		if ( pOutput != NULL ) {
			pOutput[0] = (unsigned char)iScalar;
		}
		return 1;
	}
	if ( iScalar <= 0x7FFu ) {
		if ( pOutput != NULL ) {
			pOutput[0] = (unsigned char)(0xC0u | (iScalar >> 6));
			pOutput[1] = (unsigned char)(0x80u | (iScalar & 0x3Fu));
		}
		return 2;
	}
	if ( iScalar <= 0xFFFFu ) {
		if ( pOutput != NULL ) {
			pOutput[0] = (unsigned char)(0xE0u | (iScalar >> 12));
			pOutput[1] = (unsigned char)(0x80u | ((iScalar >> 6) & 0x3Fu));
			pOutput[2] = (unsigned char)(0x80u | (iScalar & 0x3Fu));
		}
		return 3;
	}
	if ( pOutput != NULL ) {
		pOutput[0] = (unsigned char)(0xF0u | (iScalar >> 18));
		pOutput[1] = (unsigned char)(0x80u | ((iScalar >> 12) & 0x3Fu));
		pOutput[2] = (unsigned char)(0x80u | ((iScalar >> 6) & 0x3Fu));
		pOutput[3] = (unsigned char)(0x80u | (iScalar & 0x3Fu));
	}
	return 4;
}



/* 无错误副作用地编码一个 UTF-16 标量。 */
size_t __xrtUtf16Encode(uint32 iScalar, uint16* pOutput)
{
	if ( !xrtUnicodeScalar(iScalar) ) {
		return 0;
	}
	if ( iScalar <= 0xFFFFu ) {
		if ( pOutput != NULL ) {
			pOutput[0] = (uint16)iScalar;
		}
		return 1;
	}
	iScalar -= 0x10000u;
	if ( pOutput != NULL ) {
		pOutput[0] = (uint16)(0xD800u + (iScalar >> 10));
		pOutput[1] = (uint16)(0xDC00u + (iScalar & 0x3FFu));
	}
	return 2;
}



/* 设置带错误位置的 Unicode 值错误。 */
void __xrtUtfSetInvalid(cstr sOperation, size_t iOffset)
{
	char sData[64];
	xerrordesc Desc;
	xerror* pError;

	(void)snprintf(sData, sizeof(sData), "offset=%llu",
		(unsigned long long)iOffset);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_VALUE;
	Desc.Domain = "xrt.unicode";
	Desc.Code = XUTF_ERROR_INVALID;
	Desc.Operation = sOperation;
	Desc.Message = "invalid Unicode encoding";
	Desc.Data = sData;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 设置 Unicode 转换大小溢出错误。 */
void __xrtUtfSetOverflow(cstr sOperation)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_RANGE;
	Desc.Domain = "xrt.unicode";
	Desc.Code = XUTF_ERROR_OVERFLOW;
	Desc.Operation = sOperation;
	Desc.Message = "Unicode conversion size overflow";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 复制明确数量的宽码元并追加一个零码元。 */
static ptr __xrtUtfDupUnits(
	const void* pText,
	size_t iSize,
	size_t iUnitSize,
	cstr sOperation
)
{
	size_t iBytes;
	unsigned char* pCopy;

	if ( (pText == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iSize > ((SIZE_MAX / iUnitSize) - 1u) ) {
		__xrtUtfSetOverflow(sOperation);
		return NULL;
	}
	iBytes = iSize * iUnitSize;
	pCopy = (unsigned char*)xrtMalloc(iBytes + iUnitSize);
	if ( pCopy == NULL ) {
		return NULL;
	}
	if ( iBytes != 0 ) {
		memcpy(pCopy, pText, iBytes);
	}
	memset(pCopy + iBytes, 0, iUnitSize);
	return pCopy;
}



/* 从明确码元数创建 UTF-16 借用视图。 */
XRT_API xutf16view xrtUtf16View(const uint16* pText, size_t iSize)
{
	xutf16view Text;

	Text.Data = pText;
	Text.Size = iSize;
	return Text;
}



/* 从明确码元数创建 UTF-32 借用视图。 */
XRT_API xutf32view xrtUtf32View(const uint32* pText, size_t iSize)
{
	xutf32view Text;

	Text.Data = pText;
	Text.Size = iSize;
	return Text;
}



/* 返回零结尾 UTF-16 字符串的码元数。 */
XRT_API size_t xrtUtf16Len(const uint16* pText)
{
	size_t iSize = 0;

	if ( pText != NULL ) {
		while ( pText[iSize] != 0 ) {
			iSize++;
		}
	}
	return iSize;
}



/* 返回零结尾 UTF-32 字符串的码元数。 */
XRT_API size_t xrtUtf32Len(const uint32* pText)
{
	size_t iSize = 0;

	if ( pText != NULL ) {
		while ( pText[iSize] != 0 ) {
			iSize++;
		}
	}
	return iSize;
}



/* 复制零结尾 UTF-16 字符串。 */
XRT_API uint16* xrtUtf16Dup(const uint16* pText)
{
	return xrtUtf16DupView(xrtUtf16View(pText, xrtUtf16Len(pText)));
}



/* 复制 UTF-16 视图并追加零码元。 */
XRT_API uint16* xrtUtf16DupView(xutf16view Text)
{
	return (uint16*)__xrtUtfDupUnits(
		Text.Data,
		Text.Size,
		sizeof(uint16),
		"utf16-duplicate"
	);
}



/* 复制零结尾 UTF-32 字符串。 */
XRT_API uint32* xrtUtf32Dup(const uint32* pText)
{
	return xrtUtf32DupView(xrtUtf32View(pText, xrtUtf32Len(pText)));
}



/* 复制 UTF-32 视图并追加零码元。 */
XRT_API uint32* xrtUtf32DupView(xutf32view Text)
{
	return (uint32*)__xrtUtfDupUnits(
		Text.Data,
		Text.Size,
		sizeof(uint32),
		"utf32-duplicate"
	);
}



/* 判断数值是否是可编码的 Unicode 标量值。 */
XRT_API bool xrtUnicodeScalar(uint32 iScalar)
{
	return (iScalar <= 0x10FFFFu) &&
		((iScalar < 0xD800u) || (iScalar > 0xDFFFu));
}



/* 解码一个 UTF-8 标量。 */
XRT_API xutfstatus xrtUtf8Decode(xstrview Text, uint32* pScalar, size_t* pRead)
{
	xrt_utf_decode Decode;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) || (pScalar == NULL) ||
		__xrtRangesOverlap(pScalar, sizeof(*pScalar), Text.Data, Text.Size) ||
		((pRead != NULL) && (
		 __xrtRangesOverlap(pRead, sizeof(*pRead), Text.Data, Text.Size) ||
		 __xrtRangesOverlap(pRead, sizeof(*pRead), pScalar, sizeof(*pScalar))
		)) ) {
		__xrtErrorSetInvalidArgument();
		return XUTF_INVALID;
	}
	if ( pRead != NULL ) {
		*pRead = 0;
	}
	*pScalar = 0;
	Decode = __xrtUtf8Decode((const unsigned char*)Text.Data, Text.Size);
	*pScalar = Decode.Scalar;
	if ( pRead != NULL ) {
		*pRead = Decode.Read;
	}
	return Decode.Status;
}



/* 解码一个 UTF-16 标量。 */
XRT_API xutfstatus xrtUtf16Decode(xutf16view Text, uint32* pScalar, size_t* pRead)
{
	xrt_utf_decode Decode;
	size_t iTextBytes;

	if ( (Text.Size > (SIZE_MAX / sizeof(uint16))) ||
		!__xrtUtfViewValid(Text.Data, Text.Size) || (pScalar == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XUTF_INVALID;
	}
	iTextBytes = Text.Size * sizeof(uint16);
	if ( __xrtRangesOverlap(pScalar, sizeof(*pScalar), Text.Data, iTextBytes) ||
		((pRead != NULL) && (
		 __xrtRangesOverlap(pRead, sizeof(*pRead), Text.Data, iTextBytes) ||
		 __xrtRangesOverlap(pRead, sizeof(*pRead), pScalar, sizeof(*pScalar))
		)) ) {
		__xrtErrorSetInvalidArgument();
		return XUTF_INVALID;
	}
	if ( pRead != NULL ) {
		*pRead = 0;
	}
	*pScalar = 0;
	Decode = __xrtUtf16Decode(Text.Data, Text.Size);
	*pScalar = Decode.Scalar;
	if ( pRead != NULL ) {
		*pRead = Decode.Read;
	}
	return Decode.Status;
}



/* 把一个 Unicode 标量编码为 UTF-8。 */
XRT_API size_t xrtUtf8Encode(uint32 iScalar, char arrOutput[4])
{
	if ( arrOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtUnicodeScalar(iScalar) ) {
		__xrtUtfSetInvalid("utf8_encode", 0);
		return 0;
	}
	return __xrtUtf8Encode(iScalar, (unsigned char*)arrOutput);
}



/* 把一个 Unicode 标量编码为 UTF-16。 */
XRT_API size_t xrtUtf16Encode(uint32 iScalar, uint16 arrOutput[2])
{
	if ( arrOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtUnicodeScalar(iScalar) ) {
		__xrtUtfSetInvalid("utf16_encode", 0);
		return 0;
	}
	return __xrtUtf16Encode(iScalar, arrOutput);
}



/* 严格校验 UTF-8。 */
XRT_API bool xrtUtf8Valid(xstrview Text, size_t* pError)
{
	size_t iPosition = 0;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		if ( pError != NULL ) {
			*pError = 0;
		}
		return false;
	}
	if ( (pError != NULL) && __xrtRangesOverlap(
		pError, sizeof(*pError), Text.Data, Text.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pError != NULL ) {
		*pError = XRT_NPOS;
	}
	while ( iPosition < Text.Size ) {
		iPosition += __xrtUtf8AsciiPrefix(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition
		);
		if ( iPosition == Text.Size ) {
			break;
		}
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			if ( pError != NULL ) {
				*pError = iPosition;
			}
			return false;
		}
		iPosition += Decode.Read;
	}
	return true;
}



/* 严格校验 UTF-16。 */
XRT_API bool xrtUtf16Valid(xutf16view Text, size_t* pError)
{
	size_t iPosition = 0;
	size_t iTextBytes;

	if ( Text.Size > (SIZE_MAX / sizeof(uint16)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		if ( pError != NULL ) {
			*pError = 0;
		}
		return false;
	}
	iTextBytes = Text.Size * sizeof(uint16);
	if ( (pError != NULL) && __xrtRangesOverlap(
		pError, sizeof(*pError), Text.Data, iTextBytes
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pError != NULL ) {
		*pError = XRT_NPOS;
	}
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf16Decode(Text.Data + iPosition,
			Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			if ( pError != NULL ) {
				*pError = iPosition;
			}
			return false;
		}
		iPosition += Decode.Read;
	}
	return true;
}



/* 严格校验 UTF-32。 */
XRT_API bool xrtUtf32Valid(xutf32view Text, size_t* pError)
{
	size_t iTextBytes;

	if ( Text.Size > (SIZE_MAX / sizeof(uint32)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		if ( pError != NULL ) {
			*pError = 0;
		}
		return false;
	}
	iTextBytes = Text.Size * sizeof(uint32);
	if ( (pError != NULL) && __xrtRangesOverlap(
		pError, sizeof(*pError), Text.Data, iTextBytes
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pError != NULL ) {
		*pError = XRT_NPOS;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( !xrtUnicodeScalar(Text.Data[i]) ) {
			if ( pError != NULL ) {
				*pError = i;
			}
			return false;
		}
	}
	return true;
}



/* 统计 UTF-8 标量数。 */
XRT_API size_t xrtUtf8Count(xstrview Text)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		return XRT_NPOS;
	}
	while ( iPosition < Text.Size ) {
		size_t iAscii = __xrtUtf8AsciiPrefix(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition
		);

		iPosition += iAscii;
		iCount += iAscii;
		if ( iPosition == Text.Size ) {
			break;
		}
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			return XRT_NPOS;
		}
		iPosition += Decode.Read;
		iCount++;
	}
	return iCount;
}



/* 严格走过指定数量的 UTF-8 标量并返回字节位置。 */
static bool __xrtUtf8Seek(xstrview Text, size_t iIndex, size_t* pOffset,
	cstr sOperation)
{
	size_t iPosition = 0;
	size_t iCurrent = 0;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		return false;
	}
	while ( iCurrent < iIndex ) {
		xrt_utf_decode Decode;

		if ( iPosition == Text.Size ) {
			__xrtErrorSetRange();
			return false;
		}
		Decode = __xrtUtf8Decode((const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition);
		if ( Decode.Status != XUTF_OK ) {
			__xrtUtfSetInvalid(sOperation, iPosition);
			return false;
		}
		iPosition += Decode.Read;
		iCurrent++;
	}
	*pOffset = iPosition;
	return true;
}



/* 把 UTF-8 标量索引转换为字节偏移。 */
XRT_API size_t xrtUtf8Offset(xstrview Text, size_t iIndex)
{
	size_t iOffset;

	if ( !__xrtUtf8Seek(Text, iIndex, &iOffset, "utf8-offset") ) {
		return XRT_NPOS;
	}
	return iOffset;
}



/* 把 UTF-8 标量边界上的字节偏移转换为标量索引。 */
XRT_API size_t xrtUtf8Index(xstrview Text, size_t iOffset)
{
	size_t iPosition = 0;
	size_t iIndex = 0;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		return XRT_NPOS;
	}
	if ( iOffset > Text.Size ) {
		__xrtErrorSetRange();
		return XRT_NPOS;
	}
	while ( iPosition < iOffset ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			__xrtUtfSetInvalid("utf8-index", iPosition);
			return XRT_NPOS;
		}
		if ( Decode.Read > (iOffset - iPosition) ) {
			__xrtErrorSetRange();
			return XRT_NPOS;
		}
		iPosition += Decode.Read;
		iIndex++;
	}
	return iIndex;
}



/* 读取指定 UTF-8 标量索引处的标量值。 */
XRT_API bool xrtUtf8At(xstrview Text, size_t iIndex, uint32* pScalar)
{
	xrt_utf_decode Decode;
	size_t iOffset;

	if ( (pScalar == NULL) || !__xrtUtfViewValid(Text.Data, Text.Size) ||
		__xrtRangesOverlap(pScalar, sizeof(*pScalar), Text.Data, Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pScalar = 0;
	if ( !__xrtUtf8Seek(Text, iIndex, &iOffset, "utf8-at") ) {
		return false;
	}
	if ( iOffset == Text.Size ) {
		__xrtErrorSetRange();
		return false;
	}
	Decode = __xrtUtf8Decode((const unsigned char*)Text.Data + iOffset,
		Text.Size - iOffset);
	if ( Decode.Status != XUTF_OK ) {
		__xrtUtfSetInvalid("utf8-at", iOffset);
		return false;
	}
	*pScalar = Decode.Scalar;
	return true;
}



/* 按 UTF-8 标量索引返回借用切片。 */
XRT_API bool xrtUtf8Slice(xstrview Text, size_t iStart, size_t iCount,
	xstrview* pSlice)
{
	size_t iPosition = 0;
	size_t iIndex = 0;
	size_t iBegin;
	size_t iTaken = 0;

	if ( (pSlice == NULL) || !__xrtUtfViewValid(Text.Data, Text.Size) ||
		__xrtRangesOverlap(pSlice, sizeof(*pSlice), Text.Data, Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSlice->Data = NULL;
	pSlice->Size = 0;

	/* 起点和末端都按切片语义钳制，但经过的字节必须是严格 UTF-8。 */
	while ( (iIndex < iStart) && (iPosition < Text.Size) ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			__xrtUtfSetInvalid("utf8-slice", iPosition);
			return false;
		}
		iPosition += Decode.Read;
		iIndex++;
	}
	iBegin = iPosition;
	while ( (iPosition < Text.Size) &&
		((iCount == XRT_NPOS) || (iTaken < iCount)) ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			__xrtUtfSetInvalid("utf8-slice", iPosition);
			return false;
		}
		iPosition += Decode.Read;
		iTaken++;
	}
	pSlice->Data = Text.Data != NULL ? Text.Data + iBegin : NULL;
	pSlice->Size = iPosition - iBegin;
	return true;
}



/* 统计 UTF-16 标量数。 */
XRT_API size_t xrtUtf16Count(xutf16view Text)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ) {
		return XRT_NPOS;
	}
	while ( iPosition < Text.Size ) {
		xrt_utf_decode Decode = __xrtUtf16Decode(Text.Data + iPosition,
			Text.Size - iPosition);

		if ( Decode.Status != XUTF_OK ) {
			return XRT_NPOS;
		}
		iPosition += Decode.Read;
		iCount++;
	}
	return iCount;
}



/* 初始化流式 UTF-8 校验状态。 */
XRT_API void xrtUtf8StateInit(xutf8state* pState)
{
	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->Error = XRT_NPOS;
	pState->PendingOffset = XRT_NPOS;
}



/* 校验一个可能截断在任意字节位置的 UTF-8 分块。 */
XRT_API xutfstatus xrtUtf8StateFeed(xutf8state* pState, xstrview Text, bool bFinal)
{
	size_t iBase;
	size_t iPosition = 0;

	if ( pState == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XUTF_INVALID;
	}
	if ( !__xrtUtfViewValid(Text.Data, Text.Size) ||
		__xrtRangesOverlap(pState, sizeof(*pState), Text.Data, Text.Size) ||
		(pState->PendingSize > 3u) ||
		((pState->PendingSize != 0) &&
		 ((pState->PendingOffset == XRT_NPOS) ||
		  (pState->PendingOffset > pState->Total))) ) {
		__xrtErrorSetInvalidArgument();
		return XUTF_INVALID;
	}
	if ( pState->Failed ) {
		return XUTF_INVALID;
	}
	if ( Text.Size > (SIZE_MAX - pState->Total) ) {
		pState->Failed = true;
		pState->Error = pState->Total;
		__xrtUtfSetOverflow("utf8_state_feed");
		return XUTF_OVERFLOW;
	}
	iBase = pState->Total;
	pState->Total += Text.Size;

	/* 先用新分块补齐上一分块保留的合法前缀。 */
	while ( pState->PendingSize != 0 ) {
		xrt_utf_decode Decode = __xrtUtf8Decode(pState->Pending,
			pState->PendingSize);

		if ( Decode.Status == XUTF_OK ) {
			pState->PendingSize = 0;
			pState->PendingOffset = XRT_NPOS;
			break;
		}
		if ( Decode.Status == XUTF_INVALID ) {
			pState->Failed = true;
			pState->Error = pState->PendingOffset;
			return XUTF_INVALID;
		}
		if ( iPosition == Text.Size ) {
			if ( bFinal ) {
				pState->Failed = true;
				pState->Error = pState->PendingOffset;
				return XUTF_INVALID;
			}
			return XUTF_MORE;
		}
		pState->Pending[pState->PendingSize++] =
			(unsigned char)Text.Data[iPosition++];
	}

	/* 完整标量直接跨过，只在分块尾保存最多三个前缀字节。 */
	while ( iPosition < Text.Size ) {
		iPosition += __xrtUtf8AsciiPrefix(
			(const unsigned char*)Text.Data + iPosition,
			Text.Size - iPosition
		);
		if ( iPosition == Text.Size ) {
			break;
		}
		xrt_utf_decode Decode = __xrtUtf8Decode(
			(const unsigned char*)Text.Data + iPosition, Text.Size - iPosition);

		if ( Decode.Status == XUTF_OK ) {
			iPosition += Decode.Read;
			continue;
		}
		if ( Decode.Status == XUTF_INVALID ) {
			pState->Failed = true;
			pState->Error = iBase + iPosition;
			return XUTF_INVALID;
		}
		pState->PendingOffset = iBase + iPosition;
		pState->PendingSize = (uint8)(Text.Size - iPosition);
		memcpy(pState->Pending, Text.Data + iPosition, pState->PendingSize);
		iPosition = Text.Size;
	}
	if ( pState->PendingSize != 0 ) {
		if ( bFinal ) {
			pState->Failed = true;
			pState->Error = pState->PendingOffset;
			return XUTF_INVALID;
		}
		return XUTF_MORE;
	}
	return XUTF_OK;
}



/* 返回流式校验器记录的绝对错误位置。 */
XRT_API size_t xrtUtf8StateError(const xutf8state* pState)
{
	return pState != NULL ? pState->Error : XRT_NPOS;
}



/* UTF-8 转 UTF-16。 */
XRT_API xutfresult xrtUtf8To16Buffer(xstrview Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_8, XRT_UTF_UNIT_16, Policy, "utf8_to_utf16");
}



/* UTF-8 转 UTF-32。 */
XRT_API xutfresult xrtUtf8To32Buffer(xstrview Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_8, XRT_UTF_UNIT_32, Policy, "utf8_to_utf32");
}



/* UTF-16 转 UTF-8。 */
XRT_API xutfresult xrtUtf16To8Buffer(xutf16view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_16, XRT_UTF_UNIT_8, Policy, "utf16_to_utf8");
}



/* UTF-16 转 UTF-32。 */
XRT_API xutfresult xrtUtf16To32Buffer(xutf16view Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_16, XRT_UTF_UNIT_32, Policy, "utf16_to_utf32");
}



/* UTF-32 转 UTF-8。 */
XRT_API xutfresult xrtUtf32To8Buffer(xutf32view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_32, XRT_UTF_UNIT_8, Policy, "utf32_to_utf8");
}



/* UTF-32 转 UTF-16。 */
XRT_API xutfresult xrtUtf32To16Buffer(xutf32view Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy)
{
	return __xrtUtfConvert(Source.Data, Source.Size, pTarget, iCapacity,
		XRT_UTF_UNIT_32, XRT_UTF_UNIT_16, Policy, "utf32_to_utf16");
}



/* 严格转换零结尾 UTF-8。 */
XRT_API uint16* xrtUtf8To16(cstr sText, size_t* pSize)
{
	xstrview Source;

	Source.Data = sText;
	Source.Size = sText != NULL ? strlen(sText) : 0;
	return xrtUtf8ViewTo16(Source, XUTF_STRICT, pSize);
}



/* 严格转换零结尾 UTF-8。 */
XRT_API uint32* xrtUtf8To32(cstr sText, size_t* pSize)
{
	xstrview Source;

	Source.Data = sText;
	Source.Size = sText != NULL ? strlen(sText) : 0;
	return xrtUtf8ViewTo32(Source, XUTF_STRICT, pSize);
}



/* 严格转换零结尾 UTF-16。 */
XRT_API str xrtUtf16To8(const uint16* pText, size_t* pSize)
{
	return xrtUtf16ViewTo8(xrtUtf16View(pText, xrtUtf16Len(pText)),
		XUTF_STRICT, pSize);
}



/* 严格转换零结尾 UTF-16。 */
XRT_API uint32* xrtUtf16To32(const uint16* pText, size_t* pSize)
{
	return xrtUtf16ViewTo32(xrtUtf16View(pText, xrtUtf16Len(pText)),
		XUTF_STRICT, pSize);
}



/* 严格转换零结尾 UTF-32。 */
XRT_API str xrtUtf32To8(const uint32* pText, size_t* pSize)
{
	return xrtUtf32ViewTo8(xrtUtf32View(pText, xrtUtf32Len(pText)),
		XUTF_STRICT, pSize);
}



/* 严格转换零结尾 UTF-32。 */
XRT_API uint16* xrtUtf32To16(const uint32* pText, size_t* pSize)
{
	return xrtUtf32ViewTo16(xrtUtf32View(pText, xrtUtf32Len(pText)),
		XUTF_STRICT, pSize);
}



/* 分配零结尾 UTF-16 字符串。 */
XRT_API uint16* xrtUtf8ViewTo16(xstrview Source, xutfpolicy Policy, size_t* pSize)
{
	return (uint16*)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_8, XRT_UTF_UNIT_16, Policy, pSize, "utf8_to_utf16");
}



/* 分配零结尾 UTF-32 字符串。 */
XRT_API uint32* xrtUtf8ViewTo32(xstrview Source, xutfpolicy Policy, size_t* pSize)
{
	return (uint32*)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_8, XRT_UTF_UNIT_32, Policy, pSize, "utf8_to_utf32");
}



/* 分配零结尾 UTF-8 字符串。 */
XRT_API str xrtUtf16ViewTo8(xutf16view Source, xutfpolicy Policy, size_t* pSize)
{
	return (str)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_16, XRT_UTF_UNIT_8, Policy, pSize, "utf16_to_utf8");
}



/* 分配零结尾 UTF-32 字符串。 */
XRT_API uint32* xrtUtf16ViewTo32(xutf16view Source, xutfpolicy Policy, size_t* pSize)
{
	return (uint32*)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_16, XRT_UTF_UNIT_32, Policy, pSize, "utf16_to_utf32");
}



/* 分配零结尾 UTF-8 字符串。 */
XRT_API str xrtUtf32ViewTo8(xutf32view Source, xutfpolicy Policy, size_t* pSize)
{
	return (str)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_32, XRT_UTF_UNIT_8, Policy, pSize, "utf32_to_utf8");
}



/* 分配零结尾 UTF-16 字符串。 */
XRT_API uint16* xrtUtf32ViewTo16(xutf32view Source, xutfpolicy Policy, size_t* pSize)
{
	return (uint16*)__xrtUtfConvertAlloc(Source.Data, Source.Size,
		XRT_UTF_UNIT_32, XRT_UTF_UNIT_16, Policy, pSize, "utf32_to_utf16");
}

#endif
