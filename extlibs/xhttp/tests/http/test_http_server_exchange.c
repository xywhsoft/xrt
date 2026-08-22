#include "../test.h"



/* 测试事件状态记录策略、正文片段、暂停和路由限额。 */
typedef struct test_http_server_exchange_context {
	xhttpserverbodypolicy Policy;
	unsigned char Body[64];
	size_t BodySize;
	size_t Headers;
	size_t Bodies;
	size_t Completes;
	uint64 Limit;
	bool SetLimit;
	bool PauseFirst;
	bool FailBody;
	bool InvalidPolicy;
	bool Reenter;
} test_http_server_exchange_context;



/* 比较借用文本视图与零结尾常量。 */
static bool testHttpServerExchangeTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 在 Header 事件中选择正文策略并可覆盖路由限额。 */
static xhttpserverbodypolicy testHttpServerExchangeHeaders(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_exchange_context* pContext =
		(test_http_server_exchange_context*)pData;

	testRequire((pExchange != NULL) && (pRequest != NULL),
		"HTTP server Headers callback input is null");
	pContext->Headers++;
	if ( pContext->SetLimit ) {
		(void)xrtHttp1ServerExchangeSetBodyLimit(
			pExchange, pContext->Limit
		);
	}
	if ( pContext->InvalidPolicy ) {
		return (xhttpserverbodypolicy)99;
	}
	return pContext->Policy;
}



/* 收集流式正文，并在首段之后模拟消费者背压。 */
static bool testHttpServerExchangeBody(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	test_http_server_exchange_context* pContext =
		(test_http_server_exchange_context*)pData;

	testRequire((pExchange != NULL) && (pRequest != NULL) &&
		(Data.Data != NULL) && (Data.Size != 0),
		"HTTP server Body callback input is invalid");
	testRequire(Data.Size <=
		(sizeof(pContext->Body) - pContext->BodySize),
		"HTTP server Body callback fixture overflowed");
	memcpy(
		pContext->Body + pContext->BodySize,
		Data.Data,
		Data.Size
	);
	pContext->BodySize += Data.Size;
	pContext->Bodies++;
	if ( pContext->Reenter ) {
		size_t iAccepted = 0;

		testRequire(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){ NULL, 0 },
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR,
			"HTTP server callback reentry was accepted");
	}
	if ( pContext->PauseFirst &&
		(pContext->Bodies == 1) ) {
		testRequire(xrtHttp1ServerExchangePause(pExchange),
			"HTTP server Body callback pause failed");
	}
	if ( pContext->FailBody ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.server.body",
			77,
			"body sink failed"
		);

		testRequire(pError != NULL,
			"HTTP server Body callback error create failed");
		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	return true;
}



/* 记录完整请求事件。 */
static bool testHttpServerExchangeComplete(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_exchange_context* pContext =
		(test_http_server_exchange_context*)pData;

	testRequire((pExchange != NULL) && (pRequest != NULL),
		"HTTP server Complete callback input is null");
	pContext->Completes++;
	return true;
}



/* 创建使用完整测试事件表的 Exchange。 */
static xhttp1serverexchange* testHttpServerExchangeCreate(
	const xhttp1serverconfig* pConfig,
	test_http_server_exchange_context* pContext
)
{
	xhttp1serverevents Events = {
		testHttpServerExchangeHeaders,
		testHttpServerExchangeBody,
		testHttpServerExchangeComplete,
		pContext
	};

	return xrtHttp1ServerExchangeCreate(
		pConfig, &Events
	);
}



