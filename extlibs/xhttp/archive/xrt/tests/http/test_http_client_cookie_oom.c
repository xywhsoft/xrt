#include "../test.h"
#include "../fixtures/http_call.h"
#include "../../src/internal/xrt_http_client_runtime.h"
#include "../../src/internal/xrt_memory.h"



/* 只拒绝指定大小的一次分配，避免故障注入误伤被测路径之前的准备工作。 */
typedef struct test_http_cookie_allocator {
	size_t Target;
	bool Armed;
	bool Failed;
} test_http_cookie_allocator;



/* 转发普通分配，并在命中目标大小时注入一次内存不足。 */
static ptr testHttpCookieAlloc(ptr pContext, size_t iSize)
{
	test_http_cookie_allocator* pState =
		(test_http_cookie_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并与初始分配共享同一个故障点。 */
static ptr testHttpCookieRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_cookie_allocator* pState =
		(test_http_cookie_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放由测试分配器产生的内存。 */
static void testHttpCookieFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 配置下一次精确大小的分配失败。 */
static void testHttpCookieArm(
	test_http_cookie_allocator* pState,
	size_t iLogicalSize
)
{
	pState->Target = iLogicalSize +
		__xrtHeapHeaderSize() +
		__xrtMemDebugTailSize() +
		(XRT_HEAP_ALIGNMENT - 1u);
	pState->Armed = true;
	pState->Failed = false;
}



/* 验证同步 Cookie 准备失败被收敛到稳定的客户端错误域。 */
static void testHttpCookieSubmitError(cstr sOperation)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_MEMORY) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_CLIENT_ERROR_COOKIE) &&
		(strcmp(
			xrtErrorOperation(pError),
			sOperation
		) == 0) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL),
		"HTTP Client Cookie submit OOM lost its stable error"
	);
}



/* 记录策略失败通过统一 Call 完成边界发布的结果。 */
typedef struct test_http_cookie_done {
	bool Called;
	xnetresult Result;
	const xerror* Error;
} test_http_cookie_done;



/* 借用完成结果，所有权仍由栈上的模拟 Call 保留。 */
static void testHttpCookieDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_cookie_done* pState =
		(test_http_cookie_done*)pData;

	testRequire(
		(pCall != NULL) && (pResult != NULL),
		"HTTP Client Cookie OOM completion arguments were invalid"
	);
	pState->Called = true;
	pState->Result = pResult->Result;
	pState->Error = pResult->Error;
}



/* 分区键复制与请求 Cookie 生成 OOM 都必须在任何网络操作前失败。 */
static void testHttpCookiePrepareOom(
	test_http_cookie_allocator* pAllocator,
	xcookiejar* pJar,
	xhttprequest* pRequest
)
{
	char Partition[3333];
	char SetCookie[3008];
	xhttpclient Client;
	xhttpcall Call;
	xhttpcalloptions Options;

	memset(Partition, 'p', sizeof(Partition));
	memset(&Client, 0, sizeof(Client));
	Client.Cookies = pJar;
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	xrtHttpCallOptionsInit(&Options);
	Options.Cookies.PartitionKey = (xstrview){
		Partition,
		sizeof(Partition)
	};
	testHttpCookieArm(pAllocator, sizeof(Partition) + 1u);
	testRequire(
		!__xrtHttpCookieInit(&Call, &Options) &&
		pAllocator->Failed,
		"HTTP Client Cookie partition copy survived OOM"
	);
	__xrtHttpCookieSetSubmitError(&Call);
	testHttpCookieSubmitError("prepare-http-cookies");
	xrtClearError();
	__xrtHttpCookieUnit(&Call);

	memcpy(SetCookie, "sid=", 4u);
	memset(SetCookie + 4u, 'v', 2996u);
	memcpy(SetCookie + 3000u, "; Path=/", 8u);
	pAllocator->Armed = false;
	testRequire(
		xrtCookieJarStoreUrl(
			pJar,
			XRT_STR_LITERAL("http://cookie.test/"),
			(xstrview){ SetCookie, sizeof(SetCookie) },
			NULL
		) == XCOOKIE_STORE_STORED,
		"HTTP Client Cookie request OOM fixture store failed"
	);

	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	Call.CookiesEnabled = true;
	Call.CookieFlags = XHTTP_COOKIE_SAME_SITE;
	testHttpCookieArm(pAllocator, 3001u);
	testRequire(
		!__xrtHttpCookiePrepare(&Call) &&
		pAllocator->Failed,
		"HTTP Client Cookie request rendering survived OOM"
	);
	__xrtHttpCookieSetSubmitError(&Call);
	testHttpCookieSubmitError("prepare-http-cookies");
	xrtClearError();
}



