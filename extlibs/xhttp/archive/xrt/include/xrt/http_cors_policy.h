#ifndef XRT_HTTP_CORS_POLICY_H
#define XRT_HTTP_CORS_POLICY_H

#include <xrt/http_cors.h>



#if defined(XRT_FEATURE_HTTP_CORS_POLICY) && \
	!defined(XRT_FEATURE_HTTP_CORS)
	#error "XRT HTTP CORS policy requires CORS protocol support"
#endif

#if defined(XRT_FEATURE_HTTP_CORS_WRITE) && \
	(!defined(XRT_FEATURE_HTTP_CORS_POLICY) || \
	 !defined(XRT_FEATURE_HTTP_ORIGIN_WRITE))
	#error "XRT HTTP CORS writer requires CORS policy and Origin writer support"
#endif



#if defined(XRT_FEATURE_HTTP_CORS_POLICY)

/* 策略标志控制通配、凭据和预检缓存，不隐含应用路由授权。 */
typedef enum xhttpcorspolicyflag {
	XHTTP_CORS_POLICY_NONE = 0,
	XHTTP_CORS_POLICY_ANY_ORIGIN = UINT32_C(0x00000001),
	XHTTP_CORS_POLICY_CREDENTIALS = UINT32_C(0x00000002),
	XHTTP_CORS_POLICY_ANY_METHOD = UINT32_C(0x00000004),
	XHTTP_CORS_POLICY_ANY_HEADER = UINT32_C(0x00000008),
	XHTTP_CORS_POLICY_MAX_AGE = UINT32_C(0x00000010)
} xhttpcorspolicyflag;



/* 策略拒绝与参数、协议和内存错误分开返回。 */
typedef enum xhttpcorsreject {
	XHTTP_CORS_REJECT_NONE = 0,
	XHTTP_CORS_REJECT_ORIGIN,
	XHTTP_CORS_REJECT_METHOD,
	XHTTP_CORS_REJECT_HEADER
} xhttpcorsreject;



/* 决策位直接描述应写出的 CORS 响应字段。 */
typedef enum xhttpcorsdecisionflag {
	XHTTP_CORS_DECISION_NONE = 0,
	XHTTP_CORS_DECISION_ALLOW = UINT32_C(0x00000001),
	XHTTP_CORS_DECISION_PREFLIGHT = UINT32_C(0x00000002),
	XHTTP_CORS_DECISION_CREDENTIALS = UINT32_C(0x00000004),
	XHTTP_CORS_DECISION_ALLOW_HEADERS = UINT32_C(0x00000008),
	XHTTP_CORS_DECISION_EXPOSE_HEADERS = UINT32_C(0x00000010),
	XHTTP_CORS_DECISION_MAX_AGE = UINT32_C(0x00000020),
	XHTTP_CORS_DECISION_VARY_ORIGIN = UINT32_C(0x00000040)
} xhttpcorsdecisionflag;



/* 策略只借用预解析 Origin 与 token 数组；方法和字段通配由 Flags 明确表达。 */
typedef struct xhttpcorspolicy {
	const xhttporigin* Origins;
	size_t OriginCount;
	const xstrview* Methods;
	size_t MethodCount;
	const xstrview* Headers;
	size_t HeaderCount;
	const xstrview* ExposeHeaders;
	size_t ExposeCount;
	uint64 MaxAge;
	uint32 Flags;
} xhttpcorspolicy;



/* 决策借用请求 Origin、预检方法和策略暴露字段数组。 */
typedef struct xhttpcorsdecision {
	xhttpcorsorigin AllowOrigin;
	xstrview AllowMethod;
	const xstrview* ExposeHeaders;
	size_t ExposeCount;
	size_t HeaderCount;
	uint64 MaxAge;
	xhttpcorsreject Reject;
	uint32 Flags;
} xhttpcorsdecision;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CORS_POLICY)

/*
	读取请求并执行数组策略。
	返回 false 表示 API、协议或策略描述错误；策略拒绝返回 true 并设置 Reject。
*/
XRT_API bool xrtHttpCorsPolicyCheck(
	const xhttpcorspolicy* pPolicy,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsdecision* pDecision
);

#endif



#if defined(XRT_FEATURE_HTTP_CORS_WRITE)

/*
	直接写出 CORS 响应字段行，不附加最终空行。
	预检响应的 Vary 同时覆盖 Origin、Request-Method 与 Request-Headers 依赖。
	NULL/0 查询精确大小；容量不足不写出部分结果。
*/
XRT_API bool xrtHttpCorsDecisionWrite(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
