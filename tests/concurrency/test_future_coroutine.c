#include "../test.h"
#include "../test_thread.h"



/* 协程等待上下文保存 Future、等待结果和完成值。 */
typedef struct testfutureawait {
	xfuture* Future;
	xpromise* Promise;
	xwaitresult Result;
	ptr Value;
	uint64 Timeout;
	bool Confirm;
} testfutureawait;



/* 调度协程等待 Future，可选择有限截止时间。 */
static ptr testFutureAwaitProc(ptr pData)
{
	testfutureawait* pContext = (testfutureawait*)pData;

	pContext->Result = pContext->Timeout == UINT64_MAX ?
		xrtFutureAwait(pContext->Future) :
		xrtFutureAwaitFor(pContext->Future, pContext->Timeout);
	if ( (pContext->Result == XWAIT_OK) &&
		 (xrtFutureState(pContext->Future) == XFUTURE_RESOLVED) ) {
		pContext->Value = xrtFutureValue(pContext->Future);
	}
	if ( pContext->Confirm && (pContext->Result == XWAIT_CANCELLED) ) {
		testRequire(xrtCoConfirmCancel(),
			"future await cancellation confirmation failed");
	}
	return pContext;
}



/* 外部线程完成 Promise，覆盖跨线程 Future 到协程的唤醒链。 */
static int testFutureAwaitProducer(ptr pData)
{
	testfutureawait* pContext = (testfutureawait*)pData;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(10);
	#else
		struct timespec tTime = { 0, 10000000 };

		(void)nanosleep(&tTime, NULL);
	#endif
	return xrtPromiseResolve(pContext->Promise, pContext->Value) ? 0 : 1;
}



/* 取消协程先让目标进入 Future await，再发出协作取消。 */
typedef struct testfuturecancel {
	xcoro* Target;
} testfuturecancel;



/* 在同一调度器中取消已经挂起的 Future 等待者。 */
static ptr testFutureAwaitCancelProc(ptr pData)
{
	testfuturecancel* pContext = (testfuturecancel*)pData;

	testRequire(xrtCoSleep(0) == XWAIT_OK, "future cancel helper yield failed");
	testRequire(xrtCoCancel(pContext->Target), "future await coroutine cancel failed");
	return pContext;
}



/* 验证完成、超时、取消与跨线程唤醒不会阻塞调度线程。 */
int main(void)
{
	testfutureawait tCross;
	testfutureawait tTimeout;
	testfutureawait tCancel;
	testfutureawait tConfirmed;
	testfuturecancel tCancelHelper;
	testthread tProducer;
	xcosched* pSched;
	xcoro* pCross;
	xcoro* pTimeout;
	xcoro* pCancel;
	xcoro* pConfirmed;
	int iValue = 73;

	memset(&tCross, 0, sizeof(tCross));
	memset(&tTimeout, 0, sizeof(tTimeout));
	memset(&tCancel, 0, sizeof(tCancel));
	memset(&tConfirmed, 0, sizeof(tConfirmed));
	memset(&tCancelHelper, 0, sizeof(tCancelHelper));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "future await scheduler create failed");

	tCross.Promise = xrtPromiseCreate(&tCross.Future, NULL);
	testRequire(tCross.Promise != NULL, "cross-thread future pair create failed");
	tCross.Timeout = UINT64_MAX;
	tCross.Value = &iValue;
	pCross = xrtCoSpawn(pSched, testFutureAwaitProc, &tCross, NULL);
	testRequire(pCross != NULL, "cross-thread future await spawn failed");
	tProducer.Proc = testFutureAwaitProducer;
	tProducer.Data = &tCross;
	testThreadsStart(&tProducer, 1);
	testRequire(xrtCoSchedRun(pSched), "cross-thread future await run failed");
	testThreadsJoin(&tProducer, 1);
	testRequire(tProducer.Result == 0, "cross-thread future producer failed");
	testRequire((tCross.Result == XWAIT_OK) && (tCross.Value == &iValue),
		"cross-thread future await result mismatch");
	testRequire(xrtCoDestroy(pCross), "cross-thread await coroutine destroy failed");
	xrtPromiseDestroy(tCross.Promise);
	xrtFutureDestroy(tCross.Future);

	tTimeout.Promise = xrtPromiseCreate(&tTimeout.Future, NULL);
	testRequire(tTimeout.Promise != NULL, "timeout future pair create failed");
	tTimeout.Timeout = 1;
	pTimeout = xrtCoSpawn(pSched, testFutureAwaitProc, &tTimeout, NULL);
	testRequire(pTimeout != NULL, "timeout future await spawn failed");
	testRequire(xrtCoSchedRun(pSched), "timeout future await run failed");
	testRequire(tTimeout.Result == XWAIT_TIMEOUT, "future await timeout mismatch");
	testRequire(xrtPromiseResolve(tTimeout.Promise, &iValue),
		"future resolve after await timeout failed");
	testRequire(xrtCoDestroy(pTimeout), "timeout await coroutine destroy failed");
	xrtPromiseDestroy(tTimeout.Promise);
	xrtFutureDestroy(tTimeout.Future);

	tCancel.Promise = xrtPromiseCreate(&tCancel.Future, NULL);
	testRequire(tCancel.Promise != NULL, "cancel future pair create failed");
	tCancel.Timeout = UINT64_MAX;
	pCancel = xrtCoSpawn(pSched, testFutureAwaitProc, &tCancel, NULL);
	testRequire(pCancel != NULL, "cancel future await spawn failed");
	tCancelHelper.Target = pCancel;
	testRequire(xrtCoGo(pSched, testFutureAwaitCancelProc, &tCancelHelper, NULL),
		"future await cancel helper spawn failed");
	testRequire(xrtCoSchedRun(pSched), "cancel future await run failed");
	testRequire(tCancel.Result == XWAIT_CANCELLED, "future await cancel result mismatch");
	testRequire(xrtCoTerm(pCancel) == XCORO_TERM_RETURNED,
		"handled future await cancellation did not return normally");
	testRequire(xrtCoDestroy(pCancel), "cancel await coroutine destroy failed");
	xrtPromiseDestroy(tCancel.Promise);
	xrtFutureDestroy(tCancel.Future);

	tConfirmed.Promise = xrtPromiseCreate(&tConfirmed.Future, NULL);
	testRequire(tConfirmed.Promise != NULL,
		"confirmed cancel future pair create failed");
	tConfirmed.Timeout = UINT64_MAX;
	tConfirmed.Confirm = true;
	pConfirmed = xrtCoSpawn(pSched, testFutureAwaitProc, &tConfirmed, NULL);
	testRequire(pConfirmed != NULL,
		"confirmed cancel future await spawn failed");
	tCancelHelper.Target = pConfirmed;
	testRequire(xrtCoGo(pSched, testFutureAwaitCancelProc, &tCancelHelper, NULL),
		"confirmed future await cancel helper spawn failed");
	testRequire(xrtCoSchedRun(pSched),
		"confirmed cancel future await run failed");
	testRequire((tConfirmed.Result == XWAIT_CANCELLED) &&
		(xrtCoTerm(pConfirmed) == XCORO_TERM_CANCELLED),
		"confirmed future await cancellation term mismatch");
	testRequire(xrtCoDestroy(pConfirmed),
		"confirmed cancel await coroutine destroy failed");
	xrtPromiseDestroy(tConfirmed.Promise);
	xrtFutureDestroy(tConfirmed.Future);

	testRequire(xrtCoSchedDestroy(pSched), "future await scheduler destroy failed");
	printf("[PASS] future coroutine\n");
	return 0;
}
