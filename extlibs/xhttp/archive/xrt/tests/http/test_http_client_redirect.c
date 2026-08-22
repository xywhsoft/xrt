#include "../test.h"

#include "../../src/internal/xrt_http_client_runtime.h"
#include "../../src/internal/xrt_memory.h"



#if !defined(TEST_HTTP_REDIRECT_BACKEND)
	#define TEST_HTTP_REDIRECT_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_REDIRECT_BACKEND_NAME "select"
#endif

#define TEST_HTTP_REDIRECT_STREAMS 8u
#define TEST_HTTP_REDIRECT_FRAGMENT_SIZE 4096u



/* 只拒绝 fragment 继承扩容对应的一次底层申请。 */
typedef struct test_http_redirect_allocator {
	size_t Target;
	bool Armed;
	bool Failed;
} test_http_redirect_allocator;



/* 转发普通分配，并在命中目标大小时注入一次内存不足。 */
static ptr testHttpRedirectAlloc(ptr pData, size_t iSize)
{
	test_http_redirect_allocator* pState =
		(test_http_redirect_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并与初始分配共享同一个故障点。 */
static ptr testHttpRedirectRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_redirect_allocator* pState =
		(test_http_redirect_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放透传分配器产生的底层内存。 */
static void testHttpRedirectFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 配置下一次精确逻辑大小的 backing 分配失败。 */
static void testHttpRedirectArm(
	test_http_redirect_allocator* pState,
	size_t iLogicalSize
)
{
	pState->Target = iLogicalSize +
		__xrtHeapHeaderSize() +
		__xrtMemDebugTailSize() +
		(XRT_HEAP_ALIGNMENT - 1u);
	pState->Armed = true;
	pState->Failed = false;
}



typedef enum test_http_redirect_scenario {
	TEST_HTTP_REDIRECT_FOLLOW = 0,
	TEST_HTTP_REDIRECT_MANUAL,
	TEST_HTTP_REDIRECT_ERROR,
	TEST_HTTP_REDIRECT_POST_303,
	TEST_HTTP_REDIRECT_POST_307,
	TEST_HTTP_REDIRECT_LOWER_POST_302,
	TEST_HTTP_REDIRECT_NON_REPLAYABLE,
	TEST_HTTP_REDIRECT_LIMIT,
	TEST_HTTP_REDIRECT_DUPLICATE,
	TEST_HTTP_REDIRECT_DUPLICATE_MANUAL,
	TEST_HTTP_REDIRECT_CROSS_ORIGIN,
	TEST_HTTP_REDIRECT_FRAGMENT_INHERIT,
	TEST_HTTP_REDIRECT_FRAGMENT_EMPTY
} test_http_redirect_scenario;



/* 一次场景拥有独立网络栈，避免上一场景连接关闭时序污染断言。 */
typedef struct test_http_redirect {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Streams[TEST_HTTP_REDIRECT_STREAMS];
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 Closed;
	xatomic32 Completed;
	xatomic32 ListenerClosed;
	test_http_redirect_scenario Scenario;
	xhttpclienterror ExpectedError;
	size_t ExpectedRedirects;
	uint16 Port;
	char ExpectedUrl[192];
	size_t ExpectedUrlSize;
	size_t HeaderCalls;
	size_t BodyCalls;
	size_t BodyBytes;
	size_t OneShotOffset;
	char StreamBody[16];
} test_http_redirect;



/* 在十秒边界内等待网络 Worker 发布指定计数。 */
static void testHttpRedirectWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 两个测试域名都解析到同一监听器，用于验证 origin 安全边界。 */
static xnetaddrlist* testHttpRedirectLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		(strcmp(sHost, "redirect.test") == 0) ||
		(strcmp(sHost, "other.test") == 0),
		"HTTP redirect resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP redirect resolver address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 静态正文块没有独立租约，释放过程只满足 Body Chunk 契约。 */
static void testHttpRedirectBodyRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 发布一次性正文的剩余前缀。 */
static xhttpbodystatus testHttpRedirectBodyNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	static const uint8 Data[] = { 'D', 'A', 'T', 'A' };
	size_t* pOffset = (size_t*)pContext;
	size_t iSize;

	if ( *pOffset == sizeof(Data) ) {
		return XHTTP_BODY_EOF;
	}
	iSize = sizeof(Data) - *pOffset;
	if ( iSize > iMaxBytes ) {
		iSize = iMaxBytes;
	}
	pChunk->Data = Data + *pOffset;
	pChunk->Size = iSize;
	pChunk->Release = testHttpRedirectBodyRelease;
	*pOffset += iSize;
	return XHTTP_BODY_DATA;
}



/* 每个一次性 Body 只会调用一次 Open，因此可以直接借用工厂状态。 */
static bool testHttpRedirectBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	size_t* pOffset = (size_t*)pFactory;

	*pOffset = 0;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpRedirectBodyNext;
	*ppReader = pOffset;
	return true;
}



/* 在流式模式只记录最终响应回调次数。 */
static bool testHttpRedirectHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;

	(void)pCall;
	testRequire(
		xrtHttpResponseStatus(pResponse) == 200,
		"HTTP redirect exposed intermediate headers"
	);
	pState->HeaderCalls++;
	return true;
}



