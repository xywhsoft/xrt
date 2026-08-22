#include "../test.h"

#include <xrt/http_cors_cache.h>



/* 解析缓存测试使用的请求 Origin。 */
static xhttporigin testCorsCacheOrigin(xstrview Text)
{
	xhttporigin Origin;

	testRequire(
		xrtHttpOriginParse(Text, &Origin),
		"CORS cache Origin parse failed"
	);
	return Origin;
}



/* 创建带可选分区的预检缓存键。 */
static xhttpcorscachekey testCorsCacheKey(
	const xhttporigin* pOrigin,
	xstrview URL,
	xstrview Partition
)
{
	xhttpcorscachekey Key;

	testRequire(
		xrtHttpCorsCacheKeyInit(&Key, pOrigin, URL),
		"CORS cache key init failed"
	);
	Key.Partition = Partition;
	return Key;
}



/* 校验一个成功预检响应并返回可直接写入缓存的结果。 */
static xhttpcorsclientresult testCorsCacheCheck(
	const xhttporigin* pOrigin,
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount
)
{
	xhttpcorsclientresult Result;

	testRequire(xrtHttpCorsPreflightCheck(
		204u,
		pOrigin,
		Method,
		pRequestFields,
		iRequestFieldCount,
		bCredentials,
		pResponseFields,
		iResponseFieldCount,
		&Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0),
		"CORS cache preflight check failed");
	return Result;
}



/* 验证方法、字段、分区、更新、命中和主动删除。 */
static void testCorsCacheBasic(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("600") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey Key = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/items"),
		XRT_STR_LITERAL("site-a")
	);
	xhttpcorscachekey Other = Key;
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);
	size_t iChanged;

	testRequire(pCache != NULL,
		"CORS cache create failed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_REQUIRED) != 0) &&
		((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS empty cache unexpectedly hit");
	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Request,
		2u,
		false,
		Response,
		4u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache,
		&Key,
		XRT_STR_LITERAL("PATCH"),
		false,
		false,
		Response,
		4u,
		&Result,
		&iChanged
	) && (iChanged == 4u),
		"CORS cache permission update mismatch");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS explicit permissions did not hit");
	Other.Partition = XRT_STR_LITERAL("site-b");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Other, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS cache crossed network partitions");
	testRequire(xrtHttpCorsCacheRemove(
		pCache, &Key, &iChanged
	) && (iChanged == 4u),
		"CORS cache key remove mismatch");
	testRequire(xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) && (Stats.Lookups == 3u) &&
		(Stats.Hits == 1u) && (Stats.Misses == 2u) &&
		(Stats.Stores == 4u) && (Stats.Removals == 4u),
		"CORS cache basic statistics mismatch");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证大小写、空端口和显式默认端口共享同一个 Origin 缓存键。 */
