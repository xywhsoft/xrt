#include "../internal/xrt_http.h"

#include <xrt/http_sse.h>



#if defined(XHTTP_FEATURE_HTTP_SSE_PARSER)

#define XRT_HTTP_SSE_PARSER_OPEN UINT32_C(0x53534501)
#define XRT_HTTP_SSE_PARSER_FAILED UINT32_C(0x53534502)
#define XRT_HTTP_SSE_PARSER_DONE UINT32_C(0x53534503)

#define XRT_HTTP_SSE_FIRST UINT32_C(0x00000001)
#define XRT_HTTP_SSE_SKIP_LF UINT32_C(0x00000002)
#define XRT_HTTP_SSE_HOLD_LINE UINT32_C(0x00000004)
#define XRT_HTTP_SSE_HOLD_EVENT UINT32_C(0x00000008)

#define XRT_HTTP_SSE_LINE_DEFAULT ((size_t)65536u)
#define XRT_HTTP_SSE_DATA_DEFAULT ((size_t)1048576u)
#define XRT_HTTP_SSE_TYPE_DEFAULT ((size_t)1024u)
#define XRT_HTTP_SSE_ID_DEFAULT ((size_t)8192u)



/* 验证拥有动态缓冲的 Parser 本体自然对齐并完整可访问。 */
static bool __xrtHttpSseParserPointerValid(
	const xhttpsseparser* pParser
)
{
	return __xrtRangeValid(pParser, sizeof(*pParser)) &&
		(((uintptr_t)pParser %
		 XRT_INTERNAL_OBJECT_ALIGNOF(xhttpsseparser)) == 0);
}



/* 判断已经对齐的 Parser 配置值是否满足状态机约束。 */
static bool __xrtHttpSseParserConfigValueValid(
	const xhttpsseparserconfig* pConfig
)
{
	return (pConfig->LineLimit <= (SIZE_MAX - 3u)) &&
		(pConfig->DataLimit < SIZE_MAX) &&
		(pConfig->TypeLimit < SIZE_MAX) &&
		(pConfig->IdLimit < SIZE_MAX) &&
		((pConfig->Utf8Policy == XUTF_STRICT) ||
		 (pConfig->Utf8Policy == XUTF_REPLACE));
}



/* 读取并验证可能未对齐的 Parser 配置。 */
static bool __xrtHttpSseParserConfigResolve(
	const xhttpsseparserconfig* pConfig,
	xhttpsseparserconfig* pValue
)
{
	if ( !__xrtRangeValid(pConfig, sizeof(*pValue)) ) {
		return false;
	}
	memcpy(pValue, pConfig, sizeof(*pValue));
	return __xrtHttpSseParserConfigValueValid(pValue);
}



/* 判断公开缓冲描述符没有被调用方破坏。 */
static bool __xrtHttpSseBufferValid(const xbuffer* pBuffer)
{
	return (pBuffer != NULL) &&
		(pBuffer->Size <= pBuffer->Capacity) &&
		((pBuffer->Data != NULL) || (pBuffer->Capacity == 0)) &&
		__xrtRangeValid(pBuffer->Data, pBuffer->Capacity);
}



/* 判断 Parser 当前仍是可操作对象。 */
static bool __xrtHttpSseParserValid(const xhttpsseparser* pParser)
{
	const uint32 iFlags = XRT_HTTP_SSE_FIRST |
		XRT_HTTP_SSE_SKIP_LF |
		XRT_HTTP_SSE_HOLD_LINE |
		XRT_HTTP_SSE_HOLD_EVENT;

	return __xrtHttpSseParserPointerValid(pParser) &&
		((pParser->State == XRT_HTTP_SSE_PARSER_OPEN) ||
		 (pParser->State == XRT_HTTP_SSE_PARSER_FAILED) ||
		 (pParser->State == XRT_HTTP_SSE_PARSER_DONE)) &&
		__xrtHttpSseParserConfigValueValid(&pParser->Config) &&
		__xrtHttpSseBufferValid(&pParser->Line) &&
		__xrtHttpSseBufferValid(&pParser->Decoded) &&
		__xrtHttpSseBufferValid(&pParser->Data) &&
		__xrtHttpSseBufferValid(&pParser->Type) &&
		__xrtHttpSseBufferValid(&pParser->Id) &&
		(pParser->Line.Size <= (pParser->Config.LineLimit + 3u)) &&
		(pParser->Decoded.Size <= pParser->Config.LineLimit) &&
		(pParser->Data.Size <= (pParser->Config.DataLimit + 1u)) &&
		(pParser->Type.Size <= pParser->Config.TypeLimit) &&
		(pParser->Id.Size <= pParser->Config.IdLimit) &&
		(pParser->LineOffset <= pParser->Offset) &&
		(pParser->LineNumber != 0) &&
		((pParser->Flags & ~iFlags) == 0);
}



