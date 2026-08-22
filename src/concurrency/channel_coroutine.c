#include "../internal/xrt_channel.h"
#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_CHANNEL_COROUTINE)

/* 协程 Select 只在真正注册等待时开启资源通知代际。 */
typedef struct xrt_channel_await {
	xrt_co_wait Wait;
	xcoro* Coroutine;
	bool Opened;
} xrt_channel_await;



/* 构造没有 case 被选中的协程等待结果。 */
static xchannelselectresult __xrtChannelAwaitEmpty(xwaitresult iWait)
{
	xchannelselectresult tResult;

	tResult.Wait = iWait;
	tResult.Index = XCHANNEL_SELECT_NONE;
	tResult.Result = XCHANNEL_ERROR;
	return tResult;
}



/* 把 Channel 操作结果映射为统一等待结果。 */
static xwaitresult __xrtChannelAwaitResult(
	xchannelselectresult tResult
)
{
	if ( tResult.Wait != XWAIT_OK ) {
		return tResult.Wait;
	}
	if ( tResult.Result == XCHANNEL_OK ) {
		return XWAIT_OK;
	}
	if ( tResult.Result == XCHANNEL_CLOSED ) {
		return XWAIT_CLOSED;
	}
	return XWAIT_ERROR;
}



/* 快速路径失败后按需开启一次资源等待令牌。 */
static bool __xrtChannelAwaitPrepare(ptr pData)
{
	xrt_channel_await* pAwait = (xrt_channel_await*)pData;

	if ( pAwait->Opened ) {
		return true;
	}
	if ( !__xrtCoWaitOpen(pAwait->Coroutine, &pAwait->Wait) ) {
		return false;
	}
	pAwait->Opened = true;
	return true;
}



/* Channel 状态变化只通知当前 Await 的资源代际。 */
static void __xrtChannelAwaitWake(ptr pData)
{
	xrt_channel_await* pAwait = (xrt_channel_await*)pData;

	/*
	 * 当前协程注册 case 时会在返回后主动重检，只有其他执行上下文
	 * 才需要向调度器投递通知。
	 */
	if (
		pAwait->Opened &&
		(xrtCoCurrent() != pAwait->Coroutine)
	) {
		__xrtCoWaitWake(&pAwait->Wait);
	}
}



/* 通过当前 Await 的资源令牌挂起协程。 */
static xwaitresult __xrtChannelAwaitPark(
	ptr pData,
	xdeadline iDeadline
)
{
	xrt_channel_await* pAwait = (xrt_channel_await*)pData;

	if ( !pAwait->Opened ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	return __xrtCoWaitParkUntil(&pAwait->Wait, iDeadline);
}



/* 验证调用点位于调度器管理的当前协程中。 */
static xcoro* __xrtChannelAwaitCurrent(void)
{
	xcoro* pCo = xrtCoCurrent();

	if ( (pCo == NULL) || (xrtCoSchedCurrent() == NULL) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pCo;
}



/* 在当前协程中执行一次多路 Channel 等待。 */
XRT_API xchannelselectresult xrtChannelSelectAwaitUntil(
	const xchannelcase* pCases,
	size_t iCount,
	xdeadline iDeadline
)
{
	xchannelselectresult tResult;
	xcoro* pCo = __xrtChannelAwaitCurrent();
	xrt_channel_await tAwait;

	if ( pCo == NULL ) {
		return __xrtChannelAwaitEmpty(XWAIT_ERROR);
	}
	memset(&tAwait, 0, sizeof(tAwait));
	tAwait.Coroutine = pCo;
	tResult = __xrtChannelSelectWait(
		pCases,
		iCount,
		iDeadline,
		NULL,
		__xrtChannelAwaitWake,
		__xrtChannelAwaitPrepare,
		__xrtChannelAwaitPark,
		&tAwait
	);
	if ( tAwait.Opened ) {
		__xrtCoWaitClose(&tAwait.Wait);
	}
	return tResult;
}



/* 在当前协程中无限期等待任意一个 case。 */
XRT_API xchannelselectresult xrtChannelSelectAwait(
	const xchannelcase* pCases,
	size_t iCount
)
{
	return xrtChannelSelectAwaitUntil(
		pCases,
		iCount,
		XRT_DEADLINE_NEVER
	);
}



/* 在当前协程中等待任意 case 到相对期限。 */
XRT_API xchannelselectresult xrtChannelSelectAwaitFor(
	const xchannelcase* pCases,
	size_t iCount,
	uint64 iTimeout
)
{
	return xrtChannelSelectAwaitUntil(
		pCases,
		iCount,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 在当前协程中发送一个可为空的指针值。 */
XRT_API xwaitresult xrtChannelSendAwaitUntil(
	xchannel* pChannel,
	ptr pItem,
	xdeadline iDeadline
)
{
	xchannelcase tCase = xrtChannelCaseSend(pChannel, pItem);

	return __xrtChannelAwaitResult(
		xrtChannelSelectAwaitUntil(&tCase, 1, iDeadline)
	);
}



/* 在当前协程中无限期发送一个指针值。 */
XRT_API xwaitresult xrtChannelSendAwait(
	xchannel* pChannel,
	ptr pItem
)
{
	return xrtChannelSendAwaitUntil(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER
	);
}



/* 在当前协程中发送一个指针值到相对期限。 */
XRT_API xwaitresult xrtChannelSendAwaitFor(
	xchannel* pChannel,
	ptr pItem,
	uint64 iTimeout
)
{
	return xrtChannelSendAwaitUntil(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 在当前协程中接收一个可为空的指针值。 */
XRT_API xwaitresult xrtChannelRecvAwaitUntil(
	xchannel* pChannel,
	ptr* pItem,
	xdeadline iDeadline
)
{
	xchannelcase tCase = xrtChannelCaseRecv(pChannel, pItem);

	return __xrtChannelAwaitResult(
		xrtChannelSelectAwaitUntil(&tCase, 1, iDeadline)
	);
}



/* 在当前协程中无限期接收一个指针值。 */
XRT_API xwaitresult xrtChannelRecvAwait(
	xchannel* pChannel,
	ptr* pItem
)
{
	return xrtChannelRecvAwaitUntil(
		pChannel,
		pItem,
		XRT_DEADLINE_NEVER
	);
}



/* 在当前协程中接收一个指针值到相对期限。 */
XRT_API xwaitresult xrtChannelRecvAwaitFor(
	xchannel* pChannel,
	ptr* pItem,
	uint64 iTimeout
)
{
	return xrtChannelRecvAwaitUntil(
		pChannel,
		pItem,
		xrtDeadlineAfter(iTimeout)
	);
}

#endif
