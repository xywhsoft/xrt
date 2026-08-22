#include "../test.h"



/* 可调失败分配器扫描 Exchange 构建、输出和输入的全部分配点。 */
typedef struct test_http_exchange_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_exchange_allocator;



/* 在指定调用序号拒绝分配。 */
static ptr testHttpExchangeOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_exchange_allocator* pState =
		(test_http_exchange_allocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败时保留原块与存活计数。 */
static ptr testHttpExchangeOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_exchange_allocator* pState =
		(test_http_exchange_allocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 回收底层分配并校验存活计数不会下溢。 */
static void testHttpExchangeOomFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_exchange_allocator* pState =
		(test_http_exchange_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP exchange OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下执行请求准备、正文输出和 chunked 响应解析。 */
static bool testHttpExchangeOomAttempt(void)
{
	static const cstr sResponsePrefix =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"X-Large: ";
	static const cstr sResponseSuffix =
		"\r\n\r\n"
		"7\r\npayload\r\n"
		"0\r\nDigest: value\r\n\r\n";
	static unsigned char Response[32768];
	static char RequestValue[16384];
	static size_t iResponseSize;
	static bool bInitialized;
	xhttprequest* pRequest = NULL;
	xhttp1requestplan* pPlan = NULL;
	xhttp1exchange* pExchange = NULL;
	xhttp1exchangeconfig Config;
	xhttp1outputstatus OutputStatus;
	xhttp1feedstatus FeedStatus;
	xbytesview Data;
	size_t iAccepted = 0;
	bool bComplete = false;

	if ( !bInitialized ) {
		size_t iPrefix = strlen(sResponsePrefix);
		size_t iValue = 24576;
		size_t iSuffix = strlen(sResponseSuffix);

		memset(RequestValue, 'q', sizeof(RequestValue));
		memcpy(Response, sResponsePrefix, iPrefix);
		memset(Response + iPrefix, 'r', iValue);
		memcpy(
			Response + iPrefix + iValue,
			sResponseSuffix,
			iSuffix
		);
		iResponseSize = iPrefix + iValue + iSuffix;
		bInitialized = true;
	}
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/oom")
	);
	if ( (pRequest == NULL) ||
		!xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("X-Large"),
			(xstrview){
				RequestValue,
				sizeof(RequestValue)
			}
		) ||
		!xrtHttpRequestSetBytes(
			pRequest,
			XRT_BYTES_LITERAL("request"),
			XRT_STR_LITERAL("application/octet-stream")
		) ) {
		goto done;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	if ( pPlan == NULL ) {
		goto done;
	}
	xrtHttp1ExchangeConfigInit(&Config);
	Config.Head.MaxHead = 32768;
	Config.Head.MaxFieldLine = 25000;
	Config.Headers.MaxValue = 24576;
	Config.Headers.MaxBytes = 32768;
	pExchange = xrtHttp1ExchangeCreate(
		pPlan, &Config, NULL
	);
	if ( pExchange == NULL ) {
		goto done;
	}
	pPlan = NULL;
	for ( ;; ) {
		OutputStatus = xrtHttp1ExchangeOutput(
			pExchange, 11, &Data
		);
		if ( OutputStatus != XHTTP1_OUTPUT_DATA ) {
			break;
		}
		if ( !xrtHttp1ExchangeOutputConsume(
			pExchange, Data.Size
		) ) {
			goto done;
		}
	}
	if ( OutputStatus != XHTTP1_OUTPUT_DONE ) {
		goto done;
	}
	FeedStatus = xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){
			Response,
			iResponseSize
		},
		false,
		&iAccepted
	);
	if ( (FeedStatus != XHTTP1_FEED_DONE) ||
		(iAccepted != iResponseSize) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtHttp1ExchangeDestroy(pExchange);
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 扫描全部分配失败序号并验证每条退出路径都回到零存活块。 */
int main(void)
{
	test_http_exchange_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpExchangeOomAlloc,
		testHttpExchangeOomRealloc,
		testHttpExchangeOomFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iWarm;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP exchange OOM allocator install failed");
	testRequire(testHttpExchangeOomAttempt(),
		"HTTP exchange OOM warm-up failed");
	for ( iWarm = 0; iWarm < 2; iWarm++ ) {
		for ( iFail = 1; iFail <= 256; iFail++ ) {
			State.Calls = 0;
			State.FailAt = iFail;
			(void)testHttpExchangeOomAttempt();
		}
	}
	testMemoryDebugDrain(
		"HTTP exchange OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 256; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpExchangeOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP exchange OOM memory debug reset failed"
		);
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"[OOM] fail=%zu calls=%zu live=%zu baseline=%zu\n",
				iFail,
				State.Calls,
				State.Live,
				iBaseline
			);
		}
		testRequire(State.Live == iBaseline,
			"HTTP exchange OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP exchange OOM sweep missed failure or success");
	printf("[PASS] HTTP client exchange OOM\n");
	return 0;
}

