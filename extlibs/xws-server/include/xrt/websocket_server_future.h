#ifndef XWS_SERVER_FUTURE_H_API
#define XWS_SERVER_FUTURE_H_API

#include <xws-server/export.h>
#include <xrt/websocket_server.h>

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)
	#include <xrt/future.h>
#endif





#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT WebSocket server Future requires server, result and Future bridge support"
#endif




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	连接建立结果拥有 WebSocket Connection。
	客户端 Future 启用时，成功结果还拥有对应的 101 Response。
*/
typedef struct xwsserverresult xwsserverresult;



/* 连接建立结果错误码只描述结果对象本身，不依赖 HTTP 握手实现。 */
typedef enum xwsserverresulterror {
	XWS_SERVER_RESULT_ERROR_ARGUMENT = 1,
	XWS_SERVER_RESULT_ERROR_STATE
} xwsserverresulterror;



XRT_EXTERN_C_BEGIN



/*
	增加连接建立结果引用并返回原指针。
	空指针、无效对象范围或不可再增加的引用状态返回 NULL 并记录错误。
*/
XWS_SERVER_API xwsserverresult* xrtWsServerResultRef(
	xwsserverresult* pResult
);



/*
	释放结果引用。
	最后一个引用会中止尚未取走的 Connection，并销毁尚未取走的 Response。
	NULL 是空操作；非空但无效的对象范围会记录参数错误。
*/
XWS_SERVER_API void xrtWsServerResultDestroy(
	xwsserverresult* pResult
);



/*
	返回结果借用的 Connection；结果有效且 Connection 未被取走期间可用。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_SERVER_API xwsconn* xrtWsServerResultConnection(
	const xwsserverresult* pResult
);



/*
	原子取走 Connection 所有权。
	调用方必须在使用结束后关闭或中止 Connection，并释放其调用方引用。
	同一结果的 Take 与借用访问不可并发。
	空指针或无效对象范围返回 NULL 并记录参数错误。
*/
XWS_SERVER_API xwsconn* xrtWsServerResultTakeConnection(
	xwsserverresult* pResult
);






#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	验证当前请求、提交 101，并以 Future 交付拥有型连接建立结果。
	取消 Future 会异常关闭当前 HTTP Connection；此函数只应在 Request Worker 中调用。
	HTTP Connection、Config 和 Events 在分配桥接状态前验证完整对象范围。
	Connection 事件使用独立的 Data，不会看到 Future 内部桥接上下文。
*/
XWS_SERVER_API xfuture* xrtWsUpgradeAsync(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);

#endif



XRT_EXTERN_C_END

#endif


#endif
