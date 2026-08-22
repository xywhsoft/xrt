#include "../test.h"



#if !defined(TEST_HTTP_DECOMPRESS_BACKEND)
	#define TEST_HTTP_DECOMPRESS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_DECOMPRESS_BACKEND_NAME "select"
#endif



static const uint8 TestHttpGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};

static const uint8 TestHttpDeflate[] = {
	0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0x63, 0x85, 0x08, 0xB2
};

static const uint8 TestHttpStacked[] = {
	0x78, 0x9C, 0xAB, 0x98, 0x73, 0xDA, 0xE3, 0xEC,
	0xC9, 0x93, 0xE1, 0x1E, 0xE7, 0xCE, 0xEB, 0x6A,
	0x78, 0xE9, 0xEA, 0xF9, 0xF9, 0x06, 0x6A, 0x9C,
	0xD7, 0x3F, 0xE5, 0xC9, 0xC8, 0x90, 0xDC, 0xCA,
	0xB1, 0x09, 0x00, 0xDC, 0x37, 0x0C, 0x85
};

static const uint8 TestHttpGzipLimit[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0x73, 0x74, 0x1C, 0x58, 0x00, 0x00,
	0xDE, 0x8A, 0x18, 0x04, 0x80, 0x00, 0x00, 0x00
};

static const uint8 TestHttpGzipLarge[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xED, 0xC1, 0x31, 0x01, 0x00, 0x00,
	0x00, 0xC2, 0xA0, 0x6C, 0xEB, 0x5F, 0xCA, 0x12,
	0x9E, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6F, 0x03,
	0xB8, 0x86, 0xC5, 0xAB, 0x70, 0x11, 0x01, 0x00
};

static const char TestHttpPlain[] = "hello compressed world";

#define TEST_HTTP_LARGE_BODY_SIZE 70000u



typedef enum test_http_decompress_scenario {
	TEST_HTTP_DECOMPRESS_GZIP = 0,
	TEST_HTTP_DECOMPRESS_DEFLATE,
	TEST_HTTP_DECOMPRESS_STACKED,
	TEST_HTTP_DECOMPRESS_LIST,
	TEST_HTTP_DECOMPRESS_RAW,
	TEST_HTTP_DECOMPRESS_UNKNOWN,
	TEST_HTTP_DECOMPRESS_BAD,
	TEST_HTTP_DECOMPRESS_MALFORMED,
	TEST_HTTP_DECOMPRESS_LIMIT,
	TEST_HTTP_DECOMPRESS_NESTING,
	TEST_HTTP_DECOMPRESS_STREAM,
	TEST_HTTP_DECOMPRESS_STREAM_LARGE,
	TEST_HTTP_DECOMPRESS_CUSTOM_ACCEPT,
	TEST_HTTP_DECOMPRESS_HEAD
} test_http_decompress_scenario;



/* 每个场景独占网络对象，避免异步关闭时序污染下一项断言。 */
typedef struct test_http_decompress {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_http_decompress_scenario Scenario;
	char StreamBody[64];
	size_t StreamBodySize;
	size_t HeaderCalls;
	bool RequestEncoding;
	bool RequestCustomEncoding;
	bool ResponseSent;
} test_http_decompress;



/* 在十秒截止时间内等待 Worker 发布一个终态标志。 */
static void testHttpDecompressWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 测试域名固定解析到本机 IPv4。 */
static xnetaddrlist* testHttpDecompressLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "compress.test") == 0,
		"HTTP decompression resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP decompression resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 记录服务端连接底层资源已经关闭。 */
static void testHttpDecompressServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 根据场景返回线路正文、编码字段和长度。 */
static void testHttpDecompressResponse(
	test_http_decompress* pState,
	const uint8** ppBody,
	size_t* pBodySize,
	cstr* psEncoding
)
{
	*ppBody = TestHttpGzip;
	*pBodySize = sizeof(TestHttpGzip);
	*psEncoding = "gzip";
	if ( pState->Scenario == TEST_HTTP_DECOMPRESS_DEFLATE ) {
		*ppBody = TestHttpDeflate;
		*pBodySize = sizeof(TestHttpDeflate);
		*psEncoding = "deflate";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_STACKED
	) {
		*ppBody = TestHttpStacked;
		*pBodySize = sizeof(TestHttpStacked);
		*psEncoding = "deflate, deflate";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_LIST
	) {
		*ppBody = TestHttpStacked;
		*pBodySize = sizeof(TestHttpStacked);
		*psEncoding =
			", deflate,\r\n"
			"Content-Encoding: identity, deflate,";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_UNKNOWN
	) {
		*ppBody = (const uint8*)"raw-br";
		*pBodySize = 6;
		*psEncoding = "br";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_LIMIT
	) {
		*ppBody = TestHttpGzipLimit;
		*pBodySize = sizeof(TestHttpGzipLimit);
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_BAD
	) {
		static uint8 Bad[sizeof(TestHttpGzip)];

		memcpy(Bad, TestHttpGzip, sizeof(Bad));
		Bad[sizeof(Bad) - 8u] ^= 1u;
		*ppBody = Bad;
		*pBodySize = sizeof(Bad);
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_MALFORMED
	) {
		*psEncoding = "gzip;q=1";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_NESTING
	) {
		*ppBody = TestHttpStacked;
		*pBodySize = sizeof(TestHttpStacked);
		*psEncoding = "deflate, deflate";
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_STREAM_LARGE
	) {
		*ppBody = TestHttpGzipLarge;
		*pBodySize = sizeof(TestHttpGzipLarge);
	}
}



