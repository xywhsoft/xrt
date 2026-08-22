#include "../test.h"

#include <xrt/http_sse.h>



typedef struct test_httpsse_capture {
	size_t Events;
	size_t Comments;
	size_t Retries;
} test_httpsse_capture;



/* 立即验证借用项目，避免把视图错误地保存到下一次 Read 之后。 */
static void testHttpSseCapture(
	test_httpsse_capture* pCapture,
	const xhttpsseitem* pItem
)
{
	if ( pItem->Kind == XHTTP_SSE_ITEM_COMMENT ) {
		pCapture->Comments++;
		testRequire(
			(pItem->Comment.Size == 4u) &&
			(memcmp(pItem->Comment.Data, "keep", 4u) == 0),
			"SSE comment item mismatch"
		);
		return;
	}
	if ( pItem->Kind == XHTTP_SSE_ITEM_RETRY ) {
		pCapture->Retries++;
		testRequire(
			pItem->Retry == 1500,
			"SSE retry item mismatch"
		);
		return;
	}
	testRequire(
		pItem->Kind == XHTTP_SSE_ITEM_EVENT,
		"SSE parser returned an unknown item"
	);
	if ( pCapture->Events == 0 ) {
		testRequire(
			(pItem->Message.Type.Size == 3u) &&
			(memcmp(pItem->Message.Type.Data, "add", 3u) == 0) &&
			(pItem->Message.Data.Size == 12u) &&
			(memcmp(
				pItem->Message.Data.Data,
				"first\nsecond",
				12u
			) == 0) &&
			(pItem->Message.LastEventId.Size == 1u) &&
			(pItem->Message.LastEventId.Data[0] == '7') &&
			(pItem->Message.Retry == 1500),
			"SSE first application event mismatch"
		);
	} else {
		testRequire(
			(pItem->Message.Type.Size == 7u) &&
			(memcmp(pItem->Message.Type.Data, "message", 7u) == 0) &&
			(pItem->Message.Data.Size == 0) &&
			(pItem->Message.LastEventId.Size == 0),
			"SSE empty application event mismatch"
		);
	}
	pCapture->Events++;
}



/* 逐字节驱动混合换行与 BOM，验证任意网络分块边界。 */
static void testHttpSseParserChunks(void)
{
	static const uint8 Stream[] = {
		0xEFu, 0xBBu, 0xBFu,
		':', ' ', 'k', 'e', 'e', 'p', '\r', '\n',
		'r', 'e', 't', 'r', 'y', ':', ' ', '1', '5', '0', '0', '\r',
		'i', 'd', ':', ' ', '7', '\n',
		'e', 'v', 'e', 'n', 't', ':', ' ', 'a', 'd', 'd', '\r', '\n',
		'd', 'a', 't', 'a', ':', ' ', 'f', 'i', 'r', 's', 't', '\r',
		'd', 'a', 't', 'a', ':', ' ', 's', 'e', 'c', 'o', 'n', 'd', '\n',
		'\r', '\n',
		'i', 'd', '\n',
		'd', 'a', 't', 'a', ':', '\n', '\n',
		'd', 'a', 't', 'a', ':', ' ', 'u', 'n', 'f', 'i', 'n', 'i', 's', 'h', 'e', 'd'
	};
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	test_httpsse_capture Capture;
	size_t i;

	xrtHttpSseParserConfigInit(&Config);
	Config.EmitComments = true;
	memset(&Capture, 0, sizeof(Capture));
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(Parser.Line.Capacity == 0) &&
		(Parser.Data.Capacity == 0) &&
		(Parser.Id.Capacity == 0),
		"SSE Parser Init allocated fixed buffers"
	);
	for ( i = 0; i < sizeof(Stream); i++ ) {
		xhttpsseitem Item;
		xhttpsseerrorinfo Error;
		xhttpsseparsestatus Status;
		size_t iConsumed = 0;

		Status = xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ &Stream[i], 1u },
			false,
			&iConsumed,
			&Item,
			&Error
		);
		testRequire(
			(Status != XHTTP_SSE_PARSE_ERROR) &&
			(iConsumed == 1u),
			"SSE one-byte parser drive failed"
		);
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			testHttpSseCapture(&Capture, &Item);
		}
	}
	{
		xhttpsseitem Item;
		xhttpsseerrorinfo Error;
		size_t iConsumed = 9;

		testRequire(
			xrtHttpSseParserRead(
				&Parser,
				(xbytesview){ NULL, 0 },
				true,
				&iConsumed,
				&Item,
				&Error
			) == XHTTP_SSE_PARSE_DONE &&
			(iConsumed == 0),
			"SSE EOF did not discard unfinished event"
		);
	}
	testRequire(
		(Capture.Events == 2u) &&
		(Capture.Comments == 1u) &&
		(Capture.Retries == 1u) &&
		(xrtHttpSseParserLastEventId(&Parser).Size == 0) &&
		(xrtHttpSseParserRetry(&Parser) == 1500),
		"SSE parser state summary mismatch"
	);
	xrtHttpSseParserReconnect(&Parser);
	testRequire(
		(xrtHttpSseParserRetry(&Parser) == 1500) &&
		(xrtHttpSseParserLastEventId(&Parser).Size == 0),
		"SSE reconnect did not preserve metadata"
	);
	xrtHttpSseParserReset(&Parser);
	testRequire(
		(xrtHttpSseParserRetry(&Parser) == XHTTP_SSE_RETRY_DEFAULT) &&
		xrtHttpSseParserTrim(&Parser) &&
		(Parser.Line.Capacity == 0) &&
		(Parser.Data.Capacity == 0) &&
		(Parser.Id.Capacity == 0),
		"SSE reset or Trim mismatch"
	);
	xrtHttpSseParserUnit(&Parser);
}



