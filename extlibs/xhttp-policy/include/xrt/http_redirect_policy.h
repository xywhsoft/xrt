#ifndef XHTTP_REDIRECT_POLICY_H
#define XHTTP_REDIRECT_POLICY_H

#include <xhttp-policy/export.h>
#include <xrt/http_client.h>

#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS) || \
	 !defined(XHTTP_FEATURE_URL))
#error "HTTP redirect policy requires requests, trailers and URL support"
#endif

#define XHTTP_REDIRECT_MAX_DEFAULT UINT32_C(10)

#define XHTTP_REDIRECT_POST_TO_GET \
	UINT32_C(0x00000001)
#define XHTTP_REDIRECT_FORWARD_CREDENTIALS \
	UINT32_C(0x00000002)
#define XHTTP_REDIRECT_ALLOW_DOWNGRADE \
	UINT32_C(0x00000004)



/* 重定向配置用零跳明确关闭自动跟随，其余字段只调整安全策略。 */
typedef struct xhttpredirectconfig {
	uint32 Flags;
	uint32 MaxHops;
} xhttpredirectconfig;



/* 每次调用可以继承客户端策略、强制跟随、返回原响应或拒绝重定向。 */
typedef enum xhttpredirectmode {
	XHTTP_REDIRECT_DEFAULT = 0,
	XHTTP_REDIRECT_FOLLOW,
	XHTTP_REDIRECT_MANUAL,
	XHTTP_REDIRECT_ERROR
} xhttpredirectmode;



/* 无 IO 的下一跳构建结果；不会改变输入请求。 */
typedef enum xhttpredirecterror {
	XHTTP_REDIRECT_OK = 0,
	XHTTP_REDIRECT_INVALID,
	XHTTP_REDIRECT_REPLAY,
	XHTTP_REDIRECT_DOWNGRADE
} xhttpredirecterror;

XRT_EXTERN_C_BEGIN
#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)
/* 初始化十跳上限和安全重定向策略，允许未对齐完整存储。 */
XHTTP_POLICY_API void xrtHttpRedirectConfigInit(xhttpredirectconfig* pConfig);
/* 判断状态是否为 301、302、303、307 或 308。 */
XHTTP_POLICY_API bool xrtHttpRedirectStatus(uint16 iStatus);
/* 根据 Location 构建独立的下一跳请求，不访问网络。
   继承 fragment，按状态改写方法/正文，拒绝未授权降级和不可重放正文，
   跨 origin 默认去除凭据。成功返回的请求由调用方销毁；失败返回 NULL。
   pError 必须指向可写存储，给出拒绝原因；分配失败仍可读取 XRT 错误链。 */
XHTTP_POLICY_API xhttprequest* xrtHttpRedirectRequest(
	const xhttprequest* pRequest, uint16 iStatus, xstrview Location,
	uint32 iFlags, xhttpredirecterror* pError
);
#endif
XRT_EXTERN_C_END
#endif