/* 收到完整请求后检查自动协商 Header 并发送二进制响应。 */
static void testHttpDecompressServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;
	const uint8* pBody;
	size_t iBodySize;
	cstr sEncoding;
	char Request[2048];
	char Head[256];
	size_t iSize = xrtNetBufSize(pBuffer);
	int iHeadSize;
	bool bHead =
		pState->Scenario == TEST_HTTP_DECOMPRESS_HEAD;

	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTP decompression request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP decompression request peek failed"
	);
	Request[iSize] = '\0';
	if ( strstr(Request, "\r\n\r\n") == NULL ) {
		return;
	}
	testRequire(
		!pState->ResponseSent,
		"HTTP decompression fixture sent duplicate response"
	);
	pState->ResponseSent = true;
	pState->RequestEncoding =
		strstr(
			Request,
			"\r\nAccept-Encoding: gzip, deflate\r\n"
		) != NULL;
	pState->RequestCustomEncoding =
		strstr(
			Request,
			"\r\nAccept-Encoding: identity\r\n"
		) != NULL;
	testRequire(
		xrtNetBufConsume(pBuffer, iSize) == iSize,
		"HTTP decompression request consume failed"
	);
	testHttpDecompressResponse(
		pState,
		&pBody,
		&iBodySize,
		&sEncoding
	);
	iHeadSize = snprintf(
		Head,
		sizeof(Head),
		"HTTP/1.1 200 OK\r\n"
		"Content-Encoding: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n",
		sEncoding,
		iBodySize
	);
	testRequire(
		(iHeadSize > 0) &&
		((size_t)iHeadSize < sizeof(Head)) &&
		(xrtNetStreamSend(
			pStream,
			Head,
			(size_t)iHeadSize
		) == XNET_RESULT_OK),
		"HTTP decompression response Head send failed"
	);
	if ( !bHead ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				pBody,
				iBodySize
			) == XNET_RESULT_OK,
			"HTTP decompression response body send failed"
		);
	}
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP decompression response close failed"
	);
}



/* 接管唯一服务端连接并安装读取回调。 */
static bool testHttpDecompressAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;
	xnetstreamevents Events;

	(void)pListener;
	testRequire(
		pState->Server == NULL,
		"HTTP decompression accepted duplicate connection"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpDecompressServerRead;
	Events.Close = testHttpDecompressServerClose;
	xrtNetStreamSetEvents(pStream, &Events, pState);
	pState->Server = xrtNetStreamRef(pStream);
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录监听器已经关闭。 */
static void testHttpDecompressListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 流式 Header 回调必须看到已经改写的表示元数据。 */
static bool testHttpDecompressHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;
	xstrview Encoding = xrtHttpResponseOriginalEncoding(
		pResponse
	);

	(void)pCall;
	testRequire(
		(xrtHttpResponseFlags(pResponse) &
		 XHTTP_RESPONSE_DECOMPRESSED) != 0 &&
		(xrtHttpResponseHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Encoding")
		) == NULL) &&
		(xrtHttpResponseHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Length")
		) == NULL) &&
		(Encoding.Size == 4) &&
		(memcmp(Encoding.Data, "gzip", 4) == 0),
		"HTTP streaming decoded Header metadata mismatch"
	);
	pState->HeaderCalls++;
	return true;
}



