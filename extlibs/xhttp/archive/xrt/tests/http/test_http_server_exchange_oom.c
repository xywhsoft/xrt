#include "../test.h"



/* 可调失败分配器扫描 Exchange 的全部实际拥有型存储。 */
typedef struct test_http_server_exchange_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_server_exchange_allocator;



/* 在指定分配序号失败。 */
static ptr testHttpServerExchangeAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_server_exchange_allocator* pState =
		(test_http_server_exchange_allocator*)pContext;
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



/* 重分配失败时保留原块。 */
static ptr testHttpServerExchangeRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_exchange_allocator* pState =
		(test_http_server_exchange_allocator*)pContext;
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



/* 释放成功分配的底层块。 */
static void testHttpServerExchangeFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_server_exchange_allocator* pState =
		(test_http_server_exchange_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP server Exchange OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下覆盖跨段 Header、正文和 Trailer。 */
static bool testHttpServerExchangeOomAttempt(void)
{
	static unsigned char First[4096];
	static unsigned char Second[8192];
	static size_t iFirstSize;
	static size_t iSecondSize;
	static bool bInitialized;
	xhttp1serverexchange* pExchange =
		xrtHttp1ServerExchangeCreate(NULL, NULL);
	xhttp1serverfeedstatus Status;
	size_t iAccepted = 0;
	bool bComplete = false;

	if ( !bInitialized ) {
		static const cstr sFirstPrefix =
			"POST /upload HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Transfer-Encoding: chunked\r\n"
			"X-Large: ";
		static const cstr sSecondPrefix =
			"\r\n\r\n800\r\n";
		static const cstr sSecondMiddle =
			"\r\n0\r\nX-Meta: ";
		static const cstr sSecondSuffix = "\r\n\r\n";

		iFirstSize = strlen(sFirstPrefix);
		memcpy(First, sFirstPrefix, iFirstSize);
		memset(First + iFirstSize, 'h', 2048);
		iFirstSize += 2048;

		iSecondSize = strlen(sSecondPrefix);
		memcpy(Second, sSecondPrefix, iSecondSize);
		memset(Second + iSecondSize, 'b', 2048);
		iSecondSize += 2048;
		memcpy(
			Second + iSecondSize,
			sSecondMiddle,
			strlen(sSecondMiddle)
		);
		iSecondSize += strlen(sSecondMiddle);
		memset(Second + iSecondSize, 't', 2048);
		iSecondSize += 2048;
		memcpy(
			Second + iSecondSize,
			sSecondSuffix,
			strlen(sSecondSuffix)
		);
		iSecondSize += strlen(sSecondSuffix);
		bInitialized = true;
	}
	if ( pExchange == NULL ) {
		goto done;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)First,
			iFirstSize
		},
		false,
		&iAccepted
	);
	if ( Status == XHTTP1_SERVER_FEED_ERROR ) {
		goto done;
	}
	testRequire(
		(Status == XHTTP1_SERVER_FEED_MORE) &&
		(iAccepted == iFirstSize),
		"HTTP server Exchange OOM partial Header mismatch"
	);
	iAccepted = 0;
	Status = xrtHttp1ServerExchangeFeed(
		pExchange,
		(xbytesview){
			(cbytes)Second,
			iSecondSize
		},
		false,
		&iAccepted
	);
	if ( Status == XHTTP1_SERVER_FEED_ERROR ) {
		goto done;
	}
	testRequire(
		(Status == XHTTP1_SERVER_FEED_COMPLETE) &&
		(iAccepted == iSecondSize) &&
		(xrtHttpServerRequestBodyBytes(
			xrtHttp1ServerExchangeRequest(pExchange)
		) == 2048) &&
		(xrtHttpServerRequestTrailerCount(
			xrtHttp1ServerExchangeRequest(pExchange)
		) == 1),
		"HTTP server Exchange OOM success state mismatch"
	);
	bComplete = true;

done:
	xrtHttp1ServerExchangeDestroy(pExchange);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并要求每次尝试都回到稳定基线。 */
int main(void)
{
	test_http_server_exchange_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpServerExchangeAlloc,
		testHttpServerExchangeRealloc,
		testHttpServerExchangeFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iWarm;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP server Exchange OOM allocator install failed");
	testRequire(testHttpServerExchangeOomAttempt(),
		"HTTP server Exchange OOM warm-up failed");
	for ( iWarm = 0; iWarm < 2; iWarm++ ) {
		for ( iFail = 1; iFail <= 192; iFail++ ) {
			State.Calls = 0;
			State.FailAt = iFail;
			(void)testHttpServerExchangeOomAttempt();
		}
	}
	testMemoryDebugDrain(
		"HTTP server Exchange OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 192; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpServerExchangeOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP server Exchange OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP server Exchange OOM attempt leaked storage");
	}
	testRequire(iFailures != 0,
		"HTTP server Exchange OOM sweep missed allocation failures");
	testRequire(bSuccess,
		"HTTP server Exchange OOM sweep never recovered");
	printf("[PASS] HTTP server Exchange OOM\n");
	return 0;
}
