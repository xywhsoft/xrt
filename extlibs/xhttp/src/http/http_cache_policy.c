#include "../internal/xrt_http_cache.h"

#include <xrt/http_cache_policy.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

#define XRT_HTTP_CACHE_STORE_FLAGS \
	((uint32)XHTTP_CACHE_STORE_SHARED | \
	 (uint32)XHTTP_CACHE_STORE_AUTHORIZATION | \
	 (uint32)XHTTP_CACHE_STORE_METHOD_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_STORE_METHOD_CACHEABLE | \
	 (uint32)XHTTP_CACHE_STORE_STATUS_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_STORE_STATUS_HEURISTIC | \
	 (uint32)XHTTP_CACHE_STORE_HEADERS_COMPLETE | \
	 (uint32)XHTTP_CACHE_STORE_RESPONSE_COMPLETE | \
	 (uint32)XHTTP_CACHE_STORE_RANGE_SUPPORTED | \
	 (uint32)XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION_FRESHNESS | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION_OVERRIDE)

#define XRT_HTTP_CACHE_USE_FLAGS \
	((uint32)XHTTP_CACHE_USE_SHARED | \
	 (uint32)XHTTP_CACHE_USE_CANDIDATE_MATCH | \
	 (uint32)XHTTP_CACHE_USE_REPRESENTATION | \
	 (uint32)XHTTP_CACHE_USE_CLOCK | \
	 (uint32)XHTTP_CACHE_USE_VALIDATED | \
	 (uint32)XHTTP_CACHE_USE_DISCONNECTED | \
	 (uint32)XHTTP_CACHE_USE_STALE_ALLOWED | \
	 (uint32)XHTTP_CACHE_USE_EXTENSION | \
	 (uint32)XHTTP_CACHE_USE_STATUS_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_USE_AUTHORIZATION)

#define XRT_HTTP_CACHE_STORE_ACTIONS \
	((uint32)XHTTP_CACHE_STORE_REMOVE_CONNECTION | \
	 (uint32)XHTTP_CACHE_STORE_REMOVE_PROXY | \
	 (uint32)XHTTP_CACHE_STORE_SEPARATE_TRAILERS)



/* 判断方法是否等于一个标准大写方法名。 */
static bool __xrtHttpCachePolicyMethod(
	xstrview Method,
	xstrview Expected
)
{
	return xrtHttpMethodEqual(Method, Expected);
}



/* 判断一个已知指令存在且参数有效，并按需拒绝重复值。 */
static bool __xrtHttpCachePolicyDirective(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive,
	bool Unique
)
{
	uint32 iDirective = (uint32)Directive;

	return
		((pControl->Flags & iDirective) != 0) &&
		((pControl->InvalidDirectives & iDirective) == 0) &&
		(!Unique ||
		 ((pControl->DuplicateDirectives &
		   iDirective) == 0));
}



/* 判断单值限定指令是否可靠地携带字段名列表。 */
static bool __xrtHttpCachePolicyFields(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive,
	uint32 iFieldFlag
)
{
	return
		__xrtHttpCachePolicyDirective(
			pControl, Directive, true
		) &&
		((pControl->Flags & iFieldFlag) != 0);
}



/* 判断一个数值请求指令是否存在但不能可靠解释。 */
static bool __xrtHttpCachePolicyUnreliable(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive
)
{
	uint32 iDirective = (uint32)Directive;

	return
		((pControl->Flags & iDirective) != 0) &&
		!__xrtHttpCachePolicyDirective(
			pControl, Directive, true
		);
}



/* 判断状态码是否由当前公共状态表完整识别。 */
static bool __xrtHttpCachePolicyStatusKnown(uint16 iStatus)
{
	return xrtHttpStatusText(iStatus).Size != 0;
}