/* 对无法继续精确表示的位置执行饱和累加。 */
static size_t __xrtHttpSsePositionAdd(size_t iValue, size_t iAdd)
{
	return iAdd > (SIZE_MAX - iValue) ? SIZE_MAX : iValue + iAdd;
}



/* 返回稳定解析错误文本。 */
static cstr __xrtHttpSseErrorMessage(xhttpsseerror Code)
{
	switch ( Code ) {
		case XHTTP_SSE_ERROR_ARGUMENT:
			return "SSE parser arguments are invalid";

		case XHTTP_SSE_ERROR_STATE:
			return "SSE parser is not open";

		case XHTTP_SSE_ERROR_UTF8:
			return "SSE line contains invalid UTF-8";

		case XHTTP_SSE_ERROR_LINE_TOO_LARGE:
			return "SSE line exceeds its configured limit";

		case XHTTP_SSE_ERROR_DATA_TOO_LARGE:
			return "SSE event data exceeds its configured limit";

		case XHTTP_SSE_ERROR_TYPE_TOO_LARGE:
			return "SSE event type exceeds its configured limit";

		case XHTTP_SSE_ERROR_ID_TOO_LARGE:
			return "SSE event ID exceeds its configured limit";

		case XHTTP_SSE_ERROR_ALLOCATION:
			return "SSE parser could not grow a dynamic buffer";

		default:
			return "SSE parser failed";
	}
}



/* 发布解析错误并把当前响应固定为失败终态。 */
static xhttpsseparsestatus __xrtHttpSseFail(
	xhttpsseparser* pParser,
	xhttpsseerrorinfo* pError,
	xhttpsseerror Code,
	size_t iOffset,
	size_t iLine,
	xerrkind Kind
)
{
	const xhttpsseerrorinfo Error = {
		Code,
		iOffset,
		iLine
	};

	if ( pParser != NULL ) {
		pParser->State = XRT_HTTP_SSE_PARSER_FAILED;
	}
	if ( __xrtRangeValid(pError, sizeof(Error)) ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	if ( Code == XHTTP_SSE_ERROR_ALLOCATION ) {
		__xrtErrorWrapDetail(
			XERR_MEMORY,
			"xrt.http.sse",
			(int32)Code,
			"parse-sse",
			__xrtHttpSseErrorMessage(Code)
		);
	} else {
		__xrtErrorSetDetail(
			Kind,
			"xrt.http.sse",
			(int32)Code,
			"parse-sse",
			__xrtHttpSseErrorMessage(Code),
			NULL
		);
	}
	return XHTTP_SSE_PARSE_ERROR;
}



/* 判断两个借用文本视图逐字节相等。 */
static bool __xrtHttpSseEqual(xstrview Text, cstr sLiteral, size_t iSize)
{
	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, sLiteral, iSize) == 0));
}



/* 在大小限额内原子地替换一个 Parser 缓冲。 */
static bool __xrtHttpSseAssign(
	xhttpsseparser* pParser,
	xbuffer* pBuffer,
	xstrview Text,
	size_t iLimit,
	xhttpsseerror iLimitError,
	xhttpsseerrorinfo* pError
)
{
	if ( Text.Size > iLimit ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			iLimitError,
			pParser->LineOffset,
			pParser->LineNumber,
			XERR_RANGE
		);
		return false;
	}
	if ( !xrtBufferAssign(
		pBuffer, (xbytesview){ (const uint8*)Text.Data, Text.Size }
	) ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_ALLOCATION,
			pParser->LineOffset,
			pParser->LineNumber,
			XERR_MEMORY
		);
		return false;
	}
	return true;
}



