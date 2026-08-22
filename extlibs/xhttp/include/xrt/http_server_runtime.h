#ifndef XRT_HTTP_SERVER_RUNTIME_H
#define XRT_HTTP_SERVER_RUNTIME_H

#include <xrt/http_server_exchange.h>
#include <xrt/tcp_server.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_EXCHANGE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE) || \
	 !defined(XRT_FEATURE_NET_TCP_SERVER))
	#error "XRT HTTP server requires Exchange, response and TCP Server support"
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC))
	#error "XRT async HTTP server body support requires server and async response support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER)

typedef struct xhttpserver xhttpserver;
typedef struct xhttpconn xhttpconn;



/* Server 状态单向进入排空、终止和关闭终态。 */
typedef enum xhttpserverstate {
	XHTTP_SERVER_RUNNING = 0,
	XHTTP_SERVER_DRAINING,
	XHTTP_SERVER_ABORTING,
	XHTTP_SERVER_CLOSED
} xhttpserverstate;



/* Connection 状态描述当前可观察阶段，keep-alive 时可以重新进入 REQUEST。 */
typedef enum xhttpconnstate {
	XHTTP_CONN_REQUEST = 0,
	XHTTP_CONN_BODY,
	XHTTP_CONN_WAITING,
	XHTTP_CONN_INFORMATION,
	XHTTP_CONN_RESPONSE,
	XHTTP_CONN_UPGRADED,
	XHTTP_CONN_CLOSING,
	XHTTP_CONN_CLOSED
} xhttpconnstate;



/* 服务端错误按配置、监听、TLS、协议、响应、回调和超时来源稳定分类。 */
typedef enum xhttpservererror {
	XHTTP_SERVER_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_ERROR_CONFIG,
	XHTTP_SERVER_ERROR_LISTEN,
	XHTTP_SERVER_ERROR_TLS,
	XHTTP_SERVER_ERROR_CONNECTION,
	XHTTP_SERVER_ERROR_PROTOCOL,
	XHTTP_SERVER_ERROR_RESPONSE,
	XHTTP_SERVER_ERROR_CALLBACK,
	XHTTP_SERVER_ERROR_TIMEOUT_HEADER,
	XHTTP_SERVER_ERROR_TIMEOUT_BODY,
	XHTTP_SERVER_ERROR_TIMEOUT_REQUEST,
	XHTTP_SERVER_ERROR_TIMEOUT_IDLE,
	XHTTP_SERVER_ERROR_TIMEOUT_WRITE,
	XHTTP_SERVER_ERROR_UPGRADE,
	XHTTP_SERVER_ERROR_STATE,
	XHTTP_SERVER_ERROR_INTERNAL
} xhttpservererror;



/*
	全部超时使用微秒，零表示关闭对应保护。
	WriteSize 只限制单次零复制发送租约，不建立每连接固定缓冲。
	MaxConnections 为零时不限制，仍受系统和 Engine 硬边界约束。
*/
typedef struct xhttpserverconfig {
	xnetserverconfig Network;
	xhttp1serverconfig Http1;
	size_t WriteSize;
	uint64 HeaderTimeout;
	uint64 BodyTimeout;
	uint64 RequestTimeout;
	uint64 IdleTimeout;
	uint64 WriteTimeout;
	size_t MaxConnections;
	size_t MaxInformations;
} xhttpserverconfig;



/*
	Headers、Body 和 Request 在连接 Worker 上串行执行。
	Headers 为空时默认缓冲正文；Body 只在 STREAM 策略下调用。
	Body 可以暂停当前请求正文，异步消费者完成后从任意线程请求恢复。
	Headers 可直接提交最终响应：无正文请求正常完成并可继续 keep-alive，有正文请求
	停在 Header 边界并在响应排空后关闭。Body 提交最终响应后不再交付剩余正文，
	回调返回值不再替换已经固定的最终响应。
	Request 收到完整拥有型请求，响应可以在回调内同步提交或稍后提交。
*/
typedef struct xhttpserverevents {
	void (*Open)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		ptr pData
	);
	xhttpserverbodypolicy (*Headers)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		ptr pData
	);
	bool (*Body)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		xbytesview Data,
		ptr pData
	);
	void (*Request)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		ptr pData
	);
	void (*Close)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
	void (*Error)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xerror* pError,
		ptr pData
	);
	/* 全部 HTTP Connection 运行时和受理的 Upgrade 回调退出后发布。 */
	void (*Shutdown)(xhttpserver* pServer, ptr pData);
	ptr Data;
} xhttpserverevents;



