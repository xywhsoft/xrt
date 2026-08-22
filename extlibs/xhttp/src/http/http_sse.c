#include "../internal/xrt_http.h"

#include <xrt/http_sse.h>



#if defined(XHTTP_FEATURE_HTTP_SSE)

#define XRT_HTTP_SSE_EVENT_FLAGS \
	((uint32)XHTTP_SSE_EVENT_DATA | \
	 (uint32)XHTTP_SSE_EVENT_TYPE | \
	 (uint32)XHTTP_SSE_EVENT_ID | \
	 (uint32)XHTTP_SSE_EVENT_RETRY)



/* 安全累加封包长度。 */
static bool __xrtHttpSseSizeAdd(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 判断 UTF-8 视图有效且没有指定的单行禁用字符。 */
static bool __xrtHttpSseTextValid(
	xstrview Text,
	bool bAllowLf,
	bool bAllowNull
)
{
	size_t i;

	if ( !__xrtHttpViewValid(Text) ||
		!xrtUtf8Valid(Text, NULL) ) {
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == (unsigned char)'\r') ||
			(!bAllowLf && (iByte == (unsigned char)'\n')) ||
			(!bAllowNull && (iByte == 0)) ) {
			return false;
		}
	}
	return true;
}



/* 计算一条带可选空格的字段行。 */
static bool __xrtHttpSseFieldSize(
	size_t iName,
	size_t iValue,
	size_t* pSize
)
{
	return __xrtHttpSseSizeAdd(pSize, iName) &&
		__xrtHttpSseSizeAdd(pSize, 1u) &&
		((iValue == 0) || __xrtHttpSseSizeAdd(pSize, 1u)) &&
		__xrtHttpSseSizeAdd(pSize, iValue) &&
		__xrtHttpSseSizeAdd(pSize, 1u);
}



/* 按 LF 切分多行值并计算每一条规范字段行。 */
static bool __xrtHttpSseLinesSize(
	size_t iName,
	xstrview Text,
	size_t* pSize
)
{
	size_t iStart = 0;

	for ( ;; ) {
		size_t iEnd = iStart;

		while ( (iEnd < Text.Size) &&
			(Text.Data[iEnd] != '\n') ) {
			iEnd++;
		}
		if ( !__xrtHttpSseFieldSize(
			iName, iEnd - iStart, pSize
		) ) {
			return false;
		}
		if ( iEnd == Text.Size ) {
			return true;
		}
		iStart = iEnd + 1u;
	}
}



/* 写出一条带可选空格的字段行。 */
static size_t __xrtHttpSseFieldWrite(
	bytes pOutput,
	size_t iPosition,
	cstr sName,
	size_t iName,
	xstrview Value
)
{
	memcpy(pOutput + iPosition, sName, iName);
	iPosition += iName;
	pOutput[iPosition++] = (uint8)':';
	if ( Value.Size != 0 ) {
		pOutput[iPosition++] = (uint8)' ';
		memcpy(pOutput + iPosition, Value.Data, Value.Size);
		iPosition += Value.Size;
	}
	pOutput[iPosition++] = (uint8)'\n';
	return iPosition;
}



/* 按 LF 切分并写出全部规范字段行。 */
static size_t __xrtHttpSseLinesWrite(
	bytes pOutput,
	size_t iPosition,
	cstr sName,
	size_t iName,
	xstrview Text
)
{
	size_t iStart = 0;

	for ( ;; ) {
		size_t iEnd = iStart;

		while ( (iEnd < Text.Size) &&
			(Text.Data[iEnd] != '\n') ) {
			iEnd++;
		}
		iPosition = __xrtHttpSseFieldWrite(
			pOutput,
			iPosition,
			sName,
			iName,
			(xstrview){ Text.Data + iStart, iEnd - iStart }
		);
		if ( iEnd == Text.Size ) {
			return iPosition;
		}
		iStart = iEnd + 1u;
	}
}



/* 读取并验证可能未对齐的事件固定描述符。 */
static bool __xrtHttpSseEventResolve(
	const xhttpsseevent* pEvent,
	xhttpsseevent* pValue
)
{
	if ( !__xrtRangeValid(pEvent, sizeof(*pValue)) ) {
		return false;
	}
	memcpy(pValue, pEvent, sizeof(*pValue));
	if ( (pValue->Flags & ~XRT_HTTP_SSE_EVENT_FLAGS) != 0 ) {
		return false;
	}
	if ( ((pValue->Flags & XHTTP_SSE_EVENT_TYPE) != 0) &&
		!__xrtHttpSseTextValid(pValue->Type, false, true) ) {
		return false;
	}
	if ( ((pValue->Flags & XHTTP_SSE_EVENT_DATA) != 0) &&
		!__xrtHttpSseTextValid(pValue->Data, true, true) ) {
		return false;
	}
	if ( ((pValue->Flags & XHTTP_SSE_EVENT_ID) != 0) &&
		!xrtHttpSseLastEventIdValid(pValue->Id) ) {
		return false;
	}
	return true;
}



