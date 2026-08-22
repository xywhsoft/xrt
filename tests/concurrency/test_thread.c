#include "../test.h"



/* 基础线程返回传入的整数。 */
static int32 testThreadReturn(ptr pData)
{
	xrtSleep(20000u / 1000u);
	return (int32)(intptr_t)pData;
}



/* 协作停止线程验证当前对象和当前线程标识。 */
static int32 testThreadStopWorker(ptr pData)
{
	uint64* pId = (uint64*)pData;

	*pId = xrtThreadCurrentId();
	if ( xrtThreadCurrent() == NULL ) {
		return -2;
	}
	while ( !xrtThreadStopping() ) {
		xrtThreadYield();
	}
	return 17;
}



/* 线程不能等待自己完成。 */
static int32 testThreadSelfWait(ptr pData)
{
	(void)pData;
	return xrtThreadWait(xrtThreadCurrent()) == XWAIT_ERROR ? 23 : -1;
}



/* 多等待者共享状态。 */
typedef struct testthreadwaiter {
	xthread* Target;
	xwaitresult Result;
} testthreadwaiter;



/* 等待另一个线程完成。 */
static int32 testThreadWaiter(ptr pData)
{
	testthreadwaiter* pWaiter = (testthreadwaiter*)pData;

	pWaiter->Result = xrtThreadWait(pWaiter->Target);
	return pWaiter->Result == XWAIT_OK ? 0 : -1;
}



/* 真正分离的线程不再有外部引用，仍必须能安全运行到返回。 */
static int32 testThreadDetached(ptr pData)
{
	(void)pData;
	xrtSleep(5);
	return 41;
}



/* 验证线程生命周期、超时、停止和多等待者契约。 */
int main(void)
{
	xthread* pThread;
	xthread* pRetained;
	xthread* pWaiters[3];
	testthreadwaiter arrWaiters[3];
	uint64 iWorkerId = 0;
	xdeadline iExpired;

	testRequire(xrtThreadCurrent() == NULL, "host thread exposed an XRT thread object");
	testRequire(xrtThreadCurrentId() != 0, "host thread id was zero");
	testRequire(xrtThreadCreate(NULL, NULL, 0) == NULL, "null thread entry was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null thread entry error mismatch");
	xrtClearError();
	testRequire(xrtThreadRef(NULL) == NULL, "null thread retain succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null thread retain error mismatch");
	xrtClearError();
	testRequire(xrtThreadWait(NULL) == XWAIT_ERROR, "null thread wait succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null thread wait error mismatch");
	xrtClearError();
	testRequire(!xrtThreadStop(NULL), "null thread stop succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null thread stop error mismatch");
	xrtClearError();
	testRequire(!xrtThreadStopRequested(NULL), "null thread reported a stop request");
	testRequire(!xrtThreadStopping(), "host thread reported a stop request");
	xrtThreadDestroy(NULL);

	pThread = xrtThreadCreate(testThreadStopWorker, &iWorkerId, 0);
	testRequire(pThread != NULL, "thread create failed");
	testRequire(xrtThreadState(pThread) == XTHREAD_RUNNING, "new thread state mismatch");
	testRequire(xrtThreadExitCode(pThread) == 0, "running thread returned an exit code");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "running exit code error mismatch");
	xrtClearError();
	testRequire(xrtThreadWaitFor(pThread, 0) == XWAIT_TIMEOUT, "zero timeout did not time out");
	iExpired = xrtClock();
	if ( iExpired != 0 ) {
		iExpired--;
	}
	testRequire(
		xrtThreadWaitUntil(pThread, iExpired) == XWAIT_TIMEOUT,
		"expired thread deadline did not time out"
	);
	testRequire(xrtThreadStop(pThread), "thread stop request failed");
	testRequire(xrtThreadStop(pThread), "repeated thread stop request failed");
	testRequire(xrtThreadStopRequested(pThread), "thread stop request was not visible");
	testRequire(
		xrtThreadWaitFor(pThread, UINT64_C(2000000)) == XWAIT_OK,
		"thread wait timed out"
	);
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "repeated thread wait failed");
	testRequire(xrtThreadExitCode(pThread) == 17, "thread exit code mismatch");
	testRequire(xrtThreadState(pThread) == XTHREAD_FINISHED, "finished thread state mismatch");
	testRequire(xrtThreadId(pThread) == iWorkerId, "current and object thread id mismatch");
	xrtThreadDestroy(pThread);

	pThread = xrtThreadCreate(testThreadReturn, (ptr)(intptr_t)7, 0);
	testRequire(pThread != NULL, "return worker create failed");
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "return worker wait failed");
	testRequire(xrtThreadExitCode(pThread) == 7, "return worker exit code mismatch");
	xrtThreadDestroy(pThread);

	pThread = xrtThreadCreate(testThreadSelfWait, NULL, 0);
	testRequire(pThread != NULL, "self-wait worker create failed");
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "self-wait worker wait failed");
	testRequire(xrtThreadExitCode(pThread) == 23, "self-wait was not rejected");
	xrtThreadDestroy(pThread);
	xrtClearError();

	pThread = xrtThreadCreate(testThreadReturn, (ptr)(intptr_t)31, 0);
	testRequire(pThread != NULL, "multi-wait target create failed");
	for ( size_t i = 0; i < 3; i++ ) {
		arrWaiters[i].Target = pThread;
		arrWaiters[i].Result = XWAIT_ERROR;
		pWaiters[i] = xrtThreadCreate(testThreadWaiter, &arrWaiters[i], 0);
		testRequire(pWaiters[i] != NULL, "multi-wait waiter create failed");
	}
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(xrtThreadWait(pWaiters[i]) == XWAIT_OK, "multi-wait waiter did not finish");
		testRequire(arrWaiters[i].Result == XWAIT_OK, "multi-wait waiter result mismatch");
		xrtThreadDestroy(pWaiters[i]);
	}
	testRequire(xrtThreadExitCode(pThread) == 31, "multi-wait target exit code mismatch");
	xrtThreadDestroy(pThread);

	pThread = xrtThreadCreate(testThreadReturn, (ptr)(intptr_t)43, 0);
	testRequire(pThread != NULL, "retained thread create failed");
	pRetained = xrtThreadRef(pThread);
	testRequire(pRetained == pThread, "thread retain failed");
	xrtThreadDestroy(pThread);
	testRequire(xrtThreadWait(pRetained) == XWAIT_OK, "retained thread wait failed");
	testRequire(xrtThreadExitCode(pRetained) == 43, "retained thread exit code mismatch");
	xrtThreadDestroy(pRetained);

	pThread = xrtThreadCreate(testThreadDetached, NULL, 0);
	testRequire(pThread != NULL, "detached thread create failed");
	xrtThreadDestroy(pThread);
	xrtSleep(20);

	printf("[PASS] thread\n");
	return 0;
}
