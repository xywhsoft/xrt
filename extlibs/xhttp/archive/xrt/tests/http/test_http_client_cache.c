#include "../test.h"

#include "../../src/internal/xrt_http_client_runtime.h"



typedef enum test_http_client_cache_scenario {
	TEST_HTTP_CLIENT_CACHE_FRESH = 0,
	TEST_HTTP_CLIENT_CACHE_VALIDATE,
	TEST_HTTP_CLIENT_CACHE_VARY,
	TEST_HTTP_CLIENT_CACHE_PARTITION,
	TEST_HTTP_CLIENT_CACHE_INVALIDATE,
	TEST_HTTP_CLIENT_CACHE_RANGE,
	TEST_HTTP_CLIENT_CACHE_RANGE_PARTIAL,
	TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART,
	TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_COMPLETE,
	TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_INVALID,
	TEST_HTTP_CLIENT_CACHE_HEAD_FRESH,
	TEST_HTTP_CLIENT_CACHE_HEAD_VALIDATE,
	TEST_HTTP_CLIENT_CACHE_HEAD_UPDATE,
	TEST_HTTP_CLIENT_CACHE_HEAD_INVALIDATE
} test_http_client_cache_scenario;



/* 自定义后端可以在指定操作点稳定注入 I/O 失败。 */
typedef enum test_http_client_cache_fault {
	TEST_HTTP_CLIENT_CACHE_FAULT_NONE = 0,
	TEST_HTTP_CLIENT_CACHE_FAULT_GET,
	TEST_HTTP_CLIENT_CACHE_FAULT_PUT,
	TEST_HTTP_CLIENT_CACHE_FAULT_REMOVE_URI,
	TEST_HTTP_CLIENT_CACHE_FAULT_REPLACE_CONFLICT
} test_http_client_cache_fault;



/* 故障后端把未注入的操作委托给真实内存缓存。 */
typedef struct test_http_client_cache_backend {
	xhttpcache* Memory;
	test_http_client_cache_fault Fault;
	size_t Gets;
	size_t Puts;
	size_t Removes;
	size_t Conflicts;
	bool Closed;
} test_http_client_cache_backend;



typedef struct test_http_client_cache {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpcache* Cache;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xhttpcallinfo Info;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_http_client_cache_scenario Scenario;
	size_t Requests;
	xerrkind FailureKind;
	int32 FailureCode;
	uint8 StreamBody[64];
	size_t StreamSize;
	bool ExpectFailure;
	bool RequestTrailer;
	bool Stream;
} test_http_client_cache;



/* 描述客户端缓存应在本地生成的 multipart/byteranges 正文。 */
typedef struct test_http_client_cache_multipart {
	cstr sCompleteBody;
	const xhttpbyterange* pRanges;
	size_t RangeCount;
} test_http_client_cache_multipart;



/* 描述 HEAD 响应必须保留或移除的字段与 Trailer。 */
typedef struct test_http_client_cache_head {
	cstr sLength;
	cstr sMetadata;
	bool NoTrailers;
} test_http_client_cache_head;



/* 为动态响应字段和正文提供附加检查。 */
typedef void (*test_http_client_cache_response_check)(
	const xhttpresponse* pResponse,
	const void* pData
);



/* 为流式一致性测试留下一个会被下一次 Body 回调清除的线程错误。 */
static bool testHttpClientCacheStreamHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	(void)pCall;
	(void)pResponse;
	(void)pData;
	__xrtErrorSetRange();
	return true;
}



/* 验证网络响应与缓存重放对计数、错误和正文块采用同一回调契约。 */
static bool testHttpClientCacheStreamBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;

	(void)pCall;
	testRequire(
		(xrtGetError() == NULL) &&
		(xrtHttpResponseWireBodyBytes(pResponse) ==
		 (uint64)(pState->StreamSize + Data.Size)) &&
		(xrtHttpResponseBodyBytes(pResponse) ==
		 (uint64)pState->StreamSize) &&
		(Data.Size <=
		 (sizeof(pState->StreamBody) - pState->StreamSize)),
		"HTTP cache streamed callback contract changed"
	);
	memcpy(
		pState->StreamBody + pState->StreamSize,
		Data.Data,
		Data.Size
	);
	pState->StreamSize += Data.Size;
	return true;
}



/* 为自定义缓存后端发布一个可被高层原因链保留的 I/O 错误。 */
static void testHttpClientCacheBackendError(cstr sOperation)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO,
		"test.http.cache.backend",
		1,
		sOperation
	);

	testRequire(
		pError != NULL,
		"HTTP cache backend error creation failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 查询时按故障开关失败，否则委托给内存缓存。 */
static xhttpcachelookup testHttpClientCacheBackendGet(
	ptr pContext,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	pBackend->Gets++;
	if ( pBackend->Fault ==
		TEST_HTTP_CLIENT_CACHE_FAULT_GET ) {
		*ppRecord = NULL;
		testHttpClientCacheBackendError(
			"forced cache lookup failure"
		);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	return xrtHttpCacheGet(
		pBackend->Memory,
		pKey,
		ppRecord
	);
}



/* 保存时按故障开关失败，否则委托给内存缓存。 */
static xhttpcacheput testHttpClientCacheBackendPut(
	ptr pContext,
	xhttpcacherecord* pRecord
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	pBackend->Puts++;
	if ( pBackend->Fault ==
		TEST_HTTP_CLIENT_CACHE_FAULT_PUT ) {
		testHttpClientCacheBackendError(
			"forced cache store failure"
		);
		return XHTTP_CACHE_PUT_ERROR;
	}
	return xrtHttpCachePut(
		pBackend->Memory,
		pRecord
	);
}



/* 条件插入保留并发冲突，同时复用保存故障开关。 */
static xhttpcacheput testHttpClientCacheBackendInsert(
	ptr pContext,
	xhttpcacherecord* pRecord
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	pBackend->Puts++;
	if ( pBackend->Fault ==
		TEST_HTTP_CLIENT_CACHE_FAULT_PUT ) {
		testHttpClientCacheBackendError(
			"forced cache insert failure"
		);
		return XHTTP_CACHE_PUT_ERROR;
	}
	return xrtHttpCacheInsert(
		pBackend->Memory,
		pRecord
	);
}



/* 复制并发提交记录，并加入必须跨重试保留的响应元数据。 */
static xhttpcacherecord* testHttpClientCacheBackendConcurrent(
	const xhttpcacherecord* pSource
)
{
	const xhttpcachekey* pKey =
		xrtHttpCacheRecordKey(pSource);
	size_t iFieldCount =
		xrtHttpCacheRecordFieldCount(pSource);
	size_t iTrailerCount =
		xrtHttpCacheRecordTrailerCount(pSource);
	size_t iPartCount =
		xrtHttpCacheRecordPartCount(pSource);
	xhttpfield* pFields = NULL;
	xhttpfield* pTrailers = NULL;
	xhttpcachepart* pParts = NULL;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord = NULL;
	size_t i;

	if ( (pKey == NULL) ||
		(iFieldCount == SIZE_MAX) ||
		((iFieldCount + 1u) >
		 (SIZE_MAX / sizeof(*pFields))) ||
		(iTrailerCount >
		 (SIZE_MAX / sizeof(*pTrailers))) ||
		(iPartCount > (SIZE_MAX / sizeof(*pParts))) ) {
		return NULL;
	}
	pFields = (xhttpfield*)xrtMalloc(
		(iFieldCount + 1u) * sizeof(*pFields)
	);
	if ( pFields == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		const xhttpfield* pField =
			xrtHttpCacheRecordFieldAt(pSource, i);

		if ( pField == NULL ) {
			goto cleanup;
		}
		pFields[i] = *pField;
	}
	pFields[iFieldCount].Name =
		XRT_STR_LITERAL("X-Metadata");
	pFields[iFieldCount].Value =
		XRT_STR_LITERAL("concurrent");

	if ( iTrailerCount != 0 ) {
		pTrailers = (xhttpfield*)xrtMalloc(
			iTrailerCount * sizeof(*pTrailers)
		);
		if ( pTrailers == NULL ) {
			goto cleanup;
		}
		for ( i = 0; i < iTrailerCount; i++ ) {
			const xhttpfield* pTrailer =
				xrtHttpCacheRecordTrailerAt(
					pSource,
					i
				);

			if ( pTrailer == NULL ) {
				goto cleanup;
			}
			pTrailers[i] = *pTrailer;
		}
	}
	if ( iPartCount != 0 ) {
		pParts = (xhttpcachepart*)xrtMalloc(
			iPartCount * sizeof(*pParts)
		);
		if ( pParts == NULL ) {
			goto cleanup;
		}
		for ( i = 0; i < iPartCount; i++ ) {
			const xhttpcachepart* pPart =
				xrtHttpCacheRecordPartAt(
					pSource,
					i
				);

			if ( pPart == NULL ) {
				goto cleanup;
			}
			pParts[i] = *pPart;
		}
	}
	if ( !xrtHttpCacheRecordInputInit(
		&Input,
		pKey,
		xrtHttpCacheRecordStatus(pSource)
	) ) {
		goto cleanup;
	}
	Input.Version = xrtHttpCacheRecordVersion(pSource);
	Input.Flags = xrtHttpCacheRecordFlags(pSource);
	Input.Reason = xrtHttpCacheRecordReason(pSource);
	Input.Fields = pFields;
	Input.FieldCount = iFieldCount + 1u;
	Input.Trailers = pTrailers;
	Input.TrailerCount = iTrailerCount;
	Input.Parts = pParts;
	Input.PartCount = iPartCount;
	Input.Length = xrtHttpCacheRecordLength(pSource);
	Input.ResponseTime =
		xrtHttpCacheRecordResponseTime(pSource);
	Input.RequestClock =
		xrtHttpCacheRecordRequestClock(pSource);
	Input.ResponseClock =
		xrtHttpCacheRecordResponseClock(pSource);
	pRecord = xrtHttpCacheRecordCreate(&Input);

cleanup:
	xrtFree(pParts);
	xrtFree(pTrailers);
	xrtFree(pFields);
	return pRecord;
}



/* 条件替换委托给内存缓存，并可注入一次真实并发冲突。 */
static xhttpcacheput testHttpClientCacheBackendReplace(
	ptr pContext,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	pBackend->Puts++;
	if ( pBackend->Fault ==
		TEST_HTTP_CLIENT_CACHE_FAULT_PUT ) {
		testHttpClientCacheBackendError(
			"forced cache replace failure"
		);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (pBackend->Fault ==
		  TEST_HTTP_CLIENT_CACHE_FAULT_REPLACE_CONFLICT) &&
		(pBackend->Conflicts == 0) ) {
		xhttpcacherecord* pConcurrent =
			testHttpClientCacheBackendConcurrent(
				pReplacement
			);
		xhttpcacheput Put;

		if ( pConcurrent == NULL ) {
			return XHTTP_CACHE_PUT_ERROR;
		}
		Put = xrtHttpCachePut(
			pBackend->Memory,
			pConcurrent
		);
		xrtHttpCacheRecordRelease(pConcurrent);
		if ( (Put != XHTTP_CACHE_PUT_STORED) &&
			(Put != XHTTP_CACHE_PUT_REPLACED) ) {
			return XHTTP_CACHE_PUT_ERROR;
		}
		pBackend->Conflicts++;
	}
	return xrtHttpCacheReplace(
		pBackend->Memory,
		pExpected,
		pReplacement
	);
}



/* 按 Record 身份删除直接保留底层条件冲突语义。 */
static xhttpcachechange
testHttpClientCacheBackendRemoveRecord(
	ptr pContext,
	const xhttpcacherecord* pExpected
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	return xrtHttpCacheRemoveRecord(
		pBackend->Memory,
		pExpected
	);
}



/* 精确删除始终委托，当前高层客户端只使用 URI 失效。 */
static bool testHttpClientCacheBackendRemove(
	ptr pContext,
	const xhttpcachekey* pKey,
	size_t* pRemoved
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	return xrtHttpCacheRemove(
		pBackend->Memory,
		pKey,
		pRemoved
	);
}



/* URI 失效时按故障开关失败，否则委托给内存缓存。 */
static bool testHttpClientCacheBackendRemoveURI(
	ptr pContext,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	pBackend->Removes++;
	if ( pBackend->Fault ==
		TEST_HTTP_CLIENT_CACHE_FAULT_REMOVE_URI ) {
		*pRemoved = 0;
		testHttpClientCacheBackendError(
			"forced cache invalidation failure"
		);
		return false;
	}
	return xrtHttpCacheRemoveURI(
		pBackend->Memory,
		URI,
		Partition,
		pRemoved
	);
}



/* 清空未参与客户端热路径，仍保持完整后端契约。 */
static bool testHttpClientCacheBackendClear(ptr pContext)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	return xrtHttpCacheClear(pBackend->Memory);
}



/* 统计未参与故障判断，直接返回内存缓存快照。 */
static bool testHttpClientCacheBackendStats(
	ptr pContext,
	xhttpcachestats* pStats
)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	return xrtHttpCacheStats(
		pBackend->Memory,
		pStats
	);
}