/* 验证替换解码遵循 UTF-8 最大子部件，严格模式保留精确位置。 */
static void testHttpSseParserUtf8(void)
{
	static const uint8 Invalid[] = {
		'd', 'a', 't', 'a', ':', ' ',
		0xF0u, 0x28u, 0x8Cu, 0x28u,
		'\n', '\n'
	};
	static const uint8 Replaced[] = {
		0xEFu, 0xBFu, 0xBDu, 0x28u,
		0xEFu, 0xBFu, 0xBDu, 0x28u
	};
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	size_t iConsumed;

	xrtHttpSseParserConfigInit(&Config);
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ Invalid, sizeof(Invalid) },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ITEM) &&
		(Item.Kind == XHTTP_SSE_ITEM_EVENT) &&
		(Item.Message.Data.Size == sizeof(Replaced)) &&
		(memcmp(
			Item.Message.Data.Data,
			Replaced,
			sizeof(Replaced)
		 ) == 0),
		"SSE replacement UTF-8 decode mismatch"
	);
	xrtHttpSseParserUnit(&Parser);
	Config.Utf8Policy = XUTF_STRICT;
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ Invalid, sizeof(Invalid) },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ERROR) &&
		(Error.Code == XHTTP_SSE_ERROR_UTF8) &&
		(Error.Offset == 6u) &&
		(Error.Line == 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"SSE strict UTF-8 error mismatch"
	);
	xrtClearError();
	xrtHttpSseParserReset(&Parser);
	xrtHttpSseParserUnit(&Parser);
}



/* 验证事件与行硬限额形成终态错误并可显式 Reset。 */
static void testHttpSseParserLimits(void)
{
	static const uint8 TooMuch[] = "data: four\n";
	static const uint8 TooLong[] = "abcde";
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	size_t iConsumed;

	xrtHttpSseParserConfigInit(&Config);
	Config.DataLimit = 3u;
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ TooMuch, sizeof(TooMuch) - 1u },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ERROR) &&
		(Error.Code == XHTTP_SSE_ERROR_DATA_TOO_LARGE) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SSE data limit mismatch"
	);
	xrtClearError();
	xrtHttpSseParserUnit(&Parser);
	Config.LineLimit = 4u;
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ TooLong, sizeof(TooLong) - 1u },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ERROR) &&
		(Error.Code == XHTTP_SSE_ERROR_LINE_TOO_LARGE) &&
		(Error.Offset == 4u) &&
		(iConsumed == 4u),
		"SSE line limit mismatch"
	);
	xrtClearError();
	xrtHttpSseParserReset(&Parser);
	xrtHttpSseParserUnit(&Parser);
}