/* Server 统计全部是无锁累计快照；Close 回调前活跃连接数已经退账。 */
typedef struct xhttpserverstats {
	xhttpserverstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Requests;
	uint64 Responses;
	uint64 Informations;
	uint64 Upgraded;
	uint64 ProtocolErrors;
	uint64 Timeouts;
	size_t Connections;
	size_t PeakConnections;
	size_t Endpoints;
	size_t Listeners;
	bool Secure;
} xhttpserverstats;



/* Connection 统计组合 HTTP 和 TCP/TLS 传输事实，不借出内部缓冲。 */
typedef struct xhttpconnstats {
	xhttpconnstate State;
	uint64 Requests;
	uint64 RequestWireBytes;
	uint64 ResponseWireBytes;
	size_t BufferedBytes;
	size_t QueuedBytes;
	bool RequestBodyPaused;
	bool RequestActive;
	bool FinalCommitted;
	bool Secure;
} xhttpconnstats;



XRT_EXTERN_C_BEGIN



/*
	初始化安全公网限额、TCP 背压和分阶段超时默认值。
	配置允许位于未对齐存储，Server 启动时会先复制完整快照。
*/
XRT_API void xrtHttpServerConfigInit(xhttpserverconfig* pConfig);



/*
	初始化全部为空的服务端事件表。
	事件表允许位于未对齐存储，Server 启动时会先复制完整快照。
*/
XRT_API void xrtHttpServerEventsInit(xhttpserverevents* pEvents);



/* 创建并立即启动一个明文 HTTP/1 TCP Server。 */
XRT_API xhttpserver* xrtHttpServerStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
);



/* 增加 Server 引用并返回原指针。 */
XRT_API xhttpserver* xrtHttpServerRef(xhttpserver* pServer);



/* 释放 Server 引用；不会隐式停止仍在运行的 Server。 */
XRT_API void xrtHttpServerDestroy(xhttpserver* pServer);



/* 停止接受新连接，关闭空闲连接并让活动请求发送最后一个响应。 */
XRT_API bool xrtHttpServerDrain(xhttpserver* pServer);



/* 停止接受并异常关闭全部 HTTP 连接。 */
XRT_API bool xrtHttpServerAbort(xhttpserver* pServer);



/* 返回 Server 当前生命周期状态。 */
XRT_API xhttpserverstate xrtHttpServerState(const xhttpserver* pServer);



/* 返回 Server 是否使用 TLS 传输。 */
XRT_API bool xrtHttpServerSecure(const xhttpserver* pServer);



/* 返回 HTTP Server 的逻辑监听端点数量。 */
XRT_API size_t xrtHttpServerEndpointCount(
	const xhttpserver* pServer
);



/* 复制指定逻辑端点的实际监听地址；输出可未对齐，但不得覆盖 Server。 */
XRT_API bool xrtHttpServerLocal(
	const xhttpserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
);



/* 返回 HTTP Server 当前拥有的实际底层 Listener 数量。 */
XRT_API size_t xrtHttpServerListenerCount(
	const xhttpserver* pServer
);



/* 增加并返回底层 TCP Server 引用，供特殊场景继续使用网络层能力。 */
XRT_API xnetserver* xrtHttpServerNetwork(xhttpserver* pServer);



/* 返回导致网络 Server 终止的借用结构化错误。 */
XRT_API const xerror* xrtHttpServerError(
	const xhttpserver* pServer
);



/* 复制 Server 并发统计；输出可未对齐，但不得覆盖 Server。 */
XRT_API bool xrtHttpServerStats(
	const xhttpserver* pServer,
	xhttpserverstats* pStats
);



/* 增加 Connection 引用并返回原指针。 */
XRT_API xhttpconn* xrtHttpConnRef(xhttpconn* pConnection);



/* 释放 Connection 引用；空指针视为空操作。 */
XRT_API void xrtHttpConnDestroy(xhttpconn* pConnection);