/* 在一次预留后追加 data 值及其规范 LF。 */
static bool __xrtHttpSseDataAppend(
	xhttpsseparser* pParser,
	xstrview Value,
	xhttpsseerrorinfo* pError
)
{
	size_t iLogical;
	size_t iRequired;

	if ( Value.Size > (SIZE_MAX - pParser->Data.Size) ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_DATA_TOO_LARGE,
			pParser->LineOffset,
			pParser->LineNumber,
			XERR_RANGE
		);
		return false;
	}
	iLogical = pParser->Data.Size + Value.Size;
	if ( (iLogical > pParser->Config.DataLimit) ||
		(iLogical == SIZE_MAX) ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_DATA_TOO_LARGE,
			pParser->LineOffset,
			pParser->LineNumber,
			XERR_RANGE
		);
		return false;
	}
	iRequired = iLogical + 1u;
	if ( !xrtBufferReserve(&pParser->Data, iRequired) ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_ALLOCATION,
			pParser->LineOffset,
			pParser->LineNumber,
			XERR_MEMORY
		);
		return false;
	}
	if ( Value.Size != 0 ) {
		memcpy(
			pParser->Data.Data + pParser->Data.Size,
			Value.Data,
			Value.Size
		);
	}
	pParser->Data.Size = iLogical;
	pParser->Data.Data[pParser->Data.Size++] = (uint8)'\n';
	return true;
}



/* 把严格失败的 UTF-8 行按最大子部件替换为 U+FFFD。 */
static bool __xrtHttpSseReplaceLine(
	xhttpsseparser* pParser,
	xstrview Source,
	xhttpsseerrorinfo* pError,
	xstrview* pText
)
{
	static const uint8 Replacement[3] = { 0xEFu, 0xBFu, 0xBDu };
	size_t iPosition = 0;

	xrtBufferClear(&pParser->Decoded);
	while ( iPosition < Source.Size ) {
		uint32 iScalar;
		size_t iRead;
		xutfstatus Status = xrtUtf8Decode(
			(xstrview){
				Source.Data + iPosition,
				Source.Size - iPosition
			},
			&iScalar,
			&iRead
		);
		const uint8* pPart;
		size_t iPart;

		(void)iScalar;
		if ( Status == XUTF_OK ) {
			pPart = (const uint8*)Source.Data + iPosition;
			iPart = iRead;
		} else {
			pPart = Replacement;
			iPart = sizeof(Replacement);
			if ( iRead == 0 ) {
				iRead = 1;
			}
		}
		if ( iPart > (pParser->Config.LineLimit -
			pParser->Decoded.Size) ) {
			(void)__xrtHttpSseFail(
				pParser,
				pError,
				XHTTP_SSE_ERROR_LINE_TOO_LARGE,
				__xrtHttpSsePositionAdd(
					pParser->LineOffset, iPosition
				),
				pParser->LineNumber,
				XERR_RANGE
			);
			return false;
		}
		if ( !xrtBufferAppend(
			&pParser->Decoded,
			(xbytesview){ pPart, iPart }
		) ) {
			(void)__xrtHttpSseFail(
				pParser,
				pError,
				XHTTP_SSE_ERROR_ALLOCATION,
				__xrtHttpSsePositionAdd(
					pParser->LineOffset, iPosition
				),
				pParser->LineNumber,
				XERR_MEMORY
			);
			return false;
		}
		iPosition += iRead;
	}
	*pText = (xstrview){
		(cstr)pParser->Decoded.Data,
		pParser->Decoded.Size
	};
	return true;
}



/* 解码当前完整行，并只在第一行移除一个 UTF-8 BOM。 */
static bool __xrtHttpSseDecodeLine(
	xhttpsseparser* pParser,
	xhttpsseerrorinfo* pError,
	xstrview* pText
)
{
	xstrview Source = {
		(cstr)pParser->Line.Data,
		pParser->Line.Size
	};
	size_t iError;

	if ( ((pParser->Flags & XRT_HTTP_SSE_FIRST) != 0) &&
		(Source.Size >= 3u) &&
		((unsigned char)Source.Data[0] == 0xEFu) &&
		((unsigned char)Source.Data[1] == 0xBBu) &&
		((unsigned char)Source.Data[2] == 0xBFu) ) {
		Source.Data += 3;
		Source.Size -= 3u;
	}
	pParser->Flags &= ~XRT_HTTP_SSE_FIRST;
	if ( Source.Size > pParser->Config.LineLimit ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_LINE_TOO_LARGE,
			__xrtHttpSsePositionAdd(
				pParser->LineOffset,
				pParser->Config.LineLimit
			),
			pParser->LineNumber,
			XERR_RANGE
		);
		return false;
	}
	if ( xrtUtf8Valid(Source, &iError) ) {
		*pText = Source;
		return true;
	}
	if ( pParser->Config.Utf8Policy == XUTF_STRICT ) {
		(void)__xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_UTF8,
			__xrtHttpSsePositionAdd(
				pParser->LineOffset, iError
			),
			pParser->LineNumber,
			XERR_PROTOCOL
		);
		return false;
	}
	return __xrtHttpSseReplaceLine(
		pParser, Source, pError, pText
	);
}



