#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"



#define TEST_HTTP_CLIENT_CACHE_OOM_RANGES 65u



/* 只拒绝目标 backing 尺寸的一次分配，保留错误对象构造能力。 */
typedef struct test_http_client_cache_oom_allocator {
	size_t Min;
	size_t Max;
	bool Armed;
	bool Failed;
} test_http_client_cache_oom_allocator;



/* 转发普通分配，并拒绝命中目标尺寸的第一次请求。 */
static ptr testHttpClientCacheOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_client_cache_oom_allocator* pState =
		(test_http_client_cache_oom_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并与初始分配共享同一个故障点。 */
static ptr testHttpClientCacheOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_client_cache_oom_allocator* pState =
		(test_http_client_cache_oom_allocator*)pData;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器产生的原始内存。 */
static void testHttpClientCacheOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 构造超过小块池阈值、但仍处于配置上限内的 Range 集合。 */
static void testHttpClientCacheOomRange(
	char* sRange,
	size_t iCapacity
)
{
	size_t iOffset = 0;
	size_t i;

	for ( i = 0; i < TEST_HTTP_CLIENT_CACHE_OOM_RANGES; i++ ) {
		int iWritten = snprintf(
			sRange + iOffset,
			iCapacity - iOffset,
			i == 0 ? "bytes=0-0" : ",0-0"
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten <
			 (iCapacity - iOffset)),
			"HTTP client cache OOM Range overflowed"
		);
		iOffset += (size_t)iWritten;
	}
}



/* 创建与测试请求主键一致的新鲜完整缓存记录。 */
static xhttpcacherecord* testHttpClientCacheOomRecord(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=3600")
		},
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"cache-v1\"")
		}
	};
	static const uint8 Body[] = "0123456789";
	xhttpcachepart Part = {
		0,
		{ Body, sizeof(Body) - 1u }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;

	testRequire(
		xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL(
				"http://cache.test/client-cache-oom"
			)
		) &&
		xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			XHTTP_STATUS_OK
		),
		"HTTP client cache OOM record input failed"
	);
	Input.Version = XHTTP_VERSION_1_1;
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = Fields;
	Input.FieldCount = sizeof(Fields) / sizeof(Fields[0]);
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = sizeof(Body) - 1u;
	Input.ResponseTime = xrtNow();
	Input.RequestClock = xrtClock();
	Input.ResponseClock = Input.RequestClock;
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	return xrtHttpCacheRecordCreate(&Input);
}



/* 初始化只执行同步缓存选择所需的最小 Client 和 Call。 */
static void testHttpClientCacheOomCall(
	xhttpclient* pClient,
	xhttpcall* pCall,
	xhttpcache* pCache,
	xhttprequest* pRequest,
	bool bStrict
)
{
	memset(pClient, 0, sizeof(*pClient));
	memset(pCall, 0, sizeof(*pCall));
	xrtHttpClientConfigInit(&pClient->Config);
	pClient->Config.Cache.MaxRanges =
		TEST_HTTP_CLIENT_CACHE_OOM_RANGES;
	pClient->Config.Cache.Strict = bStrict;
	pClient->Cache = pCache;
	pCall->Client = pClient;
	pCall->Request = pRequest;
	pCall->CacheMode = XHTTP_CLIENT_CACHE_DEFAULT;
	pCall->CacheEnabled = true;
	xrtAtomic32Init(
		&pCall->Info.Cache,
		XHTTP_CLIENT_CACHE_NONE
	);
}



/* 对规范化数组 OOM 分别验证严格终止和宽松回源。 */
int main(void)
{
	test_http_client_cache_oom_allocator State = {
		1025u,
		2048u,
		false,
		false
	};
	xallocator Allocator = {
		&State,
		testHttpClientCacheOomAlloc,
		testHttpClientCacheOomRealloc,
		testHttpClientCacheOomFree
	};
	xhttpclient Client;
	xhttpcall Call;
	xhttpcache* pCache;
	xhttpcacherecord* pRecord;
	xhttprequest* pRequest;
	const xerror* pError;
	char sRange[384];

	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP client cache OOM allocator install failed"
	);
	testHttpClientCacheOomRange(
		sRange,
		sizeof(sRange)
	);
	pCache = xrtHttpCacheCreate(NULL);
	pRecord = testHttpClientCacheOomRecord();
	testRequire(
		(pCache != NULL) &&
		(pRecord != NULL) &&
		(xrtHttpCachePut(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED),
		"HTTP client cache OOM fixture store failed"
	);
	xrtHttpCacheRecordRelease(pRecord);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"http://cache.test/client-cache-oom"
		)
	);
	testRequire(
		(pRequest != NULL) &&
		xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Range"),
			(xstrview){ sRange, strlen(sRange) }
		),
		"HTTP client cache OOM request failed"
	);

	testHttpClientCacheOomCall(
		&Client,
		&Call,
		pCache,
		pRequest,
		true
	);
	State.Armed = true;
	State.Failed = false;
	testRequire(
		!__xrtHttpClientCachePrepare(&Call) &&
		State.Failed,
		"HTTP client cache strict Range OOM was not hit"
	);
	State.Armed = false;
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_MEMORY) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		 ) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_CLIENT_ERROR_CACHE) &&
		(strcmp(
			xrtErrorOperation(pError),
			"plan-http-cache-range"
		 ) == 0) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL) &&
		Call.CacheFailed,
		"HTTP client cache strict Range OOM lost its error"
	);
	__xrtHttpClientCacheUnit(&Call);
	xrtClearError();

	testHttpClientCacheOomCall(
		&Client,
		&Call,
		pCache,
		pRequest,
		false
	);
	State.Armed = true;
	State.Failed = false;
	testRequire(
		__xrtHttpClientCachePrepare(&Call) &&
		State.Failed,
		"HTTP client cache fail-open Range OOM was not hit"
	);
	State.Armed = false;
	testRequire(
		(xrtGetError() == NULL) &&
		!Call.CacheFailed &&
		!Call.CacheReady &&
		(Call.CacheRanges == NULL) &&
		(xrtAtomic32Load(
			&Call.Info.Cache,
			XMEMORY_ACQUIRE
		 ) == XHTTP_CLIENT_CACHE_MISS),
		"HTTP client cache Range OOM did not fail open"
	);
	__xrtHttpClientCacheUnit(&Call);

	/* 响应前重试必须撤销并重建自动验证器，不能把它误当成用户字段。 */
	testRequire(
		xrtHttpRequestRemoveHeader(
			pRequest,
			XRT_STR_LITERAL("Range")
		) &&
		xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Cache-Control"),
			XRT_STR_LITERAL("no-cache")
		),
		"HTTP client cache retry request setup failed"
	);
	testHttpClientCacheOomCall(
		&Client,
		&Call,
		pCache,
		pRequest,
		false
	);
	testRequire(
		__xrtHttpClientCachePrepare(&Call) &&
		Call.CacheValidating &&
		Call.CacheIfNoneMatch &&
		(xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("If-None-Match")
		) != NULL),
		"HTTP client cache initial validator setup failed"
	);
	testRequire(
		__xrtHttpClientCachePrepare(&Call) &&
		Call.CacheValidating &&
		Call.CacheIfNoneMatch &&
		(xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("If-None-Match")
		) != NULL),
		"HTTP client cache retry lost automatic validator state"
	);
	__xrtHttpClientCacheUnit(&Call);

	xrtHttpRequestDestroy(pRequest);
	xrtHttpCacheRelease(pCache);
	printf("[PASS] HTTP client cache OOM\n");
	return 0;
}
