#ifndef XRT_QUEUE_H
#define XRT_QUEUE_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_QUEUE) && !defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_QUEUE requires XRT_FEATURE_ATOMIC"
#endif

#if defined(XRT_FEATURE_QUEUE_SPSC) && !defined(XRT_FEATURE_QUEUE)
	#error "XRT_FEATURE_QUEUE_SPSC requires XRT_FEATURE_QUEUE"
#endif

#if defined(XRT_FEATURE_QUEUE_MPSC) && !defined(XRT_FEATURE_QUEUE)
	#error "XRT_FEATURE_QUEUE_MPSC requires XRT_FEATURE_QUEUE"
#endif

#if defined(XRT_FEATURE_QUEUE_MPMC) && !defined(XRT_FEATURE_QUEUE)
	#error "XRT_FEATURE_QUEUE_MPMC requires XRT_FEATURE_QUEUE"
#endif



#if defined(XRT_FEATURE_QUEUE)

#include <xrt/atomic.h>



/* 热游标隔离跨度覆盖 XRT 支持架构上的常见最大数据缓存行。 */
#if \
	defined(__aarch64__) || \
	defined(_M_ARM64) || \
	defined(__powerpc64__) || \
	defined(__ppc64__)
	#define XRT_QUEUE_CACHE_SPAN 128u
#else
	#define XRT_QUEUE_CACHE_SPAN 64u
#endif

#define XRT_QUEUE_MAX_CAPACITY ((size_t)1u << 30)



/* 队列结果把正常流控状态与真正错误分开表达。 */
typedef enum xqueueresult {
	XQUEUE_ERROR = -1,
	XQUEUE_OK = 0,
	XQUEUE_EMPTY = 1,
	XQUEUE_FULL = 2,
	XQUEUE_CLOSED = 3
} xqueueresult;



/* 批量操作同时返回流控状态和实际处理数量。 */
typedef struct xqueuebatchresult {
	xqueueresult Result;
	size_t Count;
} xqueuebatchresult;



/* 排空回调接收已从队列移除的指针值。 */
typedef void (*xqueuedrainfn)(ptr pItem, ptr pContext);



XRT_EXTERN_C_BEGIN



/* 把最小容量向上取整为队列可使用的 2 次幂容量。 */
XRT_API size_t xrtQueueCapacity(size_t iMinimum);



XRT_EXTERN_C_END

#endif



#if \
	defined(XRT_FEATURE_QUEUE_SPSC) || \
	defined(XRT_FEATURE_QUEUE_MPSC) || \
	defined(XRT_FEATURE_QUEUE_MPMC)

/* 32 位游标独占一个架构隔离跨度，避免生产者和消费者伪共享。 */
typedef struct xqueuecursor32 {
	xatomic32 Position;
	uint8 Reserved[XRT_QUEUE_CACHE_SPAN - sizeof(xatomic32)];
} xqueuecursor32;

#endif



#if defined(XRT_FEATURE_QUEUE_MPSC) || defined(XRT_FEATURE_QUEUE_MPMC)

/* 序列槽保存一个不拥有目标的指针，并用序号表达空闲和就绪代次。 */
typedef struct xqueueslot {
	xatomic32 Sequence;
	ptr Item;
} xqueueslot;

#endif



#if defined(XRT_FEATURE_QUEUE_SPSC)



/* SPSC 保存指针值，只允许一个生产者和一个消费者并发操作。 */
typedef struct xspscqueue {
	ptr* Items;
	ptr Allocation;
	size_t Capacity;
	size_t Mask;
	xatomic32 Closed;
	xqueuecursor32 Tail;
	xqueuecursor32 Head;
} xspscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化拥有内部指针环的单生产者单消费者队列。 */
XRT_API bool xrtSPSCQueueInit(xspscqueue* pQueue, size_t iCapacity);



