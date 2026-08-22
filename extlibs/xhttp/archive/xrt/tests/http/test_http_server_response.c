#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 自定义正文来源用于 AGAIN、未知长度和长度失败测试。 */
typedef struct test_http_server_response_body {
	const unsigned char* Data;
	size_t Size;
	size_t Offset;
	bool Again;
} test_http_server_response_body;



/* 自定义正文 Chunk 不拥有静态测试数据。 */
static void testHttpServerResponseBodyRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 返回一次 AGAIN 后按 MaxBytes 发布静态测试数据。 */
static xhttpbodystatus testHttpServerResponseBodyNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_server_response_body* pBody =
		(test_http_server_response_body*)pContext;
	size_t iTake;

	if ( pBody->Again ) {
		pBody->Again = false;
		return XHTTP_BODY_AGAIN;
	}
	if ( pBody->Offset == pBody->Size ) {
		return XHTTP_BODY_EOF;
	}
	iTake = pBody->Size - pBody->Offset;
	if ( iTake > iMaxBytes ) {
		iTake = iMaxBytes;
	}
	pChunk->Data = pBody->Data + pBody->Offset;
	pChunk->Size = iTake;
	pChunk->Release = testHttpServerResponseBodyRelease;
	pChunk->Context = NULL;
	pBody->Offset += iTake;
	return XHTTP_BODY_DATA;
}



/* 打开只允许单次读取的测试正文。 */
static bool testHttpServerResponseBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_server_response_body* pBody =
		(test_http_server_response_body*)pFactory;

	pBody->Offset = 0;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpServerResponseBodyNext;
	*ppReader = pBody;
	return true;
}



/* 创建声明指定长度的自定义测试正文。 */
static xhttpbody* testHttpServerResponseBodyCreate(
	test_http_server_response_body* pBody,
	uint64 iLength
)
{
	xhttpbodyops Ops = {
		testHttpServerResponseBodyOpen,
		NULL
	};

	return xrtHttpBodyCreate(
		&Ops, pBody, iLength, XHTTP_BODY_NONE
	);
}



/* 从完整 Header 创建拥有型请求事实。 */
static xhttpserverrequest* testHttpServerResponseRequest(
	cstr sWire,
	uint32 iFlags
)
{
	xhttpfield Fields[16];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)sWire,
			strlen(sWire)
		},
		&Head,
		NULL,
		NULL
	) == XHTTP1_READY,
		"HTTP server response request fixture parse failed");
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP server response request plan failed");
	return __xrtHttpServerRequestCreate(
		&Head, &Plan, iFlags
	);
}



/* 以短写方式收集完整响应，并验证未消费 Offer 保持稳定。 */
static xhttp1serveroutputstatus testHttpServerResponseCollect(
	xhttp1serverresponse* pResponse,
	unsigned char* pOutput,
	size_t iCapacity,
	size_t* pSize,
	size_t* pAgain
)
{
	xhttp1serveroutputstatus Status =
		XHTTP1_SERVER_OUTPUT_ERROR;
	size_t iOutput = 0;
	size_t iGuard = 0;
	size_t iAgain = 0;
	bool bReoffered = false;

	while ( iGuard++ < 8192 ) {
		xbytesview Data;

		Status = xrtHttp1ServerResponseOutput(
			pResponse, 64, &Data
		);
		if ( Status == XHTTP1_SERVER_OUTPUT_AGAIN ) {
			iAgain++;
			continue;
		}
		if ( Status != XHTTP1_SERVER_OUTPUT_DATA ) {
			break;
		}
		testRequire(
			(Data.Data != NULL) &&
			(Data.Size != 0) &&
			(Data.Size <= (iCapacity - iOutput)),
			"HTTP server response published invalid output"
		);
		if ( !bReoffered ) {
			xbytesview Again;

			testRequire(xrtHttp1ServerResponseOutput(
				pResponse, 1, &Again
			) == XHTTP1_SERVER_OUTPUT_DATA,
				"HTTP server response repeated Output failed");
			testRequire(
				(Again.Data == Data.Data) &&
				(Again.Size == Data.Size),
				"HTTP server response changed unconsumed Offer"
			);
			bReoffered = true;
		}
		memcpy(pOutput + iOutput, Data.Data, 1);
		iOutput++;
		testRequire(xrtHttp1ServerResponseOutputConsume(
			pResponse, 1
		), "HTTP server response short consume failed");
	}
	testRequire(iGuard < 8192,
		"HTTP server response output made no bounded progress");
	*pSize = iOutput;
	if ( pAgain != NULL ) {
		*pAgain = iAgain;
	}
	return Status;
}



