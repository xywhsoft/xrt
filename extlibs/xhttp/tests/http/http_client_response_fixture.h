#ifndef TEST_HTTP_CLIENT_RESPONSE_FIXTURE_H
#define TEST_HTTP_CLIENT_RESPONSE_FIXTURE_H

#include "../test.h"
#include "../../src/internal/xrt_http_client.h"



/* 比较借用文本视图与零结尾常量。 */
static inline bool testHttpResponseFixtureText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 尝试从调用方字段创建响应，失败时完整回收半成品。 */
static inline xhttpresponse* testHttpResponseFixtureTryCreate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpresponse* pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("OK"),
		NULL
	);
	size_t i;

	if ( pResponse == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpResponseAddHeader(
			pResponse,
			pFields[i].Name,
			pFields[i].Value
		) ) {
			xrtHttpResponseDestroy(pResponse);
			return NULL;
		}
	}
	return pResponse;
}



/* 从调用方字段创建拥有型客户端响应测试快照。 */
static inline xhttpresponse* testHttpResponseFixtureCreate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpresponse* pResponse = testHttpResponseFixtureTryCreate(
		pFields,
		iCount
	);

	testRequire(
		pResponse != NULL,
		"HTTP response fixture creation failed"
	);
	return pResponse;
}



/* 验证响应读取错误域、类别与代码，并清理线程错误。 */
static inline void testHttpResponseFixtureError(
	xerrkind Kind,
	xhttpresponseerror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();
	bool bMatched = (pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client.response"
		 ) == 0) &&
		(xrtErrorCode(pError) == (int32)Code);

	if ( !bMatched ) {
		fprintf(
			stderr,
			"[ERROR] kind=%d domain=%s code=%d operation=%s\n",
			pError != NULL ? (int)xrtErrorKind(pError) : -1,
			(pError != NULL) &&
				xrtErrorDomain(pError) != NULL ?
					xrtErrorDomain(pError) : "(null)",
			pError != NULL ? (int)xrtErrorCode(pError) : -1,
			(pError != NULL) &&
				xrtErrorOperation(pError) != NULL ?
					xrtErrorOperation(pError) : "(null)"
		);
	}
	testRequire(bMatched, sMessage);
	xrtClearError();
}

#endif