/* 在流式模式拼接最终正文，确保中间响应正文没有泄露。 */
static bool testHttpRedirectBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;

	(void)pCall;
	testRequire(
		(xrtHttpResponseStatus(pResponse) == 200) &&
		((pState->BodyBytes + Data.Size) <=
		 sizeof(pState->StreamBody)),
		"HTTP redirect streamed invalid final body"
	);
	memcpy(
		pState->StreamBody + pState->BodyBytes,
		Data.Data,
		Data.Size
	);
	pState->BodyBytes += Data.Size;
	pState->BodyCalls++;
	return true;
}



/* 发送完整响应并在发送队列排空后关闭当前服务端连接。 */
static void testHttpRedirectRespond(
	xnetstream* pStream,
	cstr sResponse,
	size_t iSize
)
{
	testRequire(
		xrtNetStreamSend(
			pStream,
			sResponse,
			iSize
		) == XNET_RESULT_OK,
		"HTTP redirect server response send failed"
	);
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP redirect server close failed"
	);
}



/* 严格解析一条完整测试请求，并把正文解码到调用方缓冲。 */
static bool testHttpRedirectRequestParse(
	cstr sRequest,
	size_t iSize,
	size_t* pWireSize,
	size_t* pHeadSize,
	char* sBody,
	size_t iBodyCapacity,
	size_t* pBodySize,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity,
	size_t* pTrailerCount
)
{
	xhttpfield Fields[64];
	xhttp1head Head;
	xhttp1limits HeadLimits;
	xhttp1bodyplan Plan;
	xhttp1body Body;
	xhttp1bodylimits BodyLimits;
	xhttp1errorinfo Error;
	xhttp1status HeadStatus;
	size_t iOffset;
	size_t iDecoded = 0;

	xrtHttp1LimitsInit(&HeadLimits);
	xrtHttp1HeadInit(&Head, Fields, 64u);
	HeadStatus = xrtHttp1RequestParse(
		(xbytesview){ (cbytes)sRequest, iSize },
		&Head,
		&HeadLimits,
		&Error
	);
	if ( HeadStatus == XHTTP1_MORE ) {
		return false;
	}
	testRequire(
		HeadStatus == XHTTP1_READY,
		"HTTP redirect emitted an invalid request Header"
	);
	testRequire(
		xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP redirect emitted invalid request framing"
	);
	xrtHttp1BodyLimitsInit(&BodyLimits);
	testRequire(
		xrtHttp1BodyInit(
			&Body,
			&Plan,
			pTrailers,
			iTrailerCapacity,
			&BodyLimits
		),
		"HTTP redirect request Body Reader init failed"
	);
	iOffset = Head.Bytes;
	for ( ;; ) {
		xbytesview Data;
		size_t iConsumed = 0;
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){
				(cbytes)sRequest + iOffset,
				iSize - iOffset
			},
			false,
			&iConsumed,
			&Data,
			&Error
		);

		iOffset += iConsumed;
		if ( Status == XHTTP1_BODY_MORE ) {
			return false;
		}
		testRequire(
			(Status != XHTTP1_BODY_ERROR) &&
			(Status != XHTTP1_BODY_FIELDS),
			"HTTP redirect emitted an invalid request Body"
		);
		if ( Status == XHTTP1_BODY_DATA ) {
			testRequire(
				Data.Size <= (iBodyCapacity - iDecoded),
				"HTTP redirect decoded Body exceeded fixture capacity"
			);
			memcpy(sBody + iDecoded, Data.Data, Data.Size);
			iDecoded += Data.Size;
			continue;
		}
		break;
	}
	*pWireSize = iOffset;
	*pHeadSize = Head.Bytes;
	*pBodySize = iDecoded;
	*pTrailerCount = Body.TrailerCount;
	return true;
}



