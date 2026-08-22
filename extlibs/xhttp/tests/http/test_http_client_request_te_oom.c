#include "../test.h"

#include <xrt/http_client.h>
#include <xrt/http_connection.h>
#include <xrt/http_te.h>



/* 在一个逻辑分配故障点验证请求 Header 的事务原子性。 */
static bool testHttpRequestTeOomAttempt(size_t iFail)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/events")
	);
	xhttpteinfo Te;
	xhttpnext Connection;
	size_t iCount;
	bool bResult;
	bool bTriggered;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("X-Base"),
			XRT_STR_LITERAL("kept")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("gzip;q=0.5")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("close")
		),
		"HTTP request TE OOM setup failed"
	);
	testRequire(xrtMemDebugFailAfter((uint64)iFail),
		"HTTP request TE OOM fault setup failed");
	bResult = xrtHttp1RequestAcceptTrailers(pRequest);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	iCount = xrtHttpRequestHeaderCount(pRequest);
	testRequire(xrtHttpTeParse(
		xrtHttpRequestHeaderData(pRequest), iCount, &Te
	), "HTTP request TE OOM left an invalid TE field");
	Connection = xrtHttpConnectionFind(
		xrtHttpRequestHeaderData(pRequest),
		iCount,
		XRT_STR_LITERAL("TE")
	);
	if ( bResult ) {
		testRequire(!bTriggered && (iCount == 5u) &&
			((Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
			(Connection == XHTTP_NEXT_ITEM),
			"HTTP request TE OOM success result mismatch"
		);
	} else {
		testRequire(bTriggered && (iCount == 3u) &&
			((Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) == 0) &&
			(Connection == XHTTP_NEXT_END),
			"HTTP request TE OOM failure changed request Headers"
		);
	}
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);
	testMemoryDebugDrain(
		"HTTP request TE OOM attempt leaked storage"
	);
	return bResult;
}



/* 扫描事务式 Helper 的全部逻辑分配点直到无故障成功。 */
int main(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 128u; iFail++ ) {
		if ( testHttpRequestTeOomAttempt(iFail) ) {
			testRequire(iFail != 0,
				"HTTP request TE OOM path had no allocations");
			printf(
				"[PASS] http_client_request_te_oom (%u faults)\n",
				(unsigned)iFail
			);
			return 0;
		}
	}
	testRequire(false,
		"HTTP request TE OOM scan did not converge");
	return 1;
}


