#include "../test.h"
#include "../test_thread.h"



typedef struct testcondstate {
	xmutex Mutex;
	xcond Cond;
	int Waiting;
	bool Ready;
	int WakeCount;
} testcondstate;



/* 在谓词循环中等待条件通知。 */
static int testCondWaiter(ptr pData)
{
	testcondstate* pState = (testcondstate*)pData;

	if ( !xrtMutexLock(&pState->Mutex) ) {
		return 1;
	}
	pState->Waiting++;
	while ( !pState->Ready ) {
		xwaitresult Result = xrtCondWaitFor(
			&pState->Cond,
			&pState->Mutex,
			UINT64_C(2000000)
		);

		if ( Result != XWAIT_OK ) {
			(void)xrtMutexUnlock(&pState->Mutex);
			return 2;
		}
	}
	pState->WakeCount++;
	return xrtMutexUnlock(&pState->Mutex) ? 0 : 3;
}



/* 等待工作线程进入谓词循环。 */
static void testCondWaitReady(testcondstate* pState, int iExpected)
{
	for ( ;; ) {
		bool bReady;

		testRequire(xrtMutexLock(&pState->Mutex), "condition readiness lock failed");
		bReady = pState->Waiting == iExpected;
		testRequire(xrtMutexUnlock(&pState->Mutex), "condition readiness unlock failed");
		if ( bReady ) {
			return;
		}
		xrtSleep(1);
	}
}



/* 验证条件变量超时、通知和广播基础语义。 */
int main(void)
{
	testcondstate tState;
	testthread arrThreads[3];

	memset(&tState, 0, sizeof(tState));
	memset(arrThreads, 0, sizeof(arrThreads));
	testRequire(xrtMutexInit(&tState.Mutex), "condition mutex init failed");
	testRequire(xrtCondInit(&tState.Cond), "condition init failed");
	testRequire(
		xrtCondWaitFor(&tState.Cond, &tState.Mutex, UINT64_C(1000)) == XWAIT_ERROR,
		"condition wait without mutex ownership succeeded"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"unowned condition wait error mismatch"
	);
	xrtClearError();
	testRequire(xrtMutexLock(&tState.Mutex), "condition timeout lock failed");
	testRequire(
		xrtCondWaitFor(&tState.Cond, &tState.Mutex, UINT64_C(10000)) == XWAIT_TIMEOUT,
		"condition timeout result mismatch"
	);
	testRequire(xrtMutexUnlock(&tState.Mutex), "condition timeout unlock failed");

	arrThreads[0].Proc = testCondWaiter;
	arrThreads[0].Data = &tState;
	testThreadsStart(arrThreads, 1);
	testCondWaitReady(&tState, 1);
	testRequire(xrtMutexLock(&tState.Mutex), "condition signal lock failed");
	tState.Ready = true;
	testRequire(xrtCondSignal(&tState.Cond), "condition signal failed");
	testRequire(xrtMutexUnlock(&tState.Mutex), "condition signal unlock failed");
	testThreadsJoin(arrThreads, 1);
	testRequire(arrThreads[0].Result == 0, "condition waiter failed");
	testRequire(tState.WakeCount == 1, "condition waiter wake count mismatch");

	tState.Waiting = 0;
	tState.Ready = false;
	tState.WakeCount = 0;
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < 3; i++ ) {
		arrThreads[i].Proc = testCondWaiter;
		arrThreads[i].Data = &tState;
	}
	testThreadsStart(arrThreads, 3);
	testCondWaitReady(&tState, 3);
	testRequire(xrtMutexLock(&tState.Mutex), "condition broadcast lock failed");
	tState.Ready = true;
	testRequire(xrtCondBroadcast(&tState.Cond), "condition broadcast failed");
	testRequire(xrtMutexUnlock(&tState.Mutex), "condition broadcast unlock failed");
	testThreadsJoin(arrThreads, 3);
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(arrThreads[i].Result == 0, "condition broadcast waiter failed");
	}
	testRequire(tState.WakeCount == 3, "condition broadcast lost waiters");
	testRequire(xrtCondUnit(&tState.Cond), "condition unit failed");
	testRequire(xrtMutexUnit(&tState.Mutex), "condition mutex unit failed");

	printf("[PASS] cond\n");
	return 0;
}