/* 验证每种重定向改写产生的实际请求行、正文和凭据边界。 */
static void testHttpRedirectCheckRequest(
	test_http_redirect* pState,
	uint32 iRequest,
	cstr sRequest,
	size_t iHead,
	cstr sBody,
	size_t iBody,
	const xhttpfield* pTrailers,
	size_t iTrailerCount
)
{
	(void)pTrailers;

	if ( iRequest == 1u ) {
		if ( pState->Scenario ==
			TEST_HTTP_REDIRECT_LOWER_POST_302 ) {
			testRequire(
				(strncmp(
					sRequest,
					"post /",
					6u
				) == 0) &&
				(iBody == 4u) &&
				(memcmp(sBody, "DATA", 4u) == 0),
				"HTTP redirect initial custom method mismatch"
			);
		} else if (
			(pState->Scenario == TEST_HTTP_REDIRECT_POST_303) ||
			(pState->Scenario == TEST_HTTP_REDIRECT_POST_307) ||
			(pState->Scenario ==
			 TEST_HTTP_REDIRECT_NON_REPLAYABLE) ) {
			testRequire(
				(strncmp(
					sRequest,
					"POST /",
					6u
				) == 0) &&
				(iBody == 4u) &&
				(memcmp(sBody, "DATA", 4u) == 0),
				"HTTP redirect initial POST mismatch"
			);
			#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
				if ( pState->Scenario ==
					TEST_HTTP_REDIRECT_POST_303 ) {
					testRequire(
						(strstr(
							sRequest,
							"\r\nTransfer-Encoding: chunked\r\n"
						) != NULL) &&
						(strstr(
							sRequest,
							"\r\nTrailer: X-Upload-Result\r\n"
						) != NULL) &&
						(iTrailerCount == 1u) &&
						(pTrailers[0].Name.Size == 15u) &&
						(memcmp(
							pTrailers[0].Name.Data,
							"X-Upload-Result",
							15u
						) == 0) &&
						(pTrailers[0].Value.Size == 8u) &&
						(memcmp(
							pTrailers[0].Value.Data,
							"complete",
							8u
						) == 0),
						"HTTP redirect initial POST Trailer mismatch"
					);
				}
			#endif
		} else {
			testRequire(
				strncmp(sRequest, "GET /", 5u) == 0,
				"HTTP redirect initial GET mismatch"
			);
		}
		return;
	}
	if ( pState->Scenario == TEST_HTTP_REDIRECT_POST_303 ) {
		if ( (iBody != 0) ||
			(strstr(sRequest, "\r\nContent-Length:") != NULL) ||
			(strstr(sRequest, "\r\nContent-Type:") != NULL) ||
			(strstr(sRequest, "\r\nContent-Disposition:") != NULL) ||
			(strstr(sRequest, "\r\nContent-Digest:") != NULL) ||
			(strstr(sRequest, "\r\nRepr-Digest:") != NULL)
			#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
				|| (strstr(sRequest, "\r\nTrailer:") != NULL)
			#endif
			) {
			fprintf(
				stderr,
				"[redirect-post303-request]\n%.*s\n",
				(int)(iHead + iBody),
				sRequest
			);
		}
		testRequire(
			(strncmp(
				sRequest,
				"GET /after303 HTTP/1.1\r\n",
				24u
			) == 0) &&
			(iBody == 0) &&
			(strstr(
				sRequest,
				"\r\nContent-Length:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nContent-Type:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nContent-Disposition:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nContent-Digest:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nRepr-Digest:"
			) == NULL) &&
			(iTrailerCount == 0u)
			#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
				&& (strstr(
					sRequest,
					"\r\nTrailer:"
				) == NULL)
			#endif
			,
			"HTTP 303 did not rewrite POST to bodyless GET"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_POST_307
	) {
		testRequire(
			(strncmp(
				sRequest,
				"POST /after307 HTTP/1.1\r\n",
				25u
			) == 0) &&
			(iBody == 4u) &&
			(memcmp(sBody, "DATA", 4u) == 0),
			"HTTP 307 did not replay method and body"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_LOWER_POST_302
	) {
		testRequire(
			(strncmp(
				sRequest,
				"post /after-lower302 HTTP/1.1\r\n",
				sizeof(
					"post /after-lower302 HTTP/1.1\r\n"
				) - 1u
			) == 0) &&
			(iBody == 4u) &&
			(memcmp(sBody, "DATA", 4u) == 0),
			"HTTP 302 rewrote a case-distinct custom method"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_CROSS_ORIGIN
	) {
		testRequire(
			(strncmp(
				sRequest,
				"GET /cross-final HTTP/1.1\r\n",
				27u
			) == 0) &&
			(strstr(
				sRequest,
				"\r\nHost: other.test:"
			) != NULL) &&
			(strstr(
				sRequest,
				"\r\nAuthorization:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nProxy-Authorization:"
			) == NULL) &&
			(strstr(
				sRequest,
				"\r\nCookie:"
			) == NULL),
			"HTTP cross-origin redirect forwarded credentials"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_LIMIT
	) {
		testRequire(
			strncmp(
				sRequest,
				"GET /loop HTTP/1.1\r\n",
				20u
			) == 0,
			"HTTP redirect loop target mismatch"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_INHERIT
	) {
		testRequire(
			strncmp(
				sRequest,
				"GET /fragment-final HTTP/1.1\r\n",
				30u
			) == 0,
			"HTTP redirect inherited fragment reached request target"
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_EMPTY
	) {
		testRequire(
			strncmp(
				sRequest,
				"GET /fragment-empty-final HTTP/1.1\r\n",
				36u
			) == 0,
			"HTTP redirect empty fragment reached request target"
		);
	} else {
		testRequire(
			strncmp(
				sRequest,
				"GET /final HTTP/1.1\r\n",
				21u
			) == 0,
			"HTTP redirect final target mismatch"
		);
	}
}



/* 按场景和跳数构造重定向或最终响应。 */
static void testHttpRedirectRoute(
	test_http_redirect* pState,
	xnetstream* pStream,
	uint32 iRequest
)
{
	static const char Final[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	static const char Relative[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /final\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n"
		"\r\n"
		"skip";
	static const char Post303[] =
		"HTTP/1.1 303 See Other\r\n"
		"Location: /after303\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char Post307[] =
		"HTTP/1.1 307 Temporary Redirect\r\n"
		"Location: /after307\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char LowerPost302[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /after-lower302\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char Loop[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /loop\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char Duplicate[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /first\r\n"
		"Location: /second\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char FragmentInherit[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /fragment-final\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char FragmentEmpty[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /fragment-empty-final#\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	char Cross[256];
	int iLength;

	if ( (pState->Scenario == TEST_HTTP_REDIRECT_FOLLOW) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_MANUAL) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_ERROR) ) {
		if ( iRequest == 1u ) {
			testHttpRedirectRespond(
				pStream,
				Relative,
				sizeof(Relative) - 1u
			);
		} else {
			testHttpRedirectRespond(
				pStream,
				Final,
				sizeof(Final) - 1u
			);
		}
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_POST_303
	) {
		testHttpRedirectRespond(
			pStream,
			iRequest == 1u ? Post303 : Final,
			iRequest == 1u ?
				sizeof(Post303) - 1u :
				sizeof(Final) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_POST_307
	) {
		testHttpRedirectRespond(
			pStream,
			iRequest == 1u ? Post307 : Final,
			iRequest == 1u ?
				sizeof(Post307) - 1u :
				sizeof(Final) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_LOWER_POST_302
	) {
		testHttpRedirectRespond(
			pStream,
			iRequest == 1u ? LowerPost302 : Final,
			iRequest == 1u ?
				sizeof(LowerPost302) - 1u :
				sizeof(Final) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_NON_REPLAYABLE
	) {
		testHttpRedirectRespond(
			pStream,
			Post307,
			sizeof(Post307) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_LIMIT
	) {
		testHttpRedirectRespond(
			pStream,
			Loop,
			sizeof(Loop) - 1u
		);
	} else if (
		(pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE_MANUAL)
	) {
		testHttpRedirectRespond(
			pStream,
			Duplicate,
			sizeof(Duplicate) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_INHERIT
	) {
		testHttpRedirectRespond(
			pStream,
			iRequest == 1u ? FragmentInherit : Final,
			iRequest == 1u ?
				sizeof(FragmentInherit) - 1u :
				sizeof(Final) - 1u
		);
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_EMPTY
	) {
		testHttpRedirectRespond(
			pStream,
			iRequest == 1u ? FragmentEmpty : Final,
			iRequest == 1u ?
				sizeof(FragmentEmpty) - 1u :
				sizeof(Final) - 1u
		);
	} else if ( iRequest == 1u ) {
		iLength = snprintf(
			Cross,
			sizeof(Cross),
			"HTTP/1.1 302 Found\r\n"
			"Location: http://other.test:%u/cross-final\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"\r\n",
			(unsigned int)pState->Port
		);
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Cross)),
			"HTTP cross-origin response overflowed"
		);
		testHttpRedirectRespond(
			pStream,
			Cross,
			(size_t)iLength
		);
	} else {
		testHttpRedirectRespond(
			pStream,
			Final,
			sizeof(Final) - 1u
		);
	}
}



/* 提取一个完整请求，验证后发布当前跳响应。 */
static void testHttpRedirectServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;
	char Request[4096];
	char Body[4096];
	xhttpfield Trailers[8];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHead = 0;
	size_t iWire;
	size_t iBody;
	size_t iTrailerCount;
	uint32 iRequest;

	testRequire(
		iSize < sizeof(Request),
		"HTTP redirect request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP redirect request peek failed"
	);
	Request[iSize] = 0;
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (Request[i - 3u] == '\r') &&
			(Request[i - 2u] == '\n') &&
			(Request[i - 1u] == '\r') &&
			(Request[i] == '\n') ) {
			iHead = i + 1u;
			break;
		}
	}
	if ( iHead == 0 ) {
		return;
	}
	if ( !testHttpRedirectRequestParse(
		Request,
		iSize,
		&iWire,
		&iHead,
		Body,
		sizeof(Body),
		&iBody,
		Trailers,
		8u,
		&iTrailerCount
	) ) {
		return;
	}
	testRequire(
		xrtNetBufConsume(
			pBuffer,
			iWire
		) == iWire,
		"HTTP redirect request consume failed"
	);
	iRequest = xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	testHttpRedirectCheckRequest(
		pState,
		iRequest,
		Request,
		iHead,
		Body,
		iBody,
		Trailers,
		iTrailerCount
	);
	testHttpRedirectRoute(
		pState,
		pStream,
		iRequest
	);
}



/* 客户端提前关闭时完成服务端半关闭。 */
static void testHttpRedirectServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录全部服务端连接已经离开关闭队列。 */
static void testHttpRedirectServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 接管每一跳建立的服务端连接并保留一个测试 Owner 引用。 */
static bool testHttpRedirectAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;
	xnetstreamevents Events;
	uint32 iAccepted;

	(void)pListener;
	iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);
	testRequire(
		iAccepted < TEST_HTTP_REDIRECT_STREAMS,
		"HTTP redirect accepted too many streams"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpRedirectServerRead;
	Events.End = testHttpRedirectServerEnd;
	Events.Close = testHttpRedirectServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"HTTP redirect server event takeover failed"
	);
	pState->Streams[iAccepted] = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已经排空全部在途 Accept。 */
static void testHttpRedirectListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证最终成功响应或精确重定向错误，并接管成功响应。 */
static void testHttpRedirectDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_redirect* pState =
		(test_http_redirect*)pData;
	xstrview Url;

	testRequire(
		(pCall != NULL) && (pResult != NULL),
		"HTTP redirect completion was empty"
	);
	testRequire(
		(pCall->RedirectRequest == NULL) &&
		!pCall->RedirectPending,
		"HTTP redirect terminal state retained a pending request"
	);
	if ( pState->ExpectedError != 0 ) {
		testRequire(
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorCode(pResult->Error) ==
			 (int64)pState->ExpectedError) &&
			(strcmp(
				xrtErrorDomain(pResult->Error),
				"xrt.http.client"
			) == 0) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_FAILED),
			"HTTP redirect error result mismatch"
		);
	} else {
		if ( !((pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(pResult->Error == NULL) &&
			(pResult->Info.Redirects ==
			 pState->ExpectedRedirects) &&
			(xrtHttpResponseRedirects(
				pResult->Response
			 ) == pState->ExpectedRedirects)) ) {
			fprintf(
				stderr,
				"[redirect-result] scenario=%d result=%d "
				"response=%p error=%p redirects=%zu/%zu "
				"response-redirects=%zu\n",
				(int)pState->Scenario,
				(int)pResult->Result,
				(void*)pResult->Response,
				(void*)pResult->Error,
				pResult->Info.Redirects,
				pState->ExpectedRedirects,
				xrtHttpResponseRedirects(
					pResult->Response
				)
			);
			if ( pResult->Error != NULL ) {
				const xerror* pError = pResult->Error;

				while ( pError != NULL ) {
					fprintf(
						stderr,
						"[redirect-error] kind=%d "
						"domain=%s code=%lld "
						"operation=%s message=%s\n",
						(int)xrtErrorKind(pError),
						xrtErrorDomain(pError),
						(long long)xrtErrorCode(
							pError
						),
						xrtErrorOperation(pError),
						xrtErrorMessage(pError)
					);
					pError = xrtErrorCause(pError);
				}
			}
		}
		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(pResult->Error == NULL) &&
			(pResult->Info.Redirects ==
			 pState->ExpectedRedirects) &&
			(xrtHttpResponseRedirects(
				pResult->Response
			 ) == pState->ExpectedRedirects),
			"HTTP redirect success result mismatch"
		);
		Url = xrtHttpResponseUrl(pResult->Response);
		testRequire(
			(Url.Size == pState->ExpectedUrlSize) &&
			(memcmp(
				Url.Data,
				pState->ExpectedUrl,
				Url.Size
			) == 0),
			"HTTP redirect effective URL mismatch"
		);
		if ( (pState->Scenario == TEST_HTTP_REDIRECT_MANUAL) ||
			(pState->Scenario ==
			 TEST_HTTP_REDIRECT_DUPLICATE_MANUAL) ) {
			testRequire(
				(xrtHttpResponseStatus(
					pResult->Response
				 ) == 302) &&
				(xrtHttpResponseBody(
					pResult->Response
				 ).Size ==
				 (pState->Scenario == TEST_HTTP_REDIRECT_MANUAL ?
					4u : 0u)),
				"HTTP manual redirect response mismatch"
			);
		} else if (
			pState->Scenario == TEST_HTTP_REDIRECT_FOLLOW
		) {
			testRequire(
				(xrtHttpResponseStatus(
					pResult->Response
				 ) == 200) &&
				(xrtHttpResponseBody(
					pResult->Response
				 ).Size == 0) &&
				(xrtHttpResponseBodyBytes(
					pResult->Response
				 ) == 2u),
				"HTTP streamed redirect response mismatch"
			);
		} else {
			testRequire(
				(xrtHttpResponseStatus(
					pResult->Response
				 ) == 200) &&
				(xrtHttpResponseBody(
					pResult->Response
				 ).Size == 2u),
				"HTTP redirect final response mismatch"
			);
		}
		pState->Response = pResult->Response;
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立场景的 Engine、Listener 和启用安全重定向策略的 Client。 */
static void testHttpRedirectStart(
	test_http_redirect* pState,
	test_http_redirect_scenario Scenario
)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xnetaddr Address;

	memset(pState, 0, sizeof(*pState));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&pState->Accepted, 0);
	xrtAtomic32Init(&pState->Requests, 0);
	xrtAtomic32Init(&pState->Closed, 0);
	xrtAtomic32Init(&pState->Completed, 0);
	xrtAtomic32Init(&pState->ListenerClosed, 0);
	pState->Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_REDIRECT_BACKEND;
	EngineConfig.Workers = 2;
	pState->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pState->Engine != NULL) &&
		xrtNetEngineStart(pState->Engine),
		"HTTP redirect engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP redirect listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpRedirectAccept;
	ListenerEvents.Close = testHttpRedirectListenerClose;
	pState->Listener = xrtNetListen(
		pState->Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		pState
	);
	testRequire(
		(pState->Listener != NULL) &&
		xrtNetListenerLocal(
			pState->Listener,
			&Address
		),
		"HTTP redirect listener creation failed"
	);
	pState->Port = Address.Port;

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpRedirectLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	if ( Scenario == TEST_HTTP_REDIRECT_LIMIT ) {
		ClientConfig.Redirect.MaxHops = 2;
	}
	pState->Client = xrtHttpClientCreate(
		pState->Engine,
		&ClientConfig
	);
	testRequire(
		pState->Client != NULL,
		"HTTP redirect client creation failed"
	);
}