/* 验证 BOM 只豁免自身三字节，普通首行仍严格遵守解码后限额。 */
static void testHttpSseParserBomLimit(void)
{
	static const uint8 BomEvent[] = {
		0xEFu, 0xBBu, 0xBFu,
		'd', 'a', 't', 'a', '\n', '\n'
	};
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	size_t iConsumed;

	xrtHttpSseParserConfigInit(&Config);
	Config.LineLimit = 4u;
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ BomEvent, sizeof(BomEvent) },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ITEM) &&
		(iConsumed == sizeof(BomEvent)) &&
		(Item.Kind == XHTTP_SSE_ITEM_EVENT) &&
		(Item.Message.Data.Size == 0),
		"SSE BOM was not excluded from the decoded line limit"
	);
	xrtHttpSseParserUnit(&Parser);
}



/* 验证非法输出别名在任何输出或动态缓冲修改前失败。 */
static void testHttpSseParserOverlap(void)
{
	static const uint8 Event[] = "data: retained\n\n";
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	xhttpsseerrorinfo BeforeError;
	uint8 Before[16];
	size_t iConsumed;
	size_t iBefore;

	testRequire(
		xrtHttpSseParserInit(&Parser, NULL) &&
		(xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ Event, sizeof(Event) - 1u },
			false,
			&iConsumed,
			&Item,
			&Error
		 ) == XHTTP_SSE_PARSE_ITEM) &&
		(Parser.Data.Size == 8u),
		"SSE overlap test setup failed"
	);
	iBefore = Parser.Data.Size;
	memcpy(Before, Parser.Data.Data, iBefore);
	iConsumed = 97u;
	memset(&Error, 0x5A, sizeof(Error));
	BeforeError = Error;
	testRequire(
		xrtHttpSseParserRead(
			&Parser,
			(xbytesview){ NULL, 0 },
			false,
			&iConsumed,
			(xhttpsseitem*)Parser.Data.Data,
			&Error
		) == XHTTP_SSE_PARSE_ERROR &&
		(iConsumed == 97u) &&
		(memcmp(&Error, &BeforeError, sizeof(Error)) == 0) &&
		(Parser.Data.Size == iBefore) &&
		(memcmp(Parser.Data.Data, Before, iBefore) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE Parser output overlap was not failure atomic"
	);
	xrtClearError();
	xrtHttpSseParserUnit(&Parser);
}



