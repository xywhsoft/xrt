#include <stdlib.h>
#include <string.h>

#include <xrt/http_sse.h>



#define XRT_HTTP_SSE_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_HTTP_SSE_FUZZ_DATA_MAX ((size_t)256u)



/* 摘要只保留与输入分块无关的可观察解析结果。 */
typedef struct xrt_http_sse_fuzz_summary {
	uint64 Hash;
	size_t Items;
	size_t Events;
	size_t Consumed;
	size_t ErrorOffset;
	size_t ErrorLine;
	uint64 Retry;
	xhttpsseparsestatus Status;
	xhttpsseerror Error;
} xrt_http_sse_fuzz_summary;



/* 把一个字节加入稳定的 FNV-1a 摘要。 */
static void __xrtHttpSseFuzzHashByte(uint64* pHash, uint8 iByte)
{
	*pHash ^= (uint64)iByte;
	*pHash *= UINT64_C(1099511628211);
}



/* 把字节视图及其长度边界加入稳定摘要。 */
static void __xrtHttpSseFuzzHashView(
	uint64* pHash,
	xstrview Text
)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		__xrtHttpSseFuzzHashByte(
			pHash, (uint8)(unsigned char)Text.Data[i]
		);
	}
	__xrtHttpSseFuzzHashByte(pHash, UINT8_C(0xFF));
	for ( i = 0; i < sizeof(Text.Size); i++ ) {
		__xrtHttpSseFuzzHashByte(
			pHash,
			(uint8)(Text.Size >> (i * 8u))
		);
	}
}



/* 把一个 Parser 项目的全部公开语义加入摘要。 */
static void __xrtHttpSseFuzzHashItem(
	xrt_http_sse_fuzz_summary* pSummary,
	const xhttpsseitem* pItem
)
{
	size_t i;

	__xrtHttpSseFuzzHashByte(
		&pSummary->Hash, (uint8)pItem->Kind
	);
	if ( pItem->Kind == XHTTP_SSE_ITEM_EVENT ) {
		pSummary->Events++;
		__xrtHttpSseFuzzHashView(
			&pSummary->Hash, pItem->Message.Type
		);
		__xrtHttpSseFuzzHashView(
			&pSummary->Hash, pItem->Message.Data
		);
		__xrtHttpSseFuzzHashView(
			&pSummary->Hash, pItem->Message.LastEventId
		);
		for ( i = 0; i < sizeof(pItem->Message.Retry); i++ ) {
			__xrtHttpSseFuzzHashByte(
				&pSummary->Hash,
				(uint8)(pItem->Message.Retry >> (i * 8u))
			);
		}
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_COMMENT ) {
		__xrtHttpSseFuzzHashView(
			&pSummary->Hash, pItem->Comment
		);
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_RETRY ) {
		for ( i = 0; i < sizeof(pItem->Retry); i++ ) {
			__xrtHttpSseFuzzHashByte(
				&pSummary->Hash,
				(uint8)(pItem->Retry >> (i * 8u))
			);
		}
	} else {
		abort();
	}
}



/* 使用固定限额按指定最大分块消费任意事件流。 */
static xrt_http_sse_fuzz_summary __xrtHttpSseFuzzParse(
	const uint8* pData,
	size_t iSize,
	size_t iChunk
)
{
	xrt_http_sse_fuzz_summary Summary;
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	size_t iSteps = 0;

	memset(&Summary, 0, sizeof(Summary));
	Summary.Hash = UINT64_C(1469598103934665603);
	xrtHttpSseParserConfigInit(&Config);
	Config.LineLimit = 128u;
	Config.DataLimit = 512u;
	Config.TypeLimit = 64u;
	Config.IdLimit = 64u;
	Config.EmitComments = true;
	Config.EmitRetry = true;
	if ( !xrtHttpSseParserInit(&Parser, &Config) ) {
		abort();
	}
	for ( ;; ) {
		xhttpsseitem Item;
		xhttpsseerrorinfo Error;
		xhttpsseparsestatus Status;
		const uint8* pChunk;
		size_t iRemaining = iSize - Summary.Consumed;
		size_t iAvailable = iRemaining < iChunk ?
			iRemaining : iChunk;
		size_t iConsumed;

		pChunk = iAvailable == 0 ? NULL :
			pData + Summary.Consumed;
		Status = xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ pChunk, iAvailable },
			(Summary.Consumed + iAvailable) == iSize,
			&iConsumed,
			&Item,
			&Error
		);
		if ( iConsumed > iAvailable ) {
			abort();
		}
		Summary.Consumed += iConsumed;
		Summary.Status = Status;
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			Summary.Items++;
			__xrtHttpSseFuzzHashItem(&Summary, &Item);
		} else if ( Status == XHTTP_SSE_PARSE_ERROR ) {
			Summary.Error = Error.Code;
			Summary.ErrorOffset = Error.Offset;
			Summary.ErrorLine = Error.Line;
			break;
		} else if ( Status == XHTTP_SSE_PARSE_DONE ) {
			break;
		} else if ( (iAvailable != 0) && (iConsumed == 0) ) {
			abort();
		}
		iSteps++;
		if ( iSteps > ((iSize * 3u) + 32u) ) {
			abort();
		}
	}
	Summary.Retry = xrtHttpSseParserRetry(&Parser);
	__xrtHttpSseFuzzHashView(
		&Summary.Hash, xrtHttpSseParserLastEventId(&Parser)
	);
	xrtHttpSseParserUnit(&Parser);
	xrtClearError();
	return Summary;
}



