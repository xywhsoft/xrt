#ifndef XRT_HTTP_CACHE_H
#define XRT_HTTP_CACHE_H

#include <xrt/http.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE) && \
	!defined(XRT_FEATURE_HTTP_PARAM)
	#error "XRT HTTP cache control requires HTTP parameter support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE)

/* 已知缓存指令同时作为汇总状态中的出现位使用。 */
typedef enum xhttpcachedirective {
	XHTTP_CACHE_UNKNOWN = 0,
	XHTTP_CACHE_MAX_AGE = UINT32_C(0x00000001),
	XHTTP_CACHE_MAX_STALE = UINT32_C(0x00000002),
	XHTTP_CACHE_MIN_FRESH = UINT32_C(0x00000004),
	XHTTP_CACHE_NO_CACHE = UINT32_C(0x00000008),
	XHTTP_CACHE_NO_STORE = UINT32_C(0x00000010),
	XHTTP_CACHE_NO_TRANSFORM = UINT32_C(0x00000020),
	XHTTP_CACHE_ONLY_IF_CACHED = UINT32_C(0x00000040),
	XHTTP_CACHE_MUST_REVALIDATE = UINT32_C(0x00000080),
	XHTTP_CACHE_MUST_UNDERSTAND = UINT32_C(0x00000100),
	XHTTP_CACHE_PRIVATE = UINT32_C(0x00000200),
	XHTTP_CACHE_PROXY_REVALIDATE = UINT32_C(0x00000400),
	XHTTP_CACHE_PUBLIC = UINT32_C(0x00000800),
	XHTTP_CACHE_S_MAXAGE = UINT32_C(0x00001000)
} xhttpcachedirective;



/* 汇总标志保留字段存在性、扩展、重复、非法参数和限定字段事实。 */
typedef enum xhttpcacheflag {
	XHTTP_CACHE_FLAG_NONE = 0,
	XHTTP_CACHE_PRESENT = UINT32_C(0x00010000),
	XHTTP_CACHE_EXTENSION = UINT32_C(0x00020000),
	XHTTP_CACHE_DUPLICATE = UINT32_C(0x00040000),
	XHTTP_CACHE_INVALID = UINT32_C(0x00080000),
	XHTTP_CACHE_MAX_STALE_ANY = UINT32_C(0x00100000),
	XHTTP_CACHE_NO_CACHE_FIELDS = UINT32_C(0x00200000),
	XHTTP_CACHE_PRIVATE_FIELDS = UINT32_C(0x00400000),
	XHTTP_CACHE_CONFLICT = UINT32_C(0x00800000)
} xhttpcacheflag;



/* 过大的 delta-seconds 按 RFC 9111 饱和到该安全值。 */
#define XHTTP_CACHE_DELTA_MAX UINT64_C(2147483648)



/* 游标可在重复 Cache-Control 字段之间无分配前向迭代。 */
typedef struct xhttpcachecursor {
	size_t Field;
	size_t Offset;
} xhttpcachecursor;



/*
	缓存条目借用原字段文本。
	Flags 使用 XHTTP_PARAM_HAS_VALUE 与 XHTTP_PARAM_QUOTED。
*/
typedef struct xhttpcacheitem {
	xstrview Name;
	xstrview Value;
	xhttpcachedirective Directive;
	uint32 Flags;
} xhttpcacheitem;



/*
	汇总只保存协议事实，不决定请求或响应缓存策略。
	重复数值保留第一次出现的值；计数报告数量，两个 Directives 位图精确归因。
*/
typedef struct xhttpcachecontrol {
	uint64 MaxAge;
	uint64 MaxStale;
	uint64 MinFresh;
	uint64 SMaxAge;
	size_t FieldCount;
	size_t DirectiveCount;
	size_t UnknownCount;
	size_t DuplicateCount;
	size_t InvalidCount;
	uint32 DuplicateDirectives;
	uint32 InvalidDirectives;
	uint32 Flags;
} xhttpcachecontrol;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE)

/* 返回已知缓存指令的静态小写名称；UNKNOWN 返回空视图。 */
XRT_API xstrview xrtHttpCacheDirectiveName(
	xhttpcachedirective Directive
);



/* 大小写不敏感地映射缓存指令；未知或非法名称返回 UNKNOWN。 */
XRT_API xhttpcachedirective xrtHttpCacheDirectiveParse(
	xstrview Name
);



/* 初始化可重复使用的 Cache-Control 前向游标。 */
XRT_API void xrtHttpCacheCursorInit(xhttpcachecursor* pCursor);



/*
	按字段出现顺序读取全部 Cache-Control 指令。
	未知扩展仍返回 ITEM；列表语法错误返回 ERROR。
*/
XRT_API xhttpnext xrtHttpCacheNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecursor* pCursor,
	xhttpcacheitem* pItem
);



/*
	严格解析一个完整 delta-seconds，允许两端 OWS。
	过大值饱和到 XHTTP_CACHE_DELTA_MAX。
*/
XRT_API bool xrtHttpCacheDeltaParse(
	xstrview Text,
	uint64* pSeconds
);



/*
	读取已知数值指令的 delta-seconds。
	token 与 quoted-string 都可接收，过大值饱和到 XHTTP_CACHE_DELTA_MAX。
*/
XRT_API bool xrtHttpCacheDeltaRead(
	const xhttpcacheitem* pItem,
	uint64* pSeconds
);



/* 初始化不存在 Cache-Control 字段的空汇总。 */
XRT_API void xrtHttpCacheControlInit(
	xhttpcachecontrol* pControl
);



/* 判断公开汇总结构是否满足可继续增量合并的内部约束。 */
XRT_API bool xrtHttpCacheControlValid(
	const xhttpcachecontrol* pControl
);



/*
	失败原子地合并一个 Cache-Control 字段值。
	已知指令的非法参数被记录为 INVALID，只有列表语法错误会使函数失败。
*/
XRT_API bool xrtHttpCacheControlAdd(
	xhttpcachecontrol* pControl,
	xstrview Value
);



/* 扫描全部重复 Cache-Control 字段并建立零分配汇总。 */
XRT_API bool xrtHttpCacheControlParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecontrol* pControl
);

#endif



XRT_EXTERN_C_END

#endif