/* 流式正文回调拼接解码后的字节。 */
static bool testHttpDecompressBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;
	size_t i;

	(void)pCall;
	testRequire(
		xrtHttpResponseBodyBytes(pResponse) ==
			(uint64)pState->StreamBodySize,
		"HTTP decoded stream exposed a wire byte count"
	);
	if ( pState->Scenario ==
		TEST_HTTP_DECOMPRESS_STREAM_LARGE ) {
		for ( i = 0; i < Data.Size; i++ ) {
			testRequire(
				Data.Data[i] == (uint8)'A',
				"HTTP large decoded stream data mismatch"
			);
		}
		pState->StreamBodySize += Data.Size;
		return true;
	}
	testRequire(
		Data.Size <=
			(sizeof(pState->StreamBody) -
			 pState->StreamBodySize),
		"HTTP decoded stream overflowed fixture"
	);
	memcpy(
		pState->StreamBody + pState->StreamBodySize,
		Data.Data,
		Data.Size
	);
	pState->StreamBodySize += Data.Size;
	return true;
}



/* 验证成功响应的正文、线路计数和原始编码元数据。 */
static void testHttpDecompressSuccess(
	test_http_decompress* pState,
	const xhttpcallresult* pResult
)
{
	xbytesview Body;
	xstrview Encoding;
	cstr sExpectedEncoding = "gzip";
	size_t iExpectedEncoding = 4;
	size_t iExpectedBody = sizeof(TestHttpPlain) - 1u;
	size_t iExpectedWire = sizeof(TestHttpGzip);

	if ( (pResult->Result != XNET_RESULT_OK) ||
		(pResult->Response == NULL) ) {
		const xerror* pError = pResult->Error;

		fprintf(
			stderr,
			"[INFO] HTTP decompression scenario %d failed",
			(int)pState->Scenario
		);
		while ( pError != NULL ) {
			fprintf(
				stderr,
				": %s/%d %s",
				xrtErrorDomain(pError),
				(int)xrtErrorCode(pError),
				xrtErrorMessage(pError)
			);
			pError = xrtErrorCause(pError);
		}
		fputc('\n', stderr);
	}
	testRequire(
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL),
		"HTTP automatic decompression did not succeed"
	);
	Body = xrtHttpResponseBody(pResult->Response);
	Encoding = xrtHttpResponseOriginalEncoding(
		pResult->Response
	);
	if ( pState->Scenario == TEST_HTTP_DECOMPRESS_DEFLATE ) {
		sExpectedEncoding = "deflate";
		iExpectedEncoding = 7;
		iExpectedWire = sizeof(TestHttpDeflate);
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_STACKED
	) {
		sExpectedEncoding = "deflate, deflate";
		iExpectedEncoding = 16;
		iExpectedWire = sizeof(TestHttpStacked);
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_LIST
	) {
		sExpectedEncoding =
			", deflate,, identity, deflate,";
		iExpectedEncoding = 30;
		iExpectedWire = sizeof(TestHttpStacked);
	} else if (
		pState->Scenario ==
			TEST_HTTP_DECOMPRESS_STREAM_LARGE
	) {
		iExpectedBody = TEST_HTTP_LARGE_BODY_SIZE;
		iExpectedWire = sizeof(TestHttpGzipLarge);
	}
	testRequire(
		((xrtHttpResponseFlags(pResult->Response) &
		  XHTTP_RESPONSE_DECOMPRESSED) != 0) &&
		(xrtHttpResponseHeader(
			pResult->Response,
			XRT_STR_LITERAL("Content-Encoding")
		) == NULL) &&
		(xrtHttpResponseHeader(
			pResult->Response,
			XRT_STR_LITERAL("Content-Length")
		) == NULL) &&
		(Encoding.Size == iExpectedEncoding) &&
		(memcmp(
			Encoding.Data,
			sExpectedEncoding,
			Encoding.Size
		) == 0) &&
		(xrtHttpResponseBodyBytes(pResult->Response) ==
		 (uint64)iExpectedBody) &&
		(xrtHttpResponseWireBodyBytes(pResult->Response) ==
		 (uint64)iExpectedWire) &&
		(pResult->Info.ResponseBodyBytes ==
		 (uint64)iExpectedBody),
		"HTTP automatic decompression metadata mismatch"
	);
	if ( (pState->Scenario == TEST_HTTP_DECOMPRESS_STREAM) ||
		(pState->Scenario ==
		 TEST_HTTP_DECOMPRESS_STREAM_LARGE) ) {
		testRequire(
			(Body.Data == NULL) &&
			(Body.Size == 0) &&
			(pState->HeaderCalls == 1) &&
			(pState->StreamBodySize == iExpectedBody),
			"HTTP streamed decompression result mismatch"
		);
		if ( pState->Scenario ==
			TEST_HTTP_DECOMPRESS_STREAM ) {
			testRequire(
				memcmp(
					pState->StreamBody,
					TestHttpPlain,
					pState->StreamBodySize
				) == 0,
				"HTTP streamed decompression body mismatch"
			);
		}
	} else {
		testRequire(
			(Body.Size == iExpectedBody) &&
			(memcmp(
				Body.Data,
				TestHttpPlain,
				Body.Size
			) == 0),
			"HTTP buffered decompression body mismatch"
		);
	}
}