/* 验证配置和事件描述符支持未对齐存储，并拒绝回绕地址。 */
static void testHttpServerExchangeMemoryContracts(void)
{
	static const char Wire[] =
		"GET /memory HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n";
	uint8 ConfigStorage[sizeof(xhttp1serverconfig) + 2u];
	uint8 EventsStorage[sizeof(xhttp1serverevents) + 2u];
	xhttp1serverconfig Config;
	xhttp1serverevents Events = {
		NULL,
		NULL,
		testHttpServerExchangeComplete,
		NULL
	};
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_BUFFER
	};
	xhttp1serverexchange* pExchange;
	size_t iAccepted = 0u;

	Events.Data = &Context;
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttp1ServerConfigInit((xhttp1serverconfig*)(void*)(
		ConfigStorage + 1u
	));
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.Head.MaxHead == 65536u) &&
		(Config.Body.MaxBody == (UINT64_C(4) * 1024u * 1024u)),
		"HTTP server config init did not support unaligned storage");
	memset(EventsStorage, 0xA5, sizeof(EventsStorage));
	memcpy(EventsStorage + 1u, &Events, sizeof(Events));
	pExchange = xrtHttp1ServerExchangeCreate(
		(const xhttp1serverconfig*)(const void*)(ConfigStorage + 1u),
		(const xhttp1serverevents*)(const void*)(EventsStorage + 1u)
	);
	testRequire((pExchange != NULL) &&
		(EventsStorage[0] == 0xA5) &&
		(EventsStorage[sizeof(EventsStorage) - 1u] == 0xA5),
		"HTTP server Exchange did not snapshot unaligned descriptors");
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	memset(EventsStorage + 1u, 0, sizeof(Events));
	testRequire((xrtHttp1ServerExchangeFeed(
		pExchange,
		XRT_BYTES_LITERAL(Wire),
		false,
		&iAccepted
	) == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == (sizeof(Wire) - 1u)) &&
		(Context.Completes == 1u),
		"HTTP server Exchange retained descriptor storage");
	xrtHttp1ServerExchangeDestroy(pExchange);

	xrtHttp1ServerConfigInit((xhttp1serverconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP server config init accepted wrapping output");
	xrtClearError();
	testRequire((xrtHttp1ServerExchangeCreate(
		(const xhttp1serverconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server Exchange accepted wrapping config");
	xrtClearError();
	testRequire((xrtHttp1ServerExchangeCreate(
		NULL,
		(const xhttp1serverevents*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server Exchange accepted wrapping events");
	xrtClearError();
}



/* 验证定长正文缓冲、精确流水线边界和 Next 复用。 */
static void testHttpServerExchangeBufferedPipeline(void)
{
	static const char Wire[] =
		"POST /items HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"hello"
		"GET /next HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n";
	const char* pNext = strstr(Wire, "GET /next");
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_BUFFER
	};
	xhttp1serverexchange* pExchange =
		testHttpServerExchangeCreate(NULL, &Context);
	const xhttpserverrequest* pRequest;
	xbytesview Body;
	size_t iAccepted = 0;
	size_t iFirst = (size_t)(pNext - Wire);

	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE),
		"HTTP server buffered request did not complete");
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	Body = xrtHttpServerRequestBody(pRequest);
	testRequire(
		(iAccepted == iFirst) &&
		(xrtHttp1ServerExchangeWireBytes(pExchange) == iFirst) &&
		(Context.Headers == 1) &&
		(Context.Bodies == 0) &&
		(Context.Completes == 1) &&
		testHttpServerExchangeTextEqual(
			xrtHttpServerRequestMethod(pRequest), "POST"
		) &&
		testHttpServerExchangeTextEqual(
			xrtHttpServerRequestTarget(pRequest), "/items"
		) &&
		(Body.Size == 5) &&
		(memcmp(Body.Data, "hello", 5) == 0) &&
		(xrtHttpServerRequestBodyBytes(pRequest) == 5),
		"HTTP server buffered request state mismatch"
	);

	testRequire(xrtHttp1ServerExchangeNext(pExchange),
		"HTTP server keep-alive Next failed");
	iAccepted = 0;
	testRequire(xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)pNext,
			(sizeof(Wire) - 1u) - iFirst
		},
		false,
		&iAccepted
	) == XHTTP1_SERVER_FEED_COMPLETE,
		"HTTP server pipelined request did not complete");
	testRequire(
		(iAccepted == ((sizeof(Wire) - 1u) - iFirst)) &&
		(Context.Headers == 2) &&
		(Context.Completes == 2) &&
		testHttpServerExchangeTextEqual(
			xrtHttpServerRequestTarget(
				xrtHttp1ServerExchangeRequest(pExchange)
			),
			"/next"
		),
		"HTTP server pipelined request state mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证 chunked 流式交付、暂停恢复、Trailer 和后缀保留。 */
static void testHttpServerExchangeStreamPause(void)
{
	static const char Wire[] =
		"POST /stream HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"4\r\nWiki\r\n"
		"5\r\npedia\r\n"
		"0\r\n"
		"Digest: ok\r\n"
		"\r\n"
		"suffix";
	const char* pSuffix = strstr(Wire, "suffix");
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_STREAM
	};
	xhttp1serverexchange* pExchange;
	const xhttpserverrequest* pRequest;
	const xhttpfield* pTrailer;
	size_t iAccepted = 0;
	size_t iTotal = 0;

	Context.PauseFirst = true;
	pExchange = testHttpServerExchangeCreate(NULL, &Context);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_PAUSED),
		"HTTP server streamed request did not pause");
	testRequire((iAccepted != 0) &&
		(iAccepted < (size_t)(pSuffix - Wire)) &&
		xrtHttp1ServerExchangePaused(pExchange) &&
		(Context.Bodies == 1),
		"HTTP server streamed pause boundary mismatch");
	iTotal = iAccepted;

	testRequire(xrtHttp1ServerExchangeResume(pExchange),
		"HTTP server streamed request resume failed");
	iAccepted = 0;
	testRequire(xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)Wire + iTotal,
			(sizeof(Wire) - 1u) - iTotal
		},
		false,
		&iAccepted
	) == XHTTP1_SERVER_FEED_COMPLETE,
		"HTTP server streamed request did not complete");
	iTotal += iAccepted;
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	pTrailer = xrtHttpServerRequestTrailer(
		pRequest, XRT_STR_LITERAL("digest")
	);
	testRequire(
		(iTotal == (size_t)(pSuffix - Wire)) &&
		(Context.Headers == 1) &&
		(Context.Bodies == 2) &&
		(Context.Completes == 1) &&
		(Context.BodySize == 9) &&
		(memcmp(Context.Body, "Wikipedia", 9) == 0) &&
		(xrtHttpServerRequestBody(pRequest).Data == NULL) &&
		(xrtHttpServerRequestBodyBytes(pRequest) == 9) &&
		(xrtHttpServerRequestTrailerCount(pRequest) == 1) &&
		(pTrailer != NULL) &&
		testHttpServerExchangeTextEqual(
			pTrailer->Value, "ok"
		),
		"HTTP server streamed request data mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证固定长度最后片段可以暂停，并由空输入恢复请求完成事件。 */
static void testHttpServerExchangeFinalBodyPause(void)
{
	static const char Wire[] =
		"POST /last HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 4\r\n"
		"\r\n"
		"last"
		"suffix";
	const char* pSuffix = strstr(Wire, "suffix");
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_STREAM,
		.PauseFirst = true
	};
	xhttp1serverexchange* pExchange =
		testHttpServerExchangeCreate(NULL, &Context);
	const xhttpserverrequest* pRequest;
	size_t iAccepted = 0;

	testRequire(
		(pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		 ) == XHTTP1_SERVER_FEED_PAUSED),
		"HTTP server final body fragment did not pause"
	);
	testRequire(
		(iAccepted == (size_t)(pSuffix - Wire)) &&
		xrtHttp1ServerExchangePaused(pExchange) &&
		(Context.Bodies == 1) &&
		(Context.Completes == 0),
		"HTTP server final body pause boundary mismatch"
	);
	testRequire(
		xrtHttp1ServerExchangeResume(pExchange),
		"HTTP server final body fragment resume failed"
	);
	iAccepted = SIZE_MAX;
	testRequire(
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){ NULL, 0 },
			false,
			&iAccepted
		 ) == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == 0),
		"HTTP server final body fragment did not complete"
	);
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	testRequire(
		(Context.Completes == 1) &&
		(Context.BodySize == 4) &&
		(memcmp(Context.Body, "last", 4) == 0) &&
		(xrtHttpServerRequestBodyBytes(pRequest) == 4),
		"HTTP server final body fragment state mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证丢弃模式仍排空分帧、保留 Trailer，并停止正文回调。 */
static void testHttpServerExchangeDiscard(void)
{
	static const char Wire[] =
		"POST /discard HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Trailer: Digest\r\n"
		"\r\n"
		"5\r\nhello\r\n"
		"0\r\n"
		"Digest: ok\r\n"
		"\r\n"
		"GET /next HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n";
	const char* pNext = strstr(Wire, "GET /next");
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_DISCARD
	};
	xhttp1serverexchange* pExchange =
		testHttpServerExchangeCreate(NULL, &Context);
	const xhttpserverrequest* pRequest;
	const xhttpfield* pTrailer;
	uint32 iFlags;
	size_t iAccepted = 0;

	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		 ) == XHTTP1_SERVER_FEED_COMPLETE),
		"HTTP server discarded request did not complete");
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	pTrailer = xrtHttpServerRequestTrailer(
		pRequest, XRT_STR_LITERAL("digest")
	);
	iFlags = xrtHttpServerRequestFlags(pRequest);
	testRequire(
		(iAccepted == (size_t)(pNext - Wire)) &&
		(Context.Headers == 1) &&
		(Context.Bodies == 0) &&
		(Context.Completes == 1) &&
		((iFlags & XHTTP_SERVER_REQUEST_STREAMED) != 0) &&
		((iFlags & XHTTP_SERVER_REQUEST_DISCARDED) != 0) &&
		((iFlags & XHTTP_SERVER_REQUEST_COMPLETE) != 0) &&
		(xrtHttpServerRequestBody(pRequest).Data == NULL) &&
		(xrtHttpServerRequestBody(pRequest).Size == 0) &&
		(xrtHttpServerRequestBodyBytes(pRequest) == 5) &&
		(xrtHttpServerRequestTrailerCount(pRequest) == 1) &&
		(pTrailer != NULL) &&
		testHttpServerExchangeTextEqual(pTrailer->Value, "ok"),
		"HTTP server discarded request state mismatch"
	);
	testRequire(xrtHttp1ServerExchangeNext(pExchange),
		"HTTP server discarded request Next failed");
	iAccepted = 0;
	testRequire(
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)pNext,
				(sizeof(Wire) - 1u) -
				(size_t)(pNext - Wire)
			},
			false,
			&iAccepted
		 ) == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == ((sizeof(Wire) - 1u) -
		 (size_t)(pNext - Wire))) &&
		(Context.Headers == 2) &&
		(Context.Bodies == 0) &&
		(Context.Completes == 2) &&
		testHttpServerExchangeTextEqual(
			xrtHttpServerRequestTarget(
				xrtHttp1ServerExchangeRequest(pExchange)
			),
			"/next"
		) &&
		((xrtHttpServerRequestFlags(
			xrtHttp1ServerExchangeRequest(pExchange)
		 ) & XHTTP_SERVER_REQUEST_DISCARDED) == 0) &&
		(xrtHttpServerRequestBodyBytes(
			xrtHttp1ServerExchangeRequest(pExchange)
		 ) == 0),
		"HTTP server discarded request did not preserve reuse"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证 Header 级拒绝只接受完整请求头。 */
