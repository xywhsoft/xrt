#ifndef XRT_HTTP_CACHE_VALIDATE_H
#define XRT_HTTP_CACHE_VALIDATE_H

#include <xrt/http_cache_policy.h>
#include <xrt/http_forward.h>
#include <xrt/http_origin.h>
#include <xrt/http_semantics.h>
#include <xrt/url.h>



#if defined(XRT_FEATURE_HTTP_CACHE_VALIDATE) && \
	(!defined(XRT_FEATURE_HTTP_CACHE_POLICY) || \
	 !defined(XRT_FEATURE_HTTP_FORWARD) || \
	 !defined(XRT_FEATURE_HTTP_ORIGIN) || \
	 !defined(XRT_FEATURE_HTTP_PRECONDITION) || \
	 !defined(XRT_FEATURE_URL))
	#error "XRT HTTP cache validation requires cache policy, forwarding, Origin, precondition and URL support"
#endif



#if defined(XRT_FEATURE_HTTP_CACHE_VALIDATE)

/* 缓存条目标志只描述正文完整性和当前 Range 是否可由该条目完整满足。 */
typedef enum xhttpcacheentryflag {
	XHTTP_CACHE_ENTRY_NONE = 0,
	XHTTP_CACHE_ENTRY_PARTIAL = UINT32_C(0x00000001),
	XHTTP_CACHE_ENTRY_RANGE_COVERED = UINT32_C(0x00000002)
} xhttpcacheentryflag;



/*
	缓存条目借用已保存的响应 Header。
	ResponseTime 是收到或成功验证响应时的墙钟，用于缺少 Date 时判断最新条目。
*/
typedef struct xhttpcacheentry {
	const xhttpfield* Fields;
	size_t FieldCount;
	xtime ResponseTime;
	uint32 Flags;
} xhttpcacheentry;



/* 验证请求动作直接对应需要设置或替换的条件字段。 */
typedef enum xhttpcachevalidateaction {
	XHTTP_CACHE_VALIDATE_ACTION_NONE = 0,
	XHTTP_CACHE_VALIDATE_IF_NONE_MATCH = UINT32_C(0x00000001),
	XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE = UINT32_C(0x00000002)
} xhttpcachevalidateaction;



/* 验证请求计划区分参数错误、没有可用验证器和可生成条件请求。 */
typedef enum xhttpcachevalidatedecision {
	XHTTP_CACHE_VALIDATE_ERROR = -1,
	XHTTP_CACHE_VALIDATE_NONE = 0,
	XHTTP_CACHE_VALIDATE_CONDITIONAL = 1
} xhttpcachevalidatedecision;



/*
	ETagSize 是不含零结尾的 If-None-Match 值长度。
	LastModified 仅在对应动作存在时有效。
*/
typedef struct xhttpcachevalidateplan {
	xtime LastModified;
	size_t EligibleCount;
	size_t ETagCount;
	size_t ETagSize;
	uint32 Actions;
	xhttpcachevalidatedecision Decision;
} xhttpcachevalidateplan;



/* If-Range 计划优先使用强 ETag，其次使用已经证明为强验证器的日期。 */
typedef enum xhttpcacheifrangekind {
	XHTTP_CACHE_IF_RANGE_ERROR = -1,
	XHTTP_CACHE_IF_RANGE_NONE = 0,
	XHTTP_CACHE_IF_RANGE_ETAG = 1,
	XHTTP_CACHE_IF_RANGE_DATE = 2
} xhttpcacheifrangekind;



/* If-Range 计划中的 ETag 和日期都借用或复制自缓存条目。 */
typedef struct xhttpcacheifrange {
	xhttpetag ETag;
	xtime Date;
	xhttpcacheifrangekind Kind;
} xhttpcacheifrange;



/* 验证响应分类把 304、完整响应和可按失联处理的 5xx 分开。 */
typedef enum xhttpcachevalidateresult {
	XHTTP_CACHE_VALIDATE_RESULT_ERROR = -1,
	XHTTP_CACHE_VALIDATE_RESULT_FULL = 0,
	XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED = 1,
	XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE = 2
} xhttpcachevalidateresult;



/* 304 选择结果记录实际采用的强、弱或无验证器单条规则。 */
typedef enum xhttpcacheupdatematch {
	XHTTP_CACHE_UPDATE_MATCH_ERROR = -1,
	XHTTP_CACHE_UPDATE_MATCH_NONE = 0,
	XHTTP_CACHE_UPDATE_MATCH_STRONG = 1,
	XHTTP_CACHE_UPDATE_MATCH_WEAK = 2,
	XHTTP_CACHE_UPDATE_MATCH_SINGLE = 3
} xhttpcacheupdatematch;



/* HEAD freshening 只对 200 响应比较验证器和 Content-Length。 */
typedef enum xhttpcacheheaddecision {
	XHTTP_CACHE_HEAD_ERROR = -1,
	XHTTP_CACHE_HEAD_IGNORE = 0,
	XHTTP_CACHE_HEAD_UPDATE = 1,
	XHTTP_CACHE_HEAD_STALE = 2
} xhttpcacheheaddecision;



/*
	字段更新标志由保存实现提供。
	DEPENDENT 表示已保存表示依赖该元数据，PROCESSED 表示该字段已经被消费或移除。
*/
typedef enum xhttpcacheupdatefieldflag {
	XHTTP_CACHE_UPDATE_FIELD_NONE = 0,
	XHTTP_CACHE_UPDATE_FIELD_DEPENDENT = UINT32_C(0x00000001),
	XHTTP_CACHE_UPDATE_FIELD_PROCESSED = UINT32_C(0x00000002)
} xhttpcacheupdatefieldflag;