/* 在调用方提供的 2 次幂指针环上初始化 SPSC 队列。 */
XRT_API bool xrtSPSCQueueInitBuffer(
	xspscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
);



/* 创建拥有结构和内部指针环的 SPSC 队列。 */
XRT_API xspscqueue* xrtSPSCQueueCreate(size_t iCapacity);



/* 释放拥有的指针环，但不释放队列结构或指针目标。 */
XRT_API void xrtSPSCQueueUnit(xspscqueue* pQueue);



/* 释放 Create 返回的队列结构和内部指针环。 */
XRT_API void xrtSPSCQueueDestroy(xspscqueue* pQueue);



/* 尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtSPSCQueueTryPush(xspscqueue* pQueue, ptr pItem);



/* 尝试批量压入连续指针值；数组必须对齐且不与队列对象或内部环重叠。 */
XRT_API xqueuebatchresult xrtSPSCQueuePushBatch(
	xspscqueue* pQueue,
	ptr const* pItems,
	size_t iCount
);



/* 尝试弹出一个指针值；输出必须对齐且不与队列对象或内部环重叠。 */
XRT_API xqueueresult xrtSPSCQueueTryPop(xspscqueue* pQueue, ptr* pItem);



/* 尝试批量弹出连续指针值；数组必须对齐且不与队列对象或内部环重叠。 */
XRT_API xqueuebatchresult xrtSPSCQueuePopBatch(
	xspscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
);



/* 返回并发快照下的近似元素数量。 */
XRT_API size_t xrtSPSCQueueCount(const xspscqueue* pQueue);



/* 由唯一生产者幂等关闭写入端，并允许消费者继续排空已有元素。 */
XRT_API void xrtSPSCQueueClose(xspscqueue* pQueue);



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtSPSCQueueIsClosed(const xspscqueue* pQueue);



/* 判断队列是否已经关闭且排空。 */
XRT_API bool xrtSPSCQueueIsDrained(const xspscqueue* pQueue);



/* 排空当前可见元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtSPSCQueueDrain(
	xspscqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
);



/* 在调用方独占且队列为空时重置游标并重新开放。 */
XRT_API bool xrtSPSCQueueReset(xspscqueue* pQueue);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_QUEUE_MPSC)

/* MPSC 允许多个生产者并发写入，只允许一个消费者读取。 */
typedef struct xmpscqueue {
	xqueueslot* Slots;
	ptr Allocation;
	size_t Capacity;
	size_t Mask;
	xatomic32 Closed;
	xqueuecursor32 Tail;
	xqueuecursor32 Head;
} xmpscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化拥有内部序列槽环的多生产者单消费者队列。 */
XRT_API bool xrtMPSCQueueInit(xmpscqueue* pQueue, size_t iCapacity);



/* 在调用方提供的 2 次幂序列槽环上初始化 MPSC 队列。 */
XRT_API bool xrtMPSCQueueInitBuffer(
	xmpscqueue* pQueue,
	xqueueslot* pSlots,
	size_t iCapacity
);



/* 创建拥有结构和内部序列槽环的 MPSC 队列。 */
XRT_API xmpscqueue* xrtMPSCQueueCreate(size_t iCapacity);



/* 释放拥有的序列槽环，但不释放队列结构或指针目标。 */
XRT_API void xrtMPSCQueueUnit(xmpscqueue* pQueue);



/* 释放 Create 返回的队列结构和内部序列槽环。 */
XRT_API void xrtMPSCQueueDestroy(xmpscqueue* pQueue);



/* 由任意生产者尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtMPSCQueueTryPush(xmpscqueue* pQueue, ptr pItem);



/* 由任意生产者批量压入；数组必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueuebatchresult xrtMPSCQueuePushBatch(
	xmpscqueue* pQueue,
	ptr const* pItems,
	size_t iCount
);



/* 由唯一消费者弹出；输出必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueueresult xrtMPSCQueueTryPop(xmpscqueue* pQueue, ptr* pItem);



/* 由唯一消费者批量弹出；数组必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueuebatchresult xrtMPSCQueuePopBatch(
	xmpscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
);



/* 返回包含已预留槽位的并发近似元素数量。 */
XRT_API size_t xrtMPSCQueueCount(const xmpscqueue* pQueue);