static void testHttpServerExchangeReject(void)
{
	static const char Wire[] =
		"POST /denied HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 4\r\n"
		"\r\n"
		"body";
	const char* pBody = strstr(Wire, "\r\n\r\n") + 4;
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_REJECT
	};
	xhttp1serverexchange* pExchange =
		testHttpServerExchangeCreate(NULL, &Context);
	size_t iAccepted = 0;

	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_REJECTED) &&
		(iAccepted == (size_t)(pBody - Wire)) &&
		(Context.Headers == 1) &&
		(Context.Bodies == 0) &&
		(Context.Completes == 0),
		"HTTP server rejected request consumed its body"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 要求一份完整输入以指定 Exchange 错误失败。 */
static void testHttpServerExchangeErrorCase(
	cstr sWire,
	xhttp1servererror Code
)
{
	xhttp1serverexchange* pExchange =
		xrtHttp1ServerExchangeCreate(NULL, NULL);
	const xerror* pError;
	size_t iAccepted = 0;
	xhttp1serverfeedstatus Status;

	Status = pExchange != NULL ?
		xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)sWire,
				strlen(sWire)
			},
			false,
			&iAccepted
		) : XHTTP1_SERVER_FEED_ERROR;
	if ( Status != XHTTP1_SERVER_FEED_ERROR ) {
		fprintf(
			stderr,
			"[exchange] expected error=%d status=%d wire=%s\n",
			(int)Code,
			(int)Status,
			sWire
		);
	}
	testRequire((pExchange != NULL) &&
		(Status == XHTTP1_SERVER_FEED_ERROR),
		"HTTP server invalid request was accepted");
	pError = xrtHttp1ServerExchangeError(pExchange);
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server.exchange"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		"HTTP server invalid request error mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 验证 HTTP/1.1 空 Host 字段与缺失 Host 保持不同语义。 */
static void testHttpServerExchangeEmptyHost(void)
{
	static const char Wire[] =
		"GET / HTTP/1.1\r\n"
		"Host:\r\n\r\n";
	xhttp1serverconfig Config;
	xhttp1serverexchange* pExchange;
	const xhttpserverrequest* pRequest;
	const xhttpfield* pHost;
	size_t iAccepted = 0;

	xrtHttp1ServerConfigInit(&Config);
	pExchange = xrtHttp1ServerExchangeCreate(&Config, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			XRT_BYTES_LITERAL(Wire),
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == (sizeof(Wire) - 1u)),
		"HTTP server rejected an empty Host field");
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	pHost = xrtHttpServerRequestHeader(
		pRequest, XRT_STR_LITERAL("Host")
	);
	testRequire((pHost != NULL) && (pHost->Value.Size == 0),
		"HTTP server lost the empty Host field value");
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证 Host、Expect 和请求分帧安全规则。 */
static void testHttpServerExchangeValidation(void)
{
	testHttpServerExchangeErrorCase(
		"GET * HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		XHTTP1_SERVER_ERROR_TARGET
	);
	testHttpServerExchangeErrorCase(
		"OPTIONS example.test/path HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		XHTTP1_SERVER_ERROR_TARGET
	);
	testHttpServerExchangeErrorCase(
		"CONNECT example.test HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		XHTTP1_SERVER_ERROR_TARGET
	);
	testHttpServerExchangeErrorCase(
		"GET / HTTP/1.1\r\n\r\n",
		XHTTP1_SERVER_ERROR_HOST
	);
	testHttpServerExchangeErrorCase(
		"GET / HTTP/1.1\r\n"
		"Host: a.test\r\n"
		"Host: b.test\r\n\r\n",
		XHTTP1_SERVER_ERROR_HOST
	);
	testHttpServerExchangeErrorCase(
		"GET / HTTP/1.1\r\n"
		"Host: bad host\r\n\r\n",
		XHTTP1_SERVER_ERROR_HOST
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Expect: custom\r\n"
		"Content-Length: 1\r\n\r\n",
		XHTTP1_SERVER_ERROR_EXPECTATION
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Expect: feature =on\r\n"
		"Content-Length: 1\r\n\r\n",
		XHTTP1_SERVER_ERROR_EXPECTATION
	);
	testHttpServerExchangeErrorCase(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Connection: TE\r\n"
		"TE: gzip;q=2\r\n\r\n",
		XHTTP1_SERVER_ERROR_TE
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Trailer: Digest\r\n"
		"Trailer: Content-Length\r\n"
		"Transfer-Encoding: chunked\r\n\r\n",
		XHTTP1_SERVER_ERROR_TRAILER
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Content-Length: 1\r\n\r\n",
		XHTTP1_SERVER_ERROR_FRAMING
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: gzip, chunked\r\n\r\n"
		"3\r\nabc\r\n0\r\n\r\n",
		XHTTP1_SERVER_ERROR_FRAMING
	);
	testHttpServerExchangeErrorCase(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n\r\n"
		"0\r\n"
		"Content-Length: 1\r\n\r\n",
		XHTTP1_SERVER_ERROR_TRAILER
	);
}



