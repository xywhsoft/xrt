#ifndef XRT_INTERNAL_NET_ENGINE_H
#define XRT_INTERNAL_NET_ENGINE_H

#include "xrt_net_port.h"
#include "xrt_net_buffer.h"



#if defined(XRT_FEATURE_NET_ENGINE)

#define XRT_NET_ENGINE_NODE_CLASS_COUNT 5u
#define XRT_NET_ENGINE_NODE_SIZE_MAX 1024u

typedef struct __xrt_net_engine_command __xrt_net_engine_command;
typedef struct __xrt_net_engine_timer __xrt_net_engine_timer;
typedef struct __xrt_net_engine_internal __xrt_net_engine_internal;
typedef struct __xrt_net_engine_node __xrt_net_engine_node;



/* 小节点缓存借用对象首个指针宽度保存空闲链。 */
struct __xrt_net_engine_node {
	__xrt_net_engine_node* Next;
};



/* 内部命令节点由高层对象嵌入，生命周期终态不依赖分配和公开队列容量。 */
struct __xrt_net_engine_internal {
	__xrt_net_engine_internal* Next;
	xnettaskproc Task;
	ptr Data;
};



#define XRT_NET_POST_MAGIC UINT32_C(0x584E5054)



/* 公开嵌入式 Post 在内部命令后保存用户任务和单次排队门。 */
typedef struct xrt_net_post_impl {
	__xrt_net_engine_internal Internal;
	xnettaskproc Task;
	ptr Data;
	xatomic32 Pending;
	uint32 Magic;
} xrt_net_post_impl;



typedef char xrt_net_post_storage_check[
	(sizeof(xrt_net_post_impl) <= XNET_POST_STORAGE_SIZE) ? 1 : -1
];



/* 读取公开嵌入式 Post 的内部布局。 */
static inline xrt_net_post_impl* __xrtNetPostImpl(xnetpost* pPost)
{
	return (xrt_net_post_impl*)pPost;
}



/* 命令只在目标 Worker 消费，生产端通过 MPSC 队列提交。 */
struct __xrt_net_engine_command {
	__xrt_net_engine_command* Next;
	uint32 Type;
	xnettaskproc Task;
	ptr Data;
	__xrt_net_engine_timer* Timer;
	uint64 TimerId;
};



/* Timer 同时挂入截止时间最小堆和 ID 哈希链。 */
struct __xrt_net_engine_timer {
	__xrt_net_engine_timer* HashNext;
	uint64 Id;
	xdeadline Deadline;
	xnettimerproc Proc;
	ptr Data;
	size_t HeapIndex;
};



/* Timer 表只由所属 Worker 访问，不需要跨线程锁。 */
typedef struct __xrt_net_engine_timers {
	__xrt_net_engine_timer** Heap;
	size_t Count;
	size_t Capacity;
	__xrt_net_engine_timer** Buckets;
	size_t BucketCount;
} __xrt_net_engine_timers;



/* 高频统计独立使用原子值，读取快照不阻塞 Worker。 */
typedef struct __xrt_net_engine_atomic_stats {
	xatomic64 PostsAccepted;
	xatomic64 PostsRejected;
	xatomic64 PostsExecuted;
	xatomic64 TimersAccepted;
	xatomic64 TimersRejected;
	xatomic64 TimersFired;
	xatomic64 TimersCancelled;
	xatomic64 TimersClosed;
	xatomic64 TimerErrors;
	xatomic64 Events;
	xatomic64 WaitErrors;
	xatomic64 WakeErrors;
	xatomic64 ShutdownStalls;
	xatomic32 LastWaitError;
	xatomic32 LastWaitSystemCode;
	xatomic64 NodeCacheHits;
	xatomic64 NodeCacheMisses;
} __xrt_net_engine_atomic_stats;



/* Worker 独占端口、命令消费端、Timer 表和事件批缓冲。 */
struct xnetworker {
	xnetengine* Engine;
	uint32 Index;
	xatomic32 Running;
	xatomic32 Stop;
	xatomic32 ShutdownPhase;
	xatomic32 ShutdownFailed;
	/* 最高位是停止门，其余位统计已经进入无锁提交区的调用。 */
	xatomic32 Submitters;
	xatomic32 CommandPending;
	xatomic32 TimerReserved;
	xatomic64 ThreadId;
	xthread* Thread;
	xnetport* Port;
	xnetbufpool* BufferPool;
	xmpscqueue Commands;
	xatomicptr InternalCommands;
	__xrt_net_engine_internal* InternalReady;
	xatomic32 InternalPending;
	xmutex CacheLock;
	__xrt_net_engine_node* NodeCache[XRT_NET_ENGINE_NODE_CLASS_COUNT];
	xatomic64 NodeCachedBytes;
	size_t NodeCacheLimit;
	xnetportevent* Events;
	__xrt_net_engine_timers Timers;
	__xrt_net_engine_atomic_stats Stats;
	bool CommandsReady;
	bool CacheLockReady;
	bool TimersReady;
};



/* Engine 固定 Worker 数量，Start/Stop 可以重复建立和释放运行资源。 */
struct xnetengine {
	xnetengineconfig Config;
	xnetbufpoolconfig BufferConfig;
	xnetworker* Workers;
	uint32 WorkerCount;
	xmutex Lifecycle;
	xatomic32 State;
	xatomic32 LiveObjects;
	xatomic64 NextTimer;
	xatomic64 NextOperation;
};



/* 高层网络对象借用 Engine 生命周期，阻止带活动对象的停止和销毁。 */
bool __xrtNetEngineObjectHold(xnetengine* pEngine);



/* 释放一个高层网络对象对 Engine 的生命周期占用。 */
void __xrtNetEngineObjectRelease(xnetengine* pEngine);



/* 从 Worker 的线程安全分级缓存分配并清零一个内部小节点。 */
ptr __xrtNetWorkerNodeAlloc(xnetworker* pWorker, size_t iSize);



/* 清零并回收一个由 Worker 小节点分配器取得的内部节点。 */
void __xrtNetWorkerNodeRecycle(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
);



/* 归还小节点后释放分配时取得的 Engine 生命周期占用。 */
void __xrtNetWorkerNodeRecycleHeld(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
);



/*
	无分配地把一个嵌入命令投递到目标 Worker。
	节点在执行前必须保持有效；停机封口后返回 false。
*/
bool __xrtNetEnginePostInternal(
	xnetworker* pWorker,
	__xrt_net_engine_internal* pCommand,
	xnettaskproc pProc,
	ptr pData
);



/*
	取消内部生命周期 Timer。
	所属 Worker 上立即发布取消回调，其他线程回退到公开异步请求；
	调用方必须在返回前持有回调无法释放的对象引用。
*/
bool __xrtNetEngineTimerCancelLifecycle(
	xnetengine* pEngine,
	uint64 Id
);

#endif

#endif
