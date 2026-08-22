#include "../test.h"

#include <xrt/http_client.h>
#include <xrt/http_connection.h>
#include <xrt/http_te.h>



/* 验证空请求一步补齐 TE 与 Connection，并保持重复调用幂等。 */
static void testHttpRequestAcceptTrailersBasic(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/events")
	);
	xhttpteinfo Te;
	size_t iCount;

	testRequire(pRequest != NULL,
		"HTTP request TE basic request create failed");
	testRequire(xrtHttp1RequestAcceptTrailers(pRequest),
		"HTTP request Trailer capability setup failed");
	iCount = xrtHttpRequestHeaderCount(pRequest);
	testRequire(
		(iCount == 2u) &&
		xrtHttpTeParse(
			xrtHttpRequestHeaderData(pRequest), iCount, &Te
		) &&
		((Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
		(xrtHttpConnectionFind(
			xrtHttpRequestHeaderData(pRequest),
			iCount,
			XRT_STR_LITERAL("TE")
		) == XHTTP_NEXT_ITEM),
		"HTTP request Trailer capability fields mismatch"
	);
	testRequire(
		xrtHttp1RequestAcceptTrailers(pRequest) &&
		(xrtHttpRequestHeaderCount(pRequest) == iCount),
		"HTTP request Trailer capability was not idempotent"
	);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证已有传输编码与连接选项保留为重复字段。 */
static void testHttpRequestAcceptTrailersPreserve(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/data")
	);
	xhttpteinfo Te;
	size_t iCount;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("gzip;q=0.5")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("close")
		),
		"HTTP request TE preserve setup failed"
	);
	testRequire(xrtHttp1RequestAcceptTrailers(pRequest),
		"HTTP request TE preserve operation failed");
	iCount = xrtHttpRequestHeaderCount(pRequest);
	testRequire(
		(iCount == 4u) &&
		xrtHttpTeParse(
			xrtHttpRequestHeaderData(pRequest), iCount, &Te
		) &&
		((Te.Flags & XHTTP_TE_HAS_TRANSFER_CODINGS) != 0) &&
		((Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
		(xrtHttpConnectionFind(
			xrtHttpRequestHeaderData(pRequest),
			iCount,
			XRT_STR_LITERAL("close")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpConnectionFind(
			xrtHttpRequestHeaderData(pRequest),
			iCount,
			XRT_STR_LITERAL("TE")
		) == XHTTP_NEXT_ITEM),
		"HTTP request TE preserve result mismatch"
	);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证非法既有字段失败时请求保持不变并发布稳定错误。 */
static void testHttpRequestAcceptTrailersErrors(void)
{
	xhttprequest* pRequest;
	const xerror* pError;

	testRequire(!xrtHttp1RequestAcceptTrailers(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request TE helper accepted a null request");
	xrtClearError();
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	testRequire((pRequest != NULL) && xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("TE"),
		XRT_STR_LITERAL("trailers;q=1")
	), "HTTP request malformed TE setup failed");
	testRequire(!xrtHttp1RequestAcceptTrailers(pRequest) &&
		(xrtHttpRequestHeaderCount(pRequest) == 1u),
		"HTTP request helper accepted or changed malformed TE");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_TE) &&
		(xrtErrorCause(pError) != NULL),
		"HTTP request malformed TE error contract mismatch");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	testRequire((pRequest != NULL) && xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("(")
	), "HTTP request malformed Connection setup failed");
	testRequire(!xrtHttp1RequestAcceptTrailers(pRequest) &&
		(xrtHttpRequestHeaderCount(pRequest) == 1u),
		"HTTP request helper accepted or changed malformed Connection");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_CONNECTION) &&
		(xrtErrorCause(pError) != NULL),
		"HTTP request malformed Connection error contract mismatch");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);
}



int main(void)
{
	testHttpRequestAcceptTrailersBasic();
	testHttpRequestAcceptTrailersPreserve();
	testHttpRequestAcceptTrailersErrors();
	printf("[PASS] http_client_request_te\n");
	return 0;
}