/* 验证高级服务器只在显式开启后交付未解码的扩展 Transfer Coding。 */
static void testHttpServerExchangeRawTransferCoding(void)
{
	static const char Wire[] =
		"POST /raw HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: gzip, chunked\r\n\r\n"
		"3\r\nabc\r\n0\r\n\r\n";
	xhttp1serverconfig Config;
	xhttp1serverexchange* pExchange;
	const xhttpserverrequest* pRequest;
	xbytesview Body;
	size_t iAccepted = 0;

	xrtHttp1ServerConfigInit(&Config);
	Config.AllowRawTransferCodings = true;
	pExchange = xrtHttp1ServerExchangeCreate(&Config, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			XRT_BYTES_LITERAL(Wire),
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE),
		"HTTP server raw Transfer Coding opt-in failed");
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	Body = xrtHttpServerRequestBody(pRequest);
	testRequire((iAccepted == (sizeof(Wire) - 1u)) &&
		(Body.Size == 3) &&
		(memcmp(Body.Data, "abc", 3) == 0),
		"HTTP server raw Transfer Coding body mismatch");
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证重复 Expect 标志以及 HTTP/1.0 忽略 Continue 的关闭语义。 */
static void testHttpServerExchangeVersionFacts(void)
{
	static const char RepeatedExpect[] =
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Expect: 100-continue\r\n"
		"Expect: , 100-CONTINUE,\r\n"
		"Content-Length: 1\r\n\r\nx";
	static const char EmptyExpect[] =
		"POST /empty HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Expect: 100-continue\r\n"
		"Content-Length: 0\r\n\r\n";
	static const char Http10Expect[] =
		"POST /old HTTP/1.0\r\n"
		"Expect: 100-continue\r\n"
		"Content-Length: 0\r\n\r\n";
	xhttp1serverexchange* pExchange =
		xrtHttp1ServerExchangeCreate(NULL, NULL);
	size_t iAccepted = 0;

	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)RepeatedExpect,
				sizeof(RepeatedExpect) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		((xrtHttpServerRequestFlags(
			xrtHttp1ServerExchangeRequest(pExchange)
		) & XHTTP_SERVER_REQUEST_EXPECT_CONTINUE) != 0),
		"HTTP server repeated Expect flag was not published");
	testRequire(xrtHttp1ServerExchangeNext(pExchange),
		"HTTP server repeated Expect request was not reusable");
	iAccepted = 0;
	testRequire((xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)EmptyExpect,
				sizeof(EmptyExpect) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		((xrtHttpServerRequestFlags(
			xrtHttp1ServerExchangeRequest(pExchange)
		) & XHTTP_SERVER_REQUEST_EXPECT_CONTINUE) == 0),
		"HTTP/1.1 empty request published a Continue handshake");
	testRequire(xrtHttp1ServerExchangeNext(pExchange),
		"HTTP server empty Expect request was not reusable");
	iAccepted = 0;
	testRequire((xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Http10Expect,
				sizeof(Http10Expect) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		((xrtHttpServerRequestFlags(
			xrtHttp1ServerExchangeRequest(pExchange)
		) & XHTTP_SERVER_REQUEST_EXPECT_CONTINUE) == 0),
		"HTTP/1.0 100-continue expectation was not ignored");
	testRequire(!xrtHttp1ServerExchangeNext(pExchange),
		"HTTP/1.0 closing request allowed Next");
	xrtClearError();
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证 TE 重复字段、逐跳声明和 HTTP 版本共同决定 Trailer 能力。 */
static void testHttpServerExchangeTeFacts(void)
{
	static const char Valid[] =
		"GET /trailers HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"TE: gzip;q=0.5\r\n"
		"TE: trailers\r\n"
		"Connection: keep-alive, te\r\n\r\n";
	static const char MissingConnection[] =
		"GET /raw-te HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"TE: trailers\r\n\r\n";
	static const char Http10[] =
		"GET /old HTTP/1.0\r\n"
		"TE: trailers\r\n"
		"Connection: TE\r\n\r\n";
	xhttp1serverexchange* pExchange;
	const xhttpserverrequest* pRequest;
	size_t iAccepted = 0;

	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			XRT_BYTES_LITERAL(Valid),
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE),
		"HTTP server valid TE request failed");
	pRequest = xrtHttp1ServerExchangeRequest(pExchange);
	testRequire(xrtHttpServerRequestAcceptsTrailers(
		pRequest
	) && ((xrtHttpServerRequestFlags(pRequest) &
		XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS) != 0),
		"HTTP server did not publish TE Trailer capability");
	xrtHttp1ServerExchangeDestroy(pExchange);

	iAccepted = 0;
	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			XRT_BYTES_LITERAL(MissingConnection),
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		!xrtHttpServerRequestAcceptsTrailers(
			xrtHttp1ServerExchangeRequest(pExchange)
		),
		"HTTP server trusted TE without Connection option");
	xrtHttp1ServerExchangeDestroy(pExchange);

	iAccepted = 0;
	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			XRT_BYTES_LITERAL(Http10),
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE) &&
		!xrtHttpServerRequestAcceptsTrailers(
			xrtHttp1ServerExchangeRequest(pExchange)
		),
		"HTTP/1.0 request published TE Trailer capability");
	xrtHttp1ServerExchangeDestroy(pExchange);
}