/* 最后一个统一句柄引用释放时关闭后端资源。 */
static void testHttpClientCacheBackendClose(ptr pContext)
{
	test_http_client_cache_backend* pBackend =
		(test_http_client_cache_backend*)pContext;

	testRequire(
		!pBackend->Closed,
		"HTTP client cache backend closed twice"
	);
	pBackend->Closed = true;
	xrtHttpCacheRelease(pBackend->Memory);
	pBackend->Memory = NULL;
}



/* 创建完整的故障后端操作表。 */
static xhttpcache* testHttpClientCacheBackendOpen(
	test_http_client_cache_backend* pBackend
)
{
	xhttpcacheops Ops;

	memset(pBackend, 0, sizeof(*pBackend));
	memset(&Ops, 0, sizeof(Ops));
	pBackend->Memory = xrtHttpCacheCreate(NULL);
	if ( pBackend->Memory == NULL ) {
		return NULL;
	}
	Ops.Get = testHttpClientCacheBackendGet;
	Ops.Put = testHttpClientCacheBackendPut;
	Ops.Insert = testHttpClientCacheBackendInsert;
	Ops.Replace = testHttpClientCacheBackendReplace;
	Ops.RemoveRecord =
		testHttpClientCacheBackendRemoveRecord;
	Ops.Remove = testHttpClientCacheBackendRemove;
	Ops.RemoveURI = testHttpClientCacheBackendRemoveURI;
	Ops.Clear = testHttpClientCacheBackendClear;
	Ops.Stats = testHttpClientCacheBackendStats;
	Ops.Close = testHttpClientCacheBackendClose;
	return xrtHttpCacheOpen(&Ops, pBackend);
}



/* 等待异步计数达到目标值，并给测试夹具设置硬截止时间。 */
static void testHttpClientCacheWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

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



/* 为固定测试域名返回本机 IPv4，隔离系统 DNS 与外部网络。 */
static xnetaddrlist* testHttpClientCacheLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "cache.test") == 0,
		"HTTP cache resolved an unexpected host"
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
		"HTTP cache resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 接收客户端关闭方向后完成服务端 Stream 的正常关闭。 */
static void testHttpClientCacheServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP cache server half-close failed"
	);
}



