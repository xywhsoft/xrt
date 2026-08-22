#ifndef XRT_INTERNAL_HTTP_SERVER_RUNTIME_H
#define XRT_INTERNAL_HTTP_SERVER_RUNTIME_H

#include "xrt_http_server.h"
#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
	#include <xrt/http_server_tls.h>
#endif
#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
	#include <xrt/future.h>
	#include <xrt/http_server_response_async.h>
#endif
#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
	#include <xrt/http_server_upgrade.h>
#endif
#include <xrt/http_server_runtime.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER)

#define XRT_HTTP_SERVER_TIMER_NONE UINT32_C(0)
#define XRT_HTTP_SERVER_TIMER_HEADER UINT32_C(1)
#define XRT_HTTP_SERVER_TIMER_BODY UINT32_C(2)
#define XRT_HTTP_SERVER_TIMER_REQUEST UINT32_C(3)
#define XRT_HTTP_SERVER_TIMER_IDLE UINT32_C(4)
#define XRT_HTTP_SERVER_TIMER_WRITE UINT32_C(5)

#define XRT_HTTP_CONN_TRANSPORT_TCP UINT32_C(0)
#define XRT_HTTP_CONN_TRANSPORT_TLS UINT32_C(1)

#define XRT_HTTP_CONN_GATE_OPEN UINT32_C(0)
#define XRT_HTTP_CONN_GATE_CLOSING UINT32_C(1)
#define XRT_HTTP_CONN_GATE_UPGRADED UINT32_C(2)



typedef struct __xrt_http_response_queue {
	xhttp1serverresponse* Response;
	struct __xrt_http_response_queue* Next;
	bool Information;
} __xrt_http_response_queue;



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
typedef struct __xrt_http_conn_future __xrt_http_conn_future;
typedef struct __xrt_http_server_wait __xrt_http_server_wait;
#endif



/* Server 持有聚合 TCP Server 和全部尚未退出 HTTP 所有权的连接。 */
struct xhttpserver {
	volatile int32 References;
	xatomic32 State;
	xatomic64 Accepted;
	xatomic64 Rejected;
	xatomic64 Requests;
	xatomic64 Responses;
	xatomic64 Informations;
	xatomic64 Upgraded;
	xatomic64 ProtocolErrors;
	xatomic64 Timeouts;
	/* 公开活跃连接数与尚未退出的回调生命周期分别计数。 */
	xatomic64 Connections;
	xatomic64 RuntimeConnections;
	xatomic64 PeakConnections;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		xatomic64 UpgradeCallbacks;
	#endif
	xatomic32 NetworkClosed;
	xatomic32 NetworkPublished;
	xatomicptr NetworkError;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		xatomic32 ShutdownPublished;
	#endif
	xrt_spinlock Lock;
	xnetengine* Engine;
	xnetserver* Network;
	xnetaddr* Locals;
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpconn* Head;
	xhttpconn* Tail;
	size_t EndpointCount;
	size_t ListenerCount;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrt_http_server_wait* WaitHead;
		__xrt_http_server_wait* WaitTail;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		xtlsserverconfig Tls;
		xtlsstreamconfig TlsStream;
		xtlscontext* TlsContext;
		xtlsidentity* TlsIdentity;
		xstrview* TlsProtocols;
	#endif
	bool RuntimeHeld;
	bool Secure;
};



