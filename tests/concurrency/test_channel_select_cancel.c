#include "../../src/internal/xrt_channel.h"
#include "../test.h"
#include "../test_thread.h"



/* 可取消 Select 线程保存令牌、case 和最终结果。 */
typedef struct testchannelselectcancel {
	xchannelcase Case;
	xcancel* Cancel;
	xchannelselectresult Result;
} testchannelselectcancel;



/* 在线程中等待一个可取消 Channel case。 */
static int testChannelSelectCancelWorker(ptr pData)
{
	testchannelselectcancel* pOp =
		(testchannelselectcancel*)pData;

	pOp->Result = xrtChannelSelectUntilCancel(
		&pOp->Case,
		1u,
		XRT_DEADLINE_NEVER,
		pOp->Cancel
	);
	return 0;
}



/* 等待可取消 Select 完成注册。 */
static void testChannelSelectCancelAwait(xchannel* pChannel)
{
	xrt_channel_impl* pImpl = (xrt_channel_impl*)pChannel;
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));

	for ( ;; ) {
		bool bRegistered;

		testRequire(
			xrtMutexLock(&pImpl->Mutex),
			"select cancel probe lock failed"
		);
		bRegistered = pImpl->SelectWaiters != NULL;
		testRequire(
			xrtMutexUnlock(&pImpl->Mutex),
			"select cancel probe unlock failed"
		);
		if ( bRegistered ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"select cancel registration did not converge"
		);
		xrtSleepUs(UINT64_C(1000));
	}
}



/* 验证取消唤醒、注册清理和已就绪 case 优先级。 */
int main(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	testchannelselectcancel tOp;
	testthread tThread;
	ptr pItem = (ptr)(uintptr_t)1u;

	testRequire(xrtChannelInit(&tChannel, 1u), "select cancel channel init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "select cancel token create failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Case = xrtChannelCaseRecv(&tChannel, &pItem);
	tOp.Cancel = pCancel;
	tThread.Proc = testChannelSelectCancelWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1u);
	testChannelSelectCancelAwait(&tChannel);
	testRequire(xrtCancelRequest(pCancel), "select cancel request failed");
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tOp.Result.Wait == XWAIT_CANCELLED) &&
		(tOp.Result.Index == XCHANNEL_SELECT_NONE),
		"blocked Select cancellation mismatch"
	);
	testRequire((uintptr_t)pItem == 1u, "cancelled Select modified output");
	testRequire(
		((xrt_channel_impl*)&tChannel)->SelectWaiters == NULL,
		"cancelled Select leaked registration"
	);
	xrtCancelDestroy(pCancel);

	/* 已经可接收的值优先于预取消令牌。 */
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "ready cancel token create failed");
	testRequire(xrtCancelRequest(pCancel), "ready cancel request failed");
	testRequire(
		xrtChannelTrySend(
			&tChannel,
			(ptr)(uintptr_t)88u
		) == XCHANNEL_OK,
		"ready cancel setup failed"
	);
	pItem = NULL;
	tOp.Case = xrtChannelCaseRecv(&tChannel, &pItem);
	tOp.Result = xrtChannelSelectUntilCancel(
		&tOp.Case,
		1u,
		XRT_DEADLINE_NEVER,
		pCancel
	);
	testRequire(
		(tOp.Result.Wait == XWAIT_OK) &&
		(tOp.Result.Index == 0) &&
		(tOp.Result.Result == XCHANNEL_OK) &&
		((uintptr_t)pItem == 88u),
		"ready Select lost to cancellation"
	);
	xrtCancelDestroy(pCancel);

	testRequire(xrtChannelUnit(&tChannel), "select cancel channel unit failed");
	printf("[PASS] channel select cancel\n");
	return 0;
}