static void testCorsCacheCanonicalOrigin(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") }
	};
	xhttporigin Stored = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://APP.example:")
	);
	xhttporigin Lookup = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example:443")
	);
	xhttpcorscachekey StoredKey = testCorsCacheKey(
		&Stored,
		XRT_STR_LITERAL("https://api.example/items"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorscachekey LookupKey = testCorsCacheKey(
		&Lookup,
		XRT_STR_LITERAL("https://api.example/items"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);
	size_t iChanged = 0;

	testRequire(pCache != NULL,
		"CORS canonical Origin cache create failed");
	Result = testCorsCacheCheck(
		&Stored,
		XRT_STR_LITERAL("PATCH"),
		NULL,
		0,
		false,
		Response,
		2u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache,
		&StoredKey,
		XRT_STR_LITERAL("PATCH"),
		false,
		false,
		Response,
		2u,
		&Result,
		&iChanged
	) && (iChanged == 1u) &&
		xrtHttpCorsCachePlan(
			pCache,
			&LookupKey,
			XRT_STR_LITERAL("PATCH"),
			NULL,
			0,
			false,
			false,
			&Plan
		) && ((Plan.Flags &
			XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS equivalent Origin cache key missed");
	testRequire(xrtHttpCorsCacheRemove(
		pCache, &LookupKey, &iChanged
	) && (iChanged == 1u),
		"CORS equivalent Origin cache key did not remove");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证凭据方向、通配权限和 Authorization 例外。 */
static void testCorsCacheCredentials(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Authorization[] = {
		{ XRT_STR_INIT("Authorization"), XRT_STR_INIT("Bearer token") }
	};
	static const xhttpfield Wildcard[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("*") }
	};
	static const xhttpfield Credentialed[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Credentials"), XRT_STR_INIT("true") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("x-trace") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey Key = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/private"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);

	testRequire(pCache != NULL,
		"CORS credential cache create failed");
	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Request,
		1u,
		false,
		Wildcard,
		3u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		false, false, Wildcard, 3u, &Result, NULL
	), "CORS wildcard cache update failed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Request, 1u, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS non-credential wildcard cache missed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Request, 1u, true, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS non-credential permission covered credentials");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Authorization, 1u, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS wildcard covered Authorization");

	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Request,
		1u,
		true,
		Credentialed,
		4u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		false, true, Credentialed, 4u, &Result, NULL
	), "CORS credential cache update failed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		Request, 1u, true, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS credential permission did not hit");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证强制安全方法、零秒失效和畸形列表失败原子性。 */
static void testCorsCacheForcedAndZero(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") }
	};
	static const xhttpfield Zero[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("0") }
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("GET") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("bad item") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey Key = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/forced"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);
	size_t iChanged;

	testRequire(pCache != NULL,
		"CORS forced cache create failed");
	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		false,
		Response,
		1u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("GET"),
		true, false, Response, 1u, &Result, &iChanged
	) && (iChanged == 1u),
		"CORS forced method was not cached");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("GET"),
		NULL, 0, false, true, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_FORCED) != 0) &&
		((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS forced method cache missed");
	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		false,
		Zero,
		2u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("GET"),
		true, false, Zero, 2u, &Result, &iChanged
	) && (iChanged == 1u),
		"CORS zero Max-Age did not invalidate permission");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("GET"),
		NULL, 0, false, true, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS zero Max-Age permission remained active");

	Result.Flags = XHTTP_CORS_CLIENT_ALLOW |
		XHTTP_CORS_CLIENT_PREFLIGHT;
	Result.Reject = XHTTP_CORS_CLIENT_REJECT_NONE;
	Result.MaxAge = 60u;
	testRequire(!xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("GET"),
		false, false, Invalid, 2u, &Result, NULL
	), "CORS malformed cache response was accepted");
	xrtClearError();
	testRequire(xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0),
		"CORS failed update published partial permissions");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证单项容量执行确定的 LRU 淘汰。 */
static void testCorsCacheLimit(void)
{
	static const xhttpfield PatchResponse[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") }
	};
	static const xhttpfield PutResponse[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PUT") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey First = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/first"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorscachekey Second = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/second"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorscacheconfig Config;
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache;

	xrtHttpCorsCacheConfigInit(&Config);
	Config.InitialEntries = 0;
	Config.MaxEntries = 1u;
	pCache = xrtHttpCorsCacheCreate(&Config);
	testRequire(pCache != NULL,
		"CORS limited cache create failed");
	Result = testCorsCacheCheck(
		&Origin, XRT_STR_LITERAL("PATCH"),
		NULL, 0, false, PatchResponse, 2u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &First, XRT_STR_LITERAL("PATCH"),
		false, false, PatchResponse, 2u, &Result, NULL
	), "CORS first limited update failed");
	Result = testCorsCacheCheck(
		&Origin, XRT_STR_LITERAL("PUT"),
		NULL, 0, false, PutResponse, 2u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Second, XRT_STR_LITERAL("PUT"),
		false, false, PutResponse, 2u, &Result, NULL
	), "CORS second limited update failed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &First, XRT_STR_LITERAL("PATCH"),
		NULL, 0, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) == 0),
		"CORS LRU did not evict oldest permission");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Second, XRT_STR_LITERAL("PUT"),
		NULL, 0, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS LRU evicted newest permission");
	testRequire(xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 1u) && (Stats.Evictions == 1u),
		"CORS LRU statistics mismatch");
	testRequire(xrtHttpCorsCacheClear(pCache) &&
		xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0),
		"CORS cache clear failed");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证单次响应的准备分配不会先越过缓存硬上限再依赖 LRU 回收。 */