/* 验证配置与 Read 输出支持未对齐存储，Parser 本体拒绝未对齐地址。 */
static void testHttpSseParserMemoryContracts(void)
{
	static const uint8 Stream[] = "data: ok\n\n";
	uint8 ConfigStorage[sizeof(xhttpsseparserconfig) + 2u];
	uint8 ParserStorage[sizeof(xhttpsseparser) + 2u];
	uint8 ConsumedStorage[sizeof(size_t) + 2u];
	uint8 ItemStorage[sizeof(xhttpsseitem) + 2u];
	uint8 ErrorStorage[sizeof(xhttpsseerrorinfo) + 2u];
	xhttpsseparser Parser;
	xhttpsseitem Item;
	xhttpsseerrorinfo Error;
	size_t iConsumed;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(ParserStorage, 0xA5, sizeof(ParserStorage));
	memset(ConsumedStorage, 0xA5, sizeof(ConsumedStorage));
	memset(ItemStorage, 0xA5, sizeof(ItemStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	xrtHttpSseParserConfigInit(
		(xhttpsseparserconfig*)(void*)(ConfigStorage + 1u)
	);
	testRequire(xrtHttpSseParserConfigValid(
		(const xhttpsseparserconfig*)(const void*)(ConfigStorage + 1u)
	) && xrtHttpSseParserInit(
		&Parser,
		(const xhttpsseparserconfig*)(const void*)(ConfigStorage + 1u)
	), "SSE Parser rejected an unaligned config");
	testRequire(xrtHttpSseParserRead(
		&Parser,
		(xbytesview){ Stream, sizeof(Stream) - 1u },
		false,
		(size_t*)(void*)(ConsumedStorage + 1u),
		(xhttpsseitem*)(void*)(ItemStorage + 1u),
		(xhttpsseerrorinfo*)(void*)(ErrorStorage + 1u)
	) == XHTTP_SSE_PARSE_ITEM,
		"SSE Parser rejected unaligned Read outputs");
	memcpy(&iConsumed, ConsumedStorage + 1u, sizeof(iConsumed));
	memcpy(&Item, ItemStorage + 1u, sizeof(Item));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire((iConsumed == (sizeof(Stream) - 1u)) &&
		(Item.Kind == XHTTP_SSE_ITEM_EVENT) &&
		(Item.Message.Data.Size == 2u) &&
		(memcmp(Item.Message.Data.Data, "ok", 2u) == 0) &&
		(Error.Code == 0),
		"SSE Parser published invalid unaligned outputs"
	);
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(ConsumedStorage[0] == 0xA5) &&
		(ConsumedStorage[sizeof(ConsumedStorage) - 1u] == 0xA5) &&
		(ItemStorage[0] == 0xA5) &&
		(ItemStorage[sizeof(ItemStorage) - 1u] == 0xA5) &&
		(ErrorStorage[0] == 0xA5) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] == 0xA5),
		"SSE Parser wrote outside unaligned output storage"
	);
	xrtHttpSseParserUnit(&Parser);

	testRequire(!xrtHttpSseParserInit(
		(xhttpsseparser*)(void*)(ParserStorage + 1u),
		NULL
	), "SSE Parser accepted an unaligned state object");
	xrtClearError();
	xrtHttpSseParserConfigInit(
		(xhttpsseparserconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"SSE Parser config init accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpSseParserConfigValid(
		(const xhttpsseparserconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "SSE Parser config validation accepted a wrapping input");
	testRequire(xrtHttpSseParserInit(&Parser, NULL),
		"SSE Parser wrap test setup failed");
	iConsumed = 91u;
	memset(&Item, 0x5A, sizeof(Item));
	testRequire(xrtHttpSseParserRead(
		&Parser,
		(xbytesview){ Stream, sizeof(Stream) - 1u },
		false,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Item,
		&Error
	) == XHTTP_SSE_PARSE_ERROR,
		"SSE Parser accepted a wrapping consumed output");
	xrtClearError();
	testRequire(xrtHttpSseParserRead(
		&Parser,
		(xbytesview){
			(const uint8*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u
		},
		false,
		&iConsumed,
		&Item,
		&Error
	) == XHTTP_SSE_PARSE_ERROR,
		"SSE Parser accepted a wrapping input");
	xrtClearError();
	Parser.Offset = SIZE_MAX - 1u;
	Parser.LineOffset = SIZE_MAX - 1u;
	Parser.LineNumber = SIZE_MAX;
	testRequire(xrtHttpSseParserRead(
		&Parser,
		(xbytesview){ (const uint8*)"\n", 1u },
		false,
		&iConsumed,
		&Item,
		&Error
	) == XHTTP_SSE_PARSE_MORE &&
		(Parser.Offset == SIZE_MAX) &&
		(Parser.LineOffset == SIZE_MAX) &&
		(Parser.LineNumber == SIZE_MAX),
		"SSE Parser position counters wrapped"
	);
	xrtHttpSseParserUnit(&Parser);
}



/* 运行 SSE 增量 Parser 回归。 */
int main(void)
{
	testHttpSseParserChunks();
	testHttpSseParserUtf8();
	testHttpSseParserLimits();
	testHttpSseParserBomLimit();
	testHttpSseParserOverlap();
	testHttpSseParserMemoryContracts();
	printf("[PASS] HTTP SSE parser\n");
	return 0;
}