/* 构造场景请求和调用选项，并保存预期的最终有效 URL。 */
static void testHttpRedirectCall(
	test_http_redirect* pState
)
{
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xhttpbody* pBody = NULL;
	xhttpbodyops BodyOps;
	cstr sMethod = "GET";
	cstr sTarget = "start";
	char Url[192];
	cstr sExpectedHost = "redirect.test";
	cstr sExpectedTarget = "final";
	int iLength;

	memset(&BodyOps, 0, sizeof(BodyOps));
	if ( pState->Scenario == TEST_HTTP_REDIRECT_MANUAL ) {
		sExpectedTarget = "start";
	} else if ( pState->Scenario == TEST_HTTP_REDIRECT_POST_303 ) {
		sMethod = "POST";
		sTarget = "post303";
		sExpectedTarget = "after303";
	} else if ( pState->Scenario == TEST_HTTP_REDIRECT_POST_307 ) {
		sMethod = "POST";
		sTarget = "post307";
		sExpectedTarget = "after307";
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_LOWER_POST_302
	) {
		sMethod = "post";
		sTarget = "lower-post302";
		sExpectedTarget = "after-lower302";
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_NON_REPLAYABLE
	) {
		sMethod = "POST";
		sTarget = "non-replay";
	} else if ( pState->Scenario == TEST_HTTP_REDIRECT_LIMIT ) {
		sTarget = "loop";
	} else if (
		(pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE_MANUAL)
	) {
		sTarget = "duplicate";
		if ( pState->Scenario ==
			TEST_HTTP_REDIRECT_DUPLICATE_MANUAL ) {
			sExpectedTarget = "duplicate";
		}
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_CROSS_ORIGIN
	) {
		sTarget = "cross";
		sExpectedHost = "other.test";
		sExpectedTarget = "cross-final";
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_INHERIT
	) {
		sTarget = "fragment#client";
		sExpectedTarget = "fragment-final#client";
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_FRAGMENT_EMPTY
	) {
		sTarget = "fragment-empty#client";
		sExpectedTarget = "fragment-empty-final#";
	}
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://redirect.test:%u/%s",
		(unsigned int)pState->Port,
		sTarget
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP redirect request URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP redirect request creation failed"
	);
	if ( (pState->Scenario == TEST_HTTP_REDIRECT_POST_303) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_POST_307) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_LOWER_POST_302) ) {
		testRequire(
			xrtHttpRequestSetBytes(
				pRequest,
				(xbytesview){
					(cbytes)"DATA",
					4u
				},
				XRT_STR_LITERAL("text/plain")
			),
			"HTTP redirect replayable body setup failed"
		);
		if ( pState->Scenario ==
			TEST_HTTP_REDIRECT_POST_303 ) {
			testRequire(
				xrtHttpRequestAddHeader(
					pRequest,
					XRT_STR_LITERAL(
						"Content-Disposition"
					),
					XRT_STR_LITERAL("inline")
				) &&
				xrtHttpRequestAddHeader(
					pRequest,
					XRT_STR_LITERAL("Content-Digest"),
					XRT_STR_LITERAL(
						"sha-256=:YWJj:"
					)
				) &&
				xrtHttpRequestAddHeader(
					pRequest,
					XRT_STR_LITERAL("Repr-Digest"),
					XRT_STR_LITERAL(
						"sha-256=:YWJj:"
					)
				),
				"HTTP redirect representation metadata setup failed"
			);
			#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
				testRequire(
					xrtHttpRequestAddTrailer(
						pRequest,
						XRT_STR_LITERAL("X-Upload-Result"),
						XRT_STR_LITERAL("complete")
					),
					"HTTP redirect request Trailer setup failed"
				);
			#endif
		}
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_NON_REPLAYABLE
	) {
		BodyOps.Open = testHttpRedirectBodyOpen;
		pBody = xrtHttpBodyCreate(
			&BodyOps,
			&pState->OneShotOffset,
			4u,
			XHTTP_BODY_NONE
		);
		testRequire(
			(pBody != NULL) &&
			xrtHttpRequestSetBody(pRequest, pBody),
			"HTTP redirect one-shot body setup failed"
		);
		xrtHttpBodyDestroy(pBody);
	}
	if ( pState->Scenario ==
		TEST_HTTP_REDIRECT_CROSS_ORIGIN ) {
		testRequire(
			xrtHttpRequestAddHeader(
				pRequest,
				XRT_STR_LITERAL("Authorization"),
				XRT_STR_LITERAL("Bearer secret")
			) &&
			xrtHttpRequestAddHeader(
				pRequest,
				XRT_STR_LITERAL("Proxy-Authorization"),
				XRT_STR_LITERAL("Basic secret")
			) &&
			xrtHttpRequestAddHeader(
				pRequest,
				XRT_STR_LITERAL("Cookie"),
				XRT_STR_LITERAL("sid=secret")
			),
			"HTTP redirect credential setup failed"
		);
	}

	xrtHttpCallOptionsInit(&Options);
	if ( (pState->Scenario == TEST_HTTP_REDIRECT_MANUAL) ||
		(pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE_MANUAL) ) {
		Options.Redirect = XHTTP_REDIRECT_MANUAL;
		pState->ExpectedRedirects = 0;
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_ERROR
	) {
		Options.Redirect = XHTTP_REDIRECT_ERROR;
		pState->ExpectedError =
			XHTTP_CLIENT_ERROR_REDIRECT;
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_NON_REPLAYABLE
	) {
		pState->ExpectedError =
			XHTTP_CLIENT_ERROR_REDIRECT_REPLAY;
	} else if ( pState->Scenario == TEST_HTTP_REDIRECT_LIMIT ) {
		pState->ExpectedError =
			XHTTP_CLIENT_ERROR_REDIRECT_LIMIT;
	} else if (
		pState->Scenario == TEST_HTTP_REDIRECT_DUPLICATE
	) {
		pState->ExpectedError =
			XHTTP_CLIENT_ERROR_REDIRECT;
	} else {
		pState->ExpectedRedirects = 1;
	}
	if ( pState->Scenario == TEST_HTTP_REDIRECT_FOLLOW ) {
	Options.Events.Headers =
		testHttpRedirectHeaders;
	Options.Events.Body = testHttpRedirectBody;
	Options.Events.Data = pState;
	}
	iLength = snprintf(
		pState->ExpectedUrl,
		sizeof(pState->ExpectedUrl),
		"http://%s:%u/%s",
		sExpectedHost,
		(unsigned int)pState->Port,
		sExpectedTarget
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength <
		 sizeof(pState->ExpectedUrl)),
		"HTTP redirect expected URL overflowed"
	);
	pState->ExpectedUrlSize = (size_t)iLength;
	pState->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		&Options,
		testHttpRedirectDone,
		pState
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pState->Call != NULL,
		"HTTP redirect call submission failed"
	);
}