/* 精确计算一个已经验证的完整事件块。 */
static bool __xrtHttpSseEventMeasure(
	const xhttpsseevent* pEvent,
	size_t* pSize
)
{
	size_t iRequired = 0;

	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_ID) != 0) &&
		!__xrtHttpSseFieldSize(2u, pEvent->Id.Size, &iRequired) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_TYPE) != 0) &&
		!__xrtHttpSseFieldSize(5u, pEvent->Type.Size, &iRequired) ) {
		return false;
	}
	if ( (pEvent->Flags & XHTTP_SSE_EVENT_RETRY) != 0 ) {
		if ( !__xrtHttpSseFieldSize(
			5u, __xrtHttpUInt64Size(pEvent->Retry), &iRequired
		) ) {
			return false;
		}
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_DATA) != 0) &&
		!__xrtHttpSseLinesSize(4u, pEvent->Data, &iRequired) ) {
		return false;
	}
	if ( !__xrtHttpSseSizeAdd(&iRequired, 1u) ) {
		return false;
	}
	*pSize = iRequired;
	return true;
}



/* 判断事件字段及其显式存在标志一致。 */
XRT_API bool xrtHttpSseEventValid(const xhttpsseevent* pEvent)
{
	xhttpsseevent Event;

	return __xrtHttpSseEventResolve(pEvent, &Event);
}



/* 精确计算一个完整事件块。 */
XRT_API bool xrtHttpSseEventSize(
	const xhttpsseevent* pEvent,
	size_t* pSize
)
{
	xhttpsseevent Event;
	size_t iRequired;

	if ( !__xrtHttpSseEventResolve(pEvent, &Event) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), pEvent, sizeof(Event)
		) || (((Event.Flags & XHTTP_SSE_EVENT_TYPE) != 0) &&
			__xrtRangesOverlap(
				pSize, sizeof(iRequired),
				Event.Type.Data, Event.Type.Size
			)) || (((Event.Flags & XHTTP_SSE_EVENT_DATA) != 0) &&
			__xrtRangesOverlap(
				pSize, sizeof(iRequired),
				Event.Data.Data, Event.Data.Size
			)) || (((Event.Flags & XHTTP_SSE_EVENT_ID) != 0) &&
			__xrtRangesOverlap(
				pSize, sizeof(iRequired),
				Event.Id.Data, Event.Id.Size
			)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpSseEventMeasure(&Event, &iRequired) ) {
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 判断事件写出区域不覆盖描述符或借用字段。 */
static bool __xrtHttpSseEventOutputValid(
	const xhttpsseevent* pSource,
	const xhttpsseevent* pEvent,
	const void* pOutput,
	size_t iSize,
	const size_t* pSize
)
{
	if ( !__xrtRangeValid(pOutput, iSize) ||
		__xrtRangesOverlap(
			pOutput, iSize, pSource, sizeof(*pEvent)
		) || __xrtRangesOverlap(
			pOutput, iSize, pSize, sizeof(*pSize)
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_TYPE) != 0) &&
		__xrtRangesOverlap(
			pOutput, iSize, pEvent->Type.Data, pEvent->Type.Size
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_DATA) != 0) &&
		__xrtRangesOverlap(
			pOutput, iSize, pEvent->Data.Data, pEvent->Data.Size
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_ID) != 0) &&
		__xrtRangesOverlap(
			pOutput, iSize, pEvent->Id.Data, pEvent->Id.Size
		) ) {
		return false;
	}
	return true;
}



/* 判断长度输出不覆盖事件描述符或借用字段。 */
static bool __xrtHttpSseEventSizeValid(
	const xhttpsseevent* pSource,
	const xhttpsseevent* pEvent,
	const size_t* pSize
)
{
	if ( !__xrtRangeValid(pSize, sizeof(*pSize)) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pSource, sizeof(*pEvent)
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_TYPE) != 0) &&
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pEvent->Type.Data, pEvent->Type.Size
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_DATA) != 0) &&
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pEvent->Data.Data, pEvent->Data.Size
		) ) {
		return false;
	}
	if ( ((pEvent->Flags & XHTTP_SSE_EVENT_ID) != 0) &&
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pEvent->Id.Data, pEvent->Id.Size
		) ) {
		return false;
	}
	return true;
}



