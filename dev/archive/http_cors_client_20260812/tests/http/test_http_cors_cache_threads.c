#include "../test.h"

#include <xrt/http_cors_cache.h>



/* 每个工作线程反复刷新自己的 URL，并查询共享缓存。 */
typedef struct test_cors_cache_thread {
	xhttpcorscache* Cache;
	xhttporigin Origin;
	char URL[64];
	size_t URLSize;
} test_cors_cache_thread;



/* 执行共享 Cache 的刷新和命中循环。 */
static int32 testCorsCacheThreadEntry(ptr pData)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("*") }
	};
	test_cors_cache_thread* pThread =
		(test_cors_cache_thread*)pData;
	xhttpcorscachekey Key;
	xhttpcorsclientresult Result = {
		60u,
		XHTTP_CORS_CLIENT_REJECT_NONE,
		XHTTP_CORS_CLIENT_ALLOW |
			XHTTP_CORS_CLIENT_PREFLIGHT
	};
	xhttpcorspreflightplan Plan;
	size_t i;

	if ( !xrtHttpCorsCacheKeyInit(
		&Key,
		&pThread->Origin,
		(xstrview){ pThread->URL, pThread->URLSize }
	) ) {
		return 1;
	}
	for ( i = 0; i < 300u; i++ ) {
		if ( !xrtHttpCorsCacheUpdate(
			pThread->Cache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			false,
			false,
			Response,
			2u,
			&Result,
			NULL
		) ) {
			return 2;
		}
		if ( !xrtHttpCorsCachePlan(
			pThread->Cache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			Request,
			1u,
			false,
			false,
			&Plan
		) || ((Plan.Flags &
			XHTTP_CORS_PREFLIGHT_CACHED) == 0) ) {
			return 3;
		}
	}
	return 0;
}



/* 验证共享缓存的 Mutex、Map、LRU 和统计一致性。 */
int main(void)
{
	xhttpcorscache* pCache = xrtHttpCorsCacheCreate(NULL);
	test_cors_cache_thread Contexts[4];
	xthread* Threads[4];
	xhttpcorscachestats Stats;
	size_t i;

	testRequire(pCache != NULL,
		"CORS thread cache create failed");
	for ( i = 0; i < 4u; i++ ) {
		testRequire(xrtHttpOriginParse(
			XRT_STR_LITERAL("https://app.example"),
			&Contexts[i].Origin
		), "CORS thread Origin parse failed");
		Contexts[i].Cache = pCache;
		Contexts[i].URLSize = (size_t)snprintf(
			Contexts[i].URL,
			sizeof(Contexts[i].URL),
			"https://api.example/worker/%u",
			(unsigned)i
		);
		Threads[i] = xrtThreadCreate(
			testCorsCacheThreadEntry,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"CORS cache worker create failed");
	}
	for ( i = 0; i < 4u; i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"CORS cache worker failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(xrtHttpCorsCacheStats(pCache, &Stats) &&
		(Stats.Entries == 8u) &&
		(Stats.Stores == 8u) &&
		(Stats.Replacements == 2392u) &&
		(Stats.Lookups == 1200u) &&
		(Stats.Hits == 1200u) &&
		(Stats.Misses == 0),
		"CORS concurrent cache statistics mismatch");
	xrtHttpCorsCacheRelease(pCache);
	printf("[PASS] http_cors_cache_threads\n");
	return 0;
}
