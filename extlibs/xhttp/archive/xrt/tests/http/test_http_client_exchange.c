#include "../test.h"



/* 比较借用文本与字面量。 */
static bool testHttpExchangeText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 自定义正文记录 Exchange 是否过早释放在途租约。 */
typedef struct test_http_exchange_source {
	size_t Opens;
	size_t Closes;
	size_t Releases;
	size_t Destroys;
	size_t Step;
	unsigned char Data[4];
} test_http_exchange_source;



/* 释放一次自定义正文租约并使原数据失效，便于发现悬空借用。 */
static void testHttpExchangeSourceRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_exchange_source* pSource =
		(test_http_exchange_source*)pContext;

	(void)pData;
	(void)iSize;
	pSource->Releases++;
	memset(pSource->Data, '#', sizeof(pSource->Data));
}



/* 自定义 Reader 第一次返回正文，之后正常结束。 */
static xhttpbodystatus testHttpExchangeSourceNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_exchange_source* pSource =
		(test_http_exchange_source*)pContext;

	if ( pSource->Step++ != 0 ) {
		return XHTTP_BODY_EOF;
	}
	pChunk->Data = pSource->Data;
	pChunk->Size = sizeof(pSource->Data) < iMaxBytes ?
		sizeof(pSource->Data) : iMaxBytes;
	pChunk->Release = testHttpExchangeSourceRelease;
	pChunk->Context = pSource;
	return XHTTP_BODY_DATA;
}



/* 记录 Reader 关闭，但不销毁仍由工厂拥有的测试状态。 */
static void testHttpExchangeSourceClose(ptr pContext)
{
	test_http_exchange_source* pSource =
		(test_http_exchange_source*)pContext;

	pSource->Closes++;
}



/* 为每个 Exchange 建立一条独立的测试 Reader。 */
static bool testHttpExchangeSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_exchange_source* pSource =
		(test_http_exchange_source*)pFactory;

	pSource->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpExchangeSourceNext;
	pOps->Close = testHttpExchangeSourceClose;
	*ppReader = pSource;
	return true;
}



/* 记录正文对象最后一个引用的销毁。 */
static void testHttpExchangeSourceDestroy(ptr pFactory)
{
	test_http_exchange_source* pSource =
		(test_http_exchange_source*)pFactory;

	pSource->Destroys++;
}