/* 比较收集线路与精确预期文本。 */
static void testHttpServerResponseExpect(
	xhttp1serverresponse* pResponse,
	cstr sExpected,
	xhttp1serveroutputstatus ExpectedStatus,
	size_t* pAgain
)
{
	unsigned char Output[4096];
	size_t iSize = 0;
	size_t iExpected = strlen(sExpected);
	xhttp1serveroutputstatus Status =
		testHttpServerResponseCollect(
			pResponse,
			Output,
			sizeof(Output),
			&iSize,
			pAgain
		);

	if ( (Status != ExpectedStatus) ||
		(iSize != iExpected) ||
		(memcmp(
			Output,
			sExpected,
			iSize < iExpected ? iSize : iExpected
		) != 0) ) {
		fprintf(
			stderr,
			"[response] status=%d expected=%d size=%zu expected-size=%zu\n"
			"%.*s\n",
			(int)Status,
			(int)ExpectedStatus,
			iSize,
			iExpected,
			(int)iSize,
			(cstr)Output
		);
	}
	testRequire(
		(Status == ExpectedStatus) &&
		(iSize == iExpected) &&
		(memcmp(Output, sExpected, iExpected) == 0) &&
		(xrtHttp1ServerResponseWireBytes(pResponse) ==
		 (uint64)iExpected) &&
		xrtHttp1ServerResponseComplete(pResponse),
		"HTTP server response wire mismatch"
	);
}



/* 验证 100、103 与信息响应禁止的正文分帧边界。 */
static void testHttpServerResponseInform(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(103);
	xhttp1serverresponse* pResponse;

	testRequire((pReply != NULL) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Link"),
			XRT_STR_LITERAL("</style.css>; rel=preload")
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Link"),
			XRT_STR_LITERAL("</app.js>; rel=preload")
		), "HTTP server Early Hints setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse != NULL) &&
		xrtHttp1ServerResponseInformational(pResponse) &&
		!xrtHttp1ServerResponseClose(pResponse) &&
		!xrtHttp1ServerResponseTunnel(pResponse),
		"HTTP server Early Hints facts mismatch");
	xrtHttpReplyDestroy(pReply);
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 103 Early Hints\r\n"
		"Link: </style.css>; rel=preload\r\n"
		"Link: </app.js>; rel=preload\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);

	pReply = xrtHttpReplyCreate(100);
	testRequire(pReply != NULL,
		"HTTP server Continue setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server Continue prepare failed");
	xrtHttpReplyDestroy(pReply);
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 100 Continue\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);

	pReply = xrtHttpReplyCreate(103);
	testRequire(pReply != NULL,
		"HTTP server invalid information setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_0, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_STATUS),
		"HTTP/1.0 information was accepted");
	xrtClearError();
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Content-Length"),
		XRT_STR_LITERAL("0")
	), "HTTP information Content-Length setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_FRAMING),
		"HTTP information Content-Length was accepted");
	xrtClearError();
	testRequire(
		(xrtHttpReplyRemoveHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length")
		) == 1) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		), "HTTP information Transfer-Encoding setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_FRAMING),
		"HTTP information Transfer-Encoding was accepted");
	xrtClearError();
	testRequire(
		(xrtHttpReplyRemoveHeader(
			pReply,
			XRT_STR_LITERAL("Transfer-Encoding")
		) == 1) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("X-Meta")
		), "HTTP information Trailer declaration setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_FRAMING),
		"HTTP information Trailer declaration was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(103);
	testRequire((pReply != NULL) &&
		xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("X-Meta"),
			XRT_STR_LITERAL("done")
		), "HTTP information Trailer setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_TRAILER),
		"HTTP information Trailer was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(103);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("body"),
			(xstrview){ NULL, 0 }
		), "HTTP information body setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_BODY),
		"HTTP information body was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(101);
	testRequire(pReply != NULL,
		"HTTP information 101 setup failed");
	pResponse = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_STATUS),
		"HTTP information accepted Upgrade status");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
}



