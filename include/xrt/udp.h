#ifndef XRT_UDP_H
#define XRT_UDP_H

#include <xrt/net.h>

#if defined(XRT_FEATURE_NET_UDP_FUTURE) || \
	defined(XRT_FEATURE_NET_UDP_SYNC)
	#include <xrt/future.h>
#endif



#if defined(XRT_FEATURE_NET_UDP) && !defined(XRT_FEATURE_NET_ENGINE)
	#error "XRT UDP support requires network engine support"
#endif

#if defined(XRT_FEATURE_NET_UDP_FUTURE) && !defined(XRT_FEATURE_NET_UDP)
	#error "XRT UDP Future support requires UDP support"
#endif

#if defined(XRT_FEATURE_NET_UDP_FUTURE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT UDP Future support requires Future support"
#endif

#if defined(XRT_FEATURE_NET_UDP_SYNC) && \
	(!defined(XRT_FEATURE_NET_UDP_FUTURE) || !defined(XRT_FEATURE_NET_SYNC))
	#error "XRT UDP sync support requires UDP Future and network sync support"
#endif



#if defined(XRT_FEATURE_NET_UDP)

/* 普通 IPv4 和 IPv6 UDP 的保守最大载荷，避免依赖 IP 分片外的扩展语义。 */
#define XNET_UDP_PAYLOAD_MAX 65507u



typedef struct xnetudp xnetudp;
typedef struct xnetudppacket xnetudppacket;
typedef struct xnetudperrorpacket xnetudperrorpacket;

#if defined(XRT_FEATURE_NET_UDP_FUTURE)
typedef struct xnetudpbatch xnetudpbatch;
#endif



/* UDP 状态只向前推进，CLOSED 是唯一终态。 */
typedef enum xnetudpstate {
	XNET_UDP_OPENING = 0,
	XNET_UDP_OPEN,
	XNET_UDP_CLOSING,
	XNET_UDP_CLOSED
} xnetudpstate;



#if defined(XRT_FEATURE_NET_UDP_FUTURE)
/* UDP 等待条件是水平条件；Future 只表示本次等待，不接管 UDP 对象。 */
typedef enum xnetudpwait {
	XNET_UDP_WAIT_OPEN = 0,
	XNET_UDP_WAIT_RECEIVE,
	XNET_UDP_WAIT_ERROR,
	XNET_UDP_WAIT_DRAIN,
	XNET_UDP_WAIT_CLOSE
} xnetudpwait;
#endif



/* 拉取队列满时绝不阻塞 Worker，由调用方明确选择丢弃策略。 */
typedef enum xnetudpoverflow {
	XNET_UDP_DROP_NEWEST = 0,
	XNET_UDP_DROP_OLDEST,
	XNET_UDP_DROP_ERROR
} xnetudpoverflow;



/* 接收缓冲不足时可以投递已截断前缀、静默丢弃或报告可恢复错误。 */
typedef enum xnetudptruncation {
	XNET_UDP_TRUNCATE_DELIVER = 0,
	XNET_UDP_TRUNCATE_DROP,
	XNET_UDP_TRUNCATE_ERROR
} xnetudptruncation;



/* 消息标志在推送消息和拥有型数据包之间保持一致。 */
typedef enum xnetudpflag {
	XNET_UDP_MESSAGE_TRUNCATED = 0x01
} xnetudpflag;



/* 推送消息只在 Receive 回调期间借用地址与数据。 */
typedef struct xnetudpmessage {
	xnetaddr Remote;
	xnetdgrammeta Meta;
	cbytes Data;
	size_t Size;
	uint32 Flags;
} xnetudpmessage;



/* 数据报协议错误只在 DatagramError 回调期间借用负载前缀。 */
typedef struct xnetudperrormessage {
	xnetdgramerror Error;
	cbytes Data;
	size_t Size;
} xnetudperrormessage;



