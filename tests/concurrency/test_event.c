#include "../test.h"
#include "../test_thread.h"



/* 等待跨线程设置的事件。 */
static int testEventWaiter(ptr pData)
{
	xevent* pEvent = (xevent*)pData;

	return xrtEventWaitFor(pEvent, UINT64_C(500000)) == XWAIT_OK ? 37 : 1;
}



/* 验证自动复位、手动复位、重置和跨线程等待。 */
int main(void)
{
	xevent tEvent;
	xevent* pEvent;
	testthread arrThreads[2];
	xdeadline iDeadline;
	uint64 iStarted;
	uint64 iElapsed;
	int iAutoWakeCount = 0;

	memset(&tEvent, 0, sizeof(tEvent));
	testRequire(xrtEventInit(&tEvent, false, false), "auto-reset event init failed");
	testRequire(xrtEventTryWait(&tEvent) == XWAIT_TIMEOUT, "clear auto event was signaled");
	testRequire(xrtEventSet(&tEvent), "auto event set failed");
	testRequire(xrtEventTryWait(&tEvent) == XWAIT_OK, "auto event first wait failed");
	testRequire(xrtEventTryWait(&tEvent) == XWAIT_TIMEOUT, "auto event did not reset");
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < 2; i++ ) {
		arrThreads[i].Proc = testEventWaiter;
		arrThreads[i].Data = &tEvent;
	}
	testThreadsStart(arrThreads, 2);
	xrtSleep(20);
	testRequire(xrtEventSet(&tEvent), "auto event waiter set failed");
	testThreadsJoin(arrThreads, 2);
	for ( size_t i = 0; i < 2; i++ ) {
		if ( arrThreads[i].Result == 37 ) {
			iAutoWakeCount++;
		} else {
			testRequire(arrThreads[i].Result == 1, "auto event waiter failed");
		}
	}
	testRequire(iAutoWakeCount == 1, "auto event released the wrong waiter count");
	testRequire(xrtEventUnit(&tEvent), "auto event unit failed");

	pEvent = xrtEventCreate(true, true);
	testRequire(pEvent != NULL, "manual event create failed");
	testRequire(xrtEventTryWait(pEvent) == XWAIT_OK, "manual event first wait failed");
	testRequire(xrtEventTryWait(pEvent) == XWAIT_OK, "manual event did not preserve signal");
	testRequire(xrtEventReset(pEvent), "manual event reset failed");
	testRequire(xrtEventTryWait(pEvent) == XWAIT_TIMEOUT, "manual event remained signaled");
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < 2; i++ ) {
		arrThreads[i].Proc = testEventWaiter;
		arrThreads[i].Data = pEvent;
	}
	testThreadsStart(arrThreads, 2);
	xrtSleep(20);
	testRequire(xrtEventSet(pEvent), "cross-thread event set failed");
	testThreadsJoin(arrThreads, 2);
	for ( size_t i = 0; i < 2; i++ ) {
		testRequire(arrThreads[i].Result == 37, "manual event lost a waiter");
	}
	testRequire(xrtEventReset(pEvent), "manual event second reset failed");
	iStarted = xrtClock();
	iDeadline = xrtDeadlineAfter(UINT64_C(20000));
	testRequire(
		xrtEventWaitUntil(pEvent, iDeadline) == XWAIT_TIMEOUT,
		"event deadline result mismatch"
	);
	iElapsed = xrtClock() - iStarted;
	testRequire(iElapsed >= UINT64_C(10000), "event deadline returned too early");
	testRequire(iElapsed < UINT64_C(2000000), "event deadline returned too late");
	testRequire(xrtEventDestroy(pEvent), "manual event destroy failed");

	printf("[PASS] event\n");
	return 0;
}
