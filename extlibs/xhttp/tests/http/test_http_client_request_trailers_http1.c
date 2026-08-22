#include "../test.h"



/* 比较借用字节与完整字面量。 */
static bool testHttpRequestTrailerBytes(
	xbytesview Data,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Data.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Data.Data, sExpected, iSize) == 0));
}



/* 使用一字节窗口收集全部请求线路并压实终块短写。 */
static xhttp1outputstatus testHttpRequestTrailerOutput(
	xhttp1exchange* pExchange,
	bytes pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iTotal = 0;
	size_t iGuard = 0;

	while ( iGuard++ < (iCapacity * 4u + 128u) ) {
		xbytesview Data;
		xhttp1outputstatus Status = xrtHttp1ExchangeOutput(
			pExchange, 7u, &Data
		);

		if ( Status != XHTTP1_OUTPUT_DATA ) {
			*pSize = iTotal;
			return Status;
		}
		testRequire((Data.Data != NULL) &&
			(Data.Size != 0) && (Data.Size <= 7u) &&
			(iTotal < iCapacity),
			"HTTP request Trailer short output is invalid");
		pOutput[iTotal++] = Data.Data[0];
		testRequire(xrtHttp1ExchangeOutputConsume(
			pExchange, 1u
		), "HTTP request Trailer short output consume failed");
	}
	testRequire(false,
		"HTTP request Trailer output made no bounded progress");
	return XHTTP1_OUTPUT_ERROR;
}



/* 验证 Header 声明、终块和 Exchange 都使用同一份冻结 Trailer 快照。 */
static void testHttpRequestTrailersWire(void)
{
	static const char sHead[] =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Trailer: Digest, X-Meta\r\n"
		"\r\n";
	static const char sEnd[] =
		"0\r\n"
		"Digest: sha-256=:abc:\r\n"
		"X-Meta: yes\r\n"
		"\r\n";
	static const char sWire[] =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Trailer: Digest, X-Meta\r\n"
		"\r\n"
		"3\r\nabc\r\n"
		"0\r\n"
		"Digest: sha-256=:abc:\r\n"
		"X-Meta: yes\r\n"
		"\r\n";
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	unsigned char Output[512];
	size_t iOutput;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest, XRT_BYTES_LITERAL("abc"),
			(xstrview){ NULL, 0 }
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("Ignored")
		) && xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("sha-256=:abc:")
		) && xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("X-Meta"),
			XRT_STR_LITERAL("yes")
		), "HTTP request Trailer wire setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
		 XHTTP_REQUEST_BODY_CHUNKED) &&
		testHttpRequestTrailerBytes(
			xrtHttp1RequestPlanHead(pPlan), sHead
		) && testHttpRequestTrailerBytes(
			xrtHttp1RequestPlanEnd(pPlan), sEnd
		), "HTTP request Trailer frozen plan mismatch");
	testRequire(xrtHttpRequestSetTrailer(
		pRequest,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("changed")
	), "HTTP request Trailer source mutation failed");
	xrtHttpRequestDestroy(pRequest);
	testRequire(testHttpRequestTrailerBytes(
		xrtHttp1RequestPlanEnd(pPlan), sEnd
	), "HTTP request Trailer plan changed with source request");
	pExchange = xrtHttp1ExchangeCreate(pPlan, NULL, NULL);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	testRequire(pExchange != NULL,
		"HTTP request Trailer Exchange create failed");
	testRequire((testHttpRequestTrailerOutput(
		pExchange, Output, sizeof(Output), &iOutput
	) == XHTTP1_OUTPUT_DONE) &&
		(iOutput == (sizeof(sWire) - 1u)) &&
		(memcmp(Output, sWire, iOutput) == 0) &&
		(xrtHttp1ExchangeRequestWireBytes(pExchange) ==
		 (uint64)iOutput),
		"HTTP request Trailer Exchange wire mismatch");
	xrtHttp1ExchangeDestroy(pExchange);
}



/* 验证空正文仍发送 chunked last-chunk 与 Trailer。 */
static void testHttpRequestTrailersEmptyBody(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/empty")
	);
	xhttp1requestplan* pPlan;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("X-Result"),
			XRT_STR_LITERAL("empty")
		), "HTTP request empty Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBody(pPlan) == NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
		 XHTTP_REQUEST_BODY_CHUNKED) &&
		testHttpRequestTrailerBytes(
			xrtHttp1RequestPlanEnd(pPlan),
			"0\r\nX-Result: empty\r\n\r\n"
		), "HTTP request empty Trailer plan mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 要求最近一次准备失败属于稳定请求错误码。 */
static void testHttpRequestTrailerError(
	xhttprequesterror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage);
	xrtClearError();
}



/* 验证声明、分帧、禁止字段和 TRACE 的失败契约。 */
static void testHttpRequestTrailersReject(void)
{
	xhttprequest* pRequest;
	xhttp1requestplan* pPlan;

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("Digest")
		), "HTTP request declared Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan == NULL,
		"HTTP request accepted declaration without Trailer");
	testHttpRequestTrailerError(
		XHTTP_REQUEST_ERROR_TRAILER,
		"HTTP request declaration-only Trailer error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("gzip")
		), "HTTP request coded Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan == NULL,
		"HTTP request accepted non-chunked coding with Trailer");
	testHttpRequestTrailerError(
		XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
		"HTTP request coded Trailer error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("Digest")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("Digest")
		), "HTTP request repeated Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		testHttpRequestTrailerBytes(
			xrtHttp1RequestPlanHead(pPlan),
			"POST /upload HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Trailer: Digest\r\n"
			"\r\n"
		),
		"HTTP request did not canonicalize repeated Trailer declarations");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		), "HTTP request fixed Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan == NULL,
		"HTTP request accepted fixed framing with Trailer");
	testHttpRequestTrailerError(
		XHTTP_REQUEST_ERROR_TRAILER,
		"HTTP request fixed Trailer error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/upload")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		), "HTTP request forbidden Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan == NULL,
		"HTTP request accepted forbidden Trailer");
	testHttpRequestTrailerError(
		XHTTP_REQUEST_ERROR_TRAILER,
		"HTTP request forbidden Trailer error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("TRACE"),
		XRT_STR_LITERAL("http://example.test/")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		), "HTTP request TRACE Trailer setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire(pPlan == NULL,
		"HTTP TRACE accepted Trailer fields");
	testHttpRequestTrailerError(
		XHTTP_REQUEST_ERROR_TRACE_BODY,
		"HTTP TRACE Trailer error mismatch"
	);
	xrtHttpRequestDestroy(pRequest);
}



/* 执行客户端 HTTP/1.1 请求 Trailer 端到端回归。 */
int main(void)
{
	testHttpRequestTrailersWire();
	testHttpRequestTrailersEmptyBody();
	testHttpRequestTrailersReject();
	printf("[PASS] HTTP client request Trailers HTTP/1.1\n");
	return 0;
}