/* 所有事件均在 UDP 所属 Worker 上串行执行。 */
typedef struct xnetudpevents {
	void (*Open)(xnetudp* pUdp, ptr pData);
	void (*Receive)(xnetudp* pUdp,
		const xnetudpmessage* pMessage, ptr pData);
	void (*DatagramError)(xnetudp* pUdp,
		const xnetudperrormessage* pMessage, ptr pData);
	void (*Error)(xnetudp* pUdp, const xerror* pError, ptr pData);
	void (*HighWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*LowWater)(xnetudp* pUdp,
		size_t iBytes, size_t iPackets, ptr pData);
	void (*Drain)(xnetudp* pUdp, ptr pData);
	void (*Close)(xnetudp* pUdp, xnetresult Result,
		const xerror* pError, ptr pData);
} xnetudpevents;



/* completion 收发并发独立配置；readiness 后端不为并发槽增加对象内存。 */
typedef struct xnetudpconfig {
	size_t ReceiveSize;
	uint32 ReceiveConcurrency;
	uint32 ReceiveBatch;
	uint32 ReceiveMeta;
	size_t ReceiveQueueLimit;
	size_t ReceiveQueueByteLimit;
	xnetudpoverflow Overflow;
	xnetudptruncation Truncation;
	size_t ErrorSize;
	size_t ErrorQueueLimit;
	size_t ErrorQueueByteLimit;
	xnetudpoverflow ErrorOverflow;
	size_t SendHighWater;
	size_t SendLowWater;
	size_t SendLimit;
	size_t SendPacketLimit;
	/* 大于一时允许发送完成和外部数据释放乱序；默认一保持提交顺序。 */
	uint32 SendConcurrency;
	int ReceiveBuffer;
	int SendBuffer;
	int HopLimit;
	int TrafficClass;
	xnetpmtumode PathMtu;
	bool ReuseAddress;
	bool ReusePort;
	bool ExclusiveAddress;
	bool Broadcast;
	bool IPv6Only;
	bool ReceiveErrors;
} xnetudpconfig;



/* UDP 统计是无锁并发快照，累计值在关闭后仍可读取。 */
typedef struct xnetudpstats {
	xnetudpstate State;
	uint64 ReceivedPackets;
	/* 实际捕获的前缀字节；截断报文不包含内核丢弃的尾部。 */
	uint64 ReceivedBytes;
	uint64 SentPackets;
	uint64 SentBytes;
	uint64 Truncated;
	uint64 TruncatedDropped;
	uint64 DroppedNewest;
	uint64 DroppedOldest;
	uint64 ReceiveErrors;
	uint64 SendErrors;
	uint64 SendRejected;
	uint64 DatagramErrors;
	uint64 DatagramErrorsDropped;
	uint64 PathMtuUpdates;
	size_t PathMtu;
	size_t QueuedBytes;
	size_t PeakQueuedBytes;
	size_t QueuedPackets;
	size_t PeakQueuedPackets;
	size_t ReceiveQueued;
	size_t PeakReceiveQueued;
	size_t ReceiveQueuedBytes;
	size_t PeakReceiveQueuedBytes;
	size_t ErrorQueued;
	size_t PeakErrorQueued;
	size_t ErrorQueuedBytes;
	size_t PeakErrorQueuedBytes;
	size_t ReceiveWaiters;
	size_t ErrorWaiters;
	uint32 ActiveReceives;
	uint32 ActiveSends;
	uint32 PeakActiveSends;
	bool Connected;
} xnetudpstats;



XRT_EXTERN_C_BEGIN



/* 初始化低内存默认配置：一个 2 KiB 接收槽和有界收发队列。 */
XRT_API void xrtNetUdpConfigInit(xnetudpconfig* pConfig);



/* 打开、绑定并可选连接 UDP；至少一个地址必须确定地址族。 */
XRT_API xnetudp* xrtNetUdpOpen(
	xnetengine* pEngine,
	const xnetaddr* pLocal,
	const xnetaddr* pPeer,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
);



/* 打开未连接 UDP，适合服务器、多对端客户端和多播接收。 */
XRT_API xnetudp* xrtNetUdpBind(
	xnetengine* pEngine,
	const xnetaddr* pLocal,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
);



/* 打开连接式 UDP，并自动绑定同地址族的任意本地地址。 */
XRT_API xnetudp* xrtNetUdpConnect(
	xnetengine* pEngine,
	const xnetaddr* pPeer,
	uint64 iAffinity,
	const xnetudpconfig* pConfig,
	const xnetudpevents* pEvents,
	ptr pData
);



