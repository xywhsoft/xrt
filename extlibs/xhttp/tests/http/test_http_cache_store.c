#include "../test.h"

#include <xrt/http_cache_store.h>



/* 创建带可选 Vary: Accept-Encoding 的完整响应记录。 */
static xhttpcacherecord* testHttpCacheStoreRecord(
	xstrview URI,
	xstrview Partition,
	xstrview Encoding,
	xstrview Body,
	xtime iResponseTime
)
{
	xhttpfield RequestField;
	xhttpfield ResponseFields[2];
	xhttpcachepart Part;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	size_t iFieldCount = 1;

	testRequire(
		xrtHttpCacheKeyInit(
			&Key, XRT_STR_LITERAL("GET"), URI
		),
		"HTTP cache store key init failed"
	);
	Key.Partition = Partition;
	if ( Encoding.Data != NULL ) {
		RequestField.Name =
			XRT_STR_LITERAL("Accept-Encoding");
		RequestField.Value = Encoding;
		Key.Fields = &RequestField;
		Key.FieldCount = 1;
		ResponseFields[1].Name = XRT_STR_LITERAL("Vary");
		ResponseFields[1].Value =
			XRT_STR_LITERAL("Accept-Encoding");
		iFieldCount = 2;
	}
	ResponseFields[0].Name = XRT_STR_LITERAL("Cache-Control");
	ResponseFields[0].Value = XRT_STR_LITERAL("max-age=60");
	Part.Offset = 0;
	Part.Data.Data = (cbytes)Body.Data;
	Part.Data.Size = Body.Size;
	testRequire(
		xrtHttpCacheRecordInputInit(
			&Input, &Key, XHTTP_STATUS_OK
		),
		"HTTP cache store record input init failed"
	);
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = ResponseFields;
	Input.FieldCount = iFieldCount;
	Input.Parts = Body.Size != 0 ? &Part : NULL;
	Input.PartCount = Body.Size != 0 ? 1 : 0;
	Input.Length = Body.Size;
	Input.ResponseTime = iResponseTime;
	Input.RequestClock = 10;
	Input.ResponseClock = 20;
	return xrtHttpCacheRecordCreate(&Input);
}



/* 使用普通 GET、分区和可选编码建立查询 Key。 */
static xhttpcachekey testHttpCacheStoreKey(
	xstrview URI,
	xstrview Partition,
	const xhttpfield* pFields,
	size_t iFieldCount
)
{
	xhttpcachekey Key;

	testRequire(
		xrtHttpCacheKeyInit(
			&Key, XRT_STR_LITERAL("GET"), URI
		),
		"HTTP cache store query key init failed"
	);
	Key.Partition = Partition;
	Key.Fields = pFields;
	Key.FieldCount = iFieldCount;
	return Key;
}



/* 验证首次保存、替换、Vary 变体和分区隔离。 */
static void testHttpCacheStoreVariants(void)
{
	static const xhttpfield GzipField = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("gzip")
	};
	static const xhttpfield BrField = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("br")
	};
	xhttpcache* pCache = xrtHttpCacheCreate(NULL);
	xhttpcacherecord* pGzip;
	xhttpcacherecord* pGzipNew;
	xhttpcacherecord* pBr;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;

	testRequire(pCache != NULL,
		"HTTP cache store create failed");
	pGzip = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary"),
		XRT_STR_LITERAL("site-a"),
		XRT_STR_LITERAL("gzip"),
		XRT_STR_LITERAL("gzip-old"),
		100
	);
	pGzipNew = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary"),
		XRT_STR_LITERAL("site-a"),
		XRT_STR_LITERAL("gzip"),
		XRT_STR_LITERAL("gzip-new"),
		200
	);
	pBr = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary"),
		XRT_STR_LITERAL("site-a"),
		XRT_STR_LITERAL("br"),
		XRT_STR_LITERAL("br-body"),
		150
	);
	testRequire(
		(pGzip != NULL) && (pGzipNew != NULL) &&
		(pBr != NULL),
		"HTTP cache store variant record create failed"
	);
	testRequire(
		xrtHttpCachePut(pCache, pGzip) ==
			XHTTP_CACHE_PUT_STORED &&
		xrtHttpCachePut(pCache, pBr) ==
			XHTTP_CACHE_PUT_STORED &&
		xrtHttpCachePut(pCache, pGzipNew) ==
			XHTTP_CACHE_PUT_REPLACED,
		"HTTP cache store variant put failed"
	);

	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/vary"),
		XRT_STR_LITERAL("site-a"),
		&GzipField,
		1
	);
	testRequire(
		xrtHttpCacheGet(pCache, &Key, &pHit) ==
			XHTTP_CACHE_LOOKUP_HIT &&
		xrtHttpCacheRecordPartAt(pHit, 0) != NULL &&
		memcmp(
			xrtHttpCacheRecordPartAt(pHit, 0)->Data.Data,
			"gzip-new",
			8
		) == 0,
		"HTTP cache store did not return replaced variant"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;

	Key.Fields = &BrField;
	testRequire(
		xrtHttpCacheGet(pCache, &Key, &pHit) ==
			XHTTP_CACHE_LOOKUP_HIT &&
		memcmp(
			xrtHttpCacheRecordPartAt(pHit, 0)->Data.Data,
			"br-body",
			7
		) == 0,
		"HTTP cache store selected wrong Vary variant"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;

	Key.Partition = XRT_STR_LITERAL("site-b");
	testRequire(
		xrtHttpCacheGet(pCache, &Key, &pHit) ==
			XHTTP_CACHE_LOOKUP_MISS &&
		(pHit == NULL),
		"HTTP cache store crossed partition boundary"
	);
	testRequire(
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 2) &&
		(Stats.Stores == 3) &&
		(Stats.Replacements == 1) &&
		(Stats.Hits == 2) &&
		(Stats.Misses == 1),
		"HTTP cache store variant stats mismatch"
	);

	xrtHttpCacheRecordRelease(pBr);
	xrtHttpCacheRecordRelease(pGzipNew);
	xrtHttpCacheRecordRelease(pGzip);
	xrtHttpCacheRelease(pCache);
}



