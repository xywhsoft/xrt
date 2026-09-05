#ifndef XRT_TLS_STREAM_H
#define XRT_TLS_STREAM_H

#include <xrt/tcp.h>
#include <xrt/tls_client.h>
#include <xrt/tls_server.h>

#if defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
	defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
	defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE) || \
	defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)
	#include <xrt/future.h>
#endif



#if defined(XRT_FEATURE_TLS_STREAM) && \
	(!defined(XRT_FEATURE_NET_TCP) || \
	 !defined(XRT_FEATURE_TLS_CLIENT) || \
	 !defined(XRT_FEATURE_TLS_SERVER))
	#error "XRT_FEATURE_TLS_STREAM requires TCP, TLS client and TLS server"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_DIAL) && \
	(!defined(XRT_FEATURE_TLS_STREAM) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL))
	#error "XRT_FEATURE_TLS_STREAM_DIAL requires TLS Stream and TCP Dial"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_FUTURE) && \
	(!defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_FUTURE))
	#error "XRT TLS Stream Future support requires TLS Stream and Future"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) && \
	(!defined(XRT_FEATURE_TLS_STREAM_DIAL) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT TLS Stream Dial Future support requires TLS Stream Dial, Future and Future bridge"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_LISTENER) && \
	(!defined(XRT_FEATURE_TLS_STREAM) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XRT TLS Stream Listener support requires TLS Stream and TCP"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE) && \
	(!defined(XRT_FEATURE_TLS_STREAM_LISTENER) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "XRT TLS Stream Listener Future support requires Listener and Future"
#endif

#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC) && \
	!defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
	#error "XRT TLS Stream Listener sync support requires Listener Future"
#endif



/* 公开句柄声明不随 TLS Stream 实现裁剪变化。 */
typedef struct xtlsstream xtlsstream;

/*
	两个超时都使用微秒；零值显式关闭对应计时器。
	AsyncBytesLimit 和 AsyncCountLimit 是未完成操作的独立硬边界，
	AsyncBatch 限制一次 Worker 轮转完成的操作数。
*/
typedef struct xtlsstreamconfig {
	uint64 HandshakeTimeout;
	uint64 CloseTimeout;
	size_t AsyncBytesLimit;
	uint32 AsyncCountLimit;
	uint32 AsyncBatch;
} xtlsstreamconfig;



#if defined(XRT_FEATURE_TLS_STREAM)

#define XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT UINT64_C(10000000)
#define XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT UINT64_C(5000000)

#define XTLS_STREAM_ASYNC_BYTES_DEFAULT ((size_t)1048576u)
#define XTLS_STREAM_ASYNC_COUNT_DEFAULT UINT32_C(1024)
#define XTLS_STREAM_ASYNC_BATCH_DEFAULT UINT32_C(64)



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
typedef struct xtlslistener xtlslistener;
#endif

#if defined(XRT_FEATURE_TLS_STREAM_DIAL)
typedef struct xtlsdial xtlsdial;
#endif



/* FAILED 保存 TLS 或传输根因；CLOSED 只表示完成认证关闭。 */
typedef enum xtlsstreamstate {
	XTLS_STREAM_CONNECTING = 0,
	XTLS_STREAM_HANDSHAKE,
	XTLS_STREAM_OPEN,
	XTLS_STREAM_CLOSING,
	XTLS_STREAM_CLOSED,
	XTLS_STREAM_FAILED
} xtlsstreamstate;



#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
/*
	条件 Future 是水平条件；END 表示收到认证 close_notify，
	CLOSE 表示底层传输和 TLS 组合对象进入最终终态。
*/
typedef enum xtlsstreamwait {
	XTLS_STREAM_WAIT_OPEN = 0,
	XTLS_STREAM_WAIT_READ,
	XTLS_STREAM_WAIT_WRITE,
	XTLS_STREAM_WAIT_DRAIN,
	XTLS_STREAM_WAIT_END,
	XTLS_STREAM_WAIT_CLOSE
} xtlsstreamwait;
#endif



#if defined(XRT_FEATURE_TLS_STREAM_DIAL)
/* Dial 状态区分名称解析、TCP 连接和 TLS 握手三个可取消阶段。 */
typedef enum xtlsdialstate {
	XTLS_DIAL_RESOLVING = 0,
	XTLS_DIAL_CONNECTING,
	XTLS_DIAL_HANDSHAKE,
	XTLS_DIAL_CONNECTED,
	XTLS_DIAL_FAILED,
	XTLS_DIAL_CANCELLED
} xtlsdialstate;



/* Timeout 覆盖 DNS、TCP 和 TLS 全过程；零值只保留各阶段超时。 */
typedef struct xtlsdialconfig {
	xnetdialconfig Transport;
	xtlsstreamconfig Stream;
	uint64 Timeout;
	bool ServerNameFromHost;
} xtlsdialconfig;



/* 成功回调接管 TLS Stream 引用；失败时 Stream 为空且 Error 只在回调期间借用。 */
typedef void (*xtlsdialproc)(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
);
#endif



/* 全部回调都在底层 TCP Stream 所属 Worker 上串行执行。 */
typedef struct xtlsstreamevents {
	void (*Open)(xtlsstream* pStream, ptr pData);
	void (*Read)(xtlsstream* pStream,
		const xnetbuf* pBuffer, ptr pData);
	void (*End)(xtlsstream* pStream, ptr pData);
	void (*Writable)(xtlsstream* pStream, ptr pData);
	void (*Drain)(xtlsstream* pStream, ptr pData);
	void (*Close)(xtlsstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
	/*
		客户端恢复队列新增票据时发布边沿；未启用恢复实现时不会调用。
		回调使用 xrtTlsClientTakeResume 接管一张或全部票据。
	*/
	void (*Ticket)(xtlsstream* pStream, ptr pData);
} xtlsstreamevents;



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)

/* Listener 只发布已经完成 TLS 握手的 Stream，关闭监听不会隐式关闭已发布连接。 */
typedef enum xtlslistenerstate {
	XTLS_LISTENER_OPEN = 0,
	XTLS_LISTENER_CLOSING,
	XTLS_LISTENER_CLOSED
} xtlslistenerstate;



/*
	Accept 在目标 Stream 的 Worker 上执行，返回 true 后接管一个 Stream 引用。
	Error 只报告监听层错误；单连接握手失败通过 HandshakeError 独立报告。
*/
typedef struct xtlslistenerevents {
	bool (*Accept)(xtlslistener* pListener,
		xtlsstream* pStream, ptr pData);
	void (*HandshakeError)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Error)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xtlslistener* pListener, ptr pData);
} xtlslistenerevents;