/* 严格解析 retry 的非空 ASCII 十进制；无效或溢出按规范忽略。 */
static bool __xrtHttpSseRetryParse(xstrview Value, uint64* pRetry)
{
	uint64 iValue = 0;
	size_t i;

	if ( Value.Size == 0 ) {
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];
		uint64 iDigit;

		if ( (iByte < (unsigned char)'0') ||
			(iByte > (unsigned char)'9') ) {
			return false;
		}
		iDigit = (uint64)(iByte - (unsigned char)'0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pRetry = iValue;
	return true;
}



/* 判断下一个首行字节是否属于唯一允许超出行限额的 UTF-8 BOM。 */
static bool __xrtHttpSseLineByteValid(
	const xhttpsseparser* pParser,
	uint8 iByte
)
{
	static const uint8 Bom[3] = { 0xEFu, 0xBBu, 0xBFu };
	size_t iNext;
	size_t i;

	if ( pParser->Line.Size == SIZE_MAX ) {
		return false;
	}
	iNext = pParser->Line.Size + 1u;
	if ( iNext <= pParser->Config.LineLimit ) {
		return true;
	}
	if ( (pParser->Flags & XRT_HTTP_SSE_FIRST) == 0 ) {
		return false;
	}
	for ( i = 0; (i < pParser->Line.Size) && (i < 3u); i++ ) {
		if ( pParser->Line.Data[i] != Bom[i] ) {
			return false;
		}
	}
	if ( pParser->Line.Size < 3u ) {
		return iByte == Bom[pParser->Line.Size];
	}
	return iNext <= (pParser->Config.LineLimit + 3u);
}



/* 发布一条完整应用事件并保留借用缓冲到下一次 Read。 */
static xhttpsseparsestatus __xrtHttpSseDispatch(
	xhttpsseparser* pParser,
	xhttpsseitem* pItem
)
{
	if ( pParser->Data.Size == 0 ) {
		xrtBufferClear(&pParser->Type);
		return XHTTP_SSE_PARSE_MORE;
	}
	pParser->Data.Size--;
	pItem->Kind = XHTTP_SSE_ITEM_EVENT;
	pItem->Message.Type = pParser->Type.Size == 0 ?
		XRT_STR_LITERAL("message") :
		(xstrview){
			(cstr)pParser->Type.Data,
			pParser->Type.Size
		};
	pItem->Message.Data = (xstrview){
		(cstr)pParser->Data.Data,
		pParser->Data.Size
	};
	pItem->Message.LastEventId = (xstrview){
		(cstr)pParser->Id.Data,
		pParser->Id.Size
	};
	pItem->Message.Retry = pParser->Retry;
	pParser->Flags |= XRT_HTTP_SSE_HOLD_EVENT;
	return XHTTP_SSE_PARSE_ITEM;
}



/* 解释一条完成解码的 SSE 行。 */
static xhttpsseparsestatus __xrtHttpSseLine(
	xhttpsseparser* pParser,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
)
{
	xstrview Line;
	xstrview Name;
	xstrview Value;
	size_t iColon = XRT_NPOS;
	size_t i;

	if ( !__xrtHttpSseDecodeLine(pParser, pError, &Line) ) {
		return XHTTP_SSE_PARSE_ERROR;
	}
	if ( Line.Size == 0 ) {
		return __xrtHttpSseDispatch(pParser, pItem);
	}
	if ( Line.Data[0] == ':' ) {
		Value = (xstrview){ Line.Data + 1, Line.Size - 1u };
		if ( (Value.Size != 0) && (Value.Data[0] == ' ') ) {
			Value.Data++;
			Value.Size--;
		}
		if ( pParser->Config.EmitComments ) {
			pItem->Kind = XHTTP_SSE_ITEM_COMMENT;
			pItem->Comment = Value;
			pParser->Flags |= XRT_HTTP_SSE_HOLD_LINE;
			return XHTTP_SSE_PARSE_ITEM;
		}
		return XHTTP_SSE_PARSE_MORE;
	}

	for ( i = 0; i < Line.Size; i++ ) {
		if ( Line.Data[i] == ':' ) {
			iColon = i;
			break;
		}
	}
	if ( iColon == XRT_NPOS ) {
		Name = Line;
		Value = (xstrview){ NULL, 0 };
	} else {
		Name = (xstrview){ Line.Data, iColon };
		Value = (xstrview){
			Line.Data + iColon + 1u,
			Line.Size - iColon - 1u
		};
		if ( (Value.Size != 0) && (Value.Data[0] == ' ') ) {
			Value.Data++;
			Value.Size--;
		}
	}

	if ( __xrtHttpSseEqual(Name, "data", 4u) ) {
		return __xrtHttpSseDataAppend(
			pParser, Value, pError
		) ? XHTTP_SSE_PARSE_MORE : XHTTP_SSE_PARSE_ERROR;
	}
	if ( __xrtHttpSseEqual(Name, "event", 5u) ) {
		return __xrtHttpSseAssign(
			pParser,
			&pParser->Type,
			Value,
			pParser->Config.TypeLimit,
			XHTTP_SSE_ERROR_TYPE_TOO_LARGE,
			pError
		) ? XHTTP_SSE_PARSE_MORE : XHTTP_SSE_PARSE_ERROR;
	}
	if ( __xrtHttpSseEqual(Name, "id", 2u) ) {
		if ( (Value.Size != 0) &&
			(memchr(Value.Data, 0, Value.Size) != NULL) ) {
			return XHTTP_SSE_PARSE_MORE;
		}
		return __xrtHttpSseAssign(
			pParser,
			&pParser->Id,
			Value,
			pParser->Config.IdLimit,
			XHTTP_SSE_ERROR_ID_TOO_LARGE,
			pError
		) ? XHTTP_SSE_PARSE_MORE : XHTTP_SSE_PARSE_ERROR;
	}
	if ( __xrtHttpSseEqual(Name, "retry", 5u) ) {
		uint64 iRetry;

		if ( !__xrtHttpSseRetryParse(Value, &iRetry) ) {
			return XHTTP_SSE_PARSE_MORE;
		}
		pParser->Retry = iRetry;
		if ( pParser->Config.EmitRetry ) {
			pItem->Kind = XHTTP_SSE_ITEM_RETRY;
			pItem->Retry = iRetry;
			return XHTTP_SSE_PARSE_ITEM;
		}
	}
	return XHTTP_SSE_PARSE_MORE;
}



/* 初始化默认 Parser 配置。 */
XRT_API void xrtHttpSseParserConfigInit(
	xhttpsseparserconfig* pConfig
)
{
	xhttpsseparserconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.LineLimit = XRT_HTTP_SSE_LINE_DEFAULT;
	Config.DataLimit = XRT_HTTP_SSE_DATA_DEFAULT;
	Config.TypeLimit = XRT_HTTP_SSE_TYPE_DEFAULT;
	Config.IdLimit = XRT_HTTP_SSE_ID_DEFAULT;
	Config.Retry = XHTTP_SSE_RETRY_DEFAULT;
	Config.Utf8Policy = XUTF_REPLACE;
	Config.EmitRetry = true;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 判断配置限额和 UTF-8 策略有效。 */
XRT_API bool xrtHttpSseParserConfigValid(
	const xhttpsseparserconfig* pConfig
)
{
	xhttpsseparserconfig Config;

	return __xrtHttpSseParserConfigResolve(pConfig, &Config);
}



/* 初始化初始零分配 Parser。 */
XRT_API bool xrtHttpSseParserInit(
	xhttpsseparser* pParser,
	const xhttpsseparserconfig* pConfig
)
{
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;

	if ( pConfig == NULL ) {
		xrtHttpSseParserConfigInit(&Config);
	} else if ( !__xrtHttpSseParserConfigResolve(
		pConfig, &Config
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpSseParserPointerValid(pParser) ||
		__xrtRangesOverlap(
			pParser, sizeof(Parser), pConfig,
			pConfig == NULL ? 0 : sizeof(Config)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Parser, 0, sizeof(Parser));
	Parser.Config = Config;
	(void)xrtBufferInit(&Parser.Line);
	(void)xrtBufferInit(&Parser.Decoded);
	(void)xrtBufferInit(&Parser.Data);
	(void)xrtBufferInit(&Parser.Type);
	(void)xrtBufferInit(&Parser.Id);
	Parser.Retry = Config.Retry;
	Parser.LineNumber = 1;
	Parser.State = XRT_HTTP_SSE_PARSER_OPEN;
	Parser.Flags = XRT_HTTP_SSE_FIRST;
	memcpy(pParser, &Parser, sizeof(Parser));
	return true;
}



/* 创建初始零分配 Parser。 */
XRT_API xhttpsseparser* xrtHttpSseParserCreate(
	const xhttpsseparserconfig* pConfig
)
{
	xhttpsseparserconfig Config;
	xhttpsseparser* pParser;

	if ( pConfig == NULL ) {
		xrtHttpSseParserConfigInit(&Config);
	} else if ( !__xrtHttpSseParserConfigResolve(
		pConfig, &Config
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pParser = xrtMalloc(sizeof(*pParser));

	if ( pParser == NULL ) {
		return NULL;
	}
	if ( !xrtHttpSseParserInit(pParser, &Config) ) {
		xrtFree(pParser);
		return NULL;
	}
	return pParser;
}



/* 释放 Parser 持有的动态容量。 */
XRT_API void xrtHttpSseParserUnit(xhttpsseparser* pParser)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	xrtBufferUnit(&pParser->Line);
	xrtBufferUnit(&pParser->Decoded);
	xrtBufferUnit(&pParser->Data);
	xrtBufferUnit(&pParser->Type);
	xrtBufferUnit(&pParser->Id);
	memset(pParser, 0, sizeof(*pParser));
}



/* 释放 Parser 和动态容量。 */
XRT_API void xrtHttpSseParserDestroy(xhttpsseparser* pParser)
{
	if ( pParser == NULL ) {
		return;
	}
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	xrtHttpSseParserUnit(pParser);
	xrtFree(pParser);
}



/* 清除新 EventSource 的全部状态。 */
XRT_API void xrtHttpSseParserReset(xhttpsseparser* pParser)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	xrtBufferClear(&pParser->Line);
	xrtBufferClear(&pParser->Decoded);
	xrtBufferClear(&pParser->Data);
	xrtBufferClear(&pParser->Type);
	xrtBufferClear(&pParser->Id);
	pParser->Retry = pParser->Config.Retry;
	pParser->Offset = 0;
	pParser->LineOffset = 0;
	pParser->LineNumber = 1;
	pParser->State = XRT_HTTP_SSE_PARSER_OPEN;
	pParser->Flags = XRT_HTTP_SSE_FIRST;
}



/* 清除新响应状态并保留重连元数据。 */
XRT_API void xrtHttpSseParserReconnect(xhttpsseparser* pParser)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	xrtBufferClear(&pParser->Line);
	xrtBufferClear(&pParser->Decoded);
	xrtBufferClear(&pParser->Data);
	xrtBufferClear(&pParser->Type);
	pParser->Offset = 0;
	pParser->LineOffset = 0;
	pParser->LineNumber = 1;
	pParser->State = XRT_HTTP_SSE_PARSER_OPEN;
	pParser->Flags = XRT_HTTP_SSE_FIRST;
}



/* 裁剪 Parser 的全部动态容量。 */
XRT_API bool xrtHttpSseParserTrim(xhttpsseparser* pParser)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtBufferTrim(&pParser->Line) &&
		xrtBufferTrim(&pParser->Decoded) &&
		xrtBufferTrim(&pParser->Data) &&
		xrtBufferTrim(&pParser->Type) &&
		xrtBufferTrim(&pParser->Id);
}



/* 判断 Read 的输入输出区域没有互相覆盖 Parser 状态。 */
static bool __xrtHttpSseReadValid(
	xhttpsseparser* pParser,
	xbytesview Input,
	size_t* pConsumed,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
)
{
	const void* arrOutput[3];
	size_t arrOutputSize[3];
	const xbuffer* arrBuffer[5];
	size_t i;
	size_t j;

	if ( !__xrtHttpSseParserValid(pParser) ||
		!__xrtRangeValid(Input.Data, Input.Size) ||
		!__xrtRangeValid(pConsumed, sizeof(*pConsumed)) ||
		!__xrtRangeValid(pItem, sizeof(*pItem)) ||
		((pError != NULL) &&
		 !__xrtRangeValid(pError, sizeof(*pError))) ) {
		return false;
	}
	arrOutput[0] = pConsumed;
	arrOutput[1] = pItem;
	arrOutput[2] = pError;
	arrOutputSize[0] = sizeof(*pConsumed);
	arrOutputSize[1] = sizeof(*pItem);
	arrOutputSize[2] = (pError == NULL) ? 0 : sizeof(*pError);
	arrBuffer[0] = &pParser->Line;
	arrBuffer[1] = &pParser->Decoded;
	arrBuffer[2] = &pParser->Data;
	arrBuffer[3] = &pParser->Type;
	arrBuffer[4] = &pParser->Id;
	if ( __xrtRangesOverlap(
		pParser, sizeof(*pParser), Input.Data, Input.Size
	) || __xrtRangesOverlap(
		pParser, sizeof(*pParser), pConsumed, sizeof(*pConsumed)
	) || __xrtRangesOverlap(
		pParser, sizeof(*pParser), pItem, sizeof(*pItem)
	) || ((pError != NULL) && __xrtRangesOverlap(
		pParser, sizeof(*pParser), pError, sizeof(*pError)
	)) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pConsumed, sizeof(*pConsumed), pItem, sizeof(*pItem)
	) || ((pError != NULL) && (
		 __xrtRangesOverlap(
			pConsumed, sizeof(*pConsumed), pError, sizeof(*pError)
		 ) || __xrtRangesOverlap(
			pItem, sizeof(*pItem), pError, sizeof(*pError)
		 )
	)) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		Input.Data, Input.Size,
		pParser->Line.Data, pParser->Line.Capacity
	) || __xrtRangesOverlap(
		Input.Data, Input.Size,
		pParser->Decoded.Data, pParser->Decoded.Capacity
	) || __xrtRangesOverlap(
		Input.Data, Input.Size,
		pParser->Data.Data, pParser->Data.Capacity
	) || __xrtRangesOverlap(
		Input.Data, Input.Size,
		pParser->Type.Data, pParser->Type.Capacity
	) || __xrtRangesOverlap(
		Input.Data, Input.Size,
		pParser->Id.Data, pParser->Id.Capacity
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		Input.Data, Input.Size, pConsumed, sizeof(*pConsumed)
	) || __xrtRangesOverlap(
		Input.Data, Input.Size, pItem, sizeof(*pItem)
	) || ((pError != NULL) && __xrtRangesOverlap(
		Input.Data, Input.Size, pError, sizeof(*pError)
	)) ) {
		return false;
	}
	/* 输出对象不能位于 Read 期间可能重分配的 Parser 缓冲。 */
	for ( i = 0; i < 3u; i++ ) {
		for ( j = 0; j < 5u; j++ ) {
			if ( __xrtRangesOverlap(
				arrOutput[i],
				arrOutputSize[i],
				arrBuffer[j]->Data,
				arrBuffer[j]->Capacity
			) ) {
				return false;
			}
		}
	}
	return true;
}



