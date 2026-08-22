#include "../internal/xrt_charset.h"



#if defined(XRT_FEATURE_CHARSET)

/* 按显式字节序写出一个 16 位码元。 */
static void __xrtCharsetWrite16(unsigned char* pData, uint16 iValue, bool bBigEndian)
{
	if ( bBigEndian ) {
		pData[0] = (unsigned char)(iValue >> 8);
		pData[1] = (unsigned char)iValue;
	} else {
		pData[0] = (unsigned char)iValue;
		pData[1] = (unsigned char)(iValue >> 8);
	}
}



/* 按显式字节序写出一个 32 位码元。 */
static void __xrtCharsetWrite32(unsigned char* pData, uint32 iValue, bool bBigEndian)
{
	if ( bBigEndian ) {
		pData[0] = (unsigned char)(iValue >> 24);
		pData[1] = (unsigned char)(iValue >> 16);
		pData[2] = (unsigned char)(iValue >> 8);
		pData[3] = (unsigned char)iValue;
	} else {
		pData[0] = (unsigned char)iValue;
		pData[1] = (unsigned char)(iValue >> 8);
		pData[2] = (unsigned char)(iValue >> 16);
		pData[3] = (unsigned char)(iValue >> 24);
	}
}



/* 从显式编码方案的字节流解码一个标量。 */
static xrt_utf_decode __xrtCharsetDecode(const unsigned char* pData, size_t iSize,
	xencoding Encoding)
{
	xrt_utf_decode Decode;

	if ( Encoding == XENCODING_UTF8 ) {
		return __xrtUtf8Decode(pData, iSize);
	}
	Decode.Status = XUTF_MORE;
	Decode.Scalar = 0;
	Decode.Read = iSize;

	/* UTF-16 先读取完整码元，再复用严格代理项检查。 */
	if ( (Encoding == XENCODING_UTF16_LE) ||
		 (Encoding == XENCODING_UTF16_BE) ) {
		uint16 arrUnits[2];
		xrt_utf_decode UnitDecode;

		if ( iSize < 2u ) {
			return Decode;
		}
		arrUnits[0] = __xrtCharsetRead16(pData,
			Encoding == XENCODING_UTF16_BE);
		if ( iSize >= 4u ) {
			arrUnits[1] = __xrtCharsetRead16(pData + 2,
				Encoding == XENCODING_UTF16_BE);
		}
		UnitDecode = __xrtUtf16Decode(arrUnits, iSize >= 4u ? 2u : 1u);
		UnitDecode.Read *= 2u;
		return UnitDecode;
	}

	/* UTF-32 的每个完整码元必须直接是 Unicode 标量值。 */
	if ( iSize < 4u ) {
		return Decode;
	}
	Decode.Scalar = __xrtCharsetRead32(pData,
		Encoding == XENCODING_UTF32_BE);
	Decode.Status = xrtUnicodeScalar(Decode.Scalar) ? XUTF_OK : XUTF_INVALID;
	Decode.Read = 4;
	return Decode;
}



/* 返回一个标量在目标编码中的字节数并按需写出。 */
static size_t __xrtCharsetEncode(uint32 iScalar, xencoding Encoding,
	unsigned char* pTarget)
{
	if ( Encoding == XENCODING_UTF8 ) {
		return __xrtUtf8Encode(iScalar, pTarget);
	}
	if ( (Encoding == XENCODING_UTF16_LE) ||
		 (Encoding == XENCODING_UTF16_BE) ) {
		uint16 arrUnits[2];
		size_t iCount = __xrtUtf16Encode(iScalar, arrUnits);

		if ( pTarget != NULL ) {
			for ( size_t i = 0; i < iCount; i++ ) {
				__xrtCharsetWrite16(pTarget + (i * 2u), arrUnits[i],
					Encoding == XENCODING_UTF16_BE);
			}
		}
		return iCount * 2u;
	}
	if ( pTarget != NULL ) {
		__xrtCharsetWrite32(pTarget, iScalar,
			Encoding == XENCODING_UTF32_BE);
	}
	return 4;
}



/* 检查一个值是否是受支持的 Unicode 编码方案。 */
static bool __xrtCharsetEncodingValid(xencoding Encoding)
{
	return (Encoding >= XENCODING_UTF8) &&
		(Encoding <= XENCODING_UTF32_BE);
}



