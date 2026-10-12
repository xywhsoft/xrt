#ifndef XWS_CLIENT_H_API
#define XWS_CLIENT_H_API

#include <xws-client/export.h>
#include <xws.h>


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)
	#include <xrt/http_client_runtime.h>
#endif




#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) && \
	(!defined(XRT_FEATURE_WEBSOCKET_KEYGEN) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT))
	#error "XRT WebSocket client requires keygen, connection and HTTP client"
#endif


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_HTTPS) && \
	(!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS))
	#error "XRT WebSocket HTTPS client requires client, TLS connection and HTTPS client"
#endif


#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT_DEFLATE) && \
	(!defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Deflate client requires client, compressed connection and negotiation"
#endif









#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

/*
	Protocols 和 Http 内的借用对象只需覆盖 Connect 调用。
	内部请求、协议列表、连接配置和事件表都会在返回前取得独立快照。
*/
typedef struct xwsclientconfig {
	xwsconnconfig Connection;
	xhttpcalloptions Http;
	xstrview Protocols;
	xwsdeflate Deflate;
	bool EnableDeflate;
	bool RequireDeflate;

} xwsclientconfig;



/* 客户端握手快照借用 Response 的子协议，并拥有扩展协商参数。 */
typedef struct xwsclienthandshake {
	xstrview Protocol;
	xwsdeflate Deflate;
	bool DeflateEnabled;

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






#if defined(XWS_CLIENT_FEATURE_WEBSOCKET_CLIENT)

/* 在完整、允许未对齐的存储中初始化客户端默认配置。 */
XWS_CLIENT_API void xrtWsClientConfigInit(
	xwsclientconfig* pConfig
);



/*
	创建可继续添加自定义 Header 的基础 GET 请求。
	Url 接受 ws、wss、http 或 https，并映射为 HTTP Client 可执行的 URL；
	Url 不得包含 userinfo 或 fragment；该函数不写入任何 WebSocket 握手管理字段。
*/
XWS_CLIENT_API xhttprequest* xrtWsRequestCreate(
	xstrview Url
);



/*
	创建 GET WebSocket 请求；Url 接受 ws、wss、http 或 https。
	Url 不得包含 userinfo 或 fragment；资源名中的井号必须编码为 %23。
	配置先复制为局部快照；成功时 Key 写入完整的固定输出。
*/
XWS_CLIENT_API xhttprequest* xrtWsClientRequestCreate(
	xstrview Url,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	克隆自定义 GET 请求并原子替换 WebSocket 管理字段。
	配置先复制为局部快照；Key 必须覆盖完整固定输出；
	源请求不得包含 userinfo、fragment、正文、分帧字段或扩展字段。
*/
XWS_CLIENT_API xhttprequest* xrtWsClientRequestClone(
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	char sKey[XWS_KEY_CAPACITY]
);



/*
	严格验证 101 响应、Accept、所选子协议和扩展响应。
	配置和输出允许未对齐，输出不得覆盖 Response、Key 或配置；
	Handshake 的子协议借用 Response，失败时保持不变。
*/
XWS_CLIENT_API bool xrtWsClientCheck(
	const xhttpresponse* pResponse,
	xstrview Key,
	const xwsclientconfig* pConfig,
	xwsclienthandshake* pHandshake
);



/*
	快照配置和事件表后，使用 ws/wss URL 异步执行 WebSocket 握手。
	EventData 属于 Connection 事件，Data 属于完成回调，生命周期彼此独立。
*/
XWS_CLIENT_API xhttpcall* xrtWsConnect(
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
XWS_CLIENT_API xhttpcall* xrtWsConnectRequest(
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