/* 验证成功、raw、未知编码和损坏数据终态。 */
static void testHttpDecompressDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_decompress* pState =
		(test_http_decompress*)pData;

	testRequire(
		pCall == pState->Call,
		"HTTP decompression callback Call mismatch"
	);
	if ( (pState->Scenario == TEST_HTTP_DECOMPRESS_BAD) ||
		(pState->Scenario == TEST_HTTP_DECOMPRESS_MALFORMED) ||
		(pState->Scenario == TEST_HTTP_DECOMPRESS_LIMIT) ||
		(pState->Scenario == TEST_HTTP_DECOMPRESS_NESTING) ) {
		testRequire(
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorCode(pResult->Error) ==
			 XHTTP_CLIENT_ERROR_DECOMPRESSION) &&
			(((pState->Scenario !=
			   TEST_HTTP_DECOMPRESS_LIMIT) &&
			  (pState->Scenario !=
			   TEST_HTTP_DECOMPRESS_NESTING)) ||
			 (xrtErrorKind(pResult->Error) ==
			  XERR_RANGE)),
			"HTTP decompression failure result mismatch"
		);
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_HEAD
	) {
		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(xrtHttpResponseBody(
				pResult->Response
			).Size == 0) &&
			(xrtHttpResponseWireBodyBytes(
				pResult->Response
			) == 0) &&
			((xrtHttpResponseFlags(
				pResult->Response
			) & XHTTP_RESPONSE_DECOMPRESSED) == 0) &&
			(xrtHttpResponseHeader(
				pResult->Response,
				XRT_STR_LITERAL("Content-Encoding")
			) != NULL) &&
			(xrtHttpResponseOriginalEncoding(
				pResult->Response
			).Data == NULL),
			"HTTP HEAD response was incorrectly decoded"
		);
		pState->Response = pResult->Response;
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_RAW
	) {
		xbytesview Body;

		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL),
			"HTTP raw response did not succeed"
		);
		Body = xrtHttpResponseBody(pResult->Response);
		testRequire(
			(Body.Size == sizeof(TestHttpGzip)) &&
			(memcmp(
				Body.Data,
				TestHttpGzip,
				Body.Size
			) == 0) &&
			((xrtHttpResponseFlags(
				pResult->Response
			) & XHTTP_RESPONSE_DECOMPRESSED) == 0) &&
			(xrtHttpResponseHeader(
				pResult->Response,
				XRT_STR_LITERAL("Content-Encoding")
			) != NULL) &&
			(xrtHttpResponseOriginalEncoding(
				pResult->Response
			).Data == NULL),
			"HTTP raw response mode mismatch"
		);
		pState->Response = pResult->Response;
	} else if (
		pState->Scenario == TEST_HTTP_DECOMPRESS_UNKNOWN
	) {
		xbytesview Body;

		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL),
			"HTTP unknown coding response did not succeed"
		);
		Body = xrtHttpResponseBody(pResult->Response);
		testRequire(
			(Body.Size == 6) &&
			(memcmp(Body.Data, "raw-br", 6) == 0) &&
			((xrtHttpResponseFlags(
				pResult->Response
			) & XHTTP_RESPONSE_DECOMPRESSED) == 0) &&
			(xrtHttpResponseHeader(
				pResult->Response,
				XRT_STR_LITERAL("Content-Encoding")
			) != NULL),
			"HTTP unknown Content-Encoding was not preserved"
		);
		pState->Response = pResult->Response;
	} else {
		testHttpDecompressSuccess(pState, pResult);
		pState->Response = pResult->Response;
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 运行一项真实 TCP HTTP 自动解码场景。 */
static void testHttpDecompressRun(
	test_http_decompress_scenario Scenario
)
{
	test_http_decompress State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xnetaddr Address;
	char Url[128];
	int iUrlSize;

	memset(&State, 0, sizeof(State));
	memset(&ListenEvents, 0, sizeof(ListenEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	State.Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_DECOMPRESS_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP decompression engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP decompression listen address failed"
	);
	ListenEvents.Accept = testHttpDecompressAccept;
	ListenEvents.Close =
		testHttpDecompressListenerClose;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenEvents,
		NULL,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTP decompression listener failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpDecompressLookup;
	ClientConfig.Dial.FallbackDelay = 1000;
	ClientConfig.Dial.MaxAttempts = 1;
	if ( Scenario == TEST_HTTP_DECOMPRESS_LIMIT ) {
		ClientConfig.Decompress.MaxBody = 64;
	} else if (
		Scenario == TEST_HTTP_DECOMPRESS_NESTING
	) {
		ClientConfig.Decompress.MaxCodings = 1;
	}
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTP decompression client create failed"
	);
	iUrlSize = snprintf(
		Url,
		sizeof(Url),
		"http://compress.test:%u/data",
		(unsigned int)Address.Port
	);
	testRequire(
		(iUrlSize > 0) &&
		((size_t)iUrlSize < sizeof(Url)),
		"HTTP decompression URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		Scenario == TEST_HTTP_DECOMPRESS_HEAD ?
			XRT_STR_LITERAL("HEAD") :
			XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iUrlSize }
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression request create failed"
	);
	if ( Scenario == TEST_HTTP_DECOMPRESS_CUSTOM_ACCEPT ) {
		testRequire(
			xrtHttpRequestAddHeader(
				pRequest,
				XRT_STR_LITERAL("Accept-Encoding"),
				XRT_STR_LITERAL("identity")
			),
			"HTTP custom Accept-Encoding setup failed"
		);
	}
	xrtHttpCallOptionsInit(&Options);
	if ( Scenario == TEST_HTTP_DECOMPRESS_RAW ) {
		Options.Decompress = XHTTP_DECOMPRESS_RAW;
	} else if (
		(Scenario == TEST_HTTP_DECOMPRESS_STREAM) ||
		(Scenario == TEST_HTTP_DECOMPRESS_STREAM_LARGE)
	) {
	Options.Events.Headers =
		testHttpDecompressHeaders;
	Options.Events.Body = testHttpDecompressBody;
	Options.Events.Data = &State;
	}
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		&Options,
		testHttpDecompressDone,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP decompression call submission failed"
	);
	testHttpDecompressWait(
		&State.Accepted,
		"HTTP decompression connection was not accepted"
	);
	testHttpDecompressWait(
		&State.Completed,
		"HTTP decompression call did not complete"
	);
	testRequire(
		State.RequestEncoding ==
			((Scenario != TEST_HTTP_DECOMPRESS_RAW) &&
			 (Scenario !=
			  TEST_HTTP_DECOMPRESS_CUSTOM_ACCEPT)),
		"HTTP automatic Accept-Encoding policy mismatch"
	);
	testRequire(
		State.RequestCustomEncoding ==
			(Scenario ==
			 TEST_HTTP_DECOMPRESS_CUSTOM_ACCEPT),
		"HTTP custom Accept-Encoding policy mismatch"
	);
	testHttpDecompressWait(
		&State.ServerClosed,
		"HTTP decompression server stream did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP decompression listener close failed"
	);
	testHttpDecompressWait(
		&State.ListenerClosed,
		"HTTP decompression listener did not close"
	);

	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP decompression engine destroy failed"
	);
}