/* Connection 的协议与 I/O 可变状态只在所属 Worker 上推进。 */
struct xhttpconn {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CloseGate;
	xatomic32 FinalGate;
	xatomic32 RequestActive;
	xatomic32 RequestBodyPaused;
	xatomic32 RequestBodyResumePosted;
	xatomic64 Requests;
	xatomic64 RequestWireBytes;
	xatomic64 ResponseWireBytes;
	xatomicptr Error;
	xrt_spinlock TransportLock;
	ptr AdapterData;
	void (*AdapterRelease)(ptr pData);
	xhttpserver* Server;
	xnetworker* Worker;
	xatomicptr Transport;
	xhttp1serverexchange* Exchange;
	xnetpost RequestBodyResumePost;
	xhttp1serverresponse* Response;
	__xrt_http_response_queue* ResponseHead;
	__xrt_http_response_queue* ResponseTail;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrt_http_conn_future* Future;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
		xfuturewatch BodyWatch;
		xfuture* BodyFuture;
		bool BodyWaiting;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		xnetpost UpgradePost;
		xhttpupgradeproc Upgrade;
		ptr UpgradeData;
		ptr UpgradeTransport;
		xerror* UpgradeError;
		xnetresult UpgradeResult;
		size_t UpgradeBuffered;
		bool UpgradePosted;
	#endif
	xnetaddr Local;
	xnetaddr Remote;
	size_t Endpoint;
	xhttpconn* Previous;
	xhttpconn* Next;
	uint64 Timer;
	uint64 WriteDeadline;
	uint32 TimerKind;
	uint32 TransportKind;
	size_t Offered;
	size_t WritePending;
	size_t InformationCount;
	bool OutputQueued;
	bool OutputDriving;
	bool OutputPending;
	bool OutputPosted;
	bool OutputDraining;
	bool ResponseInformation;
	bool AutoContinue;
	bool ForceClose;
	bool InputEnded;
	bool Listed;
	bool Counted;
	bool RuntimeHeld;
	bool CloseNotified;
	bool ErrorNotified;
	bool ServerCloseQueued;
	bool ServerAbortQueued;
};



