#include "../test.h"

#include <xrt/http_sse.h>



/* 比较借用文本与指定字节序列。 */
static bool testHttpSseOomTextEqual(
	xstrview Text,
	const void* pExpected,
	size_t iSize
)
{
	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, pExpected, iSize) == 0));
}



/* 完整消费测试流，并核对注释、retry 和最终应用事件。 */
static bool testHttpSseOomParse(
	xhttpsseparser* pParser,
	xbytesview Input,
	xhttpsseerrorinfo* pFailure
)
{
	static const uint8 ExpectedData[] = {
		'f', 'i', 'r', 's', 't', '\n', 0xEFu, 0xBFu, 0xBDu
	};
	size_t iOffset = 0;
	size_t iComments = 0;
	size_t iRetries = 0;
	size_t iEvents = 0;

	for ( ;; ) {
		xhttpsseitem Item;
		xhttpsseerrorinfo Error;
		xhttpsseparsestatus Status;
		size_t iConsumed;

		Status = xrtHttpSseParserRead(
			pParser,
			(xbytesview){ Input.Data + iOffset, Input.Size - iOffset },
			true,
			&iConsumed,
			&Item,
			&Error
		);
		iOffset += iConsumed;
		if ( Status == XHTTP_SSE_PARSE_ERROR ) {
			*pFailure = Error;
			return false;
		}
		if ( Status == XHTTP_SSE_PARSE_DONE ) {
			return (iOffset == Input.Size) &&
				(iComments == 1u) &&
				(iRetries == 1u) &&
				(iEvents == 1u);
		}
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			if ( Item.Kind == XHTTP_SSE_ITEM_COMMENT ) {
				iComments++;
				testRequire(
					testHttpSseOomTextEqual(
						Item.Comment, "ping", 4u
					),
					"SSE OOM comment mismatch"
				);
			} else if ( Item.Kind == XHTTP_SSE_ITEM_RETRY ) {
				iRetries++;
				testRequire(
					Item.Retry == UINT64_C(1250),
					"SSE OOM retry mismatch"
				);
			} else if ( Item.Kind == XHTTP_SSE_ITEM_EVENT ) {
				iEvents++;
				testRequire(
					testHttpSseOomTextEqual(
						Item.Message.Type, "update", 6u
					) && testHttpSseOomTextEqual(
						Item.Message.Data,
						ExpectedData,
						sizeof(ExpectedData)
					) && testHttpSseOomTextEqual(
						Item.Message.LastEventId,
						"0123456789abcdef",
						16u
					),
					"SSE OOM event mismatch"
				);
			}
		}
	}
}



/* 在指定逻辑故障序号下执行一次完整增量解析。 */
static bool testHttpSseOomAttempt(
	xbytesview Input,
	size_t iFail
)
{
	xhttpsseparserconfig Config;
	xhttpsseparser Parser;
	xhttpsseerrorinfo Failure;
	bool bComplete;
	bool bTriggered;

	xrtHttpSseParserConfigInit(&Config);
	Config.EmitComments = true;
	testRequire(
		xrtHttpSseParserInit(&Parser, &Config),
		"SSE OOM parser init failed"
	);
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"SSE OOM fault setup failed"
	);
	bComplete = testHttpSseOomParse(&Parser, Input, &Failure);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( bComplete ) {
		testRequire(
			!bTriggered,
			"SSE parser ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered &&
			(Failure.Code == XHTTP_SSE_ERROR_ALLOCATION) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"SSE parser failed outside the injected allocation point"
		);
		xrtClearError();
		xrtHttpSseParserReset(&Parser);
		testRequire(
			testHttpSseOomParse(&Parser, Input, &Failure),
			"SSE parser did not recover after Reset"
		);
	}
	xrtHttpSseParserUnit(&Parser);
	xrtClearError();
	testMemoryDebugDrain("SSE parser OOM attempt leaked storage");
	return bComplete;
}



/* 逐个扫描 Parser 的动态缓冲增长点。 */
static void testHttpSseParserOom(void)
{
	static const uint8 Input[] = {
		0xEFu, 0xBBu, 0xBFu,
		':', ' ', 'p', 'i', 'n', 'g', '\r', '\n',
		'i', 'd', ':', ' ',
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f', '\r',
		'e', 'v', 'e', 'n', 't', ':', ' ',
		'u', 'p', 'd', 'a', 't', 'e', '\n',
		'r', 'e', 't', 'r', 'y', ':', ' ',
		'1', '2', '5', '0', '\n',
		'd', 'a', 't', 'a', ':', ' ',
		'f', 'i', 'r', 's', 't', '\r', '\n',
		'd', 'a', 't', 'a', ':', ' ', 0xFFu, '\n',
		'\n'
	};
	size_t iFail;

	for ( iFail = 0; iFail < 64u; iFail++ ) {
		if ( testHttpSseOomAttempt(
			(xbytesview){ Input, sizeof(Input) }, iFail
		) ) {
			testRequire(
				iFail >= 5u,
				"SSE parser OOM scan missed dynamic buffers"
			);
			return;
		}
	}
	testRequire(false, "SSE parser OOM scan did not converge");
}



/* 验证无状态写出零分配，便利构建失败则保持输出长度不变。 */
static void testHttpSseWriterOom(void)
{
	xhttpsseevent Event;
	char Output[64];
	str sBuilt;
	size_t iSize;

	memset(&Event, 0, sizeof(Event));
	Event.Data = XRT_STR_LITERAL("payload");
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	testRequire(
		xrtMemDebugFailAfter(0) &&
		xrtHttpSseEventWrite(
			&Event, Output, sizeof(Output), &iSize
		) && xrtHttpSseCommentWrite(
			XRT_STR_LITERAL("ping"), Output, sizeof(Output), &iSize
		) && !xrtMemDebugFailTriggered(),
		"SSE direct writers performed a logical allocation"
	);
	xrtMemDebugFailClear();

	iSize = 71u;
	testRequire(
		xrtMemDebugFailAfter(0),
		"SSE EventBuild fault setup failed"
	);
	sBuilt = xrtHttpSseEventBuild(&Event, &iSize);
	testRequire(
		(sBuilt == NULL) && (iSize == 71u) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"SSE EventBuild OOM was not atomic"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	iSize = 73u;
	testRequire(
		xrtMemDebugFailAfter(0),
		"SSE CommentBuild fault setup failed"
	);
	sBuilt = xrtHttpSseCommentBuild(
		XRT_STR_LITERAL("ping"), &iSize
	);
	testRequire(
		(sBuilt == NULL) && (iSize == 73u) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"SSE CommentBuild OOM was not atomic"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testMemoryDebugDrain("SSE writer OOM test leaked storage");
}



/* 验证 Parser 对象创建失败不产生半初始化对象。 */
static void testHttpSseCreateOom(void)
{
	xhttpsseparser* pParser;

	testRequire(
		xrtMemDebugFailAfter(0),
		"SSE ParserCreate fault setup failed"
	);
	pParser = xrtHttpSseParserCreate(NULL);
	testRequire(
		(pParser == NULL) && xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"SSE ParserCreate OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testMemoryDebugDrain("SSE ParserCreate OOM leaked storage");
}



/* 运行 SSE 动态内存故障注入回归。 */
int main(void)
{
	testHttpSseWriterOom();
	testHttpSseCreateOom();
	testHttpSseParserOom();
	printf("[PASS] HTTP SSE OOM\n");
	return 0;
}