/* 关闭场景并证明所有连接、监听器和 Engine 都已排空。 */
static void testHttpRedirectStop(
	test_http_redirect* pState
)
{
	uint32 iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);

	testHttpRedirectWait(
		&pState->Closed,
		iAccepted,
		"HTTP redirect server streams did not close"
	);
	testRequire(
		xrtNetListenerClose(pState->Listener),
		"HTTP redirect listener close failed"
	);
	testHttpRedirectWait(
		&pState->ListenerClosed,
		1,
		"HTTP redirect listener did not close"
	);
	xrtHttpResponseDestroy(pState->Response);
	xrtHttpCallDestroy(pState->Call);
	xrtHttpClientDestroy(pState->Client);
	for ( uint32 i = 0; i < iAccepted; i++ ) {
		xrtNetStreamDestroy(pState->Streams[i]);
	}
	xrtNetListenerDestroy(pState->Listener);
	if ( !xrtNetEngineDestroy(pState->Engine) ) {
		const xerror* pError = xrtGetError();
		xnetenginestats Stats;

		memset(&Stats, 0, sizeof(Stats));
		(void)xrtNetEngineStats(
			pState->Engine,
			&Stats
		);

		fprintf(
			stderr,
			"[redirect-cleanup] scenario=%d "
			"accepted=%u closed=%u pending=%zu "
			"timers=%zu live=%u kind=%d "
			"operation=%s message=%s\n",
			(int)pState->Scenario,
			(unsigned int)iAccepted,
			(unsigned int)xrtAtomic32Load(
				&pState->Closed,
				XMEMORY_ACQUIRE
			),
			Stats.PendingCommands,
			Stats.ActiveTimers,
			(unsigned int)Stats.LiveObjects,
			pError != NULL ?
				(int)xrtErrorKind(pError) : 0,
			pError != NULL ?
				xrtErrorOperation(pError) : "",
			pError != NULL ?
				xrtErrorMessage(pError) : ""
		);
		testRequire(
			false,
			"HTTP redirect engine destroy failed"
		);
	}
}