/* 记录一个服务端连接已经释放底层套接字。 */
static void testHttpClientCacheServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 按场景生成可缓存、可验证或带 Vary 的源站响应。 */
static void testHttpClientCacheServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;
	char Request[2048];
	char Response[512];
	cstr sBody = "OK";
	cstr sControl = "max-age=3600";
	cstr sExtra = "";
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t i;
	int iLength;
	bool bGet;
	bool bHead;
	bool bPost;
	bool bComplete = false;

	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTP cache request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP cache request peek failed"
	);
	Request[iSize] = '\0';
	for ( i = 3; i < iSize; i++ ) {
		if ( (Request[i - 3] == '\r') &&
			(Request[i - 2] == '\n') &&
			(Request[i - 1] == '\r') &&
			(Request[i] == '\n') ) {
			bComplete = true;
			break;
		}
	}
	if ( !bComplete ) {
		return;
	}
	bGet = memcmp(Request, "GET /", 5) == 0;
	bHead = memcmp(Request, "HEAD /", 6) == 0;
	bPost = memcmp(Request, "POST /", 6) == 0;
	testRequire(
		bGet || bHead || bPost,
		"HTTP cache emitted an unexpected method or target"
	);
	testRequire(
		strstr(Request, "\r\nHost: cache.test:") != NULL,
		"HTTP cache request omitted its effective Host"
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( pState->RequestTrailer ) {
			testRequire(
				(strstr(
					Request,
					"\r\nTransfer-Encoding: chunked\r\n"
				) != NULL) && (strstr(
					Request,
					"\r\nTrailer: X-Cache-Probe\r\n"
				) != NULL),
				"HTTP cache request Trailer did not reach origin"
			);
		}
	#endif
	if ( strstr(Request, "\r\nRange:") != NULL ) {
		testRequire(
			strstr(
				Request,
				"\r\nAccept-Encoding:"
			) == NULL,
			"HTTP Range request negotiated implicit encoding"
		);
	}
	testRequire(
		xrtNetBufConsume(pBuffer, iSize) == iSize,
		"HTTP cache request consume failed"
	);
	pState->Requests++;

	if ( (pState->Scenario ==
		  TEST_HTTP_CLIENT_CACHE_INVALIDATE) &&
		bPost ) {
		testRequire(
			memcmp(
				Request,
				"POST /invalidate HTTP/1.1\r\n",
				27
			) == 0,
			"HTTP cache invalidation used the wrong target"
		);
		iLength = snprintf(
			Response,
			sizeof(Response),
			"HTTP/1.1 204 No Content\r\n"
			"Location: /related\r\n"
			"Connection: close\r\n"
			"\r\n"
		);
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Response)),
			"HTTP cache invalidation fixture overflowed"
		);
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				(size_t)iLength
			) == XNET_RESULT_OK,
			"HTTP cache invalidation response send failed"
		);
		return;
	}
	testRequire(
		bGet || bHead,
		"HTTP cache fixture received an unexpected unsafe request"
	);
	if ( pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_HEAD_FRESH ) {
		testRequire(
			bGet,
			"fresh HEAD cache fixture unexpectedly reached the origin"
		);
		if ( strstr(
			Request,
			"GET /head-trailer HTTP/1.1\r\n"
		) != NULL ) {
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 200 OK\r\n"
				"Cache-Control: max-age=3600\r\n"
				"Transfer-Encoding: chunked\r\n"
				"Trailer: X-Checksum\r\n"
				"Connection: close\r\n"
				"\r\n"
				"4\r\n"
				"DATA\r\n"
				"0\r\n"
				"X-Checksum: good\r\n"
				"\r\n"
			);
			testRequire(
				(iLength > 0) &&
				((size_t)iLength < sizeof(Response)),
				"HTTP HEAD trailer fixture overflowed"
			);
			testRequire(
				xrtNetStreamSend(
					pStream,
					Response,
					(size_t)iLength
				) == XNET_RESULT_OK,
				"HTTP HEAD trailer response send failed"
			);
			return;
		}
		sExtra = "ETag: \"head\"\r\n";
		sBody = "DATA";
	} else if ( pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_HEAD_VALIDATE ) {
		if ( pState->Requests == 1 ) {
			testRequire(
				bGet,
				"HTTP HEAD validation seed was not GET"
			);
			sControl = "max-age=0";
			sExtra = "ETag: \"head-v1\"\r\n";
			sBody = "OLD";
		} else {
			testRequire(
				bHead &&
				(strstr(
					Request,
					"\r\nIf-None-Match: \"head-v1\"\r\n"
				 ) != NULL),
				"HTTP HEAD validation omitted its GET validator"
			);
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 304 Not Modified\r\n"
				"Cache-Control: max-age=3600\r\n"
				"ETag: \"head-v1\"\r\n"
				"Connection: close\r\n"
				"\r\n"
			);
			testRequire(
				(iLength > 0) &&
				((size_t)iLength < sizeof(Response)),
				"HTTP HEAD 304 fixture overflowed"
			);
			testRequire(
				xrtNetStreamSend(
					pStream,
					Response,
					(size_t)iLength
				) == XNET_RESULT_OK,
				"HTTP HEAD 304 response send failed"
			);
			return;
		}
	} else if (
		(pState->Scenario ==
		 TEST_HTTP_CLIENT_CACHE_HEAD_UPDATE) ||
		(pState->Scenario ==
		 TEST_HTTP_CLIENT_CACHE_HEAD_INVALIDATE)
	) {
		if ( pState->Requests == 1 ) {
			testRequire(
				bGet,
				"HTTP HEAD freshening seed was not GET"
			);
			sControl = "max-age=0";
			sExtra = "X-Metadata: old\r\n";
			sBody = "OLD";
		} else {
			cstr sLength =
				(pState->Scenario ==
				 TEST_HTTP_CLIENT_CACHE_HEAD_UPDATE) ?
					"3" :
					"4";

			testRequire(
				bHead &&
				(strstr(
					Request,
					"\r\nIf-None-Match:"
				 ) == NULL) &&
				(strstr(
					Request,
					"\r\nIf-Modified-Since:"
				 ) == NULL),
				"HTTP HEAD freshening unexpectedly used a validator"
			);
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 200 OK\r\n"
				"Cache-Control: max-age=3600\r\n"
				"X-Metadata: new\r\n"
				"Content-Length: %s\r\n"
				"Connection: close\r\n"
				"\r\n",
				sLength
			);
			testRequire(
				(iLength > 0) &&
				((size_t)iLength < sizeof(Response)),
				"HTTP HEAD freshening fixture overflowed"
			);
			testRequire(
				xrtNetStreamSend(
					pStream,
					Response,
					(size_t)iLength
				) == XNET_RESULT_OK,
				"HTTP HEAD freshening response send failed"
			);
			return;
		}
	} else if ( pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_VALIDATE ) {
		if ( pState->Requests == 1 ) {
			sControl = "max-age=0";
			sExtra = "ETag: \"v1\"\r\n";
			sBody = "OLD";
		} else {
			testRequire(
				strstr(
					Request,
					"\r\nIf-None-Match: \"v1\"\r\n"
				) != NULL,
				"HTTP cache validation omitted If-None-Match"
			);
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 304 Not Modified\r\n"
				"Cache-Control: max-age=3600\r\n"
				"ETag: \"v1\"\r\n"
				"Connection: close\r\n"
				"\r\n"
			);
			testRequire(
				(iLength > 0) &&
				((size_t)iLength < sizeof(Response)),
				"HTTP cache 304 fixture overflowed"
			);
			testRequire(
				xrtNetStreamSend(
					pStream,
					Response,
					(size_t)iLength
				) == XNET_RESULT_OK,
				"HTTP cache 304 response send failed"
			);
			return;
		}
	} else if (
		pState->Scenario == TEST_HTTP_CLIENT_CACHE_VARY
	) {
		sExtra = "Vary: X-Flavor\r\n";
		if ( strstr(
			Request,
			"\r\nX-Flavor: blue\r\n"
		) != NULL ) {
			sBody = "BLUE";
		} else {
			testRequire(
				strstr(
					Request,
					"\r\nX-Flavor: red\r\n"
				) != NULL,
				"HTTP cache Vary request omitted its selector"
			);
			sBody = "RED";
		}
	} else if (
		pState->Scenario == TEST_HTTP_CLIENT_CACHE_RANGE
	) {
		sExtra =
			"ETag: \"asset\"\r\n"
			"Content-Encoding: identity\r\n";
		sBody = "0123456789";
	} else if (
		pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_RANGE_PARTIAL
	) {
		cstr sContentRange;

		if ( strstr(
			Request,
			"\r\nRange: bytes=0-4\r\n"
		) != NULL ) {
			sContentRange = "bytes 0-4/10";
			sBody = "01234";
		} else {
			testRequire(
				strstr(
					Request,
					"\r\nRange: bytes=5-9\r\n"
				) != NULL,
				"HTTP cache range fill used the wrong range"
			);
			testRequire(
				strstr(
					Request,
					"\r\nIf-Range: \"asset\"\r\n"
				) != NULL,
				"HTTP cache range fill omitted strong If-Range"
			);
			sContentRange = "bytes 5-9/10";
			sBody = "56789";
		}
		iLength = snprintf(
			Response,
			sizeof(Response),
			"HTTP/1.1 206 Partial Content\r\n"
			"Cache-Control: max-age=3600\r\n"
			"ETag: \"asset\"\r\n"
			"Content-Range: %s\r\n"
			"Content-Length: %u\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			sContentRange,
			(unsigned int)strlen(sBody),
			sBody
		);
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Response)),
			"HTTP cache partial response fixture overflowed"
		);
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				(size_t)iLength
			) == XNET_RESULT_OK,
			"HTTP cache partial response send failed"
		);
		return;
	} else if (
		pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART
	) {
		if ( pState->Requests == 1 ) {
			testRequire(
				strstr(
					Request,
					"\r\nRange: bytes=2-7\r\n"
				) != NULL,
				"HTTP cache multipart seed used the wrong range"
			);
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 206 Partial Content\r\n"
				"Cache-Control: max-age=3600\r\n"
				"ETag: \"asset\"\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Range: bytes 2-7/10\r\n"
				"Content-Length: 6\r\n"
				"Connection: close\r\n"
				"\r\n"
				"234567"
			);
		} else {
			static const char Multipart[] =
				"--origin\r\n"
				"Content-Range: bytes 8-9/10\r\n"
				"Content-Type: text/plain\r\n"
				"\r\n"
				"89\r\n"
				"--origin\r\n"
				"Content-Range: bytes 0-1/10\r\n"
				"Content-Type: text/plain\r\n"
				"\r\n"
				"01\r\n"
				"--origin--\r\n";

			testRequire(
				(pState->Requests == 2) &&
				(strstr(
					Request,
					"\r\nRange: bytes=0-1,8-9\r\n"
				 ) != NULL),
				"HTTP cache multipart fill request mismatch"
			);
			iLength = snprintf(
				Response,
				sizeof(Response),
				"HTTP/1.1 206 Partial Content\r\n"
				"Cache-Control: max-age=3600\r\n"
				"ETag: \"asset\"\r\n"
				"Content-Type: multipart/byteranges; "
					"boundary=origin\r\n"
				"Content-Length: %u\r\n"
				"Connection: close\r\n"
				"\r\n"
				"%s",
				(unsigned int)(
					sizeof(Multipart) - 1u
				),
				Multipart
			);
		}
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Response)),
			"HTTP cache multipart origin fixture overflowed"
		);
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				(size_t)iLength
			) == XNET_RESULT_OK,
			"HTTP cache multipart origin response send failed"
		);
		return;
	} else if (
		pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_COMPLETE
	) {
		static const char Multipart[] =
			"--complete\r\n"
			"Content-Range: bytes 5-9/10\r\n"
			"Content-Type: text/plain\r\n"
			"\r\n"
			"56789\r\n"
			"--complete\r\n"
			"Content-Range: bytes 0-4/10\r\n"
			"Content-Type: text/plain\r\n"
			"\r\n"
			"01234\r\n"
			"--complete--\r\n";

		testRequire(
			strstr(
				Request,
				"\r\nRange: bytes=0-4,5-9\r\n"
			) != NULL,
			"HTTP cache complete multipart used the wrong range"
		);
		iLength = snprintf(
			Response,
			sizeof(Response),
			"HTTP/1.1 206 Partial Content\r\n"
			"Cache-Control: max-age=3600\r\n"
			"ETag: \"asset\"\r\n"
			"Content-Type: multipart/byteranges; "
				"boundary=complete\r\n"
			"Content-Length: %u\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			(unsigned int)(sizeof(Multipart) - 1u),
			Multipart
		);
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Response)),
			"HTTP cache complete multipart fixture overflowed"
		);
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				(size_t)iLength
			) == XNET_RESULT_OK,
			"HTTP cache complete multipart response send failed"
		);
		return;
	} else if (
		pState->Scenario ==
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_INVALID
	) {
		static const char Multipart[] =
			"--broken\r\n"
			"Content-Range: bytes 0-1/10\r\n"
			"\r\n"
			"01\r\n"
			"--broken\r\n";

		testRequire(
			strstr(
				Request,
				"\r\nRange: bytes=0-1\r\n"
			) != NULL,
			"HTTP cache malformed multipart used the wrong range"
		);
		iLength = snprintf(
			Response,
			sizeof(Response),
			"HTTP/1.1 206 Partial Content\r\n"
			"Cache-Control: max-age=3600\r\n"
			"ETag: \"asset\"\r\n"
			"Content-Type: multipart/byteranges; "
				"boundary=broken\r\n"
			"Content-Length: %u\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			(unsigned int)(sizeof(Multipart) - 1u),
			Multipart
		);
		testRequire(
			(iLength > 0) &&
			((size_t)iLength < sizeof(Response)),
			"HTTP cache malformed multipart fixture overflowed"
		);
		testRequire(
			xrtNetStreamSend(
				pStream,
				Response,
				(size_t)iLength
			) == XNET_RESULT_OK,
			"HTTP cache malformed multipart response send failed"
		);
		return;
	}
	iLength = snprintf(
		Response,
		sizeof(Response),
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: %s\r\n"
		"%s"
		"Content-Length: %u\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		sControl,
		sExtra,
		(unsigned int)strlen(sBody),
		sBody
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Response)),
		"HTTP cache response fixture overflowed"
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			(size_t)iLength
		) == XNET_RESULT_OK,
		"HTTP cache response send failed"
	);
}



