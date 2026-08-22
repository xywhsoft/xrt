#include "http_client_response_fixture.h"
#include <xrt/memory_debug.h>



/* 在一个逻辑失败点下遍历响应便利层的成功与错误包装路径。 */
static bool testHttpResponseDataOomAttempt(void)
{
	char Oversized[XSET_COOKIE_MAX_PAIR_BYTES + 2u];
	xhttpfield Fields[2];
	xhttpresponse* pResponse = NULL;
	xmediatype Type;
	xsetcookie Cookie;
	size_t iHeader = 0;
	bool bComplete = false;

	Fields[0] = (xhttpfield){
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/json; charset=utf-8")
	};
	pResponse = testHttpResponseFixtureTryCreate(Fields, 1);
	if ( pResponse == NULL ) {
		goto done;
	}
	if ( xrtHttpResponseContentType(
		pResponse,
		&Type
	) != XHTTP_NEXT_ITEM ) {
		goto done;
	}
	xrtHttpResponseDestroy(pResponse);
	pResponse = NULL;

	/* MIME 语法错误覆盖保留 Cause 的错误包装分配。 */
	Fields[0].Value = XRT_STR_LITERAL("invalid");
	pResponse = testHttpResponseFixtureTryCreate(Fields, 1);
	if ( pResponse == NULL ) {
		goto done;
	}
	if ( xrtHttpResponseContentType(
		pResponse,
		&Type
	) != XHTTP_NEXT_ERROR ) {
		goto done;
	}
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
	pResponse = NULL;

	/* 重复单值字段覆盖强制协议类别和底层值错误 Cause。 */
	Fields[0].Value = XRT_STR_LITERAL("text/plain");
	Fields[1] = (xhttpfield){
		XRT_STR_LITERAL("content-type"),
		XRT_STR_LITERAL("application/json")
	};
	pResponse = testHttpResponseFixtureTryCreate(Fields, 2);
	if ( pResponse == NULL ) {
		goto done;
	}
	if ( xrtHttpResponseContentType(
		pResponse,
		&Type
	) != XHTTP_NEXT_ERROR ) {
		goto done;
	}
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
	pResponse = NULL;

	/* 合法 Set-Cookie 路径不产生解析器内部存储。 */
	Fields[0] = (xhttpfield){
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("sid=abc; Path=/; HttpOnly")
	};
	pResponse = testHttpResponseFixtureTryCreate(Fields, 1);
	if ( pResponse == NULL ) {
		goto done;
	}
	if ( xrtHttpResponseSetCookieNext(
		pResponse,
		&iHeader,
		&Cookie
	) != XHTTP_NEXT_ITEM ) {
		goto done;
	}
	xrtHttpResponseDestroy(pResponse);
	pResponse = NULL;

	/* 超长 Cookie 覆盖范围 Cause 的结构化包装分配。 */
	Oversized[0] = 'a';
	Oversized[1] = '=';
	memset(Oversized + 2, 'x', sizeof(Oversized) - 2u);
	Fields[0].Value = (xstrview){ Oversized, sizeof(Oversized) };
	iHeader = 0;
	pResponse = testHttpResponseFixtureTryCreate(Fields, 1);
	if ( pResponse == NULL ) {
		goto done;
	}
	if ( xrtHttpResponseSetCookieNext(
		pResponse,
		&iHeader,
		&Cookie
	) != XHTTP_NEXT_ERROR ) {
		goto done;
	}
	bComplete = true;

done:
	xrtHttpResponseDestroy(pResponse);
	xrtClearError();
	return bComplete;
}



/* 逐次失败每个逻辑分配，并要求响应与错误对象全部回到空基线。 */
int main(void)
{
	size_t iFail;
	size_t iTriggered = 0;
	bool bFinished = false;

	testRequire(
		testHttpResponseDataOomAttempt(),
		"HTTP response data OOM warm-up failed"
	);
	testMemoryDebugDrain(
		"HTTP response data OOM warm-up reset failed"
	);
	for ( iFail = 0; iFail < 128u; iFail++ ) {
		bool bComplete;
		bool bTriggered;

		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"HTTP response data OOM setup failed"
		);
		bComplete = testHttpResponseDataOomAttempt();
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		testRequire(
			bComplete || bTriggered,
			"HTTP response data failed without injected OOM"
		);
		testMemoryDebugDrain(
			"HTTP response data OOM reset failed"
		);
		if ( !bTriggered ) {
			testRequire(
				bComplete,
				"HTTP response data final attempt failed"
			);
			bFinished = true;
			break;
		}
		iTriggered++;
	}
	testRequire(
		bFinished && (iTriggered != 0),
		"HTTP response data OOM scan did not converge"
	);
	printf(
		"[PASS] HTTP client response data OOM (%u faults)\n",
		(unsigned)iTriggered
	);
	return 0;
}

