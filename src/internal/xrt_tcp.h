#ifndef XRT_INTERNAL_TCP_H
#define XRT_INTERNAL_TCP_H

#include "xrt_net_engine.h"
#include <xrt/tcp.h>



#if defined(XRT_FEATURE_NET_TCP)

typedef struct __xrt_net_stream_send __xrt_net_stream_send;
typedef struct __xrt_net_stream_refs __xrt_net_stream_refs;
typedef struct __xrt_net_stream_buffer __xrt_net_stream_buffer;
#if defined(XRT_FEATURE_NET_TCP_FILE)
typedef struct __xrt_net_stream_file __xrt_net_stream_file;
#endif
typedef struct __xrt_net_accept_slot __xrt_net_accept_slot;

#if defined(XRT_FEATURE_NET_TCP_DIAL)
typedef struct __xrt_net_stream_control __xrt_net_stream_control;
#endif

#if defined(XRT_FEATURE_NET_TCP_FUTURE)
typedef struct __xrt_net_stream_wait __xrt_net_stream_wait;
typedef struct __xrt_net_listener_wait __xrt_net_listener_wait;
#endif



/* 批量引用条目保留调用方释放过程，并反向定位所属批次。 */
typedef struct __xrt_net_stream_ref_item {
	__xrt_net_stream_refs* Batch;
	cbytes Data;
	size_t Size;
	xnetreleaseproc Release;
	ptr Context;
} __xrt_net_stream_ref_item;



/* 发送节点同时承担跨线程命令和零复制块的释放上下文。 */
struct __xrt_net_stream_send {
	xnetstream* Stream;
	cbytes Data;
	size_t Size;
	xnetreleaseproc Release;
	ptr ReleaseContext;
	bool OwnsExternal;
	uint8 Copy[1];
};



/* 批量引用只复制元数据，载荷仍由各引用块直接发送。 */
struct __xrt_net_stream_refs {
	xnetstream* Stream;
	size_t Total;
	size_t Count;
	size_t Remaining;
	__xrt_net_stream_ref_item Items[1];
};



/* 缓冲接管条目把一个借用发送块映射回共同拥有的源缓冲。 */
typedef struct __xrt_net_stream_buffer_item {
	__xrt_net_stream_buffer* Batch;
} __xrt_net_stream_buffer_item;



/* 缓冲批次持有原块链，写队列只增加不复制载荷的只读视图。 */
struct __xrt_net_stream_buffer {
	xnetstream* Stream;
	xnetbuf Owned;
	size_t Count;
	size_t Remaining;
	__xrt_net_stream_buffer_item Items[1];
};



#if defined(XRT_FEATURE_NET_TCP_FILE)

/* 文件发送节点持有独立原生句柄，不延长或借用公开 xfile 对象。 */
struct __xrt_net_stream_file {
	xnetstream* Stream;
	intptr_t Handle;
	uint64 Offset;
	size_t Size;
};

#endif



/* 每个完成式 Accept 槽独立保存身份，终态无需扫描并发槽表。 */
struct __xrt_net_accept_slot {
	xnetcompletion Completion;
	xnetlistener* Listener;
	uint64 Id;
};



/* 统一验证所有复用 Stream 配置的上层入口。 */
bool __xrtNetStreamConfigValid(const xnetstreamconfig* pConfig);