/* 运行一条完整重定向场景并核对请求数和流式隐藏规则。 */
static void testHttpRedirectRun(
	test_http_redirect_scenario Scenario,
	uint32 iExpectedRequests
)
{
	test_http_redirect State;

	testHttpRedirectStart(&State, Scenario);
	testHttpRedirectCall(&State);
	testHttpRedirectWait(
		&State.Completed,
		1,
		"HTTP redirect call did not complete"
	);
	testHttpRedirectWait(
		&State.Requests,
		iExpectedRequests,
		"HTTP redirect server request count mismatch"
	);
	if ( Scenario == TEST_HTTP_REDIRECT_FOLLOW ) {
		testRequire(
			(State.HeaderCalls == 1u) &&
			(State.BodyCalls != 0) &&
			(State.BodyBytes == 2u) &&
			(memcmp(State.StreamBody, "OK", 2u) == 0),
			"HTTP redirect exposed intermediate stream events"
		);
	}
	testHttpRedirectStop(&State);
}



/* fragment 继承扩容失败必须保持原请求并发布内存原因。 */
static void testHttpRedirectFragmentOom(
	test_http_redirect_allocator* pAllocator
)
{
	static const char Prefix[] =
		"http://redirect.test/start#";
	char Url[
		(sizeof(Prefix) - 1u) +
		TEST_HTTP_REDIRECT_FRAGMENT_SIZE
	];
	xhttpclient Client;
	xhttpcall Call;
	xhttpcalloptions Options;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	const xerror* pError;
	size_t iExpanded;

	memcpy(Url, Prefix, sizeof(Prefix) - 1u);
	memset(
		Url + sizeof(Prefix) - 1u,
		'f',
		TEST_HTTP_REDIRECT_FRAGMENT_SIZE
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, sizeof(Url) }
	);
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		302,
		XRT_STR_LITERAL("Found"),
		NULL
	);
	testRequire(
		(pRequest != NULL) &&
		(pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Location"),
			XRT_STR_LITERAL("/after")
		),
		"HTTP redirect fragment OOM fixture failed"
	);

	memset(&Client, 0, sizeof(Client));
	xrtHttpRedirectConfigInit(&Client.Config.Redirect);
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Request = pRequest;
	xrtHttpCallOptionsInit(&Options);
	testRequire(
		__xrtHttpRedirectInit(&Call, &Options),
		"HTTP redirect fragment OOM initialization failed"
	);
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpRedirectEvents(&Call, &Next);

	iExpanded =
		strlen("http://redirect.test/after") +
		1u +
		TEST_HTTP_REDIRECT_FRAGMENT_SIZE +
		1u;
	testHttpRedirectArm(pAllocator, iExpanded);
	testRequire(
		!pEvents->Headers(pResponse, pEvents->Data) &&
		pAllocator->Failed &&
		(Call.RedirectError ==
		 XHTTP_CLIENT_ERROR_REDIRECT) &&
		(Call.Request == pRequest) &&
		(Call.RedirectRequest == NULL) &&
		!Call.RedirectPending,
		"HTTP redirect fragment expansion survived or committed OOM"
	);
	pAllocator->Armed = false;
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL),
		"HTTP redirect fragment OOM lost its memory cause"
	);

	xrtClearError();
	__xrtHttpRedirectUnit(&Call);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证重定向默认配置支持未对齐存储并拒绝回绕地址。 */
