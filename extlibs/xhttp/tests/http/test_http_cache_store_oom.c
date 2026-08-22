#include "../test.h"

#include <xrt/http_cache_store.h>



/* 可调失败分配器扫描 Record、Map 和 Node 的所有分配路径。 */
typedef struct test_http_cache_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_cache_allocator;



/* 在指定分配序号失败。 */
static ptr testHttpCacheAlloc(ptr pContext, size_t iSize)
{
	test_http_cache_allocator* pState =
		(test_http_cache_allocator*)pContext;
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



/* 重分配失败时保留原块和存活计数。 */
static ptr testHttpCacheRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_cache_allocator* pState =
		(test_http_cache_allocator*)pContext;
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



/* 释放底层分配并维护存活计数。 */
static void testHttpCacheFree(ptr pContext, ptr pMemory)
{
	test_http_cache_allocator* pState =
		(test_http_cache_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP cache OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 创建 OOM 扫描使用的完整响应 Record。 */
static xhttpcacherecord* testHttpCacheOomRecord(void)
{
	static const xhttpfield RequestFields[] = {
		{
			XRT_STR_INIT("Accept-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	static const xhttpfield ResponseFields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60")
		}
	};
	static const uint8 Body[4096] = { 1 };
	xhttpcachepart Part = {
		0, { Body, sizeof(Body) }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;

	if ( !xrtHttpCacheKeyInit(
		&Key,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/oom")
	) ) {
		return NULL;
	}
	Key.Fields = RequestFields;
	Key.FieldCount = 1;
	if ( !xrtHttpCacheRecordInputInit(
		&Input, &Key, XHTTP_STATUS_OK
	) ) {
		return NULL;
	}
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Fields = ResponseFields;
	Input.FieldCount = sizeof(ResponseFields) /
		sizeof(ResponseFields[0]);
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = sizeof(Body);
	Input.ResponseTime = 100;
	Input.RequestClock = 10;
	Input.ResponseClock = 20;
	return xrtHttpCacheRecordCreate(&Input);
}



/* 在一个失败点下执行创建、条件插入、条件替换、命中和删除路径。 */
static bool testHttpCacheOomAttempt(void)
{
	static const xhttpfield QueryField = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("gzip")
	};
	xhttpcacheconfig Config;
	xhttpcache* pCache = NULL;
	xhttpcacherecord* pRecord = NULL;
	xhttpcacherecord* pReplacement = NULL;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	size_t iRemoved;
	bool bComplete = false;

	xrtHttpCacheConfigInit(&Config);
	Config.InitialEntries = 4;
	pCache = xrtHttpCacheCreate(&Config);
	if ( pCache == NULL ) {
		goto done;
	}
	pRecord = testHttpCacheOomRecord();
	if ( pRecord == NULL ) {
		goto done;
	}
	if ( xrtHttpCacheInsert(
		pCache, pRecord
	) != XHTTP_CACHE_PUT_STORED ) {
		goto done;
	}
	pReplacement = testHttpCacheOomRecord();
	if ( pReplacement == NULL ) {
		goto done;
	}
	if ( xrtHttpCacheReplace(
		pCache,
		pRecord,
		pReplacement
	) != XHTTP_CACHE_PUT_REPLACED ) {
		goto done;
	}
	if ( !xrtHttpCacheKeyInit(
		&Key,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/oom")
	) ) {
		goto done;
	}
	Key.Fields = &QueryField;
	Key.FieldCount = 1;
	if ( xrtHttpCacheGet(
		pCache, &Key, &pHit
	) != XHTTP_CACHE_LOOKUP_HIT ) {
		goto done;
	}
	if ( (xrtHttpCacheRemoveRecord(
			pCache,
			pHit
		 ) != XHTTP_CACHE_CHANGE_APPLIED) ||
		!xrtHttpCacheRemove(
			pCache,
			&Key,
			&iRemoved
		) || (iRemoved != 0) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtHttpCacheRecordRelease(pHit);
	xrtHttpCacheRecordRelease(pReplacement);
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(pCache);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并验证每个事务保持失败原子且不泄漏。 */
int main(void)
{
	test_http_cache_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpCacheAlloc,
		testHttpCacheRealloc,
		testHttpCacheFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP cache OOM allocator install failed");
	testRequire(testHttpCacheOomAttempt(),
		"HTTP cache OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP cache OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 96; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpCacheOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP cache OOM memory debug reset failed"
		);
		testRequire(
			State.Live == iBaseline,
			"HTTP cache OOM attempt leaked storage"
		);
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP cache OOM sweep missed failure or success");
	printf("[PASS] HTTP cache store OOM\n");
	return 0;
}