/* 验证固定正文、Reply 冻结和短写输出。 */
static void testHttpServerResponseFixed(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerResponseRequest(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttp1serverresponse* pResponse;

	testRequire((pRequest != NULL) && (pReply != NULL) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("X-Test"),
			XRT_STR_LITERAL("value")
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("")
		) && xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("hello"),
			(xstrview){ NULL, 0 }
		), "HTTP server fixed Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server fixed response create failed");
	testRequire(xrtHttpReplySetStatus(pReply, 500),
		"HTTP server fixed Reply mutation failed");
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);

	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 200 OK\r\n"
		"X-Test: value\r\n"
		"Connection: \r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"hello",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	testRequire(!xrtHttp1ServerResponseClose(pResponse) &&
		!xrtHttp1ServerResponseTunnel(pResponse),
		"HTTP server fixed response connection facts mismatch");
	xrtHttp1ServerResponseDestroy(pResponse);
}



/* 验证 chunked 正文与拥有型 Trailer 一次写完。 */
static void testHttpServerResponseChunkedTrailers(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerResponseRequest(
			"GET /trailers HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttp1serverresponse* pResponse;

	testRequire((pRequest != NULL) && (pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("hello"),
			(xstrview){ NULL, 0 }
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		) && xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("ok")
		), "HTTP server chunked Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server chunked response create failed");
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Trailer: Digest\r\n"
		"\r\n"
		"5\r\n"
		"hello\r\n"
		"0\r\n"
		"Digest: ok\r\n"
		"\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
}



/* 验证无正文状态、HEAD 元数据和 205 规范化。 */
static void testHttpServerResponseNoContent(void)
{
	xhttpserverrequest* pGet =
		testHttpServerResponseRequest(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpserverrequest* pHead =
		testHttpServerResponseRequest(
			"HEAD / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(204);
	xhttp1serverresponse* pResponse;

	testRequire((pGet != NULL) && (pHead != NULL) &&
		(pReply != NULL) && xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("body"),
			(xstrview){ NULL, 0 }
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("4")
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		), "HTTP server 204 Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(pGet, pReply);
	testRequire(pResponse != NULL,
		"HTTP server 204 response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 204 No Content\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(200);
	testRequire((pReply != NULL) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("7")
		), "HTTP server HEAD Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pHead, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server HEAD metadata response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 7\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(205);
	testRequire(pReply != NULL,
		"HTTP server 205 Reply create failed");
	pResponse = xrtHttp1ServerResponseCreate(pGet, pReply);
	testRequire(pResponse != NULL,
		"HTTP server 205 response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 205 Reset Content\r\n"
		"Content-Length: 0\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pHead);
	xrtHttpServerRequestDestroy(pGet);
}



/* 验证 HTTP/1.0 close-delimited 与显式 keep-alive 响应。 */
static void testHttpServerResponseHttp10(void)
{
	static const unsigned char Data[] = "stream";
	test_http_server_response_body Source = {
		Data,
		sizeof(Data) - 1u,
		0,
		false
	};
	xhttpserverrequest* pClose =
		testHttpServerResponseRequest(
			"GET / HTTP/1.0\r\n\r\n",
			XHTTP_SERVER_REQUEST_NONE
		);
	xhttpserverrequest* pKeep =
		testHttpServerResponseRequest(
			"GET / HTTP/1.0\r\n"
			"Connection: keep-alive\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttpbody* pBody = testHttpServerResponseBodyCreate(
		&Source, XHTTP_BODY_UNKNOWN
	);
	xhttp1serverresponse* pResponse;

	testRequire((pClose != NULL) && (pKeep != NULL) &&
		(pReply != NULL) && (pBody != NULL) &&
		xrtHttpReplySetBody(pReply, pBody),
		"HTTP/1.0 close response setup failed");
	xrtHttpBodyDestroy(pBody);
	pResponse = xrtHttp1ServerResponseCreate(
		pClose, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP/1.0 close response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.0 200 OK\r\n"
		"Connection: close\r\n\r\n"
		"stream",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	testRequire(xrtHttp1ServerResponseClose(pResponse),
		"HTTP/1.0 unknown body did not close");
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(200);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("ok"),
			(xstrview){ NULL, 0 }
		), "HTTP/1.0 keep-alive response setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pKeep, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP/1.0 keep-alive response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.0 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n\r\n"
		"ok",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	testRequire(!xrtHttp1ServerResponseClose(pResponse),
		"HTTP/1.0 keep-alive response forced close");
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pKeep);
	xrtHttpServerRequestDestroy(pClose);
}



/* 验证 Upgrade 与成功 CONNECT 只输出 Header 后交接连接。 */
static void testHttpServerResponseTunnel(void)
{
	xhttpserverrequest* pUpgrade =
		testHttpServerResponseRequest(
			"GET /chat HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Connection: Upgrade\r\n"
			"Upgrade: websocket\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE |
				XHTTP_SERVER_REQUEST_UPGRADE
		);
	xhttpserverrequest* pConnect =
		testHttpServerResponseRequest(
			"CONNECT example.test:443 HTTP/1.1\r\n"
			"Host: example.test:443\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(101);
	xhttp1serverresponse* pResponse;

	testRequire((pUpgrade != NULL) && (pConnect != NULL) &&
		(pReply != NULL) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		), "HTTP server Upgrade Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pUpgrade, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server Upgrade response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_TUNNEL,
		NULL
	);
	testRequire(xrtHttp1ServerResponseTunnel(pResponse) &&
		!xrtHttp1ServerResponseClose(pResponse),
		"HTTP server Upgrade connection facts mismatch");
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(200);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("ignored"),
			(xstrview){ NULL, 0 }
		), "HTTP server CONNECT Reply setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pConnect, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server CONNECT response create failed");
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 200 OK\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_TUNNEL,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pConnect);
	xrtHttpServerRequestDestroy(pUpgrade);
}