/* 验证 LRU 命中更新、容量淘汰和独立命中引用。 */
static void testHttpCacheStoreLRU(void)
{
	xhttpcacheconfig Config;
	xhttpcache* pCache;
	xhttpcacherecord* pA;
	xhttpcacherecord* pB;
	xhttpcacherecord* pC;
	xhttpcacherecord* pHit = NULL;
	xhttpcacherecord* pHeld;
	xhttpcachekey KeyA;
	xhttpcachekey KeyB;
	xhttpcachestats Stats;

	xrtHttpCacheConfigInit(&Config);
	Config.InitialEntries = 2;
	Config.MaxEntries = 2;
	pCache = xrtHttpCacheCreate(&Config);
	testRequire(pCache != NULL,
		"HTTP cache LRU create failed");
	pA = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/a"),
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("a"),
		1
	);
	pB = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/b"),
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("b"),
		2
	);
	pC = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/c"),
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("c"),
		3
	);
	testRequire(
		(pA != NULL) && (pB != NULL) && (pC != NULL) &&
		(xrtHttpCachePut(pCache, pA) ==
		 XHTTP_CACHE_PUT_STORED) &&
		(xrtHttpCachePut(pCache, pB) ==
		 XHTTP_CACHE_PUT_STORED),
		"HTTP cache LRU initial store failed"
	);
	KeyA = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/a"),
		(xstrview){ NULL, 0 },
		NULL,
		0
	);
	KeyB = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/b"),
		(xstrview){ NULL, 0 },
		NULL,
		0
	);
	testRequire(
		xrtHttpCacheGet(pCache, &KeyA, &pHit) ==
			XHTTP_CACHE_LOOKUP_HIT,
		"HTTP cache LRU touch failed"
	);
	pHeld = pHit;
	pHit = NULL;
	testRequire(
		xrtHttpCachePut(pCache, pC) ==
			XHTTP_CACHE_PUT_STORED,
		"HTTP cache LRU third store failed"
	);
	testRequire(
		xrtHttpCacheGet(pCache, &KeyB, &pHit) ==
			XHTTP_CACHE_LOOKUP_MISS,
		"HTTP cache LRU did not evict untouched record"
	);
	testRequire(
		xrtHttpCacheRecordPartAt(pHeld, 0) != NULL &&
		memcmp(
			xrtHttpCacheRecordPartAt(pHeld, 0)->Data.Data,
			"a",
			1
		) == 0,
		"HTTP cache hit reference died after eviction"
	);
	xrtHttpCacheRecordRelease(pHeld);
	testRequire(
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 2) &&
		(Stats.Evictions == 1),
		"HTTP cache LRU stats mismatch"
	);

	xrtHttpCacheRecordRelease(pC);
	xrtHttpCacheRecordRelease(pB);
	xrtHttpCacheRecordRelease(pA);
	xrtHttpCacheRelease(pCache);
}



