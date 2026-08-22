#ifndef XRT_HTTP_CACHE_TIME_H
#define XRT_HTTP_CACHE_TIME_H

#include <xrt/http_cache.h>
#include <xrt/time.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE) || \
	 !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRT HTTP cache time support requires cache control and time text support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME)

/* 时间元数据标志分别保留字段存在、重复成员和非法值事实。 */
typedef enum xhttpcachetimeflag {
	XHTTP_CACHE_TIME_NONE = 0,
	XHTTP_CACHE_TIME_DATE = UINT32_C(0x00000001),
	XHTTP_CACHE_TIME_AGE = UINT32_C(0x00000002),
	XHTTP_CACHE_TIME_EXPIRES = UINT32_C(0x00000004),
	XHTTP_CACHE_TIME_DATE_DUPLICATE = UINT32_C(0x00000010),
	XHTTP_CACHE_TIME_AGE_EXTRA = UINT32_C(0x00000020),
	XHTTP_CACHE_TIME_EXPIRES_DUPLICATE = UINT32_C(0x00000040),
	XHTTP_CACHE_TIME_DATE_INVALID = UINT32_C(0x00000100),
	XHTTP_CACHE_TIME_AGE_INVALID = UINT32_C(0x00000200),
	XHTTP_CACHE_TIME_EXPIRES_INVALID = UINT32_C(0x00000400)
} xhttpcachetimeflag;



/*
	缓存时间元数据独立于存储和网络。
	Date 与 Expires 使用 Unix Epoch 微秒，Age 使用线路秒数。
*/
typedef struct xhttpcachetime {
	xtime Date;
	xtime Expires;
	uint64 Age;
	size_t DateCount;
	size_t AgeCount;
	size_t AgeMemberCount;
	size_t ExpiresCount;
	uint32 Flags;
} xhttpcachetime;



/* 计算结果区分程序错误、无显式值、有效结果和保守失效。 */
typedef enum xhttpcachecalc {
	XHTTP_CACHE_CALC_ERROR = -1,
	XHTTP_CACHE_CALC_NONE = 0,
	XHTTP_CACHE_CALC_READY = 1,
	XHTTP_CACHE_CALC_INVALID = 2
} xhttpcachecalc;



/*
	年龄结果保留 RFC 9111 公式的全部中间量。
	除 CurrentAgeSeconds 使用线路秒数外，其余成员均使用微秒。
*/
typedef struct xhttpcacheage {
	uint64 ApparentAge;
	uint64 ResponseDelay;
	uint64 CorrectedAgeValue;
	uint64 CorrectedInitialAge;
	uint64 ResidentTime;
	uint64 CurrentAge;
	uint64 CurrentAgeSeconds;
} xhttpcacheage;



/* 显式新鲜寿命来源按规范优先级确定。 */
typedef enum xhttpcachefreshnesssource {
	XHTTP_CACHE_FRESHNESS_NONE = 0,
	XHTTP_CACHE_FRESHNESS_S_MAXAGE,
	XHTTP_CACHE_FRESHNESS_MAX_AGE,
	XHTTP_CACHE_FRESHNESS_EXPIRES,
	XHTTP_CACHE_FRESHNESS_HEURISTIC,
	XHTTP_CACHE_FRESHNESS_EXTENSION
} xhttpcachefreshnesssource;



/* 显式新鲜寿命使用微秒，不包含站点自定义的启发式策略。 */
typedef struct xhttpcachefreshness {
	uint64 Lifetime;
	xhttpcachefreshnesssource Source;
} xhttpcachefreshness;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME)

/* 初始化不存在 Date、Age 和 Expires 字段的空时间元数据。 */
XRT_API void xrtHttpCacheTimeInit(
	xhttpcachetime* pTime
);



/* 判断公开时间元数据是否满足字段计数、标志和值约束。 */
XRT_API bool xrtHttpCacheTimeValid(
	const xhttpcachetime* pTime
);



/*
	零分配扫描 Date、Age 和 Expires。
	Age 采用合并列表中的第一个成员；非法 Age 被记录并在计算中视为零。
*/
XRT_API bool xrtHttpCacheTimeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachetime* pTime
);



/*
	按 RFC 9111 计算 current_age。
	ResponseTime 是接收响应时的墙钟；其余三个值必须来自同一单调时钟。
	单个非法 Date 使用 ResponseTime 替代；重复 Date 返回 INVALID。
*/
XRT_API xhttpcachecalc xrtHttpCacheCurrentAge(
	const xhttpcachetime* pTime,
	xtime ResponseTime,
	uint64 RequestClock,
	uint64 ResponseClock,
	uint64 NowClock,
	xhttpcacheage* pAge
);



/*
	按共享或私有缓存优先级计算显式新鲜寿命。
	无显式寿命返回 NONE；非法 Expires 按已过期处理，非法 Date 使用
	ResponseTime 替代；选中的重复单值字段或指令返回 INVALID。
*/
XRT_API xhttpcachecalc xrtHttpCacheFreshness(
	const xhttpcachecontrol* pControl,
	const xhttpcachetime* pTime,
	xtime ResponseTime,
	bool Shared,
	xhttpcachefreshness* pFreshness
);



/* 判断年龄公式的全部公开中间量是否相互一致。 */
XRT_API bool xrtHttpCacheAgeValid(
	const xhttpcacheage* pAge
);



/*
	判断寿命来源和值是否有效。
	NONE 只允许零寿命，启发式与扩展寿命可由上层策略构造。
*/
XRT_API bool xrtHttpCacheFreshnessValid(
	const xhttpcachefreshness* pFreshness
);



/* 按 freshness_lifetime > current_age 判断响应是否仍然新鲜。 */
XRT_API bool xrtHttpCacheFresh(
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness
);

#endif



XRT_EXTERN_C_END

#endif
