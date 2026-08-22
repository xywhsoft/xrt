#ifndef XRT_TYPED_QUEUE_H
#define XRT_TYPED_QUEUE_H

#include <xrt/queue.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE) && !defined(XRT_FEATURE_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE requires XRT_FEATURE_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE) && !defined(XRUNTIME_FEATURE_RUNTIME_TYPE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE requires XRUNTIME_FEATURE_RUNTIME_TYPE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_SPSC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC) && !defined(XRT_FEATURE_QUEUE_SPSC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_SPSC requires XRT_FEATURE_QUEUE_SPSC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRT_FEATURE_QUEUE_MPSC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRT_FEATURE_QUEUE_MPSC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC) && !defined(XRT_FEATURE_QUEUE_MPMC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPSC requires XRT_FEATURE_QUEUE_MPMC"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC) && !defined(XRUNTIME_FEATURE_TYPED_QUEUE)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPMC requires XRUNTIME_FEATURE_TYPED_QUEUE"
#endif

#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC) && !defined(XRT_FEATURE_QUEUE_MPMC)
	#error "XRUNTIME_FEATURE_TYPED_QUEUE_MPMC requires XRT_FEATURE_QUEUE_MPMC"
#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

/* 类型队列核心拥有固定值槽；公开结构只用于栈分配，不允许直接修改字段。 */
typedef struct xtypedqueuecore {
	const xrttype* ItemType;
	ptr Allocation;
	bytes Values;
	size_t Stride;
	size_t Capacity;
	size_t ValueBytes;
	xatomic32 State;
	xatomic32 Active;
} xtypedqueuecore;



/* 对象负载中的类型队列通过元数据声明固定容量。 */
typedef struct xtypedqueuemeta {
	size_t Capacity;
} xtypedqueuemeta;



/* 三种类型队列共享同一错误域和稳定错误代码。 */
typedef enum xtypedqueueerror {
	XTYPED_QUEUE_ERROR_ARGUMENT = 1,
	XTYPED_QUEUE_ERROR_TYPE,
	XTYPED_QUEUE_ERROR_LAYOUT,
	XTYPED_QUEUE_ERROR_OPERATION,
	XTYPED_QUEUE_ERROR_STATE
} xtypedqueueerror;

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC)

/* SPSC 类型队列由固定值槽、就绪环和反向空闲环组成。 */
typedef struct xtypedspscqueue {
	xtypedqueuecore Core;
	xspscqueue Ready;
	xspscqueue Free;
	xatomicptr PushCell;
	xatomicptr PopCell;
} xtypedspscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界单生产者单消费者类型队列。 */
XRT_API bool xrtTypedSPSCQueueInit(
	xtypedspscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedspscqueue* xrtTypedSPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue);
