#ifndef XRT_HTTP_SSE_CLIENT_H
#define XRT_HTTP_SSE_CLIENT_H

#include <xrt/http_sse.h>
#include <xrt/http_client_runtime.h>



#if defined(XRT_FEATURE_HTTP_SSE_CLIENT) && \
	(!defined(XRT_FEATURE_HTTP_SSE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_SSE_PARSER) || \
	 !defined(XRT_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT))
	#error "XRT HTTP SSE client requires SSE HTTP, Parser, Client and Redirect support"
#endif



#if defined(XRT_FEATURE_HTTP_SSE_CLIENT)

/* 默认无限重连，并把服务端毫秒延迟约束在 100 ms 到五分钟。 */
#define XHTTP_SSE_RECONNECT_MAX_DEFAULT SIZE_MAX
#define XHTTP_SSE_RETRY_MIN_DEFAULT UINT64_C(100)
#define XHTTP_SSE_RETRY_MAX_DEFAULT UINT64_C(300000)



/* SSE Client 与 WHATWG EventSource 保持 CONNECTING、OPEN、CLOSED 三态。 */
typedef enum xhttpsseclientstate {
	XHTTP_SSE_CLIENT_CONNECTING = 0,
	XHTTP_SSE_CLIENT_OPEN = 1,
	XHTTP_SSE_CLIENT_CLOSED = 2
} xhttpsseclientstate;



/* 终态原因区分主动关闭、协议停止、永久失败和本地资源失败。 */
typedef enum xhttpsseclosereason {
	XHTTP_SSE_CLOSE_USER = 0,
	XHTTP_SSE_CLOSE_CANCELLED,
	XHTTP_SSE_CLOSE_STOP,
	XHTTP_SSE_CLOSE_REJECTED,
	XHTTP_SSE_CLOSE_PARSE,
	XHTTP_SSE_CLOSE_CALLBACK,
	XHTTP_SSE_CLOSE_HTTP,
	XHTTP_SSE_CLOSE_RECONNECT_LIMIT,
	XHTTP_SSE_CLOSE_INTERNAL
} xhttpsseclosereason;



/* 稳定错误码为错误链提供 SSE 会话层上下文。 */
typedef enum xhttpsseclienterror {
	XHTTP_SSE_CLIENT_ERROR_ARGUMENT = 1,
	XHTTP_SSE_CLIENT_ERROR_STATE,
	XHTTP_SSE_CLIENT_ERROR_CONFIG,
	XHTTP_SSE_CLIENT_ERROR_REQUEST,
	XHTTP_SSE_CLIENT_ERROR_RESPONSE,
	XHTTP_SSE_CLIENT_ERROR_PARSE,
	XHTTP_SSE_CLIENT_ERROR_CALLBACK,
	XHTTP_SSE_CLIENT_ERROR_CANCELLED,
	XHTTP_SSE_CLIENT_ERROR_HTTP,
	XHTTP_SSE_CLIENT_ERROR_RECONNECT,
	XHTTP_SSE_CLIENT_ERROR_INTERNAL
} xhttpsseclienterror;



/* Client 是一个拥有 Parser、Last-Event-ID 和重连状态的订阅会话。 */
typedef struct xhttpsseclient xhttpsseclient;



/* 配置内嵌一次 HTTP Call 策略；动态借用字段会在 Connect 时深持有。 */
typedef struct xhttpsseclientconfig {
	xhttpsseparserconfig Parser;
	xhttpcalloptions Http;
	size_t MaxReconnects;
	uint64 RetryMin;
	uint64 RetryMax;
} xhttpsseclientconfig;



/* 并发快照不暴露 Parser 内部借用视图。 */
typedef struct xhttpsseclientinfo {
	xhttpsseclientstate State;
	uint16 Status;
	uint64 Retry;
	uint64 Messages;
	uint64 Comments;
	uint64 RetryUpdates;
	size_t Reconnects;
	bool Paused;
} xhttpsseclientinfo;



/* Open 与流项目在 HTTP Call 的网络 Worker 上同步发布。 */
typedef bool (*xhttpsseopenproc)(
	xhttpsseclient* pClient,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpssemessageproc)(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
);

typedef bool (*xhttpssecommentproc)(
	xhttpsseclient* pClient,
	xstrview Comment,
	ptr pData
);

typedef bool (*xhttpsseretryproc)(
	xhttpsseclient* pClient,
	uint64 iRetry,
	ptr pData
);



/* Retrying 报告一次暂态断开；Error 只在本次回调期间借用。 */
typedef void (*xhttpsseretryingproc)(
	xhttpsseclient* pClient,
	size_t iReconnect,
	uint64 iDelay,
	const xerror* pError,
	ptr pData
);



/* Close 是唯一终态回调；Error 在回调后仍可由 ClientError 借用。 */
typedef void (*xhttpssecloseproc)(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
);



/* Message 必须存在，其余事件按需启用。 */
typedef struct xhttpsseclientevents {
	xhttpsseopenproc Open;
	xhttpssemessageproc Message;
	xhttpssecommentproc Comment;
	xhttpsseretryproc Retry;
	xhttpsseretryingproc Retrying;
	xhttpssecloseproc Close;
	ptr Data;
} xhttpsseclientevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SSE_CLIENT)

/* 初始化无总超时、无空闲超时、自动重连和有界重连延迟；允许未对齐存储。 */
XRT_API void xrtHttpSseClientConfigInit(
	xhttpsseclientconfig* pConfig
);



/* 使用 GET URL 建立会话；配置与事件表在返回前完整快照并允许未对齐存储。 */
XRT_API xhttpsseclient* xrtHttpSseConnect(
	xhttpclient* pHttp,
	xstrview Url,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
);



/* 克隆 GET 请求；配置与事件表在返回前完整快照并允许未对齐存储。 */
XRT_API xhttpsseclient* xrtHttpSseConnectRequest(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
);



/* 增加会话引用并返回原指针。 */
XRT_API xhttpsseclient* xrtHttpSseClientRef(
	xhttpsseclient* pClient
);



/* 释放会话引用；空指针视为空操作，不隐式关闭仍活动的订阅。 */
XRT_API void xrtHttpSseClientDestroy(
	xhttpsseclient* pClient
);



/* 从任意线程永久关闭当前 HTTP Call 或重连 Timer。 */
XRT_API bool xrtHttpSseClientClose(
	xhttpsseclient* pClient
);



/* 在当前回调 Worker 上暂停项目交付和底层传输读取。 */
XRT_API bool xrtHttpSseClientPause(
	xhttpsseclient* pClient
);



/* 从任意线程恢复 Parser 尾段，再开放底层传输读取。 */
XRT_API bool xrtHttpSseClientResume(
	xhttpsseclient* pClient
);



/* 返回当前 EventSource 三态的并发快照。 */
XRT_API xhttpsseclientstate xrtHttpSseClientState(
	const xhttpsseclient* pClient
);



/* 返回当前暂停门快照。 */
XRT_API bool xrtHttpSseClientPaused(
	const xhttpsseclient* pClient
);



/* 原子复制状态、状态码、重连时间和累计项目数量；输出允许未对齐存储。 */
XRT_API bool xrtHttpSseClientInfo(
	const xhttpsseclient* pClient,
	xhttpsseclientinfo* pInfo
);



/* 唯一 Close 已发布后返回稳定错误；正常主动或 204 停止可以为空。 */
XRT_API const xerror* xrtHttpSseClientError(
	const xhttpsseclient* pClient
);

#endif



XRT_EXTERN_C_END
#endif
