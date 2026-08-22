#ifndef XRT_CHANNEL_H
#define XRT_CHANNEL_H

#include <xrt/core.h>
#include <xrt/sync.h>

#if defined(XRT_FEATURE_CHANNEL_CANCEL) || \
	defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)
	#include <xrt/cancel.h>
#endif

#if defined(XRT_FEATURE_CHANNEL_COROUTINE)
	#include <xrt/coroutine.h>
#endif



#if defined(XRT_FEATURE_CHANNEL) && !defined(XRT_FEATURE_COND)
	#error "XRT_FEATURE_CHANNEL requires XRT_FEATURE_COND"
#endif

#if defined(XRT_FEATURE_CHANNEL_CANCEL) && !defined(XRT_FEATURE_CHANNEL)
	#error "XRT_FEATURE_CHANNEL_CANCEL requires XRT_FEATURE_CHANNEL"
#endif

#if defined(XRT_FEATURE_CHANNEL_CANCEL) && !defined(XRT_FEATURE_CANCEL)
	#error "XRT_FEATURE_CHANNEL_CANCEL requires XRT_FEATURE_CANCEL"
#endif

#if defined(XRT_FEATURE_CHANNEL_SELECT) && !defined(XRT_FEATURE_CHANNEL)
	#error "XRT_FEATURE_CHANNEL_SELECT requires XRT_FEATURE_CHANNEL"
#endif

#if defined(XRT_FEATURE_CHANNEL_SELECT) && !defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_CHANNEL_SELECT requires XRT_FEATURE_ATOMIC"
#endif

#if defined(XRT_FEATURE_CHANNEL_SELECT) && !defined(XRT_FEATURE_EVENT)
	#error "XRT_FEATURE_CHANNEL_SELECT requires XRT_FEATURE_EVENT"
#endif

#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL) && \
	!defined(XRT_FEATURE_CHANNEL_SELECT)
	#error "XRT_FEATURE_CHANNEL_SELECT_CANCEL requires XRT_FEATURE_CHANNEL_SELECT"
#endif

#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL) && \
	!defined(XRT_FEATURE_CANCEL)
	#error "XRT_FEATURE_CHANNEL_SELECT_CANCEL requires XRT_FEATURE_CANCEL"
#endif

#if defined(XRT_FEATURE_CHANNEL_COROUTINE) && \
	!defined(XRT_FEATURE_CHANNEL)
	#error "XRT_FEATURE_CHANNEL_COROUTINE requires XRT_FEATURE_CHANNEL"
#endif

#if defined(XRT_FEATURE_CHANNEL_COROUTINE) && \
	!defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_CHANNEL_COROUTINE requires XRT_FEATURE_ATOMIC"
#endif

#if defined(XRT_FEATURE_CHANNEL_COROUTINE) && \
	!defined(XRT_FEATURE_COROUTINE_SCHEDULER)
	#error "XRT_FEATURE_CHANNEL_COROUTINE requires XRT_FEATURE_COROUTINE_SCHEDULER"
#endif



#if defined(XRT_FEATURE_CHANNEL)

/*
 * Channel 使用不透明的固定存储隐藏同步状态。
 * Windows 与 POSIX 分别预留后续 select 适配所需的内部空间。
 */
#if defined(_WIN32) || defined(_WIN64)
	#define XRT_CHANNEL_STORAGE_SIZE 192u
#else
	#define XRT_CHANNEL_STORAGE_SIZE 384u
#endif



/* Channel 非阻塞结果把正常流控状态与真正错误分开表达。 */
typedef enum xchannelresult {
	XCHANNEL_ERROR = -1,
	XCHANNEL_OK = 0,
	XCHANNEL_EMPTY = 1,
	XCHANNEL_FULL = 2,
	XCHANNEL_CLOSED = 3
} xchannelresult;



/* Channel 保存不透明同步状态，允许嵌入调用方结构。 */
typedef union xchannel {
	uint64 Alignment;
	uint8 Storage[XRT_CHANNEL_STORAGE_SIZE];
} xchannel;