/* 统一验证 Listener 地址、并发、分发和 Socket 策略。 */
bool __xrtNetListenConfigValid(const xnetlistenconfig* pConfig);



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 托管连接只在公开 Open 前观察候选终态，成功发布后立即脱钩。 */
struct __xrt_net_stream_control {
	bool (*Open)(xnetstream* pStream, ptr pData);
	void (*Published)(xnetstream* pStream, ptr pData);
	void (*Close)(xnetstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
	void (*Release)(ptr pData);
	ptr Data;
};
#endif



/* Stream 的可变 IO 状态只由所属 Worker 访问。 */
struct xnetstream {
	volatile int32 References;
	xatomic32 State;
	xatomic32 ReadPaused;
	xatomic32 ControlRequests;
	xatomic32 ReadEnded;
	xatomic32 WriteEnded;
	xatomic32 WriteGate;
	xatomic32 CloseGate;
	xatomic32 AbortGate;
	xatomic32 SendSubmitters;
	xatomic32 SendCommands;
	xatomic64 QueuedBytes;
	xatomic64 PeakQueuedBytes;
	xatomic64 ReceivedBytes;
	xatomic64 SentBytes;
	xatomic64 ReadEvents;
	xatomic64 WriteEvents;
	xatomic64 SendRejected;
	xatomic64 BufferedBytes;
	xatomic32 ReadBlocked;
	xatomic32 WriteBackpressured;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetsocket Socket;
	xnetstreamconfig Config;
	xnetstreamevents Events;
	xatomicptr Data;
	xnetaddr Local;
	xnetaddr Remote;
	xnetcompletion Completion;
	xnetbuf ReadBuffer;
	xnetbuf WriteBuffer;
	xerror* Error;
	xnetresult CloseResult;
	uint64 ConnectId;
	uint64 ReadId;
	uint64 WriteId;
	uint64 ConnectTimer;
	size_t ReadCapacity;
	__xrt_net_engine_internal ControlCommand;
	uint32 WatchEvents;
	bool BuffersReady;
	bool StartPending;
	bool ConnectPending;
	bool ReadPending;
	bool ReadDirect;
	bool WritePending;
	bool WatchPending;
	bool WriteDrained;
	bool OpenEmitted;
	bool EndEmitted;
	bool ShutdownRequested;
	bool CloseRequested;
	bool AbortRequested;
	bool EngineHeld;
	bool RuntimeHeld;
	uint32 ActiveDepth;
	bool ReleasePending;
	xnetstream* AcceptNext;
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		__xrt_net_stream_control Control;
	#endif
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		xrt_spinlock WaitLock;
		__xrt_net_stream_wait* WaitHead;
		__xrt_net_stream_wait* WaitTail;
		__xrt_net_engine_internal WaitCommand;
		uint32 ReadWaiters;
		bool WaitPosted;
		bool WaitClosed;
		bool ReadPush;
	#endif
};



/* Listener 固定在一个 Worker，接受结果可以分发到任意 Worker。 */
struct xnetlistener {
	volatile int32 References;
	xatomic32 State;
	xatomic64 Accepted;
	xatomic64 Rejected;
	xatomic64 Errors;
	xatomic32 ActiveAccepts;
	xatomic32 ActiveDispatches;
	xatomic32 QueuedAccepts;
	xatomic32 PeakQueuedAccepts;
	xatomic32 AcceptWaiters;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetsocket Socket;
	xnetlistenconfig Config;
	xnetlistenerevents Events;
	xnetstreamevents StreamEvents;
	ptr Data;
	xnetaddr Local;
	xnetcompletion Completion;
	__xrt_net_accept_slot* AcceptSlots;
	uint64 WatchId;
	uint64 AcceptRetryTimer;
	uint64 AcceptRetryDelay;
	uint64 NextAffinity;
	xrt_spinlock AcceptLock;
	xnetstream* AcceptHead;
	xnetstream* AcceptTail;
	__xrt_net_engine_internal CloseCommand;
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrt_net_listener_wait* WaitHead;
		__xrt_net_listener_wait* WaitTail;
		bool WaitClosed;
	#endif
	bool WatchPending;
	bool StartPending;
	bool EngineHeld;
	bool RuntimeHeld;
};



/* 接收缓冲被消费后刷新并发快照和自动读背压。 */
void __xrtNetStreamReadRefresh(xnetstream* pStream, bool bDrive);



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 建立一个尚未启动、将在公开 Open 前由托管连接选择的数字地址候选。 */
xnetstream* __xrtNetStreamCreateControlled(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const __xrt_net_stream_control* pControl
);



/* 在候选所属 Worker 上无分配地启动连接。 */
void __xrtNetStreamStartControlled(xnetstream* pStream);



/* 在候选所属 Worker 上安装最终用户事件和数据。 */
bool __xrtNetStreamAdopt(
	xnetstream* pStream,
	const xnetstreamevents* pEvents,
	ptr pData
);

/* 在所属 Worker 上无分配地终止 Stream，并保留指定终态。 */
void __xrtNetStreamFailCurrent(
	xnetstream* pStream,
	xnetresult Result
);



/* 在候选所属 Worker 上无分配地拒绝一个尚未发布的连接。 */
void __xrtNetStreamReject(xnetstream* pStream);
#endif



/* 调用方持有 AcceptLock 时取走一个排队 Stream。 */
xnetstream* __xrtNetListenerTakeQueued(xnetlistener* pListener);



#if defined(XRT_FEATURE_NET_TCP_FUTURE)
/* 在所属 Worker 上推进已经登记的 Stream Future。 */
void __xrtNetStreamFutureNotify(xnetstream* pStream, bool bDriveRead);



/* 配对 Listener 拉取队列与 Future，并关闭终止后的剩余等待。 */
void __xrtNetListenerFutureNotify(xnetlistener* pListener);



/* 让最早的 Listener Future 直接接管一个已接受 Stream。 */
bool __xrtNetListenerFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream
);
#endif

#endif

#endif
