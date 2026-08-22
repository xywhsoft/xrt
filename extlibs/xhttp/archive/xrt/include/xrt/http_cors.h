#ifndef XRT_HTTP_CORS_H
#define XRT_HTTP_CORS_H

#include <xrt/http_origin.h>



#if defined(XRT_FEATURE_HTTP_CORS) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_ORIGIN))
	#error "XRT HTTP CORS support requires HTTP Origin support"
#endif



#if defined(XRT_FEATURE_HTTP_CORS)

/* CORS 允许源的星号形式不构造虚假的 Origin 三元组。 */
#define XHTTP_CORS_ORIGIN_WILDCARD UINT32_C(0x00000001)



/* CORS 请求事实明确区分普通 Origin 请求和预检请求。 */
typedef enum xhttpcorsrequestflag {
	XHTTP_CORS_REQUEST_NONE = 0,
	XHTTP_CORS_REQUEST_ORIGIN = UINT32_C(0x00000001),
	XHTTP_CORS_REQUEST_PREFLIGHT = UINT32_C(0x00000002),
	XHTTP_CORS_REQUEST_HEADERS = UINT32_C(0x00000004)
} xhttpcorsrequestflag;



/* CORS 响应事实保留每个字段的存在性，空列表不会被误认为缺失。 */
typedef enum xhttpcorsresponseflag {
	XHTTP_CORS_RESPONSE_NONE = 0,
	XHTTP_CORS_RESPONSE_ALLOW_ORIGIN = UINT32_C(0x00000001),
	XHTTP_CORS_RESPONSE_CREDENTIALS = UINT32_C(0x00000002),
	XHTTP_CORS_RESPONSE_ALLOW_METHODS = UINT32_C(0x00000004),
	XHTTP_CORS_RESPONSE_ALLOW_HEADERS = UINT32_C(0x00000008),
	XHTTP_CORS_RESPONSE_EXPOSE_HEADERS = UINT32_C(0x00000010),
	XHTTP_CORS_RESPONSE_MAX_AGE = UINT32_C(0x00000020)
} xhttpcorsresponseflag;



/* Allow-Origin 保存星号或一个借用的 Origin，二者互斥。 */
typedef struct xhttpcorsorigin {
	xhttporigin Origin;
	uint32 Flags;
} xhttpcorsorigin;



/* CORS 列表游标复用 HTTP 重复同名 token-list 的稳定状态。 */
typedef xhttpfieldtokencursor xhttpcorscursor;



/* 请求视图借用实际方法、Origin 和预检方法，不复制请求头名称。 */
typedef struct xhttpcorsrequest {
	xhttporigin Origin;
	xstrview Method;
	xstrview RequestMethod;
	size_t HeaderCount;
	uint32 Flags;
} xhttpcorsrequest;



/* 响应视图汇总字段事实；列表成员通过对应游标按需读取。 */
typedef struct xhttpcorsresponse {
	xhttpcorsorigin AllowOrigin;
	uint64 MaxAge;
	size_t MethodCount;
	size_t HeaderCount;
	size_t ExposeCount;
	uint32 Flags;
} xhttpcorsresponse;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CORS)

/* 严格读取一个 CORS method 字段值，结果借用输入。 */
XRT_API bool xrtHttpCorsMethodParse(
	xstrview Value,
	xstrview* pMethod
);



/* 严格读取 Access-Control-Allow-Origin 的星号或单一 Origin。 */
XRT_API bool xrtHttpCorsAllowOriginParse(
	xstrview Value,
	xhttpcorsorigin* pOrigin
);



/* 验证 Access-Control-Allow-Credentials 是否为区分大小写的 true。 */
XRT_API bool xrtHttpCorsAllowCredentialsParse(xstrview Value);



/* 严格读取 Access-Control-Max-Age 的无符号秒数。 */
XRT_API bool xrtHttpCorsMaxAgeParse(
	xstrview Value,
	uint64* pSeconds
);



/* 读取唯一 Access-Control-Allow-Origin 字段。 */
XRT_API xhttpnext xrtHttpCorsAllowOriginFields(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsorigin* pOrigin
);



/* 读取唯一 Access-Control-Request-Method 字段。 */
XRT_API xhttpnext xrtHttpCorsRequestMethodFields(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview* pMethod
);



/* 读取唯一 Access-Control-Allow-Credentials 字段；ITEM 表示 true。 */
XRT_API xhttpnext xrtHttpCorsAllowCredentialsFields(
	const xhttpfield* pFields,
	size_t iCount
);



/* 读取唯一 Access-Control-Max-Age 字段。 */
XRT_API xhttpnext xrtHttpCorsMaxAgeFields(
	const xhttpfield* pFields,
	size_t iCount,
	uint64* pSeconds
);



/* 初始化任一 CORS token-list 字段游标。 */
XRT_API void xrtHttpCorsCursorInit(xhttpcorscursor* pCursor);



/* 跨重复字段读取 Access-Control-Request-Headers 名称。 */
XRT_API xhttpnext xrtHttpCorsRequestHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
);



/* 跨重复字段读取 Access-Control-Allow-Methods 方法。 */
XRT_API xhttpnext xrtHttpCorsAllowMethodNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pMethod
);



/* 跨重复字段读取 Access-Control-Allow-Headers 名称。 */
XRT_API xhttpnext xrtHttpCorsAllowHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
);



/* 跨重复字段读取 Access-Control-Expose-Headers 名称。 */
XRT_API xhttpnext xrtHttpCorsExposeHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
);



/*
	一次性读取请求侧 CORS 字段。
	预检必须使用 OPTIONS，并同时携带 Origin 和 Request-Method。
*/
XRT_API bool xrtHttpCorsRequestRead(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsrequest* pRequest
);



/* 一次性验证并汇总响应侧 CORS 字段。 */
XRT_API bool xrtHttpCorsResponseRead(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsresponse* pResponse
);

#endif



XRT_EXTERN_C_END

#endif
