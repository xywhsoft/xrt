#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 从完整请求头和正文建立可失败的拥有型请求。 */
static xhttpserverrequest* testHttpServerRequestDataCreate(
	cstr sHead,
	xbytesview Body
)
{
	xhttpfield Fields[16];
	xhttp1bodyplan Plan;
	xhttp1head Head;
	xhttpserverrequest* pRequest;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	if ( xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)sHead,
			strlen(sHead)
		},
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY ) {
		return NULL;
	}
	if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ) {
		return NULL;
	}
	pRequest = __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_NONE
	);
	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpServerRequestAppendBody(
		pRequest,
		Body
	) ) {
		xrtHttpServerRequestDestroy(pRequest);
		return NULL;
	}
	__xrtHttpServerRequestSetFlags(
		pRequest,
		XHTTP_SERVER_REQUEST_COMPLETE
	);
	return pRequest;
}



/* 在一个失败点下遍历拥有型结果和错误 cause 包装路径。 */
static bool testHttpServerRequestDataOomAttempt(void)
{
	static const char Multipart[] =
		"--AaB03x\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n"
		"\r\n"
		"value\r\n"
		"--AaB03x--\r\n";
	xhttpserverrequest* pRequest = NULL;
	xqueryparams* pParams = NULL;
	xformdata* pFormData = NULL;
	xmultiparterrorinfo MultipartError;
	xcookiepair Cookie;
	xmediatype Type;
	size_t iOffset = 0;
	bool bComplete = false;

	pRequest = testHttpServerRequestDataCreate(
		"POST /submit?a=1&name=hello+world HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Cookie: sid=abc; theme=dark\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 20\r\n"
		"\r\n",
		XRT_BYTES_LITERAL("a=1&name=hello+world")
	);
	if ( pRequest == NULL ) {
		goto done;
	}
	pParams = xrtHttpServerRequestQueryParams(
		pRequest,
		NULL,
		&iOffset
	);
	if ( pParams == NULL ) {
		goto done;
	}
	xrtQueryParamsDestroy(pParams);
	pParams = NULL;
	if ( xrtHttpServerRequestCookie(
		pRequest,
		XRT_STR_LITERAL("sid"),
		&Cookie
	) != XCOOKIE_NEXT_ITEM ) {
		goto done;
	}
	if ( xrtHttpServerRequestContentType(
		pRequest,
		&Type
	) != XHTTP_NEXT_ITEM ) {
		goto done;
	}
	pParams = xrtHttpServerRequestForm(
		pRequest,
		NULL,
		&iOffset
	);
	if ( pParams == NULL ) {
		goto done;
	}
	xrtQueryParamsDestroy(pParams);
	pParams = NULL;
	xrtHttpServerRequestDestroy(pRequest);
	pRequest = NULL;

	memset(&MultipartError, 0, sizeof(MultipartError));
	pRequest = testHttpServerRequestDataCreate(
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: multipart/form-data; boundary=AaB03x\r\n"
		"Content-Length: 77\r\n"
		"\r\n",
		XRT_BYTES_LITERAL(Multipart)
	);
	if ( pRequest == NULL ) {
		goto done;
	}
	pFormData = xrtHttpServerRequestFormData(
		pRequest,
		NULL,
		NULL,
		&MultipartError
	);
	if ( pFormData == NULL ) {
		goto done;
	}
	xrtFormDataDestroy(pFormData);
	pFormData = NULL;
	xrtHttpServerRequestDestroy(pRequest);
	pRequest = NULL;

	/* 错误对象本身 OOM 时也必须释放底层 Cookie cause。 */
	pRequest = testHttpServerRequestDataCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Cookie: broken\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 }
	);
	if ( pRequest == NULL ) {
		goto done;
	}
	if ( xrtHttpServerRequestCookie(
		pRequest,
		XRT_STR_LITERAL("sid"),
		&Cookie
	) != XCOOKIE_NEXT_ERROR ) {
		goto done;
	}
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
	pRequest = NULL;

	/* MIME 语法错误覆盖同一稳定错误域的 cause 包装失败。 */
	pRequest = testHttpServerRequestDataCreate(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: invalid\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 }
	);
	if ( pRequest == NULL ) {
		goto done;
	}
	if ( xrtHttpServerRequestContentType(
		pRequest,
		&Type
	) != XHTTP_NEXT_ERROR ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFormDataDestroy(pFormData);
	xrtQueryParamsDestroy(pParams);
	xrtHttpServerRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 逐次失败每个逻辑分配，并要求全部存储回到空基线。 */
int main(void)
{
	size_t iFail;
	size_t iTriggered = 0;
	bool bFinished = false;

	testRequire(
		testHttpServerRequestDataOomAttempt(),
		"HTTP server request data OOM warm-up failed"
	);
	testMemoryDebugDrain(
		"HTTP server request data OOM warm-up reset failed"
	);
	for ( iFail = 0; iFail < 256u; iFail++ ) {
		bool bComplete;
		bool bTriggered;

		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"HTTP server request data OOM setup failed"
		);
		bComplete = testHttpServerRequestDataOomAttempt();
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		testRequire(
			bComplete || bTriggered,
			"HTTP server request data failed without injected OOM"
		);
		testMemoryDebugDrain(
			"HTTP server request data OOM reset failed"
		);
		if ( !bTriggered ) {
			testRequire(
				bComplete,
				"HTTP server request data final attempt failed"
			);
			bFinished = true;
			break;
		}
		iTriggered++;
	}
	testRequire(
		bFinished && (iTriggered != 0),
		"HTTP server request data OOM scan did not converge"
	);
	printf(
		"[PASS] HTTP server request data OOM (%u faults)\n",
		(unsigned)iTriggered
	);
	return 0;
}