/* 增加 UDP 引用并返回原指针。 */
XRT_API xnetudp* xrtNetUdpRef(xnetudp* pUdp);



/* 释放 UDP 引用；关闭操作必须另行请求。 */
XRT_API void xrtNetUdpDestroy(xnetudp* pUdp);



/* 复制发送到指定对端；空对端使用连接式 UDP 的固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const void* pData, size_t iSize);



/* 复制发送到连接式 UDP 的固定 Peer。 */
XRT_API xnetresult xrtNetUdpSend(xnetudp* pUdp,
	const void* pData, size_t iSize);



/* 聚集复制为一个数据报后发送，所有 Span 在返回前完成复制。 */
XRT_API xnetresult xrtNetUdpSendVecTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetspan* pSpans, size_t iCount);



/* 聚集复制为一个数据报后发送到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendVec(xnetudp* pUdp,
	const xnetspan* pSpans, size_t iCount);



/* 零复制发送；成功受理后在数据报离开队列时执行一次释放过程。 */
XRT_API xnetresult xrtNetUdpSendRefTo(xnetudp* pUdp,
	const xnetaddr* pRemote, const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);



/* 零复制发送到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendRef(xnetudp* pUdp,
	const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);



/* 接管 XRT 分配的数据并发送；失败时所有权仍属于调用方。 */
XRT_API xnetresult xrtNetUdpSendTakeTo(xnetudp* pUdp,
	const xnetaddr* pRemote, ptr pData, size_t iSize);



/* 接管 XRT 分配的数据并发送到固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendTake(xnetudp* pUdp,
	ptr pData, size_t iSize);



/* 复制发送带逐包控制的数据报；空远端使用连接式 UDP 的固定 Peer。 */
XRT_API xnetresult xrtNetUdpSendMsg(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	const void* pData, size_t iSize);



/* 引用发送带逐包控制的数据报，终态执行一次释放过程。 */
XRT_API xnetresult xrtNetUdpSendMsgRef(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	const void* pData, size_t iSize,
	xnetreleaseproc pRelease, ptr pContext);



/* 接管 XRT 分配的数据并按逐包控制发送；失败时所有权不转移。 */
XRT_API xnetresult xrtNetUdpSendMsgTake(xnetudp* pUdp,
	const xnetaddr* pRemote, const xnetdgramcontrol* pControl,
	ptr pData, size_t iSize);



/* 按前缀批量受理复制发送；受理计数输出不能为空。 */
XRT_API xnetresult xrtNetUdpSendBatch(xnetudp* pUdp,
	const xnetdgramsend* pItems, size_t iCount, size_t* pAccepted);



/* 未设置 Receive 回调时，非阻塞取出一个拥有型数据包。 */
XRT_API xnetudppacket* xrtNetUdpReceive(xnetudp* pUdp);



/* 未设置 Receive 回调时，在一次锁内取出最多指定数量的数据包。 */
XRT_API size_t xrtNetUdpReceiveBatch(xnetudp* pUdp,
	xnetudppacket** pPackets, size_t iCapacity);



/* 未设置 DatagramError 回调时，非阻塞取出一个拥有型错误包。 */
XRT_API xnetudperrorpacket* xrtNetUdpReceiveError(xnetudp* pUdp);



/* 未设置 DatagramError 回调时，在一次锁内取出一批错误包。 */
XRT_API size_t xrtNetUdpReceiveErrorBatch(xnetudp* pUdp,
	xnetudperrorpacket** pPackets, size_t iCapacity);



/* 销毁拥有型数据包；空指针视为空操作。 */
XRT_API void xrtNetUdpPacketDestroy(xnetudppacket* pPacket);



/* 增加数据包引用并返回原指针，便于跨 Future 和线程保留零复制结果。 */
XRT_API xnetudppacket* xrtNetUdpPacketRef(xnetudppacket* pPacket);



/* 返回数据包的借用远端地址。 */
XRT_API const xnetaddr* xrtNetUdpPacketRemote(const xnetudppacket* pPacket);



/* 返回数据包内拥有的接收元数据；空数据包返回空指针。 */
XRT_API const xnetdgrammeta* xrtNetUdpPacketMeta(
	const xnetudppacket* pPacket
);