/* 接管 Listener 交付的连接并安装 HTTP 测试处理器。 */
static bool testHttpClientCacheAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpClientCacheServerRead;
	Events.End = testHttpClientCacheServerEnd;
	Events.Close = testHttpClientCacheServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"HTTP cache server event takeover failed"
	);
	pState->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已完成关闭。 */
static void testHttpClientCacheListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管成功响应并冻结一次调用的诊断快照。 */
static void testHttpClientCacheDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_cache* pState =
		(test_http_client_cache*)pData;

	if ( pState->ExpectFailure ) {
		testRequire(
			(pCall != NULL) &&
			(pCall->CacheCandidate == NULL) &&
			(pCall->CacheRequestFields == NULL) &&
			(pCall->CacheResponseFields == NULL) &&
			(pCall->CacheBody == NULL) &&
			(pCall->CacheRanges == NULL) &&
			(pCall->CachePartitionKey == NULL) &&
			(pResult != NULL) &&
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(pResult->Tcp == NULL) &&
			!pResult->Upgraded &&
			xrtHttpCallInfo(pCall, &pState->Info),
			"HTTP cache strict backend call did not fail cleanly"
		);
		pState->FailureKind =
			xrtErrorKind(pResult->Error);
		pState->FailureCode =
			xrtErrorCode(pResult->Error);
		(void)xrtAtomic32FetchAdd(
			&pState->Completed,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( (pResult == NULL) ||
		(pResult->Result != XNET_RESULT_OK) ) {
		const xerror* pError =
			pResult != NULL ? pResult->Error : NULL;

		fprintf(
			stderr,
			"[HTTP CACHE DEBUG] scenario=%d requests=%u result=%d kind=%d code=%d operation=%s message=%s\n",
			(int)pState->Scenario,
			(unsigned int)pState->Requests,
			pResult != NULL ? (int)pResult->Result : -1,
			(int)xrtErrorKind(pError),
			(int)xrtErrorCode(pError),
			xrtErrorOperation(pError) != NULL ?
				xrtErrorOperation(pError) : "",
			xrtErrorMessage(pError) != NULL ?
				xrtErrorMessage(pError) : ""
		);
	}
	testRequire(
		(pCall != NULL) &&
		(pCall->CacheCandidate == NULL) &&
		(pCall->CacheRequestFields == NULL) &&
		(pCall->CacheResponseFields == NULL) &&
		(pCall->CacheBody == NULL) &&
		(pCall->CacheRanges == NULL) &&
		(pCall->CachePartitionKey == NULL) &&
		((pState->Call == NULL) ||
		 (pCall == pState->Call)) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Error == NULL) &&
		(pResult->Tcp == NULL) &&
		!pResult->Upgraded &&
		xrtHttpCallInfo(pCall, &pState->Info),
		"HTTP cache call did not complete successfully"
	);
	pState->Response = pResult->Response;
	(void)xrtAtomic32FetchAdd(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立一个共享真实网络、Client 与有界内存缓存的场景。 */
static void testHttpClientCacheOpen(
	test_http_client_cache* pState,
	test_http_client_cache_scenario Scenario,
	xnetaddr* pAddress,
	xhttpcache* pStore,
	bool bStrict
)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;

	memset(pState, 0, sizeof(*pState));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&pState->Accepted, 0);
	xrtAtomic32Init(&pState->Completed, 0);
	xrtAtomic32Init(&pState->ServerClosed, 0);
	xrtAtomic32Init(&pState->ListenerClosed, 0);
	pState->Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pState->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pState->Engine != NULL) &&
		xrtNetEngineStart(pState->Engine),
		"HTTP cache engine start failed"
	);

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP cache listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpClientCacheAccept;
	ListenerEvents.Close = testHttpClientCacheListenerClose;
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
			pAddress
		),
		"HTTP cache listener creation failed"
	);

	pState->Cache = pStore != NULL ?
		xrtHttpCacheRetain(pStore) :
		xrtHttpCacheCreate(NULL);
	testRequire(
		pState->Cache != NULL,
		"HTTP cache store creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpClientCacheLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Cache.Store = pState->Cache;
	ClientConfig.Cache.Strict = bStrict;
	pState->Client = xrtHttpClientCreate(
		pState->Engine,
		&ClientConfig
	);
	testRequire(
		(pState->Client != NULL) &&
		(xrtHttpClientCache(pState->Client) ==
		 pState->Cache),
		"HTTP cache client creation failed"
	);
}



/* 关闭场景并验证所有异步网络对象已经排空。 */
static void testHttpClientCacheClose(
	test_http_client_cache* pState
)
{
	testRequire(
		xrtNetListenerClose(pState->Listener),
		"HTTP cache listener close failed"
	);
	testHttpClientCacheWait(
		&pState->ListenerClosed,
		1,
		"HTTP cache listener did not close"
	);
	xrtHttpClientDestroy(pState->Client);
	xrtHttpCacheRelease(pState->Cache);
	xrtNetListenerDestroy(pState->Listener);
	testRequire(
		xrtNetEngineDestroy(pState->Engine),
		"HTTP cache engine destroy failed"
	);
}



/* 验证缓存生成的 multipart/byteranges 字段和完整线缆正文。 */
static void testHttpClientCacheMultipartCheck(
	const xhttpresponse* pResponse,
	const void* pData
)
{
	static const char sTypePrefix[] =
		"multipart/byteranges; boundary=";
	static const char sBoundaryPrefix[] = "xrt-cache-";
	const test_http_client_cache_multipart* pExpected =
		(const test_http_client_cache_multipart*)pData;
	const xhttpfield* pType = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Type")
	);
	const xhttpfield* pLength = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Length")
	);
	const xhttpfield* pContentRange = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Range")
	);
	xbytesview Body = xrtHttpResponseBody(pResponse);
	xstrview Boundary;
	char sExpected[1024];
	char sLength[32];
	size_t iExpected = 0;
	size_t iCompleteLength = strlen(pExpected->sCompleteBody);
	size_t i;
	int iWritten;

	testRequire(
		(pType != NULL) &&
		(pType->Value.Size ==
		 (sizeof(sTypePrefix) - 1u) + 42u) &&
		(memcmp(
			pType->Value.Data,
			sTypePrefix,
			sizeof(sTypePrefix) - 1u
		 ) == 0),
		"HTTP cache multipart Content-Type mismatch"
	);
	Boundary.Data =
		pType->Value.Data + sizeof(sTypePrefix) - 1u;
	Boundary.Size =
		pType->Value.Size - (sizeof(sTypePrefix) - 1u);
	testRequire(
		(Boundary.Size == 42u) &&
		(memcmp(
			Boundary.Data,
			sBoundaryPrefix,
			sizeof(sBoundaryPrefix) - 1u
		 ) == 0),
		"HTTP cache multipart boundary prefix mismatch"
	);
	for ( i = sizeof(sBoundaryPrefix) - 1u;
		i < Boundary.Size;
		i++ ) {
		char c = Boundary.Data[i];

		testRequire(
			((c >= '0') && (c <= '9')) ||
			((c >= 'a') && (c <= 'f')),
			"HTTP cache multipart boundary is not lowercase hex"
		);
	}

	for ( i = 0; i < pExpected->RangeCount; i++ ) {
		const xhttpbyterange* pRange =
			&pExpected->pRanges[i];
		size_t iPayload;

		testRequire(
			(pRange->First <= pRange->Last) &&
			(pRange->Last < (uint64)iCompleteLength),
			"HTTP cache multipart expected range is invalid"
		);
		iWritten = snprintf(
			sExpected + iExpected,
			sizeof(sExpected) - iExpected,
			"--%.*s\r\n"
			"Content-Type: application/octet-stream\r\n"
			"Content-Range: bytes %llu-%llu/%llu\r\n"
			"\r\n",
			(int)Boundary.Size,
			Boundary.Data,
			(unsigned long long)pRange->First,
			(unsigned long long)pRange->Last,
			(unsigned long long)iCompleteLength
		);
		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten <
			 (sizeof(sExpected) - iExpected)),
			"HTTP cache multipart expected head overflowed"
		);
		iExpected += (size_t)iWritten;
		iPayload = (size_t)(
			pRange->Last - pRange->First + 1u
		);
		testRequire(
			(iPayload <=
			 (sizeof(sExpected) - iExpected - 2u)),
			"HTTP cache multipart expected body overflowed"
		);
		memcpy(
			sExpected + iExpected,
			pExpected->sCompleteBody +
				(size_t)pRange->First,
			iPayload
		);
		iExpected += iPayload;
		memcpy(sExpected + iExpected, "\r\n", 2u);
		iExpected += 2u;
	}
	iWritten = snprintf(
		sExpected + iExpected,
		sizeof(sExpected) - iExpected,
		"--%.*s--\r\n",
		(int)Boundary.Size,
		Boundary.Data
	);
	testRequire(
		(iWritten > 0) &&
		((size_t)iWritten <
		 (sizeof(sExpected) - iExpected)),
		"HTTP cache multipart expected close overflowed"
	);
	iExpected += (size_t)iWritten;

	iWritten = snprintf(
		sLength,
		sizeof(sLength),
		"%llu",
		(unsigned long long)iExpected
	);
	testRequire(
		(iWritten > 0) &&
		((size_t)iWritten < sizeof(sLength)) &&
		(pLength != NULL) &&
		(pLength->Value.Size == (size_t)iWritten) &&
		(memcmp(
			pLength->Value.Data,
			sLength,
			(size_t)iWritten
		 ) == 0) &&
		(pContentRange == NULL) &&
		(Body.Size == iExpected) &&
		(memcmp(Body.Data, sExpected, iExpected) == 0),
		"HTTP cache multipart wire response mismatch"
	);
}