/* 创建携带自定义已知长度正文的 Exchange。 */
static xhttp1exchange* testHttpExchangeSourceCreate(
	test_http_exchange_source* pSource,
	bool bExpect
)
{
	static const xhttpbodyops Ops = {
		testHttpExchangeSourceOpen,
		testHttpExchangeSourceDestroy
	};
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/source")
	);
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops,
		pSource,
		sizeof(pSource->Data),
		XHTTP_BODY_NONE
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;

	if ( (pRequest == NULL) || (pBody == NULL) ||
		!xrtHttpRequestSetBody(pRequest, pBody) ||
		(bExpect &&
		 !xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Expect"),
			XRT_STR_LITERAL("100-continue")
		 )) ) {
		xrtHttpBodyDestroy(pBody);
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	xrtHttpBodyDestroy(pBody);
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, NULL, NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 创建请求、准备计划并把计划转移给 Exchange。 */
static xhttp1exchange* testHttpExchangeCreate(
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	bool bChunked,
	bool bExpect,
	const xhttp1exchangeconfig* pConfig,
	const xhttp1exchangeevents* pEvents
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		Method, Url
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;

	if ( (pRequest == NULL) ||
		((Body.Data != NULL) &&
		 !xrtHttpRequestSetBytes(
			pRequest,
			Body,
			(xstrview){ NULL, 0 }
		 )) ||
		(bChunked &&
		 !xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		 )) ||
		(bExpect &&
		 !xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Expect"),
			XRT_STR_LITERAL("100-continue")
		 )) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, pConfig, pEvents
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 验证 Exchange 配置和事件表支持未对齐快照并拒绝回绕范围。 */
static void testHttpExchangeConfigStorage(void)
{
	uint8 ConfigStorage[sizeof(xhttp1exchangeconfig) + 2u];
	uint8 EventsStorage[sizeof(xhttp1exchangeevents) + 2u];
	xhttp1exchangeconfig Config;
	xhttp1exchangeevents Events;
	xhttp1exchange* pExchange;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttp1ExchangeConfigInit(
		(xhttp1exchangeconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.Body.MaxBody == UINT64_C(67108864)) &&
		(Config.MaxInformational == 16u),
		"HTTP exchange config init did not support unaligned storage");
	memset(&Events, 0, sizeof(Events));
	memcpy(EventsStorage + 1u, &Events, sizeof(Events));
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/config"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		(const xhttp1exchangeconfig*)(const void*)(
			ConfigStorage + 1u
		),
		(const xhttp1exchangeevents*)(const void*)(
			EventsStorage + 1u
		)
	);
	testRequire(pExchange != NULL,
		"HTTP exchange did not snapshot unaligned inputs");
	xrtHttp1ExchangeDestroy(pExchange);

	Config.Head.MaxHead = 3;
	Config.Head.MaxStartLine = 1;
	Config.Head.MaxFieldLine = 1;
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/config-head"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		&Config,
		NULL
	);
	testRequire((pExchange == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP exchange accepted an unusable Header limit");
	xrtClearError();

	xrtHttp1ExchangeConfigInit(&Config);
	Config.Headers.MaxFields = SIZE_MAX;
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/config-storage"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		&Config,
		NULL
	);
	testRequire((pExchange == NULL) &&
		(xrtGetError() != NULL),
		"HTTP exchange deferred an impossible Header storage limit");

	xrtClearError();
	xrtHttp1ExchangeConfigInit((xhttp1exchangeconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP exchange config init accepted a wrapping range");
	xrtClearError();
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/config"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		(const xhttp1exchangeconfig*)(uintptr_t)(
			UINTPTR_MAX - 1u
		),
		NULL
	);
	testRequire((pExchange == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP exchange create accepted a wrapping config range");
	xrtClearError();
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/events"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		NULL,
		(const xhttp1exchangeevents*)(uintptr_t)(
			UINTPTR_MAX - 1u
		)
	);
	testRequire((pExchange == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP exchange create accepted a wrapping event range");
	xrtClearError();
}



/* 使用小窗口和短消费收集出站线路，返回第一个非 DATA 状态。 */
static xhttp1outputstatus testHttpExchangeOutput(
	xhttp1exchange* pExchange,
	char* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iTotal = 0;
	size_t iGuard = 0;

	while ( iGuard++ < (iCapacity * 4u + 128u) ) {
		xbytesview Data;
		xhttp1outputstatus Status =
			xrtHttp1ExchangeOutput(
				pExchange, 7, &Data
			);

		if ( Status != XHTTP1_OUTPUT_DATA ) {
			*pSize = iTotal;
			return Status;
		}
		testRequire((Data.Data != NULL) &&
			(Data.Size != 0),
			"HTTP exchange output returned empty DATA");
		{
			size_t iTake = Data.Size > 3 ?
				3 : Data.Size;

			testRequire(iTake <= (iCapacity - iTotal),
				"HTTP exchange output fixture overflow");
			memcpy(pOutput + iTotal, Data.Data, iTake);
			iTotal += iTake;
			testRequire(xrtHttp1ExchangeOutputConsume(
				pExchange, iTake
			), "HTTP exchange output consume failed");
		}
	}
	testRequire(false,
		"HTTP exchange output made no bounded progress");
	return XHTTP1_OUTPUT_ERROR;
}



/* 按固定窗口提交响应输入，只推进 Exchange 接受的前缀。 */
static xhttp1feedstatus testHttpExchangeFeed(
	xhttp1exchange* pExchange,
	const unsigned char* pInput,
	size_t iSize,
	size_t iSplit,
	bool bEnd,
	size_t* pTotalAccepted
)
{
	size_t iOffset = 0;
	size_t iGuard = 0;
	xhttp1feedstatus Status = XHTTP1_FEED_MORE;

	while ( iGuard++ < (iSize * 8u + 128u) ) {
		size_t iPiece = iSize - iOffset;
		size_t iAccepted = 0;

		if ( iPiece > iSplit ) {
			iPiece = iSplit;
		}
		Status = xrtHttp1ExchangeFeed(
			pExchange,
			iPiece != 0 ?
				(xbytesview){
					pInput + iOffset,
					iPiece
				} :
				(xbytesview){ NULL, 0 },
			bEnd && ((iOffset + iPiece) == iSize),
			&iAccepted
		);
		testRequire(iAccepted <= iPiece,
			"HTTP exchange Feed accepted beyond input");
		iOffset += iAccepted;
		if ( Status != XHTTP1_FEED_MORE ) {
			break;
		}
		if ( (iAccepted == 0) && (iPiece != 0) ) {
			testRequire(false,
				"HTTP exchange Feed made no progress");
		}
		if ( (iOffset == iSize) && !bEnd ) {
			break;
		}
	}
	testRequire(iGuard < (iSize * 8u + 128u),
		"HTTP exchange Feed exceeded progress guard");
	*pTotalAccepted = iOffset;
	return Status;
}



/* 暂停测试记录每个解帧正文片段，并在第一片内关闭输入门。 */
typedef struct test_http_exchange_pause {
	xhttp1exchange* Exchange;
	char Body[16];
	size_t Size;
	size_t Calls;
} test_http_exchange_pause;



/* 第一片正文内暂停，验证回调返回 true 与永久终止保持不同语义。 */
static bool testHttpExchangePauseBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_exchange_pause* pPause =
		(test_http_exchange_pause*)pData;

	(void)pResponse;
	if ( Data.Size > (sizeof(pPause->Body) - pPause->Size) ) {
		return false;
	}
	memcpy(pPause->Body + pPause->Size, Data.Data, Data.Size);
	pPause->Size += Data.Size;
	pPause->Calls++;
	if ( pPause->Calls == 1u ) {
		return xrtHttp1ExchangePause(pPause->Exchange);
	}
	return true;
}



/* 验证固定正文短写、响应分片、拥有型结果和连接复用。 */
static void testHttpExchangeFixed(void)
{
	static const char sExpected[] =
		"POST /api HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"hello";
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n"
		"X-Test: yes\r\n"
		"\r\n"
		"world";
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("http://example.test/api"),
			(xbytesview){ (cbytes)"hello", 5 },
			false,
			false,
			NULL,
			NULL
		);
	char Output[256];
	size_t iOutput;
	size_t iAccepted;
	const xhttpresponse* pResponse;

	testRequire(pExchange != NULL,
		"HTTP exchange fixed create failed");
	testRequire(testHttpExchangeOutput(
		pExchange,
		Output,
		sizeof(Output),
		&iOutput
	) == XHTTP1_OUTPUT_DONE,
		"HTTP exchange fixed output did not finish");
	testRequire((iOutput == (sizeof(sExpected) - 1u)) &&
		(memcmp(Output, sExpected, iOutput) == 0) &&
		xrtHttp1ExchangeRequestComplete(pExchange) &&
		(xrtHttp1ExchangeRequestWireBytes(pExchange) ==
		 (uint64)iOutput),
		"HTTP exchange fixed output mismatch");

	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		1,
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange fixed response did not finish");
	pResponse = xrtHttp1ExchangeResponse(pExchange);
	testRequire((iAccepted == (sizeof(Response) - 1u)) &&
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == 200) &&
		testHttpExchangeText(
			xrtHttpResponseHeader(
				pResponse,
				XRT_STR_LITERAL("X-Test")
			)->Value,
			"yes"
		) &&
		(xrtHttpResponseBody(pResponse).Size == 5) &&
		(memcmp(
			xrtHttpResponseBody(pResponse).Data,
			"world",
			5
		) == 0) &&
		(xrtHttpResponseWireBodyBytes(pResponse) == 5) &&
		xrtHttp1ExchangeReusable(pExchange),
		"HTTP exchange fixed response mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证 chunked 请求生成和 chunked 响应 trailer 解码。 */
static void testHttpExchangeChunked(void)
{
	static const char sExpected[] =
		"POST /stream HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"5\r\nhello\r\n"
		"0\r\n\r\n";
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"4;part=1\r\nWiki\r\n"
		"5\r\npedia\r\n"
		"0\r\nDigest: value\r\n\r\n";
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL(
				"http://example.test/stream"
			),
			(xbytesview){ (cbytes)"hello", 5 },
			true,
			false,
			NULL,
			NULL
		);
	char Output[256];
	size_t iOutput;
	size_t iAccepted;
	const xhttpresponse* pResponse;
	const xhttpfield* pTrailer;

	testRequire(pExchange != NULL,
		"HTTP exchange chunked create failed");
	testRequire(testHttpExchangeOutput(
		pExchange,
		Output,
		sizeof(Output),
		&iOutput
	) == XHTTP1_OUTPUT_DONE,
		"HTTP exchange chunked output did not finish");
	testRequire((iOutput == (sizeof(sExpected) - 1u)) &&
		(memcmp(Output, sExpected, iOutput) == 0),
		"HTTP exchange chunked output mismatch");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		1,
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange chunked response did not finish");
	pResponse = xrtHttp1ExchangeResponse(pExchange);
	pTrailer = xrtHttpResponseTrailer(
		pResponse,
		XRT_STR_LITERAL("Digest")
	);
	testRequire((iAccepted == (sizeof(Response) - 1u)) &&
		(xrtHttpResponseBody(pResponse).Size == 9) &&
		(memcmp(
			xrtHttpResponseBody(pResponse).Data,
			"Wikipedia",
			9
		) == 0) &&
		(xrtHttpResponseWireBodyBytes(pResponse) == 9) &&
		(pTrailer != NULL) &&
		testHttpExchangeText(pTrailer->Value, "value") &&
		xrtHttp1ExchangeReusable(pExchange),
		"HTTP exchange chunked response mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证客户端只在显式开启后交付未解码的扩展 Transfer Coding。 */
static void testHttpExchangeRawTransferCoding(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: gzip, chunked\r\n\r\n"
		"3\r\nabc\r\n0\r\n\r\n";
	xhttp1exchangeconfig Config;
	xhttp1exchange* pExchange;
	const xhttpresponse* pResponse;
	xbytesview Body;
	size_t iAccepted = 0;

	xrtHttp1ExchangeConfigInit(&Config);
	Config.AllowRawTransferCodings = true;
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/raw"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		&Config,
		NULL
	);
	testRequire((pExchange != NULL) &&
		(testHttpExchangeFeed(
			pExchange,
			Response,
			sizeof(Response) - 1u,
			1,
			false,
			&iAccepted
		) == XHTTP1_FEED_DONE),
		"HTTP exchange raw Transfer Coding opt-in failed");
	pResponse = xrtHttp1ExchangeResponse(pExchange);
	Body = xrtHttpResponseBody(pResponse);
	testRequire((iAccepted == (sizeof(Response) - 1u)) &&
		(Body.Size == 3) &&
		(memcmp(Body.Data, "abc", 3) == 0),
		"HTTP exchange raw Transfer Coding body mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 记录信息响应的稳定只读视图。 */
typedef struct test_http_exchange_info {
	uint32 Count;
	uint16 LastStatus;
} test_http_exchange_info;



/* 消费一条信息响应。 */
static bool testHttpExchangeInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_exchange_info* pInfo =
		(test_http_exchange_info*)pData;

	pInfo->Count++;
	pInfo->LastStatus =
		xrtHttpResponseStatus(pResponse);
	return true;
}



/* 验证 103、100 Continue 和最终响应的顺序契约。 */
static void testHttpExchangeContinue(void)
{
	static const unsigned char Informational[] =
		"HTTP/1.1 103 Early Hints\r\n"
		"Link: </a.css>; rel=preload\r\n"
		"\r\n"
		"HTTP/1.1 100 Continue\r\n"
		"\r\n";
	static const unsigned char Final[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	test_http_exchange_info Info = { 0 };
	xhttp1exchangeevents Events = {
		testHttpExchangeInformational,
		NULL,
		NULL,
		&Info
	};
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL(
				"http://example.test/continue"
			),
			(xbytesview){ (cbytes)"data", 4 },
			false,
			true,
			NULL,
			&Events
		);
	char Output[256];
	size_t iHead;
	size_t iBody;
	size_t iAccepted;

	testRequire(pExchange != NULL,
		"HTTP exchange continue create failed");
	testRequire(testHttpExchangeOutput(
		pExchange,
		Output,
		sizeof(Output),
		&iHead
	) == XHTTP1_OUTPUT_CONTINUE,
		"HTTP exchange did not wait for 100 Continue");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Informational,
		sizeof(Informational) - 1u,
		5,
		false,
		&iAccepted
	) == XHTTP1_FEED_MORE,
		"HTTP exchange informational sequence failed");
	testRequire((iAccepted ==
		(sizeof(Informational) - 1u)) &&
		(Info.Count == 2) &&
		(Info.LastStatus == 100) &&
		(xrtHttp1ExchangeInformationalCount(
			pExchange
		) == 2),
		"HTTP exchange informational callback mismatch");
	testRequire(testHttpExchangeOutput(
		pExchange,
		Output + iHead,
		sizeof(Output) - iHead,
		&iBody
	) == XHTTP1_OUTPUT_DONE,
		"HTTP exchange body did not continue");
	testRequire((iBody == 4) &&
		(memcmp(Output + iHead, "data", 4) == 0),
		"HTTP exchange continued body mismatch");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Final,
		sizeof(Final) - 1u,
		sizeof(Final),
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange final response after Continue failed");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证最终响应可以在 100 到达前停止尚未打开的正文。 */
static void testHttpExchangeEarlyFinal(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 417 Expectation Failed\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("http://example.test/early"),
			(xbytesview){ (cbytes)"body", 4 },
			false,
			true,
			NULL,
			NULL
		);
	char Output[256];
	xbytesview Data;
	size_t iHead;
	size_t iAccepted;

	testRequire((pExchange != NULL) &&
		(testHttpExchangeOutput(
			pExchange,
			Output,
			sizeof(Output),
			&iHead
		) == XHTTP1_OUTPUT_CONTINUE),
		"HTTP exchange early final setup failed");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		sizeof(Response),
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange early final response failed");
	testRequire((xrtHttpResponseStatus(
		xrtHttp1ExchangeResponse(pExchange)
	) == 417) &&
		!xrtHttp1ExchangeRequestComplete(pExchange) &&
		!xrtHttp1ExchangeReusable(pExchange) &&
		(xrtHttp1ExchangeOutput(
			pExchange, 16, &Data
		) == XHTTP1_OUTPUT_DONE),
		"HTTP exchange early final did not stop output");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证提前最终响应不会打开正文，也不会释放仍在发送的借用数据。 */
static void testHttpExchangeEarlyFinalLifetime(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 413 Content Too Large\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	test_http_exchange_source Waiting = {
		0, 0, 0, 0, 0, { 'b', 'o', 'd', 'y' }
	};
	test_http_exchange_source InFlight = {
		0, 0, 0, 0, 0, { 'b', 'o', 'd', 'y' }
	};
	xhttp1exchange* pWaiting =
		testHttpExchangeSourceCreate(&Waiting, true);
	xhttp1exchange* pInFlight =
		testHttpExchangeSourceCreate(&InFlight, false);
	char Output[256];
	xbytesview Data;
	size_t iHead;
	size_t iAccepted;

	testRequire((pWaiting != NULL) &&
		(testHttpExchangeOutput(
			pWaiting,
			Output,
			sizeof(Output),
			&iHead
		) == XHTTP1_OUTPUT_CONTINUE) &&
		(Waiting.Opens == 0),
		"HTTP exchange opened a waiting body");
	testRequire(testHttpExchangeFeed(
		pWaiting,
		Response,
		sizeof(Response) - 1u,
		sizeof(Response),
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange waiting body early final failed");
	testRequire((Waiting.Opens == 0) &&
		(Waiting.Closes == 0) &&
		(Waiting.Releases == 0),
		"HTTP exchange touched a body rejected before Continue");
	xrtHttp1ExchangeDestroy(pWaiting);
	testRequire(Waiting.Destroys == 1,
		"HTTP exchange waiting body lifetime mismatch");

	testRequire((pInFlight != NULL) &&
		(xrtHttp1ExchangeOutput(
			pInFlight,
			sizeof(Output),
			&Data
		) == XHTTP1_OUTPUT_DATA),
		"HTTP exchange in-flight Header setup failed");
	testRequire(xrtHttp1ExchangeOutputConsume(
		pInFlight, Data.Size
	), "HTTP exchange in-flight Header consume failed");
	testRequire((xrtHttp1ExchangeOutput(
		pInFlight, 16, &Data
	) == XHTTP1_OUTPUT_DATA) &&
		(Data.Size == 4) &&
		(memcmp(Data.Data, "body", 4) == 0) &&
		(InFlight.Opens == 1),
		"HTTP exchange in-flight body setup failed");
	testRequire(testHttpExchangeFeed(
		pInFlight,
		Response,
		sizeof(Response) - 1u,
		sizeof(Response),
		false,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange in-flight early final failed");
	testRequire((InFlight.Releases == 0) &&
		(InFlight.Closes == 0) &&
		(memcmp(Data.Data, "body", 4) == 0) &&
		(xrtHttp1ExchangeOutput(
			pInFlight, 16, &Data
		) == XHTTP1_OUTPUT_DONE),
		"HTTP exchange invalidated an in-flight output lease");
	testRequire(xrtHttp1ExchangeOutputConsume(
		pInFlight, 4
	), "HTTP exchange in-flight final consume failed");
	testRequire((InFlight.Releases == 1) &&
		(InFlight.Closes == 1) &&
		!xrtHttp1ExchangeRequestComplete(pInFlight),
		"HTTP exchange in-flight lease was not released once");
	xrtHttp1ExchangeDestroy(pInFlight);
	testRequire(InFlight.Destroys == 1,
		"HTTP exchange in-flight body lifetime mismatch");
}



/* 收集流式正文且不让响应分配正文副本。 */
typedef struct test_http_exchange_stream {
	char Data[32];
	size_t Size;
	bool Abort;
} test_http_exchange_stream;



/* 流式消费正文。 */
static bool testHttpExchangeBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_exchange_stream* pStream =
		(test_http_exchange_stream*)pData;

	(void)pResponse;
	if ( pStream->Abort ) {
		return false;
	}
	testRequire(Data.Size <=
		(sizeof(pStream->Data) - pStream->Size),
		"HTTP exchange stream fixture overflow");
	memcpy(
		pStream->Data + pStream->Size,
		Data.Data,
		Data.Size
	);
	pStream->Size += Data.Size;
	return true;
}



/* 验证流式交付、关闭定界 EOF 和无正文缓冲。 */
static void testHttpExchangeStreamingClose(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Connection: close\r\n"
		"\r\n"
		"close-body";
	test_http_exchange_stream Stream = { { 0 }, 0, false };
	xhttp1exchangeevents Events = {
		NULL,
		NULL,
		testHttpExchangeBody,
		&Stream
	};
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("http://example.test/close"),
			(xbytesview){ NULL, 0 },
			false,
			false,
			NULL,
			&Events
		);
	char Output[128];
	size_t iOutput;
	size_t iAccepted;
	const xhttpresponse* pResponse;

	testRequire((pExchange != NULL) &&
		(testHttpExchangeOutput(
			pExchange,
			Output,
			sizeof(Output),
			&iOutput
		) == XHTTP1_OUTPUT_DONE),
		"HTTP exchange streaming setup failed");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		3,
		true,
		&iAccepted
	) == XHTTP1_FEED_DONE,
		"HTTP exchange close-delimited response failed");
	pResponse = xrtHttp1ExchangeResponse(pExchange);
	testRequire((Stream.Size == 10) &&
		(memcmp(Stream.Data, "close-body", 10) == 0) &&
		((xrtHttpResponseFlags(pResponse) &
		  (uint32)XHTTP_RESPONSE_STREAMED) != 0) &&
		(xrtHttpResponseBody(pResponse).Size == 0) &&
		(xrtHttpResponseBodyBytes(pResponse) == 10) &&
		!xrtHttp1ExchangeReusable(pExchange),
		"HTTP exchange streamed response mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证 101 完成 Header 后准确保留外部升级字节边界。 */
static void testHttpExchangeUpgrade(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: websocket\r\n"
		"\r\n"
		"XYZ";
	static const size_t HeadSize = sizeof(Response) - 1u - 3u;
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("http://example.test/ws"),
			(xbytesview){ NULL, 0 },
			false,
			false,
			NULL,
			NULL
		);
	char Output[128];
	size_t iOutput;
	size_t iAccepted;

	testRequire((pExchange != NULL) &&
		(testHttpExchangeOutput(
			pExchange,
			Output,
			sizeof(Output),
			&iOutput
		) == XHTTP1_OUTPUT_DONE),
		"HTTP exchange upgrade setup failed");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		sizeof(Response),
		false,
		&iAccepted
	) == XHTTP1_FEED_UPGRADED,
		"HTTP exchange upgrade response failed");
	testRequire((iAccepted == HeadSize) &&
		xrtHttp1ExchangeUpgraded(pExchange) &&
		((xrtHttpResponseFlags(
			xrtHttp1ExchangeResponse(pExchange)
		) & (uint32)XHTTP_RESPONSE_UPGRADED) != 0) &&
		!xrtHttp1ExchangeReusable(pExchange),
		"HTTP exchange upgrade boundary mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证 CONNECT 2xx 忽略分帧字段并准确交出隧道余量。 */
static void testHttpExchangeConnectTunnel(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 Connection Established\r\n"
		"Content-Length: 99\r\n"
		"Transfer-Encoding: gzip\r\n"
		"\r\n"
		"XYZ";
	static const size_t HeadSize = sizeof(Response) - 1u - 3u;
	xhttp1exchange* pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("https://example.test/"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		NULL,
		NULL
	);
	char Output[256];
	size_t iOutput;
	size_t iAccepted;

	testRequire((pExchange != NULL) &&
		(testHttpExchangeOutput(
			pExchange,
			Output,
			sizeof(Output),
			&iOutput
		) == XHTTP1_OUTPUT_DONE) &&
		(strncmp(
			Output,
			"CONNECT example.test:443 HTTP/1.1\r\n",
			sizeof(
				"CONNECT example.test:443 HTTP/1.1\r\n"
			) - 1u
		) == 0),
		"HTTP exchange CONNECT request setup failed");
	testRequire(testHttpExchangeFeed(
		pExchange,
		Response,
		sizeof(Response) - 1u,
		sizeof(Response),
		false,
		&iAccepted
	) == XHTTP1_FEED_UPGRADED,
		"HTTP exchange CONNECT tunnel response failed");
	testRequire((iAccepted == HeadSize) &&
		xrtHttp1ExchangeUpgraded(pExchange) &&
		((xrtHttpResponseFlags(
			xrtHttp1ExchangeResponse(pExchange)
		) & (uint32)XHTTP_RESPONSE_UPGRADED) != 0) &&
		!xrtHttp1ExchangeReusable(pExchange),
		"HTTP exchange CONNECT tunnel boundary mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证显式放宽限额后，大 Header 按实际长度增长而不受固定缓冲限制。 */
static void testHttpExchangeDynamicHead(void)
{
	static const cstr sPrefix =
		"HTTP/1.1 200 OK\r\nX-Large: ";
	static const cstr sSuffix =
		"\r\nContent-Length: 0\r\n\r\n";
	static const size_t ValueSize = 32768;
	xhttp1exchangeconfig Config;
	xhttp1exchange* pExchange;
	unsigned char* pResponse;
	size_t iPrefix = strlen(sPrefix);
	size_t iSuffix = strlen(sSuffix);
	size_t iSize = iPrefix + ValueSize + iSuffix;
	size_t iAccepted;
	const xhttpfield* pField;

	pResponse = (unsigned char*)malloc(iSize);
	testRequire(pResponse != NULL,
		"HTTP exchange large Header fixture allocation failed");
	memcpy(pResponse, sPrefix, iPrefix);
	memset(pResponse + iPrefix, 'v', ValueSize);
	memcpy(
		pResponse + iPrefix + ValueSize,
		sSuffix,
		iSuffix
	);
	xrtHttp1ExchangeConfigInit(&Config);
	Config.Head.MaxHead = (uint32)(iSize + 64u);
	Config.Head.MaxFieldLine = (uint32)(ValueSize + 32u);
	Config.Headers.MaxValue = ValueSize;
	Config.Headers.MaxBytes = iSize + 64u;
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/large"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		&Config,
		NULL
	);
	testRequire((pExchange != NULL) &&
		(testHttpExchangeFeed(
			pExchange,
			pResponse,
			iSize,
			257,
			false,
			&iAccepted
		) == XHTTP1_FEED_DONE),
		"HTTP exchange large Header response failed");
	pField = xrtHttpResponseHeader(
		xrtHttp1ExchangeResponse(pExchange),
		XRT_STR_LITERAL("X-Large")
	);
	testRequire((iAccepted == iSize) &&
		(pField != NULL) &&
		(pField->Value.Size == ValueSize) &&
		(pField->Value.Data[0] == 'v') &&
		(pField->Value.Data[ValueSize - 1u] == 'v'),
		"HTTP exchange large Header value mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
	free(pResponse);
}



/* Header 回调拒绝响应。 */
static bool testHttpExchangeRejectHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	(void)pResponse;
	(void)pData;
	return false;
}



/* Header 回调发布可识别的根因后拒绝响应。 */
static bool testHttpExchangeRejectHeadersWithCause(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xerror* pError;

	(void)pResponse;
	(void)pData;
	pError = xrtErrorCreate(
		XERR_MEMORY,
		"test.http.callback",
		71,
		"callback allocation failed"
	);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 验证禁止分帧、信息响应上限、EOF 和回调失败错误。 */
static void testHttpExchangeErrors(void)
{
	static const cstr Responses[] = {
		"HTTP/1.1 204 No Content\r\n"
		"Content-Length: 0\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
		"Trailer: Digest\r\n"
		"Trailer: Content-Length\r\n"
		"Transfer-Encoding: chunked\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Content-Length: 1\r\n\r\n",
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 4\r\n\r\nab",
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n\r\nZ\r\n",
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: gzip, chunked\r\n\r\n"
		"3\r\nabc\r\n0\r\n\r\n"
	};
	static const xhttp1exchangeerror Codes[] = {
		XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
		XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
		XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
		XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
		XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
		XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING
	};
	size_t i;

	for ( i = 0; i < (sizeof(Responses) /
		sizeof(Responses[0])); i++ ) {
		xhttp1exchange* pExchange =
			testHttpExchangeCreate(
				XRT_STR_LITERAL("GET"),
				XRT_STR_LITERAL(
					"http://example.test/error"
				),
				(xbytesview){ NULL, 0 },
				false,
				false,
				NULL,
				NULL
			);
		size_t iAccepted;
		xerror* pStale = NULL;

		testRequire(pExchange != NULL,
			"HTTP exchange error fixture create failed");
		if ( i == 0 ) {
			pStale = xrtErrorCreate(
				XERR_IO,
				"test.unrelated",
				91,
				"unrelated previous error"
			);
			testRequire(pStale != NULL,
				"HTTP exchange stale error setup failed");
			xrtSetError(pStale);
			xrtErrorFree(pStale);
		}
		testRequire(testHttpExchangeFeed(
			pExchange,
			(const unsigned char*)Responses[i],
			strlen(Responses[i]),
			strlen(Responses[i]),
			i >= 2,
			&iAccepted
		) == XHTTP1_FEED_ERROR,
			"HTTP exchange accepted invalid response");
		testRequire((xrtHttp1ExchangeError(
			pExchange
		) != NULL) &&
			(strcmp(
				xrtErrorDomain(
					xrtHttp1ExchangeError(
						pExchange
					)
				),
				"xrt.http.exchange"
			) == 0) &&
			(xrtErrorCode(
				xrtHttp1ExchangeError(pExchange)
			) == (int32)Codes[i]),
			"HTTP exchange invalid response error mismatch");
		if ( i == 0 ) {
			testRequire(xrtErrorCause(
				xrtHttp1ExchangeError(pExchange)
			) == NULL,
				"HTTP exchange captured an unrelated stale cause");
		}
		xrtHttp1ExchangeDestroy(pExchange);
		xrtClearError();
	}

	{
		xhttp1exchangeconfig Config;
		xhttp1exchange* pExchange;
		size_t iAccepted;
		cstr sInfo =
			"HTTP/1.1 103 Early Hints\r\n\r\n"
			"HTTP/1.1 102 Processing\r\n\r\n";

		xrtHttp1ExchangeConfigInit(&Config);
		Config.MaxInformational = 1;
		pExchange = testHttpExchangeCreate(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("http://example.test/info"),
			(xbytesview){ NULL, 0 },
			false,
			false,
			&Config,
			NULL
		);
		testRequire((pExchange != NULL) &&
			(testHttpExchangeFeed(
				pExchange,
				(const unsigned char*)sInfo,
				strlen(sInfo),
				strlen(sInfo),
				false,
				&iAccepted
			) == XHTTP1_FEED_ERROR) &&
			(xrtErrorCode(
				xrtHttp1ExchangeError(pExchange)
			) ==
			 XHTTP1_EXCHANGE_ERROR_INFORMATIONAL_LIMIT),
			"HTTP exchange informational limit mismatch");
		xrtHttp1ExchangeDestroy(pExchange);
		xrtClearError();
	}

	{
		xhttp1exchangeevents Events = {
			NULL,
			testHttpExchangeRejectHeaders,
			NULL,
			NULL
		};
		xhttp1exchange* pExchange =
			testHttpExchangeCreate(
				XRT_STR_LITERAL("GET"),
				XRT_STR_LITERAL(
					"http://example.test/callback"
				),
				(xbytesview){ NULL, 0 },
				false,
				false,
				NULL,
				&Events
			);
		size_t iAccepted;
		cstr sResponse =
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 0\r\n\r\n";
		xerror* pStale = xrtErrorCreate(
			XERR_IO,
			"test.stale.callback",
			70,
			"stale callback error"
		);

		testRequire(pStale != NULL,
			"HTTP exchange callback stale error setup failed");
		xrtSetError(pStale);
		xrtErrorFree(pStale);
		testRequire((pExchange != NULL) &&
			(testHttpExchangeFeed(
				pExchange,
				(const unsigned char*)sResponse,
				strlen(sResponse),
				strlen(sResponse),
				false,
				&iAccepted
			) == XHTTP1_FEED_ERROR) &&
			(xrtErrorCode(
				xrtHttp1ExchangeError(pExchange)
			) ==
			 XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK) &&
			(xrtErrorCause(
				xrtHttp1ExchangeError(pExchange)
			) == NULL),
			"HTTP exchange Header callback abort mismatch");
		xrtHttp1ExchangeDestroy(pExchange);
		xrtClearError();
	}

	{
		xhttp1exchangeevents Events = {
			NULL,
			testHttpExchangeRejectHeadersWithCause,
			NULL,
			NULL
		};
		xhttp1exchange* pExchange =
			testHttpExchangeCreate(
				XRT_STR_LITERAL("GET"),
				XRT_STR_LITERAL(
					"http://example.test/callback-cause"
				),
				(xbytesview){ NULL, 0 },
				false,
				false,
				NULL,
				&Events
			);
		size_t iAccepted;
		cstr sResponse =
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 0\r\n\r\n";
		const xerror* pCause;

		testRequire((pExchange != NULL) &&
			(testHttpExchangeFeed(
				pExchange,
				(const unsigned char*)sResponse,
				strlen(sResponse),
				strlen(sResponse),
				false,
				&iAccepted
			) == XHTTP1_FEED_ERROR),
			"HTTP exchange callback cause was accepted");
		pCause = xrtErrorCause(
			xrtHttp1ExchangeError(pExchange)
		);
		testRequire((pCause != NULL) &&
			(xrtErrorKind(pCause) == XERR_MEMORY) &&
			(xrtErrorCode(pCause) == 71) &&
			(strcmp(
				xrtErrorDomain(pCause),
				"test.http.callback"
			) == 0),
			"HTTP exchange callback cause was not preserved");
		xrtHttp1ExchangeDestroy(pExchange);
		xrtClearError();
	}

	{
		test_http_exchange_stream Stream = {
			{ 0 }, 0, true
		};
		xhttp1exchangeevents Events = {
			NULL,
			NULL,
			testHttpExchangeBody,
			&Stream
		};
		xhttp1exchange* pExchange =
			testHttpExchangeCreate(
				XRT_STR_LITERAL("GET"),
				XRT_STR_LITERAL(
					"http://example.test/body-callback"
				),
				(xbytesview){ NULL, 0 },
				false,
				false,
				NULL,
				&Events
			);
		size_t iAccepted;
		cstr sResponse =
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 1\r\n\r\nx";

		testRequire((pExchange != NULL) &&
			(testHttpExchangeFeed(
				pExchange,
				(const unsigned char*)sResponse,
				strlen(sResponse),
				strlen(sResponse),
				false,
				&iAccepted
			) == XHTTP1_FEED_ERROR) &&
			(xrtErrorCode(
				xrtHttp1ExchangeError(pExchange)
			) ==
			 XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK) &&
			(iAccepted == strlen(sResponse)),
			"HTTP exchange Body callback abort mismatch");
		xrtHttp1ExchangeDestroy(pExchange);
		xrtClearError();
	}
}



/* 验证暂停只确认当前片段，保留后缀，并在恢复后继承已到达的 EOF。 */
static void testHttpExchangePause(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n\r\n"
		"3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n";
	test_http_exchange_pause Pause;
	xhttp1exchangeevents Events;
	xhttp1exchange* pExchange;
	xhttp1feedstatus Status;
	size_t iAccepted;
	size_t iRest;

	memset(&Pause, 0, sizeof(Pause));
	memset(&Events, 0, sizeof(Events));
	Events.Body = testHttpExchangePauseBody;
	Events.Data = &Pause;
	pExchange = testHttpExchangeCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/pause"),
		(xbytesview){ NULL, 0 },
		false,
		false,
		NULL,
		&Events
	);
	testRequire(pExchange != NULL, "HTTP exchange pause setup failed");
	Pause.Exchange = pExchange;
	Status = xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){ Response, sizeof(Response) - 1u },
		true,
		&iAccepted
	);
	testRequire(
		(Status == XHTTP1_FEED_PAUSED) &&
		xrtHttp1ExchangePaused(pExchange) &&
		(iAccepted != 0) && (iAccepted < (sizeof(Response) - 1u)) &&
		(Pause.Calls == 1u) && (Pause.Size == 3u) &&
		(memcmp(Pause.Body, "abc", 3u) == 0),
		"HTTP exchange did not pause after the current body item"
	);
	iRest = sizeof(Response) - 1u - iAccepted;
	Status = xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){ Response + iAccepted, iRest },
		false,
		&iAccepted
	);
	testRequire(
		(Status == XHTTP1_FEED_PAUSED) && (iAccepted == 0),
		"HTTP exchange consumed input while paused"
	);
	testRequire(
		xrtHttp1ExchangeResume(pExchange) &&
		xrtHttp1ExchangeResume(pExchange) &&
		!xrtHttp1ExchangePaused(pExchange),
		"HTTP exchange resume was not idempotent"
	);
	Status = xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){ Response + (sizeof(Response) - 1u - iRest), iRest },
		false,
		&iAccepted
	);
	testRequire(
		(Status == XHTTP1_FEED_DONE) && (iAccepted == iRest) &&
		(Pause.Calls == 2u) && (Pause.Size == 6u) &&
		(memcmp(Pause.Body, "abcdef", 6u) == 0),
		"HTTP exchange did not continue after resume"
	);
	testRequire(
		!xrtHttp1ExchangePause(pExchange) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP exchange accepted pause after completion"
	);
	xrtClearError();
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证响应所有权可以在终态从 Exchange 取走。 */
static void testHttpExchangeTakeResponse(void)
{
	static const unsigned char Response[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
	xhttp1exchange* pExchange =
		testHttpExchangeCreate(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("http://example.test/take"),
			(xbytesview){ NULL, 0 },
			false,
			false,
			NULL,
			NULL
		);
	xhttpresponse* pResponse;
	size_t iAccepted;

	testRequire((pExchange != NULL) &&
		(testHttpExchangeFeed(
			pExchange,
			Response,
			sizeof(Response) - 1u,
			sizeof(Response),
			false,
			&iAccepted
		) == XHTTP1_FEED_DONE),
		"HTTP exchange take response setup failed");
	pResponse = xrtHttp1ExchangeTakeResponse(pExchange);
	testRequire((pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == 200) &&
		(xrtHttp1ExchangeResponse(pExchange) == NULL),
		"HTTP exchange response ownership transfer failed");
	xrtHttp1ExchangeDestroy(pExchange);
	xrtHttpResponseDestroy(pResponse);
}



/* 运行 HTTP/1 单次 Exchange 双向状态机测试。 */
int main(void)
{
	testHttpExchangeConfigStorage();
	testHttpExchangeFixed();
	testHttpExchangeChunked();
	testHttpExchangeRawTransferCoding();
	testHttpExchangeContinue();
	testHttpExchangeEarlyFinal();
	testHttpExchangeEarlyFinalLifetime();
	testHttpExchangeStreamingClose();
	testHttpExchangeUpgrade();
	testHttpExchangeConnectTunnel();
	testHttpExchangeDynamicHead();
	testHttpExchangeErrors();
	testHttpExchangePause();
	testHttpExchangeTakeResponse();
	printf("[PASS] HTTP client exchange\n");
	return 0;
}