/* 验证默认正文限额可以被路由提高，也可以在 Header 后立即拒绝。 */
static void testHttpServerExchangeBodyLimits(void)
{
	static const char Wire[] =
		"POST /limit HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 4\r\n\r\n"
		"data";
	xhttp1serverconfig Config;
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_BUFFER
	};
	xhttp1serverexchange* pExchange;
	size_t iAccepted = 0;

	xrtHttp1ServerConfigInit(&Config);
	Config.Body.MaxBody = 3;
	Context.SetLimit = true;
	Context.Limit = 4;
	pExchange = testHttpServerExchangeCreate(
		&Config, &Context
	);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE),
		"HTTP server route body limit could not be raised");
	xrtHttp1ServerExchangeDestroy(pExchange);

	pExchange = xrtHttp1ServerExchangeCreate(
		&Config, NULL
	);
	iAccepted = 0;
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(xrtErrorCode(
			xrtHttp1ServerExchangeError(pExchange)
		) == XHTTP1_SERVER_ERROR_BODY_LIMIT),
		"HTTP server default body limit was not enforced");
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 验证可靠 EOF 区分空闲关闭和截断请求。 */
static void testHttpServerExchangeEnd(void)
{
	static const char Head[] =
		"GET / HTTP/1.1\r\nHost: example.test\r\n";
	static const char Body[] =
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 4\r\n\r\n"
		"ab";
	xhttp1serverexchange* pExchange;
	size_t iAccepted = 0;

	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){ NULL, 0 },
			true,
			&iAccepted
		) == XHTTP1_SERVER_FEED_CLOSED) &&
		(iAccepted == 0),
		"HTTP server clean idle EOF was not distinguished");
	xrtHttp1ServerExchangeDestroy(pExchange);

	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Head,
				sizeof(Head) - 1u
			},
			true,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(xrtErrorCode(
			xrtHttp1ServerExchangeError(pExchange)
		) == XHTTP1_SERVER_ERROR_UNEXPECTED_EOF),
		"HTTP server truncated Header was not rejected");
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();

	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	iAccepted = 0;
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Body,
				sizeof(Body) - 1u
			},
			true,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(xrtErrorCode(
			xrtHttp1ServerExchangeError(pExchange)
		) == XHTTP1_SERVER_ERROR_FRAMING),
		"HTTP server truncated body was not rejected");
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 验证应用回调错误保留为稳定原因链。 */
static void testHttpServerExchangeCallbackError(void)
{
	static const char Wire[] =
		"POST /sink HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 1\r\n\r\n"
		"x";
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_STREAM
	};
	xhttp1serverexchange* pExchange;
	const xerror* pError;
	const xerror* pCause;
	size_t iAccepted = 0;

	Context.FailBody = true;
	pExchange = testHttpServerExchangeCreate(NULL, &Context);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR),
		"HTTP server Body callback failure was ignored");
	pError = xrtHttp1ServerExchangeError(pExchange);
	pCause = xrtErrorCause(pError);
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) ==
		 XHTTP1_SERVER_ERROR_BODY_CALLBACK) &&
		(xrtErrorKind(pError) == XERR_IO) &&
		(pCause != NULL) &&
		(strcmp(
			xrtErrorDomain(pCause),
			"test.http.server.body"
		) == 0) &&
		(xrtErrorCode(pCause) == 77),
		"HTTP server Body callback cause chain mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 验证同一 Exchange 不能从事件回调中重入推进。 */