/*
	Listen 负责 TCP 接入，Tls 和 Stream 负责每条连接的 TLS 会话与组合层限制。
	AcceptQueueLimit 只限制完成握手但尚未被 pull/Future 消费的连接；
	HandshakeLimit 在分配 TLS 会话前硬性限制并发握手数。
	初始化默认完成队列 1024 条、并发握手 128 条，均可显式调整。
*/
typedef struct xtlslistenerconfig {
	xnetlistenconfig Listen;
	xtlsserverconfig Tls;
	xtlsstreamconfig Stream;
	uint32 AcceptQueueLimit;
	uint32 HandshakeLimit;
} xtlslistenerconfig;



/* 统计值均为并发快照，累计计数在关闭后仍可读取。 */
typedef struct xtlslistenerstats {
	xtlslistenerstate State;
	uint64 Handshakes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 HandshakeErrors;
	uint32 ActiveHandshakes;
	uint32 PeakHandshakes;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xtlslistenerstats;

#endif



XRT_EXTERN_C_BEGIN



/* 初始化握手与认证关闭超时。 */
XRT_API void xrtTlsStreamConfigInit(xtlsstreamconfig* pConfig);



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
/* 初始化单 IPv4 动态端口、有界握手与有界完成队列。 */
XRT_API void xrtTlsListenerConfigInit(xtlslistenerconfig* pConfig);



/*
	同步完成 TCP 绑定并开始异步接入；配置数组只在调用期间借用。
	Listener 会保留 Context、Identity，并深复制 ALPN 协议列表。
	SelectContext 与 ResumeContext 由调用方持有，必须存活到 Listener 关闭回调结束。
*/
XRT_API xtlslistener* xrtTlsListenerStart(
	xnetengine* pEngine,
	const xtlslistenerconfig* pConfig,
	const xtlslistenerevents* pEvents,
	const xtlsstreamevents* pStreamEvents,
	ptr pData
);



/* 增加 Listener 引用并返回原指针。 */
XRT_API xtlslistener* xrtTlsListenerRef(xtlslistener* pListener);



/* 释放 Listener 引用；不会隐式关闭仍在监听的对象。 */
XRT_API void xrtTlsListenerDestroy(xtlslistener* pListener);



/* pull 模式下非阻塞取得一个已完成握手的 Stream；空队列返回空指针。 */
XRT_API xtlsstream* xrtTlsListenerAccept(xtlslistener* pListener);



/* 原子停止接入并丢弃尚未交付的连接；已交付连接保持独立生命周期。 */
XRT_API bool xrtTlsListenerClose(xtlslistener* pListener);



/* 返回 Listener 当前生命周期状态。 */
XRT_API xtlslistenerstate xrtTlsListenerState(
	const xtlslistener* pListener
);



/* 复制监听 Socket 的实际本地地址，支持动态端口。 */
XRT_API bool xrtTlsListenerLocal(
	xtlslistener* pListener,
	xnetaddr* pAddress
);



/* 返回创建时保存的用户数据快照。 */
XRT_API ptr xrtTlsListenerData(const xtlslistener* pListener);



/* 复制 Listener 的并发统计快照。 */
XRT_API bool xrtTlsListenerStats(
	const xtlslistener* pListener,
	xtlslistenerstats* pStats
);
#endif



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
/* pull 模式下异步接受一个已完成握手的 Stream；Future 持有结果引用。 */
XRT_API xfuture* xrtTlsListenerAcceptAsync(xtlslistener* pListener);
#endif



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)
/* 阻塞接受一个已完成握手的 Stream；禁止从该 Engine 的 Worker 调用。 */
XRT_API xtlsstream* xrtTlsListenerAcceptWait(
	xtlslistener* pListener,
	xdeadline iDeadline,
	xcancel* pCancel
);
#endif