/* 验证精确删除、URI 删除、清空和单条目限额拒绝。 */
static void testHttpCacheStoreRemoval(void)
{
	xhttpcacheconfig Config;
	xhttpcache* pCache;
	xhttpcacherecord* pA;
	xhttpcacherecord* pB;
	xhttpcachekey Key;
	xhttpcachestats Stats;
	size_t iRemoved;

	pA = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/remove"),
		XRT_STR_LITERAL("one"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("first"),
		1
	);
	pB = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/remove"),
		XRT_STR_LITERAL("two"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("second"),
		2
	);
	testRequire((pA != NULL) && (pB != NULL),
		"HTTP cache removal record create failed");
	xrtHttpCacheConfigInit(&Config);
	Config.MaxEntryBytes =
		xrtHttpCacheRecordCharge(pA) - 1u;
	pCache = xrtHttpCacheCreate(&Config);
	testRequire(
		(pCache != NULL) &&
		(xrtHttpCachePut(pCache, pA) ==
		 XHTTP_CACHE_PUT_REJECTED),
		"HTTP cache entry limit did not reject record"
	);
	xrtHttpCacheRelease(pCache);

	xrtHttpCacheConfigInit(&Config);
	pCache = xrtHttpCacheCreate(&Config);
	testRequire(
		(pCache != NULL) &&
		(xrtHttpCachePut(pCache, pA) ==
		 XHTTP_CACHE_PUT_STORED) &&
		(xrtHttpCachePut(pCache, pB) ==
		 XHTTP_CACHE_PUT_STORED),
		"HTTP cache removal store failed"
	);
	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/remove"),
		XRT_STR_LITERAL("one"),
		NULL,
		0
	);
	testRequire(
		xrtHttpCacheRemove(
			pCache,
			&Key,
			&iRemoved
		) && (iRemoved == 1),
		"HTTP cache exact remove failed"
	);
	testRequire(
		xrtHttpCacheRemoveURI(
			pCache,
			XRT_STR_LITERAL("https://example.test/remove"),
			XRT_STR_LITERAL("two"),
			&iRemoved
		) && (iRemoved == 1),
		"HTTP cache URI remove failed"
	);
	testRequire(
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Bytes == 0) &&
		(Stats.Removals == 2),
		"HTTP cache removal stats mismatch"
	);
	testRequire(
		xrtHttpCachePut(pCache, pA) ==
			XHTTP_CACHE_PUT_STORED,
		"HTTP cache restorage failed"
	);
	testRequire(
		xrtHttpCacheClear(pCache) &&
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Removals == 3),
		"HTTP cache clear failed"
	);

	xrtHttpCacheRelease(pCache);
	xrtHttpCacheRecordRelease(pB);
	xrtHttpCacheRecordRelease(pA);
}



/* 验证条件替换合并 Vary 碰撞后再执行容量限额。 */
static void testHttpCacheStoreVariantMove(void)
{
	static const xhttpfield GzipField = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("gzip")
	};
	static const xhttpfield BrField = {
		XRT_STR_INIT("Accept-Encoding"),
		XRT_STR_INIT("br")
	};
	xhttpcacheconfig Config;
	xhttpcache* pCache;
	xhttpcacherecord* pGzip;
	xhttpcacherecord* pBr;
	xhttpcacherecord* pBrNew;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;

	pGzip = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary-move"),
		XRT_STR_LITERAL("site"),
		XRT_STR_LITERAL("gzip"),
		XRT_STR_LITERAL("gzip"),
		100
	);
	pBr = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary-move"),
		XRT_STR_LITERAL("site"),
		XRT_STR_LITERAL("br"),
		XRT_STR_LITERAL("br"),
		110
	);
	pBrNew = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/vary-move"),
		XRT_STR_LITERAL("site"),
		XRT_STR_LITERAL("br"),
		XRT_STR_LITERAL("br-new-body-is-larger"),
		120
	);
	testRequire(
		(pGzip != NULL) &&
		(pBr != NULL) &&
		(pBrNew != NULL),
		"HTTP cache Vary move record create failed"
	);

	xrtHttpCacheConfigInit(&Config);
	Config.MaxEntryBytes =
		xrtHttpCacheRecordCharge(pBrNew);
	Config.MaxBytes =
		xrtHttpCacheRecordCharge(pBrNew) +
		xrtHttpCacheRecordCharge(pBr) - 1u;
	pCache = xrtHttpCacheCreate(&Config);
	testRequire(
		(pCache != NULL) &&
		(xrtHttpCachePut(
			pCache,
			pGzip
		 ) == XHTTP_CACHE_PUT_STORED) &&
		(xrtHttpCachePut(
			pCache,
			pBr
		 ) == XHTTP_CACHE_PUT_STORED),
		"HTTP cache Vary move initial store failed"
	);
	testRequire(
		xrtHttpCacheReplace(
			pCache,
			pGzip,
			pBrNew
		) == XHTTP_CACHE_PUT_REPLACED,
		"HTTP cache Vary move replace failed"
	);

	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/vary-move"),
		XRT_STR_LITERAL("site"),
		&BrField,
		1
	);
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pHit
		 ) == XHTTP_CACHE_LOOKUP_HIT) &&
		(pHit == pBrNew),
		"HTTP cache Vary move lost replacement"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;
	Key.Fields = &GzipField;
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pHit
		 ) == XHTTP_CACHE_LOOKUP_MISS) &&
		(pHit == NULL) &&
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 1) &&
		(Stats.Bytes ==
		 xrtHttpCacheRecordCharge(pBrNew)) &&
		(Stats.Stores == 3) &&
		(Stats.Replacements == 1) &&
		(Stats.Evictions == 0),
		"HTTP cache Vary move result mismatch"
	);

	xrtHttpCacheRelease(pCache);
	xrtHttpCacheRecordRelease(pBrNew);
	xrtHttpCacheRecordRelease(pBr);
	xrtHttpCacheRecordRelease(pGzip);
}



