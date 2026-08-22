#include "../../src/internal/xrt_channel.h"
#include "../test.h"
#include "../test_thread.h"



/* 可取消单次操作的线程状态。 */
typedef struct testchannelcancelop {
	xchannel* Channel;
	xcancel* Cancel;
	ptr Item;
	xwaitresult Result;
	bool Send;
} testchannelcancelop;



/* 在线程中执行可取消发送或接收。 */
static int testChannelCancelWorker(ptr pData)
{
	testchannelcancelop* pOp = (testchannelcancelop*)pData;

	if ( pOp->Send ) {
		pOp->Result = xrtChannelSendUntilCancel(
			pOp->Channel,
			pOp->Item,
			XRT_DEADLINE_NEVER,
			pOp->Cancel
		);
	} else {
		pOp->Result = xrtChannelRecvUntilCancel(
			pOp->Channel,
			&pOp->Item,
			XRT_DEADLINE_NEVER,
			pOp->Cancel
		);
	}
	return 0;
}



/* 等待 Channel 出现指定方向的等待者。 */
static void testChannelCancelAwait(
	xchannel* pChannel,
	size_t iReaders,
	size_t iWriters
)
{
	xrt_channel_impl* pImpl = (xrt_channel_impl*)pChannel;
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));

	for ( ;; ) {
		size_t iActualReaders;
		size_t iActualWriters;

		testRequire(
			xrtMutexLock(&pImpl->Mutex),
			"channel cancel probe lock failed"
		);
		iActualReaders = pImpl->ReadWaiters;
		iActualWriters = pImpl->WriteWaiters;
		testRequire(
			xrtMutexUnlock(&pImpl->Mutex),
			"channel cancel probe unlock failed"
		);
		if (
			(iActualReaders == iReaders) &&
			(iActualWriters == iWriters)
		) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"channel cancel waiter count did not converge"
		);
		xrtSleepUs(UINT64_C(1000));
	}
}



/* 验证空接收和满发送都能由取消无轮询唤醒。 */
static void testChannelCancelBuffered(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	testchannelcancelop tOp;
	testthread tThread;
	ptr pItem = NULL;

	testRequire(xrtChannelInit(&tChannel, 1u), "cancel channel init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "receive cancel token create failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Cancel = pCancel;
	tThread.Proc = testChannelCancelWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelCancelAwait(&tChannel, 1, 0);
	testRequire(xrtCancelRequest(pCancel), "receive cancel request failed");
	testThreadsJoin(&tThread, 1);
	testRequire(
		(tOp.Result == XWAIT_CANCELLED) && (tOp.Item == NULL),
		"empty receive cancellation mismatch"
	);
	xrtCancelDestroy(pCancel);

	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)1u) == XCHANNEL_OK,
		"send cancel prefill failed"
	);
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "send cancel token create failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Cancel = pCancel;
	tOp.Item = (ptr)(uintptr_t)2u;
	tOp.Send = true;
	tThread.Proc = testChannelCancelWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelCancelAwait(&tChannel, 0, 1);
	testRequire(xrtCancelRequest(pCancel), "send cancel request failed");
	testThreadsJoin(&tThread, 1);
	testRequire(tOp.Result == XWAIT_CANCELLED, "full send cancellation mismatch");
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_OK,
		"send cancellation changed buffered item"
	);
	testRequire((uintptr_t)pItem == 1u, "send cancellation payload mismatch");
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_EMPTY,
		"cancelled send remained buffered"
	);
	xrtCancelDestroy(pCancel);
	testRequire(xrtChannelUnit(&tChannel), "cancel channel unit failed");
}



/* 验证无缓冲待发送值可取消撤回。 */
static void testChannelCancelRendezvous(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	testchannelcancelop tOp;
	testthread tThread;
	ptr pItem = NULL;

	testRequire(xrtChannelInit(&tChannel, 0), "cancel rendezvous init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "rendezvous cancel token create failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Cancel = pCancel;
	tOp.Item = (ptr)(uintptr_t)7u;
	tOp.Send = true;
	tThread.Proc = testChannelCancelWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelCancelAwait(&tChannel, 0, 1);
	testRequire(xrtCancelRequest(pCancel), "rendezvous cancel request failed");
	testThreadsJoin(&tThread, 1);
	testRequire(
		tOp.Result == XWAIT_CANCELLED,
		"rendezvous send cancellation mismatch"
	);
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_EMPTY,
		"cancelled rendezvous value remained visible"
	);
	xrtCancelDestroy(pCancel);
	testRequire(xrtChannelUnit(&tChannel), "cancel rendezvous unit failed");
}



/* 验证已就绪操作优先于预取消令牌。 */
static void testChannelCancelReady(void)
{
	xchannel tChannel;
	xcancel* pCancel = xrtCancelCreate();
	ptr pItem = NULL;

	testRequire(pCancel != NULL, "ready cancel token create failed");
	testRequire(xrtCancelRequest(pCancel), "ready cancel request failed");
	testRequire(xrtChannelInit(&tChannel, 1u), "ready cancel channel init failed");
	testRequire(
		xrtChannelSendUntilCancel(
			&tChannel,
			(ptr)(uintptr_t)21u,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_OK,
		"ready send lost to pre-cancel token"
	);
	testRequire(
		xrtChannelRecvUntilCancel(
			&tChannel,
			&pItem,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_OK,
		"ready receive lost to pre-cancel token"
	);
	testRequire((uintptr_t)pItem == 21u, "ready cancel payload mismatch");

	xrtChannelClose(&tChannel);
	testRequire(
		xrtChannelRecvUntilCancel(
			&tChannel,
			&pItem,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_CLOSED,
		"closed receive lost to cancellation"
	);
	testRequire(
		xrtChannelSendUntilCancel(
			&tChannel,
			NULL,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_CLOSED,
		"closed send lost to cancellation"
	);
	testRequire(xrtChannelUnit(&tChannel), "ready cancel channel unit failed");
	xrtCancelDestroy(pCancel);
}



/* 验证已提交 rendezvous 值不会被随后取消撤销。 */
static void testChannelCancelCommitted(void)
{
	xchannel tChannel;
	xcancel* pCancel = xrtCancelCreate();
	testchannelcancelop tOp;
	testthread tThread;

	testRequire(pCancel != NULL, "committed cancel token create failed");
	testRequire(xrtChannelInit(&tChannel, 0), "committed channel init failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Cancel = pCancel;
	tThread.Proc = testChannelCancelWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelCancelAwait(&tChannel, 1, 0);
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)31u) == XCHANNEL_OK,
		"committed rendezvous send failed"
	);
	(void)xrtCancelRequest(pCancel);
	testThreadsJoin(&tThread, 1);
	testRequire(
		(tOp.Result == XWAIT_OK) && ((uintptr_t)tOp.Item == 31u),
		"cancellation revoked committed rendezvous"
	);
	testRequire(xrtChannelUnit(&tChannel), "committed channel unit failed");
	xrtCancelDestroy(pCancel);
}



/* 执行 Channel 取消合同测试。 */
int main(void)
{
	testChannelCancelBuffered();
	testChannelCancelRendezvous();
	testChannelCancelReady();
	testChannelCancelCommitted();
	printf("[PASS] channel cancel\n");
	return 0;
}
