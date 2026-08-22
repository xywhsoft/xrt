#ifndef XRT_TCP_SERVER_H
#define XRT_TCP_SERVER_H

#include <xrt/tcp.h>

#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE) || \
	defined(XRT_FEATURE_NET_TCP_SERVER_SYNC)
	#include <xrt/future.h>
#endif



#if defined(XRT_FEATURE_NET_TCP_SERVER) && \
	!defined(XRT_FEATURE_NET_TCP)
	#error "XRT TCP Server support requires TCP support"
#endif

#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE) && \
	(!defined(XRT_FEATURE_NET_TCP_SERVER) || !defined(XRT_FEATURE_FUTURE))
	#error "XRT TCP Server Future support requires TCP Server and Future support"
#endif

#if defined(XRT_FEATURE_NET_TCP_SERVER_SYNC) && \
	(!defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE) || \
	 !defined(XRT_FEATURE_NET_SYNC))
	#error "XRT TCP Server sync support requires TCP Server Future and network sync support"
#endif



#if defined(XRT_FEATURE_NET_TCP_SERVER)

typedef struct xnetserver xnetserver;



/* Server 只在全部端点绑定成功后进入 OPEN，CLOSED 是唯一终态。 */
typedef enum xnetserverstate {
	XNET_SERVER_STARTING = 0,
	XNET_SERVER_OPEN,
	XNET_SERVER_CLOSING,
	XNET_SERVER_CLOSED
} xnetserverstate;



/* SHARED 每端点使用一个 Listener，REUSE_PORT 为每个 Worker 建立一份。 */
typedef enum xnetservermode {
	XNET_SERVER_SHARED = 0,
	XNET_SERVER_REUSE_PORT
} xnetservermode;



/*
	Server 回调收到逻辑端点索引，Accept 返回 true 后接管一个 Stream 引用。
	Close 在状态进入 CLOSED 后发布；只轮询状态不能代替等待 Close 通知。
*/
typedef struct xnetserverevents {
	bool (*Accept)(xnetserver* pServer, size_t iEndpoint,
		xnetstream* pStream, ptr pData);
	void (*Error)(xnetserver* pServer, size_t iEndpoint,
		const xerror* pError, ptr pData);
	void (*Close)(xnetserver* pServer, ptr pData);
} xnetserverevents;



/*
	Listen 是第零个端点，Additional 只在启动调用期间借用。
	SharedPort 让零端口继承整组首个实际端口，非零端口必须彼此一致。
	REUSE_PORT 模式固定本地分发、关闭独占绑定，并为每个 Engine Worker 建立 Listener。
*/
typedef struct xnetserverconfig {
	xnetlistenconfig Listen;
	const xnetlistenconfig* Additional;
	size_t AdditionalCount;
	uint32 AcceptQueueLimit;
	xnetservermode Mode;
	bool SharedPort;
} xnetserverconfig;



/* Server 统计聚合全部 Listener，并保留关闭后的累计值。 */
typedef struct xnetserverstats {
	xnetserverstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Errors;
	size_t Endpoints;
	size_t Listeners;
	size_t ClosedListeners;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xnetserverstats;



XRT_EXTERN_C_BEGIN



/* 初始化单 IPv4 动态端口、共享 Listener 和有界 Accept 队列。 */
XRT_API void xrtNetServerConfigInit(xnetserverconfig* pConfig);



/*
	同步绑定全部端点后启动异步 Accept；任一步失败都会关闭已建 Listener。
	pData 同时作为 Server 事件和已接受 Stream 事件的初始用户数据。
*/
XRT_API xnetserver* xrtNetServerStart(
	xnetengine* pEngine,
	const xnetserverconfig* pConfig,
	const xnetserverevents* pEvents,
	const xnetstreamevents* pStreamEvents,
	ptr pData
);



/* 线程安全地增加 Server 引用；无效计数或溢出时返回空。 */
XRT_API xnetserver* xrtNetServerRef(xnetserver* pServer);



/* 释放 Server 引用；不会隐式关闭仍在运行的 Server。 */
XRT_API void xrtNetServerDestroy(xnetserver* pServer);



/* 拉取模式下非阻塞取走一个已接受 Stream；空队列返回空指针。 */
XRT_API xnetstream* xrtNetServerAccept(xnetserver* pServer);



/* 原子停止全部 Listener，并丢弃尚未交给调用方的排队 Stream。 */
XRT_API bool xrtNetServerClose(xnetserver* pServer);



/* 返回 Server 当前生命周期状态。 */
XRT_API xnetserverstate xrtNetServerState(const xnetserver* pServer);



/* 返回配置中的逻辑端点数量。 */
XRT_API size_t xrtNetServerEndpointCount(const xnetserver* pServer);



/* 复制指定逻辑端点的实际地址，支持共享动态端口。 */
XRT_API bool xrtNetServerLocal(
	const xnetserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
);



/* 返回实际 Listener 数量；reuse-port 模式通常是端点数乘 Worker 数。 */
XRT_API size_t xrtNetServerListenerCount(const xnetserver* pServer);



/*
	增加并返回指定底层 Listener 引用，便于特殊场景继续使用低级能力。
	Listener 已经关闭时返回空指针，这不是调用错误。
*/
XRT_API xnetlistener* xrtNetServerListener(
	xnetserver* pServer,
	size_t iListener
);



/* 返回创建时保存的用户数据，不延长目标生命周期。 */
XRT_API ptr xrtNetServerData(const xnetserver* pServer);



/* 复制 Server 并发统计。 */
XRT_API bool xrtNetServerStats(
	const xnetserver* pServer,
	xnetserverstats* pStats
);



#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
/* 拉取模式下异步接受一个连接；成功值由 Future 持有一个 Stream 引用。 */
XRT_API xfuture* xrtNetServerAcceptAsync(xnetserver* pServer);
#endif



#if defined(XRT_FEATURE_NET_TCP_SERVER_SYNC)
/* 阻塞接受一个连接；禁止从任意 Engine Worker 调用。 */
XRT_API xnetstream* xrtNetServerAcceptWait(
	xnetserver* pServer,
	xdeadline iDeadline,
	xcancel* pCancel
);
#endif



XRT_EXTERN_C_END

#endif

#endif
