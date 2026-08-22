#ifndef XRT_HTTP_CORS_CLIENT_H
#define XRT_HTTP_CORS_CLIENT_H

#include <xrt/http_cors_safelist.h>



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT) && \
	!defined(XRT_FEATURE_HTTP_CORS_SAFELIST)
	#error "XRT HTTP CORS client requires CORS safelist support"
#endif

#if defined(XRT_FEATURE_HTTP_CORS_CLIENT_WRITE) && \
	(!defined(XRT_FEATURE_HTTP_CORS_CLIENT) || \
	 !defined(XRT_FEATURE_HTTP_ORIGIN_WRITE))
	#error "XRT HTTP CORS client writer requires client and Origin writer support"
#endif



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)

/* 预检规划同时表达触发原因和需要发送的唯一非安全字段名数量。 */
typedef enum xhttpcorspreflightflag {
	XHTTP_CORS_PREFLIGHT_NONE = 0,
	XHTTP_CORS_PREFLIGHT_REQUIRED = UINT32_C(0x00000001),
	XHTTP_CORS_PREFLIGHT_METHOD = UINT32_C(0x00000002),
	XHTTP_CORS_PREFLIGHT_HEADERS = UINT32_C(0x00000004),
	XHTTP_CORS_PREFLIGHT_FORCED = UINT32_C(0x00000008),

	/* 可选预检缓存已经覆盖当前方法和全部非安全字段。 */
	XHTTP_CORS_PREFLIGHT_CACHED = UINT32_C(0x00000010)
} xhttpcorspreflightflag;



/* 客户端校验拒绝与 API、内存和字段语法错误分开返回。 */
typedef enum xhttpcorsclientreject {
	XHTTP_CORS_CLIENT_REJECT_NONE = 0,
	XHTTP_CORS_CLIENT_REJECT_STATUS,
	XHTTP_CORS_CLIENT_REJECT_ORIGIN,
	XHTTP_CORS_CLIENT_REJECT_CREDENTIALS,
	XHTTP_CORS_CLIENT_REJECT_METHOD,
	XHTTP_CORS_CLIENT_REJECT_HEADER
} xhttpcorsclientreject;



/* 校验标志指出请求是否通过，以及结果是否来自预检。 */
typedef enum xhttpcorsclientflag {
	XHTTP_CORS_CLIENT_NONE = 0,
	XHTTP_CORS_CLIENT_ALLOW = UINT32_C(0x00000001),
	XHTTP_CORS_CLIENT_PREFLIGHT = UINT32_C(0x00000002)
} xhttpcorsclientflag;



/* 预检规划不复制方法或字段，HeaderCount 是大小写不敏感的唯一名称数。 */
typedef struct xhttpcorspreflightplan {
	size_t HeaderCount;
	uint32 Flags;
} xhttpcorspreflightplan;



/* 预检成功时 MaxAge 可直接用于缓存；缺失或非法线路值按 Fetch 默认为 5 秒。 */
typedef struct xhttpcorsclientresult {
	uint64 MaxAge;
	xhttpcorsclientreject Reject;
	uint32 Flags;
} xhttpcorsclientresult;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)

/*
	依据方法、header-list 和可选强制标志规划 CORS 预检。
	字段必须是合法 HTTP 字段；函数不分配内存，也不修改输入。
*/
XRT_API bool xrtHttpCorsPreflightPlan(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	bool bForce,
	xhttpcorspreflightplan* pPlan
);



/*
	执行实际响应的 Allow-Origin 与 Allow-Credentials 检查。
	返回 false 表示 API 或协议错误；返回 true 时由 Reject 和 Flags 表达授权结果。
*/
XRT_API bool xrtHttpCorsClientCheck(
	const xhttporigin* pRequestOrigin,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	xhttpcorsclientresult* pResult
);



/*
	校验 2xx 预检响应的方法、非安全字段名和缓存时间。
	请求字段是即将发送的 header-list，不包含预检请求自身生成的 CORS 字段。
*/
XRT_API bool xrtHttpCorsPreflightCheck(
	uint16 iStatus,
	const xhttporigin* pRequestOrigin,
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	xhttpcorsclientresult* pResult
);

#endif



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT_WRITE)

/* 写出排序、去重和小写化的 Access-Control-Request-Headers 字段值。 */
XRT_API bool xrtHttpCorsPreflightHeaderNamesWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	直接写出 Origin、Request-Method 和可选 Request-Headers 字段行。
	不写 Accept 字段或最终空行；调用方可以直接追加到自己的请求头块。
*/
XRT_API bool xrtHttpCorsPreflightFieldsWrite(
	const xhttporigin* pOrigin,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END
#endif