/* 转码一遍；目标为空时仅计量，不进行任何分配。 */
static xutfresult __xrtCharsetConvert(xbytesview Source,
	xencoding SourceEncoding, xencoding TargetEncoding, xutfpolicy Policy,
	bytes pTarget, size_t iCapacity, size_t iPrefix)
{
	xutfresult Result;
	bool bMeasure = pTarget == NULL;

	Result.Status = XUTF_OK;
	Result.Read = 0;
	Result.Written = iPrefix;
	Result.Error = XRT_NPOS;
	if ( ((Source.Data == NULL) && (Source.Size != 0)) ||
		 !__xrtCharsetEncodingValid(SourceEncoding) ||
		 !__xrtCharsetEncodingValid(TargetEncoding) ||
		 ((pTarget == NULL) && (iCapacity != 0)) ||
		 ((Policy != XUTF_STRICT) && (Policy != XUTF_REPLACE)) ) {
		__xrtErrorSetInvalidArgument();
		Result.Status = XUTF_INVALID;
		Result.Error = 0;
		return Result;
	}
	if ( !bMeasure && (iPrefix > iCapacity) ) {
		Result.Status = XUTF_NO_SPACE;
		return Result;
	}
	if ( !bMeasure && __xrtRangesOverlap(
		Source.Data,
		Source.Size,
		pTarget,
		iCapacity
	) ) {
		__xrtErrorSetInvalidArgument();
		Result.Status = XUTF_INVALID;
		Result.Error = 0;
		return Result;
	}

	/* 每轮消费一个最大子部件，严格模式在首个错误处停止。 */
	while ( Result.Read < Source.Size ) {
		xrt_utf_decode Decode;
		uint32 iScalar;
		size_t iNeed;

		/* UTF-8 的 ASCII 主路径无需进入通用变长解码器。 */
		if ( (SourceEncoding == XENCODING_UTF8) &&
			 (Source.Data[Result.Read] < 0x80u) ) {
			Decode.Status = XUTF_OK;
			Decode.Scalar = Source.Data[Result.Read];
			Decode.Read = 1;
		} else {
			Decode = __xrtCharsetDecode(Source.Data + Result.Read,
				Source.Size - Result.Read, SourceEncoding);
		}
		iScalar = Decode.Scalar;
		if ( Decode.Status != XUTF_OK ) {
			if ( Policy == XUTF_STRICT ) {
				Result.Status = XUTF_INVALID;
				Result.Error = Result.Read;
				__xrtUtfSetInvalid("transcode", Result.Error);
				return Result;
			}
			iScalar = 0xFFFDu;
		}
		iNeed = __xrtCharsetEncode(iScalar, TargetEncoding, NULL);
		if ( iNeed > (SIZE_MAX - Result.Written) ) {
			Result.Status = XUTF_OVERFLOW;
			__xrtUtfSetOverflow("transcode");
			return Result;
		}
		if ( !bMeasure && (iNeed > (iCapacity - Result.Written)) ) {
			Result.Status = XUTF_NO_SPACE;
			return Result;
		}
		if ( !bMeasure ) {
			(void)__xrtCharsetEncode(iScalar, TargetEncoding,
				pTarget + Result.Written);
		}
		Result.Read += Decode.Read;
		Result.Written += iNeed;
	}
	return Result;
}



/* 返回编码方案的码元字节数。 */
XRT_API size_t xrtEncodingUnitSize(xencoding Encoding)
{
	if ( Encoding == XENCODING_UTF8 ) {
		return 1;
	}
	if ( (Encoding == XENCODING_UTF16_LE) ||
		 (Encoding == XENCODING_UTF16_BE) ) {
		return 2;
	}
	if ( (Encoding == XENCODING_UTF32_LE) ||
		 (Encoding == XENCODING_UTF32_BE) ) {
		return 4;
	}
	return 0;
}



/* 检查开头的 Unicode BOM，较长的 UTF-32 标记优先。 */
XRT_API xencoding xrtEncodingBom(xbytesview Data, size_t* pSize)
{
	if ( ((Data.Data == NULL) && (Data.Size != 0)) ||
		 ((pSize != NULL) && __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			Data.Data,
			Data.Size
		)) ) {
		__xrtErrorSetInvalidArgument();
		return XENCODING_UNKNOWN;
	}
	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( Data.Size >= 4u ) {
		if ( (Data.Data[0] == 0xFFu) && (Data.Data[1] == 0xFEu) &&
			 (Data.Data[2] == 0x00u) && (Data.Data[3] == 0x00u) ) {
			if ( pSize != NULL ) {
				*pSize = 4;
			}
			return XENCODING_UTF32_LE;
		}
		if ( (Data.Data[0] == 0x00u) && (Data.Data[1] == 0x00u) &&
			 (Data.Data[2] == 0xFEu) && (Data.Data[3] == 0xFFu) ) {
			if ( pSize != NULL ) {
				*pSize = 4;
			}
			return XENCODING_UTF32_BE;
		}
	}
	if ( (Data.Size >= 3u) && (Data.Data[0] == 0xEFu) &&
		 (Data.Data[1] == 0xBBu) && (Data.Data[2] == 0xBFu) ) {
		if ( pSize != NULL ) {
			*pSize = 3;
		}
		return XENCODING_UTF8;
	}
	if ( (Data.Size >= 2u) && (Data.Data[0] == 0xFFu) &&
		 (Data.Data[1] == 0xFEu) ) {
		if ( pSize != NULL ) {
			*pSize = 2;
		}
		return XENCODING_UTF16_LE;
	}
	if ( (Data.Size >= 2u) && (Data.Data[0] == 0xFEu) &&
		 (Data.Data[1] == 0xFFu) ) {
		if ( pSize != NULL ) {
			*pSize = 2;
		}
		return XENCODING_UTF16_BE;
	}
	return XENCODING_UNKNOWN;
}