/* 使用自然对齐的局部输出增量读取任意输入分块。 */
static xhttpsseparsestatus __xrtHttpSseParserRead(
	xhttpsseparser* pParser,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
)
{
	size_t iPosition = 0;

	*pConsumed = 0;
	memset(pItem, 0, sizeof(*pItem));
	if ( pError != NULL ) {
		memset(pError, 0, sizeof(*pError));
	}
	if ( pParser->State == XRT_HTTP_SSE_PARSER_FAILED ) {
		return __xrtHttpSseFail(
			pParser,
			pError,
			XHTTP_SSE_ERROR_STATE,
			pParser->Offset,
			pParser->LineNumber,
			XERR_STATE
		);
	}
	if ( pParser->State == XRT_HTTP_SSE_PARSER_DONE ) {
		return XHTTP_SSE_PARSE_DONE;
	}

	if ( (pParser->Flags & XRT_HTTP_SSE_HOLD_LINE) != 0 ) {
		xrtBufferClear(&pParser->Line);
		xrtBufferClear(&pParser->Decoded);
		pParser->Flags &= ~XRT_HTTP_SSE_HOLD_LINE;
	}
	if ( (pParser->Flags & XRT_HTTP_SSE_HOLD_EVENT) != 0 ) {
		xrtBufferClear(&pParser->Data);
		xrtBufferClear(&pParser->Type);
		pParser->Flags &= ~XRT_HTTP_SSE_HOLD_EVENT;
	}

	while ( iPosition < Input.Size ) {
		uint8 iByte;

		if ( (pParser->Flags & XRT_HTTP_SSE_SKIP_LF) != 0 ) {
			pParser->Flags &= ~XRT_HTTP_SSE_SKIP_LF;
			if ( Input.Data[iPosition] == (uint8)'\n' ) {
				iPosition++;
				pParser->Offset = __xrtHttpSsePositionAdd(
					pParser->Offset, 1u
				);
				pParser->LineOffset = pParser->Offset;
				continue;
			}
		}
		iByte = Input.Data[iPosition];
		if ( (iByte != (uint8)'\r') &&
			(iByte != (uint8)'\n') ) {
			if ( !__xrtHttpSseLineByteValid(pParser, iByte) ) {
				*pConsumed = iPosition;
				return __xrtHttpSseFail(
					pParser,
					pError,
					XHTTP_SSE_ERROR_LINE_TOO_LARGE,
					pParser->Offset,
					pParser->LineNumber,
					XERR_RANGE
				);
			}
			if ( !xrtBufferAppendByte(&pParser->Line, iByte) ) {
				*pConsumed = iPosition;
				return __xrtHttpSseFail(
					pParser,
					pError,
					XHTTP_SSE_ERROR_ALLOCATION,
					pParser->Offset,
					pParser->LineNumber,
					XERR_MEMORY
				);
			}
			iPosition++;
			pParser->Offset = __xrtHttpSsePositionAdd(
				pParser->Offset, 1u
			);
			continue;
		}

		iPosition++;
		pParser->Offset = __xrtHttpSsePositionAdd(
			pParser->Offset, 1u
		);
		if ( iByte == (uint8)'\r' ) {
			pParser->Flags |= XRT_HTTP_SSE_SKIP_LF;
		}
		{
			xhttpsseparsestatus Status = __xrtHttpSseLine(
				pParser, pItem, pError
			);

			pParser->LineNumber = __xrtHttpSsePositionAdd(
				pParser->LineNumber, 1u
			);
			pParser->LineOffset = pParser->Offset;
			*pConsumed = iPosition;
			if ( Status == XHTTP_SSE_PARSE_ERROR ) {
				return Status;
			}
			if ( Status == XHTTP_SSE_PARSE_ITEM ) {
				if ( (pParser->Flags & XRT_HTTP_SSE_HOLD_LINE) == 0 ) {
					xrtBufferClear(&pParser->Line);
					xrtBufferClear(&pParser->Decoded);
				}
				return Status;
			}
		}
		xrtBufferClear(&pParser->Line);
		xrtBufferClear(&pParser->Decoded);
	}

	*pConsumed = iPosition;
	if ( bEnd ) {
		xrtBufferClear(&pParser->Line);
		xrtBufferClear(&pParser->Decoded);
		xrtBufferClear(&pParser->Data);
		xrtBufferClear(&pParser->Type);
		pParser->Flags &= ~(
			XRT_HTTP_SSE_SKIP_LF |
			XRT_HTTP_SSE_HOLD_LINE |
			XRT_HTTP_SSE_HOLD_EVENT
		);
		pParser->State = XRT_HTTP_SSE_PARSER_DONE;
		return XHTTP_SSE_PARSE_DONE;
	}
	return XHTTP_SSE_PARSE_MORE;
}