/* 建立 Server 域错误并保留底层原因。 */
xerror* __xrtHttpServerErrorCreate(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 沿原因链选取最内层有效错误类别。 */
xerrkind __xrtHttpServerCauseKind(
	const xerror* pError,
	xerrkind Fallback
);



/* 设置当前线程的 Server 创建或同步调用错误。 */
void __xrtHttpServerSetError(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 保存 Connection 的第一个稳定错误并发布给当前线程。 */
void __xrtHttpConnRememberError(
	xhttpconn* pConnection,
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 把 Connection 加入 Server 生命周期列表。 */
void __xrtHttpServerAddConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
);



/* 从 Server 的可遍历列表移除 Connection，但暂不归还运行时计数。 */
void __xrtHttpServerRemoveConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
);



/* 在 Connection 的 HTTP 资源和运行时引用退出后归还运行时计数。 */
void __xrtHttpServerConnectionFinished(xhttpserver* pServer);



/* 在发布 Connection Close 前归还公开活跃连接计数。 */
void __xrtHttpServerConnectionClosed(xhttpserver* pServer);



/* 连接和聚合 TCP Server 都退出后发布唯一 Server 关闭终态。 */
void __xrtHttpServerTryFinish(xhttpserver* pServer);



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

/* 摘除并完成 Server 的全部关闭等待。 */
void __xrtHttpServerFutureFinish(xhttpserver* pServer);

#endif



/* 验证配置并创建尚未开始监听的 Server。 */
xhttpserver* __xrtHttpServerCreate(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
);



/* 启动 Server 的 TCP Listener；失败时销毁传入的调用方引用。 */
xhttpserver* __xrtHttpServerListen(xhttpserver* pServer);



/* 检查每个网络端点的 TCP 写入上限都不小于指定值。 */
bool __xrtHttpServerNetworkWriteLimit(
	const xhttpserverconfig* pConfig,
	size_t iMinimum
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 校验并保存 HTTPS Server 使用的 TLS 配置与引用。 */
bool __xrtHttpServerTlsSetup(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
);



/* 释放 HTTPS Server 保存的 TLS 配置与引用。 */
void __xrtHttpServerTlsCleanup(xhttpserver* pServer);

#endif



/* 在所属 Worker 上创建并接管一个已接受的明文 TCP Stream。 */
xhttpconn* __xrtHttpConnCreateTcp(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 在所属 Worker 上创建并接管一个已接受的 TLS Stream。 */
xhttpconn* __xrtHttpConnCreateTls(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
);

#endif



/* 取得 Connection 底层 TCP Stream 的稳定引用。 */
xnetstream* __xrtHttpConnTcpRef(xhttpconn* pConnection);



/* 在所属 Worker 上安装唯一顶层适配器上下文。 */
bool __xrtHttpConnAdapterSet(
	xhttpconn* pConnection,
	ptr pData,
	void (*pRelease)(ptr pData)
);



/* 在所属 Worker 上借用当前顶层适配器上下文。 */
ptr __xrtHttpConnAdapterData(xhttpconn* pConnection);



/* 在所属 Worker 上取走顶层适配器上下文并取消析构责任。 */
ptr __xrtHttpConnAdapterTake(xhttpconn* pConnection);



/* 借用 Connection 当前可读的明文块链。 */
const xnetbuf* __xrtHttpConnBuffer(xhttpconn* pConnection);



/* 消费 Connection 当前明文输入。 */
bool __xrtHttpConnConsume(
	xhttpconn* pConnection,
	size_t iSize
);



/* 暂停明文 TCP 输入；TLS 由明文所有权自动施加背压。 */
void __xrtHttpConnPauseInput(xhttpconn* pConnection);



/* 恢复明文 TCP 输入；TLS 在消费明文后自动恢复。 */
void __xrtHttpConnResumeInput(xhttpconn* pConnection);



/* 恢复协议 Exchange，并用空输入发布已经到达的正文终态。 */
bool __xrtHttpConnResumeExchange(xhttpconn* pConnection);



/* 串行消费 Stream 已经缓冲的输入。 */
void __xrtHttpConnDriveInput(xhttpconn* pConnection);



/* 串行输出当前信息或最终响应。 */
void __xrtHttpConnDriveOutput(xhttpconn* pConnection);



/*
	建立以 Body 作为完整线缆字节的响应计划。
	普通 Raw 与 Upgrade Raw 共用该唯一构造路径。
*/
xhttp1serverresponse* __xrtHttpConnWireResponse(
	xhttpbody* pBody,
	uint64 iLength,
	bool bClose,
	bool bTunnel
);



/* 建立顺序输出一组外部引用的完整线缆响应计划。 */
xhttp1serverresponse* __xrtHttpConnWireRefsResponse(
	const xnetref* pRefs,
	size_t iCount,
	uint64 iLength,
	bool bClose,
	bool bTunnel
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

/* 完成 Tunnel 输出并把传输异步转交给 Upgrade 回调。 */
void __xrtHttpConnUpgradeFinish(xhttpconn* pConnection);



/* 为尚未完成的 Upgrade 安排唯一失败回调。 */
bool __xrtHttpConnUpgradeFail(
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError
);

#endif



/* 检查当前 Worker、连接终态和活动请求是否允许提交响应。 */
bool __xrtHttpConnCanRespond(
	xhttpconn* pConnection,
	cstr sOperation,
	bool bRequireRequest
);



/*
	把已经拥有的响应计划提交给唯一最终响应门。
	无论成功失败，该函数都接管 Response；Upgrade 层可以显式允许 Tunnel 终态。
*/
xnetresult __xrtHttpConnCommitResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	cstr sOperation,
	bool bAllowTunnel
);



/* 发布 Connection 已经保存的稳定错误。 */
void __xrtHttpConnEmitError(xhttpconn* pConnection);



/* 发送自动 100 Continue 并在完成后恢复请求正文。 */
bool __xrtHttpConnContinue(xhttpconn* pConnection);



/* 发送协议错误并强制在响应后关闭。 */
void __xrtHttpConnProtocolFail(
	xhttpconn* pConnection,
	uint16 iStatus,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 为当前协议阶段安装唯一 Timer。 */
bool __xrtHttpConnArmTimer(
	xhttpconn* pConnection,
	uint32 iKind,
	uint64 iTimeout
);



/* 取消当前阶段 Timer；引用由最终 Timer 回调释放。 */
void __xrtHttpConnCancelTimer(xhttpconn* pConnection);



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

/* 解除当前响应 Future；关闭、超时和手动响应可以同时请求协作取消。 */
void __xrtHttpConnFutureDetach(
	xhttpconn* pConnection,
	bool bCancel
);

#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)

/* 订阅当前响应正文的可读性 Future。 */
bool __xrtHttpConnBodyWait(xhttpconn* pConnection);



/* 取消并摘除尚未完成的响应正文等待。 */
void __xrtHttpConnBodyStop(
	xhttpconn* pConnection,
	bool bCancel
);

#endif



/* 返回 HTTP Server 使用的 TCP Stream 事件表。 */
const xnetstreamevents* __xrtHttpConnStreamEvents(void);

#endif

#endif
