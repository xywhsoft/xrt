#ifndef XRT_HTTP_CACHE_STATUS_H
#define XRT_HTTP_CACHE_STATUS_H

#include <xrt/http_structured.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_STATUS) && \
	!defined(XHTTP_FEATURE_HTTP_STRUCTURED)
	#error "XRT HTTP Cache-Status requires Structured Fields support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CACHE_STATUS_WRITE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_STATUS) || \
	 !defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE))
	#error "XRT HTTP Cache-Status writer requires parser and Structured Fields writer support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STATUS)

/* Flags 表示类型正确的已知参数，InvalidFlags 使用相同位。 */
typedef enum xhttpcachestatusflag {
	XHTTP_CACHE_STATUS_HAS_HIT = 0x0001,
	XHTTP_CACHE_STATUS_HAS_FORWARD = 0x0002,
	XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS = 0x0004,
	XHTTP_CACHE_STATUS_HAS_TTL = 0x0008,
	XHTTP_CACHE_STATUS_HAS_STORED = 0x0010,
	XHTTP_CACHE_STATUS_HAS_COLLAPSED = 0x0020,
	XHTTP_CACHE_STATUS_HAS_KEY = 0x0040,
	XHTTP_CACHE_STATUS_HAS_DETAIL = 0x0080
} xhttpcachestatusflag;



/* 非语法错误但值得上层诊断的参数组合。 */
typedef enum xhttpcachestatusissue {
	XHTTP_CACHE_STATUS_ISSUE_HIT_AND_FORWARD = 0x01,
	XHTTP_CACHE_STATUS_ISSUE_FORWARD_REQUIRED = 0x02
} xhttpcachestatusissue;



/* 一个 Cache-Status 成员；所有文本视图借用原字段值。 */
typedef struct xhttpcachestatus {
	xhttpstructuredbare Cache;
	xhttpstructuredbare Forward;
	xhttpstructuredbare Key;
	xhttpstructuredbare Detail;
	xstrview Parameters;
	int64 ForwardStatus;
	int64 Ttl;
	uint16 Flags;
	uint16 InvalidFlags;
	uint8 Issues;
	uint8 Hit;
	uint8 Stored;
	uint8 Collapsed;
} xhttpcachestatus;



/* 单字段游标绑定首次迭代的不可变字段值，调用方不得直接修改。 */
typedef struct xhttpcachestatuscursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 Validated;
} xhttpcachestatuscursor;



/* 重复字段游标独立持有通用 Structured Fields 状态。 */
typedef struct xhttpcachestatusfieldcursor {
	xhttpstructuredfieldcursor Structured;
	uint8 Validated;
} xhttpcachestatusfieldcursor;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Cache-Status 字段值游标。 */
XRT_API void xrtHttpCacheStatusCursorInit(
	xhttpcachestatuscursor* pCursor
);



/* 初始化跨重复 Cache-Status 字段行游标。 */
XRT_API void xrtHttpCacheStatusFieldCursorInit(
	xhttpcachestatusfieldcursor* pCursor
);



/* 严格验证完整 Cache-Status 字段值和每个缓存标识。 */
XRT_API bool xrtHttpCacheStatusValid(xstrview Value);



/* 迭代一个完整 Cache-Status 字段值。 */
XRT_API xhttpnext xrtHttpCacheStatusNext(
	xstrview Value,
	xhttpcachestatuscursor* pCursor,
	xhttpcachestatus* pStatus
);



/* 按线路顺序跨越全部重复 Cache-Status 字段行。 */
XRT_API xhttpnext xrtHttpCacheStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachestatusfieldcursor* pCursor,
	xhttpcachestatus* pStatus
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STATUS_WRITE)

XRT_EXTERN_C_BEGIN



/* 规范写出一个 Cache-Status 成员，适合追加独立字段行。 */
XRT_API bool xrtHttpCacheStatusWrite(
	const xhttpstructureditemvalue* pStatus,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
