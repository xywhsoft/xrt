#ifndef TEST_HTTP_SERVER_REQUEST_FIXTURE_H
#define TEST_HTTP_SERVER_REQUEST_FIXTURE_H

#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 比较借用文本视图与零结尾常量。 */
static inline bool testHttpServerRequestFixtureText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 从完整请求头和可选正文创建测试用拥有型请求快照。 */
static inline xhttpserverrequest* testHttpServerRequestFixtureCreate(
	cstr sHead,
	xbytesview Body,
	uint32 iFlags
)
{
	xhttpfield Fields[32];
	xhttp1bodyplan Plan;
	xhttp1head Head;
	xhttpserverrequest* pRequest;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		xrtHttp1RequestParse(
			(xbytesview){
				(cbytes)sHead,
				strlen(sHead)
			},
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"HTTP server request fixture parse failed"
	);
	testRequire(
		xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP server request fixture body plan failed"
	);
	pRequest = __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		iFlags
	);
	testRequire(
		pRequest != NULL,
		"HTTP server request fixture allocation failed"
	);
	if ( Body.Size != 0 ) {
		if ( (iFlags & XHTTP_SERVER_REQUEST_STREAMED) != 0 ) {
			testRequire(
				__xrtHttpServerRequestDeliverBody(
					pRequest,
					(uint64)Body.Size
				),
				"HTTP server request fixture stream delivery failed"
			);
		} else if ( (iFlags &
			XHTTP_SERVER_REQUEST_DISCARDED) == 0 ) {
			testRequire(
				__xrtHttpServerRequestAppendBody(
					pRequest,
					Body
				),
				"HTTP server request fixture body append failed"
			);
		}
	}
	__xrtHttpServerRequestSetFlags(
		pRequest,
		XHTTP_SERVER_REQUEST_COMPLETE
	);
	return pRequest;
}



/* 验证请求辅助层错误域、类别与代码，并清理线程错误。 */
static inline void testHttpServerRequestFixtureError(
	xerrkind Kind,
	xhttpserverrequesterror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();
	bool bMatched = (pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server.request"
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
