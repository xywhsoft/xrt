#ifndef XRT_INTERNAL_TCP_SERVER_H
#define XRT_INTERNAL_TCP_SERVER_H

#include "xrt_tcp.h"
#include <xrt/tcp_server.h>



#if defined(XRT_FEATURE_NET_TCP_SERVER)

typedef struct __xrt_net_server_child __xrt_net_server_child;

#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
typedef struct __xrt_net_server_wait __xrt_net_server_wait;
#endif



/* 每个 Child 在 Server 内存中保持稳定，允许 Listener 回调与启动线程并发。 */
struct __xrt_net_server_child {
	xnetserver* Server;
	xnetlistener* Listener;
	size_t Endpoint;
	bool Closed;
};



/* Server 只聚合 Listener，不重复持有 Socket 或实现平台 Accept。 */
struct xnetserver {
	volatile int32 References;
	xatomic32 State;
	xatomic64 Accepted;
	xatomic64 Rejected;
	xatomic64 Errors;
	xatomic64 ClosedListeners;
	xatomic32 QueuedAccepts;
	xatomic32 PeakQueuedAccepts;
	xatomic32 AcceptWaiters;
	xrt_spinlock Lock;
	xrt_spinlock AcceptLock;
	xnetengine* Engine;
	xnetserverevents Events;
	ptr Data;
	__xrt_net_server_child* Children;
	xnetaddr* Locals;
	xnetstream* AcceptHead;
	xnetstream* AcceptTail;
	xerror* StartupError;
	size_t EndpointCount;
	size_t ListenerCount;
	size_t StartedListeners;
	uint32 AcceptQueueLimit;
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		__xrt_net_server_wait* WaitHead;
		__xrt_net_server_wait* WaitTail;
	#endif
	bool StartDone;
	bool Published;
	bool RuntimeHeld;
};



/* 调用方持有 AcceptLock 时取走一个排队 Stream。 */
xnetstream* __xrtNetServerTakeQueued(xnetserver* pServer);



#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
/* 配对 Server 拉取队列与 Future，并关闭终止后的剩余等待。 */
void __xrtNetServerFutureNotify(xnetserver* pServer);



/* 让最早的 Server Future 直接接管一个已接受 Stream。 */
bool __xrtNetServerFutureAccept(
	xnetserver* pServer,
	xnetstream* pStream
);
#endif

#endif

#endif