/* 返回 Connection 当前协议阶段。 */
XRT_API xhttpconnstate xrtHttpConnState(
	const xhttpconn* pConnection
);



/* 返回借用的所属 Server。 */
XRT_API xhttpserver* xrtHttpConnServer(
	const xhttpconn* pConnection
);



/* 返回当前请求的借用指针；没有活动请求时为空。 */
XRT_API const xhttpserverrequest* xrtHttpConnRequest(
	const xhttpconn* pConnection
);



/* 返回 Connection 所属的借用 Worker。 */
XRT_API xnetworker* xrtHttpConnWorker(
	const xhttpconn* pConnection
);



/* 只在 Connection Worker 回调内返回底层借用 TCP Stream，包括 TLS 的传输。 */
XRT_API xnetstream* xrtHttpConnTcp(xhttpconn* pConnection);



/* 返回 Connection 是否使用 TLS 传输。 */
XRT_API bool xrtHttpConnSecure(const xhttpconn* pConnection);



/* 返回接受当前 Connection 的逻辑 Server 端点索引。 */
XRT_API size_t xrtHttpConnEndpoint(const xhttpconn* pConnection);



/* 复制连接本地地址；输出可未对齐、成功才修改，且不得覆盖 Connection。 */
XRT_API bool xrtHttpConnLocal(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
);



/* 复制连接远端地址；输出可未对齐、成功才修改，且不得覆盖 Connection。 */
XRT_API bool xrtHttpConnRemote(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
);



/* 返回导致连接关闭或协议终止的借用结构化错误。 */
XRT_API const xerror* xrtHttpConnError(
	const xhttpconn* pConnection
);



/* 复制 Connection 协议和传输统计；输出可未对齐，但不得覆盖 Connection。 */
XRT_API bool xrtHttpConnStats(
	const xhttpconn* pConnection,
	xhttpconnstats* pStats
);



/*
	在 Connection Worker 的 Headers 回调中替换当前请求正文硬上限。
	零表示不接受非空正文，UINT64_MAX 表示不增加应用层总量上限。
*/
XRT_API bool xrtHttpConnSetRequestBodyLimit(
	xhttpconn* pConnection,
	uint64 iMaxBody
);



/*
	在 Connection Worker 的流式 Body 回调中暂停后续请求正文交付。
	当前回调片段视为已经接受；正文空闲超时在暂停期间继续生效。
*/
XRT_API bool xrtHttpConnPauseRequestBody(
	xhttpconn* pConnection
);



/*
	从任意线程恢复请求正文交付，并让所属 Worker 处理已经缓冲的输入。
	未暂停时视为成功的空操作；关闭或升级后的连接返回 false。
*/
XRT_API bool xrtHttpConnResumeRequestBody(
	xhttpconn* pConnection
);



/* 返回请求正文是否被应用暂停的并发快照。 */
XRT_API bool xrtHttpConnRequestBodyPaused(
	const xhttpconn* pConnection
);



/*
	在 Connection Worker 上提交一条信息响应。
	发送完成后继续当前请求，不能代替最终响应。
*/
XRT_API xnetresult xrtHttpConnInform(
	xhttpconn* pConnection,
	const xhttpreply* pReply
);



/* 在 Connection Worker 上提交唯一最终 Reply。 */
XRT_API xnetresult xrtHttpConnRespond(
	xhttpconn* pConnection,
	const xhttpreply* pReply
);



/* 一次调用复制常用固定正文并提交最终响应。 */
XRT_API xnetresult xrtHttpConnReply(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body
);



/*
	一次调用保留可选 Body 引用并提交最终响应。
	调用返回后可以立即销毁调用方引用；未知长度正文按请求版本选择 chunked 或关闭分帧。
*/
XRT_API xnetresult xrtHttpConnReplyBody(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody
);



/* 从任意线程停止读取并在已经受理的响应字节排空后关闭。 */
XRT_API bool xrtHttpConnClose(xhttpconn* pConnection);



/* 从任意线程丢弃发送队列并异常关闭。 */
XRT_API bool xrtHttpConnAbort(xhttpconn* pConnection);



XRT_EXTERN_C_END

#endif

#endif