/* 排空回调接收已从 Channel 移除的指针值。 */
typedef void (*xchanneldrainfn)(ptr pItem, ptr pContext);



#if defined(XRT_FEATURE_CHANNEL_SELECT) || \
	defined(XRT_FEATURE_CHANNEL_COROUTINE)

/* Select case 明确区分发送输入和接收输出。 */
typedef enum xchannelop {
	XCHANNEL_OP_RECV = 0,
	XCHANNEL_OP_SEND = 1
} xchannelop;



/* 一个 Select case 只描述操作，不持有 Channel 或消息的所有权。 */
typedef struct xchannelcase {
	xchannel* Channel;
	xchannelop Operation;
	ptr Value;
	ptr* Output;
} xchannelcase;



/* Select 结果同时表达等待状态、被选索引和该 Channel 操作结果。 */
typedef struct xchannelselectresult {
	xwaitresult Wait;
	size_t Index;
	xchannelresult Result;
} xchannelselectresult;



/* 没有 case 被选中时使用无效索引。 */
#define XCHANNEL_SELECT_NONE SIZE_MAX

#endif



XRT_EXTERN_C_BEGIN



/* 初始化精确容量的 Channel；容量为零时创建同步 rendezvous Channel。 */
XRT_API bool xrtChannelInit(xchannel* pChannel, size_t iCapacity);



/* 在调用方提供的精确容量指针环上初始化有缓冲 Channel。 */
XRT_API bool xrtChannelInitBuffer(
	xchannel* pChannel,
	ptr* pItems,
	size_t iCapacity
);



/* 创建精确容量的 Channel；容量为零时不分配消息缓冲。 */
XRT_API xchannel* xrtChannelCreate(size_t iCapacity);



/* 释放 Channel 内部资源；仍有等待者或 rendezvous 消息时失败。 */
XRT_API bool xrtChannelUnit(xchannel* pChannel);



/* 释放 Create 返回的 Channel；Unit 失败时保留对象。 */
XRT_API bool xrtChannelDestroy(xchannel* pChannel);



/* 非阻塞发送一个可为空的指针值。 */
XRT_API xchannelresult xrtChannelTrySend(xchannel* pChannel, ptr pItem);



/* 等待发送一个可为空的指针值。 */
XRT_API xwaitresult xrtChannelSend(xchannel* pChannel, ptr pItem);



/* 在相对微秒数内等待发送一个指针值。 */
XRT_API xwaitresult xrtChannelSendFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
);



/* 等待发送一个指针值到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtChannelSendUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
);



/* 非阻塞接收；输出必须对齐且不能覆盖 Channel 或内部指针环。 */
XRT_API xchannelresult xrtChannelTryRecv(
	xchannel* pChannel,
	ptr* pItem
);



/* 等待接收一个指针值。 */
XRT_API xwaitresult xrtChannelRecv(xchannel* pChannel, ptr* pItem);



/* 在相对微秒数内等待接收一个指针值。 */
XRT_API xwaitresult xrtChannelRecvFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
);



/* 等待接收一个指针值到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtChannelRecvUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
);



#if defined(XRT_FEATURE_CHANNEL_CANCEL)

/* 无限等待发送，并允许取消令牌中断尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendCancel(
	xchannel* pChannel,
	ptr pItem,
	xcancel* pCancel
);



/* 在相对微秒数内等待发送，并允许取消令牌中断尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendForCancel(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout,
	xcancel* pCancel
);



/* 等待发送到截止时间，并允许取消令牌中断尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendUntilCancel(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 无限等待接收，并允许取消令牌中断尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvCancel(
	xchannel* pChannel,
	ptr* pItem,
	xcancel* pCancel
);



/* 在相对微秒数内等待接收，并允许取消令牌中断尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvForCancel(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout,
	xcancel* pCancel
);



/* 等待接收到截止时间，并允许取消令牌中断尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvUntilCancel(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);

#endif



/* 返回有缓冲 Channel 的精确元素数量；同步 Channel 始终返回零。 */
XRT_API size_t xrtChannelCount(xchannel* pChannel);



