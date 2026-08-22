#ifndef XRT_INTERNAL_UDP_H
#define XRT_INTERNAL_UDP_H

#include "xrt_net_engine.h"
#include <xrt/udp.h>



#if defined(XRT_FEATURE_NET_UDP)

#define XRT_NET_UDP_CONTROL_START 0x00000001u
#define XRT_NET_UDP_CONTROL_CLOSE 0x00000002u
#define XRT_NET_UDP_CONTROL_ABORT 0x00000004u
#define XRT_NET_UDP_CONTROL_FINISH 0x00000008u
#define XRT_NET_UDP_CONTROL_LIFECYCLE \
	(XRT_NET_UDP_CONTROL_CLOSE | XRT_NET_UDP_CONTROL_ABORT)
#define XRT_NET_UDP_CONTROL_POSTED 0x80000000u

typedef struct __xrt_net_udp_send __xrt_net_udp_send;
typedef struct __xrt_net_udp_send_slot __xrt_net_udp_send_slot;
typedef struct __xrt_net_udp_receive __xrt_net_udp_receive;
typedef struct __xrt_net_udp_error_receive __xrt_net_udp_error_receive;
typedef struct __xrt_net_udp_error_state __xrt_net_udp_error_state;
typedef struct __xrt_net_udp_config __xrt_net_udp_config;
typedef struct __xrt_net_udp_events __xrt_net_udp_events;

#if defined(XRT_FEATURE_NET_UDP_FUTURE)
typedef struct __xrt_net_udp_wait __xrt_net_udp_wait;
#endif



/* 对象只保留收发热路径需要的基础配置，扩展配置进入对应可选状态。 */
struct __xrt_net_udp_config {
	size_t ReceiveSize;
	uint32 ReceiveConcurrency;
	uint32 ReceiveBatch;
	uint32 ReceiveMeta;
	size_t ReceiveQueueLimit;
	size_t ReceiveQueueByteLimit;
	xnetudpoverflow Overflow;
	xnetudptruncation Truncation;
	size_t SendHighWater;
	size_t SendLowWater;
	size_t SendLimit;
	size_t SendPacketLimit;
	uint32 SendConcurrency;
};



/* 基础事件保持固定尺寸，可选协议错误回调跟随错误状态分配。 */
struct __xrt_net_udp_events {
	void (*Open)(xnetudp* pUdp, ptr pData);
	void (*Receive)(xnetudp* pUdp,
		const xnetudpmessage* pMessage, ptr pData);
	void (*Error)(xnetudp* pUdp, const xerror* pError, ptr pData);
	void (*HighWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*LowWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*Drain)(xnetudp* pUdp, ptr pData);
	void (*Close)(xnetudp* pUdp, xnetresult Result,
		const xerror* pError, ptr pData);
};



/* 发送节点保留单个数据报边界，并承载复制、引用或接管三种所有权。 */
struct __xrt_net_udp_send {
	__xrt_net_udp_send* Next;
	__xrt_net_udp_send* Previous;
	xnetudp* Udp;
	__xrt_net_udp_send_slot* Slot;
	xnetaddr Remote;
	cbytes Data;
	size_t Size;
	xnetreleaseproc Release;
	ptr ReleaseContext;
	bool Submitted;
	bool OwnsExternal;
	bool Copied;
	bool Controlled;
	union {
		uint64 Align;
		uint8 Data[1];
	} Tail;
};



/* 返回发送节点尾部按需保存的逐包控制值。 */
static inline xnetdgramcontrol* __xrtNetUdpSendControl(
	__xrt_net_udp_send* pSend
)
{
	return pSend->Controlled ?
		(xnetdgramcontrol*)pSend->Tail.Data : NULL;
}



/* 返回发送节点尾部按需保存的复制载荷。 */
static inline bytes __xrtNetUdpSendCopy(__xrt_net_udp_send* pSend)
{
	return pSend->Tail.Data + (pSend->Controlled ?
		sizeof(xnetdgramcontrol) : 0);
}



