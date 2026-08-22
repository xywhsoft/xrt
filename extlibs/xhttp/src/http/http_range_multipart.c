#include "../internal/xrt_http_semantics.h"



#if defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART)

/* canonical Part 固定使用两个表示字段。 */
static const char sXrtHttpRangeMultipartType[] =
	"Content-Type: ";
static const char sXrtHttpRangeMultipartRange[] =
	"Content-Range: ";



/* 安全累加 size_t 线缆长度。 */
static bool __xrtHttpRangeMultipartSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 安全累加公开的 64 位正文长度。 */
static bool __xrtHttpRangeMultipartLengthAdd(
	uint64* pLength,
	uint64 iAdd
)
{
	if ( *pLength > (UINT64_MAX - iAdd) ) {
		__xrtErrorSetRange();
		return false;
	}
	*pLength += iAdd;
	return true;
}



/* 空媒体类型采用稳定的二进制默认值。 */
static xstrview __xrtHttpRangeMultipartType(
	xstrview ContentType
)
{
	return ContentType.Size != 0 ?
		ContentType :
		XRT_STR_LITERAL("application/octet-stream");
}



/* 验证媒体类型字段值和无需引号的安全 boundary。 */
static bool __xrtHttpRangeMultipartConfigValid(
	xstrview ContentType,
	xstrview Boundary
)
{
	if ( !__xrtHttpViewValid(ContentType) ||
		!__xrtHttpViewValid(Boundary) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	ContentType = __xrtHttpRangeMultipartType(
		ContentType
	);
	if ( !xrtHttpFieldValueValid(ContentType) ||
		(Boundary.Size > 70u) ||
		!xrtHttpTokenValid(Boundary) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 验证一个完整表示内的非空字节区间。 */
static bool __xrtHttpRangeMultipartRangeValid(
	const xhttpbyterange* pRange,
	uint64 iCompleteLength
)
{
	return (pRange != NULL) &&
		(pRange->First <= pRange->Last) &&
		(pRange->Last < iCompleteLength);
}



/* 测量一个 Part 头部的完整线缆长度。 */
static bool __xrtHttpRangeMultipartHeadMeasure(
	const xhttpbyterange* pRange,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	size_t* pSize,
	xhttpcontentrange* pContentRange,
	size_t* pRangeSize
)
{
	xhttpcontentrange ContentRange;
	size_t iSize = 0;
	size_t iRangeSize;

	if ( !__xrtHttpRangeMultipartConfigValid(
		ContentType,
		Boundary
	) ) {
		return false;
	}
	if ( pRange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRangeMultipartRangeValid(
		pRange,
		iCompleteLength
	) ) {
		__xrtErrorSetRange();
		return false;
	}
	memset(&ContentRange, 0, sizeof(ContentRange));
	ContentRange.Satisfied = true;
	ContentRange.HasLength = true;
	ContentRange.First = pRange->First;
	ContentRange.Last = pRange->Last;
	ContentRange.Length = iCompleteLength;
	if ( !xrtHttpContentRangeWrite(
		&ContentRange,
		NULL,
		0,
		&iRangeSize
	) ) {
		return false;
	}
	ContentType = __xrtHttpRangeMultipartType(
		ContentType
	);
	if ( !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			2u
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			Boundary.Size
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			2u
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			sizeof(sXrtHttpRangeMultipartType) - 1u
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			ContentType.Size
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			2u
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			sizeof(sXrtHttpRangeMultipartRange) - 1u
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			iRangeSize
		) || !__xrtHttpRangeMultipartSizeAdd(
			&iSize,
			4u
		) ) {
		return false;
	}
	*pSize = iSize;
	if ( pContentRange != NULL ) {
		*pContentRange = ContentRange;
	}
	if ( pRangeSize != NULL ) {
		*pRangeSize = iRangeSize;
	}
	return true;
}



/* 验证排序后的范围数组并计算完整 multipart 正文长度。 */
bool __xrtHttpRangeMultipartLength(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	uint64* pLength
)
{
	xhttpbyterange Current;
	xhttpbyterange Previous;
	uint64 iLength = 0;
	size_t iRangeBytes;
	size_t i;

	if ( (pLength == NULL) || (iRangeCount == 0) ||
		(iRangeCount >
		 (SIZE_MAX / sizeof(*pRanges))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iRangeBytes = iRangeCount * sizeof(*pRanges);
	if ( !__xrtRangeValid(pRanges, iRangeBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRangeMultipartConfigValid(
		ContentType,
		Boundary
	) ) {
		return false;
	}
	for ( i = 0; i < iRangeCount; i++ ) {
		size_t iHeadSize;
		uint64 iDataSize;

		memcpy(
			&Current,
			((const uint8*)pRanges) +
				(i * sizeof(Current)),
			sizeof(Current)
		);
		if ( !__xrtHttpRangeMultipartRangeValid(
				&Current,
				iCompleteLength
			) || ((i != 0) &&
			 (Previous.Last >= Current.First)) ) {
			__xrtErrorSetRange();
			return false;
		}
		if ( !__xrtHttpRangeMultipartHeadMeasure(
			&Current,
			iCompleteLength,
			ContentType,
			Boundary,
			&iHeadSize,
			NULL,
			NULL
		) ) {
			return false;
		}
#if SIZE_MAX > UINT64_MAX
		if ( iHeadSize > (size_t)UINT64_MAX ) {
			__xrtErrorSetRange();
			return false;
		}
#endif
		iDataSize =
			(Current.Last - Current.First) +
			UINT64_C(1);
		if ( !__xrtHttpRangeMultipartLengthAdd(
				&iLength,
				(uint64)iHeadSize
			) || !__xrtHttpRangeMultipartLengthAdd(
				&iLength,
				iDataSize
			) || !__xrtHttpRangeMultipartLengthAdd(
				&iLength,
				UINT64_C(2)
			) ) {
			return false;
		}
		Previous = Current;
	}
#if SIZE_MAX > UINT64_MAX
	if ( Boundary.Size > ((size_t)UINT64_MAX - 6u) ) {
		__xrtErrorSetRange();
		return false;
	}
#endif
	if ( !__xrtHttpRangeMultipartLengthAdd(
		&iLength,
		(uint64)Boundary.Size + UINT64_C(6)
	) ) {
		return false;
	}
	*pLength = iLength;
	return true;
}



/* 判断输出描述符和缓冲区没有覆盖输入元数据。 */
static bool __xrtHttpRangeMultipartOutputValid(
	const void* pInput,
	size_t iInputSize,
	xstrview ContentType,
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize
)
{
	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			pInput,
			iInputSize
		) || __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			ContentType.Data,
			ContentType.Size
		) || __xrtRangesOverlap(
			pSize,
			sizeof(*pSize),
			Boundary.Data,
			Boundary.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( (iCapacity >= iRequired) &&
		!__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
			pOutput,
			iRequired,
			pInput,
			iInputSize
		) || __xrtRangesOverlap(
			pOutput,
			iRequired,
			ContentType.Data,
			ContentType.Size
		) || __xrtRangesOverlap(
			pOutput,
			iRequired,
			Boundary.Data,
			Boundary.Size
		) || __xrtRangesOverlap(
			pOutput,
			iRequired,
			pSize,
			sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	return true;
}



/* 计算 canonical multipart/byteranges 正文长度。 */
XRT_API bool xrtHttpRangeMultipartLength(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	uint64* pLength
)
{
	uint64 iLength;

	if ( !__xrtRangeValid(pLength, sizeof(iLength)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRangeMultipartLength(
		pRanges,
		iRangeCount,
		iCompleteLength,
		ContentType,
		Boundary,
		&iLength
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
			pLength,
			sizeof(*pLength),
			pRanges,
			iRangeCount * sizeof(*pRanges)
		) || __xrtRangesOverlap(
			pLength,
			sizeof(*pLength),
			ContentType.Data,
			ContentType.Size
		) || __xrtRangesOverlap(
			pLength,
			sizeof(*pLength),
			Boundary.Data,
			Boundary.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pLength, &iLength, sizeof(iLength));
	return true;
}



/* 写出一个范围 Part 的头部。 */
XRT_API bool xrtHttpRangeMultipartHeadWrite(
	const xhttpbyterange* pRange,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpbyterange Range;
	xhttpcontentrange ContentRange;
	size_t iRequired;
	size_t iRangeSize;
	size_t iWritten;
	size_t iOffset = 0;
	bytes pBytes = (bytes)pOutput;

	if ( !__xrtRangeValid(pRange, sizeof(Range)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Range, pRange, sizeof(Range));
	if ( !__xrtHttpRangeMultipartHeadMeasure(
			&Range,
			iCompleteLength,
			ContentType,
			Boundary,
			&iRequired,
			&ContentRange,
			&iRangeSize
		) || !__xrtHttpRangeMultipartOutputValid(
			pRange,
			sizeof(Range),
			ContentType,
			Boundary,
			pOutput,
			iCapacity,
			iRequired,
			pSize
		) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	ContentType = __xrtHttpRangeMultipartType(
		ContentType
	);
	memcpy(pBytes + iOffset, "--", 2u);
	iOffset += 2u;
	memcpy(
		pBytes + iOffset,
		Boundary.Data,
		Boundary.Size
	);
	iOffset += Boundary.Size;
	memcpy(pBytes + iOffset, "\r\n", 2u);
	iOffset += 2u;
	memcpy(
		pBytes + iOffset,
		sXrtHttpRangeMultipartType,
		sizeof(sXrtHttpRangeMultipartType) - 1u
	);
	iOffset +=
		sizeof(sXrtHttpRangeMultipartType) - 1u;
	memcpy(
		pBytes + iOffset,
		ContentType.Data,
		ContentType.Size
	);
	iOffset += ContentType.Size;
	memcpy(pBytes + iOffset, "\r\n", 2u);
	iOffset += 2u;
	memcpy(
		pBytes + iOffset,
		sXrtHttpRangeMultipartRange,
		sizeof(sXrtHttpRangeMultipartRange) - 1u
	);
	iOffset +=
		sizeof(sXrtHttpRangeMultipartRange) - 1u;
	if ( !xrtHttpContentRangeWrite(
		&ContentRange,
		pBytes + iOffset,
		iRangeSize,
		&iWritten
	) || (iWritten != iRangeSize) ) {
		return false;
	}
	iOffset += iWritten;
	memcpy(pBytes + iOffset, "\r\n\r\n", 4u);
	iOffset += 4u;
	if ( iOffset != iRequired ) {
		__xrtErrorSetInternal();
		return false;
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 写出范围数据后的 CRLF。 */
XRT_API bool xrtHttpRangeMultipartEndWrite(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired = 2u;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && __xrtRangesOverlap(
			pOutput,
			2u,
			pSize,
			sizeof(iRequired)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < 2u ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtRangeValid(pOutput, iRequired) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, "\r\n", 2u);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 写出 multipart 的关闭 boundary。 */
XRT_API bool xrtHttpRangeMultipartCloseWrite(
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired;
	bytes pBytes = (bytes)pOutput;

	if ( !__xrtHttpRangeMultipartConfigValid(
		XRT_STR_LITERAL("application/octet-stream"),
		Boundary
	) ) {
		return false;
	}
	iRequired = Boundary.Size + 6u;
	if ( !__xrtHttpRangeMultipartOutputValid(
			NULL,
			0,
			(xstrview){ NULL, 0 },
			Boundary,
			pOutput,
			iCapacity,
			iRequired,
			pSize
		) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	memcpy(pBytes, "--", 2u);
	memcpy(pBytes + 2u, Boundary.Data, Boundary.Size);
	memcpy(
		pBytes + 2u + Boundary.Size,
		"--\r\n",
		4u
	);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