static void testHttpServerExchangeReentry(void)
{
	static const char Wire[] =
		"POST /reenter HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 1\r\n\r\n"
		"x";
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_STREAM
	};
	xhttp1serverexchange* pExchange;
	size_t iAccepted = 0;

	Context.Reenter = true;
	pExchange = testHttpServerExchangeCreate(NULL, &Context);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(xrtErrorCode(
			xrtHttp1ServerExchangeError(pExchange)
		) == XHTTP1_SERVER_ERROR_STATE),
		"HTTP server callback reentry did not fix a stable error"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 验证 Header、chunk 分帧和 Trailer 的每个两段切分点。 */
static void testHttpServerExchangeEverySplit(void)
{
	static const char Wire[] =
		"POST /split HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"X-Test: value\r\n"
		"\r\n"
		"3;name=value\r\n"
		"abc\r\n"
		"2\r\n"
		"de\r\n"
		"0\r\n"
		"Digest: ok\r\n"
		"\r\n";
	size_t iSize = sizeof(Wire) - 1u;
	size_t iSplit;

	for ( iSplit = 1; iSplit < iSize; iSplit++ ) {
		xhttp1serverexchange* pExchange =
			xrtHttp1ServerExchangeCreate(NULL, NULL);
		size_t iFirst = 0;
		size_t iSecond = 0;
		xhttp1serverfeedstatus Status;

		testRequire(pExchange != NULL,
			"HTTP server split Exchange create failed");
		Status = xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				iSplit
			},
			false,
			&iFirst
		);
		testRequire(
			(Status == XHTTP1_SERVER_FEED_MORE) &&
			(iFirst <= iSplit),
			"HTTP server first split state mismatch"
		);
		Status = xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire + iFirst,
				iSize - iFirst
			},
			false,
			&iSecond
		);
		testRequire(
			(Status == XHTTP1_SERVER_FEED_COMPLETE) &&
			((iFirst + iSecond) == iSize) &&
			(xrtHttpServerRequestBodyBytes(
				xrtHttp1ServerExchangeRequest(pExchange)
			) == 5),
			"HTTP server second split state mismatch"
		);
		xrtHttp1ServerExchangeDestroy(pExchange);
	}
}



