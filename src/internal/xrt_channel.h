#ifndef XRT_INTERNAL_CHANNEL_H
#define XRT_INTERNAL_CHANNEL_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_CHANNEL)

#define XRT_CHANNEL_MAGIC 0x5843484Eu

#define XRT_CHANNEL_FLAG_CLOSED			0x00000001u
#define XRT_CHANNEL_FLAG_PENDING		0x00000002u
#define XRT_CHANNEL_FLAG_COMMITTED		0x00000004u
#define XRT_CHANNEL_FLAG_MASK			0x00000007u



#if defined(XRT_FEATURE_CHANNEL_SELECT) || \
	defined(XRT_FEATURE_CHANNEL_COROUTINE)
	#define XRT_INTERNAL_CHANNEL_WAIT
#endif



#if defined(XRT_INTERNAL_CHANNEL_WAIT)

#define XRT_CHANNEL_SELECT_NONE		UINT32_MAX
#define XRT_CHANNEL_SELECT_CLAIMING	(UINT32_MAX - 1u)
#define XRT_CHANNEL_SELECT_STOPPED	(UINT32_MAX - 2u)
#define XRT_CHANNEL_SELECT_MAX_INDEX	(UINT32_MAX - 3u)
#define XRT_CHANNEL_SELECT_INLINE_CASES 8u



typedef struct xrt_channel_select_state xrt_channel_select_state;
typedef struct xrt_channel_select_waiter xrt_channel_select_waiter;
typedef void (*xrt_channel_select_wake_proc)(ptr pData);
typedef bool (*xrt_channel_select_prepare_proc)(ptr pData);
typedef xwaitresult (*xrt_channel_select_wait_proc)(
	ptr pData,
	xdeadline iDeadline
);



/* 一次 Select 的共享状态只允许一个 case 或停止原因成为赢家。 */
struct xrt_channel_select_state {
	xatomic32 Winner;
	xrt_channel_select_wake_proc Wake;
	ptr WakeData;
	xwaitresult StopResult;
};



/* 每个 case 通过侵入节点挂入对应 Channel 的等待链。 */
struct xrt_channel_select_waiter {
	xrt_channel_select_waiter* Next;
	xrt_channel_select_state* Select;
	xchannel* Channel;
	ptr Value;
	ptr* Output;
	uint32 Index;
	xchannelop Operation;
	xchannelresult Result;
	bool Registered;
};

#endif



/* Channel 私有状态同时覆盖有缓冲队列和无缓冲 rendezvous。 */
typedef struct xrt_channel_impl {
	uint32 Magic;
	uint32 Flags;
	xmutex Mutex;
	xcond Readable;
	xcond Writable;
	ptr* Items;
	ptr Allocation;
	size_t Capacity;
	size_t Count;
	size_t Head;
	size_t Tail;
	ptr Pending;
	uint64 NextGeneration;
	uint64 CompletedGeneration;
	size_t ReadWaiters;
	size_t WriteWaiters;
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		xrt_channel_select_waiter* SelectWaiters;
	#else
		ptr SelectWaiters;
	#endif
	uint64 Epoch;
} xrt_channel_impl;



#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(
	sizeof(xrt_channel_impl) <= XRT_CHANNEL_STORAGE_SIZE,
	"XRT_CHANNEL_STORAGE_SIZE is too small"
);
#endif

#if defined(__cplusplus)
static_assert(
	sizeof(xrt_channel_impl) <= XRT_CHANNEL_STORAGE_SIZE,
	"XRT_CHANNEL_STORAGE_SIZE is too small"
);
#endif



/* 验证并锁定 Channel，成功后保证私有状态可安全使用。 */
xrt_channel_impl* __xrtChannelLock(xchannel* pChannel);



/* 解锁已经验证的 Channel。 */
void __xrtChannelUnlock(xrt_channel_impl* pImpl);



/* 检查接收输出没有覆盖 Channel 对象或内部指针环。 */
bool __xrtChannelOutputValid(
	const xrt_channel_impl* pImpl,
	const ptr* pItem
);



/* 在锁内执行一次非阻塞发送。 */
xchannelresult __xrtChannelTrySendLocked(
	xrt_channel_impl* pImpl,
	ptr pItem
);



/* 在锁内执行一次非阻塞接收。 */
xchannelresult __xrtChannelTryRecvLocked(
	xrt_channel_impl* pImpl,
	ptr* pItem
);



#if defined(XRT_INTERNAL_CHANNEL_WAIT)

/* 用调用方提供的等待驱动完成一次同步或协程 Select。 */
xchannelselectresult __xrtChannelSelectWait(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	ptr pCancel,
	xrt_channel_select_wake_proc pWake,
	xrt_channel_select_prepare_proc pPrepare,
	xrt_channel_select_wait_proc pWait,
	ptr pWaitData
);

/* 唤醒当前 Channel 上注册的全部 Select。 */
void __xrtChannelNotifySelectLocked(xrt_channel_impl* pImpl);



/* 尝试独占一次 Select 提交。 */
bool __xrtChannelSelectClaim(xrt_channel_select_waiter* pWaiter);



/* 发布一个已经完成的 Select case 并唤醒选择者。 */
void __xrtChannelSelectCommit(
	xrt_channel_select_waiter* pWaiter,
	xchannelresult iResult
);



/* 放弃尚未完成的临时 Select 提交权。 */
void __xrtChannelSelectAbort(xrt_channel_select_waiter* pWaiter);

#endif

#endif

#endif
