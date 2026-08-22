#include "../test.h"



#define TEST_SIGNAL_THREAD_COUNT 8u
#define TEST_SIGNAL_RAISE_COUNT 2000u



typedef struct testsignalthreadstate {
	xatomic32 Observed;
	xatomic32 Failed;
} testsignalthreadstate;



/* 每个回调按批次数量累计，而不是把一次回调误当成一次信号。 */
static void testSignalThreadCallback(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	testsignalthreadstate* pState = (testsignalthreadstate*)pData;

	(void)pWatch;
	(void)xrtAtomic32FetchAdd(
		&pState->Observed,
		pEvent->Count,
		XMEMORY_RELEASE
	);
}



/* 并发线程反复向当前进程发送受管中断信号。 */
static int32 testSignalRaiseThread(ptr pData)
{
	testsignalthreadstate* pState = (testsignalthreadstate*)pData;

	for ( uint32 i = 0; i < TEST_SIGNAL_RAISE_COUNT; i++ ) {
		if ( !xrtSignalRaise(XSIGNAL_INT) ) {
			xrtAtomic32Store(&pState->Failed, 1u, XMEMORY_RELEASE);
			return -1;
		}
	}
	return 0;
}



/* 在有界时间内等待调度线程汇总全部信号。 */
static bool testSignalThreadWait(
	const testsignalthreadstate* pState,
	uint32 iExpected
)
{
	uint64 iDeadline = xrtClock() + UINT64_C(5000000);

	while ( xrtAtomic32Load(&pState->Observed, XMEMORY_ACQUIRE) < iExpected ) {
		if ( xrtClock() >= iDeadline ) {
			return false;
		}
		xrtSleep(1u);
	}
	return true;
}



/* 验证多线程突发投递经过唤醒合并后仍保持精确数量。 */
int main(void)
{
	testsignalthreadstate State;
	xthread* arrThreads[TEST_SIGNAL_THREAD_COUNT];
	xsignalwatch* pWatch;
	uint32 iExpected = TEST_SIGNAL_THREAD_COUNT * TEST_SIGNAL_RAISE_COUNT;

	xrtAtomic32Init(&State.Observed, 0u);
	xrtAtomic32Init(&State.Failed, 0u);
	pWatch = xrtSignalOn(XSIGNAL_INT, testSignalThreadCallback, &State);
	testRequire(pWatch != NULL, "threaded signal watch creation failed");
	testRequire(xrtSignalClear(XSIGNAL_INT), "threaded signal clear failed");
	for ( uint32 i = 0; i < TEST_SIGNAL_THREAD_COUNT; i++ ) {
		arrThreads[i] = xrtThreadCreate(testSignalRaiseThread, &State, 0u);
		testRequire(arrThreads[i] != NULL,
			"threaded signal raiser creation failed");
	}
	for ( uint32 i = 0; i < TEST_SIGNAL_THREAD_COUNT; i++ ) {
		testRequire(xrtThreadWait(arrThreads[i]) == XWAIT_OK,
			"threaded signal raiser wait failed");
		testRequire(xrtThreadExitCode(arrThreads[i]) == 0,
			"threaded signal raiser reported failure");
		xrtThreadDestroy(arrThreads[i]);
	}
	testRequire(xrtAtomic32Load(&State.Failed, XMEMORY_ACQUIRE) == 0u,
		"threaded signal raise failed");
	testRequire(testSignalThreadWait(&State, iExpected),
		"threaded signal delivery lost notifications");
	testRequire(xrtAtomic32Load(&State.Observed, XMEMORY_ACQUIRE) == iExpected,
		"threaded signal callback count mismatch");
	testRequire(xrtSignalCount(XSIGNAL_INT) == (uint64)iExpected,
		"threaded signal total mismatch");
	xrtSignalFree(pWatch);
	testRequire(xrtSignalShutdown(), "threaded signal shutdown failed");
	testMemoryDebugDrain("threaded signal test leaked memory");

	printf("[PASS] signal threads\n");
	return 0;
}