/* 验证未知长度正文自动使用 chunked 输出。 */
static void testHttpServerResponseUnknown(void)
{
	static const unsigned char Data[] = "later";
	test_http_server_response_body Source = {
		Data,
		sizeof(Data) - 1u,
		0,
		false
	};
	xhttpserverrequest* pRequest =
		testHttpServerResponseRequest(
			"GET /events HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttpbody* pBody = testHttpServerResponseBodyCreate(
		&Source, XHTTP_BODY_UNKNOWN
	);
	xhttp1serverresponse* pResponse;

	testRequire((pRequest != NULL) && (pReply != NULL) &&
		(pBody != NULL) && xrtHttpReplySetBody(
			pReply, pBody
		), "HTTP server unknown response setup failed");
	xrtHttpBodyDestroy(pBody);
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server unknown response create failed");
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
	testHttpServerResponseExpect(
		pResponse,
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n\r\n"
		"5\r\nlater\r\n"
		"0\r\n\r\n",
		XHTTP1_SERVER_OUTPUT_DONE,
		NULL
	);
	xrtHttp1ServerResponseDestroy(pResponse);
}



/* 验证语义无效的 Reply 在输出前被拒绝。 */
static void testHttpServerResponseInvalid(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerResponseRequest(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply;
	xhttp1serverresponse* pResponse;

	pReply = xrtHttpReplyCreate(200);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("body"),
			(xstrview){ NULL, 0 }
		) && xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("3")
		), "HTTP server invalid length setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_LENGTH),
		"HTTP server mismatched Content-Length was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(200);
	testRequire((pReply != NULL) &&
		xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("1")
		), "HTTP server forbidden Trailer setup failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_TRAILER),
		"HTTP server forbidden Trailer was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(103);
	testRequire(pReply != NULL,
		"HTTP server informational Reply create failed");
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_STATUS),
		"HTTP server final informational Reply was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = xrtHttpReplyCreate(101);
	testRequire((pReply != NULL) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("")
		), "HTTP server empty Upgrade setup failed");
	pResponse = xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE |
			XHTTP_SERVER_REQUEST_UPGRADE,
		pReply
	);
	testRequire((pResponse == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP1_SERVER_RESPONSE_ERROR_STATUS),
		"HTTP server empty Upgrade response was accepted");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证正文来源长度失败被包装成稳定响应错误。 */