/* 验证自动解压配置支持未对齐存储并拒绝回绕地址。 */
static void testHttpDecompressConfig(void)
{
	static uint8 Storage[sizeof(xhttpdecompressconfig) + 1u];
	xhttpdecompressconfig Config;
	xhttpdecompressconfig* pUnaligned =
		(xhttpdecompressconfig*)(Storage + 1u);
	const xerror* pError;

	xrtHttpDecompressConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		Config.Enabled &&
		(Config.MaxBody == XHTTP_DECOMPRESS_BODY_DEFAULT) &&
		(Config.MaxCodings ==
		 XHTTP_DECOMPRESS_CODINGS_DEFAULT),
		"HTTP decompression unaligned config init mismatch"
	);
	xrtHttpDecompressConfigInit(
		(xhttpdecompressconfig*)(UINTPTR_MAX - 1u)
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP decompression wrapping config init mismatch"
	);
	xrtClearError();
}



/* 覆盖旧版解码资产、现代 raw 逃生路径和流式交付。 */
int main(void)
{
	testHttpDecompressConfig();
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_GZIP);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_DEFLATE);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_STACKED);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_LIST);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_RAW);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_UNKNOWN);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_BAD);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_MALFORMED);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_LIMIT);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_NESTING);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_STREAM);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_STREAM_LARGE);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_CUSTOM_ACCEPT);
	testHttpDecompressRun(TEST_HTTP_DECOMPRESS_HEAD);
	printf(
		"[PASS] HTTP automatic decompression (%s)\n",
		TEST_HTTP_DECOMPRESS_BACKEND_NAME
	);
	return 0;
}
