#ifndef XWS_SERVER_H_API
#define XWS_SERVER_H_API

#include <xws-server/export.h>
#include <xws.h>

#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)
	#include <xrt/http_server_upgrade.h>
#endif




#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE))
	#error "XRT WebSocket server requires handshake, connection and HTTP Upgrade"
#endif


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_TLS) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT WebSocket TLS server requires server, TLS connection and HTTPS server"
#endif


#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE) && \
	(!defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate server requires server, compressed connection and negotiation"
#endif




/*
	服务端压缩策略返回 true 表示接受并写出 Response。
	返回 false 且未设置线程错误表示主动放弃，带错误则终止握手。
	返回 true 时回调内部已恢复的错误会被丢弃，进入回调前的线程错误保持不变。
*/
typedef bool (*xwsdeflateacceptproc)(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
);



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/*
	Accept 是末尾补零的响应值。
	Protocol 借用请求字段；空视图表示没有协商子协议。
*/
typedef struct xwsserverhandshake {
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Protocol;
	xwsdeflate Deflate;
	char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
	bool DeflateEnabled;

} xwsserverhandshake;



/*
	Protocols 是按服务端支持顺序书写的候选列表，但选择保持客户端偏好顺序。
	Upgrade 调用返回后不再借用配置中的任何视图。
*/
typedef struct xwsserverconfig {
	xwsconnconfig Connection;
	xstrview Protocols;
	xwsdeflateacceptproc AcceptDeflate;
	ptr DeflateData;
	bool EnableDeflate;
	bool RequireDeflate;

} xwsserverconfig;



/*
	异步 Upgrade 终态只发布一次。
	成功时 Connection 调用方引用转移给回调；Error 只在回调期间借用。
*/
typedef void (*xwsupgradeproc)(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
);

#endif






XRT_EXTERN_C_BEGIN



#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER)

/* 在完整、允许未对齐的存储中初始化服务端默认配置。 */
XWS_SERVER_API void xrtWsServerConfigInit(
	xwsserverconfig* pConfig
);



/* 从允许未对齐的完整快照无分配验证服务端配置。 */
XWS_SERVER_API bool xrtWsServerConfigValid(
	const xwsserverconfig* pConfig
);



/*
	严格验证一个完整 HTTP/1.1 WebSocket 请求并计算响应。
	配置和输出允许未对齐，输出不得覆盖请求或配置；
	重复子协议字段按一份合并列表验证，失败不修改 Handshake。
*/
XWS_SERVER_API bool xrtWsServerCheck(
	const xhttpserverrequest* pRequest,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
);



/* 从完整、允许未对齐的已验证握手快照创建拥有型 101 Reply。 */
XWS_SERVER_API xhttpreply* xrtWsServerReply(
	const xwsserverhandshake* pHandshake
);



/*
	把 WebSocket 握手错误映射为无正文 400、405、426 或 500 最终响应。
	Error 为空或不属于握手域时按服务端内部错误处理。
*/
XWS_SERVER_API xnetresult xrtWsServerReject(
	xhttpconn* pHttp,
	const xerror* pError
);



/*
	提交已经通过 xrtWsServerCheck 的 WebSocket 握手。
	Headers 可以为空；非空时只追加普通响应字段，Upgrade、Connection、
	Sec-WebSocket-* 以及正文分帧字段由协议层独占，不能覆盖。
	Handshake 和 Headers 只在调用期间借用，配置、事件和子协议会在返回前快照。
	该入口适合鉴权后追加 Cookie、追踪标识或应用自定义 Header；同步失败不调用 Proc。
*/
XWS_SERVER_API xnetresult xrtWsUpgradeAccept(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsserverhandshake* pHandshake,
	const xhttpheaders* pHeaders,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
);



/*
	验证当前 HTTP 请求、提交 101 并接管为 WebSocket Connection。
	配置和事件表在分配前快照；调用前可以直接读取请求完成鉴权；
	EventData 只交给 Connection 事件，Data 只交给完成回调；
	同步失败不会调用 Proc。
*/
XWS_SERVER_API xnetresult xrtWsUpgrade(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
);

#endif






XRT_EXTERN_C_END

#endif