/* 每个 completion 发送槽只保存一个在途节点和稳定操作标识。 */
struct __xrt_net_udp_send_slot {
	xnetcompletion Completion;
	xnetudp* Udp;
	__xrt_net_udp_send* Send;
	uint64 Id;
};



/* 每个 completion 接收槽独占一个自适应缓冲和稳定 Completion。 */
struct __xrt_net_udp_receive {
	xnetcompletion Completion;
	xnetbuf Buffer;
	xnetudp* Udp;
	uint64 Id;
	bool Ready;
	bool Pending;
};



/* 错误队列使用独立自适应缓冲，避免与普通完成式接收争用。 */
struct __xrt_net_udp_error_receive {
	xnetcompletion Completion;
	xnetbuf Buffer;
	xnetudp* Udp;
	uint64 Id;
	bool Ready;
	bool Pending;
};



/* 仅在显式启用错误接收时分配队列、统计和完成槽。 */
struct __xrt_net_udp_error_state {
	xatomic64 Received;
	xatomic64 Dropped;
	xatomic64 PathMtuUpdates;
	xatomic64 PathMtu;
	xatomic64 Queued;
	xatomic64 PeakQueued;
	xatomic64 QueuedBytes;
	xatomic64 PeakQueuedBytes;
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		xatomic64 ErrorWaiters;
		__xrt_net_udp_wait* WaitHead;
		__xrt_net_udp_wait* WaitTail;
		size_t ReadyWaiters;
	#endif
	xnetudperrorpacket* Head;
	xnetudperrorpacket* Tail;
	size_t Size;
	size_t QueueLimit;
	size_t QueueByteLimit;
	xnetudpoverflow Overflow;
	void (*Callback)(xnetudp* pUdp,
		const xnetudperrormessage* pMessage, ptr pData);
	__xrt_net_udp_error_receive Receive;
};



/* 拉取数据包使用单次尾部分配，可从任意线程销毁。 */
struct xnetudppacket {
	volatile int32 References;
	xnetudppacket* Next;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	size_t Size;
	uint32 Flags;
	uint8 Data[1];
};



/* 拉取错误包拥有结构化错误和原数据报负载前缀。 */
struct xnetudperrorpacket {
	volatile int32 References;
	xnetudperrorpacket* Next;
	xnetdgramerror Error;
	size_t Size;
	uint8 Data[1];
};



/* 可变 IO 状态归 Worker，跨线程入口只触碰原子门、预算和拉取队列锁。 */
struct xnetudp {
	volatile int32 References;
	xatomic32 State;
	xatomic32 ControlRequests;
	xatomic32 SendGate;
	xatomic32 CloseGate;
	xatomic32 AbortGate;
	xatomic32 SendSubmitters;
	xatomic32 SendCommands;
	xatomic32 ActiveReceives;
	xatomic64 ActiveSends;
	xatomic64 PeakActiveSends;
	xatomic64 QueuedBytes;
	xatomic64 PeakQueuedBytes;
	xatomic64 QueuedPackets;
	xatomic64 PeakQueuedPackets;
	xatomic64 ReceiveQueued;
	xatomic64 PeakReceiveQueued;
	xatomic64 ReceiveQueuedBytes;
	xatomic64 PeakReceiveQueuedBytes;
	xatomic64 ReceivedPackets;
	xatomic64 ReceivedBytes;
	xatomic64 SentPackets;
	xatomic64 SentBytes;
	xatomic64 Truncated;
	xatomic64 TruncatedDropped;
	xatomic64 DroppedNewest;
	xatomic64 DroppedOldest;
	xatomic64 ReceiveErrors;
	xatomic64 SendErrors;
	xatomic64 SendRejected;
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		xatomic64 ReceiveWaiters;
	#endif
	xnetengine* Engine;
	xnetworker* Worker;
	xnetsocket Socket;
	__xrt_net_udp_config Config;
	__xrt_net_udp_events Events;
	xatomicptr Data;
	xnetaddr Local;
	xnetaddr Peer;
	xnetcompletion Completion;
	xrt_spinlock ReceiveLock;
	xnetudppacket* ReceiveHead;
	xnetudppacket* ReceiveTail;
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		__xrt_net_udp_wait* WaitHead;
		__xrt_net_udp_wait* WaitTail;
		__xrt_net_udp_wait* ReceiveFutureHead;
		__xrt_net_udp_wait* ReceiveFutureTail;
		__xrt_net_engine_internal WaitCommand;
		size_t ReceiveReadyWaiters;
		bool WaitPosted;
		bool WaitClosed;
	#endif
	__xrt_net_udp_send* SendHead;
	__xrt_net_udp_send* SendTail;
	__xrt_net_udp_send* SendReady;
	__xrt_net_udp_send_slot* SendSlots;
	__xrt_net_udp_receive* Receives;
	__xrt_net_udp_error_state* Errors;
	xerror* Error;
	xnetresult CloseResult;
	uint64 WatchId;
	uint32 WatchEvents;
	uint32 ReceiveSlots;
	uint32 SendSlotCount;
	uint32 NextSendSlot;
	uint32 SendControl;
	bool ReceiveLockReady;
	bool Connected;
	bool CompletionPort;
	bool WriteDriving;
	bool WatchPending;
	bool OpenEmitted;
	bool HighWater;
	bool CloseRequested;
	bool AbortRequested;
	bool EngineHeld;
	bool RuntimeHeld;
	__xrt_net_engine_internal ControlCommand;
};