#if defined(XRT_FEATURE_TLS_STREAM_DIAL)
/* 初始化 TCP 拨号、TLS Stream 和总超时策略。 */
XRT_API void xrtTlsDialConfigInit(xtlsdialconfig* pConfig);



/* 解析主机、竞争 TCP 地址并完成 TLS 握手；成功 Stream 引用转移给完成回调。 */
XRT_API xtlsdial* xrtTlsDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData,
	xtlsdialproc pDone,
	ptr pDoneData
);



#if defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
/*
	以 Future 接收完成握手的 TLS Stream；Open 先于成功终态发布。
	Future 持有一个 Stream 引用，取消请求协作终止 DNS、TCP 或 TLS 当前阶段。
*/
XRT_API xfuture* xrtTlsDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData
);
#endif



/* 增加 TLS Dial 引用并返回原指针。 */
XRT_API xtlsdial* xrtTlsDialRef(xtlsdial* pDial);



/* 释放 TLS Dial 引用；空指针视为空操作。 */
XRT_API void xrtTlsDialDestroy(xtlsdial* pDial);



/* 原子受理取消；返回真保证最终结果不会再变为成功。 */
XRT_API bool xrtTlsDialCancel(xtlsdial* pDial);



/* 返回当前拨号阶段或不可变终态。 */
XRT_API xtlsdialstate xrtTlsDialState(const xtlsdial* pDial);



