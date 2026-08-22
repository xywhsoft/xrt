#ifndef XRT_HTTP_CACHE_POLICY_H
#define XRT_HTTP_CACHE_POLICY_H

#include <xrt/http_cache_time.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) && \
	!defined(XHTTP_FEATURE_HTTP_CACHE_TIME)
	#error "XRT HTTP cache policy requires HTTP cache time support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

/* 策略原因位用于解释存储、复用、验证和转发决定。 */
typedef enum xhttpcachepolicyreason {
	XHTTP_CACHE_REASON_NONE = 0,
	XHTTP_CACHE_REASON_METHOD_UNKNOWN = UINT32_C(0x00000001),
	XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE = UINT32_C(0x00000002),
	XHTTP_CACHE_REASON_STATUS_NOT_FINAL = UINT32_C(0x00000004),
	XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD = UINT32_C(0x00000008),
	XHTTP_CACHE_REASON_HEADERS_INCOMPLETE = UINT32_C(0x00000010),
	XHTTP_CACHE_REASON_REQUEST_NO_STORE = UINT32_C(0x00000020),
	XHTTP_CACHE_REASON_RESPONSE_NO_STORE = UINT32_C(0x00000040),
	XHTTP_CACHE_REASON_SHARED_PRIVATE = UINT32_C(0x00000080),
	XHTTP_CACHE_REASON_AUTHORIZATION = UINT32_C(0x00000100),
	XHTTP_CACHE_REASON_NO_PERMISSION = UINT32_C(0x00000200),
	XHTTP_CACHE_REASON_POST_REQUIREMENTS = UINT32_C(0x00000400),
	XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED = UINT32_C(0x00000800),
	XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE = UINT32_C(0x00001000),
	XHTTP_CACHE_REASON_CANDIDATE_MISS = UINT32_C(0x00002000),
	XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE = UINT32_C(0x00004000),
	XHTTP_CACHE_REASON_NO_CLOCK = UINT32_C(0x00008000),
	XHTTP_CACHE_REASON_REQUEST_REVALIDATE = UINT32_C(0x00010000),
	XHTTP_CACHE_REASON_RESPONSE_REVALIDATE = UINT32_C(0x00020000),
	XHTTP_CACHE_REASON_STALE = UINT32_C(0x00040000),
	XHTTP_CACHE_REASON_ONLY_IF_CACHED = UINT32_C(0x00080000),
	XHTTP_CACHE_REASON_DISCONNECTED = UINT32_C(0x00100000),
	XHTTP_CACHE_REASON_EXTENSION = UINT32_C(0x00200000)
} xhttpcachepolicyreason;



/* 存储输入把协议无法自行观察的缓存能力和请求事实显式化。 */
typedef enum xhttpcachestoreflag {
	XHTTP_CACHE_STORE_NONE = 0,
	XHTTP_CACHE_STORE_SHARED = UINT32_C(0x00000001),
	XHTTP_CACHE_STORE_AUTHORIZATION = UINT32_C(0x00000002),
	XHTTP_CACHE_STORE_METHOD_UNDERSTOOD = UINT32_C(0x00000004),
	XHTTP_CACHE_STORE_METHOD_CACHEABLE = UINT32_C(0x00000008),
	XHTTP_CACHE_STORE_STATUS_UNDERSTOOD = UINT32_C(0x00000010),
	XHTTP_CACHE_STORE_STATUS_HEURISTIC = UINT32_C(0x00000020),
	XHTTP_CACHE_STORE_HEADERS_COMPLETE = UINT32_C(0x00000040),
	XHTTP_CACHE_STORE_RESPONSE_COMPLETE = UINT32_C(0x00000080),
	XHTTP_CACHE_STORE_RANGE_SUPPORTED = UINT32_C(0x00000100),
	XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH = UINT32_C(0x00000200),
	XHTTP_CACHE_STORE_EXTENSION = UINT32_C(0x00000400),
	XHTTP_CACHE_STORE_EXTENSION_FRESHNESS = UINT32_C(0x00000800),
	XHTTP_CACHE_STORE_EXTENSION_OVERRIDE = UINT32_C(0x00001000)
} xhttpcachestoreflag;



/* 存储动作描述保存响应前必须由存储实现完成的变换。 */
typedef enum xhttpcachestoreaction {
	XHTTP_CACHE_STORE_ACTION_NONE = 0,
	XHTTP_CACHE_STORE_REMOVE_CONNECTION = UINT32_C(0x00000001),
	XHTTP_CACHE_STORE_REMOVE_PROXY = UINT32_C(0x00000002),
	XHTTP_CACHE_STORE_SEPARATE_TRAILERS = UINT32_C(0x00000004),
	XHTTP_CACHE_STORE_REMOVE_NO_CACHE = UINT32_C(0x00000008),
	XHTTP_CACHE_STORE_REMOVE_PRIVATE = UINT32_C(0x00000010),
	XHTTP_CACHE_STORE_MARK_INCOMPLETE = UINT32_C(0x00000020),
	XHTTP_CACHE_STORE_AS_200 = UINT32_C(0x00000040),
	XHTTP_CACHE_STORE_IGNORE_NO_STORE = UINT32_C(0x00000080)
} xhttpcachestoreaction;