static void testCorsCacheBatchLimit(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT, DELETE") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("X-A, X-B, X-C") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey Key = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/batch"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorscacheconfig Config;
	xhttpcorsclientresult Result;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache;
	size_t iUpdated = 0;

	xrtHttpCorsCacheConfigInit(&Config);
	Config.InitialEntries = 0;
	Config.MaxEntries = 2u;
	pCache = xrtHttpCorsCacheCreate(&Config);
	testRequire(pCache != NULL,
		"CORS batch-limited cache create failed");
	Result = testCorsCacheCheck(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		NULL,
		0,
		false,
		Response,
		3u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache,
		&Key,
		XRT_STR_LITERAL("PATCH"),
		false,
		false,
		Response,
		3u,
		&Result,
		&iUpdated
	) && (iUpdated == 2u) &&
		xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 2u) &&
		(Stats.Stores == 2u) &&
		(Stats.Evictions == 0),
		"CORS batch update exceeded its hard allocation limit");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证响应寿命受本地上限钳制，并可按单调时钟清理。 */
static void testCorsCacheExpiry(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("600") }
	};
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	xhttpcorscachekey Key = testCorsCacheKey(
		&Origin,
		XRT_STR_LITERAL("https://api.example/expiring"),
		(xstrview){ NULL, 0 }
	);
	xhttpcorscacheconfig Config;
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache;
	size_t iRemoved;

	xrtHttpCorsCacheConfigInit(&Config);
	Config.InitialEntries = 0;
	Config.MaxAge = 1u;
	pCache = xrtHttpCorsCacheCreate(&Config);
	testRequire(pCache != NULL,
		"CORS expiring cache create failed");
	Result = testCorsCacheCheck(
		&Origin, XRT_STR_LITERAL("PATCH"),
		NULL, 0, false, Response, 3u
	);
	testRequire(xrtHttpCorsCacheUpdate(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		false, false, Response, 3u, &Result, NULL
	), "CORS expiring cache update failed");
	testRequire(xrtHttpCorsCachePlan(
		pCache, &Key, XRT_STR_LITERAL("PATCH"),
		NULL, 0, false, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0),
		"CORS expiring permission missed before deadline");
	xrtSleep(1100u);
	testRequire(xrtHttpCorsCachePurge(
		pCache, &iRemoved
	) && (iRemoved == 1u),
		"CORS expired permission purge mismatch");
	testRequire(xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) && (Stats.Expired == 1u),
		"CORS expired permission statistics mismatch");
	xrtHttpCorsCacheRelease(pCache);
}



/* 验证公开描述符和统计支持未对齐存储。 */
static void testCorsCacheUnaligned(void)
{
	xhttporigin Origin = testCorsCacheOrigin(
		XRT_STR_LITERAL("https://app.example")
	);
	uint8 KeyStorage[sizeof(xhttpcorscachekey) + 1u];
	uint8 StatsStorage[sizeof(xhttpcorscachestats) + 1u];
	xhttpcorscachekey Key;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);
	size_t iRemoved;

	testRequire(pCache != NULL,
		"CORS unaligned cache create failed");
	testRequire(xrtHttpCorsCacheKeyInit(
		(xhttpcorscachekey*)(KeyStorage + 1u),
		&Origin,
		XRT_STR_LITERAL("https://api.example/unaligned")
	), "CORS unaligned key init failed");
	memcpy(&Key, KeyStorage + 1u, sizeof(Key));
	testRequire(xrtHttpCorsCacheRemove(
		pCache, &Key, &iRemoved
	) && (iRemoved == 0),
		"CORS empty unaligned key removal failed");
	testRequire(xrtHttpCorsCacheStats(
		pCache,
		(xhttpcorscachestats*)(StatsStorage + 1u)
	), "CORS unaligned stats failed");
	memcpy(&Stats, StatsStorage + 1u, sizeof(Stats));
	testRequire(Stats.Entries == 0,
		"CORS unaligned stats mismatch");
	testRequire(xrtHttpCorsCachePurge(
		pCache, &iRemoved
	) && (iRemoved == 0),
		"CORS empty purge failed");
	xrtHttpCorsCacheRelease(pCache);
}



/* 执行 CORS 预检缓存契约测试。 */
int main(void)
{
	testCorsCacheBasic();
	testCorsCacheCanonicalOrigin();
	testCorsCacheCredentials();
	testCorsCacheForcedAndZero();
	testCorsCacheLimit();
	testCorsCacheBatchLimit();
	testCorsCacheExpiry();
	testCorsCacheUnaligned();
	printf("[PASS] http_cors_cache\n");
	return 0;
}
