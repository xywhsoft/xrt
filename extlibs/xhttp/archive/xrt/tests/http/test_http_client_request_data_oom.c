#include "../test.h"



/* 请求数据适配类型用于复用同一套失败原子性扫描。 */
typedef enum test_http_request_data_kind {
	TEST_HTTP_REQUEST_DATA_QUERY = 0,
	TEST_HTTP_REQUEST_DATA_FORM,
	TEST_HTTP_REQUEST_DATA_FORM_DATA,
	TEST_HTTP_REQUEST_DATA_FORM_DATA_RANDOM,
	TEST_HTTP_REQUEST_DATA_COUNT
} test_http_request_data_kind;



/* 返回每种请求适配失败时必须发布的稳定错误码。 */
static xhttprequesterror testHttpRequestDataError(
	test_http_request_data_kind Kind
)
{
	if ( Kind == TEST_HTTP_REQUEST_DATA_QUERY ) {
		return XHTTP_REQUEST_ERROR_QUERY;
	}
	if ( Kind == TEST_HTTP_REQUEST_DATA_FORM ) {
		return XHTTP_REQUEST_ERROR_FORM;
	}
	return XHTTP_REQUEST_ERROR_FORM_DATA;
}



/* 验证失败后请求的 URL、正文和 Content-Type 仍是操作前的完整状态。 */
static bool testHttpRequestDataStateUnchanged(
	const xhttprequest* pRequest,
	xhttpbody* pBody
)
{
	xstrview Url = xrtHttpRequestUrlText(pRequest);
	const xhttpfield* pType = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	);

	return (Url.Size == 28u) &&
		(memcmp(
			Url.Data,
			"https://example.test/old?q=1",
			28u
		) == 0) &&
		(xrtHttpRequestBody(pRequest) == pBody) &&
		(pType != NULL) && (pType->Value.Size == 10u) &&
		(memcmp(pType->Value.Data, "text/plain", 10u) == 0);
}



/* 在给定故障序号执行一次请求数据适配，并验证失败原子性与错误链。 */
static bool testHttpRequestDataOomAttempt(
	test_http_request_data_kind Kind,
	size_t iFail
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/old?q=1")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xmultipartboundary Output;
	xmultipartboundary Sentinel;
	xhttpbody* pBody;
	const xerror* pError;
	bool bResult;
	bool bTriggered;

	testRequire(
		(pRequest != NULL) && (pParams != NULL) &&
		(pForm != NULL) && xrtHttpRequestSetBytes(
			pRequest,
			XRT_BYTES_LITERAL("old"),
			XRT_STR_LITERAL("text/plain")
		) && xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("xrt runtime")
		) && xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("xrt runtime")
		) && xrtMultipartBoundaryParse(
			XRT_STR_LITERAL("xrt-client-oom"),
			&Boundary
		),
		"HTTP request data OOM setup failed"
	);
	pBody = xrtHttpRequestBody(pRequest);
	memset(&Sentinel, 0xA5, sizeof(Sentinel));
	Output = Sentinel;
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"HTTP request data OOM fault setup failed"
	);
	if ( Kind == TEST_HTTP_REQUEST_DATA_QUERY ) {
		bResult = xrtHttpRequestSetQueryParams(
			pRequest,
			pParams
		);
	} else if ( Kind == TEST_HTTP_REQUEST_DATA_FORM ) {
		bResult = xrtHttpRequestSetForm(
			pRequest,
			pParams
		);
	} else if ( Kind == TEST_HTTP_REQUEST_DATA_FORM_DATA ) {
		bResult = xrtHttpRequestSetFormData(
			pRequest,
			pForm,
			&Boundary
		);
	} else {
		bResult = xrtHttpRequestSetFormDataRandom(
			pRequest,
			pForm,
			&Output
		);
	}
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( !bResult ) {
		pError = xrtGetError();
		testRequire(
			bTriggered &&
			testHttpRequestDataStateUnchanged(
				pRequest,
				pBody
			) && (pError != NULL) &&
			(xrtErrorKind(pError) == XERR_MEMORY) &&
			(xrtErrorCode(pError) == (int32)
				testHttpRequestDataError(Kind)) &&
			(strcmp(
				xrtErrorDomain(pError),
				"http.request"
			) == 0),
			"HTTP request data OOM changed state or error contract"
		);
		if ( Kind == TEST_HTTP_REQUEST_DATA_FORM_DATA_RANDOM ) {
			testRequire(
				memcmp(
					&Output,
					&Sentinel,
					sizeof(Output)
				) == 0,
				"HTTP request random FormData OOM published boundary"
			);
		}
	} else {
		testRequire(
			!bTriggered,
			"HTTP request data ignored an injected allocation fault"
		);
	}

	xrtFormDataDestroy(pForm);
	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP request data OOM attempt leaked storage"
	);
	return bResult;
}



/* 扫描一种请求数据适配的全部逻辑分配点直到无故障完成。 */
static size_t testHttpRequestDataOomSweep(
	test_http_request_data_kind Kind
)
{
	size_t iFail;

	for ( iFail = 0; iFail < 128u; iFail++ ) {
		if ( testHttpRequestDataOomAttempt(Kind, iFail) ) {
			testRequire(
				iFail != 0,
				"HTTP request data OOM path had no allocations"
			);
			return iFail;
		}
	}
	testRequire(
		false,
		"HTTP request data OOM scan did not converge"
	);
	return 0;
}



/* 扫描 Query、Form 与两种 FormData 请求适配的全部失败点。 */
int main(void)
{
	size_t iFaults = 0;
	size_t i;

	for ( i = 0; i < TEST_HTTP_REQUEST_DATA_COUNT; i++ ) {
		iFaults += testHttpRequestDataOomSweep(
			(test_http_request_data_kind)i
		);
	}
	printf(
		"[PASS] HTTP client request data OOM (%u faults)\n",
		(unsigned)iFaults
	);
	return 0;
}