/* Set-Cookie 入库 OOM 穿过 Exchange 包装后仍必须保留内存错误分类。 */
static void testHttpCookieStoreOom(
	test_http_cookie_allocator* pAllocator,
	xcookiejar* pJar,
	xhttprequest* pRequest
)
{
	char SetCookie[3029];
	xhttpclient Client;
	xhttpcall Call;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	xhttpresponse* pResponse;
	test_http_cookie_done Done;
	xerror* pMemory;
	xerror* pExchange;

	xrtCookieJarClear(pJar);
	memcpy(SetCookie, "boom=", 5u);
	memset(SetCookie + 5u, 'x', 3016u);
	memcpy(SetCookie + 3021u, "; Path=/", 8u);
	pAllocator->Armed = false;
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("OK"),
		NULL
	);
	testRequire(
		(pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Set-Cookie"),
			(xstrview){ SetCookie, sizeof(SetCookie) }
		),
		"HTTP Client Cookie response OOM fixture failed"
	);

	memset(&Client, 0, sizeof(Client));
	Client.Cookies = pJar;
	testHttpClientStateInit(&Client);
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	Call.CookiesEnabled = true;
	Call.CookieFlags = XHTTP_COOKIE_SAME_SITE;
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpCookieEvents(&Call, &Next);
	testHttpCookieArm(pAllocator, 3037u);
	testRequire(
		!pEvents->Headers(pResponse, pEvents->Data) &&
		pAllocator->Failed &&
		(Call.CookieError == XHTTP_CLIENT_ERROR_COOKIE) &&
		(xrtCookieJarCount(pJar) == 0),
		"HTTP Client Cookie response storage survived or committed OOM"
	);
	pMemory = xrtTakeError();
	testRequire(
		(pMemory != NULL) &&
		(xrtErrorKind(pMemory) == XERR_MEMORY),
		"HTTP Client Cookie response OOM did not publish its cause"
	);
	pExchange = xrtErrorWrap(
		pMemory,
		XERR_CANCELLED,
		"xrt.http.exchange",
		1,
		"HTTP response Header callback stopped the exchange"
	);
	xrtErrorFree(pMemory);
	testRequire(
		pExchange != NULL,
		"HTTP Client Cookie response OOM wrapper creation failed"
	);

	memset(&Done, 0, sizeof(Done));
	testHttpCallStateInit(
		&Call,
		testHttpCookieDone,
		&Done
	);
	testRequire(
		__xrtHttpCookieFail(&Call, pExchange) &&
		Done.Called &&
		(Done.Result == XNET_RESULT_ERROR) &&
		(Done.Error == Call.Error) &&
		(xrtErrorKind(Call.Error) == XERR_MEMORY) &&
		(xrtErrorCode(Call.Error) ==
		 (int32)XHTTP_CLIENT_ERROR_COOKIE) &&
		(strcmp(
			xrtErrorOperation(Call.Error),
			"process-http-cookies"
		) == 0) &&
		(xrtErrorCause(Call.Error) == pExchange) &&
		(xrtErrorIs(Call.Error, XERR_MEMORY) != NULL),
		"HTTP Client Cookie response OOM was hidden by Exchange"
	);

	xrtErrorFree(pExchange);
	xrtErrorFree(Call.Error);
	testHttpCallStateUnit(&Call);
	testHttpClientStateUnit(&Client);
	xrtHttpResponseDestroy(pResponse);
}



/* 覆盖 Cookie 客户端策略的三条分配失败边界。 */
int main(void)
{
	test_http_cookie_allocator State;
	xallocator Allocator;
	xcookiejar* pJar;
	xhttprequest* pRequest;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testHttpCookieAlloc;
	Allocator.Realloc = testHttpCookieRealloc;
	Allocator.Free = testHttpCookieFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP Client Cookie OOM allocator install failed"
	);
	pJar = xrtCookieJarCreate(NULL);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://cookie.test/")
	);
	testRequire(
		(pJar != NULL) && (pRequest != NULL),
		"HTTP Client Cookie OOM fixture creation failed"
	);

	testHttpCookiePrepareOom(&State, pJar, pRequest);
	testHttpCookieStoreOom(&State, pJar, pRequest);

	State.Armed = false;
	xrtHttpRequestDestroy(pRequest);
	xrtCookieJarRelease(pJar);
	printf("[PASS] high-level HTTP Cookie OOM\n");
	return 0;
}
