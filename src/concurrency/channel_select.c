#include "../internal/xrt_channel.h"



#if defined(XRT_INTERNAL_CHANNEL_WAIT)

/* 全局轮转票据避免固定从第一个 case 开始造成长期偏置。 */
static xatomic32 __xrtChannelSelectTicket = XRT_ATOMIC32_INIT(0);



/* 构造一个没有 case 被选中的结果。 */
static xchannelselectresult __xrtChannelSelectEmpty(xwaitresult iWait)
{
	xchannelselectresult tResult;

	tResult.Wait = iWait;
	tResult.Index = XCHANNEL_SELECT_NONE;
	tResult.Result = XCHANNEL_ERROR;
	return tResult;
}



/* 构造一个已经选择具体 case 的结果。 */
static xchannelselectresult __xrtChannelSelectChosen(
	size_t iIndex,
	xchannelresult iResult
)
{
	xchannelselectresult tResult;

	tResult.Wait = iResult == XCHANNEL_ERROR ?
		XWAIT_ERROR : XWAIT_OK;
	tResult.Index = iIndex;
	tResult.Result = iResult;
	return tResult;
}



/* 读取一次 Select 的原子赢家。 */
static uint32 __xrtChannelSelectWinner(
	const xrt_channel_select_state* pSelect
)
{
	return xrtAtomic32Load(
		&pSelect->Winner,
		XMEMORY_ACQUIRE
	);
}



/* 把未提交的 Select 固定为超时、取消或平台错误。 */
static bool __xrtChannelSelectStop(
	xrt_channel_select_state* pSelect,
	xwaitresult iResult
)
{
	uint32 iExpected = XRT_CHANNEL_SELECT_NONE;

	if (
		!xrtAtomic32CompareExchange(
			&pSelect->Winner,
			&iExpected,
			XRT_CHANNEL_SELECT_CLAIMING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		)
	) {
		return false;
	}
	pSelect->StopResult = iResult;
	xrtAtomic32Store(
		&pSelect->Winner,
		XRT_CHANNEL_SELECT_STOPPED,
		XMEMORY_RELEASE
	);
	pSelect->Wake(pSelect->WakeData);
	return true;
}



/* 检查取消和截止时间，但不覆盖已经提交的 case。 */
static xwaitresult __xrtChannelSelectStopReason(
	xdeadline iDeadline,
	ptr pCancel
)
{
	#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)
		if (
			(pCancel != NULL) &&
			xrtCancelRequested((xcancel*)pCancel)
		) {
			return XWAIT_CANCELLED;
		}
	#else
		(void)pCancel;
	#endif
	return xrtDeadlineExpired(iDeadline) ?
		XWAIT_TIMEOUT : XWAIT_OK;
}