/* 失败或取消后借用完整错误原因链。 */
XRT_API const xerror* xrtTlsDialError(const xtlsdial* pDial);



/* 取得底层 TCP Dial 统计；TLS 握手阶段仍保留获胜地址信息。 */
XRT_API bool xrtTlsDialTransportStats(
	const xtlsdial* pDial,
	xnetdialstats* pStats
);
#endif



/* 创建 TLS 客户端并异步连接数字 TCP 地址。 */
XRT_API xtlsstream* xrtTlsStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData
);



/*
	在已公开的 TCP Stream 所属 Worker 上接管 Transport 和 Session。
	Transport 必须仍可双向收发，调用方必须停止直接操作其 IO。
	成功时接管两者的调用方引用；失败时所有权、Session 分配归属和
	Transport 事件均保持不变，输出清空。
*/
XRT_API bool xrtTlsStreamAttach(
	xnetstream* pTransport,
	xtlssession* pSession,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);



/*
	在已连接 TCP Stream 上创建 TLS 客户端。
	适用于代理隧道、STARTTLS 和自定义拨号；成功时接管 Transport 引用。
*/
XRT_API bool xrtTlsStreamClient(
	xnetstream* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);



/* 在 TCP Accept 回调内接管 Stream；返回值应直接作为该回调结果。 */
XRT_API bool xrtTlsStreamAccept(
	xnetstream* pTransport,
	const xtlsserverconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
);



/* 增加 TLS Stream 引用并返回原指针；引用耗尽时返回空并设置状态错误。 */
XRT_API xtlsstream* xrtTlsStreamRef(xtlsstream* pStream);



/* 释放 TLS Stream 引用；关闭必须另行请求。 */
XRT_API void xrtTlsStreamDestroy(xtlsstream* pStream);



/*
	在所属 Worker 上替换已打开 TLS Stream 的事件与用户数据。
	不会自动重放当前明文缓冲，协议升级层必须显式处理已有后缀。
*/
XRT_API bool xrtTlsStreamSetEvents(
	xtlsstream* pStream,
	const xtlsstreamevents* pEvents,
	ptr pData
);



/* 在所属 Worker 上把明文编码为记录；允许成功短写。 */
XRT_API xtlsresult xrtTlsStreamSend(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize,
	size_t* pWritten
);



/* 在所属 Worker 上依次编码明文片段；返回跨片段的连续受理前缀。 */
XRT_API xtlsresult xrtTlsStreamSendVec(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pWritten
);



/*
	在所属 Worker 上返回一次明文发送产生的精确密文线路字节数。
	结果包含记录头、显式 nonce、内层类型和认证标签，失败不修改 pBound。
	pBound 不得与 Stream 或其 Session 对象存储重叠。
*/
XRT_API bool xrtTlsStreamSendBound(
	xtlsstream* pStream,
	size_t iPlainSize,
	size_t* pBound
);



/* 返回当前待应用消费明文字节数的并发快照。 */
XRT_API size_t xrtTlsStreamAvailable(const xtlsstream* pStream);



/* 返回 TLS 密文暂存与底层 TCP 队列的总待发字节并发快照。 */
XRT_API size_t xrtTlsStreamPending(const xtlsstream* pStream);



/*
	在所属 Worker 上借用明文块链，借用期不超过本次回调。
	默认在当前明文消费前暂停底层读取；增量协议解析器可显式请求 ReadMore。
*/
XRT_API const xnetbuf* xrtTlsStreamBuffer(xtlsstream* pStream);



/*
	在所属 Worker 上把精确明文前缀按需连续化并返回借用视图。
	不消费明文；视图在下一次明文缓冲修改或消费前有效，零长度和越界请求失败。
*/
XRT_API bool xrtTlsStreamPullup(
	xtlsstream* pStream,
	size_t iSize,
	xnetspan* pSpan
);



