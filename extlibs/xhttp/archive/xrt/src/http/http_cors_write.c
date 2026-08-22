#include "../internal/xrt_http_cors.h"



#if defined(XRT_FEATURE_HTTP_CORS_WRITE)

/* 双遍 writer 在计长和写出阶段共享完全相同的字段顺序。 */
typedef struct xrt_http_cors_writer {
	bytes Output;
	size_t Capacity;
	size_t Size;
} xrt_http_cors_writer;



/* 为 writer 保留字节并检查 size_t 溢出。 */
static bool __xrtHttpCorsWriterReserve(
	xrt_http_cors_writer* pWriter,
	size_t iSize
)
{
	if ( iSize > (SIZE_MAX - pWriter->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( (pWriter->Output != NULL) &&
		((pWriter->Size > pWriter->Capacity) ||
		 (iSize > (pWriter->Capacity - pWriter->Size))) ) {
		__xrtErrorSetRange();
		return false;
	}
	return true;
}



/* 追加固定或借用文本。 */
static bool __xrtHttpCorsWriterAppend(
	xrt_http_cors_writer* pWriter,
	const void* pData,
	size_t iSize
)
{
	if ( !__xrtHttpCorsWriterReserve(pWriter, iSize) ) {
		return false;
	}
	if ( (pWriter->Output != NULL) && (iSize != 0) ) {
		memcpy(pWriter->Output + pWriter->Size, pData, iSize);
	}
	pWriter->Size += iSize;
	return true;
}



/* 追加一个完整字段行。 */
static bool __xrtHttpCorsWriterField(
	xrt_http_cors_writer* pWriter,
	cstr pName,
	size_t iNameSize,
	xstrview Value
)
{
	return __xrtHttpCorsWriterAppend(
		pWriter, pName, iNameSize
	) && __xrtHttpCorsWriterAppend(
		pWriter, ": ", 2u
	) && __xrtHttpCorsWriterAppend(
		pWriter, Value.Data, Value.Size
	) && __xrtHttpCorsWriterAppend(
		pWriter, "\r\n", 2u
	);
}



/* 追加规范化 Origin 响应字段。 */
static bool __xrtHttpCorsWriterOrigin(
	xrt_http_cors_writer* pWriter,
	const xhttpcorsorigin* pOrigin
)
{
	static const char Name[] = "Access-Control-Allow-Origin: ";
	static const char Wildcard[] = "*\r\n";
	size_t iOriginSize = 0;
	size_t iWritten;

	if ( !__xrtHttpCorsWriterAppend(
		pWriter, Name, sizeof(Name) - 1u
	) ) {
		return false;
	}
	if ( (pOrigin->Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0 ) {
		return __xrtHttpCorsWriterAppend(
			pWriter, Wildcard, sizeof(Wildcard) - 1u
		);
	}
	if ( !xrtHttpOriginWrite(
		&pOrigin->Origin, NULL, 0, &iOriginSize
	) || (iOriginSize > (SIZE_MAX - 2u)) ||
		!__xrtHttpCorsWriterReserve(
		pWriter, iOriginSize + 2u
	) ) {
		if ( iOriginSize > (SIZE_MAX - 2u) ) {
			__xrtErrorSetSizeOverflow();
		}
		return false;
	}
	if ( pWriter->Output != NULL ) {
		if ( !xrtHttpOriginWrite(
			&pOrigin->Origin,
			pWriter->Output + pWriter->Size,
			pWriter->Capacity - pWriter->Size,
			&iWritten
		) || (iWritten != iOriginSize) ) {
			return false;
		}
	}
	pWriter->Size += iOriginSize;
	return __xrtHttpCorsWriterAppend(pWriter, "\r\n", 2u);
}



/* 追加请求头名称的规范化回显列表。 */
static bool __xrtHttpCorsWriterRequestHeaders(
	xrt_http_cors_writer* pWriter,
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pFields,
	size_t iCount
)
{
	static const char Name[] = "Access-Control-Allow-Headers: ";
	xhttpcorscursor Cursor;
	xstrview Header;
	xhttpnext Next;
	size_t iItems = 0;

	if ( !__xrtHttpCorsWriterAppend(
		pWriter, Name, sizeof(Name) - 1u
	) ) {
		return false;
	}
	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsRequestHeaderNext(
		pFields, iCount, &Cursor, &Header
	)) == XHTTP_NEXT_ITEM ) {
		if ( (iItems != 0) && !__xrtHttpCorsWriterAppend(
			pWriter, ", ", 2u
		) ) {
			return false;
		}
		if ( !__xrtHttpCorsWriterAppend(
			pWriter, Header.Data, Header.Size
		) ) {
			return false;
		}
		iItems++;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( iItems != pDecision->HeaderCount ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpCorsWriterAppend(pWriter, "\r\n", 2u);
}



/* 追加策略暴露字段名列表。 */
static bool __xrtHttpCorsWriterExposeHeaders(
	xrt_http_cors_writer* pWriter,
	const xhttpcorsdecision* pDecision
)
{
	static const char Name[] = "Access-Control-Expose-Headers: ";
	xstrview Header;
	size_t i;

	if ( !__xrtHttpCorsWriterAppend(
		pWriter, Name, sizeof(Name) - 1u
	) ) {
		return false;
	}
	for ( i = 0; i < pDecision->ExposeCount; i++ ) {
		memcpy(
			&Header, &pDecision->ExposeHeaders[i], sizeof(Header)
		);
		if ( (i != 0) && !__xrtHttpCorsWriterAppend(
			pWriter, ", ", 2u
		) ) {
			return false;
		}
		if ( !__xrtHttpCorsWriterAppend(
			pWriter, Header.Data, Header.Size
		) ) {
			return false;
		}
	}
	return __xrtHttpCorsWriterAppend(pWriter, "\r\n", 2u);
}



/* 将 uint64 转为无前导零十进制视图。 */
static xstrview __xrtHttpCorsSecondsText(
	uint64 iValue,
	char* pBuffer
)
{
	size_t iEnd = 20u;
	size_t i = iEnd;

	do {
		pBuffer[--i] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0 );
	return (xstrview){ pBuffer + i, iEnd - i };
}



/* 写出响应实际依赖的全部请求字段，防止共享缓存复用错误的预检结果。 */
static bool __xrtHttpCorsWriterVary(
	xrt_http_cors_writer* pWriter,
	const xhttpcorsdecision* pDecision
)
{
	static const char Name[] = "Vary: ";
	static const char Origin[] = "Origin";
	static const char Method[] =
		"Access-Control-Request-Method";
	static const char Headers[] =
		"Access-Control-Request-Headers";
	bool bItem = false;

	if ( !__xrtHttpCorsWriterAppend(
		pWriter, Name, sizeof(Name) - 1u
	) ) {
		return false;
	}
	if ( (pDecision->Flags &
		XHTTP_CORS_DECISION_VARY_ORIGIN) != 0 ) {
		if ( !__xrtHttpCorsWriterAppend(
			pWriter, Origin, sizeof(Origin) - 1u
		) ) {
			return false;
		}
		bItem = true;
	}
	if ( (pDecision->Flags &
		XHTTP_CORS_DECISION_PREFLIGHT) != 0 ) {
		if ( bItem && !__xrtHttpCorsWriterAppend(
			pWriter, ", ", 2u
		) ) {
			return false;
		}
		if ( !__xrtHttpCorsWriterAppend(
			pWriter, Method, sizeof(Method) - 1u
		) || !__xrtHttpCorsWriterAppend(
			pWriter, ", ", 2u
		) || !__xrtHttpCorsWriterAppend(
			pWriter, Headers, sizeof(Headers) - 1u
		) ) {
			return false;
		}
	}
	return __xrtHttpCorsWriterAppend(pWriter, "\r\n", 2u);
}



/* 执行一次计长或写出遍历。 */
static bool __xrtHttpCorsDecisionWritePass(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	xrt_http_cors_writer* pWriter
)
{
	static const char Credentials[] =
		"Access-Control-Allow-Credentials: true\r\n";
	char AgeBuffer[20];
	xstrview Age;

	if ( (pDecision->Flags &
		XHTTP_CORS_DECISION_ALLOW) == 0 ) {
		return true;
	}
	if ( !__xrtHttpCorsWriterOrigin(
		pWriter, &pDecision->AllowOrigin
	) ) {
		return false;
	}
	if ( ((pDecision->Flags &
		XHTTP_CORS_DECISION_CREDENTIALS) != 0) &&
		!__xrtHttpCorsWriterAppend(
			pWriter, Credentials, sizeof(Credentials) - 1u
		) ) {
		return false;
	}
	if ( ((pDecision->Flags &
		XHTTP_CORS_DECISION_PREFLIGHT) != 0) &&
		!__xrtHttpCorsWriterField(
			pWriter,
			"Access-Control-Allow-Methods",
			28u,
			pDecision->AllowMethod
		) ) {
		return false;
	}
	if ( ((pDecision->Flags &
		XHTTP_CORS_DECISION_ALLOW_HEADERS) != 0) &&
		!__xrtHttpCorsWriterRequestHeaders(
			pWriter,
			pDecision,
			pRequestFields,
			iRequestFieldCount
		) ) {
		return false;
	}
	if ( ((pDecision->Flags &
		XHTTP_CORS_DECISION_EXPOSE_HEADERS) != 0) &&
		!__xrtHttpCorsWriterExposeHeaders(
			pWriter, pDecision
		) ) {
		return false;
	}
	if ( (pDecision->Flags &
		XHTTP_CORS_DECISION_MAX_AGE) != 0 ) {
		Age = __xrtHttpCorsSecondsText(
			pDecision->MaxAge, AgeBuffer
		);
		if ( !__xrtHttpCorsWriterField(
			pWriter, "Access-Control-Max-Age", 22u, Age
		) ) {
			return false;
		}
	}
	if ( ((pDecision->Flags & (
		XHTTP_CORS_DECISION_VARY_ORIGIN |
		XHTTP_CORS_DECISION_PREFLIGHT
	)) != 0) && !__xrtHttpCorsWriterVary(
		pWriter, pDecision
	) ) {
		return false;
	}
	return true;
}



/* 直接写出完整 CORS 响应字段片段。 */
XRT_API bool xrtHttpCorsDecisionWrite(
	const xhttpcorsdecision* pInput,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpcorsdecision Decision;
	xrt_http_cors_writer Writer;
	size_t iRequired;

	if ( !__xrtRangeValid(pInput, sizeof(Decision)) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Decision, pInput, sizeof(Decision));
	if ( !__xrtHttpCorsDecisionValid(
		&Decision, pRequestFields, iRequestFieldCount
	) || __xrtRangesOverlap(
		pInput, sizeof(Decision), pSize, sizeof(iRequired)
	) || __xrtHttpCorsDecisionOverlap(
		&Decision,
		pRequestFields,
		iRequestFieldCount,
		pSize,
		sizeof(iRequired)
	) || ((pOutput != NULL) && __xrtRangesOverlap(
		pOutput, iCapacity, pSize, sizeof(iRequired)
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Writer.Output = NULL;
	Writer.Capacity = 0;
	Writer.Size = 0;
	if ( !__xrtHttpCorsDecisionWritePass(
		&Decision,
		pRequestFields,
		iRequestFieldCount,
		&Writer
	) ) {
		return false;
	}
	iRequired = Writer.Size;
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		pInput, sizeof(Decision), pOutput, iRequired
	) || __xrtHttpCorsDecisionOverlap(
		&Decision,
		pRequestFields,
		iRequestFieldCount,
		pOutput,
		iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Writer.Output = (bytes)pOutput;
	Writer.Capacity = iRequired;
	Writer.Size = 0;
	return __xrtHttpCorsDecisionWritePass(
		&Decision,
		pRequestFields,
		iRequestFieldCount,
		&Writer
	) && (Writer.Size == iRequired);
}

#endif