/* 更新字段计划区分非法输入、规范要求保留旧值和应使用新值替换。 */
typedef enum xhttpcachefieldupdate {
	XHTTP_CACHE_FIELD_UPDATE_ERROR = -1,
	XHTTP_CACHE_FIELD_UPDATE_SKIP = 0,
	XHTTP_CACHE_FIELD_UPDATE_REPLACE = 1
} xhttpcachefieldupdate;



/* 初次保存字段时区分协议错误、必须删除和可以保留。 */
typedef enum xhttpcachefieldstore {
	XHTTP_CACHE_FIELD_STORE_ERROR = -1,
	XHTTP_CACHE_FIELD_STORE_SKIP = 0,
	XHTTP_CACHE_FIELD_STORE_KEEP = 1
} xhttpcachefieldstore;



/* 失效候选来源区分请求目标和两个可选响应字段。 */
typedef enum xhttpcacheinvalidatekind {
	XHTTP_CACHE_INVALIDATE_TARGET = 1,
	XHTTP_CACHE_INVALIDATE_LOCATION,
	XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION
} xhttpcacheinvalidatekind;



/* 失效游标先返回目标 URI，再按线路顺序扫描两个位置字段。 */
typedef struct xhttpcacheinvalidatecursor {
	size_t Field;
	bool Target;
} xhttpcacheinvalidatecursor;



/*
	失效候选借用目标 URI或响应字段值。
	Field 对目标 URI 为 XRT_NPOS，对字段候选为原字段下标。
*/
typedef struct xhttpcacheinvalidateitem {
	xstrview Reference;
	size_t Field;
	xhttpcacheinvalidatekind Kind;
} xhttpcacheinvalidateitem;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断缓存条目视图、字段数组和正文范围标志是否自洽。 */
XRT_API bool xrtHttpCacheEntryValid(
	const xhttpcacheentry* pEntry
);



/*
	按 RFC 9111 为一组已经完成 URI、方法与 Vary 选择的条目生成验证计划。
	Range 为 true 时，部分条目只有声明 RANGE_COVERED 才能贡献 ETag，
	且不会生成 If-Modified-Since。
*/
XRT_API xhttpcachevalidatedecision xrtHttpCacheValidatePlan(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
);



/*
	写出计划中的去重 If-None-Match 值。
	空输出可查询精确长度；短缓冲和重叠输出保持目标内存不变。
*/
XRT_API bool xrtHttpCacheValidateETagsWrite(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	为单个缓存条目的 Range 请求选择 If-Range。
	弱 ETag 不可使用，并且存在任何 ETag 时不会退回 Last-Modified。
*/
XRT_API xhttpcacheifrangekind xrtHttpCacheIfRangePlan(
	const xhttpcacheentry* pEntry,
	xhttpcacheifrange* pPlan
);



/*
	缓存按 RFC 9111 §4.3.2 评估收到的条件请求。
	只处理 GET/HEAD 的 If-None-Match 与 If-Modified-Since；
	源站专用的 If-Match 和 If-Unmodified-Since 保持转发。
*/
XRT_API xhttpprecondition xrtHttpCachePreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestCount,
	const xhttpcacheentry* pEntry
);



/*
	分类条件请求的最终响应。
	Treat5xxAsFailure 为 true 时，5xx 交给重试或失联复用策略处理。
*/
XRT_API xhttpcachevalidateresult xrtHttpCacheValidateResult(
	uint16 iStatus,
	bool Treat5xxAsFailure
);



/*
	按 304 中的验证器选择应更新的候选下标。
	强验证器选择全部匹配项；弱验证器只选择 Date 最新项；
	无验证器仅允许唯一且同样无验证器的候选。
	空下标输出可查询数量，短缓冲不修改下标数组。
*/
XRT_API xhttpcacheupdatematch xrtHttpCache304Select(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iCapacity,
	size_t* pCount
);



/*
	判断 200 HEAD 是否能更新一个已保存 GET 响应。
	HEAD 中出现的 ETag、Last-Modified 和 Content-Length 必须逐项匹配。
*/
XRT_API xhttpcacheheaddecision xrtHttpCacheHeadPlan(
	uint16 iStatus,
	const xhttpcacheentry* pEntry,
	const xhttpfield* pFields,
	size_t iFieldCount
);



/*
	按 RFC 9111 §3.2 判断新响应中的一个字段是否可替换已保存字段。
	函数自动过滤 Content-Length、Connection 及其提名字段、代理专用字段、
	以及限定 no-cache/private 排除的字段。
*/
XRT_API xhttpcachefieldupdate xrtHttpCacheFieldUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iFlags
);



/*
	按 StorePlan 的 Actions 判断初次保存时是否保留一个响应字段。
	函数统一过滤固定逐跳字段、Connection 提名字段、代理专用字段，
	以及限定 no-cache/private 明确排除的字段。
*/
XRT_API xhttpcachefieldstore xrtHttpCacheFieldStore(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iActions
);



/* 初始化可重复使用的缓存失效候选游标。 */
XRT_API void xrtHttpCacheInvalidationCursorInit(
	xhttpcacheinvalidatecursor* pCursor
);



/*
	迭代 unsafe 方法在 2xx/3xx 后产生的失效候选。
	目标 URI 必定先返回；Location 与 Content-Location 只有语法有效且同源才返回。
*/
XRT_API xhttpnext xrtHttpCacheInvalidationNext(
	xstrview Method,
	uint16 iStatus,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcacheinvalidatecursor* pCursor,
	xhttpcacheinvalidateitem* pItem
);



/*
	把失效候选解析为不含 fragment 的绝对 URI。
	空输出可查询精确长度，结果不附加零结尾。
	相对引用解析遵循 URL 模块契约，当前可能使用临时内存。
*/
XRT_API bool xrtHttpCacheInvalidationWrite(
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
