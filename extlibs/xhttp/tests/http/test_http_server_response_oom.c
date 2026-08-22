#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 可调失败分配器覆盖 Reply 冻结、线路计划和正文输出。 */
typedef struct test_http_server_response_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_server_response_allocator;



/* 在指定底层分配序号失败。 */
static ptr testHttpServerResponseAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_server_response_allocator* pState =
		(test_http_server_response_allocator*)pContext;
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



/* 重分配失败保留原块。 */
static ptr testHttpServerResponseRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_response_allocator* pState =
		(test_http_server_response_allocator*)pContext;
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
static void testHttpServerResponseFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_server_response_allocator* pState =
		(test_http_server_response_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP server response OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 创建拥有型 HTTP/1.1 请求快照。 */
static xhttpserverrequest* testHttpServerResponseOomRequest(void)
{
	static char Wire[] =
		"GET /oom HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n";
	xhttpfield Fields[4];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 4);
	if ( xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)Wire,
			sizeof(Wire) - 1u
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
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
}



/* 在一个失败点下建立大 Header、正文、Trailer 并完整输出。 */
static bool testHttpServerResponseOomAttempt(void)
{
	static unsigned char Large[4096];
	xhttpserverrequest* pRequest = NULL;
	xhttpreply* pReply = NULL;
	xhttpreply* pInformationReply = NULL;
	xhttpbody* pBody = NULL;
	xhttp1serverresponse* pResponse = NULL;
	xhttp1serverresponse* pInformation = NULL;
	xhttp1serveroutputstatus Status;
	xbytesview Data;
	bool bComplete = false;

	memset(Large, 'x', sizeof(Large));
	pRequest = testHttpServerResponseOomRequest();
	pReply = xrtHttpReplyCreate(200);
	pBody = xrtHttpBodyBorrow(
		(xbytesview){ Large, sizeof(Large) }
	);
	if ( (pRequest == NULL) || (pReply == NULL) ||
		(pBody == NULL) || !xrtHttpReplySetBody(
			pReply, pBody
		) || !xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("X-Large"),
			(xstrview){ (cstr)Large, 2048 }
		) || !xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("X-Meta"),
			(xstrview){ (cstr)Large, 2048 }
		) ) {
		goto done;
	}
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest, pReply
	);
	if ( pResponse == NULL ) {
		goto done;
	}
	for ( ;; ) {
		Status = xrtHttp1ServerResponseOutput(
			pResponse, 1024, &Data
		);
		if ( Status != XHTTP1_SERVER_OUTPUT_DATA ) {
			break;
		}
		if ( !xrtHttp1ServerResponseOutputConsume(
			pResponse, Data.Size
		) ) {
			goto done;
		}
	}
	if ( Status != XHTTP1_SERVER_OUTPUT_DONE ) {
		goto done;
	}

	pInformationReply = xrtHttpReplyCreate(103);
	if ( (pInformationReply == NULL) ||
		!xrtHttpReplyAddHeader(
			pInformationReply,
			XRT_STR_LITERAL("Link"),
			(xstrview){ (cstr)Large, 2048 }
		) ) {
		goto done;
	}
	pInformation = xrtHttp1ServerResponseInform(
		XHTTP_VERSION_1_1, pInformationReply
	);
	if ( pInformation == NULL ) {
		goto done;
	}
	for ( ;; ) {
		Status = xrtHttp1ServerResponseOutput(
			pInformation, 1024, &Data
		);
		if ( Status != XHTTP1_SERVER_OUTPUT_DATA ) {
			break;
		}
		if ( !xrtHttp1ServerResponseOutputConsume(
			pInformation, Data.Size
		) ) {
			goto done;
		}
	}
	bComplete = Status == XHTTP1_SERVER_OUTPUT_DONE;

done:
	xrtHttp1ServerResponseDestroy(pInformation);
	xrtHttpReplyDestroy(pInformationReply);
	xrtHttp1ServerResponseDestroy(pResponse);
	xrtHttpBodyDestroy(pBody);
	xrtHttpReplyDestroy(pReply);
	xrtHttpServerRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并要求全部直接与池化资产回到稳定基线。 */
int main(void)
{
	test_http_server_response_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpServerResponseAlloc,
		testHttpServerResponseRealloc,
		testHttpServerResponseFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iWarm;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP server response OOM allocator install failed");
	testRequire(testHttpServerResponseOomAttempt(),
		"HTTP server response OOM warm-up failed");
	for ( iWarm = 0; iWarm < 2; iWarm++ ) {
		for ( iFail = 1; iFail <= 256; iFail++ ) {
			State.Calls = 0;
			State.FailAt = iFail;
			(void)testHttpServerResponseOomAttempt();
		}
	}
	testMemoryDebugDrain(
		"HTTP server response OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 256; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpServerResponseOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP server response OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP server response OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP server response OOM sweep missed failure or success");
	printf("[PASS] HTTP server response OOM\n");
	return 0;
}

