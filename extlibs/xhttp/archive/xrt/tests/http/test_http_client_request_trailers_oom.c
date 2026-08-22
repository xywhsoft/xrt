#include "../test.h"



/* 在一个故障点验证首次 Add 不会发布半创建 Trailer 容器。 */
static bool testHttpRequestTrailerAddOom(size_t iFail)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	bool bResult;
	bool bTriggered;

	testRequire(pRequest != NULL,
		"HTTP request Trailer Add OOM setup failed");
	testRequire(xrtMemDebugFailAfter((uint64)iFail),
		"HTTP request Trailer Add OOM fault setup failed");
	bResult = xrtHttpRequestAddTrailer(
		pRequest,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("sha-256=:abc:")
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( !bResult ) {
		testRequire(bTriggered &&
			(xrtHttpRequestTrailers(pRequest) == NULL) &&
			(xrtHttpRequestTrailerCount(pRequest) == 0),
			"HTTP request Trailer Add OOM published partial state");
	} else {
		testRequire(!bTriggered &&
			(xrtHttpRequestTrailerCount(pRequest) == 1),
			"HTTP request Trailer Add ignored allocation fault");
	}
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP request Trailer Add OOM leaked storage"
	);
	return bResult;
}



/* 在一个故障点验证 Clone 不改变源 Trailer。 */
static bool testHttpRequestTrailerCloneOom(size_t iFail)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xhttprequest* pClone;
	const xhttpfield* pField;
	bool bTriggered;
	bool bResult;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		), "HTTP request Trailer Clone OOM setup failed");
	testRequire(xrtMemDebugFailAfter((uint64)iFail),
		"HTTP request Trailer Clone OOM fault setup failed");
	pClone = xrtHttpRequestClone(pRequest);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	pField = xrtHttpRequestTrailer(
		pRequest, XRT_STR_LITERAL("Digest")
	);
	testRequire((pField != NULL) &&
		(pField->Value.Size == 5u) &&
		(memcmp(pField->Value.Data, "value", 5u) == 0),
		"HTTP request Trailer Clone OOM changed source");
	if ( pClone == NULL ) {
		testRequire(bTriggered,
			"HTTP request Trailer Clone failed without injected fault");
	} else {
		testRequire(!bTriggered,
			"HTTP request Trailer Clone ignored allocation fault");
	}
	bResult = pClone != NULL;
	xrtHttpRequestDestroy(pClone);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP request Trailer Clone OOM leaked storage"
	);
	return bResult;
}



/* 在一个故障点验证 Prepare 不改变请求且释放临时 Trailer 存储。 */
static bool testHttpRequestTrailerPrepareOom(size_t iFail)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xhttp1requestplan* pPlan;
	const xhttpfield* pField;
	bool bTriggered;
	bool bResult;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest, XRT_BYTES_LITERAL("payload"),
			(xstrview){ NULL, 0 }
		) && xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("sha-256=:abc:")
		), "HTTP request Trailer Prepare OOM setup failed");
	testRequire(xrtMemDebugFailAfter((uint64)iFail),
		"HTTP request Trailer Prepare OOM fault setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	pField = xrtHttpRequestTrailer(
		pRequest, XRT_STR_LITERAL("Digest")
	);
	testRequire((pField != NULL) &&
		(pField->Value.Size == 13u) &&
		(memcmp(
			pField->Value.Data,
			"sha-256=:abc:",
			13u
		) == 0) && (xrtHttpBodyLength(
			xrtHttpRequestBody(pRequest)
		) == 7u),
		"HTTP request Trailer Prepare OOM changed request");
	if ( pPlan == NULL ) {
		testRequire(bTriggered,
			"HTTP request Trailer Prepare failed without injected fault");
	} else {
		testRequire(!bTriggered,
			"HTTP request Trailer Prepare ignored allocation fault");
	}
	bResult = pPlan != NULL;
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP request Trailer Prepare OOM leaked storage"
	);
	return bResult;
}



/* 扫描一个 Trailer 操作的全部逻辑分配点直到无故障成功。 */
static size_t testHttpRequestTrailerSweep(
	bool (*pAttempt)(size_t)
)
{
	size_t iFail;

	for ( iFail = 0; iFail < 128u; iFail++ ) {
		if ( pAttempt(iFail) ) {
			testRequire(iFail != 0,
				"HTTP request Trailer OOM path had no allocations");
			return iFail;
		}
	}
	testRequire(false,
		"HTTP request Trailer OOM scan did not converge");
	return 0;
}



/* 扫描 Trailer 容器、克隆与 HTTP/1 准备的故障原子性。 */
int main(void)
{
	size_t iFaults = 0;

	iFaults += testHttpRequestTrailerSweep(
		testHttpRequestTrailerAddOom
	);
	iFaults += testHttpRequestTrailerSweep(
		testHttpRequestTrailerCloneOom
	);
	iFaults += testHttpRequestTrailerSweep(
		testHttpRequestTrailerPrepareOom
	);
	printf(
		"[PASS] HTTP client request Trailers OOM (%u faults)\n",
		(unsigned)iFaults
	);
	return 0;
}