/* 增量读取任意输入分块并最多发布一个项目。 */
XRT_API xhttpsseparsestatus xrtHttpSseParserRead(
	xhttpsseparser* pParser,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
)
{
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	xhttpsseparsestatus Status;
	size_t iConsumed;

	if ( !__xrtHttpSseReadValid(
		pParser, Input, pConsumed, pItem, pError
	) ) {
		return __xrtHttpSseFail(
			NULL,
			NULL,
			XHTTP_SSE_ERROR_ARGUMENT,
			0,
			0,
			XERR_ARGUMENT
		);
	}
	Status = __xrtHttpSseParserRead(
		pParser,
		Input,
		bEnd,
		&iConsumed,
		&Item,
		pError != NULL ? &Error : NULL
	);
	memcpy(pConsumed, &iConsumed, sizeof(iConsumed));
	memcpy(pItem, &Item, sizeof(Item));
	if ( pError != NULL ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	return Status;
}



/* 返回当前持久 Last-Event-ID。 */
XRT_API xstrview xrtHttpSseParserLastEventId(
	const xhttpsseparser* pParser
)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		(cstr)pParser->Id.Data,
		pParser->Id.Size
	};
}



/* 返回当前重连毫秒数。 */
XRT_API uint64 xrtHttpSseParserRetry(
	const xhttpsseparser* pParser
)
{
	if ( !__xrtHttpSseParserValid(pParser) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pParser->Retry;
}

#endif