/* 写出指定编码的 BOM。 */
XRT_API size_t xrtEncodingWriteBom(xencoding Encoding, bytes pTarget,
	size_t iCapacity)
{
	static const unsigned char arrBom[][4] = {
		{ 0, 0, 0, 0 },
		{ 0xEFu, 0xBBu, 0xBFu, 0 },
		{ 0xFFu, 0xFEu, 0, 0 },
		{ 0xFEu, 0xFFu, 0, 0 },
		{ 0xFFu, 0xFEu, 0x00u, 0x00u },
		{ 0x00u, 0x00u, 0xFEu, 0xFFu }
	};
	size_t iSize;

	if ( !__xrtCharsetEncodingValid(Encoding) ||
		 ((pTarget == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iSize = Encoding == XENCODING_UTF8 ? 3u :
		(xrtEncodingUnitSize(Encoding));
	if ( pTarget == NULL ) {
		return iSize;
	}
	if ( iCapacity < iSize ) {
		__xrtErrorSetRange();
		return 0;
	}
	memcpy(pTarget, arrBom[Encoding], iSize);
	return iSize;
}



/* 在五种 Unicode 编码方案之间转码。 */
XRT_API bytes xrtTranscode(xbytesview Source, xencoding SourceEncoding,
	xencoding TargetEncoding, xutfpolicy Policy, bool bWriteBom, size_t* pSize)
{
	xutfresult Measure;
	xutfresult Convert;
	size_t iBomSize;
	size_t iUnitSize;
	size_t iAllocation;
	bytes pOutput;

	/* 所有参数在修改可选输出前完成校验，失败保持调用方数据不变。 */
	if ( ((Source.Data == NULL) && (Source.Size != 0)) ||
		 !__xrtCharsetEncodingValid(SourceEncoding) ||
		 !__xrtCharsetEncodingValid(TargetEncoding) ||
		 ((Policy != XUTF_STRICT) && (Policy != XUTF_REPLACE)) ||
		 ((pSize != NULL) && __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			Source.Data,
			Source.Size
		)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSize != NULL ) {
		*pSize = 0;
	}
	iBomSize = bWriteBom ?
		xrtEncodingWriteBom(TargetEncoding, NULL, 0) : 0;
	Measure = __xrtCharsetConvert(Source, SourceEncoding, TargetEncoding,
		Policy, NULL, 0, iBomSize);
	if ( Measure.Status != XUTF_OK ) {
		return NULL;
	}
	iUnitSize = xrtEncodingUnitSize(TargetEncoding);
	if ( (iUnitSize == 0) || (Measure.Written > (SIZE_MAX - iUnitSize)) ) {
		__xrtUtfSetOverflow("transcode");
		return NULL;
	}
	iAllocation = Measure.Written + iUnitSize;
	pOutput = (bytes)xrtMalloc(iAllocation);
	if ( pOutput == NULL ) {
		return NULL;
	}

	/* BOM 与正文共享一次精确分配，末尾补一个目标码元宽度的零。 */
	if ( iBomSize != 0 ) {
		(void)xrtEncodingWriteBom(TargetEncoding, pOutput, iBomSize);
	}
	Convert = __xrtCharsetConvert(Source, SourceEncoding, TargetEncoding,
		Policy, pOutput, Measure.Written, iBomSize);
	if ( Convert.Status != XUTF_OK ) {
		xrtFree(pOutput);
		return NULL;
	}
	memset(pOutput + Convert.Written, 0, iUnitSize);
	if ( pSize != NULL ) {
		*pSize = Convert.Written;
	}
	return pOutput;
}

#endif
