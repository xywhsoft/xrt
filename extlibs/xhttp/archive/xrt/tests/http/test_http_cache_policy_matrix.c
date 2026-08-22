#include "../test.h"

#include <xrt/http_cache_policy.h>



#define TEST_HTTP_CACHE_POLICY_REASONS \
	((uint32)XHTTP_CACHE_REASON_METHOD_UNKNOWN | \
	 (uint32)XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE | \
	 (uint32)XHTTP_CACHE_REASON_STATUS_NOT_FINAL | \
	 (uint32)XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_REASON_HEADERS_INCOMPLETE | \
	 (uint32)XHTTP_CACHE_REASON_REQUEST_NO_STORE | \
	 (uint32)XHTTP_CACHE_REASON_RESPONSE_NO_STORE | \
	 (uint32)XHTTP_CACHE_REASON_SHARED_PRIVATE | \
	 (uint32)XHTTP_CACHE_REASON_AUTHORIZATION | \
	 (uint32)XHTTP_CACHE_REASON_NO_PERMISSION | \
	 (uint32)XHTTP_CACHE_REASON_POST_REQUIREMENTS | \
	 (uint32)XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED | \
	 (uint32)XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE | \
	 (uint32)XHTTP_CACHE_REASON_CANDIDATE_MISS | \
	 (uint32)XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE | \
	 (uint32)XHTTP_CACHE_REASON_NO_CLOCK | \
	 (uint32)XHTTP_CACHE_REASON_REQUEST_REVALIDATE | \
	 (uint32)XHTTP_CACHE_REASON_RESPONSE_REVALIDATE | \
	 (uint32)XHTTP_CACHE_REASON_STALE | \
	 (uint32)XHTTP_CACHE_REASON_ONLY_IF_CACHED | \
	 (uint32)XHTTP_CACHE_REASON_DISCONNECTED | \
	 (uint32)XHTTP_CACHE_REASON_EXTENSION)

#define TEST_HTTP_CACHE_STORE_ACTIONS \
	((uint32)XHTTP_CACHE_STORE_REMOVE_CONNECTION | \
	 (uint32)XHTTP_CACHE_STORE_REMOVE_PROXY | \
	 (uint32)XHTTP_CACHE_STORE_SEPARATE_TRAILERS | \
	 (uint32)XHTTP_CACHE_STORE_REMOVE_NO_CACHE | \
	 (uint32)XHTTP_CACHE_STORE_REMOVE_PRIVATE | \
	 (uint32)XHTTP_CACHE_STORE_MARK_INCOMPLETE | \
	 (uint32)XHTTP_CACHE_STORE_AS_200 | \
	 (uint32)XHTTP_CACHE_STORE_IGNORE_NO_STORE)

#define TEST_HTTP_CACHE_USE_ACTIONS \
	((uint32)XHTTP_CACHE_USE_SET_AGE | \
	 (uint32)XHTTP_CACHE_USE_REMOVE_NO_CACHE | \
	 (uint32)XHTTP_CACHE_USE_STALE | \
	 (uint32)XHTTP_CACHE_USE_EVICT)



/* 穷举全部公开存储输入位并验证结果结构与失败原子性。 */
static void testHttpCachePolicyStoreMatrix(
	const xhttpcachecontrol* pEmpty,
	const xhttpcachetime* pTime
)
{
	xhttpcachestoreinput Input;
	xhttpcachestoreplan Plan;
	xhttpcachestoreplan Before;
	uint32 iFlags;

	Input.Method = XRT_STR_LITERAL("GET");
	Input.Status = 200;
	memset(&Before, 0xA5, sizeof(Before));
	for ( iFlags = 0; iFlags <= UINT32_C(0x1FFF);
		iFlags++ ) {
		xhttpcachestoredecision Decision;
		bool bInvalid =
			((iFlags &
			  XHTTP_CACHE_STORE_METHOD_CACHEABLE) != 0) &&
			((iFlags &
			  XHTTP_CACHE_STORE_METHOD_UNDERSTOOD) == 0);

		Input.Flags = iFlags;
		Plan = Before;
		Decision = xrtHttpCacheStorePlan(
			pEmpty, pEmpty, pTime,
			&Input, &Plan
		);
		if ( bInvalid ) {
			testRequire(
				(Decision ==
				 XHTTP_CACHE_STORE_ERROR) &&
				(memcmp(
					&Plan, &Before,
					sizeof(Plan)
				 ) == 0),
				"invalid store flag combination changed output"
			);
			xrtClearError();
		} else {
			testRequire(
				((Decision ==
				  XHTTP_CACHE_STORE_SKIP) ||
				 (Decision ==
				  XHTTP_CACHE_STORE_KEEP)) &&
				(Plan.Decision == Decision) &&
				((Plan.Actions &
				  ~TEST_HTTP_CACHE_STORE_ACTIONS) == 0) &&
				((Plan.Reasons &
				  ~TEST_HTTP_CACHE_POLICY_REASONS) == 0),
				"valid store flag combination escaped contract"
			);
		}
	}
}



