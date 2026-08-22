#include <stdio.h>

#include <xrt.h>



/* 展示条件通知不保存状态，以及超时返回前会重新持有 mutex。 */
int main(void)
{
	xmutex Mutex;
	xcond Cond;
	xwaitresult Result;
	bool bLocked;

	if ( !xrtMutexInit(&Mutex) ) {
		return 1;
	}
	if ( !xrtCondInit(&Cond) ) {
		(void)xrtMutexUnit(&Mutex);
		return 1;
	}

	/* 没有等待者时，Signal 和 Broadcast 不会保存后续可消费的通知。 */
	if ( !xrtCondSignal(&Cond) || !xrtCondBroadcast(&Cond) ||
		!xrtMutexLock(&Mutex) ) {
		(void)xrtCondUnit(&Cond);
		(void)xrtMutexUnit(&Mutex);
		return 1;
	}
	Result = xrtCondWaitUntil(
		&Cond,
		&Mutex,
		xrtDeadlineAfter(UINT64_C(1000))
	);
	bLocked = xrtMutexUnlock(&Mutex);

	printf("wait=%d mutex-restored=%d\n", (int)Result, (int)bLocked);
	return xrtCondUnit(&Cond) &&
		xrtMutexUnit(&Mutex) &&
		(Result == XWAIT_TIMEOUT) &&
		bLocked ? 0 : 2;
}