static void testHttpServerResponseBodyError(void)
{
	static const unsigned char Data[] = "ab";
	test_http_server_response_body Source = {
		Data,
		sizeof(Data) - 1u,
		0,
		false
	};
	xhttpserverrequest* pRequest =
		testHttpServerResponseRequest(
			"GET /broken HTTP/1.1\r\n"
			"Host: example.test\r\n\r\n",
			XHTTP_SERVER_REQUEST_KEEP_ALIVE
		);
	xhttpreply* pReply = xrtHttpReplyCreate(200);
	xhttpbody* pBody = testHttpServerResponseBodyCreate(
		&Source, 4
	);
	xhttp1serverresponse* pResponse;
	unsigned char Output[512];
	size_t iSize = 0;
	xhttp1serveroutputstatus Status;

	testRequire((pRequest != NULL) && (pReply != NULL) &&
		(pBody != NULL) && xrtHttpReplySetBody(
			pReply, pBody
		), "HTTP server failing body setup failed");
	xrtHttpBodyDestroy(pBody);
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server failing body response create failed");
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
	Status = testHttpServerResponseCollect(
		pResponse,
		Output,
		sizeof(Output),
		&iSize,
		NULL
	);
	testRequire(
		(Status == XHTTP1_SERVER_OUTPUT_ERROR) &&
		(xrtHttp1ServerResponseError(pResponse) != NULL) &&
		(xrtErrorCode(
			xrtHttp1ServerResponseError(pResponse)
		) == XHTTP1_SERVER_RESPONSE_ERROR_BODY),
		"HTTP server body source failure was not preserved"
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtClearError();
}



/* 验证空参数与无 Offer 消费契约。 */
static void testHttpServerResponseContracts(void)
{
	unsigned char FieldStorage[sizeof(xhttpfield) + 2u];
	xhttpfield InvalidField = {
		XRT_STR_LITERAL("X-Invalid"),
		{ NULL, 1 }
	};
	xrt_http1_server_response_source Source = {
		200,
		XRT_STR_LITERAL("OK"),
		&InvalidField,
		1,
		NULL,
		0,
		NULL,
		0
	};
	xhttpreply* pReply;
	xhttpbody* pBody;
	xhttp1serverresponse* pResponse;
	unsigned char OutputStorage[sizeof(xbytesview) + 2u];
	xbytesview* pUnaligned = (xbytesview*)(void*)(
		OutputStorage + 1u
	);
	xbytesview Data;

	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	InvalidField.Name = XRT_STR_LITERAL("Content-Length");
	InvalidField.Value = XRT_STR_LITERAL("0");
	memcpy(FieldStorage + 1u, &InvalidField, sizeof(InvalidField));
	Source.Headers = (const xhttpfield*)(const void*)(
		FieldStorage + 1u
	);
	testRequire(
		((pResponse = __xrtHttp1ServerResponsePrepareSource(
			XHTTP_VERSION_1_1,
			XRT_STR_LITERAL("GET"),
			XHTTP_SERVER_REQUEST_KEEP_ALIVE,
			&Source
		 )) != NULL) &&
		(FieldStorage[0] == 0xA5) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5),
		"HTTP server response rejected unaligned Header descriptors"
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	InvalidField.Name = XRT_STR_LITERAL("X-Invalid");
	InvalidField.Value = (xstrview){ NULL, 1 };
	Source.Headers = &InvalidField;

	testRequire(
		(__xrtHttp1ServerResponsePrepareSource(
			XHTTP_VERSION_1_1,
			XRT_STR_LITERAL("GET"),
			XHTTP_SERVER_REQUEST_KEEP_ALIVE,
			&Source
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response source accepted invalid Header view"
	);
	xrtClearError();
	Source.Headers = NULL;
	Source.HeaderCount = 0;
	Source.BodyLength = 1;
	testRequire(
		(__xrtHttp1ServerResponsePrepareSource(
			XHTTP_VERSION_1_1,
			XRT_STR_LITERAL("GET"),
			XHTTP_SERVER_REQUEST_KEEP_ALIVE,
			&Source
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response source accepted missing Body"
	);
	xrtClearError();
	pBody = xrtHttpBodyCopy(XRT_BYTES_LITERAL("ok"));
	testRequire(pBody != NULL,
		"HTTP server response source Body fixture failed");
	Source.Body = pBody;
	testRequire(
		(__xrtHttp1ServerResponsePrepareSource(
			XHTTP_VERSION_1_1,
			XRT_STR_LITERAL("GET"),
			XHTTP_SERVER_REQUEST_KEEP_ALIVE,
			&Source
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response source accepted mismatched Body length"
	);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	testRequire(xrtHttp1ServerResponseCreate(NULL, NULL) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response null create contract mismatch");
	xrtClearError();
	pReply = xrtHttpReplyCreate(200);
	testRequire(pReply != NULL,
		"HTTP server response contract Reply create failed");
	testRequire(xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		UINT32_C(0x80000000),
		pReply
	) == NULL,
		"HTTP server response accepted unknown request flags");
	pResponse = xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE,
		pReply
	);
	testRequire(pResponse != NULL,
		"HTTP server response output contract fixture failed");
	xrtHttpReplyDestroy(pReply);
	xrtClearError();

	memset(OutputStorage, 0xA5, sizeof(OutputStorage));
	testRequire(
		xrtHttp1ServerResponseOutput(
			pResponse, 1, pUnaligned
		) == XHTTP1_SERVER_OUTPUT_DATA,
		"HTTP server response rejected an unaligned output descriptor"
	);
	memcpy(&Data, OutputStorage + 1u, sizeof(Data));
	testRequire(
		(Data.Data != NULL) &&
		(Data.Size == 1) &&
		(OutputStorage[0] == 0xA5) &&
		(OutputStorage[sizeof(OutputStorage) - 1u] == 0xA5),
		"HTTP server response unaligned output damaged guards"
	);
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pResponse,
			1,
			(xbytesview*)(void*)pResponse
		 ) == XHTTP1_SERVER_OUTPUT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response output overwrote its state"
	);
	xrtClearError();
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pResponse,
			1,
			(xbytesview*)(void*)Data.Data
		 ) == XHTTP1_SERVER_OUTPUT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response output overwrote offered data"
	);
	xrtClearError();
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pResponse,
			1,
			(xbytesview*)(uintptr_t)(
				UINTPTR_MAX - (sizeof(xbytesview) / 2u)
			)
		 ) == XHTTP1_SERVER_OUTPUT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server response accepted a wrapping output descriptor"
	);
	xrtClearError();
	testRequire(
		(xrtHttp1ServerResponseOutput(
			pResponse, 1, &Data
		 ) == XHTTP1_SERVER_OUTPUT_DATA) &&
		(Data.Size == 1) &&
		xrtHttp1ServerResponseOutputConsume(
			pResponse, Data.Size
		),
		"HTTP server response did not recover after invalid output"
	);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtClearError();

	testRequire(xrtHttp1ServerResponseOutput(
		NULL, 1, &Data
	) == XHTTP1_SERVER_OUTPUT_ERROR,
		"HTTP server response null Output contract mismatch");
	xrtClearError();
	testRequire(!xrtHttp1ServerResponseOutputConsume(
		NULL, 0
	), "HTTP server response null Consume succeeded");
	xrtClearError();
	xrtHttp1ServerResponseDestroy(NULL);
}



/* 运行 HTTP/1 服务端响应准备与输出测试。 */
int main(void)
{
	testHttpServerResponseInform();
	testHttpServerResponseFixed();
	testHttpServerResponseChunkedTrailers();
	testHttpServerResponseNoContent();
	testHttpServerResponseHttp10();
	testHttpServerResponseTunnel();
	testHttpServerResponseUnknown();
	testHttpServerResponseInvalid();
	testHttpServerResponseBodyError();
	testHttpServerResponseContracts();
	printf("[PASS] HTTP server response\n");
	return 0;
}
