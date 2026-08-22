#include "../test.h"
#include "../fixtures/http_origin.h"



#if !defined(TEST_HTTP_DECOMPRESS_FUTURE_BACKEND)
	#define TEST_HTTP_DECOMPRESS_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_DECOMPRESS_FUTURE_BACKEND_NAME "select"
#endif



/*
	创建仅使用 IPv4 的客户端，避免双栈连接竞速干扰组合契约测试。
*/
static xhttpclient* testHttpDecompressFutureClient(
	xnetengine* pEngine
)
{
	xhttpclientconfig Config;
	xhttpclient* pClient;

	xrtHttpClientConfigInit(&Config);
	Config.Dial.Family = XNET_FAMILY_IPV4;
	Config.Dial.MaxAttempts = 1;
	Config.Timeout = UINT64_C(5000000);
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient != NULL,
		"HTTP decompression Future client create failed"
	);
	return pClient;
}



/*
	创建指向测试 Origin 的拥有型 GET 请求。
*/
static xhttprequest* testHttpDecompressFutureRequest(
	const testhttporigin* pOrigin
)
{
	char Url[256];
	int iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u/decompress-future",
		(unsigned)testHttpOriginPort(pOrigin)
	);
	xhttprequest* pRequest;

	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP decompression Future URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression Future request create failed"
	);
	return pRequest;
}



/*
	等待全部异步析构完成后销毁 Engine。
*/
static void testHttpDecompressFutureEngineDestroy(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP decompression Future retained an Engine object"
		);
		xrtThreadYield();
	}
}



/*
	验证 Future 返回的是解码后的拥有型响应，同时保留完整线路计数与编码元数据。
*/
int main(void)
{
	static const char sDecoded[] = "future decoded body\n";
	static const char sWire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Encoding: gzip\r\n"
		"Content-Length: 40\r\n"
		"Connection: close\r\n"
		"\r\n"
		"\x1F""\x8B""\x08""\x00""\x00""\x00""\x00""\x00"
		"\x02""\xFF""\x4B""\x2B""\x2D""\x29""\x2D""\x4A"
		"\x55""\x48""\x49""\x4D""\xCE""\x4F""\x49""\x4D"
		"\x51""\x48""\xCA""\x4F""\xA9""\xE4""\x02""\x00"
		"\x34""\x32""\x86""\xF3""\x14""\x00""\x00""\x00";
	testhttporigin Origin;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xhttpresult* pResult;
	const xhttpresponse* pResponse;
	xhttpcallinfo Info;
	xbytesview Body;
	xstrview Encoding;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_DECOMPRESS_FUTURE_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP decompression Future engine start failed"
	);
	testHttpOriginStart(
		&Origin,
		pEngine,
		sWire,
		sizeof(sWire) - 1u
	);
	pClient = testHttpDecompressFutureClient(pEngine);
	pRequest = testHttpDecompressFutureRequest(&Origin);
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pFuture != NULL,
		"HTTP decompression Future submission failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP decompression Future did not resolve"
	);

	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		xrtHttpResultInfo(pResult, &Info) &&
		(Info.State == XHTTP_CALL_SUCCEEDED) &&
		(Info.Result == XNET_RESULT_OK) &&
		(Info.Error == XHTTP_CLIENT_ERROR_NONE) &&
		(Info.ResponseBodyBytes == (sizeof(sDecoded) - 1u)),
		"HTTP decompression Future metadata mismatch"
	);
	pResponse = xrtHttpResultResponse(pResult);
	testRequire(
		pResponse != NULL,
		"HTTP decompression Future response missing"
	);
	Body = xrtHttpResponseBody(pResponse);
	Encoding = xrtHttpResponseOriginalEncoding(pResponse);
	testRequire(
		((xrtHttpResponseFlags(pResponse) &
		  XHTTP_RESPONSE_DECOMPRESSED) != 0) &&
		(Body.Size == (sizeof(sDecoded) - 1u)) &&
		(memcmp(
			Body.Data,
			sDecoded,
			sizeof(sDecoded) - 1u
		) == 0) &&
		(xrtHttpResponseBodyBytes(pResponse) ==
		 (sizeof(sDecoded) - 1u)) &&
		(xrtHttpResponseWireBodyBytes(pResponse) == 40u) &&
		(Encoding.Size == 4u) &&
		(memcmp(Encoding.Data, "gzip", 4u) == 0),
		"HTTP decompression Future response mismatch"
	);

	xrtFutureDestroy(pFuture);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
	testHttpDecompressFutureEngineDestroy(pEngine);
	printf(
		"[PASS] HTTP decompression Future contract (%s)\n",
		TEST_HTTP_DECOMPRESS_FUTURE_BACKEND_NAME
	);
	return 0;
}