/* 返回创建时指定的精确容量。 */
XRT_API size_t xrtChannelCapacity(xchannel* pChannel);



/* 判断发送端是否已经关闭。 */
XRT_API bool xrtChannelIsClosed(xchannel* pChannel);



/* 判断 Channel 是否已经关闭且没有可接收值。 */
XRT_API bool xrtChannelIsDrained(xchannel* pChannel);



/* 幂等关闭发送端；已有缓冲值仍可继续接收。 */
XRT_API void xrtChannelClose(xchannel* pChannel);



/* 排空调用开始时已有的值；用户回调在 Channel 锁外执行。 */
XRT_API size_t xrtChannelDrain(
	xchannel* pChannel,
	xchanneldrainfn pDrain,
	ptr pContext
);



/* 在独占、无等待者且为空时重置并重新开放 Channel。 */
XRT_API bool xrtChannelReset(xchannel* pChannel);



#if defined(XRT_FEATURE_CHANNEL_SELECT) || \
	defined(XRT_FEATURE_CHANNEL_COROUTINE)

/* 构造一个发送 case。 */
XRT_API xchannelcase xrtChannelCaseSend(
	xchannel* pChannel,
	ptr pItem
);



/* 构造一个接收 case。 */
XRT_API xchannelcase xrtChannelCaseRecv(
	xchannel* pChannel,
	ptr* pItem
);

#endif



#if defined(XRT_FEATURE_CHANNEL_SELECT)



/* 公平地尝试全部 case，不可立即提交时返回 TIMEOUT 和无效索引。 */
XRT_API xchannelselectresult xrtChannelSelectTry(
	const xchannelcase* pCases,
	size_t iCount
);



/* 等待任意一个 case 原子提交。 */
XRT_API xchannelselectresult xrtChannelSelect(
	const xchannelcase* pCases,
	size_t iCount
);



/* 在相对微秒数内等待任意一个 case 原子提交。 */
XRT_API xchannelselectresult xrtChannelSelectFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
);



/* 等待任意一个 case 原子提交到指定单调时钟截止时间。 */
XRT_API xchannelselectresult xrtChannelSelectUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
);

#endif



#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)

/* 等待任意 case 提交，并允许取消令牌中断未提交的选择。 */
XRT_API xchannelselectresult xrtChannelSelectUntilCancel(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
);

#endif



#if defined(XRT_FEATURE_CHANNEL_COROUTINE)

/* 在当前调度协程中挂起发送，不阻塞调度线程。 */
XRT_API xwaitresult xrtChannelSendAwait(
	xchannel* pChannel,
	ptr pItem
);



/* 在当前调度协程中挂起发送，直到相对期限结束。 */
XRT_API xwaitresult xrtChannelSendAwaitFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
);



/* 在当前调度协程中挂起发送，直到绝对截止时间。 */
XRT_API xwaitresult xrtChannelSendAwaitUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
);



/* 在当前调度协程中挂起接收，不阻塞调度线程。 */
XRT_API xwaitresult xrtChannelRecvAwait(
	xchannel* pChannel,
	ptr* pItem
);



/* 在当前调度协程中挂起接收，直到相对期限结束。 */
XRT_API xwaitresult xrtChannelRecvAwaitFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
);



/* 在当前调度协程中挂起接收，直到绝对截止时间。 */
XRT_API xwaitresult xrtChannelRecvAwaitUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
);



/* 在当前调度协程中挂起，直到任意一个 case 原子提交。 */
XRT_API xchannelselectresult xrtChannelSelectAwait(
	const xchannelcase* pCases,
	size_t iCount
);



/* 在当前调度协程中挂起，直到任意 case 提交或相对期限结束。 */
XRT_API xchannelselectresult xrtChannelSelectAwaitFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
);



/* 在当前调度协程中挂起，直到任意 case 提交或到达截止时间。 */
XRT_API xchannelselectresult xrtChannelSelectAwaitUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
);

#endif



XRT_EXTERN_C_END

#endif

#endif