/* 验证不可变快照的条件替换和条件删除不会覆盖较新的并发结果。 */
static void testHttpCacheStoreCurrent(void)
{
	xhttpcache* pCache = xrtHttpCacheCreate(NULL);
	xhttpcacherecord* pOld;
	xhttpcacherecord* pNew;
	xhttpcacherecord* pStale;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;

	testRequire(
		pCache != NULL,
		"HTTP cache current store create failed"
	);
	pOld = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/current"),
		XRT_STR_LITERAL("site"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("old"),
		100
	);
	pNew = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/current"),
		XRT_STR_LITERAL("site"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("new"),
		200
	);
	pStale = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/current"),
		XRT_STR_LITERAL("site"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("stale"),
		150
	);
	testRequire(
		(pOld != NULL) &&
		(pNew != NULL) &&
		(pStale != NULL),
		"HTTP cache current record create failed"
	);
	testRequire(
		xrtHttpCachePut(pCache, pOld) ==
			XHTTP_CACHE_PUT_STORED,
		"HTTP cache current initial put failed"
	);
	testRequire(
		xrtHttpCacheInsert(
			pCache,
			pStale
		) == XHTTP_CACHE_PUT_CONFLICT,
		"HTTP cache duplicate insert was not rejected"
	);
	testRequire(
		(xrtHttpCacheReplace(
			pCache,
			pOld,
			pNew
		 ) == XHTTP_CACHE_PUT_REPLACED),
		"HTTP cache current replace failed"
	);
	testRequire(
		xrtHttpCacheReplace(
			pCache,
			pOld,
			pStale
		) == XHTTP_CACHE_PUT_CONFLICT,
		"HTTP cache stale writer was not rejected"
	);
	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/current"),
		XRT_STR_LITERAL("site"),
		NULL,
		0
	);
	testRequire(
		(xrtHttpCacheGet(pCache, &Key, &pHit) ==
		 XHTTP_CACHE_LOOKUP_HIT) &&
		(pHit == pNew),
		"HTTP cache conflict replaced the current record"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;
	testRequire(
		(xrtHttpCacheRemoveRecord(
			pCache,
			pOld
		 ) == XHTTP_CACHE_CHANGE_CONFLICT) &&
		(xrtHttpCacheRemoveRecord(
			pCache,
			pNew
		 ) == XHTTP_CACHE_CHANGE_APPLIED),
		"HTTP cache current remove contract failed"
	);
	testRequire(
		(xrtHttpCacheGet(pCache, &Key, &pHit) ==
		 XHTTP_CACHE_LOOKUP_MISS) &&
		(pHit == NULL) &&
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Stores == 2) &&
		(Stats.Replacements == 1) &&
		(Stats.Conflicts == 3) &&
		(Stats.Removals == 1),
		"HTTP cache current statistics mismatch"
	);

	xrtHttpCacheRecordRelease(pStale);
	xrtHttpCacheRecordRelease(pNew);
	xrtHttpCacheRecordRelease(pOld);
	xrtHttpCacheRelease(pCache);
}



/* 自定义后端夹具把全部操作委托给一个真实内存缓存。 */
typedef struct test_http_cache_backend {
	xhttpcache* Memory;
	xhttpcacherecord* Forced;
	size_t Calls;
	bool Closed;
} test_http_cache_backend;



/* 委托一次查询并记录调用。 */
static xhttpcachelookup testHttpCacheBackendGet(
	ptr pContext,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheGet(pBackend->Memory, pKey, ppRecord);
}