/* 验证存储输入结构及方法借用边界。 */
static bool __xrtHttpCacheStoreInputValid(
	const xhttpcachestoreinput* pInput
)
{
	if ( (pInput == NULL) ||
		!__xrtHttpViewValid(pInput->Method) ||
		!xrtHttpTokenValid(pInput->Method) ||
		(pInput->Status < 100) ||
		(pInput->Status > 999) ||
		((pInput->Flags &
		  ~XRT_HTTP_CACHE_STORE_FLAGS) != 0) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput),
			pInput->Method.Data,
			pInput->Method.Size
		) ) {
		return false;
	}
	return
		((pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_CACHEABLE) == 0) ||
		((pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_UNDERSTOOD) != 0);
}



/* 验证复用输入结构。 */
static bool __xrtHttpCacheUseInputValid(
	const xhttpcacheuseinput* pInput
)
{
	return
		(pInput != NULL) &&
		(pInput->Status >= 100) &&
		(pInput->Status <= 999) &&
		((pInput->Flags &
		  ~XRT_HTTP_CACHE_USE_FLAGS) == 0);
}



/* 判断响应是否具有当前缓存可以依赖的显式新鲜寿命。 */
static bool __xrtHttpCachePolicyExplicit(
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	bool Shared,
	bool Extension
)
{
	return
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_MAX_AGE, true
		) ||
		(Shared &&
		 __xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_S_MAXAGE, true
		 )) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES) != 0) &&
		 ((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES_DUPLICATE) == 0)) ||
		Extension;
}



/* 判断响应具有 RFC 9111 存储许可或调用方扩展许可。 */
static bool __xrtHttpCachePolicyStorable(
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput
)
{
	bool bShared =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_SHARED) != 0;

	return
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PUBLIC, false
		) ||
		(!bShared &&
		 __xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PRIVATE, true
		 )) ||
		__xrtHttpCachePolicyExplicit(
			pResponse, pTime, bShared,
			(pInput->Flags &
			 XHTTP_CACHE_STORE_EXTENSION) != 0
		) ||
		((pInput->Flags &
		  XHTTP_CACHE_STORE_STATUS_HEURISTIC) != 0);
}



/* 判断共享缓存可以保存带 Authorization 请求的响应。 */
static bool __xrtHttpCachePolicyAuthorized(
	const xhttpcachecontrol* pResponse
)
{
	return
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_REVALIDATE,
			false
		) ||
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PUBLIC, false
		) ||
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_S_MAXAGE, true
		);
}



/* 按 only-if-cached 和线路状态结束一个需要源站的决定。 */
static xhttpcacheusedecision __xrtHttpCachePolicyOrigin(
	xhttpcacheuseplan* pPlan,
	uint32 iReason,
	bool Validate,
	bool OnlyIfCached,
	bool Disconnected
)
{
	pPlan->Reasons |= iReason;
	if ( OnlyIfCached || Disconnected ) {
		if ( OnlyIfCached ) {
			pPlan->Reasons |=
				XHTTP_CACHE_REASON_ONLY_IF_CACHED;
		}
		if ( Disconnected ) {
			pPlan->Reasons |=
				XHTTP_CACHE_REASON_DISCONNECTED;
		}
		pPlan->Decision =
			XHTTP_CACHE_USE_GATEWAY_TIMEOUT;
	} else {
		pPlan->Decision = Validate ?
			XHTTP_CACHE_USE_VALIDATE :
			XHTTP_CACHE_USE_FORWARD;
	}
	return pPlan->Decision;
}



/* 发布一个可直接使用的缓存响应计划。 */
static xhttpcacheusedecision __xrtHttpCachePolicyUse(
	xhttpcacheuseplan* pPlan,
	uint32 iActions,
	uint32 iReasons,
	uint64 iStaleBy
)
{
	pPlan->StaleBy = iStaleBy;
	pPlan->Decision = XHTTP_CACHE_USE_STORED;
	pPlan->Actions =
		XHTTP_CACHE_USE_SET_AGE | iActions;
	pPlan->Reasons = iReasons;
	return pPlan->Decision;
}



