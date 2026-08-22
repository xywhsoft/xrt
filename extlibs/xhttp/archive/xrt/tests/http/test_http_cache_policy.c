#include "../test.h"

#include <xrt/http_cache_policy.h>



/* 解析一个 Cache-Control 值，空视图表示没有该字段。 */
static void testHttpCachePolicyControl(
	xstrview Value,
	xhttpcachecontrol* pControl
)
{
	xhttpfield Field;

	if ( Value.Size == 0 ) {
		xrtHttpCacheControlInit(pControl);
		return;
	}
	Field.Name = XRT_STR_LITERAL("Cache-Control");
	Field.Value = Value;
	testRequire(
		xrtHttpCacheControlParse(
			&Field, 1, pControl
		),
		"HTTP cache policy control setup failed"
	);
}



/* 构造公式一致的整秒缓存年龄。 */
static void testHttpCachePolicyAge(
	uint64 iSeconds,
	xhttpcacheage* pAge
)
{
	uint64 iTime = iSeconds *
		(uint64)XRT_TIME_SECOND;

	memset(pAge, 0, sizeof(*pAge));
	pAge->ApparentAge = iTime;
	pAge->CorrectedInitialAge = iTime;
	pAge->CurrentAge = iTime;
	pAge->CurrentAgeSeconds = iSeconds;
	testRequire(
		xrtHttpCacheAgeValid(pAge),
		"HTTP cache policy age setup failed"
	);
}



/* 验证默认方法、状态和最小存储许可。 */
static void testHttpCachePolicyDefaults(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcachestoreinput Input;
	xhttpcachestoreplan Plan;

	testRequire(
		xrtHttpCacheMethodDefault(
			XRT_STR_LITERAL("GET")
		) &&
		xrtHttpCacheMethodDefault(
			XRT_STR_LITERAL("HEAD")
		) &&
		xrtHttpCacheMethodDefault(
			XRT_STR_LITERAL("POST")
		) &&
		!xrtHttpCacheMethodDefault(
			XRT_STR_LITERAL("PUT")
		) &&
		xrtHttpCacheStatusHeuristic(200) &&
		xrtHttpCacheStatusHeuristic(308) &&
		xrtHttpCacheStatusHeuristic(404) &&
		!xrtHttpCacheStatusHeuristic(302),
		"HTTP cache policy defaults mismatch"
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Response
	);
	xrtHttpCacheTimeInit(&Time);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			200,
			false
		) &&
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_KEEP) &&
		((Plan.Actions & (
			XHTTP_CACHE_STORE_REMOVE_CONNECTION |
			XHTTP_CACHE_STORE_REMOVE_PROXY |
			XHTTP_CACHE_STORE_SEPARATE_TRAILERS
		 )) == (
			XHTTP_CACHE_STORE_REMOVE_CONNECTION |
			XHTTP_CACHE_STORE_REMOVE_PROXY |
			XHTTP_CACHE_STORE_SEPARATE_TRAILERS
		 )),
		"heuristically cacheable GET was rejected"
	);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			302,
			false
		) &&
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_NO_PERMISSION) != 0),
		"non-heuristic response was stored implicitly"
	);
}



/* 验证请求和响应禁止、限定字段及 must-understand。 */
static void testHttpCachePolicyStorageControls(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcachestoreinput Input;
	xhttpcachestoreplan Plan;

	xrtHttpCacheTimeInit(&Time);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			200,
			true
		),
		"shared cache input setup failed"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("no-store"), &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("public"), &Response
	);
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_REQUEST_NO_STORE) != 0),
		"request no-store did not block storage"
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("no-store"), &Response
	);
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_RESPONSE_NO_STORE) != 0),
		"response no-store did not block storage"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL(
			"public, must-understand, no-store"
		),
		&Response
	);
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_KEEP) &&
		((Plan.Actions &
		  XHTTP_CACHE_STORE_IGNORE_NO_STORE) != 0),
		"must-understand did not override no-store"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("private"), &Response
	);
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_SHARED_PRIVATE) != 0),
		"unqualified private entered shared cache"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL(
			"private=\"Set-Cookie\", public, "
			"no-cache=\"X-Secret\""
		),
		&Response
	);
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_KEEP) &&
		((Plan.Actions & (
			XHTTP_CACHE_STORE_REMOVE_PRIVATE |
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE
		 )) == (
			XHTTP_CACHE_STORE_REMOVE_PRIVATE |
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE
		 )),
		"qualified cache fields did not produce removal actions"
	);
}