/* 穷举全部公开复用输入位并验证决定、动作和原因位。 */
static void testHttpCachePolicyUseMatrix(
	const xhttpcachecontrol* pEmpty,
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness
)
{
	xhttpcacheuseinput Input;
	xhttpcacheuseplan Plan;
	uint32 iFlags;

	Input.Status = 200;
	for ( iFlags = 0; iFlags <= UINT32_C(0x03FF);
		iFlags++ ) {
		xhttpcacheusedecision Decision;

		Input.Flags = iFlags;
		Decision = xrtHttpCacheUsePlan(
			pEmpty, pEmpty,
			pAge, pFreshness,
			&Input, &Plan
		);
		testRequire(
			(Decision >=
			 XHTTP_CACHE_USE_FORWARD) &&
			(Decision <=
			 XHTTP_CACHE_USE_GATEWAY_TIMEOUT) &&
			(Plan.Decision == Decision) &&
			((Plan.Actions &
			  ~TEST_HTTP_CACHE_USE_ACTIONS) == 0) &&
			((Plan.Reasons &
			  ~TEST_HTTP_CACHE_POLICY_REASONS) == 0),
			"valid use flag combination escaped contract"
		);
	}
}



/* 验证公开标志掩码、组合约束和输出边界。 */
int main(void)
{
	xhttpcachecontrol Empty;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcachestoreinput StoreInput;
	xhttpcachestoreplan StorePlan;
	xhttpcachestoreplan StoreBefore;
	xhttpcacheuseinput UseInput;
	xhttpcacheuseplan UsePlan;
	xhttpcacheuseplan UseBefore;

	xrtHttpCacheControlInit(&Empty);
	xrtHttpCacheTimeInit(&Time);
	memset(&Age, 0, sizeof(Age));
	Freshness.Lifetime = UINT64_C(1000000);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_HEURISTIC;
	testHttpCachePolicyStoreMatrix(&Empty, &Time);
	testHttpCachePolicyUseMatrix(
		&Empty, &Age, &Freshness
	);

	StoreInput.Method = XRT_STR_LITERAL("GET");
	StoreInput.Status = 200;
	StoreInput.Flags = UINT32_C(0x2000);
	memset(&StoreBefore, 0x5A, sizeof(StoreBefore));
	StorePlan = StoreBefore;
	testRequire(
		(xrtHttpCacheStorePlan(
			&Empty, &Empty, &Time,
			&StoreInput, &StorePlan
		 ) == XHTTP_CACHE_STORE_ERROR) &&
		(memcmp(
			&StorePlan, &StoreBefore,
			sizeof(StorePlan)
		 ) == 0),
		"unknown store flag changed output"
	);
	xrtClearError();
	UseInput.Status = 200;
	UseInput.Flags = UINT32_C(0x0400);
	memset(&UseBefore, 0x5A, sizeof(UseBefore));
	UsePlan = UseBefore;
	testRequire(
		(xrtHttpCacheUsePlan(
			&Empty, &Empty,
			&Age, &Freshness,
			&UseInput, &UsePlan
		 ) == XHTTP_CACHE_USE_ERROR) &&
		(memcmp(
			&UsePlan, &UseBefore,
			sizeof(UsePlan)
		 ) == 0),
		"unknown use flag changed output"
	);
	printf("[PASS] http_cache_policy_matrix\n");
	return 0;
}