/* 判断标准方法是否具有当前模块实现的缓存语义。 */
XRT_API bool xrtHttpCacheMethodDefault(xstrview Method)
{
	if ( !__xrtHttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ) {
		return false;
	}
	return
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("GET")
		) ||
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("HEAD")
		) ||
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("POST")
		);
}



/* 判断标准状态是否允许启发式缓存。 */
XRT_API bool xrtHttpCacheStatusHeuristic(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_OK:
		case XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
		case XHTTP_STATUS_NO_CONTENT:
		case XHTTP_STATUS_PARTIAL_CONTENT:
		case XHTTP_STATUS_MULTIPLE_CHOICES:
		case XHTTP_STATUS_MOVED_PERMANENTLY:
		case XHTTP_STATUS_PERMANENT_REDIRECT:
		case XHTTP_STATUS_NOT_FOUND:
		case XHTTP_STATUS_METHOD_NOT_ALLOWED:
		case XHTTP_STATUS_GONE:
		case XHTTP_STATUS_URI_TOO_LONG:
		case XHTTP_STATUS_NOT_IMPLEMENTED:
			return true;

		default:
			return false;
	}
}



/* 建立标准方法和状态的默认存储能力。 */
XRT_API bool xrtHttpCacheStoreInputInit(
	xhttpcachestoreinput* pInput,
	xstrview Method,
	uint16 iStatus,
	bool Shared
)
{
	xhttpcachestoreinput Input;

	if ( (pInput == NULL) ||
		!__xrtHttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		(iStatus < 100) ||
		(iStatus > 999) ||
		__xrtRangesOverlap(
			pInput, sizeof(*pInput),
			Method.Data, Method.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Method = Method;
	Input.Status = iStatus;
	Input.Flags =
		XHTTP_CACHE_STORE_HEADERS_COMPLETE |
		XHTTP_CACHE_STORE_RESPONSE_COMPLETE;
	if ( Shared ) {
		Input.Flags |= XHTTP_CACHE_STORE_SHARED;
	}
	if ( xrtHttpCacheMethodDefault(Method) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_METHOD_UNDERSTOOD |
			XHTTP_CACHE_STORE_METHOD_CACHEABLE;
	}
	if ( __xrtHttpCachePolicyStatusKnown(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_STATUS_UNDERSTOOD;
	}
	if ( xrtHttpCacheStatusHeuristic(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_STATUS_HEURISTIC;
	}
	*pInput = Input;
	return true;
}



/* 按协议事实建立存储计划。 */
XRT_API xhttpcachestoredecision xrtHttpCacheStorePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput,
	xhttpcachestoreplan* pPlan
)
{
	xhttpcachestoreplan Plan;
	uint32 iActions = XRT_HTTP_CACHE_STORE_ACTIONS;
	bool bShared;
	bool bGet;
	bool bPost;
	bool bStatusUnderstood;
	bool bMustUnderstand;
	bool bOverride;

	if ( !xrtHttpCacheControlValid(pRequest) ||
		!xrtHttpCacheControlValid(pResponse) ||
		!xrtHttpCacheTimeValid(pTime) ||
		!__xrtHttpCacheStoreInputValid(pInput) ||
		(pPlan == NULL) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pRequest, sizeof(*pRequest)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pResponse, sizeof(*pResponse)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pTime, sizeof(*pTime)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pInput, sizeof(*pInput)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_STORE_ERROR;
	}
	memset(&Plan, 0, sizeof(Plan));
	bShared =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_SHARED) != 0;
	bGet = __xrtHttpCachePolicyMethod(
		pInput->Method, XRT_STR_LITERAL("GET")
	);
	bPost = __xrtHttpCachePolicyMethod(
		pInput->Method, XRT_STR_LITERAL("POST")
	);
	bStatusUnderstood =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_STATUS_UNDERSTOOD) != 0;
	bMustUnderstand =
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_UNDERSTAND,
			false
		);
	bOverride =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_EXTENSION_OVERRIDE) != 0;
	if ( bOverride ) {
		Plan.Decision = XHTTP_CACHE_STORE_KEEP;
		Plan.Actions = iActions;
		Plan.Reasons = XHTTP_CACHE_REASON_EXTENSION;
		*pPlan = Plan;
		return Plan.Decision;
	}

	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_UNDERSTOOD) == 0 ) {
		Plan.Reasons |= XHTTP_CACHE_REASON_METHOD_UNKNOWN;
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_CACHEABLE) == 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE;
	}
	if ( pInput->Status < 200 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_STATUS_NOT_FINAL;
	}
	if ( (((pInput->Status == XHTTP_STATUS_PARTIAL_CONTENT) ||
		   (pInput->Status == XHTTP_STATUS_NOT_MODIFIED) ||
		   bMustUnderstand)) &&
		!bStatusUnderstood ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD;
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_HEADERS_COMPLETE) == 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_HEADERS_INCOMPLETE;
	}
	if ( (pRequest->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_REQUEST_NO_STORE;
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 ) {
		if ( bMustUnderstand && bStatusUnderstood ) {
			iActions |=
				XHTTP_CACHE_STORE_IGNORE_NO_STORE;
		} else {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_RESPONSE_NO_STORE;
		}
	}
	if ( bShared &&
		((pResponse->Flags &
		  XHTTP_CACHE_PRIVATE) != 0) ) {
		if ( __xrtHttpCachePolicyFields(
			pResponse,
			XHTTP_CACHE_PRIVATE,
			XHTTP_CACHE_PRIVATE_FIELDS
		) ) {
			iActions |=
				XHTTP_CACHE_STORE_REMOVE_PRIVATE;
		} else {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_SHARED_PRIVATE;
		}
	}
	if ( __xrtHttpCachePolicyFields(
		pResponse,
		XHTTP_CACHE_NO_CACHE,
		XHTTP_CACHE_NO_CACHE_FIELDS
	) ) {
		iActions |=
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE;
	}
	if ( bShared &&
		((pInput->Flags &
		  XHTTP_CACHE_STORE_AUTHORIZATION) != 0) &&
		!__xrtHttpCachePolicyAuthorized(pResponse) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_AUTHORIZATION;
	}
	if ( !__xrtHttpCachePolicyStorable(
		pResponse, pTime, pInput
	) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_NO_PERMISSION;
	}
	if ( bPost &&
		(!__xrtHttpCachePolicyExplicit(
			pResponse, pTime, bShared,
			(pInput->Flags &
			 XHTTP_CACHE_STORE_EXTENSION_FRESHNESS) != 0
		 ) ||
		 ((pInput->Flags &
		   XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH) == 0)) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_POST_REQUIREMENTS;
	}
	if ( pInput->Status ==
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		if ( !bGet ||
			((pInput->Flags &
			  XHTTP_CACHE_STORE_RANGE_SUPPORTED) == 0) ) {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED;
		} else {
			iActions |=
				XHTTP_CACHE_STORE_MARK_INCOMPLETE |
				XHTTP_CACHE_STORE_AS_200;
		}
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_RESPONSE_COMPLETE) == 0 ) {
		if ( !bGet ||
			((pInput->Status != XHTTP_STATUS_OK) &&
			 (pInput->Status !=
			  XHTTP_STATUS_PARTIAL_CONTENT)) ||
			((pInput->Flags &
			  XHTTP_CACHE_STORE_RANGE_SUPPORTED) == 0) ) {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE;
		} else {
			iActions |=
				XHTTP_CACHE_STORE_MARK_INCOMPLETE;
		}
	}
	if ( Plan.Reasons == XHTTP_CACHE_REASON_NONE ) {
		Plan.Decision = XHTTP_CACHE_STORE_KEEP;
		Plan.Actions = iActions;
	} else {
		Plan.Decision = XHTTP_CACHE_STORE_SKIP;
	}
	*pPlan = Plan;
	return Plan.Decision;
}