/* 验证认证响应、POST、部分响应和不完整响应的存储边界。 */
static void testHttpCachePolicyStorageEdges(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcachestoreinput Input;
	xhttpcachestoreplan Plan;

	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Response
	);
	xrtHttpCacheTimeInit(&Time);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			200,
			true
		),
		"authorization input setup failed"
	);
	Input.Flags |= XHTTP_CACHE_STORE_AUTHORIZATION;
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_AUTHORIZATION) != 0),
		"authenticated response entered shared cache"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("public"), &Response
	);
	testRequire(
		xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		) == XHTTP_CACHE_STORE_KEEP,
		"public did not permit authenticated response"
	);

	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=60"), &Response
	);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("POST"),
			200,
			false
		) &&
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_POST_REQUIREMENTS) != 0),
		"POST without matching Content-Location was stored"
	);
	Input.Flags |=
		XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH;
	testRequire(
		xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		) == XHTTP_CACHE_STORE_KEEP,
		"explicit fresh POST with Content-Location was rejected"
	);

	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Response
	);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			206,
			false
		) &&
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_SKIP) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED) != 0),
		"206 was stored without Range support"
	);
	Input.Flags |= XHTTP_CACHE_STORE_RANGE_SUPPORTED;
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_KEEP) &&
		((Plan.Actions & (
			XHTTP_CACHE_STORE_MARK_INCOMPLETE |
			XHTTP_CACHE_STORE_AS_200
		 )) == (
			XHTTP_CACHE_STORE_MARK_INCOMPLETE |
			XHTTP_CACHE_STORE_AS_200
		 )),
		"206 storage actions mismatch"
	);
	testRequire(
		xrtHttpCacheStoreInputInit(
			&Input,
			XRT_STR_LITERAL("GET"),
			200,
			false
		),
		"incomplete response input setup failed"
	);
	Input.Flags &=
		~(uint32)XHTTP_CACHE_STORE_RESPONSE_COMPLETE;
	Input.Flags |= XHTTP_CACHE_STORE_RANGE_SUPPORTED;
	testRequire(
		(xrtHttpCacheStorePlan(
			&Request, &Response, &Time,
			&Input, &Plan
		 ) == XHTTP_CACHE_STORE_KEEP) &&
		((Plan.Actions &
		  XHTTP_CACHE_STORE_MARK_INCOMPLETE) != 0),
		"incomplete GET was not marked"
	);
}



/* 验证新鲜响应的请求约束和 no-cache 两种响应形式。 */
static void testHttpCachePolicyFreshUse(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcacheuseinput Input;
	xhttpcacheuseplan Plan;

	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=60"), &Response
	);
	testHttpCachePolicyAge(10, &Age);
	Freshness.Lifetime =
		UINT64_C(60000000);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_MAX_AGE;
	testRequire(
		xrtHttpCacheUseInputInit(
			&Input, 200, false
		) &&
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_STORED) &&
		((Plan.Actions &
		  XHTTP_CACHE_USE_SET_AGE) != 0),
		"fresh response was not reused"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=5"), &Request
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_VALIDATE,
		"request max-age did not require validation"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("min-fresh=51"), &Request
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_VALIDATE,
		"request min-fresh did not require validation"
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL(
			"max-age=60, no-cache=\"Set-Cookie\""
		),
		&Response
	);
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_STORED) &&
		((Plan.Actions &
		  XHTTP_CACHE_USE_REMOVE_NO_CACHE) != 0),
		"qualified no-cache was not reused with field removal"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=60, no-cache"),
		&Response
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_VALIDATE,
		"unqualified no-cache was reused"
	);
}



/* 验证陈旧响应、断网、强制验证和 only-if-cached。 */
static void testHttpCachePolicyStaleUse(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcacheuseinput Input;
	xhttpcacheuseplan Plan;

	testHttpCachePolicyAge(70, &Age);
	Freshness.Lifetime =
		UINT64_C(60000000);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_MAX_AGE;
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-stale=10"), &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=60"), &Response
	);
	testRequire(
		xrtHttpCacheUseInputInit(
			&Input, 200, false
		) &&
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_STORED) &&
		((Plan.Actions &
		  XHTTP_CACHE_USE_STALE) != 0) &&
		(Plan.StaleBy ==
		 UINT64_C(10000000)),
		"bounded max-stale did not permit boundary value"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-stale=9"), &Request
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_VALIDATE,
		"max-stale limit was exceeded"
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	Input.Flags |= XHTTP_CACHE_USE_DISCONNECTED;
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_STORED,
		"disconnected cache did not serve permitted stale response"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL(
			"max-age=60, must-revalidate"
		),
		&Response
	);
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_GATEWAY_TIMEOUT) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_RESPONSE_REVALIDATE) != 0),
		"must-revalidate was ignored while disconnected"
	);
	Input.Flags &=
		~(uint32)XHTTP_CACHE_USE_DISCONNECTED;
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("no-cache, only-if-cached"),
		&Request
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_GATEWAY_TIMEOUT,
		"only-if-cached forwarded validation"
	);
}