/* 要求同一字节流不受网络输入分块方式影响。 */
static void __xrtHttpSseFuzzChunks(
	const uint8* pData,
	size_t iSize
)
{
	xrt_http_sse_fuzz_summary Whole;
	xrt_http_sse_fuzz_summary Single;
	xrt_http_sse_fuzz_summary Variable;
	size_t iVariable = iSize == 0 ? 1u :
		1u + ((size_t)pData[0] % 31u);

	Whole = __xrtHttpSseFuzzParse(
		pData, iSize, iSize == 0 ? 1u : iSize
	);
	Single = __xrtHttpSseFuzzParse(pData, iSize, 1u);
	Variable = __xrtHttpSseFuzzParse(
		pData, iSize, iVariable
	);
	if ( (memcmp(&Whole, &Single, sizeof(Whole)) != 0) ||
		(memcmp(&Whole, &Variable, sizeof(Whole)) != 0) ) {
		abort();
	}
}



/* 从任意输入生成合法事件，并验证 Writer 与 Parser 精确往返。 */
static void __xrtHttpSseFuzzRoundTrip(
	const uint8* pData,
	size_t iSize
)
{
	static const char Alphabet[] =
		"abcdefghijklmnopqrstuvwxyz0123456789 /_-.";
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	xhttpsseevent Event;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	uint8 Data[XRT_HTTP_SSE_FUZZ_DATA_MAX];
	uint8 Encoded[4096];
	size_t iData = iSize < sizeof(Data) ? iSize : sizeof(Data);
	size_t iEncoded;
	size_t iOffset = 0;
	size_t iChunk = iSize == 0 ? 1u :
		1u + ((size_t)pData[0] % 23u);
	bool bEvent = false;
	size_t i;

	for ( i = 0; i < iData; i++ ) {
		Data[i] = (pData[i] & UINT8_C(0x0F)) == 0 ?
			(uint8)'\n' : (uint8)Alphabet[
			pData[i] % (sizeof(Alphabet) - 1u)
		];
	}
	memset(&Event, 0, sizeof(Event));
	Event.Type = XRT_STR_LITERAL("update");
	Event.Data = (xstrview){ (cstr)Data, iData };
	Event.Id = XRT_STR_LITERAL("roundtrip-id");
	Event.Retry = UINT64_C(4321);
	Event.Flags = XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_RETRY;
	if ( !xrtHttpSseEventWrite(
		&Event, Encoded, sizeof(Encoded), &iEncoded
	) ) {
		abort();
	}
	xrtHttpSseParserConfigInit(&Config);
	Config.LineLimit = XRT_HTTP_SSE_FUZZ_DATA_MAX;
	Config.DataLimit = XRT_HTTP_SSE_FUZZ_DATA_MAX;
	Config.EmitRetry = false;
	if ( !xrtHttpSseParserInit(&Parser, &Config) ) {
		abort();
	}
	while ( !bEvent ) {
		xhttpsseparsestatus Status;
		size_t iRemaining = iEncoded - iOffset;
		size_t iAvailable = iRemaining < iChunk ?
			iRemaining : iChunk;
		size_t iConsumed;

		Status = xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ Encoded + iOffset, iAvailable },
			(iOffset + iAvailable) == iEncoded,
			&iConsumed,
			&Item,
			&Error
		);
		iOffset += iConsumed;
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			if ( (Item.Kind != XHTTP_SSE_ITEM_EVENT) ||
				(Item.Message.Type.Size != Event.Type.Size) ||
				(memcmp(
					Item.Message.Type.Data,
					Event.Type.Data,
					Event.Type.Size
				 ) != 0) ||
				(Item.Message.Data.Size != Event.Data.Size) ||
				((Event.Data.Size != 0) && (memcmp(
					Item.Message.Data.Data,
					Event.Data.Data,
					Event.Data.Size
				 ) != 0)) ||
				(Item.Message.LastEventId.Size != Event.Id.Size) ||
				(memcmp(
					Item.Message.LastEventId.Data,
					Event.Id.Data,
					Event.Id.Size
				 ) != 0) ||
				(Item.Message.Retry != Event.Retry) ) {
				abort();
			}
			bEvent = true;
		} else if ( Status != XHTTP_SSE_PARSE_MORE ) {
			abort();
		}
	}
	xrtHttpSseParserUnit(&Parser);
	xrtClearError();
}



/* 统一公开给确定性回归和 Clang/libFuzzer 的 SSE 协议入口。 */
int xrtHttpSseFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_HTTP_SSE_FUZZ_INPUT_MAX) ) {
		return 0;
	}
	__xrtHttpSseFuzzChunks(pData, iSize);
	__xrtHttpSseFuzzRoundTrip(pData, iSize);
	return 0;
}



#if defined(XRT_HTTP_SSE_FUZZ_LIBFUZZER)

/* 把独立 SSE 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtHttpSseFuzzerTestOneInput(pData, iSize);
}

#endif