/* 验证 multipart framing 已恢复为原表示的字段。 */
static void testHttpClientCacheRepresentationCheck(
	const xhttpresponse* pResponse,
	const void* pData
)
{
	const xhttpfield* pType = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Type")
	);
	const xhttpfield* pLength = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Length")
	);
	const xhttpfield* pRange = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Range")
	);

	(void)pData;
	testRequire(
		(pType != NULL) &&
		(pType->Value.Size == 10) &&
		(memcmp(
			pType->Value.Data,
			"text/plain",
			10
		 ) == 0) &&
		(pLength != NULL) &&
		(pLength->Value.Size == 2) &&
		(memcmp(pLength->Value.Data, "10", 2) == 0) &&
		(pRange == NULL),
		"HTTP cache multipart representation fields mismatch"
	);
}



/* 验证 HEAD 没有正文、范围封装和缓存 GET 的 Trailer。 */
static void testHttpClientCacheHeadCheck(
	const xhttpresponse* pResponse,
	const void* pData
)
{
	const test_http_client_cache_head* pExpected =
		(const test_http_client_cache_head*)pData;
	const xhttpfield* pLength = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Length")
	);
	const xhttpfield* pRange = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Range")
	);
	const xhttpfield* pMetadata = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("X-Metadata")
	);
	xbytesview Body = xrtHttpResponseBody(pResponse);
	bool bLength = pExpected->sLength != NULL ?
		(pLength != NULL) &&
			(pLength->Value.Size ==
			 strlen(pExpected->sLength)) &&
			(memcmp(
				pLength->Value.Data,
				pExpected->sLength,
				pLength->Value.Size
			 ) == 0) :
		pLength == NULL;
	bool bMetadata = pExpected->sMetadata != NULL ?
		(pMetadata != NULL) &&
			(pMetadata->Value.Size ==
			 strlen(pExpected->sMetadata)) &&
			(memcmp(
				pMetadata->Value.Data,
				pExpected->sMetadata,
				pMetadata->Value.Size
			 ) == 0) :
		true;

	testRequire(
		bLength &&
		bMetadata &&
		(pRange == NULL) &&
		(Body.Size == 0) &&
		(!pExpected->NoTrailers ||
		 (xrtHttpResponseTrailerCount(pResponse) == 0)),
		"HTTP HEAD cache response metadata mismatch"
	);
}



/* 验证 HEAD 更新后的 GET 仍保留正文并取得新元数据。 */
static void testHttpClientCacheMetadataCheck(
	const xhttpresponse* pResponse,
	const void* pData
)
{
	cstr sExpected = (cstr)pData;
	const xhttpfield* pMetadata = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("X-Metadata")
	);

	testRequire(
		(pMetadata != NULL) &&
		(pMetadata->Value.Size == strlen(sExpected)) &&
		(memcmp(
			pMetadata->Value.Data,
			sExpected,
			pMetadata->Value.Size
		 ) == 0),
		"HTTP cache response metadata was not updated"
	);
}



/* 执行一条可附带范围条件的请求并验证来源、状态、字段和正文。 */
static void testHttpClientCacheCallRequest(
	test_http_client_cache* pState,
	cstr sMethod,
	uint16 iPort,
	cstr sPath,
	cstr sFlavor,
	cstr sPartition,
	cstr sRange,
	cstr sIfRange,
	cstr sContentRange,
	xhttpclientcachemode Mode,
	bool bNetwork,
	xhttpclientcacheoutcome Outcome,
	uint16 iStatus,
	cstr sBody,
	test_http_client_cache_response_check pCheck,
	const void* pCheckData
)
{
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	xbytesview Body;
	char Url[192];
	int iLength;
	uint32 iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);
	uint32 iCompleted = xrtAtomic32Load(
		&pState->Completed,
		XMEMORY_ACQUIRE
	);
	uint32 iClosed = xrtAtomic32Load(
		&pState->ServerClosed,
		XMEMORY_ACQUIRE
	);

	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://cache.test:%u%s",
		(unsigned int)iPort,
		sPath
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP cache request URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP cache request creation failed"
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( pState->RequestTrailer ) {
			testRequire(
				xrtHttpRequestAddTrailer(
					pRequest,
					XRT_STR_LITERAL("X-Cache-Probe"),
					XRT_STR_LITERAL("network")
				),
				"HTTP cache request Trailer setup failed"
			);
		}
	#endif
	if ( sFlavor != NULL ) {
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("X-Flavor"),
				(xstrview){
					sFlavor,
					strlen(sFlavor)
				}
			),
			"HTTP cache request selector failed"
		);
	}
	if ( sRange != NULL ) {
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Range"),
				(xstrview){
					sRange,
					strlen(sRange)
				}
			),
			"HTTP cache request Range failed"
		);
	}
	if ( sIfRange != NULL ) {
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("If-Range"),
				(xstrview){
					sIfRange,
					strlen(sIfRange)
				}
			),
			"HTTP cache request If-Range failed"
		);
	}
	xrtHttpCallOptionsInit(&Options);
	Options.Cache.Mode = Mode;
	if ( pState->Stream ) {
		Options.Events.Headers =
			testHttpClientCacheStreamHeaders;
		Options.Events.Body =
			testHttpClientCacheStreamBody;
		Options.Events.Data = pState;
	}
	if ( sPartition != NULL ) {
		Options.Cache.PartitionKey = (xstrview){
			sPartition,
			strlen(sPartition)
		};
	}
	pState->ExpectFailure = false;
	pState->Response = NULL;
	pState->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		&Options,
		testHttpClientCacheDone,
		pState
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pState->Call != NULL,
		"HTTP cache call submission failed"
	);
	testHttpClientCacheWait(
		&pState->Completed,
		iCompleted + 1u,
		"HTTP cache call did not complete"
	);
	if ( bNetwork ) {
		testHttpClientCacheWait(
			&pState->Accepted,
			iAccepted + 1u,
			"HTTP cache call did not reach the origin"
		);
		testHttpClientCacheWait(
			&pState->ServerClosed,
			iClosed + 1u,
			"HTTP cache origin connection did not close"
		);
		xrtNetStreamDestroy(pState->Server);
		pState->Server = NULL;
	} else {
		testRequire(
			xrtAtomic32Load(
				&pState->Accepted,
				XMEMORY_ACQUIRE
			) == iAccepted,
			"HTTP cache hit unexpectedly reached the origin"
		);
	}
	Body = xrtHttpResponseBody(pState->Response);
	testRequire(
		(xrtHttpResponseStatus(pState->Response) ==
		 iStatus) &&
		(pState->Info.Cache == Outcome),
		"HTTP cache response or outcome mismatch"
	);
	if ( sBody != NULL ) {
		testRequire(
			(Body.Size == strlen(sBody)) &&
			((Body.Size == 0) ||
			 (memcmp(
				Body.Data,
				sBody,
				Body.Size
			 ) == 0)),
			"HTTP cache response body mismatch"
		);
	}
	if ( sContentRange != NULL ) {
		const xhttpfield* pField = xrtHttpResponseHeader(
			pState->Response,
			XRT_STR_LITERAL("Content-Range")
		);

		testRequire(
			(pField != NULL) &&
			(pField->Value.Size ==
			 strlen(sContentRange)) &&
			(memcmp(
				pField->Value.Data,
				sContentRange,
				pField->Value.Size
			 ) == 0),
			"HTTP cache Content-Range mismatch"
		);
	}
	if ( !bNetwork ) {
		testRequire(
			(pState->Info.TransportReady == 0) &&
			(pState->Info.RequestSent == 0) &&
			(pState->Info.RequestWireBytes == 0) &&
			(pState->Info.ResponseWireBytes == 0),
			"HTTP cache hit reported network activity"
		);
	}
	if ( pCheck != NULL ) {
		pCheck(pState->Response, pCheckData);
	}
	xrtHttpResponseDestroy(pState->Response);
	pState->Response = NULL;
	xrtHttpCallDestroy(pState->Call);
	pState->Call = NULL;
}



