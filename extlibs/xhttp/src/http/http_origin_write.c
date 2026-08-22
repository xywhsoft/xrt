#include "../internal/xrt_http_origin.h"



#if defined(XHTTP_FEATURE_HTTP_ORIGIN_WRITE)

/* Origin Writer 在测量与写入阶段共享同一条追加路径。 */
typedef struct xrt_http_origin_writer {
	bytes Output;
	size_t Size;
} xrt_http_origin_writer;



/* 从可能未对齐的数组读取 Origin 描述符。 */
static void __xrtHttpOriginLoad(
	const xhttporigin* pOrigins,
	size_t iIndex,
	xhttporigin* pOrigin
)
{
	memcpy(
		pOrigin,
		(const uint8*)pOrigins +
		(iIndex * sizeof(*pOrigins)),
		sizeof(*pOrigin)
	);
}



/* 向 Writer 追加已经验证的字节。 */
static bool __xrtHttpOriginWriterBytes(
	xrt_http_origin_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( !__xrtHttpSizeAdd(&pWriter->Size, iSize) ) {
		return false;
	}
	if ( pWriter->Output != NULL ) {
		memcpy(
			pWriter->Output + pWriter->Size - iSize,
			pData,
			iSize
		);
	}
	return true;
}



/* 以 ASCII 小写形式追加 scheme 或 host。 */
static bool __xrtHttpOriginWriterLower(
	xrt_http_origin_writer* pWriter,
	xstrview Text
)
{
	size_t iStart = pWriter->Size;
	size_t i;

	if ( !__xrtHttpSizeAdd(&pWriter->Size, Text.Size) ) {
		return false;
	}
	if ( pWriter->Output == NULL ) {
		return true;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		pWriter->Output[iStart + i] =
			__xhttpAsciiLower((uint8)Text.Data[i]);
	}
	return true;
}



/* 判断规范写出是否需要保留显式端口。 */
static bool __xrtHttpOriginPortWrite(const xurl* pUrl)
{
	uint16 iDefault;

	if ( (pUrl->Flags & XURL_HAS_PORT) == 0 ) {
		return false;
	}
	if ( (pUrl->Flags & XURL_PORT_EMPTY) != 0 ) {
		return false;
	}
	iDefault = xrtUrlDefaultPort(pUrl->Scheme);
	return (iDefault == 0) || (pUrl->Port != iDefault);
}



/* 写出一个已经验证的 Origin。 */
static bool __xrtHttpOriginWriteOne(
	xrt_http_origin_writer* pWriter,
	const xhttporigin* pOrigin
)
{
	char sPort[20];
	size_t iPort;

	if ( (pOrigin->Flags & XHTTP_ORIGIN_NULL) != 0 ) {
		return __xrtHttpOriginWriterBytes(
			pWriter, "null", 4u
		);
	}
	if ( !__xrtHttpOriginWriterLower(
		pWriter, pOrigin->Url.Scheme
	) || !__xrtHttpOriginWriterBytes(
		pWriter, "://", 3u
	) ) {
		return false;
	}
	if ( (pOrigin->Url.Flags & XURL_HOST_IP_LITERAL) != 0 ) {
		if ( !__xrtHttpOriginWriterBytes(
			pWriter, "[", 1u
		) || !__xrtHttpOriginWriterLower(
			pWriter, pOrigin->Url.Host
		) || !__xrtHttpOriginWriterBytes(
			pWriter, "]", 1u
		) ) {
			return false;
		}
	} else if ( !__xrtHttpOriginWriterLower(
		pWriter, pOrigin->Url.Host
	) ) {
		return false;
	}
	if ( !__xrtHttpOriginPortWrite(&pOrigin->Url) ) {
		return true;
	}
	iPort = __xrtHttpUInt64Write(
		sPort, (uint64)pOrigin->Url.Port
	);
	return __xrtHttpOriginWriterBytes(
		pWriter, ":", 1u
	) && __xrtHttpOriginWriterBytes(
		pWriter, sPort, iPort
	);
}