/* 验证参数、状态和非法 Header 策略契约。 */
static void testHttpServerExchangeContracts(void)
{
	static const char Wire[] =
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 1\r\n\r\n"
		"x";
	test_http_server_exchange_context Context = {
		.Policy = XHTTP_SERVER_BODY_BUFFER
	};
	xhttp1serverexchange* pExchange;
	size_t iAccepted = 99;
	uint8 AcceptedStorage[sizeof(size_t) + 2u];
	size_t* pUnalignedAccepted =
		(size_t*)(void*)(AcceptedStorage + 1u);
	union {
		size_t Align;
		char Bytes[sizeof(Wire)];
	} Alias;

	testRequire(
		(xrtHttp1ServerExchangeFeed(
			NULL,
			(xbytesview){ NULL, 0 },
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(iAccepted == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server null Exchange contract mismatch"
	);
	xrtClearError();
	testRequire(!xrtHttp1ServerExchangePause(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server null Pause contract mismatch");
	xrtClearError();

	/* 非法范围和别名不能改写输入或推进 Exchange。 */
	pExchange = xrtHttp1ServerExchangeCreate(NULL, NULL);
	testRequire(pExchange != NULL,
		"HTTP server contract Exchange create failed");
	iAccepted = 99;
	testRequire(
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) && (iAccepted == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server accepted a wrapping Feed range"
	);
	xrtClearError();
	memcpy(Alias.Bytes, Wire, sizeof(Wire));
	testRequire(
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Alias.Bytes,
				sizeof(Wire) - 1u
			},
			false,
			(size_t*)(void*)Alias.Bytes
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(memcmp(Alias.Bytes, Wire, sizeof(Wire)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server Feed output alias modified its input"
	);
	xrtClearError();
	memset(AcceptedStorage, 0xA5, sizeof(AcceptedStorage));
	testRequire(
		xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			pUnalignedAccepted
		) == XHTTP1_SERVER_FEED_COMPLETE,
		"HTTP server rejected an unaligned Feed output"
	);
	memcpy(&iAccepted, pUnalignedAccepted, sizeof(iAccepted));
	testRequire(
		(iAccepted == (sizeof(Wire) - 1u)) &&
		(AcceptedStorage[0] == UINT8_C(0xA5)) &&
		(AcceptedStorage[sizeof(AcceptedStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server unaligned Feed output mismatch"
	);
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();

	Context.InvalidPolicy = true;
	pExchange = testHttpServerExchangeCreate(NULL, &Context);
	testRequire((pExchange != NULL) &&
		(xrtHttp1ServerExchangeFeed(
			pExchange,
			(xbytesview){
				(cbytes)Wire,
				sizeof(Wire) - 1u
			},
			false,
			&iAccepted
		) == XHTTP1_SERVER_FEED_ERROR) &&
		(xrtErrorCode(
			xrtHttp1ServerExchangeError(pExchange)
		) == XHTTP1_SERVER_ERROR_HEADERS_CALLBACK),
		"HTTP server invalid body policy was accepted");
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
}



/* 运行无 I/O HTTP/1 服务端 Exchange 测试。 */
int main(void)
{
	testHttpServerExchangeMemoryContracts();
	testHttpServerExchangeBufferedPipeline();
	testHttpServerExchangeStreamPause();
	testHttpServerExchangeFinalBodyPause();
	testHttpServerExchangeDiscard();
	testHttpServerExchangeReject();
	testHttpServerExchangeEmptyHost();
	testHttpServerExchangeValidation();
	testHttpServerExchangeRawTransferCoding();
	testHttpServerExchangeVersionFacts();
	testHttpServerExchangeTeFacts();
	testHttpServerExchangeBodyLimits();
	testHttpServerExchangeEnd();
	testHttpServerExchangeCallbackError();
	testHttpServerExchangeReentry();
	testHttpServerExchangeEverySplit();
	testHttpServerExchangeContracts();
	printf("[PASS] HTTP server Exchange\n");
	return 0;
}

