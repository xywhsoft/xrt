#ifndef XRT_WEBSOCKET_HTTP_FUTURE_H
#define XRT_WEBSOCKET_HTTP_FUTURE_H

#include <xrt/websocket_http.h>

#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE) || \
	defined(XWS_FEATURE_WEBSOCKET_SERVER_FUTURE)
	#include <xrt/future.h>
#endif



#if defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE) && \
	!defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
	#error "XRT WebSocket HTTP Future result requires WebSocket connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT WebSocket client Future requires client, result and Future bridge support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_SERVER_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT WebSocket server Future requires server, result and Future bridge support"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE)

/*
	连接建立结果拥有 WebSocket Connection。
	客户端 Future 启用时，成功结果还拥有对应的 101 Response。
*/
typedef struct xwsopenresult xwsopenresult;



/* 连接建立结果错误码只描述结果对象本身，不依赖 HTTP 握手实现。 */
typedef enum xwsopenresulterror {
	XWS_OPEN_RESULT_ERROR_ARGUMENT = 1,
	XWS_OPEN_RESULT_ERROR_STATE
} xwsopenresulterror;



XRT_EXTERN_C_BEGIN



/*
	增加连接建立结果引用并返回原指针。
	空指针、无效对象范围或不可再增加的引用状态返回 NULL 并记录错误。
*/
XRT_API xwsopenresult* xrtWsOpenResultRef(
	xwsopenresult* pResult
);



/*
	释放结果引用。
	最后一个引用会中止尚未取走的 Connection，并销毁尚未取走的 Response。
	NULL 是空操作；非空但无效的对象范围会记录参数错误。
*/
XRT_API void xrtWsOpenResultDestroy(
	xwsopenresult* pResult
);



/*
	返回结果借用的 Connection；结果有效且 Connection 未被取走期间可用。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XRT_API xwsconn* xrtWsOpenResultConnection(
	const xwsopenresult* pResult
);



/*
	原子取走 Connection 所有权。
	调用方必须在使用结束后关闭或中止 Connection，并释放其调用方引用。
	同一结果的 Take 与借用访问不可并发。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XRT_API xwsconn* xrtWsOpenResultTakeConnection(
	xwsopenresult* pResult
);



#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/* 返回客户端结果借用的 101 Response；无效结果返回 NULL 并记录参数错误。 */
XRT_API const xhttpresponse* xrtWsOpenResultResponse(
	const xwsopenresult* pResult
);



/*
	原子取走客户端 101 Response 所有权；同一结果的 Take 与借用访问不可并发。
	无效结果返回 NULL 并记录参数错误。
*/
XRT_API xhttpresponse* xrtWsOpenResultTakeResponse(
	xwsopenresult* pResult
);



/*
	使用 ws/wss URL 异步建立 WebSocket。
	Future 成功值是其拥有的 xwsopenresult；取消会协作取消底层 HTTP Call。
	Client、Config 和 Events 在创建任何异步状态前验证完整对象范围。
	Connection 事件使用独立的 Data，不会看到 Future 内部桥接上下文。
*/
XRT_API xfuture* xrtWsConnectAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/* 以自定义 GET 请求为基础异步建立 WebSocket；Request 同样先验证完整对象范围。 */
XRT_API xfuture* xrtWsConnectRequestAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/*
	阻塞建立 WebSocket 并返回拥有型结果。
	网络 Worker 不允许调用；协程应等待 xrtWsConnectAsync 返回的 Future。
*/
XRT_API xwsopenresult* xrtWsConnectSync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



/* 以自定义 GET 请求为基础阻塞建立 WebSocket。 */
XRT_API xwsopenresult* xrtWsConnectRequestSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);

#endif



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	验证当前请求、提交 101，并以 Future 交付拥有型连接建立结果。
	取消 Future 会异常关闭当前 HTTP Connection；此函数只应在 Request Worker 中调用。
	HTTP Connection、Config 和 Events 在分配桥接状态前验证完整对象范围。
	Connection 事件使用独立的 Data，不会看到 Future 内部桥接上下文。
*/
XRT_API xfuture* xrtWsUpgradeAsync(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);

#endif



XRT_EXTERN_C_END

#endif

#endif
