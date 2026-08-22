#include "../internal/xrt_channel.h"



#if defined(XRT_FEATURE_CHANNEL)

/* 把公开不透明存储转换为私有 Channel 状态。 */
static xrt_channel_impl* __xrtChannelImpl(xchannel* pChannel)
{
	return (xrt_channel_impl*)pChannel;
}



/* 检查 Channel 地址满足私有状态的最低对齐要求。 */
static bool __xrtChannelAddressValid(const xchannel* pChannel)
{
	return
		(pChannel != NULL) &&
		(((uintptr_t)pChannel & (sizeof(uint64) - 1u)) == 0);
}



/* 检查有缓冲 Channel 的指针环地址和长度可以安全访问。 */
static bool __xrtChannelBufferValid(
	const xchannel* pChannel,
	const ptr* pItems,
	size_t iCapacity
)
{
	size_t iBytes;

	if (
		!__xrtChannelAddressValid(pChannel) ||
		(pItems == NULL) ||
		(((uintptr_t)pItems & (sizeof(ptr) - 1u)) != 0) ||
		(iCapacity == 0) ||
		(iCapacity > (SIZE_MAX / sizeof(ptr)))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	/* 拒绝回绕地址和覆盖 Channel 状态的外部环。 */
	iBytes = iCapacity * sizeof(ptr);
	if (
		((uintptr_t)pItems > (UINTPTR_MAX - iBytes)) ||
		((uintptr_t)pChannel > (UINTPTR_MAX - sizeof(xchannel))) ||
		__xrtRangesOverlap(
			pChannel,
			sizeof(xchannel),
			pItems,
			iBytes
		)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 检查锁内状态满足有缓冲或 rendezvous 模式的不变量。 */
static bool __xrtChannelStateValid(const xrt_channel_impl* pImpl)
{
	size_t iExpectedTail;
	size_t iBytes;

	if (
		(pImpl == NULL) ||
		(pImpl->Magic != XRT_CHANNEL_MAGIC) ||
		((pImpl->Flags & ~XRT_CHANNEL_FLAG_MASK) != 0) ||
		(pImpl->CompletedGeneration > pImpl->NextGeneration)
	) {
		return false;
	}
	#if !defined(XRT_INTERNAL_CHANNEL_WAIT)
		if ( pImpl->SelectWaiters != NULL ) {
			return false;
		}
	#endif

	/* 无缓冲模式只允许一个由代次标识的 rendezvous 值。 */
	if ( pImpl->Capacity == 0 ) {
		if (
			(pImpl->Items != NULL) ||
			(pImpl->Allocation != NULL) ||
			(pImpl->Count != 0) ||
			(pImpl->Head != 0) ||
			(pImpl->Tail != 0) ||
			(((pImpl->Flags & XRT_CHANNEL_FLAG_COMMITTED) != 0) &&
			 ((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) == 0))
		) {
			return false;
		}
		if (
			((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) &&
			(
				(pImpl->NextGeneration == 0) ||
				(pImpl->CompletedGeneration >= pImpl->NextGeneration)
			)
		) {
			return false;
		}
		if (
			((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) == 0) &&
			(pImpl->Pending != NULL)
		) {
			return false;
		}
		return true;
	}

	/* 有缓冲模式不使用 rendezvous 字段。 */
	if (
		(pImpl->Items == NULL) ||
		(((uintptr_t)pImpl->Items & (sizeof(ptr) - 1u)) != 0) ||
		(pImpl->Count > pImpl->Capacity) ||
		(pImpl->Head >= pImpl->Capacity) ||
		(pImpl->Tail >= pImpl->Capacity) ||
		((pImpl->Flags &
		  (XRT_CHANNEL_FLAG_PENDING | XRT_CHANNEL_FLAG_COMMITTED)) != 0) ||
		(pImpl->Pending != NULL)
	) {
		return false;
	}
	if ( pImpl->Capacity > (SIZE_MAX / sizeof(ptr)) ) {
		return false;
	}

	/* 指针环必须完整、与对象分离，并与计数对应。 */
	iBytes = pImpl->Capacity * sizeof(ptr);
	if (
		((uintptr_t)pImpl->Items > (UINTPTR_MAX - iBytes)) ||
		((uintptr_t)pImpl > (UINTPTR_MAX - sizeof(xchannel))) ||
		__xrtRangesOverlap(
			pImpl,
			sizeof(xchannel),
			pImpl->Items,
			iBytes
		) ||
		((pImpl->Allocation != NULL) &&
		 (pImpl->Allocation != (ptr)pImpl->Items))
	) {
		return false;
	}
	iExpectedTail = pImpl->Head + pImpl->Count;
	if ( iExpectedTail >= pImpl->Capacity ) {
		iExpectedTail -= pImpl->Capacity;
	}
	return iExpectedTail == pImpl->Tail;
}



/* 验证并锁定 Channel，成功后保证私有状态可安全使用。 */
xrt_channel_impl* __xrtChannelLock(xchannel* pChannel)
{
	xrt_channel_impl* pImpl;

	if ( !__xrtChannelAddressValid(pChannel) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtChannelImpl(pChannel);
	if ( pImpl->Magic != XRT_CHANNEL_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( !xrtMutexLock(&pImpl->Mutex) ) {
		return NULL;
	}
	if ( !__xrtChannelStateValid(pImpl) ) {
		(void)xrtMutexUnlock(&pImpl->Mutex);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 解锁已经验证的 Channel。 */
void __xrtChannelUnlock(xrt_channel_impl* pImpl)
{
	(void)xrtMutexUnlock(&pImpl->Mutex);
}



/* 检查接收输出没有覆盖 Channel 对象或内部指针环。 */
bool __xrtChannelOutputValid(
	const xrt_channel_impl* pImpl,
	const ptr* pItem
)
{
	size_t iBytes;

	if (
		(pItem == NULL) ||
		(((uintptr_t)pItem & (sizeof(ptr) - 1u)) != 0) ||
		((uintptr_t)pItem > (UINTPTR_MAX - sizeof(ptr))) ||
		__xrtRangesOverlap(
			pImpl,
			sizeof(xchannel),
			pItem,
			sizeof(ptr)
		)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pImpl->Capacity == 0 ) {
		return true;
	}

	/* 有缓冲模式还要拒绝写回内部环。 */
	iBytes = pImpl->Capacity * sizeof(ptr);
	if (
		__xrtRangesOverlap(
			pImpl->Items,
			iBytes,
			pItem,
			sizeof(ptr)
		)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 初始化已经完成参数检查的 Channel 状态。 */
static bool __xrtChannelSetup(
	xchannel* pChannel,
	ptr* pItems,
	size_t iCapacity,
	ptr pAllocation
)
{
	xrt_channel_impl* pImpl;

	memset(pChannel, 0, sizeof(xchannel));
	pImpl = __xrtChannelImpl(pChannel);
	if ( !xrtMutexInit(&pImpl->Mutex) ) {
		return false;
	}
	if ( !xrtCondInit(&pImpl->Readable) ) {
		(void)xrtMutexUnit(&pImpl->Mutex);
		memset(pChannel, 0, sizeof(xchannel));
		return false;
	}
	if ( !xrtCondInit(&pImpl->Writable) ) {
		(void)xrtCondUnit(&pImpl->Readable);
		(void)xrtMutexUnit(&pImpl->Mutex);
		memset(pChannel, 0, sizeof(xchannel));
		return false;
	}

	/* Magic 最后发布，避免半初始化对象通过公共校验。 */
	pImpl->Items = pItems;
	pImpl->Allocation = pAllocation;
	pImpl->Capacity = iCapacity;
	pImpl->Magic = XRT_CHANNEL_MAGIC;
	return true;
}



/* 返回环中指定位置的下一个索引。 */
static size_t __xrtChannelNext(size_t iIndex, size_t iCapacity)
{
	iIndex++;
	return iIndex == iCapacity ? 0 : iIndex;
}



/* 为新的 rendezvous 值分配不可回绕的代次。 */
static uint64 __xrtChannelNextGeneration(xrt_channel_impl* pImpl)
{
	if ( pImpl->NextGeneration == UINT64_MAX ) {
		__xrtErrorSetSizeOverflow();
		return 0;
	}
	pImpl->NextGeneration++;
	return pImpl->NextGeneration;
}



/* 在无缓冲 Channel 中发布一个 rendezvous 值。 */
static bool __xrtChannelOffer(
	xrt_channel_impl* pImpl,
	ptr pItem,
	bool bCommitted
)
{
	if ( __xrtChannelNextGeneration(pImpl) == 0 ) {
		return false;
	}
	pImpl->Pending = pItem;
	pImpl->Flags |= XRT_CHANNEL_FLAG_PENDING;
	if ( bCommitted ) {
		pImpl->Flags |= XRT_CHANNEL_FLAG_COMMITTED;
	} else {
		pImpl->Flags &= ~XRT_CHANNEL_FLAG_COMMITTED;
	}
	pImpl->Epoch++;
	(void)xrtCondSignal(&pImpl->Readable);
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		__xrtChannelNotifySelectLocked(pImpl);
	#endif
	return true;
}



/* 接收并完成当前 rendezvous 值。 */
static void __xrtChannelTakePending(
	xrt_channel_impl* pImpl,
	ptr* pItem
)
{
	*pItem = pImpl->Pending;
	pImpl->Pending = NULL;
	pImpl->CompletedGeneration = pImpl->NextGeneration;
	pImpl->Flags &=
		~(XRT_CHANNEL_FLAG_PENDING | XRT_CHANNEL_FLAG_COMMITTED);
	pImpl->Epoch++;

	/* 同时唤醒值的发送者和等待发布下一个值的发送者。 */
	(void)xrtCondBroadcast(&pImpl->Writable);
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		__xrtChannelNotifySelectLocked(pImpl);
	#endif
}



/* 撤回尚未提交的 rendezvous 值。 */
static void __xrtChannelWithdraw(xrt_channel_impl* pImpl)
{
	pImpl->Pending = NULL;
	pImpl->Flags &=
		~(XRT_CHANNEL_FLAG_PENDING | XRT_CHANNEL_FLAG_COMMITTED);
	pImpl->Epoch++;
	(void)xrtCondBroadcast(&pImpl->Writable);
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		__xrtChannelNotifySelectLocked(pImpl);
	#endif
}



#if defined(XRT_INTERNAL_CHANNEL_WAIT)

/* 把无缓冲发送直接提交给一个尚未选择其他 case 的接收者。 */
static bool __xrtChannelMatchSelectRecvLocked(
	xrt_channel_impl* pImpl,
	ptr pItem
)
{
	xrt_channel_select_waiter* pWaiter = pImpl->SelectWaiters;

	while ( pWaiter != NULL ) {
		if (
			(pWaiter->Operation == XCHANNEL_OP_RECV) &&
			__xrtChannelSelectClaim(pWaiter)
		) {
			*pWaiter->Output = pItem;
			__xrtChannelSelectCommit(pWaiter, XCHANNEL_OK);
			return true;
		}
		pWaiter = pWaiter->Next;
	}
	return false;
}



/* 把一个等待发送的 Select case 直接交给无缓冲接收者。 */
static bool __xrtChannelMatchSelectSendLocked(
	xrt_channel_impl* pImpl,
	ptr* pItem
)
{
	xrt_channel_select_waiter* pWaiter = pImpl->SelectWaiters;

	while ( pWaiter != NULL ) {
		if (
			(pWaiter->Operation == XCHANNEL_OP_SEND) &&
			__xrtChannelSelectClaim(pWaiter)
		) {
			*pItem = pWaiter->Value;
			__xrtChannelSelectCommit(pWaiter, XCHANNEL_OK);
			return true;
		}
		pWaiter = pWaiter->Next;
	}
	return false;
}

#endif



/* 在锁内执行一次非阻塞发送。 */
xchannelresult __xrtChannelTrySendLocked(
	xrt_channel_impl* pImpl,
	ptr pItem
)
{
	if ( (pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0 ) {
		return XCHANNEL_CLOSED;
	}

	/* 无缓冲发送只有在接收者已经等待时才能立即提交。 */
	if ( pImpl->Capacity == 0 ) {
		if ( (pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0 ) {
			return XCHANNEL_FULL;
		}
		if ( pImpl->ReadWaiters != 0 ) {
			if ( !__xrtChannelOffer(pImpl, pItem, true) ) {
				return XCHANNEL_ERROR;
			}
			return XCHANNEL_OK;
		}
		#if defined(XRT_INTERNAL_CHANNEL_WAIT)
			if ( __xrtChannelMatchSelectRecvLocked(pImpl, pItem) ) {
				pImpl->Epoch++;
				__xrtChannelNotifySelectLocked(pImpl);
				return XCHANNEL_OK;
			}
		#endif
		return XCHANNEL_FULL;
	}

	/* 有缓冲发送保持创建时指定的精确容量。 */
	if ( pImpl->Count == pImpl->Capacity ) {
		return XCHANNEL_FULL;
	}
	pImpl->Items[pImpl->Tail] = pItem;
	pImpl->Tail = __xrtChannelNext(pImpl->Tail, pImpl->Capacity);
	pImpl->Count++;
	pImpl->Epoch++;
	(void)xrtCondSignal(&pImpl->Readable);
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		__xrtChannelNotifySelectLocked(pImpl);
	#endif
	return XCHANNEL_OK;
}



/* 在锁内执行一次非阻塞接收。 */
xchannelresult __xrtChannelTryRecvLocked(
	xrt_channel_impl* pImpl,
	ptr* pItem
)
{
	if (
		(pImpl->Capacity == 0) &&
		((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0)
	) {
		__xrtChannelTakePending(pImpl, pItem);
		return XCHANNEL_OK;
	}
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		if (
			(pImpl->Capacity == 0) &&
			__xrtChannelMatchSelectSendLocked(pImpl, pItem)
		) {
			pImpl->Epoch++;
			__xrtChannelNotifySelectLocked(pImpl);
			return XCHANNEL_OK;
		}
	#endif
	if ( (pImpl->Capacity != 0) && (pImpl->Count != 0) ) {
		*pItem = pImpl->Items[pImpl->Head];
		pImpl->Items[pImpl->Head] = NULL;
		pImpl->Head = __xrtChannelNext(pImpl->Head, pImpl->Capacity);
		pImpl->Count--;
		pImpl->Epoch++;
		(void)xrtCondSignal(&pImpl->Writable);
		#if defined(XRT_INTERNAL_CHANNEL_WAIT)
			__xrtChannelNotifySelectLocked(pImpl);
		#endif
		return XCHANNEL_OK;
	}

	*pItem = NULL;
	return (pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0 ?
		XCHANNEL_CLOSED : XCHANNEL_EMPTY;
}



/* 把非阻塞结果映射为等待结果。 */
static xwaitresult __xrtChannelWaitResult(xchannelresult iResult)
{
	switch ( iResult ) {
		case XCHANNEL_OK:
			return XWAIT_OK;
		case XCHANNEL_CLOSED:
			return XWAIT_CLOSED;
		case XCHANNEL_ERROR:
			return XWAIT_ERROR;
		default:
			return XWAIT_TIMEOUT;
	}
}



/* 检查尚未提交的等待是否已取消或到达截止时间。 */
static xwaitresult __xrtChannelStop(
	xdeadline iDeadline,
	ptr pCancel
)
{
	#if defined(XRT_FEATURE_CHANNEL_CANCEL)
		if (
			(pCancel != NULL) &&
			xrtCancelRequested((xcancel*)pCancel)
		) {
			return XWAIT_CANCELLED;
		}
	#else
		(void)pCancel;
	#endif
	return xrtDeadlineExpired(iDeadline) ? XWAIT_TIMEOUT : XWAIT_OK;
}



/* 等待指定条件变量，并保留锁内重新检查谓词的机会。 */
static xwaitresult __xrtChannelWaitCondition(
	xcond* pCond,
	xmutex* pMutex,
	xdeadline iDeadline
)
{
	return iDeadline == XRT_DEADLINE_NEVER ?
		xrtCondWait(pCond, pMutex) :
		xrtCondWaitUntil(pCond, pMutex, iDeadline);
}



/* 在锁内完成有缓冲或 rendezvous 发送等待。 */
static xwaitresult __xrtChannelSendWait(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline,
	ptr pCancel
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	xwaitresult iResult = XWAIT_ERROR;
	uint64 iGeneration = 0;

	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	pImpl->WriteWaiters++;

	/* 循环只在谓词不满足时休眠，虚假唤醒不会改变结果。 */
	for ( ;; ) {
		if ( pImpl->Capacity != 0 ) {
			xchannelresult iTry = __xrtChannelTrySendLocked(pImpl, pItem);

			if ( iTry != XCHANNEL_FULL ) {
				iResult = __xrtChannelWaitResult(iTry);
				break;
			}
		} else if ( iGeneration == 0 ) {
			if ( (pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0 ) {
				iResult = XWAIT_CLOSED;
				break;
			}
			if ( (pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) == 0 ) {
				bool bReady = pImpl->ReadWaiters != 0;

				/* 已等待的接收者优先于同时到达的取消或超时。 */
				iResult = bReady ?
					XWAIT_OK :
					__xrtChannelStop(iDeadline, pCancel);
				if ( iResult != XWAIT_OK ) {
					break;
				}
				if ( !__xrtChannelOffer(pImpl, pItem, bReady) ) {
					iResult = XWAIT_ERROR;
					break;
				}
				if ( bReady ) {
					iResult = XWAIT_OK;
					break;
				}
				iGeneration = pImpl->NextGeneration;
				continue;
			}
		} else {
			bool bOwnPending =
				((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) &&
				(pImpl->NextGeneration == iGeneration);
			bool bCommitted =
				bOwnPending &&
				((pImpl->Flags & XRT_CHANNEL_FLAG_COMMITTED) != 0);

			if ( pImpl->CompletedGeneration >= iGeneration ) {
				iResult = XWAIT_OK;
				break;
			}
			if (
				((pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0) &&
				!bCommitted
			) {
				if ( bOwnPending ) {
					__xrtChannelWithdraw(pImpl);
				}
				iResult = XWAIT_CLOSED;
				break;
			}

			/* 配对提交后忽略取消，避免失败返回与对端收值同时发生。 */
			if ( !bCommitted ) {
				iResult = __xrtChannelStop(iDeadline, pCancel);
				if ( iResult != XWAIT_OK ) {
					if ( bOwnPending ) {
						__xrtChannelWithdraw(pImpl);
					}
					break;
				}
			}
		}

		/* 有缓冲满队列和 rendezvous 发送者共用可写通知。 */
		if (
			(pImpl->Capacity != 0) ||
			(iGeneration == 0)
		) {
			iResult = __xrtChannelStop(iDeadline, pCancel);
			if ( iResult != XWAIT_OK ) {
				break;
			}
		}
		iResult = __xrtChannelWaitCondition(
			&pImpl->Writable,
			&pImpl->Mutex,
			iDeadline
		);
		if ( iResult == XWAIT_ERROR ) {
			if (
				(iGeneration != 0) &&
				((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) &&
				(pImpl->NextGeneration == iGeneration) &&
				((pImpl->Flags & XRT_CHANNEL_FLAG_COMMITTED) == 0)
			) {
				__xrtChannelWithdraw(pImpl);
			} else if (
				(iGeneration != 0) &&
				((pImpl->Flags & XRT_CHANNEL_FLAG_COMMITTED) != 0)
			) {
				iResult = XWAIT_OK;
			}
			break;
		}
	}

	pImpl->WriteWaiters--;
	__xrtChannelUnlock(pImpl);
	return iResult;
}



/* 在锁内完成有缓冲或 rendezvous 接收等待。 */
static xwaitresult __xrtChannelRecvWait(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline,
	ptr pCancel
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	xwaitresult iResult = XWAIT_ERROR;

	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	if ( !__xrtChannelOutputValid(pImpl, pItem) ) {
		__xrtChannelUnlock(pImpl);
		return XWAIT_ERROR;
	}
	*pItem = NULL;
	pImpl->ReadWaiters++;
	if ( pImpl->Capacity == 0 ) {
		(void)xrtCondBroadcast(&pImpl->Writable);
		#if defined(XRT_INTERNAL_CHANNEL_WAIT)
			__xrtChannelNotifySelectLocked(pImpl);
		#endif
	}

	/* 可接收值优先于关闭、取消和截止时间。 */
	for ( ;; ) {
		xchannelresult iTry = __xrtChannelTryRecvLocked(pImpl, pItem);

		if ( iTry != XCHANNEL_EMPTY ) {
			iResult = __xrtChannelWaitResult(iTry);
			break;
		}
		iResult = __xrtChannelStop(iDeadline, pCancel);
		if ( iResult != XWAIT_OK ) {
			break;
		}
		iResult = __xrtChannelWaitCondition(
			&pImpl->Readable,
			&pImpl->Mutex,
			iDeadline
		);
		if ( iResult == XWAIT_ERROR ) {
			break;
		}
	}

	pImpl->ReadWaiters--;
	__xrtChannelUnlock(pImpl);
	return iResult;
}



#if defined(XRT_FEATURE_CHANNEL_CANCEL)

/* 取消回调在 Channel 锁内广播两类等待者，避免检查与休眠之间丢通知。 */
static void __xrtChannelCancelWake(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	xrt_channel_impl* pImpl;

	if ( !__xrtChannelAddressValid(pChannel) ) {
		return;
	}
	pImpl = __xrtChannelImpl(pChannel);
	if (
		(pImpl->Magic != XRT_CHANNEL_MAGIC) ||
		!xrtMutexLock(&pImpl->Mutex)
	) {
		return;
	}
	(void)xrtCondBroadcast(&pImpl->Readable);
	(void)xrtCondBroadcast(&pImpl->Writable);
	(void)xrtMutexUnlock(&pImpl->Mutex);
}

#endif



/* 初始化精确容量的 Channel。 */
XRT_API bool xrtChannelInit(xchannel* pChannel, size_t iCapacity)
{
	ptr* pItems = NULL;
	size_t iBytes = 0;

	if ( !__xrtChannelAddressValid(pChannel) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(ptr)) ) {
		memset(pChannel, 0, sizeof(xchannel));
		__xrtErrorSetSizeOverflow();
		return false;
	}

	/* 同步 Channel 不分配消息区。 */
	if ( iCapacity != 0 ) {
		iBytes = iCapacity * sizeof(ptr);
		pItems = (ptr*)xrtMalloc(iBytes);
		if ( pItems == NULL ) {
			memset(pChannel, 0, sizeof(xchannel));
			return false;
		}
		memset(pItems, 0, iBytes);
	}
	if (
		!__xrtChannelSetup(
			pChannel,
			pItems,
			iCapacity,
			(ptr)pItems
		)
	) {
		xrtFree(pItems);
		return false;
	}
	return true;
}



/* 在调用方提供的精确容量指针环上初始化有缓冲 Channel。 */
XRT_API bool xrtChannelInitBuffer(
	xchannel* pChannel,
	ptr* pItems,
	size_t iCapacity
)
{
	if ( !__xrtChannelBufferValid(pChannel, pItems, iCapacity) ) {
		if ( pChannel != NULL ) {
			memset(pChannel, 0, sizeof(xchannel));
		}
		return false;
	}
	memset(pItems, 0, iCapacity * sizeof(ptr));
	return __xrtChannelSetup(pChannel, pItems, iCapacity, NULL);
}



/* 创建精确容量的 Channel。 */
XRT_API xchannel* xrtChannelCreate(size_t iCapacity)
{
	xchannel* pChannel = (xchannel*)xrtMalloc(sizeof(xchannel));

	if ( pChannel == NULL ) {
		return NULL;
	}
	if ( !xrtChannelInit(pChannel, iCapacity) ) {
		xrtFree(pChannel);
		return NULL;
	}
	return pChannel;
}



/* 释放 Channel 内部资源。 */
XRT_API bool xrtChannelUnit(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	ptr pAllocation;
	bool bResult = true;

	if ( pImpl == NULL ) {
		return false;
	}
	if (
		(pImpl->ReadWaiters != 0) ||
		(pImpl->WriteWaiters != 0) ||
		((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) ||
		(pImpl->SelectWaiters != NULL)
	) {
		__xrtChannelUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}

	/* 独占生命周期保证同步对象此时不再被其他线程访问。 */
	pAllocation = pImpl->Allocation;
	pImpl->Magic = 0;
	__xrtChannelUnlock(pImpl);
	if ( !xrtCondUnit(&pImpl->Writable) ) {
		bResult = false;
	}
	if ( !xrtCondUnit(&pImpl->Readable) ) {
		bResult = false;
	}
	if ( !xrtMutexUnit(&pImpl->Mutex) ) {
		bResult = false;
	}
	xrtFree(pAllocation);
	memset(pChannel, 0, sizeof(xchannel));
	return bResult;
}



/* 释放 Create 返回的 Channel。 */
XRT_API bool xrtChannelDestroy(xchannel* pChannel)
{
	if ( pChannel == NULL ) {
		return true;
	}
	if ( !xrtChannelUnit(pChannel) ) {
		return false;
	}
	xrtFree(pChannel);
	return true;
}



/* 非阻塞发送一个指针值。 */
XRT_API xchannelresult xrtChannelTrySend(xchannel* pChannel, ptr pItem)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	xchannelresult iResult;

	if ( pImpl == NULL ) {
		return XCHANNEL_ERROR;
	}
	iResult = __xrtChannelTrySendLocked(pImpl, pItem);
	__xrtChannelUnlock(pImpl);
	return iResult;
}



/* 等待发送一个指针值。 */
XRT_API xwaitresult xrtChannelSend(xchannel* pChannel, ptr pItem)
{
	return __xrtChannelSendWait(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对微秒数内等待发送一个指针值。 */
XRT_API xwaitresult xrtChannelSendFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
)
{
	return xrtChannelSendUntil(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 等待发送一个指针值到指定截止时间。 */
XRT_API xwaitresult xrtChannelSendUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
)
{
	return __xrtChannelSendWait(
		pChannel,
		pItem,
		iDeadline,
		NULL
	);
}



/* 非阻塞接收一个指针值。 */
XRT_API xchannelresult xrtChannelTryRecv(
	xchannel* pChannel,
	ptr* pItem
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	xchannelresult iResult;

	if ( pImpl == NULL ) {
		return XCHANNEL_ERROR;
	}
	if ( !__xrtChannelOutputValid(pImpl, pItem) ) {
		__xrtChannelUnlock(pImpl);
		return XCHANNEL_ERROR;
	}
	iResult = __xrtChannelTryRecvLocked(pImpl, pItem);
	__xrtChannelUnlock(pImpl);
	return iResult;
}



/* 等待接收一个指针值。 */
XRT_API xwaitresult xrtChannelRecv(xchannel* pChannel, ptr* pItem)
{
	return __xrtChannelRecvWait(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对微秒数内等待接收一个指针值。 */
XRT_API xwaitresult xrtChannelRecvFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
)
{
	return xrtChannelRecvUntil(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 等待接收一个指针值到指定截止时间。 */
XRT_API xwaitresult xrtChannelRecvUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
)
{
	return __xrtChannelRecvWait(
		pChannel,
		pItem,
		iDeadline,
		NULL
	);
}



#if defined(XRT_FEATURE_CHANNEL_CANCEL)

/* 无限等待发送，并允许取消尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendCancel(
	xchannel* pChannel,
	ptr pItem,
	xcancel* pCancel
)
{
	return xrtChannelSendUntilCancel(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER,
		pCancel
	);
}



/* 在相对微秒数内等待发送，并允许取消尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendForCancel(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout,
	xcancel* pCancel
)
{
	return xrtChannelSendUntilCancel(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout),
		pCancel
	);
}



/* 等待发送到截止时间，并允许取消尚未提交的操作。 */
XRT_API xwaitresult xrtChannelSendUntilCancel(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch;
	xrt_channel_impl* pImpl;
	xchannelresult iTry;
	xwaitresult iResult;

	if ( pCancel == NULL ) {
		return xrtChannelSendUntil(pChannel, pItem, iDeadline);
	}

	/* 有缓冲的立即成功路径不为取消监听分配内存。 */
	pImpl = __xrtChannelLock(pChannel);
	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	iTry = __xrtChannelTrySendLocked(pImpl, pItem);
	__xrtChannelUnlock(pImpl);
	if ( iTry != XCHANNEL_FULL ) {
		return __xrtChannelWaitResult(iTry);
	}
	iResult = __xrtChannelStop(iDeadline, (ptr)pCancel);
	if ( iResult != XWAIT_OK ) {
		return iResult;
	}

	/* 监听在 Channel 锁外装配，迟到取消会同步触发广播。 */
	pWatch = xrtCancelWatch(
		pCancel,
		__xrtChannelCancelWake,
		pChannel
	);
	if ( pWatch == NULL ) {
		return XWAIT_ERROR;
	}
	iResult = __xrtChannelSendWait(
		pChannel,
		pItem,
		iDeadline,
		(ptr)pCancel
	);
	xrtCancelUnwatch(pWatch);
	return iResult;
}



/* 无限等待接收，并允许取消尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvCancel(
	xchannel* pChannel,
	ptr* pItem,
	xcancel* pCancel
)
{
	return xrtChannelRecvUntilCancel(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER,
		pCancel
	);
}



/* 在相对微秒数内等待接收，并允许取消尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvForCancel(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout,
	xcancel* pCancel
)
{
	return xrtChannelRecvUntilCancel(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout),
		pCancel
	);
}



/* 等待接收到截止时间，并允许取消尚未完成的操作。 */
XRT_API xwaitresult xrtChannelRecvUntilCancel(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch;
	xrt_channel_impl* pImpl;
	xchannelresult iTry;
	xwaitresult iResult;

	if ( pCancel == NULL ) {
		return xrtChannelRecvUntil(pChannel, pItem, iDeadline);
	}

	/* 已经可接收的值优先于监听分配、取消和截止时间。 */
	pImpl = __xrtChannelLock(pChannel);
	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	if ( !__xrtChannelOutputValid(pImpl, pItem) ) {
		__xrtChannelUnlock(pImpl);
		return XWAIT_ERROR;
	}
	iTry = __xrtChannelTryRecvLocked(pImpl, pItem);
	__xrtChannelUnlock(pImpl);
	if ( iTry != XCHANNEL_EMPTY ) {
		return __xrtChannelWaitResult(iTry);
	}
	iResult = __xrtChannelStop(iDeadline, (ptr)pCancel);
	if ( iResult != XWAIT_OK ) {
		return iResult;
	}

	/* 监听回调和条件检查共用 Channel mutex，不会丢失取消通知。 */
	pWatch = xrtCancelWatch(
		pCancel,
		__xrtChannelCancelWake,
		pChannel
	);
	if ( pWatch == NULL ) {
		return XWAIT_ERROR;
	}
	iResult = __xrtChannelRecvWait(
		pChannel,
		pItem,
		iDeadline,
		(ptr)pCancel
	);
	xrtCancelUnwatch(pWatch);
	return iResult;
}

#endif



/* 返回有缓冲 Channel 的精确元素数量。 */
XRT_API size_t xrtChannelCount(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	size_t iCount;

	if ( pImpl == NULL ) {
		return 0;
	}
	iCount = pImpl->Capacity == 0 ? 0 : pImpl->Count;
	__xrtChannelUnlock(pImpl);
	return iCount;
}



/* 返回 Channel 的精确容量。 */
XRT_API size_t xrtChannelCapacity(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	size_t iCapacity;

	if ( pImpl == NULL ) {
		return 0;
	}
	iCapacity = pImpl->Capacity;
	__xrtChannelUnlock(pImpl);
	return iCapacity;
}



/* 判断发送端是否已经关闭。 */
XRT_API bool xrtChannelIsClosed(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	bool bClosed;

	if ( pImpl == NULL ) {
		return false;
	}
	bClosed = (pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0;
	__xrtChannelUnlock(pImpl);
	return bClosed;
}



/* 判断 Channel 是否已经关闭且没有可接收值。 */
XRT_API bool xrtChannelIsDrained(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	bool bDrained;

	if ( pImpl == NULL ) {
		return false;
	}
	bDrained =
		((pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) != 0) &&
		(
			(pImpl->Capacity == 0) ?
				((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) == 0) :
				(pImpl->Count == 0)
		);
	__xrtChannelUnlock(pImpl);
	return bDrained;
}



/* 幂等关闭发送端并唤醒全部等待者。 */
XRT_API void xrtChannelClose(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);

	if ( pImpl == NULL ) {
		return;
	}
	if ( (pImpl->Flags & XRT_CHANNEL_FLAG_CLOSED) == 0 ) {
		pImpl->Flags |= XRT_CHANNEL_FLAG_CLOSED;

		/* 未配对的 rendezvous 发送在关闭时撤回。 */
		if (
			(pImpl->Capacity == 0) &&
			((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) &&
			((pImpl->Flags & XRT_CHANNEL_FLAG_COMMITTED) == 0)
		) {
			__xrtChannelWithdraw(pImpl);
		}
		pImpl->Epoch++;
		(void)xrtCondBroadcast(&pImpl->Readable);
		(void)xrtCondBroadcast(&pImpl->Writable);
		#if defined(XRT_INTERNAL_CHANNEL_WAIT)
			__xrtChannelNotifySelectLocked(pImpl);
		#endif
	}
	__xrtChannelUnlock(pImpl);
}



/* 排空调用开始时已有的值，并在锁外执行用户回调。 */
XRT_API size_t xrtChannelDrain(
	xchannel* pChannel,
	xchanneldrainfn pDrain,
	ptr pContext
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);
	size_t iLimit;
	size_t iDrained = 0;

	if ( pImpl == NULL ) {
		return 0;
	}

	/* 固定本次排空上限，回调新发送的值留给后续接收。 */
	iLimit = pImpl->Capacity == 0 ?
		(
			((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) ?
				1u : 0u
		) :
		pImpl->Count;
	while ( iDrained < iLimit ) {
		ptr pItem;

		if ( pImpl->Capacity == 0 ) {
			if ( (pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) == 0 ) {
				break;
			}
			__xrtChannelTakePending(pImpl, &pItem);
		} else {
			if ( pImpl->Count == 0 ) {
				break;
			}
			(void)__xrtChannelTryRecvLocked(pImpl, &pItem);
		}

		/* 用户代码绝不在 Channel 内部同步锁下执行。 */
		__xrtChannelUnlock(pImpl);
		pImpl = NULL;
		if ( pDrain != NULL ) {
			pDrain(pItem, pContext);
		}
		iDrained++;
		if ( iDrained == iLimit ) {
			break;
		}
		pImpl = __xrtChannelLock(pChannel);
		if ( pImpl == NULL ) {
			return iDrained;
		}
	}
	if ( pImpl != NULL ) {
		__xrtChannelUnlock(pImpl);
	}
	return iDrained;
}



/* 在独占、无等待者且为空时重置并重新开放 Channel。 */
XRT_API bool xrtChannelReset(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pChannel);

	if ( pImpl == NULL ) {
		return false;
	}
	if (
		(pImpl->ReadWaiters != 0) ||
		(pImpl->WriteWaiters != 0) ||
		(pImpl->Count != 0) ||
		((pImpl->Flags & XRT_CHANNEL_FLAG_PENDING) != 0) ||
		(pImpl->SelectWaiters != NULL)
	) {
		__xrtChannelUnlock(pImpl);
		__xrtErrorSetAgain();
		return false;
	}

	/* 清除全部公开可观察状态和旧代次。 */
	pImpl->Flags = 0;
	pImpl->Head = 0;
	pImpl->Tail = 0;
	pImpl->Pending = NULL;
	pImpl->NextGeneration = 0;
	pImpl->CompletedGeneration = 0;
	pImpl->Epoch++;
	if ( pImpl->Capacity != 0 ) {
		memset(
			pImpl->Items,
			0,
			pImpl->Capacity * sizeof(ptr)
		);
	}
	#if defined(XRT_INTERNAL_CHANNEL_WAIT)
		__xrtChannelNotifySelectLocked(pImpl);
	#endif
	__xrtChannelUnlock(pImpl);
	return true;
}

#endif
