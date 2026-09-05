#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/condition —— 条件变量：等待-通知与锁恢复
 * ----------------------------------------------------------------
 * 演示 API：
 *   条件等待族（等待时放锁、唤醒后自动重新持锁）
 * 模块宏：XRT_MODULE_COND
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/condition/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   wait=1 mutex-restored=1
 *
 * 两条核心契约（范例标题即考点）：
 *   条件通知不保存状态——没人等时通知丢失，
 *   所以条件必须用共享变量表达、锁内重查；
 *   超时返回前重新持有 mutex——wait=1 与
 *   mutex-restored=1 分别验证。
 */


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