/* 返回数据包的借用连续载荷。 */
XRT_API cbytes xrtNetUdpPacketData(const xnetudppacket* pPacket);



/* 返回数据包载荷长度。 */
XRT_API size_t xrtNetUdpPacketSize(const xnetudppacket* pPacket);



/* 返回数据包是否只包含一个被截断的前缀。 */
XRT_API bool xrtNetUdpPacketTruncated(const xnetudppacket* pPacket);



/* 销毁拥有型数据报错误包；空指针视为空操作。 */
XRT_API void xrtNetUdpErrorPacketDestroy(xnetudperrorpacket* pPacket);



/* 增加错误包引用并返回原指针。 */
XRT_API xnetudperrorpacket* xrtNetUdpErrorPacketRef(
	xnetudperrorpacket* pPacket
);



/* 返回错误包内拥有的结构化数据报错误。 */
XRT_API const xnetdgramerror* xrtNetUdpErrorPacketInfo(
	const xnetudperrorpacket* pPacket
);



/* 返回错误包内原数据报负载前缀。 */
XRT_API cbytes xrtNetUdpErrorPacketData(
	const xnetudperrorpacket* pPacket
);



/* 返回错误包负载前缀长度。 */
XRT_API size_t xrtNetUdpErrorPacketSize(
	const xnetudperrorpacket* pPacket
);



/* 停止接收，并在已受理发送全部终结后正常关闭。 */
XRT_API bool xrtNetUdpClose(xnetudp* pUdp);



/* 取消在途 IO、丢弃发送队列并尽快关闭。 */
XRT_API bool xrtNetUdpAbort(xnetudp* pUdp);



/* 只在 UDP Worker 内加入多播组。 */
XRT_API bool xrtNetUdpJoin(xnetudp* pUdp,
	const xnetaddr* pGroup, const xnetaddr* pInterface);



/* 只在 UDP Worker 内离开多播组。 */
XRT_API bool xrtNetUdpLeave(xnetudp* pUdp,
	const xnetaddr* pGroup, const xnetaddr* pInterface);



/* 只在 UDP Worker 内设置多播回环。 */
XRT_API bool xrtNetUdpMulticastLoop(xnetudp* pUdp, bool bEnabled);



/* 只在 UDP Worker 内设置多播跳数。 */
XRT_API bool xrtNetUdpMulticastHopLimit(xnetudp* pUdp, int iHopLimit);



/* 只在 UDP Worker 内选择多播发送接口，空接口恢复系统默认。 */
XRT_API bool xrtNetUdpMulticastInterface(xnetudp* pUdp,
	const xnetaddr* pInterface);



/* 返回 UDP 当前状态的并发快照。 */
XRT_API xnetudpstate xrtNetUdpState(const xnetudp* pUdp);



/* 复制实际绑定的本地地址。 */
XRT_API bool xrtNetUdpLocal(const xnetudp* pUdp, xnetaddr* pAddress);



/* 复制连接式 UDP 的固定 Peer，未连接时失败。 */
XRT_API bool xrtNetUdpPeer(const xnetudp* pUdp, xnetaddr* pAddress);



/* 返回 UDP 是否具有固定 Peer。 */
XRT_API bool xrtNetUdpConnected(const xnetudp* pUdp);



/* 返回此 UDP 对象可用于 SendMsg 的逐数据报发送控制位。 */
XRT_API uint32 xrtNetUdpSendControlAvailable(const xnetudp* pUdp);



/* 返回 UDP 所属的借用 Worker。 */
XRT_API xnetworker* xrtNetUdpWorker(const xnetudp* pUdp);



/* 只在 UDP Worker 内返回借用 Socket，调用方不得关闭或接管 IO。 */
XRT_API xnetsocket xrtNetUdpSocket(xnetudp* pUdp);



/* 只在 UDP Worker 内替换用户数据。 */
XRT_API bool xrtNetUdpSetData(xnetudp* pUdp, ptr pData);



/* 原子读取借用的用户数据快照，不延长指针目标生命周期。 */
XRT_API ptr xrtNetUdpData(const xnetudp* pUdp);



/* 返回导致 UDP 终止的借用错误，正常关闭和未关闭时为空。 */
XRT_API const xerror* xrtNetUdpError(const xnetudp* pUdp);