/* 执行一次预期失败的调用，并验证同步或异步错误边界。 */
static void testHttpClientCacheCallFailure(
	test_http_client_cache* pState,
	cstr sMethod,
	uint16 iPort,
	cstr sPath,
	cstr sRange,
	xerrkind Kind,
	int32 iCode,
	uint64 iResponseBodyLimit,
	bool bSynchronous,
	bool bNetwork
)
{
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	char Url[192];
	int iLength;
	uint32 iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);
	uint32 iCompleted = xrtAtomic32Load(
		&pState->Completed,
		XMEMORY_ACQUIRE
	);
	uint32 iClosed = xrtAtomic32Load(
		&pState->ServerClosed,
		XMEMORY_ACQUIRE
	);

	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://cache.test:%u%s",
		(unsigned int)iPort,
		sPath
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP cache strict failure URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP cache strict failure request creation failed"
	);
	if ( sRange != NULL ) {
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Range"),
				(xstrview){
					sRange,
					strlen(sRange)
				}
			),
			"HTTP cache strict failure Range failed"
		);
	}
	xrtHttpCallOptionsInit(&Options);
	Options.ResponseBodyLimit = iResponseBodyLimit;
	pState->ExpectFailure = true;
	pState->FailureKind = XERR_NONE;
	pState->FailureCode = 0;
	pState->Response = NULL;
	pState->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		&Options,
		testHttpClientCacheDone,
		pState
	);
	xrtHttpRequestDestroy(pRequest);
	if ( bSynchronous ) {
		const xerror* pError = xrtGetError();

		testRequire(
			(pState->Call == NULL) &&
			(xrtAtomic32Load(
				&pState->Completed,
				XMEMORY_ACQUIRE
			 ) == iCompleted) &&
			(xrtAtomic32Load(
				&pState->Accepted,
				XMEMORY_ACQUIRE
			 ) == iAccepted) &&
			(xrtErrorKind(pError) == Kind) &&
			(xrtErrorCode(pError) == iCode),
			"HTTP cache synchronous failure contract changed"
		);
		xrtClearError();
		pState->ExpectFailure = false;
		return;
	}
	testRequire(
		pState->Call != NULL,
		"HTTP cache strict failure call submission failed"
	);
	testHttpClientCacheWait(
		&pState->Completed,
		iCompleted + 1u,
		"HTTP cache strict failure call did not complete"
	);
	if ( bNetwork ) {
		testHttpClientCacheWait(
			&pState->Accepted,
			iAccepted + 1u,
			"HTTP cache strict failure did not reach origin"
		);
		testHttpClientCacheWait(
			&pState->ServerClosed,
			iClosed + 1u,
			"HTTP cache strict failure connection did not close"
		);
		xrtNetStreamDestroy(pState->Server);
		pState->Server = NULL;
	} else {
		testRequire(
			xrtAtomic32Load(
				&pState->Accepted,
				XMEMORY_ACQUIRE
			) == iAccepted,
			"HTTP cache strict lookup failure reached origin"
		);
	}
	testRequire(
		(pState->FailureKind == Kind) &&
		(pState->FailureCode == iCode) &&
		(pState->Response == NULL),
		"HTTP cache failure lost its error contract"
	);
	xrtHttpCallDestroy(pState->Call);
	pState->Call = NULL;
	pState->ExpectFailure = false;
}



/* 执行没有范围条件的通用方法请求。 */
static void testHttpClientCacheCallMethod(
	test_http_client_cache* pState,
	cstr sMethod,
	uint16 iPort,
	cstr sPath,
	cstr sFlavor,
	cstr sPartition,
	xhttpclientcachemode Mode,
	bool bNetwork,
	xhttpclientcacheoutcome Outcome,
	uint16 iStatus,
	cstr sBody
)
{
	testHttpClientCacheCallRequest(
		pState,
		sMethod,
		iPort,
		sPath,
		sFlavor,
		sPartition,
		NULL,
		NULL,
		NULL,
		Mode,
		bNetwork,
		Outcome,
		iStatus,
		sBody,
		NULL,
		NULL
	);
}



/* 使用 GET 执行带 Range 和可选 If-Range 的缓存路径。 */
static void testHttpClientCacheCallRange(
	test_http_client_cache* pState,
	uint16 iPort,
	cstr sPath,
	cstr sRange,
	cstr sIfRange,
	xhttpclientcachemode Mode,
	bool bNetwork,
	xhttpclientcacheoutcome Outcome,
	uint16 iStatus,
	cstr sContentRange,
	cstr sBody
)
{
	testHttpClientCacheCallRequest(
		pState,
		"GET",
		iPort,
		sPath,
		NULL,
		NULL,
		sRange,
		sIfRange,
		sContentRange,
		Mode,
		bNetwork,
		Outcome,
		iStatus,
		sBody,
		NULL,
		NULL
	);
}



/* 使用 GET 验证缓存本地合成的多范围响应。 */
static void testHttpClientCacheCallMultipart(
	test_http_client_cache* pState,
	uint16 iPort,
	cstr sPath,
	cstr sRange,
	xhttpclientcachemode Mode,
	xhttpclientcacheoutcome Outcome,
	const test_http_client_cache_multipart* pExpected
)
{
	testHttpClientCacheCallRequest(
		pState,
		"GET",
		iPort,
		sPath,
		NULL,
		NULL,
		sRange,
		NULL,
		NULL,
		Mode,
		false,
		Outcome,
		XHTTP_STATUS_PARTIAL_CONTENT,
		NULL,
		testHttpClientCacheMultipartCheck,
		pExpected
	);
}



/* 使用 GET 执行最常见的缓存读取路径。 */
static void testHttpClientCacheCall(
	test_http_client_cache* pState,
	uint16 iPort,
	cstr sPath,
	cstr sFlavor,
	cstr sPartition,
	xhttpclientcachemode Mode,
	bool bNetwork,
	xhttpclientcacheoutcome Outcome,
	uint16 iStatus,
	cstr sBody
)
{
	testHttpClientCacheCallMethod(
		pState,
		"GET",
		iPort,
		sPath,
		sFlavor,
		sPartition,
		Mode,
		bNetwork,
		Outcome,
		iStatus,
		sBody
	);
}



/* 验证默认配置、首次保存、直接命中和禁止回源的 504。 */
static void testHttpClientCacheFresh(void)
{
	test_http_client_cache State;
	uint8 ConfigStorage[
		sizeof(xhttpclientcacheconfig) + 2u
	];
	uint8 OptionsStorage[
		sizeof(xhttpclientcacheoptions) + 2u
	];
	xhttpclientcacheconfig Config;
	xhttpclientcacheoptions Options;
	xhttpcachestats Stats;
	xnetaddr Address;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpClientCacheConfigInit(
		(xhttpclientcacheconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	memset(OptionsStorage, 0x5A, sizeof(OptionsStorage));
	xrtHttpClientCacheOptionsInit(
		(xhttpclientcacheoptions*)(void*)(OptionsStorage + 1u)
	);
	memcpy(&Options, OptionsStorage + 1u, sizeof(Options));
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(OptionsStorage[0] == 0x5A) &&
		(OptionsStorage[sizeof(OptionsStorage) - 1u] == 0x5A) &&
		(Config.Store == NULL) &&
		(Config.MaxBody == XHTTP_CLIENT_CACHE_BODY_DEFAULT) &&
		(Config.HeuristicMax ==
		 XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT) &&
		(Config.MaxRanges ==
		 XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT) &&
		(Config.HeuristicPercent ==
		 XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT) &&
		!Config.Shared &&
		Config.Heuristic &&
		!Config.Strict &&
		(Options.Mode == XHTTP_CLIENT_CACHE_DEFAULT),
		"HTTP cache defaults changed"
	);
	xrtClearError();
	xrtHttpClientCacheConfigInit(
		(xhttpclientcacheconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP cache wrapping config initializer mismatch"
	);
	xrtClearError();
	xrtHttpClientCacheOptionsInit(
		(xhttpclientcacheoptions*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP cache wrapping options initializer mismatch"
	);
	xrtClearError();

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_FRESH,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/fresh",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCallFailure(
		&State,
		"GET",
		Address.Port,
		"/fresh",
		NULL,
		XERR_RANGE,
		XHTTP_CLIENT_ERROR_RESPONSE,
		1,
		false,
		false
	);
	testRequire(
		(State.Info.Cache == XHTTP_CLIENT_CACHE_HIT) &&
		(State.Info.ResponseBodyBytes == 0),
		"HTTP cache hit bypassed the response body limit"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/fresh",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/absent",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		""
	);
	testRequire(
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Lookups == 4) &&
		(Stats.Hits == 2) &&
		(Stats.Misses == 2) &&
		(Stats.Stores == 1),
		"HTTP cache fresh-path stats mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证流式回调在网络首次响应和缓存命中时观察到完全相同的状态。 */
static void testHttpClientCacheStream(void)
{
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_FRESH,
		&Address,
		NULL,
		false
	);
	State.Stream = true;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/stream",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		NULL
	);
	testRequire(
		(State.StreamSize == 2u) &&
		(memcmp(State.StreamBody, "OK", 2u) == 0) &&
		(State.Info.ResponseBodyBytes == 2u),
		"HTTP network streamed response state mismatch"
	);
	State.StreamSize = 0;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/stream",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		NULL
	);
	testRequire(
		(State.StreamSize == 2u) &&
		(memcmp(State.StreamBody, "OK", 2u) == 0) &&
		(State.Info.ResponseBodyBytes == 2u),
		"HTTP cache streamed response state mismatch"
	);
	testHttpClientCacheClose(&State);
}



#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)

/* 验证请求 Trailer 绕过普通命中，cache-only 则稳定返回本地未命中。 */
static void testHttpClientCacheRequestTrailers(void)
{
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_FRESH,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/trailer",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	State.RequestTrailer = true;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/trailer",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_BYPASS,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/trailer",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		""
	);
	State.RequestTrailer = false;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/trailer",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheClose(&State);
}

#endif