/* 模拟命中时返回与查询 Key 不匹配的 Record。 */
static xhttpcachelookup testHttpCacheBackendGetWrong(
	ptr pContext,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	(void)pKey;
	pBackend->Calls++;
	*ppRecord = xrtHttpCacheRecordRetain(pBackend->Forced);
	return XHTTP_CACHE_LOOKUP_HIT;
}



/* 委托一次保存并记录调用。 */
static xhttpcacheput testHttpCacheBackendPut(
	ptr pContext,
	xhttpcacherecord* pRecord
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCachePut(pBackend->Memory, pRecord);
}



/* 模拟违反普通 Put 返回值契约的自定义后端。 */
static xhttpcacheput testHttpCacheBackendPutConflict(
	ptr pContext,
	xhttpcacherecord* pRecord
)
{
	(void)pContext;
	(void)pRecord;
	return XHTTP_CACHE_PUT_CONFLICT;
}



/* 委托一次条件插入并记录调用。 */
static xhttpcacheput testHttpCacheBackendInsert(
	ptr pContext,
	xhttpcacherecord* pRecord
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheInsert(
		pBackend->Memory,
		pRecord
	);
}



/* 委托一次条件替换并记录调用。 */
static xhttpcacheput testHttpCacheBackendReplace(
	ptr pContext,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheReplace(
		pBackend->Memory,
		pExpected,
		pReplacement
	);
}



/* 委托一次按 Record 身份删除并记录调用。 */
static xhttpcachechange testHttpCacheBackendRemoveRecord(
	ptr pContext,
	const xhttpcacherecord* pExpected
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheRemoveRecord(
		pBackend->Memory,
		pExpected
	);
}



/* 委托一次精确删除并记录调用。 */
static bool testHttpCacheBackendRemove(
	ptr pContext,
	const xhttpcachekey* pKey,
	size_t* pRemoved
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheRemove(
		pBackend->Memory,
		pKey,
		pRemoved
	);
}



/* 委托一次 URI 删除并记录调用。 */
static bool testHttpCacheBackendRemoveURI(
	ptr pContext,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheRemoveURI(
		pBackend->Memory,
		URI,
		Partition,
		pRemoved
	);
}



/* 委托清空并记录调用。 */
static bool testHttpCacheBackendClear(ptr pContext)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheClear(pBackend->Memory);
}



/* 委托统计快照并记录调用。 */
static bool testHttpCacheBackendStats(
	ptr pContext,
	xhttpcachestats* pStats
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	pBackend->Calls++;
	return xrtHttpCacheStats(pBackend->Memory, pStats);
}



/* 模拟失败且不初始化输出的统计后端。 */
static bool testHttpCacheBackendStatsFail(
	ptr pContext,
	xhttpcachestats* pStats
)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	(void)pStats;
	pBackend->Calls++;
	return false;
}



/* 记录统一句柄只在最后一个引用释放时关闭后端。 */
static void testHttpCacheBackendClose(ptr pContext)
{
	test_http_cache_backend* pBackend =
		(test_http_cache_backend*)pContext;

	testRequire(
		!pBackend->Closed,
		"HTTP cache custom backend closed twice"
	);
	pBackend->Closed = true;
}



/* 初始化完整的自定义缓存后端操作表。 */
static void testHttpCacheBackendOpsInit(xhttpcacheops* pOps)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Get = testHttpCacheBackendGet;
	pOps->Put = testHttpCacheBackendPut;
	pOps->Insert = testHttpCacheBackendInsert;
	pOps->Replace = testHttpCacheBackendReplace;
	pOps->RemoveRecord = testHttpCacheBackendRemoveRecord;
	pOps->Remove = testHttpCacheBackendRemove;
	pOps->RemoveURI = testHttpCacheBackendRemoveURI;
	pOps->Clear = testHttpCacheBackendClear;
	pOps->Stats = testHttpCacheBackendStats;
	pOps->Close = testHttpCacheBackendClose;
}