/* 写出一个完整事件块。 */
XRT_API bool xrtHttpSseEventWrite(
	const xhttpsseevent* pEvent,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpsseevent Event;
	bytes pBytes = (bytes)pOutput;
	size_t iRequired;
	size_t iPosition = 0;
	char arrRetry[20];

	if ( !__xrtHttpSseEventResolve(pEvent, &Event) ||
		!__xrtHttpSseEventSizeValid(pEvent, &Event, pSize) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpSseEventMeasure(&Event, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		if ( iCapacity != 0 ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !__xrtHttpSseEventOutputValid(
		pEvent, &Event, pOutput, iRequired, pSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}

	if ( (Event.Flags & XHTTP_SSE_EVENT_ID) != 0 ) {
		iPosition = __xrtHttpSseFieldWrite(
			pBytes, iPosition, "id", 2u, Event.Id
		);
	}
	if ( (Event.Flags & XHTTP_SSE_EVENT_TYPE) != 0 ) {
		iPosition = __xrtHttpSseFieldWrite(
			pBytes, iPosition, "event", 5u, Event.Type
		);
	}
	if ( (Event.Flags & XHTTP_SSE_EVENT_RETRY) != 0 ) {
		size_t iRetry = __xrtHttpUInt64Write(
			arrRetry, Event.Retry
		);

		iPosition = __xrtHttpSseFieldWrite(
			pBytes,
			iPosition,
			"retry",
			5u,
			(xstrview){ arrRetry, iRetry }
		);
	}
	if ( (Event.Flags & XHTTP_SSE_EVENT_DATA) != 0 ) {
		iPosition = __xrtHttpSseLinesWrite(
			pBytes, iPosition, "data", 4u, Event.Data
		);
	}
	pBytes[iPosition] = (uint8)'\n';
	return true;
}



/* 构建零结尾事件块。 */
XRT_API str xrtHttpSseEventBuild(
	const xhttpsseevent* pEvent,
	size_t* pSize
)
{
	xhttpsseevent Event;
	str sOutput;
	size_t iSize;

	if ( !__xrtHttpSseEventResolve(pEvent, &Event) ||
		((pSize != NULL) &&
		 !__xrtHttpSseEventSizeValid(pEvent, &Event, pSize)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpSseEventWrite(
		&Event, NULL, 0, &iSize
	) ) {
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpSseEventWrite(
		&Event, sOutput, iSize, &iSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iSize] = 0;
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}



/* 精确计算注释心跳。 */
XRT_API bool xrtHttpSseCommentSize(
	xstrview Comment,
	size_t* pSize
)
{
	size_t iRequired = 0;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		!__xrtHttpSseTextValid(Comment, true, true) ||
		__xrtRangesOverlap(
			pSize, sizeof(iRequired), Comment.Data, Comment.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpSseLinesSize(0, Comment, &iRequired) ) {
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 写出一条或多条注释心跳。 */
XRT_API bool xrtHttpSseCommentWrite(
	xstrview Comment,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired = 0;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		!__xrtHttpSseTextValid(Comment, true, true) ||
		__xrtRangesOverlap(
		pSize, sizeof(iRequired), Comment.Data, Comment.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpSseLinesSize(0, Comment, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		if ( iCapacity != 0 ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !__xrtRangeValid(pOutput, iRequired) ||
		__xrtRangesOverlap(
		pOutput, iRequired, Comment.Data, Comment.Size
	) || __xrtRangesOverlap(
		pOutput, iRequired, pSize, sizeof(*pSize)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	(void)__xrtHttpSseLinesWrite(
		(bytes)pOutput, 0, "", 0, Comment
	);
	return true;
}



/* 构建零结尾注释心跳。 */
XRT_API str xrtHttpSseCommentBuild(
	xstrview Comment,
	size_t* pSize
)
{
	str sOutput;
	size_t iSize;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		 __xrtRangesOverlap(
			pSize, sizeof(*pSize), Comment.Data, Comment.Size
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpSseCommentWrite(
		Comment, NULL, 0, &iSize
	) ) {
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpSseCommentWrite(
		Comment, sOutput, iSize, &iSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iSize] = 0;
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}



/* 判断 Last-Event-ID 是否可安全复用为请求字段值。 */
XRT_API bool xrtHttpSseLastEventIdValid(xstrview Id)
{
	return __xrtHttpSseTextValid(Id, false, false);
}

#endif