/* 存储决定只区分参数错误、不得保存和可以按动作保存。 */
typedef enum xhttpcachestoredecision {
	XHTTP_CACHE_STORE_ERROR = -1,
	XHTTP_CACHE_STORE_SKIP = 0,
	XHTTP_CACHE_STORE_KEEP = 1
} xhttpcachestoredecision;



/* 方法和状态属于线路事实，其余位描述调用方已经实现的缓存能力。 */
typedef struct xhttpcachestoreinput {
	xstrview Method;
	uint16 Status;
	uint32 Flags;
} xhttpcachestoreinput;



/* 存储计划失败原子地返回决定、全部阻断原因和成功时的必要动作。 */
typedef struct xhttpcachestoreplan {
	xhttpcachestoredecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcachestoreplan;



/* 复用输入由键选择、表示范围、时钟、线路状态和扩展策略共同组成。 */
typedef enum xhttpcacheuseflag {
	XHTTP_CACHE_USE_NONE = 0,
	XHTTP_CACHE_USE_SHARED = UINT32_C(0x00000001),
	XHTTP_CACHE_USE_CANDIDATE_MATCH = UINT32_C(0x00000002),
	XHTTP_CACHE_USE_REPRESENTATION = UINT32_C(0x00000004),
	XHTTP_CACHE_USE_CLOCK = UINT32_C(0x00000008),
	XHTTP_CACHE_USE_VALIDATED = UINT32_C(0x00000010),
	XHTTP_CACHE_USE_DISCONNECTED = UINT32_C(0x00000020),
	XHTTP_CACHE_USE_STALE_ALLOWED = UINT32_C(0x00000040),
	XHTTP_CACHE_USE_EXTENSION = UINT32_C(0x00000080),
	XHTTP_CACHE_USE_STATUS_UNDERSTOOD = UINT32_C(0x00000100),
	XHTTP_CACHE_USE_AUTHORIZATION = UINT32_C(0x00000200)
} xhttpcacheuseflag;



/* 复用动作由缓存输出层执行，不与具体 Header 容器绑定。 */
typedef enum xhttpcacheuseaction {
	XHTTP_CACHE_USE_ACTION_NONE = 0,
	XHTTP_CACHE_USE_SET_AGE = UINT32_C(0x00000001),
	XHTTP_CACHE_USE_REMOVE_NO_CACHE = UINT32_C(0x00000002),
	XHTTP_CACHE_USE_STALE = UINT32_C(0x00000004),
	XHTTP_CACHE_USE_EVICT = UINT32_C(0x00000008)
} xhttpcacheuseaction;



/* 复用决定覆盖命中、源站验证、普通转发和 only-if-cached 的 504。 */
typedef enum xhttpcacheusedecision {
	XHTTP_CACHE_USE_ERROR = -1,
	XHTTP_CACHE_USE_FORWARD = 0,
	XHTTP_CACHE_USE_STORED = 1,
	XHTTP_CACHE_USE_VALIDATE = 2,
	XHTTP_CACHE_USE_GATEWAY_TIMEOUT = 3
} xhttpcacheusedecision;



/* 状态用于 must-understand；Flags 保存候选和运行环境事实。 */
typedef struct xhttpcacheuseinput {
	uint16 Status;
	uint32 Flags;
} xhttpcacheuseinput;



/* StaleBy 使用微秒；新鲜命中和非复用结果均为零。 */
typedef struct xhttpcacheuseplan {
	uint64 StaleBy;
	xhttpcacheusedecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcacheuseplan;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

/* 判断标准方法是否定义了本模块实现的默认缓存语义。 */
XRT_API bool xrtHttpCacheMethodDefault(xstrview Method);



/* 判断 RFC 9110 状态是否允许在没有显式寿命时使用启发式新鲜度。 */
XRT_API bool xrtHttpCacheStatusHeuristic(uint16 iStatus);



/*
	建立完整响应的默认存储输入。
	GET、HEAD、POST 和已注册状态自动获得相应能力位，扩展语义可由调用方修改。
*/
XRT_API bool xrtHttpCacheStoreInputInit(
	xhttpcachestoreinput* pInput,
	xstrview Method,
	uint16 iStatus,
	bool Shared
);



/*
	按 RFC 9111 建立零分配存储计划。
	请求与响应 Cache-Control 必须分别解析；Time 来自响应 Header。
*/
XRT_API xhttpcachestoredecision xrtHttpCacheStorePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput,
	xhttpcachestoreplan* pPlan
);



/*
	建立已匹配、表示可用、具有时钟的默认复用输入。
	键匹配必须已经覆盖目标 URI、方法规则和 Vary。
*/
XRT_API bool xrtHttpCacheUseInputInit(
	xhttpcacheuseinput* pInput,
	uint16 iStatus,
	bool Shared
);



/*
	按请求约束、响应约束、年龄和寿命建立零分配复用计划。
	启发式或扩展寿命由上层构造有效 xhttpcachefreshness 后传入。
*/
XRT_API xhttpcacheusedecision xrtHttpCacheUsePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness,
	const xhttpcacheuseinput* pInput,
	xhttpcacheuseplan* pPlan
);

#endif



XRT_EXTERN_C_END

#endif