/* 在全部生产者停止后幂等关闭写入端。 */
XRT_API void xrtMPSCQueueClose(xmpscqueue* pQueue);



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtMPSCQueueIsClosed(const xmpscqueue* pQueue);



/* 判断队列是否已经关闭且排空。 */
XRT_API bool xrtMPSCQueueIsDrained(const xmpscqueue* pQueue);



/* 由唯一消费者排空元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtMPSCQueueDrain(
	xmpscqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
);



/* 在调用方独占且队列为空时重置全部序列槽并重新开放。 */
XRT_API bool xrtMPSCQueueReset(xmpscqueue* pQueue);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_QUEUE_MPMC)

/* MPMC 允许多个生产者和多个消费者并发访问同一个有界序列槽环。 */
typedef struct xmpmcqueue {
	xqueueslot* Slots;
	ptr Allocation;
	size_t Capacity;
	size_t Mask;
	xatomic32 Closed;
	xqueuecursor32 Tail;
	xqueuecursor32 Head;
} xmpmcqueue;



XRT_EXTERN_C_BEGIN



/* 初始化拥有内部序列槽环的多生产者多消费者队列。 */
XRT_API bool xrtMPMCQueueInit(xmpmcqueue* pQueue, size_t iCapacity);



/* 在调用方提供的 2 次幂序列槽环上初始化 MPMC 队列。 */
XRT_API bool xrtMPMCQueueInitBuffer(
	xmpmcqueue* pQueue,
	xqueueslot* pSlots,
	size_t iCapacity
);



/* 创建拥有结构和内部序列槽环的 MPMC 队列。 */
XRT_API xmpmcqueue* xrtMPMCQueueCreate(size_t iCapacity);



/* 释放拥有的序列槽环，但不释放队列结构或指针目标。 */
XRT_API void xrtMPMCQueueUnit(xmpmcqueue* pQueue);



/* 释放 Create 返回的队列结构和内部序列槽环。 */
XRT_API void xrtMPMCQueueDestroy(xmpmcqueue* pQueue);



/* 由任意生产者尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtMPMCQueueTryPush(xmpmcqueue* pQueue, ptr pItem);



/* 由任意生产者批量压入；数组必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueuebatchresult xrtMPMCQueuePushBatch(
	xmpmcqueue* pQueue,
	ptr const* pItems,
	size_t iCount
);



/* 由任意消费者弹出；输出必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueueresult xrtMPMCQueueTryPop(xmpmcqueue* pQueue, ptr* pItem);



/* 由任意消费者批量弹出；数组必须对齐且不与队列对象或槽环重叠。 */
XRT_API xqueuebatchresult xrtMPMCQueuePopBatch(
	xmpmcqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
);



/* 返回包含已预留生产和消费区间的并发近似元素数量。 */
XRT_API size_t xrtMPMCQueueCount(const xmpmcqueue* pQueue);



/* 在全部生产者停止后幂等关闭写入端。 */
XRT_API void xrtMPMCQueueClose(xmpmcqueue* pQueue);



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtMPMCQueueIsClosed(const xmpmcqueue* pQueue);



/* 判断队列是否已经关闭且没有尚未领取的元素。 */
XRT_API bool xrtMPMCQueueIsDrained(const xmpmcqueue* pQueue);



/* 由任意消费者排空元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtMPMCQueueDrain(
	xmpmcqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
);



/* 在调用方独占且队列为空时重置全部序列槽并重新开放。 */
XRT_API bool xrtMPMCQueueReset(xmpmcqueue* pQueue);



XRT_EXTERN_C_END

#endif

#endif