/*
	在 Read 回调保留现有明文时，请求继续解密并在明文增长后再次发布 Read。
	累积量受 Context PlainLimit 硬约束，并必须为一条最大明文 record 留出空间。
	普通消费者无需调用，重复请求是幂等的；请求待完成时不能替换事件接收者。
*/
XRT_API bool xrtTlsStreamReadMore(xtlsstream* pStream);



/* 在所属 Worker 上复制并安全消费明文。 */
XRT_API xtlsresult xrtTlsStreamRead(
	xtlsstream* pStream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
);



/* 在所属 Worker 上安全消费精确数量的明文。 */
XRT_API bool xrtTlsStreamConsume(xtlsstream* pStream, size_t iSize);



/*
	从任意线程请求 close_notify、等待对端认证关闭并排空 TCP。
	调用前已接纳的异步发送会先按 FIFO 完成；调用后的新发送不再接纳。
*/
XRT_API bool xrtTlsStreamClose(xtlsstream* pStream);



/*
	从任意线程立即放弃 TLS 与 TCP 会话。
	失败收尾尚未完成时仍会中止 TCP，但不会覆盖已经保存的首个根因。
*/
XRT_API bool xrtTlsStreamAbort(xtlsstream* pStream);



/* 返回组合 Stream 状态的并发快照。 */
XRT_API xtlsstreamstate xrtTlsStreamState(const xtlsstream* pStream);



/* 借用底层 TCP Stream，调用方不得改变其 IO 状态机。 */
XRT_API xnetstream* xrtTlsStreamTransport(const xtlsstream* pStream);



/* 在所属 Worker 上借用协议会话，供 ALPN、票据等高级查询。 */
XRT_API xtlssession* xrtTlsStreamSession(xtlsstream* pStream);



/* 返回线程安全的用户数据指针快照，不延长目标生命周期。 */
XRT_API ptr xrtTlsStreamData(const xtlsstream* pStream);



/* 终态失败时借用保存的 TLS 或传输根因。 */
XRT_API const xerror* xrtTlsStreamError(const xtlsstream* pStream);



#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
/* 返回尚未由所属 Worker 终结的异步发送负载字节数。 */
XRT_API size_t xrtTlsStreamAsyncBytes(const xtlsstream* pStream);



/* 返回异步发送、接收和条件等待的合计操作数。 */
XRT_API uint32 xrtTlsStreamAsyncCount(const xtlsstream* pStream);



/*
	建立 OPEN、READ、WRITE、DRAIN、END 或 CLOSE 条件 Future。
	WRITE 要求发送 FIFO 清空且当前至少可受理明文；DRAIN 还要求 TLS
	与 TCP 两级发送队列归零。END 在已认证明文全部交付后完成。
	取消只移除本次等待。
*/
XRT_API xfuture* xrtTlsStreamWaitAsync(
	xtlsstream* pStream,
	xtlsstreamwait Wait
);



/*
	在拉取模式下复制并消费当前可用明文。
	零上限表示读取全部当前明文；成功值是由 Future 持有的 xnetbytes。
*/
XRT_API xfuture* xrtTlsStreamRecvAsync(
	xtlsstream* pStream,
	size_t iMaxBytes
);



/*
	从任意线程复制并按 FIFO 提交一段完整明文。
	Future 在全部明文被 TLS 会话受理时完成；排空必须另行等待 DRAIN。
	取消只在首个字节受理前有效，已开始的发送保持完整和有序。
	Close 线性化前已接纳的发送保证先完成，之后的发送以 STATE 拒绝。
*/
XRT_API xfuture* xrtTlsStreamSendAsync(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize
);



/*
	从任意线程复制片段并按 FIFO 提交为一段连续明文。
	全部片段在返回前完成校验和复制，失败不会发布部分操作。
*/
XRT_API xfuture* xrtTlsStreamSendVecAsync(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
);
#endif



XRT_EXTERN_C_END

#endif

#endif