/* Worker 回调读取当前用户数据，不取得指针目标所有权。 */
static inline ptr __xrtNetUdpDataCurrent(const xnetudp* pUdp)
{
	return xrtAtomicPtrLoad(&pUdp->Data, XMEMORY_RELAXED);
}



/* 设置当前执行上下文的 UDP 结构化错误。 */
void __xrtNetUdpSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
);



/* UDP 峰值只在 FULL 统计级别采样。 */
#define __xrtNetUdpPeak(pPeak, iValue) \
	__xrtNetStatFullPeak64((pPeak), (iValue))



/* 发送门建立后，由最后一个提交者或命令唤醒生命周期收敛。 */
void __xrtNetUdpWakeLifecycle(xnetudp* pUdp);



/* 丢弃一个发送节点并回滚预算和所有权。 */
void __xrtNetUdpDiscardSend(
	__xrt_net_udp_send* pSend,
	bool bReleaseExternal
);



/* 终结一个已挂入 Worker 的发送节点。 */
void __xrtNetUdpReleaseSend(__xrt_net_udp_send* pSend);



/* 在 Worker 上推进 UDP 发送队列。 */
void __xrtNetUdpDriveWrite(xnetudp* pUdp);



/* 在全部 IO 和发送预算终结后完成关闭。 */
void __xrtNetUdpTryFinish(xnetudp* pUdp);



/* 处理 UDP 共享发送与 readiness completion。 */
void __xrtNetUdpCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
);



/* 处理一个 completion UDP 发送槽的终态。 */
void __xrtNetUdpSendCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
);



/* 在所属 Worker 上合并启动、正常关闭和异常关闭。 */
void __xrtNetUdpControl(xnetworker* pWorker, ptr pData);



#if defined(XRT_FEATURE_NET_UDP_FUTURE)
/* 调用方持有 ReceiveLock 时，把排队数据包配对给拉取 Future。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairReceiveLocked(xnetudp* pUdp);



/* 调用方持有 ReceiveLock 时，把排队错误配对给拉取 Future。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairErrorLocked(xnetudp* pUdp);



/* 完成一条已经从等待链取出的 Future 链。 */
void __xrtNetUdpFutureFinishList(__xrt_net_udp_wait* pWaiter);



/* 推进 UDP 状态、可读、可写、排空和关闭 Future。 */
void __xrtNetUdpFutureNotify(xnetudp* pUdp);
#endif



/* 调用方持有 ReceiveLock 时取走一个错误包并同步队列统计。 */
xnetudperrorpacket* __xrtNetUdpTakeErrorLocked(xnetudp* pUdp);

#endif

#endif