/* 验证列表、测量长度并拒绝生成不规范的相邻重复项。 */
static bool __xrtHttpOriginListMeasure(
	const xhttporigin* pOrigins,
	size_t iCount,
	size_t* pRequired
)
{
	xrt_http_origin_writer Writer;
	xhttporigin Previous;
	xhttporigin Origin;
	size_t iBytes;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(Origin))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBytes = iCount * sizeof(Origin);
	if ( !__xrtRangeValid(pOrigins, iBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Writer, 0, sizeof(Writer));
	memset(&Previous, 0, sizeof(Previous));
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpOriginLoad(pOrigins, i, &Origin);
		if ( !__xrtHttpOriginValueValid(&Origin) ||
			(((Origin.Flags & XHTTP_ORIGIN_NULL) != 0) &&
			 (iCount != 1u)) ||
			((i != 0) &&
			 ((Previous.Flags & XHTTP_ORIGIN_NULL) == 0) &&
			 ((Origin.Flags & XHTTP_ORIGIN_NULL) == 0) &&
			 __xrtHttpOriginTupleSame(&Previous, &Origin)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( ((i != 0) && !__xrtHttpOriginWriterBytes(
			&Writer, " ", 1u
		)) || !__xrtHttpOriginWriteOne(
			&Writer, &Origin
		) ) {
			return false;
		}
		Previous = Origin;
	}
	*pRequired = Writer.Size;
	return true;
}



/* 判断输出是否覆盖 Origin 数组或任一借用组件。 */
static bool __xrtHttpOriginListOverlap(
	const xhttporigin* pOrigins,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	xhttporigin Origin;
	size_t iBytes = iCount * sizeof(Origin);
	size_t i;

	if ( __xrtRangesOverlap(
		pOrigins, iBytes, pOutput, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpOriginLoad(pOrigins, i, &Origin);
		if ( __xrtHttpOriginOverlap(
			&Origin, pOutput, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 原子写出 Origin 数组。 */
XRT_API bool xrtHttpOriginListWrite(
	const xhttporigin* pOrigins,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xrt_http_origin_writer Writer;
	xhttporigin Origin;
	size_t iRequired;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 (!__xrtRangeValid(pOutput, iCapacity) ||
		  __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpOriginListMeasure(
		pOrigins, iCount, &iRequired
	) ) {
		return false;
	}
	if ( __xrtHttpOriginListOverlap(
		pOrigins, iCount, pSize, sizeof(iRequired)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtHttpOriginListOverlap(
			pOrigins, iCount, pOutput, iRequired
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Writer.Output = (bytes)pOutput;
	Writer.Size = 0;
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpOriginLoad(pOrigins, i, &Origin);
		if ( (i != 0) && !__xrtHttpOriginWriterBytes(
			&Writer, " ", 1u
		) ) {
			return false;
		}
		if ( !__xrtHttpOriginWriteOne(&Writer, &Origin) ) {
			return false;
		}
	}
	return true;
}



/* 原子写出一个 Origin。 */
XRT_API bool xrtHttpOriginWrite(
	const xhttporigin* pOrigin,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpOriginListWrite(
		pOrigin, 1u, pOutput, iCapacity, pSize
	);
}



/* 单次分配构建 Origin 字段值。 */
XRT_API str xrtHttpOriginBuild(
	const xhttporigin* pOrigins,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;
	size_t iWritten;

	if ( !xrtHttpOriginListWrite(
		pOrigins, iCount, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		 __xrtHttpOriginListOverlap(
			pOrigins, iCount, pSize, sizeof(iRequired)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpOriginListWrite(
		pOrigins, iCount, sOutput, iRequired, &iWritten
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iWritten] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iWritten, sizeof(iWritten));
	}
	return sOutput;
}

#endif