/* 检查 case 数组、操作、对象、输出和跨 Channel 别名。 */
static bool __xrtChannelSelectCasesValid(
	const xchannelcase* pCases,
	size_t iCount
)
{
	size_t iCaseBytes;

	if (
		(pCases == NULL) ||
		(iCount == 0) ||
		(iCount > ((size_t)XRT_CHANNEL_SELECT_MAX_INDEX + 1u)) ||
		(((uintptr_t)pCases & (sizeof(ptr) - 1u)) != 0) ||
		(iCount > (SIZE_MAX / sizeof(xchannelcase)))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iCaseBytes = iCount * sizeof(xchannelcase);
	if ( (uintptr_t)pCases > (UINTPTR_MAX - iCaseBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	/* 先拒绝无效操作和会被接收写回破坏的 case 数组。 */
	for ( size_t i = 0; i < iCount; i++ ) {
		if (
			(pCases[i].Channel == NULL) ||
			(
				(pCases[i].Operation != XCHANNEL_OP_RECV) &&
				(pCases[i].Operation != XCHANNEL_OP_SEND)
			) ||
			__xrtRangesOverlap(
				pCases[i].Channel,
				sizeof(xchannel),
				pCases,
				iCaseBytes
			)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if (
			(pCases[i].Operation == XCHANNEL_OP_RECV) &&
			(
				(pCases[i].Output == NULL) ||
				(((uintptr_t)pCases[i].Output &
				  (sizeof(ptr) - 1u)) != 0) ||
				((uintptr_t)pCases[i].Output >
				 (UINTPTR_MAX - sizeof(ptr))) ||
				__xrtRangesOverlap(
					pCases[i].Output,
					sizeof(ptr),
					pCases,
					iCaseBytes
				)
			)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}

	/* 每个接收输出都必须与全部参与 Channel 及其消息环分离。 */
	for ( size_t i = 0; i < iCount; i++ ) {
		xrt_channel_impl* pImpl = __xrtChannelLock(
			pCases[i].Channel
		);
		size_t iItemBytes;

		if ( pImpl == NULL ) {
			return false;
		}
		iItemBytes = pImpl->Capacity * sizeof(ptr);
		if (
			__xrtRangesOverlap(
				pImpl,
				sizeof(xchannel),
				pCases,
				iCaseBytes
			) ||
			(
				(pImpl->Capacity != 0) &&
				__xrtRangesOverlap(
					pImpl->Items,
					iItemBytes,
					pCases,
					iCaseBytes
				)
			)
		) {
			__xrtChannelUnlock(pImpl);
			__xrtErrorSetInvalidArgument();
			return false;
		}
		for ( size_t j = 0; j < iCount; j++ ) {
			if (
				(pCases[j].Operation == XCHANNEL_OP_RECV) &&
				!__xrtChannelOutputValid(
					pImpl,
					pCases[j].Output
				)
			) {
				__xrtChannelUnlock(pImpl);
				return false;
			}
		}
		__xrtChannelUnlock(pImpl);
	}
	return true;
}



/* 尝试一个未注册 case，并保证未选接收输出保持不变。 */
static xchannelresult __xrtChannelSelectTryCase(
	const xchannelcase* pCase
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pCase->Channel);
	xchannelresult iResult;
	ptr pItem = NULL;

	if ( pImpl == NULL ) {
		return XCHANNEL_ERROR;
	}
	if ( pCase->Operation == XCHANNEL_OP_SEND ) {
		iResult = __xrtChannelTrySendLocked(
			pImpl,
			pCase->Value
		);
	} else {
		iResult = __xrtChannelTryRecvLocked(pImpl, &pItem);
		if (
			(iResult == XCHANNEL_OK) ||
			(iResult == XCHANNEL_CLOSED)
		) {
			*pCase->Output = pItem;
		}
	}
	__xrtChannelUnlock(pImpl);
	return iResult;
}



/* 按轮转起点尝试全部未注册 case。 */
static xchannelselectresult __xrtChannelSelectTryCases(
	const xchannelcase* pCases,
	size_t iCount,
	size_t iStart
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iIndex = iStart + i;
		xchannelresult iResult;

		if ( iIndex >= iCount ) {
			iIndex -= iCount;
		}
		iResult = __xrtChannelSelectTryCase(&pCases[iIndex]);
		if (
			(iResult != XCHANNEL_EMPTY) &&
			(iResult != XCHANNEL_FULL)
		) {
			return __xrtChannelSelectChosen(iIndex, iResult);
		}
	}
	return __xrtChannelSelectEmpty(XWAIT_TIMEOUT);
}



/* 把一个等待节点追加到 Channel，保持同类选择者的注册顺序。 */
static bool __xrtChannelSelectRegister(
	xrt_channel_select_waiter* pWaiter
)
{
	xrt_channel_impl* pImpl = __xrtChannelLock(pWaiter->Channel);
	xrt_channel_select_waiter** ppLink;

	if ( pImpl == NULL ) {
		return false;
	}
	ppLink = &pImpl->SelectWaiters;
	while ( *ppLink != NULL ) {
		ppLink = &(*ppLink)->Next;
	}
	*ppLink = pWaiter;
	pWaiter->Registered = true;

	/* 新的对端 case 可能让另一个无缓冲 Select 立即可提交。 */
	__xrtChannelNotifySelectLocked(pImpl);
	__xrtChannelUnlock(pImpl);
	return true;
}



/* 从 Channel 等待链移除一个已经注册的节点。 */
static void __xrtChannelSelectUnregister(
	xrt_channel_select_waiter* pWaiter
)
{
	xrt_channel_impl* pImpl;
	xrt_channel_select_waiter** ppLink;

	if ( !pWaiter->Registered ) {
		return;
	}
	pImpl = __xrtChannelLock(pWaiter->Channel);
	if ( pImpl == NULL ) {
		return;
	}
	ppLink = &pImpl->SelectWaiters;
	while ( (*ppLink != NULL) && (*ppLink != pWaiter) ) {
		ppLink = &(*ppLink)->Next;
	}
	if ( *ppLink == pWaiter ) {
		*ppLink = pWaiter->Next;
		pWaiter->Next = NULL;
		pWaiter->Registered = false;
		__xrtChannelNotifySelectLocked(pImpl);
	}
	__xrtChannelUnlock(pImpl);
}



/* 依次移除当前 Select 已经注册的全部节点。 */
static void __xrtChannelSelectUnregisterAll(
	xrt_channel_select_waiter* pWaiters,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtChannelSelectUnregister(&pWaiters[i]);
	}
}



/* 在 Channel 锁内尝试提交一个已注册 case。 */
static void __xrtChannelSelectTryWaiter(
	xrt_channel_select_waiter* pWaiter
)
{
	xrt_channel_impl* pImpl;
	xchannelresult iResult;
	ptr pItem = NULL;

	if ( !pWaiter->Registered ) {
		return;
	}
	pImpl = __xrtChannelLock(pWaiter->Channel);
	if ( pImpl == NULL ) {
		return;
	}
	if ( !__xrtChannelSelectClaim(pWaiter) ) {
		__xrtChannelUnlock(pImpl);
		return;
	}

	if ( pWaiter->Operation == XCHANNEL_OP_SEND ) {
		iResult = __xrtChannelTrySendLocked(
			pImpl,
			pWaiter->Value
		);
	} else {
		iResult = __xrtChannelTryRecvLocked(pImpl, &pItem);
	}
	if (
		(iResult == XCHANNEL_EMPTY) ||
		(iResult == XCHANNEL_FULL)
	) {
		__xrtChannelSelectAbort(pWaiter);
		__xrtChannelUnlock(pImpl);
		return;
	}

	if (
		(pWaiter->Operation == XCHANNEL_OP_RECV) &&
		(
			(iResult == XCHANNEL_OK) ||
			(iResult == XCHANNEL_CLOSED)
		)
	) {
		*pWaiter->Output = pItem;
	}
	__xrtChannelSelectCommit(pWaiter, iResult);
	__xrtChannelUnlock(pImpl);
}



/* 按轮转起点尝试全部已注册 case。 */
static void __xrtChannelSelectTryWaiters(
	xrt_channel_select_waiter* pWaiters,
	size_t iCount,
	size_t iStart
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iIndex = iStart + i;

		if (
			__xrtChannelSelectWinner(
				pWaiters[0].Select
			) != XRT_CHANNEL_SELECT_NONE
		) {
			return;
		}
		if ( iIndex >= iCount ) {
			iIndex -= iCount;
		}
		__xrtChannelSelectTryWaiter(&pWaiters[iIndex]);
	}
}



#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)

/* 取消回调只唤醒等待驱动，提交竞争仍由原子赢家决定。 */
static void __xrtChannelSelectCancelWake(ptr pData)
{
	xrt_channel_select_state* pSelect =
		(xrt_channel_select_state*)pData;

	if ( pSelect != NULL ) {
		pSelect->Wake(pSelect->WakeData);
	}
}

#endif



/* 用调用方等待驱动完成 Select 的注册、等待、停止和清理。 */
xchannelselectresult __xrtChannelSelectWait(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	ptr pCancel,
	xrt_channel_select_wake_proc pWake,
	xrt_channel_select_prepare_proc pPrepare,
	xrt_channel_select_wait_proc pWait,
	ptr pWaitData
)
{
	xchannelselectresult tTry;
	xrt_channel_select_waiter* pWaiters;
	xrt_channel_select_waiter arrInline[
		XRT_CHANNEL_SELECT_INLINE_CASES
	];
	xrt_channel_select_state tSelect;
	size_t iStart;
	uint32 iWinner;
	bool bAllocated;
	#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)
		xcancelwatch* pWatch = NULL;
	#endif

	if ( (pWake == NULL) || (pWait == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return __xrtChannelSelectEmpty(XWAIT_ERROR);
	}
	if ( !__xrtChannelSelectCasesValid(pCases, iCount) ) {
		return __xrtChannelSelectEmpty(XWAIT_ERROR);
	}
	iStart = iCount == 1u ? 0 :
		xrtAtomic32FetchAdd(
			&__xrtChannelSelectTicket,
			1u,
			XMEMORY_RELAXED
		) % iCount;
	tTry = __xrtChannelSelectTryCases(
		pCases,
		iCount,
		iStart
	);
	if ( tTry.Wait != XWAIT_TIMEOUT ) {
		return tTry;
	}
	tTry.Wait = __xrtChannelSelectStopReason(
		iDeadline,
		pCancel
	);
	if ( tTry.Wait != XWAIT_OK ) {
		return __xrtChannelSelectEmpty(tTry.Wait);
	}
	if (
		(pPrepare != NULL) &&
		!pPrepare(pWaitData)
	) {
		return __xrtChannelSelectEmpty(XWAIT_ERROR);
	}

	bAllocated = iCount > XRT_CHANNEL_SELECT_INLINE_CASES;
	if ( bAllocated ) {
		pWaiters = (xrt_channel_select_waiter*)xrtMalloc(
			iCount * sizeof(xrt_channel_select_waiter)
		);
		if ( pWaiters == NULL ) {
			return __xrtChannelSelectEmpty(XWAIT_ERROR);
		}
	} else {
		pWaiters = arrInline;
	}
	memset(
		pWaiters,
		0,
		iCount * sizeof(xrt_channel_select_waiter)
	);
	memset(&tSelect, 0, sizeof(tSelect));
	xrtAtomic32Init(
		&tSelect.Winner,
		XRT_CHANNEL_SELECT_NONE
	);
	tSelect.Wake = pWake;
	tSelect.WakeData = pWaitData;
	tSelect.StopResult = XWAIT_ERROR;

	#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)
		if ( pCancel != NULL ) {
			pWatch = xrtCancelWatch(
				(xcancel*)pCancel,
				__xrtChannelSelectCancelWake,
				&tSelect
			);
			if ( pWatch == NULL ) {
				if ( bAllocated ) {
					xrtFree(pWaiters);
				}
				return __xrtChannelSelectEmpty(XWAIT_ERROR);
			}
		}
	#endif

	/* 节点完整初始化后再逐项发布到 Channel 等待链。 */
	for ( size_t i = 0; i < iCount; i++ ) {
		pWaiters[i].Select = &tSelect;
		pWaiters[i].Channel = pCases[i].Channel;
		pWaiters[i].Value = pCases[i].Value;
		pWaiters[i].Output = pCases[i].Output;
		pWaiters[i].Index = (uint32)i;
		pWaiters[i].Operation = pCases[i].Operation;
		pWaiters[i].Result = XCHANNEL_ERROR;
		if ( !__xrtChannelSelectRegister(&pWaiters[i]) ) {
			(void)__xrtChannelSelectStop(
				&tSelect,
				XWAIT_ERROR
			);
			break;
		}
		if (
			__xrtChannelSelectWinner(
				&tSelect
			) != XRT_CHANNEL_SELECT_NONE
		) {
			break;
		}
	}

	for ( ;; ) {
		xwaitresult iStop;
		xwaitresult iWait;

		iWinner = __xrtChannelSelectWinner(&tSelect);
		if (
			(iWinner != XRT_CHANNEL_SELECT_NONE) &&
			(iWinner != XRT_CHANNEL_SELECT_CLAIMING)
		) {
			break;
		}
		if (
			(pPrepare != NULL) &&
			!pPrepare(pWaitData)
		) {
			(void)__xrtChannelSelectStop(
				&tSelect,
				XWAIT_ERROR
			);
			continue;
		}
		if (
			__xrtChannelSelectWinner(
				&tSelect
			) == XRT_CHANNEL_SELECT_NONE
		) {
			__xrtChannelSelectTryWaiters(
				pWaiters,
				iCount,
				iStart
			);
		}

		iWinner = __xrtChannelSelectWinner(&tSelect);
		if (
			(iWinner != XRT_CHANNEL_SELECT_NONE) &&
			(iWinner != XRT_CHANNEL_SELECT_CLAIMING)
		) {
			break;
		}
		iStop = __xrtChannelSelectStopReason(
			iDeadline,
			pCancel
		);
		if (
			(iStop != XWAIT_OK) &&
			__xrtChannelSelectStop(&tSelect, iStop)
		) {
			continue;
		}

		iWait = pWait(pWaitData, iDeadline);
		if ( iWait != XWAIT_OK ) {
			(void)__xrtChannelSelectStop(
				&tSelect,
				iWait
			);
		}
	}

	__xrtChannelSelectUnregisterAll(pWaiters, iCount);
	#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)
		xrtCancelUnwatch(pWatch);
	#endif
	iWinner = __xrtChannelSelectWinner(&tSelect);
	if ( iWinner <= XRT_CHANNEL_SELECT_MAX_INDEX ) {
		tTry = __xrtChannelSelectChosen(
			iWinner,
			pWaiters[iWinner].Result
		);
	} else {
		tTry = __xrtChannelSelectEmpty(tSelect.StopResult);
	}
	if ( bAllocated ) {
		xrtFree(pWaiters);
	}
	return tTry;
}



/* 唤醒当前 Channel 上注册的全部 Select。 */
void __xrtChannelNotifySelectLocked(xrt_channel_impl* pImpl)
{
	xrt_channel_select_waiter* pWaiter;

	if ( pImpl == NULL ) {
		return;
	}
	pWaiter = pImpl->SelectWaiters;
	while ( pWaiter != NULL ) {
		pWaiter->Select->Wake(pWaiter->Select->WakeData);
		pWaiter = pWaiter->Next;
	}
}



/* 尝试独占一次 Select 提交。 */
bool __xrtChannelSelectClaim(xrt_channel_select_waiter* pWaiter)
{
	uint32 iExpected = XRT_CHANNEL_SELECT_NONE;

	return
		(pWaiter != NULL) &&
		xrtAtomic32CompareExchange(
			&pWaiter->Select->Winner,
			&iExpected,
			XRT_CHANNEL_SELECT_CLAIMING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		);
}



/* 发布一个已经完成的 Select case 并唤醒选择者。 */
void __xrtChannelSelectCommit(
	xrt_channel_select_waiter* pWaiter,
	xchannelresult iResult
)
{
	pWaiter->Result = iResult;
	xrtAtomic32Store(
		&pWaiter->Select->Winner,
		pWaiter->Index,
		XMEMORY_RELEASE
	);
	pWaiter->Select->Wake(pWaiter->Select->WakeData);
}



/* 放弃尚未完成的临时 Select 提交权。 */
void __xrtChannelSelectAbort(xrt_channel_select_waiter* pWaiter)
{
	xrtAtomic32Store(
		&pWaiter->Select->Winner,
		XRT_CHANNEL_SELECT_NONE,
		XMEMORY_RELEASE
	);
}



/* 构造一个发送 case。 */
XRT_API xchannelcase xrtChannelCaseSend(
	xchannel* pChannel,
	ptr pItem
)
{
	xchannelcase tCase;

	tCase.Channel = pChannel;
	tCase.Operation = XCHANNEL_OP_SEND;
	tCase.Value = pItem;
	tCase.Output = NULL;
	return tCase;
}



/* 构造一个接收 case。 */
XRT_API xchannelcase xrtChannelCaseRecv(
	xchannel* pChannel,
	ptr* pItem
)
{
	xchannelcase tCase;

	tCase.Channel = pChannel;
	tCase.Operation = XCHANNEL_OP_RECV;
	tCase.Value = NULL;
	tCase.Output = pItem;
	return tCase;
}



#if defined(XRT_FEATURE_CHANNEL_SELECT)

/* 公平地尝试全部同步 Select case。 */
static xchannelselectresult __xrtChannelSelectTry(
	const xchannelcase* pCases,
	size_t iCount
)
{
	size_t iStart;

	if ( !__xrtChannelSelectCasesValid(pCases, iCount) ) {
		return __xrtChannelSelectEmpty(XWAIT_ERROR);
	}
	iStart = iCount == 1u ? 0 :
		xrtAtomic32FetchAdd(
			&__xrtChannelSelectTicket,
			1u,
			XMEMORY_RELAXED
		) % iCount;
	return __xrtChannelSelectTryCases(
		pCases,
		iCount,
		iStart
	);
}



/* 原生事件驱动只在真正需要阻塞时初始化事件。 */
typedef struct xrt_channel_select_event {
	xevent Event;
	bool Initialized;
} xrt_channel_select_event;



/* Channel 状态变化时设置本次 Select 的原生事件。 */
static void __xrtChannelSelectEventWake(ptr pData)
{
	xrt_channel_select_event* pEvent =
		(xrt_channel_select_event*)pData;

	if ( pEvent->Initialized ) {
		(void)xrtEventSet(&pEvent->Event);
	}
}



/* 首次调用初始化事件，后续调用清除上轮通知。 */
static bool __xrtChannelSelectEventPrepare(ptr pData)
{
	xrt_channel_select_event* pEvent =
		(xrt_channel_select_event*)pData;

	if ( !pEvent->Initialized ) {
		if ( !xrtEventInit(&pEvent->Event, true, false) ) {
			return false;
		}
		pEvent->Initialized = true;
		return true;
	}
	return xrtEventReset(&pEvent->Event);
}



/* 在原生事件上等待到指定单调时钟截止时间。 */
static xwaitresult __xrtChannelSelectEventWait(
	ptr pData,
	xdeadline iDeadline
)
{
	xrt_channel_select_event* pEvent =
		(xrt_channel_select_event*)pData;

	return xrtEventWaitUntil(&pEvent->Event, iDeadline);
}



/* 使用原生事件执行同步 Select，并在返回前释放事件。 */
static xchannelselectresult __xrtChannelSelectEvent(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	ptr pCancel
)
{
	xrt_channel_select_event tEvent;
	xchannelselectresult tResult;

	memset(&tEvent, 0, sizeof(tEvent));
	tResult = __xrtChannelSelectWait(
		pCases,
		iCount,
		iDeadline,
		pCancel,
		__xrtChannelSelectEventWake,
		__xrtChannelSelectEventPrepare,
		__xrtChannelSelectEventWait,
		&tEvent
	);
	if ( tEvent.Initialized ) {
		(void)xrtEventUnit(&tEvent.Event);
	}
	return tResult;
}



/* 公平地尝试全部 case。 */
XRT_API xchannelselectresult xrtChannelSelectTry(
	const xchannelcase* pCases,
	size_t iCount
)
{
	return __xrtChannelSelectTry(pCases, iCount);
}



/* 等待任意一个 case 原子提交。 */
XRT_API xchannelselectresult xrtChannelSelect(
	const xchannelcase* pCases,
	size_t iCount
)
{
	return __xrtChannelSelectEvent(
		pCases,
		iCount,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对微秒数内等待任意一个 case 原子提交。 */
XRT_API xchannelselectresult xrtChannelSelectFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
)
{
	return xrtChannelSelectUntil(
		pCases,
		iCount,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 等待任意一个 case 原子提交到指定截止时间。 */
XRT_API xchannelselectresult xrtChannelSelectUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
)
{
	return __xrtChannelSelectEvent(
		pCases,
		iCount,
		iDeadline,
		NULL
	);
}



#if defined(XRT_FEATURE_CHANNEL_SELECT_CANCEL)

/* 等待任意 case 提交，并允许取消未提交的选择。 */
XRT_API xchannelselectresult xrtChannelSelectUntilCancel(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( pCancel == NULL ) {
		return xrtChannelSelectUntil(
			pCases,
			iCount,
			iDeadline
		);
	}
	return __xrtChannelSelectEvent(
		pCases,
		iCount,
		iDeadline,
		(ptr)pCancel
	);
}

#endif

#endif

#endif