/* 验证 Store 配置、后端表、查询和统计输出的固定描述符契约。 */
static void testHttpCacheStoreMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xhttpcacheconfig) + 2u];
	uint8 KeyStorage[sizeof(xhttpcachekey) + 2u];
	uint8 RecordStorage[sizeof(xhttpcacherecord*) + 2u];
	uint8 RemovedStorage[sizeof(size_t) + 2u];
	uint8 StatsStorage[sizeof(xhttpcachestats) + 2u];
	uint8 OpsStorage[sizeof(xhttpcacheops) + 2u];
	test_http_cache_backend Backend;
	xhttpcacheconfig Config;
	xhttpcachekey Key;
	xhttpcacheops Ops;
	xhttpcachestats Stats;
	xhttpcache* pCache;
	xhttpcache* pCustom;
	xhttpcacherecord* pRecord;
	xhttpcacherecord* pHit;
	size_t iRemoved;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(KeyStorage, 0xA5, sizeof(KeyStorage));
	memset(RecordStorage, 0xA5, sizeof(RecordStorage));
	memset(RemovedStorage, 0xA5, sizeof(RemovedStorage));
	memset(StatsStorage, 0xA5, sizeof(StatsStorage));
	memset(OpsStorage, 0xA5, sizeof(OpsStorage));
	xrtHttpCacheConfigInit(
		(xhttpcacheconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	Config.InitialEntries = 1u;
	Config.MaxEntries = 1u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	pCache = xrtHttpCacheCreate(
		(const xhttpcacheconfig*)(const void*)(ConfigStorage + 1u)
	);
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	testRequire(pCache != NULL,
		"HTTP cache Store rejected an unaligned config");
	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/memory"),
		XRT_STR_LITERAL("partition"),
		NULL,
		0
	);
	memcpy(KeyStorage + 1u, &Key, sizeof(Key));
	pRecord = testHttpCacheStoreRecord(
		Key.URI,
		Key.Partition,
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("body"),
		100
	);
	testRequire((pRecord != NULL) &&
		(xrtHttpCachePut(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED),
		"HTTP cache Store memory fixture insert failed");
	testRequire(xrtHttpCacheGet(
		pCache,
		(const xhttpcachekey*)(const void*)(KeyStorage + 1u),
		(xhttpcacherecord**)(void*)(RecordStorage + 1u)
	) == XHTTP_CACHE_LOOKUP_HIT,
		"HTTP cache Get rejected unaligned descriptors");
	memcpy(&pHit, RecordStorage + 1u, sizeof(pHit));
	testRequire(pHit != NULL,
		"HTTP cache Get did not publish an unaligned hit");
	xrtHttpCacheRecordRelease(pHit);
	testRequire(xrtHttpCacheStats(
		pCache,
		(xhttpcachestats*)(void*)(StatsStorage + 1u)
	), "HTTP cache Stats rejected an unaligned output");
	memcpy(&Stats, StatsStorage + 1u, sizeof(Stats));
	testRequire((Stats.Entries == 1u) && (Stats.Hits == 1u),
		"HTTP cache Stats published the wrong snapshot");
	testRequire(xrtHttpCacheRemove(
		pCache,
		(const xhttpcachekey*)(const void*)(KeyStorage + 1u),
		(size_t*)(void*)(RemovedStorage + 1u)
	), "HTTP cache Remove rejected unaligned descriptors");
	memcpy(&iRemoved, RemovedStorage + 1u, sizeof(iRemoved));
	testRequire(iRemoved == 1u,
		"HTTP cache Remove published the wrong count");
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(KeyStorage[0] == 0xA5) &&
		(KeyStorage[sizeof(KeyStorage) - 1u] == 0xA5) &&
		(RecordStorage[0] == 0xA5) &&
		(RecordStorage[sizeof(RecordStorage) - 1u] == 0xA5) &&
		(RemovedStorage[0] == 0xA5) &&
		(RemovedStorage[sizeof(RemovedStorage) - 1u] == 0xA5) &&
		(StatsStorage[0] == 0xA5) &&
		(StatsStorage[sizeof(StatsStorage) - 1u] == 0xA5),
		"HTTP cache Store wrote outside unaligned storage"
	);

	memset(&Backend, 0, sizeof(Backend));
	Backend.Memory = xrtHttpCacheCreate(NULL);
	testRequire(Backend.Memory != NULL,
		"HTTP cache custom memory fixture create failed");
	testHttpCacheBackendOpsInit(&Ops);
	memcpy(OpsStorage + 1u, &Ops, sizeof(Ops));
	pCustom = xrtHttpCacheOpen(
		(const xhttpcacheops*)(const void*)(OpsStorage + 1u),
		&Backend
	);
	memset(OpsStorage + 1u, 0, sizeof(Ops));
	testRequire((pCustom != NULL) && xrtHttpCacheStats(
		pCustom,
		(xhttpcachestats*)(void*)(StatsStorage + 1u)
	), "HTTP cache custom backend did not snapshot unaligned Ops");
	xrtHttpCacheRelease(pCustom);
	testRequire(Backend.Closed,
		"HTTP cache custom backend snapshot lost Close callback");
	xrtHttpCacheRelease(Backend.Memory);

	testRequire(xrtHttpCacheCreate(
		(const xhttpcacheconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP cache Store accepted a wrapping config");
	xrtClearError();
	testRequire(xrtHttpCacheOpen(
		(const xhttpcacheops*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL
	) == NULL, "HTTP cache Store accepted wrapping Ops");
	xrtClearError();
	pHit = (xhttpcacherecord*)(uintptr_t)1u;
	testRequire(xrtHttpCacheGet(
		pCache,
		(const xhttpcachekey*)(uintptr_t)(UINTPTR_MAX - 1u),
		&pHit
	) == XHTTP_CACHE_LOOKUP_ERROR && (pHit == NULL),
		"HTTP cache Get accepted a wrapping key or kept stale output");
	xrtClearError();
	testRequire(!xrtHttpCacheStats(
		pCache,
		(xhttpcachestats*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP cache Stats accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpCacheRemove(
		pCache,
		&Key,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP cache Remove accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpCacheRemoveURI(
		pCache,
		Key.URI,
		Key.Partition,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP cache RemoveURI accepted a wrapping output");
	xrtClearError();
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(pCache);
}



/* 条件写回能力不完整的后端不能绕过原子提交契约。 */
static void testHttpCacheStoreBackendContract(void)
{
	test_http_cache_backend Backend;
	xhttpcacheops Ops;
	xhttpcache* pCache;
	xhttpcacherecord* pRecord;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;
	xhttpcachestats Zero;

	memset(&Backend, 0, sizeof(Backend));
	testHttpCacheBackendOpsInit(&Ops);
	Ops.Insert = NULL;
	testRequire(
		xrtHttpCacheOpen(&Ops, &Backend) == NULL,
		"HTTP cache custom backend accepted missing insert"
	);

	testHttpCacheBackendOpsInit(&Ops);
	Ops.Replace = NULL;
	testRequire(
		xrtHttpCacheOpen(&Ops, &Backend) == NULL,
		"HTTP cache custom backend accepted missing replace"
	);

	testHttpCacheBackendOpsInit(&Ops);
	Ops.RemoveRecord = NULL;
	testRequire(
		xrtHttpCacheOpen(&Ops, &Backend) == NULL,
		"HTTP cache custom backend accepted missing record remove"
	);
	testRequire(
		!Backend.Closed,
		"HTTP cache rejected backend was closed"
	);

	testHttpCacheBackendOpsInit(&Ops);
	Ops.Put = testHttpCacheBackendPutConflict;
	pCache = xrtHttpCacheOpen(&Ops, &Backend);
	pRecord = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/invalid-backend"),
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("record"),
		1
	);
	testRequire(
		(pCache != NULL) &&
		(pRecord != NULL) &&
		(xrtHttpCachePut(
			pCache,
			pRecord
		 ) == XHTTP_CACHE_PUT_ERROR),
		"HTTP cache accepted conflict from unconditional backend put"
	);
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(pCache);
	testRequire(
		Backend.Closed,
		"HTTP cache invalid-result backend did not close"
	);

	memset(&Backend, 0, sizeof(Backend));
	memset(&Zero, 0, sizeof(Zero));
	testHttpCacheBackendOpsInit(&Ops);
	Ops.Get = testHttpCacheBackendGetWrong;
	Ops.Stats = testHttpCacheBackendStatsFail;
	Backend.Forced = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/wrong"),
		XRT_STR_LITERAL("private"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("wrong"),
		2
	);
	pCache = xrtHttpCacheOpen(&Ops, &Backend);
	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/requested"),
		XRT_STR_LITERAL("private"),
		NULL,
		0
	);
	memset(&Stats, 0xff, sizeof(Stats));
	testRequire(
		(pCache != NULL) &&
		(Backend.Forced != NULL) &&
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pHit
		 ) == XHTTP_CACHE_LOOKUP_ERROR) &&
		(pHit == NULL),
		"HTTP cache accepted mismatched backend hit"
	);
	testRequire(
		!xrtHttpCacheStats(pCache, &Stats) &&
		(memcmp(&Stats, &Zero, sizeof(Stats)) == 0),
		"HTTP cache stats failure left undefined output"
	);
	xrtHttpCacheRelease(pCache);
	xrtHttpCacheRecordRelease(Backend.Forced);
	testRequire(
		Backend.Closed,
		"HTTP cache invalid-output backend did not close"
	);
}



/* 验证自定义后端操作、冻结表和最后引用关闭契约。 */
static void testHttpCacheStoreBackend(void)
{
	test_http_cache_backend Backend;
	xhttpcacheops Ops;
	xhttpcache* pCache;
	xhttpcache* pHeld;
	xhttpcacherecord* pRecord;
	xhttpcacherecord* pReplacement;
	xhttpcacherecord* pHit = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;
	size_t iRemoved;

	memset(&Backend, 0, sizeof(Backend));
	Backend.Memory = xrtHttpCacheCreate(NULL);
	testRequire(
		Backend.Memory != NULL,
		"HTTP cache custom backend memory create failed"
	);
	testHttpCacheBackendOpsInit(&Ops);
	pCache = xrtHttpCacheOpen(&Ops, &Backend);
	testRequire(
		pCache != NULL,
		"HTTP cache custom backend open failed"
	);
	memset(&Ops, 0, sizeof(Ops));
	pRecord = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/custom"),
		XRT_STR_LITERAL("partition"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("custom"),
		1
	);
	pReplacement = testHttpCacheStoreRecord(
		XRT_STR_LITERAL("https://example.test/custom"),
		XRT_STR_LITERAL("partition"),
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("replacement"),
		2
	);
	Key = testHttpCacheStoreKey(
		XRT_STR_LITERAL("https://example.test/custom"),
		XRT_STR_LITERAL("partition"),
		NULL,
		0
	);
	testRequire(
		(pRecord != NULL) &&
		(pReplacement != NULL) &&
		(xrtHttpCacheInsert(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED) &&
		(xrtHttpCacheInsert(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_CONFLICT) &&
		(xrtHttpCacheGet(pCache, &Key, &pHit) ==
		 XHTTP_CACHE_LOOKUP_HIT) &&
		(pHit != NULL) &&
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 1),
		"HTTP cache custom backend delegation failed"
	);
	testRequire(
		(xrtHttpCacheReplace(
			pCache,
			pHit,
			pReplacement
		 ) == XHTTP_CACHE_PUT_REPLACED) &&
		(xrtHttpCacheRemoveRecord(
			pCache,
			pHit
		 ) == XHTTP_CACHE_CHANGE_CONFLICT),
		"HTTP cache custom backend conditional replace failed"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pHit
		 ) == XHTTP_CACHE_LOOKUP_HIT) &&
		(pHit == pReplacement) &&
		(xrtHttpCacheRemoveRecord(
			pCache,
			pHit
		 ) == XHTTP_CACHE_CHANGE_APPLIED),
		"HTTP cache custom backend conditional remove failed"
	);
	xrtHttpCacheRecordRelease(pHit);
	pHit = NULL;
	testRequire(
		(xrtHttpCachePut(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED) &&
		xrtHttpCacheRemove(
			pCache,
			&Key,
			&iRemoved
		) && (iRemoved == 1),
		"HTTP cache custom backend exact remove failed"
	);
	testRequire(
		(xrtHttpCachePut(pCache, pRecord) ==
		 XHTTP_CACHE_PUT_STORED) &&
		(xrtHttpCacheRemoveURI(
			pCache,
			Key.URI,
			Key.Partition,
			&iRemoved
		 ) && (iRemoved == 1)),
		"HTTP cache custom backend URI remove failed"
	);
	testRequire(
		xrtHttpCachePut(pCache, pRecord) ==
			XHTTP_CACHE_PUT_STORED,
		"HTTP cache custom backend restorage failed"
	);
	testRequire(
		xrtHttpCacheClear(pCache),
		"HTTP cache custom backend clear failed"
	);
	pHeld = xrtHttpCacheRetain(pCache);
	testRequire(
		(pHeld == pCache) &&
		(Backend.Calls == 14) &&
		!Backend.Closed,
		"HTTP cache custom backend call accounting failed"
	);
	xrtHttpCacheRelease(pCache);
	testRequire(
		!Backend.Closed,
		"HTTP cache custom backend closed before last reference"
	);
	xrtHttpCacheRelease(pHeld);
	testRequire(
		Backend.Closed,
		"HTTP cache custom backend did not close"
	);

	xrtHttpCacheRecordRelease(pReplacement);
	xrtHttpCacheRecordRelease(pRecord);
	xrtHttpCacheRelease(Backend.Memory);
}



/* 执行内存缓存和可扩展后端的完整存储契约。 */
int main(void)
{
	testHttpCacheStoreVariants();
	testHttpCacheStoreLRU();
	testHttpCacheStoreRemoval();
	testHttpCacheStoreVariantMove();
	testHttpCacheStoreCurrent();
	testHttpCacheStoreMemoryContracts();
	testHttpCacheStoreBackendContract();
	testHttpCacheStoreBackend();
	printf("[PASS] HTTP cache store\n");
	return 0;
}
