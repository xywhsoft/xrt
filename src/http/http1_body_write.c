#include "../internal/xrt_http.h"

#include <xrt/http_trailer.h>



#if defined(XRT_FEATURE_HTTP1_BODY)

/* 把无符号长度写成不带前缀的最短小写十六进制文本。 */
static size_t __xrtHttp1ChunkHexWrite(uint64 iValue, bytes pOutput)
{
	static const char sHex[] = "0123456789abcdef";
	char sReverse[16];
	size_t iDigits = 0;
	size_t i;

	do {
		sReverse[iDigits++] = sHex[iValue & UINT64_C(0x0F)];
		iValue >>= 4;
	} while ( iValue != 0 );
	for ( i = 0; i < iDigits; i++ ) {
		pOutput[i] = (uint8)sReverse[iDigits - i - 1u];
	}
	return iDigits;
}



/* 返回十六进制长度需要的最短字符数。 */
static size_t __xrtHttp1ChunkHexSize(uint64 iValue)
{
	size_t iDigits = 1;

	while ( iValue >= UINT64_C(16) ) {
		iValue >>= 4;
		iDigits++;
	}
	return iDigits;
}



/* 写入一个可供向量发送复用的 chunk-size 行。 */
XRT_API bool xrtHttp1ChunkLineWrite(
	uint64 iSize,
	xstrview Extensions,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Parts[1];
	size_t iRequired = __xrtHttp1ChunkHexSize(iSize);
	size_t iPosition;
	bytes pBytes = (bytes)pOutput;

	if ( !__xrtHttp1ChunkExtensionsValid(Extensions) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_CHUNK_EXTENSION, 0, 0,
			XERR_VALUE, "write-http1-chunk-line",
			"HTTP/1 chunk extension suffix is invalid"
		);
		return false;
	}
	if ( !__xrtHttp1SizeAdd(&iRequired, Extensions.Size) ||
		!__xrtHttp1SizeAdd(&iRequired, 2) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, "write-http1-chunk-line",
			"HTTP/1 chunk line size overflows"
		);
		return false;
	}
	Parts[0] = Extensions;
	if ( !__xrtHttp1WriteOutputValid(
		Parts, 1, NULL, 0,
		pOutput, iCapacity, iRequired, pSize,
		"write-http1-chunk-line"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	iPosition = __xrtHttp1ChunkHexWrite(iSize, pBytes);
	if ( Extensions.Size != 0 ) {
		memcpy(pBytes + iPosition, Extensions.Data, Extensions.Size);
		iPosition += Extensions.Size;
	}
	memcpy(pBytes + iPosition, "\r\n", 2);
	*pSize = iPosition + 2u;
	return true;
}



/* 把一段正文复制封装为完整 chunk。 */
XRT_API bool xrtHttp1ChunkWrite(
	xbytesview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Parts[1];
	size_t iLine;
	size_t iRequired;
	size_t iPosition;
	bytes pBytes = (bytes)pOutput;

	if ( (Data.Data == NULL) && (Data.Size != 0) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, "write-http1-chunk",
			"HTTP/1 chunk data view is invalid"
		);
		return false;
	}
	Parts[0] = (xstrview){ (cstr)Data.Data, Data.Size };
	if ( Data.Size == 0 ) {
		if ( !__xrtHttp1WriteOutputValid(
			Parts, 1, NULL, 0,
			pOutput, iCapacity, 0, pSize,
			"write-http1-chunk"
		) ) {
			return false;
		}
		*pSize = 0;
		return true;
	}
	iLine = __xrtHttp1ChunkHexSize((uint64)Data.Size) + 2u;
	iRequired = iLine;
	if ( !__xrtHttp1SizeAdd(&iRequired, Data.Size) ||
		!__xrtHttp1SizeAdd(&iRequired, 2) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, "write-http1-chunk",
			"HTTP/1 chunk size overflows"
		);
		return false;
	}
	if ( !__xrtHttp1WriteOutputValid(
		Parts, 1, NULL, 0,
		pOutput, iCapacity, iRequired, pSize,
		"write-http1-chunk"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	iPosition = __xrtHttp1ChunkHexWrite((uint64)Data.Size, pBytes);
	memcpy(pBytes + iPosition, "\r\n", 2);
	iPosition += 2;
	memcpy(pBytes + iPosition, Data.Data, Data.Size);
	iPosition += Data.Size;
	memcpy(pBytes + iPosition, "\r\n", 2);
	*pSize = iPosition + 2u;
	return true;
}



/* 写入 last-chunk、标准 trailer 字段和最终空行。 */
XRT_API bool xrtHttp1ChunkEndWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired;
	size_t iPosition;
	bytes pBytes = (bytes)pOutput;

	if ( (pTrailers == NULL) && (iTrailerCount != 0) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, "write-http1-chunk-end",
			"HTTP/1 trailer array is null"
		);
		return false;
	}
	if ( !xrtHttpTrailerSectionValid(
		pTrailers, iTrailerCount
	) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_FORBIDDEN_TRAILER, 0, 0,
			XERR_VALUE, "write-http1-chunk-end",
			"HTTP/1 trailer section is invalid or forbidden"
		);
		return false;
	}
	if ( !__xrtHttp1WriteMeasure(
		pTrailers, iTrailerCount, 3, &iRequired,
		"write-http1-chunk-end"
	) ) {
		return false;
	}
	if ( !__xrtHttp1WriteOutputValid(
		NULL, 0, pTrailers, iTrailerCount,
		pOutput, iCapacity, iRequired, pSize,
		"write-http1-chunk-end"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	memcpy(pBytes, "0\r\n", 3);
	iPosition = __xrtHttp1FieldsWrite(
		pBytes, 3, pTrailers, iTrailerCount
	);
	*pSize = iPosition;
	return true;
}

#endif
