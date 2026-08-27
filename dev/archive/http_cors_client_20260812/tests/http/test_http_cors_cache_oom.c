#include "../test.h"

#include <xrt/http_cors_cache.h>



/* 扫描预检缓存创建过程中的全部逻辑分配点。 */
static void testHttpCorsCacheCreateOom(void)
{
	xhttpcorscacheconfig Config;
	size_t iFailures = 0;
	bool bComplete = false;

	xrtHttpCorsCacheConfigInit(&Config);
	Config.InitialEntries = 4u;
	Config.MaxEntries = 16u;
	for ( size_t iFail = 0; iFail < 32u; iFail++ ) {
		xhttpcorscache* pCache;
		bool bTriggered;

		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"CORS cache create OOM setup failed"
		);
		pCache = xrtHttpCorsCacheCreate(&Config);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pCache == NULL ) {
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"CORS cache create failed without injected OOM"
			);
			iFailures++;
		} else {
			testRequire(
				!bTriggered,
				"CORS cache create ignored an allocation fault"
			);
			xrtHttpCorsCacheRelease(pCache);
			bComplete = true;
		}
		xrtClearError();
		testMemoryDebugDrain(
			"CORS cache create OOM leaked storage"
		);
		if ( bComplete ) {
			break;
		}
	}
	testRequire(
		bComplete && (iFailures != 0),
		"CORS cache create OOM sweep did not converge"
	);
}



/* 构造不会分配内存的预检响应与缓存键。 */
static void testHttpCorsCacheOomFixture(
	xhttporigin* pOrigin,
	xhttpcorscachekey* pKey,
	xhttpcorsclientresult* pResult
)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT, DELETE") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace, x-token") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("60") }
	};

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://app.example"), pOrigin
		) && xrtHttpCorsCacheKeyInit(
			pKey,
			pOrigin,
			XRT_STR_LITERAL("https://api.example/oom")
		) && xrtHttpCorsPreflightCheck(
			204u,
			pOrigin,
			XRT_STR_LITERAL("PATCH"),
			Request,
			2u,
			false,
			Response,
			4u,
			pResult
		),
		"CORS cache update OOM fixture failed"
	);
}



/* 扫描批量更新分配点并验证失败时不会发布部分权限。 */
static void testHttpCorsCacheUpdateOom(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT, DELETE") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace, x-token") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("60") }
	};
	xhttpcorscacheconfig Config;
	xhttporigin Origin;
	xhttpcorscachekey Key;
	xhttpcorsclientresult Result;
	size_t iFailures = 0;
	bool bComplete = false;

	testHttpCorsCacheOomFixture(&Origin, &Key, &Result);
	xrtHttpCorsCacheConfigInit(&Config);
	Config.InitialEntries = 0;
	Config.MaxEntries = 16u;
	for ( size_t iFail = 0; iFail < 32u; iFail++ ) {
		xhttpcorscache* pCache;
		xhttpcorscachestats Stats;
		xhttpcorspreflightplan Plan;
		size_t iUpdated = SIZE_MAX;
		bool bUpdated;
		bool bTriggered;

		pCache = xrtHttpCorsCacheCreate(&Config);
		testRequire(
			pCache != NULL,
			"CORS cache update OOM cache creation failed"
		);
		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"CORS cache update OOM setup failed"
		);
		bUpdated = xrtHttpCorsCacheUpdate(
			pCache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			false,
			false,
			Response,
			4u,
			&Result,
			&iUpdated
		);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		testRequire(
			xrtHttpCorsCacheStats(pCache, &Stats),
			"CORS cache update OOM stats failed"
		);
		if ( !bUpdated ) {
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(iUpdated == 0) && (Stats.Entries == 0),
				"CORS cache update OOM published partial permissions"
			);
			iFailures++;
		} else {
			testRequire(
				!bTriggered && (iUpdated == 6u) &&
				(Stats.Entries == 6u) &&
				xrtHttpCorsCachePlan(
					pCache,
					&Key,
					XRT_STR_LITERAL("PATCH"),
					NULL,
					0,
					false,
					false,
					&Plan
				) && ((Plan.Flags &
					XHTTP_CORS_PREFLIGHT_CACHED) != 0),
				"CORS cache update OOM retry state mismatch"
			);
			bComplete = true;
		}
		xrtClearError();
		xrtHttpCorsCacheRelease(pCache);
		testMemoryDebugDrain(
			"CORS cache update OOM leaked storage"
		);
		if ( bComplete ) {
			break;
		}
	}
	testRequire(
		bComplete && (iFailures != 0),
		"CORS cache update OOM sweep did not converge"
	);
}



/* 验证零寿命撤销在故障注入下不请求任何内存。 */
static void testHttpCorsCacheZeroNoAlloc(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Stored[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT, DELETE") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace, x-token") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("60") }
	};
	static const xhttpfield Zero[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH, PUT, DELETE") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace, x-token") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("0") }
	};
	xhttporigin Origin;
	xhttpcorscachekey Key;
	xhttpcorsclientresult StoredResult;
	xhttpcorsclientresult ZeroResult;
	xhttpcorscachestats Stats;
	xhttpcorscache* pCache;
	size_t iUpdated = SIZE_MAX;
	bool bTriggered;

	testHttpCorsCacheOomFixture(&Origin, &Key, &StoredResult);
	pCache = xrtHttpCorsCacheCreate(NULL);
	testRequire(
		(pCache != NULL) && xrtHttpCorsCacheUpdate(
			pCache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			false,
			false,
			Stored,
			4u,
			&StoredResult,
			NULL
		) && xrtHttpCorsPreflightCheck(
			204u,
			&Origin,
			XRT_STR_LITERAL("PATCH"),
			Request,
			2u,
			false,
			Zero,
			4u,
			&ZeroResult
		),
		"CORS zero-age no-allocation fixture failed"
	);
	testRequire(
		xrtMemDebugFailAfter(0),
		"CORS zero-age fault setup failed"
	);
	testRequire(
		xrtHttpCorsCacheUpdate(
			pCache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			false,
			false,
			Zero,
			4u,
			&ZeroResult,
			&iUpdated
		),
		"CORS zero-age revocation failed under allocation fault"
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		!bTriggered && (iUpdated == 6u) &&
		xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) && (Stats.Removals == 6u),
		"CORS zero-age revocation allocated or left permissions"
	);
	xrtHttpCorsCacheRelease(pCache);
	testMemoryDebugDrain(
		"CORS zero-age no-allocation test leaked storage"
	);
}



int main(void)
{
	testHttpCorsCacheCreateOom();
	testHttpCorsCacheUpdateOom();
	testHttpCorsCacheZeroNoAlloc();
	puts("[PASS] http_cors_cache_oom");
	return 0;
}