/* 验证 stale ETag 条目使用 304 更新后向用户重放原始 200。 */
static void testHttpClientCacheValidation(void)
{
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_VALIDATE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/validate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/validate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_REVALIDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testRequire(
		State.Requests == 2,
		"HTTP cache validation request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证 304 提交冲突不会覆盖较新的并发缓存记录。 */
static void testHttpClientCacheValidationConflict(void)
{
	test_http_client_cache_backend Backend;
	test_http_client_cache State;
	xhttpcache* pStore;
	xhttpcachestats Stats;
	xnetaddr Address;

	pStore = testHttpClientCacheBackendOpen(&Backend);
	testRequire(
		pStore != NULL,
		"HTTP validation conflict backend creation failed"
	);
	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_VALIDATE,
		&Address,
		pStore,
		false
	);
	xrtHttpCacheRelease(pStore);
	Backend.Fault =
		TEST_HTTP_CLIENT_CACHE_FAULT_REPLACE_CONFLICT;

	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/validate-conflict",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/validate-conflict",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_REVALIDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCallRequest(
		&State,
		"GET",
		Address.Port,
		"/validate-conflict",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OLD",
		testHttpClientCacheMetadataCheck,
		"concurrent"
	);
	testRequire(
		(State.Requests == 2) &&
		(Backend.Puts == 2) &&
		(Backend.Conflicts == 1) &&
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Stores == 2) &&
		(Stats.Replacements == 1) &&
		(Stats.Conflicts == 1),
		"HTTP validation conflict result mismatch"
	);
	testHttpClientCacheClose(&State);
	testRequire(
		Backend.Closed,
		"HTTP validation conflict backend did not close"
	);
}



/* 验证新鲜 GET 可直接服务 HEAD，并忽略 Range 与 GET Trailer。 */
static void testHttpClientCacheHeadFresh(void)
{
	static const test_http_client_cache_head Length = {
		"4",
		NULL,
		true
	};
	static const test_http_client_cache_head NoLength = {
		NULL,
		NULL,
		true
	};
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_HEAD_FRESH,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-length",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"DATA"
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-length",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Length
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-length",
		NULL,
		NULL,
		"bytes=0-0",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Length
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-trailer",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"DATA"
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-trailer",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&NoLength
	);
	testRequire(
		State.Requests == 2,
		"fresh HTTP HEAD cache request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证 HEAD 的 304 能更新 GET 元数据而不丢失 GET 正文。 */
static void testHttpClientCacheHeadValidation(void)
{
	static const test_http_client_cache_head Expected = {
		"3",
		NULL,
		true
	};
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_HEAD_VALIDATE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-validate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-validate",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_REVALIDATED,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Expected
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-validate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testRequire(
		State.Requests == 2,
		"HTTP HEAD validation request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证无 Validator 的 200 HEAD 原子更新 GET 元数据。 */
static void testHttpClientCacheHeadUpdate(void)
{
	static const test_http_client_cache_head Expected = {
		"3",
		"new",
		true
	};
	test_http_client_cache State;
	xhttpcachestats Stats;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_HEAD_UPDATE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-update",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-update",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Expected
	);
	testHttpClientCacheCallRequest(
		&State,
		"GET",
		Address.Port,
		"/head-update",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OLD",
		testHttpClientCacheMetadataCheck,
		"new"
	);
	testRequire(
		(State.Requests == 2) &&
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Replacements == 1),
		"HTTP HEAD metadata update result mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证长度失配的 HEAD 删除 GET，并可独立保存 HEAD 记录。 */
static void testHttpClientCacheHeadInvalidate(void)
{
	static const test_http_client_cache_head Expected = {
		"4",
		"new",
		true
	};
	test_http_client_cache State;
	xhttpcachestats Stats;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_HEAD_INVALIDATE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OLD"
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-invalidate",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Expected
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/head-invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		""
	);
	testHttpClientCacheCallRequest(
		&State,
		"HEAD",
		Address.Port,
		"/head-invalidate",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"",
		testHttpClientCacheHeadCheck,
		&Expected
	);
	testRequire(
		(State.Requests == 2) &&
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Removals == 1),
		"HTTP HEAD invalidation result mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证 Vary 选择相同请求字段命中并隔离不同变体。 */
static void testHttpClientCacheVary(void)
{
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_VARY,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/vary",
		"red",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"RED"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/vary",
		"red",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"RED"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/vary",
		"blue",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"BLUE"
	);
	testRequire(
		State.Requests == 2,
		"HTTP cache Vary request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证相同 URI 在不同调用分区中不会互相泄漏。 */
static void testHttpClientCachePartition(void)
{
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_PARTITION,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/partition",
		NULL,
		"tenant-a",
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/partition",
		NULL,
		"tenant-a",
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/partition",
		NULL,
		"tenant-b",
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testRequire(
		State.Requests == 2,
		"HTTP cache partition request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证 unsafe 成功响应使目标和同源 Location 条目同时失效。 */
static void testHttpClientCacheInvalidation(void)
{
	test_http_client_cache State;
	xhttpcachestats Stats;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_INVALIDATE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/related",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/related",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCallMethod(
		&State,
		"POST",
		Address.Port,
		"/invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_MISS,
		XHTTP_STATUS_NO_CONTENT,
		""
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/related",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	testRequire(
		(State.Requests == 5) &&
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 2) &&
		(Stats.Removals == 2),
		"HTTP cache invalidation result mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证完整记录直接服务单范围、后缀范围、416 和 If-Range 回退。 */
static void testHttpClientCacheRange(void)
{
	static const xhttpbyterange MultiRanges[] = {
		{ 0u, 1u },
		{ 8u, 9u }
	};
	static const xhttpbyterange MergedRanges[] = {
		{ 0u, 4u },
		{ 8u, 9u }
	};
	static const test_http_client_cache_multipart Multi = {
		"0123456789",
		MultiRanges,
		sizeof(MultiRanges) / sizeof(MultiRanges[0])
	};
	static const test_http_client_cache_multipart Merged = {
		"0123456789",
		MergedRanges,
		sizeof(MergedRanges) / sizeof(MergedRanges[0])
	};
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/range",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"0123456789"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=2-5",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 2-5/10",
		"2345"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=-3",
		"\"asset\"",
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 7-9/10",
		"789"
	);
	testHttpClientCacheCallMultipart(
		&State,
		Address.Port,
		"/range",
		"bytes=0-1,8-9",
		XHTTP_CLIENT_CACHE_DEFAULT,
		XHTTP_CLIENT_CACHE_HIT,
		&Multi
	);
	testHttpClientCacheCallMultipart(
		&State,
		Address.Port,
		"/range",
		"bytes=8-9,0-2,2-4",
		XHTTP_CLIENT_CACHE_DEFAULT,
		XHTTP_CLIENT_CACHE_HIT,
		&Merged
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=0-1,20-30",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 0-1/10",
		"01"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=20-30,40-50",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_RANGE_NOT_SATISFIABLE,
		"bytes */10",
		""
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=2-5",
		"\"other\"",
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		NULL,
		"0123456789"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/range",
		"bytes=0-0,1-1,2-2,3-3,4-4,5-5,6-6,7-7,"
		"8-8,9-9,10-10,11-11,12-12,13-13,14-14,"
		"15-15,16-16",
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		NULL,
		""
	);
	testRequire(
		State.Requests == 1,
		"HTTP cache complete range path reached the origin"
	);
	testHttpClientCacheClose(&State);
}



/* 验证两个强验证器一致的 206 被合并并最终服务完整表示。 */
static void testHttpClientCacheRangeCombine(void)
{
	static const xhttpbyterange PartialRanges[] = {
		{ 0u, 1u },
		{ 3u, 4u }
	};
	static const test_http_client_cache_multipart Partial = {
		"0123456789",
		PartialRanges,
		sizeof(PartialRanges) / sizeof(PartialRanges[0])
	};
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_PARTIAL,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial",
		"bytes=0-4",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 0-4/10",
		"01234"
	);
	testHttpClientCacheCallMultipart(
		&State,
		Address.Port,
		"/partial",
		"bytes=0-1,3-4",
		XHTTP_CLIENT_CACHE_DEFAULT,
		XHTTP_CLIENT_CACHE_HIT,
		&Partial
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial",
		"bytes=0-1,8-9",
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		NULL,
		""
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial",
		"bytes=0-4",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 0-4/10",
		"01234"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial",
		"bytes=5-9",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 5-9/10",
		"56789"
	);
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/partial",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"0123456789"
	);
	testRequire(
		State.Requests == 2,
		"HTTP cache partial range combine request count mismatch"
	);
	testHttpClientCacheClose(&State);
}



/* 验证片段提交冲突后会重读、重算并保留并发写入的元数据。 */
static void testHttpClientCacheRangeConflict(void)
{
	test_http_client_cache_backend Backend;
	test_http_client_cache State;
	xhttpcache* pStore;
	xhttpcachestats Stats;
	xnetaddr Address;

	pStore = testHttpClientCacheBackendOpen(&Backend);
	testRequire(
		pStore != NULL,
		"HTTP client cache conflict backend creation failed"
	);
	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_PARTIAL,
		&Address,
		pStore,
		false
	);
	xrtHttpCacheRelease(pStore);
	Backend.Fault =
		TEST_HTTP_CLIENT_CACHE_FAULT_REPLACE_CONFLICT;

	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial-conflict",
		"bytes=0-4",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 0-4/10",
		"01234"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/partial-conflict",
		"bytes=5-9",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 5-9/10",
		"56789"
	);
	testHttpClientCacheCallRequest(
		&State,
		"GET",
		Address.Port,
		"/partial-conflict",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"0123456789",
		testHttpClientCacheMetadataCheck,
		"concurrent"
	);
	testRequire(
		(State.Requests == 2) &&
		(Backend.Puts == 3) &&
		(Backend.Conflicts == 1) &&
		xrtHttpCacheStats(State.Cache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Stores == 3) &&
		(Stats.Replacements == 2) &&
		(Stats.Conflicts == 1),
		"HTTP client cache conflict retry result mismatch"
	);
	testHttpClientCacheClose(&State);
	testRequire(
		Backend.Closed,
		"HTTP client cache conflict backend did not close"
	);
}



/* 验证源站 multipart 缺口被原子拆分并与已有覆盖合并。 */
static void testHttpClientCacheOriginMultipart(void)
{
	static const char Multipart[] =
		"--origin\r\n"
		"Content-Range: bytes 8-9/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"89\r\n"
		"--origin\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"01\r\n"
		"--origin--\r\n";
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/origin-multipart",
		"bytes=2-7",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		"bytes 2-7/10",
		"234567"
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/origin-multipart",
		"bytes=0-1,8-9",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		NULL,
		Multipart
	);
	testHttpClientCacheCallRequest(
		&State,
		"GET",
		Address.Port,
		"/origin-multipart",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"0123456789",
		testHttpClientCacheRepresentationCheck,
		NULL
	);
	testRequire(
		State.Requests == 2,
		"HTTP cache origin multipart reached origin after completion"
	);
	testHttpClientCacheClose(&State);
}



/* 验证一次完整 multipart 206 可以直接保存为完整 200 表示。 */
static void testHttpClientCacheOriginMultipartComplete(void)
{
	static const char Multipart[] =
		"--complete\r\n"
		"Content-Range: bytes 5-9/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"56789\r\n"
		"--complete\r\n"
		"Content-Range: bytes 0-4/10\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"01234\r\n"
		"--complete--\r\n";
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_COMPLETE,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/origin-multipart-complete",
		"bytes=0-4,5-9",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_PARTIAL_CONTENT,
		NULL,
		Multipart
	);
	testHttpClientCacheCallRequest(
		&State,
		"GET",
		Address.Port,
		"/origin-multipart-complete",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_HIT,
		XHTTP_STATUS_OK,
		"0123456789",
		testHttpClientCacheRepresentationCheck,
		NULL
	);
	testRequire(
		State.Requests == 1,
		"HTTP cache complete origin multipart was not reusable"
	);
	testHttpClientCacheClose(&State);
}



