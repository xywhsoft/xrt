#ifndef XRT_INTERNAL_FUTURE_H
#define XRT_INTERNAL_FUTURE_H

#include "xrt_internal.h"
#include "xrt_cancel.h"



#if defined(XRT_FEATURE_FUTURE)

/* 内部等待节点由注册方保存，回调不得重新进入同一个 Future。 */
typedef struct xrt_future_waiter {
	struct xrt_future_waiter* Next;
	void (*Proc)(ptr pData);
	void (*Release)(ptr pData);
	ptr Data;
	xfuture* NotifyFuture;
	bool Linked;
	bool Calling;
	bool NotifyRelease;
	/* Explicit opt-in: the resident callbacks coordinate their own graph
	 * transitions and activity/refusal states. A trace alone does not prove it. */
	bool Phased;
	uint8 Certified; /* 0=opaque/traced, 1=direct Data owner, 2=projected owner. */
	/* Exact ownership released by Release(Data); NULL keeps old opaque nodes
	 * fail-closed. Fits the existing 64-byte public Watch storage on x64. */
	union {
		xrtownershiptrace OwnershipTrace;
		const xfuturewatchownershipv1* OwnershipPolicy;
		const xfuturewatchownershipv2* ProjectedOwnershipPolicy;
	};
} xrt_future_waiter;



#define XRT_FUTURE_WATCH_MAGIC UINT32_C(0x58574657)



/* 公开固定存储在内部等待节点后保存初始化标记。 */
typedef struct xrt_future_watch_impl {
	xrt_future_waiter Waiter;
	uint32 Magic;
} xrt_future_watch_impl;



typedef char xrt_future_watch_storage_check[
	(sizeof(xrt_future_watch_impl) <= XRT_FUTURE_WATCH_STORAGE_SIZE) ? 1 : -1
];



/* 读取公开 Watch 的内部布局。 */
static inline xrt_future_watch_impl* __xrtFutureWatchImpl(
	xfuturewatch* pWatch
)
{
	return (xrt_future_watch_impl*)pWatch;
}



/* Future 尚未完成时挂入等待节点，已完成时返回 false 且不设置错误。
 * Add/Detach/Remove 自己在 Future 锁外进入拥有转换 scope；Remove 的等待不占
 * mutation。默认回调仍以完整 scope 保守隔离；只有显式 Phased 注册的
 * 协作回调在 scope 外分发，并负责自身的计数、边转换和在途拒绝。 */
bool __xrtFutureWaiterAdd(xfuture* pFuture, xrt_future_waiter* pWaiter);



/* 只摘除仍挂接的等待节点，不等待已经开始的回调；返回是否实际摘除。 */
bool __xrtFutureWaiterDetach(xfuture* pFuture, xrt_future_waiter* pWaiter);



/* 移除仍然挂接的等待节点；返回时并发完成回调已经结束。 */
void __xrtFutureWaiterRemove(xfuture* pFuture, xrt_future_waiter* pWaiter);



#endif

#endif