static void testHttpRedirectConfigDefaults(void)
{
	uint8 Storage[sizeof(xhttpredirectconfig) + 2u];
	xhttpredirectconfig Config;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttpRedirectConfigInit(
		(xhttpredirectconfig*)(Storage + 1u)
	);
	memcpy(&Config, Storage + 1u, sizeof(Config));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Config.Flags == XHTTP_REDIRECT_POST_TO_GET) &&
		(Config.MaxHops == XHTTP_REDIRECT_MAX_DEFAULT),
		"HTTP redirect unaligned initializer mismatch"
	);

	xrtClearError();
	xrtHttpRedirectConfigInit(
		(xhttpredirectconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP redirect wrapping initializer contract mismatch"
	);
	xrtClearError();
}



/* 覆盖旧版已压实的全部核心重定向语义。 */
int main(void)
{
	test_http_redirect_allocator AllocatorState = { 0 };
	xallocator Allocator = {
		&AllocatorState,
		testHttpRedirectAlloc,
		testHttpRedirectRealloc,
		testHttpRedirectFree
	};

	testHttpRedirectConfigDefaults();
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP redirect OOM allocator install failed"
	);
	testHttpRedirectFragmentOom(&AllocatorState);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_FOLLOW, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_MANUAL, 1);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_ERROR, 1);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_POST_303, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_POST_307, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_LOWER_POST_302, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_NON_REPLAYABLE, 1);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_LIMIT, 3);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_DUPLICATE, 1);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_DUPLICATE_MANUAL, 1);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_CROSS_ORIGIN, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_FRAGMENT_INHERIT, 2);
	testHttpRedirectRun(TEST_HTTP_REDIRECT_FRAGMENT_EMPTY, 2);
	printf(
		"[PASS] HTTP client redirects (%s)\n",
		TEST_HTTP_REDIRECT_BACKEND_NAME
	);
	return 0;
}