/* 验证畸形源站 multipart 在宽松和严格模式下分别旁路或失败。 */
static void testHttpClientCacheOriginMultipartInvalid(void)
{
	static const char Multipart[] =
		"--broken\r\n"
		"Content-Range: bytes 0-1/10\r\n"
		"\r\n"
		"01\r\n"
		"--broken\r\n";
	test_http_client_cache State;
	xnetaddr Address;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_INVALID,
		&Address,
		NULL,
		false
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/origin-multipart-invalid",
		"bytes=0-1",
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_MISS,
		XHTTP_STATUS_PARTIAL_CONTENT,
		NULL,
		Multipart
	);
	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/origin-multipart-invalid",
		"bytes=0-1",
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		NULL,
		""
	);
	testRequire(
		State.Requests == 1,
		"HTTP cache malformed multipart was stored"
	);
	testHttpClientCacheClose(&State);

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE_MULTIPART_INVALID,
		&Address,
		NULL,
		true
	);
	testHttpClientCacheCallFailure(
		&State,
		"GET",
		Address.Port,
		"/origin-multipart-invalid",
		"bytes=0-1",
		XERR_VALUE,
		XHTTP_CLIENT_ERROR_CACHE,
		0,
		false,
		true
	);
	testHttpClientCacheClose(&State);
}



#if defined(XRT_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 验证自动解压不会把编码表示的线路字节作为普通 Range 切片。 */
static void testHttpClientCacheEncodedRange(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=3600")
		},
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	static const uint8 Body[] = { 0x1f, 0x8b, 0x00, 0x00 };
	test_http_client_cache State;
	xhttpcachepart Part = {
		0,
		{ Body, sizeof(Body) }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord;
	xnetaddr Address;
	char Url[192];
	int iLength;

	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_RANGE,
		&Address,
		NULL,
		false
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://cache.test:%u/encoded",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)) &&
		xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			(xstrview){ Url, (size_t)iLength }
		) &&
		xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			XHTTP_STATUS_OK
		),
		"HTTP cache encoded range fixture failed"
	);
	Input.Version = XHTTP_VERSION_1_1;
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = Fields;
	Input.FieldCount = sizeof(Fields) / sizeof(Fields[0]);
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = sizeof(Body);
	Input.ResponseTime = xrtNow();
	Input.RequestClock = xrtClock();
	Input.ResponseClock = Input.RequestClock;
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	pRecord = xrtHttpCacheRecordCreate(&Input);
	testRequire(
		(pRecord != NULL) &&
		(xrtHttpCachePut(State.Cache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED),
		"HTTP cache encoded range record failed"
	);
	xrtHttpCacheRecordRelease(pRecord);

	testHttpClientCacheCallRange(
		&State,
		Address.Port,
		"/encoded",
		"bytes=0-1",
		NULL,
		XHTTP_CLIENT_CACHE_ONLY,
		false,
		XHTTP_CLIENT_CACHE_ONLY_MISS,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		NULL,
		""
	);
	testRequire(
		State.Requests == 0,
		"HTTP cache encoded Range reached the origin"
	);
	testHttpClientCacheClose(&State);
}

#endif



/* 验证默认模式对查询、保存和失效后端错误保持网络响应可用。 */
static void testHttpClientCacheBackendFailOpen(void)
{
	test_http_client_cache_backend Backend;
	test_http_client_cache State;
	xhttpcache* pStore;
	xnetaddr Address;

	pStore = testHttpClientCacheBackendOpen(&Backend);
	testRequire(
		pStore != NULL,
		"HTTP client cache fail-open backend creation failed"
	);
	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_INVALIDATE,
		&Address,
		pStore,
		false
	);
	xrtHttpCacheRelease(pStore);

	Backend.Fault = TEST_HTTP_CLIENT_CACHE_FAULT_GET;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/backend-get",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_UPDATED,
		XHTTP_STATUS_OK,
		"OK"
	);
	Backend.Fault = TEST_HTTP_CLIENT_CACHE_FAULT_PUT;
	testHttpClientCacheCall(
		&State,
		Address.Port,
		"/backend-put",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_MISS,
		XHTTP_STATUS_OK,
		"OK"
	);
	Backend.Fault =
		TEST_HTTP_CLIENT_CACHE_FAULT_REMOVE_URI;
	testHttpClientCacheCallMethod(
		&State,
		"POST",
		Address.Port,
		"/invalidate",
		NULL,
		NULL,
		XHTTP_CLIENT_CACHE_DEFAULT,
		true,
		XHTTP_CLIENT_CACHE_MISS,
		XHTTP_STATUS_NO_CONTENT,
		""
	);
	testRequire(
		(Backend.Gets == 3) &&
		(Backend.Puts == 2) &&
		(Backend.Removes == 1),
		"HTTP client cache fail-open operation accounting failed"
	);
	testHttpClientCacheClose(&State);
	testRequire(
		Backend.Closed,
		"HTTP client cache fail-open backend did not close"
	);
}



/* 验证严格模式把查询、保存和失效失败统一提升为缓存错误。 */
static void testHttpClientCacheBackendStrict(void)
{
	test_http_client_cache_backend Backend;
	test_http_client_cache State;
	xhttpcache* pStore;
	xnetaddr Address;

	pStore = testHttpClientCacheBackendOpen(&Backend);
	testRequire(
		pStore != NULL,
		"HTTP client cache strict backend creation failed"
	);
	testHttpClientCacheOpen(
		&State,
		TEST_HTTP_CLIENT_CACHE_INVALIDATE,
		&Address,
		pStore,
		true
	);
	xrtHttpCacheRelease(pStore);

	Backend.Fault = TEST_HTTP_CLIENT_CACHE_FAULT_GET;
	testHttpClientCacheCallFailure(
		&State,
		"GET",
		Address.Port,
		"/strict-get",
		NULL,
		XERR_IO,
		XHTTP_CLIENT_ERROR_CACHE,
		0,
		true,
		false
	);
	Backend.Fault = TEST_HTTP_CLIENT_CACHE_FAULT_PUT;
	testHttpClientCacheCallFailure(
		&State,
		"GET",
		Address.Port,
		"/strict-put",
		NULL,
		XERR_IO,
		XHTTP_CLIENT_ERROR_CACHE,
		0,
		false,
		true
	);
	Backend.Fault =
		TEST_HTTP_CLIENT_CACHE_FAULT_REMOVE_URI;
	testHttpClientCacheCallFailure(
		&State,
		"POST",
		Address.Port,
		"/invalidate",
		NULL,
		XERR_IO,
		XHTTP_CLIENT_ERROR_CACHE,
		0,
		false,
		true
	);
	testRequire(
		(Backend.Gets == 3) &&
		(Backend.Puts == 1) &&
		(Backend.Removes == 1),
		"HTTP client cache strict operation accounting failed"
	);
	testHttpClientCacheClose(&State);
	testRequire(
		Backend.Closed,
		"HTTP client cache strict backend did not close"
	);
}



/* 覆盖 HTTP 客户端缓存的主要端到端协议路径。 */
int main(void)
{
	testHttpClientCacheFresh();
	testHttpClientCacheStream();
	#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		testHttpClientCacheRequestTrailers();
	#endif
	testHttpClientCacheValidation();
	testHttpClientCacheValidationConflict();
	testHttpClientCacheHeadFresh();
	testHttpClientCacheHeadValidation();
	testHttpClientCacheHeadUpdate();
	testHttpClientCacheHeadInvalidate();
	testHttpClientCacheVary();
	testHttpClientCachePartition();
	testHttpClientCacheInvalidation();
	testHttpClientCacheRange();
	testHttpClientCacheRangeCombine();
	testHttpClientCacheRangeConflict();
	testHttpClientCacheOriginMultipart();
	testHttpClientCacheOriginMultipartComplete();
	testHttpClientCacheOriginMultipartInvalid();
	#if defined(XRT_FEATURE_HTTP_CLIENT_DECOMPRESS)
		testHttpClientCacheEncodedRange();
	#endif
	testHttpClientCacheBackendFailOpen();
	testHttpClientCacheBackendStrict();
	printf("[PASS] HTTP client cache\n");
	return 0;
}