XRT_API void xrtTypedSPSCQueueDestroy(xtypedspscqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedSPSCQueueItemType(
	const xtypedspscqueue* pQueue
);
XRT_API size_t xrtTypedSPSCQueueCapacity(const xtypedspscqueue* pQueue);
XRT_API size_t xrtTypedSPSCQueueCount(const xtypedspscqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPush(
	xtypedspscqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedSPSCQueueTryPushTake(
	xtypedspscqueue* pQueue,
	ptr pItem
);



/* 移动弹出到已初始化输出；类型移动失败时元素仍由队列拥有。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPop(
	xtypedspscqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePushBatch(
	xtypedspscqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedSPSCQueuePopBatch(
	xtypedspscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedSPSCQueueClose(xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueIsClosed(const xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueIsDrained(const xtypedspscqueue* pQueue);
XRT_API bool xrtTypedSPSCQueueReset(xtypedspscqueue* pQueue);



/* 验证对象队列描述，并返回 SPSC 队列实例操作表。 */
XRT_API bool xrtTypedSPSCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedSPSCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC)

/* MPSC 类型队列允许多个生产者并发复制，值槽由 MPMC 空闲环回收。 */
typedef struct xtypedmpscqueue {
	xtypedqueuecore Core;
	xmpscqueue Ready;
	xmpmcqueue Free;
	xatomicptr PopCell;
} xtypedmpscqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界多生产者单消费者类型队列。 */
XRT_API bool xrtTypedMPSCQueueInit(
	xtypedmpscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedmpscqueue* xrtTypedMPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue);
XRT_API void xrtTypedMPSCQueueDestroy(xtypedmpscqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedMPSCQueueItemType(
	const xtypedmpscqueue* pQueue
);
XRT_API size_t xrtTypedMPSCQueueCapacity(const xtypedmpscqueue* pQueue);
XRT_API size_t xrtTypedMPSCQueueCount(const xtypedmpscqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPush(
	xtypedmpscqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedMPSCQueueTryPushTake(
	xtypedmpscqueue* pQueue,
	ptr pItem
);



/* 由唯一消费者移动弹出；类型移动失败时元素仍由队列拥有。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPop(
	xtypedmpscqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePushBatch(
	xtypedmpscqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedMPSCQueuePopBatch(
	xtypedmpscqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedMPSCQueueClose(xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueIsClosed(const xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueIsDrained(const xtypedmpscqueue* pQueue);
XRT_API bool xrtTypedMPSCQueueReset(xtypedmpscqueue* pQueue);



/* 验证对象队列描述，并返回 MPSC 队列实例操作表。 */
XRT_API bool xrtTypedMPSCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedMPSCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC)

/* MPMC 类型队列以独立就绪、空闲和失败重试环保存固定值槽。 */
typedef struct xtypedmpmcqueue {
	xtypedqueuecore Core;
	xmpmcqueue Ready;
	xmpmcqueue Free;
	xmpmcqueue Retry;
} xtypedmpmcqueue;



XRT_EXTERN_C_BEGIN



/* 初始化、创建、结束或销毁一个有界多生产者多消费者类型队列。 */
XRT_API bool xrtTypedMPMCQueueInit(
	xtypedmpmcqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API xtypedmpmcqueue* xrtTypedMPMCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
);
XRT_API void xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue);
XRT_API void xrtTypedMPMCQueueDestroy(xtypedmpmcqueue* pQueue);



/* 返回借用元素类型、实际 2 次幂容量和并发近似元素数。 */
XRT_API const xrttype* xrtTypedMPMCQueueItemType(
	const xtypedmpmcqueue* pQueue
);
XRT_API size_t xrtTypedMPMCQueueCapacity(const xtypedmpmcqueue* pQueue);
XRT_API size_t xrtTypedMPMCQueueCount(const xtypedmpmcqueue* pQueue);



/* 复制或移动压入一个值；满、关闭和错误使用 xqueueresult 区分。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPush(
	xtypedmpmcqueue* pQueue,
	const void* pItem
);
XRT_API xqueueresult xrtTypedMPMCQueueTryPushTake(
	xtypedmpmcqueue* pQueue,
	ptr pItem
);



/* 由任意消费者移动弹出；类型移动失败时元素进入内部重试环。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPop(
	xtypedmpmcqueue* pQueue,
	ptr pValue
);



/* 批量处理连续类型值；部分成功返回 OK 和实际处理数量。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePushBatch(
	xtypedmpmcqueue* pQueue,
	const void* pItems,
	size_t iCount
);
XRT_API xqueuebatchresult xrtTypedMPMCQueuePopBatch(
	xtypedmpmcqueue* pQueue,
	ptr pValues,
	size_t iCapacity
);



/* 关闭写端、查询终态，或在独占且排空后重新开放。 */
XRT_API void xrtTypedMPMCQueueClose(xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueIsClosed(const xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueIsDrained(const xtypedmpmcqueue* pQueue);
XRT_API bool xrtTypedMPMCQueueReset(xtypedmpmcqueue* pQueue);



/* 验证对象队列描述，并返回 MPMC 队列实例操作表。 */
XRT_API bool xrtTypedMPMCQueueTypeValidate(const xrttype* pType);
XRT_API const xrtinstanceops* xrtTypedMPMCQueueInstanceOps(void);



XRT_EXTERN_C_END

#endif

#endif
