#ifndef XRT_HTTP_CORS_CACHE_H
#define XRT_HTTP_CORS_CACHE_H

#include <xrt/http_cors_client.h>



#if defined(XRT_FEATURE_HTTP_CORS_CACHE) && \
	(!defined(XRT_FEATURE_HTTP_CORS_CLIENT) || \
	 !defined(XRT_FEATURE_MAP) || \
	 !defined(XRT_FEATURE_MUTEX) || \
	 !defined(XRT_FEATURE_TIME))
	#error "XRT HTTP CORS cache requires client, Map, Mutex and Time support"
#endif



#if defined(XRT_FEATURE_HTTP_CORS_CACHE)

#define XHTTP_CORS_CACHE_ENTRIES_DEFAULT	512u
#define XHTTP_CORS_CACHE_INITIAL_DEFAULT	64u
#define XHTTP_CORS_CACHE_MAX_AGE_DEFAULT	UINT64_C(86400)



/* CORS Cache 是引用计数、线程安全且有界的预检权限缓存。 */
typedef struct xhttpcorscache xhttpcorscache;



/* Key 用调用方定义的网络分区、请求 Origin 和规范目标 URL 隔离权限。 */
typedef struct xhttpcorscachekey {
	xhttporigin Origin;
	xstrview URL;
	xstrview Partition;
} xhttpcorscachekey;



/* MaxAge 是实现接受的秒数上限，阻止服务端产生无限期权限。 */
typedef struct xhttpcorscacheconfig {
	size_t InitialEntries;
	size_t MaxEntries;
	uint64 MaxAge;
} xhttpcorscacheconfig;



/* Stats 在同一把锁内取得，累计计数在缓存生命周期内保持单调。 */
typedef struct xhttpcorscachestats {
	size_t Entries;
	uint64 Lookups;
	uint64 Hits;
	uint64 Misses;
	uint64 Stores;
	uint64 Replacements;
	uint64 Evictions;
	uint64 Expired;
	uint64 Removals;
} xhttpcorscachestats;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CORS_CACHE)

/* 写入可直接用于 Create 的默认有界配置。 */
XRT_API void xrtHttpCorsCacheConfigInit(
	xhttpcorscacheconfig* pConfig
);



/* 初始化没有网络分区的缓存键；URL 由调用方保持规范且不含片段。 */
XRT_API bool xrtHttpCorsCacheKeyInit(
	xhttpcorscachekey* pKey,
	const xhttporigin* pOrigin,
	xstrview URL
);



/* 创建线程安全的有界内存缓存；空配置使用默认值。 */
XRT_API xhttpcorscache* xrtHttpCorsCacheCreate(
	const xhttpcorscacheconfig* pConfig
);



/* 增加 Cache 引用并返回原指针；引用溢出时返回空指针。 */
XRT_API xhttpcorscache* xrtHttpCorsCacheRetain(
	const xhttpcorscache* pCache
);



/* 释放 Cache 引用；空指针是安全的空操作。 */
XRT_API void xrtHttpCorsCacheRelease(xhttpcorscache* pCache);



/*
	规划请求并查询缓存；命中时增加 CACHED 标志，但保留原始触发原因。
	调用方仅在 REQUIRED 存在且 CACHED 不存在时发送网络预检。
*/
XRT_API bool xrtHttpCorsCachePlan(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	bool bCredentials,
	bool bForce,
	xhttpcorspreflightplan* pPlan
);



/*
	把已经通过 xrtHttpCorsPreflightCheck 的响应权限写入缓存。
	Result 必须同时带有 ALLOW 和 PREFLIGHT；MaxAge 会按配置钳制。
	单次更新至多准备 MaxEntries 个权限，超出部分不缓存且不临时分配。
	方法字段缺失且 bForce 为 true 时，按 Fetch 规则缓存当前方法。
*/
XRT_API bool xrtHttpCorsCacheUpdate(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	xstrview Method,
	bool bForce,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	const xhttpcorsclientresult* pResult,
	size_t* pUpdated
);



/* 删除同一分区、Origin 和 URL 下的全部凭据与权限项。 */
XRT_API bool xrtHttpCorsCacheRemove(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	size_t* pRemoved
);



/* 删除全部已经到期的权限项，并返回实际删除数量。 */
XRT_API bool xrtHttpCorsCachePurge(
	xhttpcorscache* pCache,
	size_t* pRemoved
);



/* 清空全部权限并保留 Map 桶容量供后续复用。 */
XRT_API bool xrtHttpCorsCacheClear(xhttpcorscache* pCache);



/* 取得一致统计快照；未对齐输出受支持，失败时输出全零。 */
XRT_API bool xrtHttpCorsCacheStats(
	xhttpcorscache* pCache,
	xhttpcorscachestats* pStats
);

#endif



XRT_EXTERN_C_END

#endif