/* 返回当前发送队列占用的字节数。 */
XRT_API size_t xrtNetUdpPending(const xnetudp* pUdp);



/* 返回当前拉取接收队列中的数据包数量。 */
XRT_API size_t xrtNetUdpQueued(const xnetudp* pUdp);



/* 返回当前拉取接收队列中的载荷字节数。 */
XRT_API size_t xrtNetUdpQueuedBytes(const xnetudp* pUdp);



/* 返回当前拉取错误队列中的条目数量。 */
XRT_API size_t xrtNetUdpQueuedErrors(const xnetudp* pUdp);



/* 返回当前拉取错误队列中的负载前缀字节数。 */
XRT_API size_t xrtNetUdpQueuedErrorBytes(const xnetudp* pUdp);



/* 返回最近一次错误队列确认的路径 MTU，未知时为零。 */
XRT_API size_t xrtNetUdpPathMtu(const xnetudp* pUdp);



/* 复制 UDP 并发统计。 */
XRT_API bool xrtNetUdpStats(const xnetudp* pUdp, xnetudpstats* pStats);



#if defined(XRT_FEATURE_NET_UDP_FUTURE)
/* 异步等待 UDP 条件；成功、失败、取消和关闭映射到统一 Future 终态。 */
XRT_API xfuture* xrtNetUdpWaitAsync(
	xnetudp* pUdp,
	xnetudpwait Wait
);



/* 等待发送队列至少能够原子接纳一个指定大小的数据报。 */
XRT_API xfuture* xrtNetUdpWritableAsync(
	xnetudp* pUdp,
	size_t iSize
);



/* 拉取模式下异步接收一个数据包；成功值由 Future 持有一个 Packet 引用。 */
XRT_API xfuture* xrtNetUdpReceiveAsync(xnetudp* pUdp);



/* 拉取模式下异步接收一个结构化数据报错误；成功值由 Future 持有引用。 */
XRT_API xfuture* xrtNetUdpReceiveErrorAsync(xnetudp* pUdp);



/* 拉取模式下异步接收当前可用批次；容量必须位于 1 到 256 之间。 */
XRT_API xfuture* xrtNetUdpReceiveBatchAsync(
	xnetudp* pUdp,
	size_t iCapacity
);



/* 增加批量结果引用并返回原指针。 */
XRT_API xnetudpbatch* xrtNetUdpBatchRef(xnetudpbatch* pBatch);



/* 返回 Future 批量结果中的数据包数量。 */
XRT_API size_t xrtNetUdpBatchCount(const xnetudpbatch* pBatch);



/* 返回批量结果中一个借用的数据包。 */
XRT_API xnetudppacket* xrtNetUdpBatchPacket(
	const xnetudpbatch* pBatch,
	size_t iIndex
);



/* 从批量结果转移一个数据包所有权；对应位置随后为空。 */
XRT_API xnetudppacket* xrtNetUdpBatchTake(
	xnetudpbatch* pBatch,
	size_t iIndex
);



/* 销毁批量结果及其中仍未转移的数据包。 */
XRT_API void xrtNetUdpBatchDestroy(xnetudpbatch* pBatch);
#endif



#if defined(XRT_FEATURE_NET_UDP_SYNC)
/* 阻塞等待一个 UDP 条件；禁止从该 UDP 所属 Worker 调用。 */
XRT_API bool xrtNetUdpWait(
	xnetudp* pUdp,
	xnetudpwait Wait,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞等待发送队列能够原子接纳指定大小的数据报。 */
XRT_API bool xrtNetUdpWritable(
	xnetudp* pUdp,
	size_t iSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞接收一个拥有型数据包。 */
XRT_API xnetudppacket* xrtNetUdpReceiveWait(
	xnetudp* pUdp,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞接收一个拥有型结构化数据报错误。 */
XRT_API xnetudperrorpacket* xrtNetUdpReceiveErrorWait(
	xnetudp* pUdp,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 阻塞接收一个拥有型批量结果。 */
XRT_API xnetudpbatch* xrtNetUdpReceiveBatchWait(
	xnetudp* pUdp,
	size_t iCapacity,
	xdeadline iDeadline,
	xcancel* pCancel
);
#endif



XRT_EXTERN_C_END

#endif

#endif