/* 建立标准状态的默认复用环境。 */
XRT_API bool xrtHttpCacheUseInputInit(
	xhttpcacheuseinput* pInput,
	uint16 iStatus,
	bool Shared
)
{
	xhttpcacheuseinput Input;

	if ( (pInput == NULL) ||
		(iStatus < 100) ||
		(iStatus > 999) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Status = iStatus;
	Input.Flags =
		XHTTP_CACHE_USE_CANDIDATE_MATCH |
		XHTTP_CACHE_USE_REPRESENTATION |
		XHTTP_CACHE_USE_CLOCK;
	if ( Shared ) {
		Input.Flags |= XHTTP_CACHE_USE_SHARED;
	}
	if ( __xrtHttpCachePolicyStatusKnown(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_USE_STATUS_UNDERSTOOD;
	}
	*pInput = Input;
	return true;
}



/* 按缓存约束建立复用、验证、转发或 504 计划。 */
XRT_API xhttpcacheusedecision xrtHttpCacheUsePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness,
	const xhttpcacheuseinput* pInput,
	xhttpcacheuseplan* pPlan
)
{
	uint32 iUseActions = 0;
	bool bShared;
	bool bOnlyIfCached;
	bool bDisconnected;
	bool bMustUnderstand;
	bool bStatusUnderstood;
	bool bFresh;
	bool bStaleAllowed;
	uint64 iStaleBy;

	if ( !xrtHttpCacheControlValid(pRequest) ||
		!xrtHttpCacheControlValid(pResponse) ||
		!xrtHttpCacheAgeValid(pAge) ||
		!xrtHttpCacheFreshnessValid(pFreshness) ||
		!__xrtHttpCacheUseInputValid(pInput) ||
		(pPlan == NULL) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pRequest, sizeof(*pRequest)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pResponse, sizeof(*pResponse)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pAge, sizeof(*pAge)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pFreshness, sizeof(*pFreshness)
		) ||
		__xrtRangesOverlap(
			pPlan, sizeof(*pPlan),
			pInput, sizeof(*pInput)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_USE_ERROR;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	bShared =
		(pInput->Flags &
		 XHTTP_CACHE_USE_SHARED) != 0;
	bOnlyIfCached =
		(pRequest->Flags &
		 XHTTP_CACHE_ONLY_IF_CACHED) != 0;
	bDisconnected =
		(pInput->Flags &
		 XHTTP_CACHE_USE_DISCONNECTED) != 0;
	bMustUnderstand =
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_UNDERSTAND,
			false
		);
	bStatusUnderstood =
		(pInput->Flags &
		 XHTTP_CACHE_USE_STATUS_UNDERSTOOD) != 0;

	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_CANDIDATE_MATCH) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_CANDIDATE_MISS,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_REPRESENTATION) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_EXTENSION) != 0 ) {
		return __xrtHttpCachePolicyUse(
			pPlan, 0,
			XHTTP_CACHE_REASON_EXTENSION, 0
		);
	}
	if ( bShared &&
		((pInput->Flags &
		  XHTTP_CACHE_USE_AUTHORIZATION) != 0) &&
		!__xrtHttpCachePolicyAuthorized(pResponse) ) {
		pPlan->Actions = XHTTP_CACHE_USE_EVICT;
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_AUTHORIZATION,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 &&
		!(bMustUnderstand && bStatusUnderstood) ) {
		pPlan->Actions = XHTTP_CACHE_USE_EVICT;
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_RESPONSE_NO_STORE,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_VALIDATED) != 0 ) {
		return __xrtHttpCachePolicyUse(
			pPlan, 0, XHTTP_CACHE_REASON_NONE, 0
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_CLOCK) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_NO_CLOCK,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pRequest->Flags &
		  XHTTP_CACHE_NO_CACHE) != 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REQUEST_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_CACHE) != 0 ) {
		if ( __xrtHttpCachePolicyFields(
			pResponse,
			XHTTP_CACHE_NO_CACHE,
			XHTTP_CACHE_NO_CACHE_FIELDS
		) ) {
			iUseActions |=
				XHTTP_CACHE_USE_REMOVE_NO_CACHE;
		} else {
			return __xrtHttpCachePolicyOrigin(
				pPlan,
				XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
				true,
				bOnlyIfCached,
				bDisconnected
			);
		}
	}
	if ( __xrtHttpCachePolicyUnreliable(
			pRequest, XHTTP_CACHE_MAX_AGE
		) ||
		__xrtHttpCachePolicyUnreliable(
			pRequest, XHTTP_CACHE_MIN_FRESH
		) ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REQUEST_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	bFresh =
		(pFreshness->Source !=
		 XHTTP_CACHE_FRESHNESS_NONE) &&
		(pFreshness->Lifetime > pAge->CurrentAge);
	if ( bFresh &&
		__xrtHttpCachePolicyDirective(
			pRequest, XHTTP_CACHE_MAX_AGE, true
		) ) {
		bFresh =
			pAge->CurrentAge <=
			__xrtHttpCacheTimeSeconds(
				pRequest->MaxAge
			);
	}
	if ( bFresh &&
		__xrtHttpCachePolicyDirective(
			pRequest, XHTTP_CACHE_MIN_FRESH, true
		) ) {
		bFresh =
			pFreshness->Lifetime >=
			__xrtHttpCacheTimeAdd(
				pAge->CurrentAge,
				__xrtHttpCacheTimeSeconds(
					pRequest->MinFresh
				)
			);
	}
	if ( bFresh ) {
		return __xrtHttpCachePolicyUse(
			pPlan,
			iUseActions,
			XHTTP_CACHE_REASON_NONE,
			0
		);
	}
	if ( pFreshness->Source ==
		XHTTP_CACHE_FRESHNESS_NONE ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	iStaleBy =
		pAge->CurrentAge > pFreshness->Lifetime ?
		pAge->CurrentAge - pFreshness->Lifetime :
		0;
	pPlan->Reasons |= XHTTP_CACHE_REASON_STALE;
	if ( ((pResponse->Flags &
		   XHTTP_CACHE_MUST_REVALIDATE) != 0) ||
		(bShared &&
		 ((pResponse->Flags & (
			XHTTP_CACHE_PROXY_REVALIDATE |
			XHTTP_CACHE_S_MAXAGE
		  )) != 0)) ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_STALE |
			XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	bStaleAllowed =
		bDisconnected ||
		((pInput->Flags &
		  XHTTP_CACHE_USE_STALE_ALLOWED) != 0);
	if ( __xrtHttpCachePolicyDirective(
		pRequest, XHTTP_CACHE_MAX_STALE, true
	) ) {
		if ( (pRequest->Flags &
			  XHTTP_CACHE_MAX_STALE_ANY) != 0 ) {
			bStaleAllowed = true;
		} else if ( iStaleBy <=
			__xrtHttpCacheTimeSeconds(
				pRequest->MaxStale
			) ) {
			bStaleAllowed = true;
		}
	}
	if ( bStaleAllowed ) {
		return __xrtHttpCachePolicyUse(
			pPlan,
			iUseActions |
				XHTTP_CACHE_USE_STALE,
			XHTTP_CACHE_REASON_STALE,
			iStaleBy
		);
	}
	return __xrtHttpCachePolicyOrigin(
		pPlan,
		XHTTP_CACHE_REASON_STALE,
		true,
		bOnlyIfCached,
		bDisconnected
	);
}

#endif
