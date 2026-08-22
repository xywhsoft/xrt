#ifndef XRT_WEBSOCKET_HTTP_H
#define XRT_WEBSOCKET_HTTP_H

#include <xrt/websocket.h>

#if defined(XRT_FEATURE_WEBSOCKET_SERVER)
	#include <xrt/http_server_upgrade.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CLIENT)
	#include <xrt/http_client_runtime.h>
#endif



#if defined(XRT_FEATURE_WEBSOCKET_SERVER) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_UPGRADE))
	#error "XRT WebSocket server requires handshake, connection and HTTP Upgrade"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CLIENT) && \
	(!defined(XRT_FEATURE_WEBSOCKET_KEYGEN) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_HTTP_CLIENT))
	#error "XRT WebSocket client requires keygen, connection and HTTP client"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_SERVER_TLS) && \
	(!defined(XRT_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_TLS))
	#error "XRT WebSocket TLS server requires server, TLS connection and HTTPS server"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_HTTPS) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XRT_FEATURE_HTTP_CLIENT_HTTPS))
	#error "XRT WebSocket HTTPS client requires client, TLS connection and HTTPS client"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_SERVER_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_SERVER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate server requires server, compressed connection and negotiation"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate client requires client, compressed connection and negotiation"
#endif



#if defined(XRT_FEATURE_WEBSOCKET_SERVER_DEFLATE)
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
#endif



#if defined(XRT_FEATURE_WEBSOCKET_SERVER)

/*
	Accept 是末尾补零的响应值。
	Protocol 借用请求字段；空视图表示没有协商子协议。
*/
typedef struct xwsserverhandshake {
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Protocol;
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		xwsdeflate Deflate;
		char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
		bool DeflateEnabled;
	#endif
} xwsserverhandshake;



/*
	Protocols 是按服务端支持顺序书写的候选列表，但选择保持客户端偏好顺序。
	Upgrade 调用返回后不再借用配置中的任何视图。
*/
typedef struct xwsserverconfig {
	xwsconnconfig Connection;
	xstrview Protocols;
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		xwsdeflateacceptproc AcceptDeflate;
		ptr DeflateData;
		bool EnableDeflate;
		bool RequireDeflate;
	#endif
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



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT)

/*
	Protocols 和 Http 内的借用对象只需覆盖 Connect 调用。
	内部请求、协议列表、连接配置和事件表都会在返回前取得独立快照。
*/
typedef struct xwsclientconfig {
	xwsconnconfig Connection;
	xhttpcalloptions Http;
	xstrview Protocols;
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xwsdeflate Deflate;
		bool EnableDeflate;
		bool RequireDeflate;
	#endif
} xwsclientconfig;



/* 客户端握手快照借用 Response 的子协议，并拥有扩展协商参数。 */
typedef struct xwsclienthandshake {
	xstrview Protocol;
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		xwsdeflate Deflate;
		bool DeflateEnabled;
	#endif
} xwsclienthandshake;



/*
	完成回调在 HTTP Call 的网络 Worker 上发布一次。
	Response 无论握手成功失败都把非空调用方引用转移给回调；
	成功时 Connection 调用方引用同时转移，Error 只在回调期间借用。
*/
typedef void (*xwsconnectproc)(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_WEBSOCKET_SERVER)

/* 在完整、允许未对齐的存储中初始化服务端默认配置。 */
XRT_API void xrtWsServerConfigInit(
	xwsserverconfig* pConfig
);



/* 从允许未对齐的完整快照无分配验证服务端配置。 */
XRT_API bool xrtWsServerConfigValid(
	const xwsserverconfig* pConfig
);



/*
	严格验证一个完整 HTTP/1.1 WebSocket 请求并计算响应。
	配置和输出允许未对齐，输出不得覆盖请求或配置；
	重复子协议字段按一份合并列表验证，失败不修改 Handshake。
*/
XRT_API bool xrtWsServerCheck(
	const xhttpserverrequest* pRequest,
	const xwsserverconfig* pConfig,
	xwsserverhandshake* pHandshake
);



/* 从完整、允许未对齐的已验证握手快照创建拥有型 101 Reply。 */
XRT_API xhttpreply* xrtWsServerReply(
	const xwsserverhandshake* pHandshake
);



/*
	把 WebSocket 握手错误映射为无正文 400、405、426 或 500 最终响应。
	Error 为空或不属于握手域时按服务端内部错误处理。
*/
XRT_API xnetresult xrtWsServerReject(
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
XRT_API xnetresult xrtWsUpgradeAccept(
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
XRT_API xnetresult xrtWsUpgrade(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsupgradeproc pProc,
	ptr pData
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT)

/* 在完整、允许未对齐的存储中初始化客户端默认配置。 */
XRT_API void xrtWsClientConfigInit(
	xwsclientconfig* pConfig
);



/*
	创建可继续添加自定义 Header 的基础 GET 请求。
	Url 接受 ws、wss、http 或 https，并映射为 HTTP Client 可执行的 URL；
	该函数不写入任何 WebSocket 握手管理字段。
*/
XRT_API xhttprequest* xrtWsRequestCreate(
	xstrview Url
);



/*
	创建 GET WebSocket 请求；Url 接受 ws、wss、http 或 https。
	Url 不得包含 fragment；资源名中的井号必须编码为 %23。
	配置先复制为局部快照；成功时 Key 写入完整的固定输出。
*/
XRT_API xhttprequest* xrtWsClientRequestCreate(
	xstrview Url,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	克隆自定义 GET 请求并原子替换 WebSocket 管理字段。
	配置先复制为局部快照；Key 必须覆盖完整固定输出；
	源请求不得包含 fragment、正文、分帧字段或扩展字段。
*/
XRT_API xhttprequest* xrtWsClientRequestClone(
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	严格验证 101 响应、Accept、所选子协议和扩展响应。
	配置和输出允许未对齐，输出不得覆盖 Response、Key 或配置；
	Handshake 的子协议借用 Response，失败时保持不变。
*/
XRT_API bool xrtWsClientCheck(
	const xhttpresponse* pResponse,
	xstrview Key,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
);



/*
	快照配置和事件表后，使用 ws/wss URL 异步执行 WebSocket 握手。
	EventData 属于 Connection 事件，Data 属于完成回调，生命周期彼此独立。
*/
XRT_API xhttpcall* xrtWsConnect(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
);



/*
	以自定义 GET 请求为基础异步握手。
	配置和事件表在任何分配或 HTTP 提交前复制为局部快照。
	管理字段由适配器替换，其它 Header、代理、Cookie 和超时策略继续保留。
	EventData 与 Data 分别交给 Connection 事件和完成回调。
*/
XRT_API xhttpcall* xrtWsConnectRequest(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pEventData,
	xwsconnectproc pProc,
	ptr pData
);

#endif



XRT_EXTERN_C_END

#endif