/* 验证候选失配、无时钟、淘汰、验证和扩展入口。 */
static void testHttpCachePolicyUseEdges(void)
{
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcacheuseinput Input;
	xhttpcacheuseplan Plan;
	xhttpcacheuseplan Before;

	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Request
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("max-age=60"), &Response
	);
	testHttpCachePolicyAge(10, &Age);
	Freshness.Lifetime =
		UINT64_C(60000000);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_HEURISTIC;
	testRequire(
		xrtHttpCacheUseInputInit(
			&Input, 200, false
		),
		"use edge input setup failed"
	);
	Input.Flags &=
		~(uint32)XHTTP_CACHE_USE_CANDIDATE_MATCH;
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_FORWARD,
		"candidate miss did not forward"
	);
	Input.Flags |= XHTTP_CACHE_USE_CANDIDATE_MATCH;
	Input.Flags &=
		~(uint32)XHTTP_CACHE_USE_CLOCK;
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_VALIDATE,
		"cache without clock reused response"
	);
	Input.Flags |=
		XHTTP_CACHE_USE_CLOCK |
		XHTTP_CACHE_USE_VALIDATED;
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_STORED,
		"successfully validated response was not reused"
	);
	Input.Flags &=
		~(uint32)XHTTP_CACHE_USE_VALIDATED;
	Input.Flags |= XHTTP_CACHE_USE_EXTENSION;
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("no-store"), &Response
	);
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_STORED) &&
		((Plan.Reasons &
		  XHTTP_CACHE_REASON_EXTENSION) != 0),
		"explicit cache extension could not override policy"
	);
	Input.Flags &=
		~(uint32)XHTTP_CACHE_USE_EXTENSION;
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_FORWARD) &&
		((Plan.Actions &
		  XHTTP_CACHE_USE_EVICT) != 0),
		"stored no-store response was not evicted"
	);
	testHttpCachePolicyControl(
		(xstrview){ NULL, 0 }, &Response
	);
	Freshness.Lifetime = 0;
	Freshness.Source = XHTTP_CACHE_FRESHNESS_NONE;
	Input.Flags |=
		XHTTP_CACHE_USE_DISCONNECTED |
		XHTTP_CACHE_USE_STALE_ALLOWED;
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_GATEWAY_TIMEOUT,
		"response without any lifetime was served stale"
	);
	Freshness.Lifetime = UINT64_C(60000000);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_HEURISTIC;
	Input.Flags =
		XHTTP_CACHE_USE_SHARED |
		XHTTP_CACHE_USE_CANDIDATE_MATCH |
		XHTTP_CACHE_USE_REPRESENTATION |
		XHTTP_CACHE_USE_CLOCK |
		XHTTP_CACHE_USE_STATUS_UNDERSTOOD |
		XHTTP_CACHE_USE_AUTHORIZATION;
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_FORWARD) &&
		((Plan.Actions &
		  XHTTP_CACHE_USE_EVICT) != 0),
		"unauthorized shared entry was reused"
	);
	testHttpCachePolicyControl(
		XRT_STR_LITERAL("public"), &Response
	);
	testRequire(
		xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		) == XHTTP_CACHE_USE_STORED,
		"public shared entry did not satisfy authorized request"
	);
	memset(&Before, 0x5A, sizeof(Before));
	Plan = Before;
	Input.Flags = UINT32_MAX;
	testRequire(
		(xrtHttpCacheUsePlan(
			&Request, &Response,
			&Age, &Freshness,
			&Input, &Plan
		 ) == XHTTP_CACHE_USE_ERROR) &&
		(memcmp(&Plan, &Before, sizeof(Plan)) == 0),
		"invalid use input changed output"
	);
}



/* 执行 HTTP 缓存存储和复用策略矩阵。 */
int main(void)
{
	testHttpCachePolicyDefaults();
	testHttpCachePolicyStorageControls();
	testHttpCachePolicyStorageEdges();
	testHttpCachePolicyFreshUse();
	testHttpCachePolicyStaleUse();
	testHttpCachePolicyUseEdges();
	printf("[PASS] http_cache_policy\n");
	return 0;
}
