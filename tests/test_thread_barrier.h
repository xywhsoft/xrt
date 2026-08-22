#ifndef XRT_TEST_THREAD_BARRIER_H
#define XRT_TEST_THREAD_BARRIER_H

/* 可复用测试屏障让指定数量的工作线程从同一条件变量批次起跑。 */
typedef struct testthreadbarrier {
	xmutex Lock;
	xcond Ready;
	size_t Target;
	size_t Waiting;
	bool Go;
} testthreadbarrier;



/* 初始化指定参与工作线程数的一次性测试屏障。 */
static void testThreadBarrierInit(
	testthreadbarrier* pBarrier,
	size_t iTarget
)
{
	memset(pBarrier, 0, sizeof(*pBarrier));
	pBarrier->Target = iTarget;
	testRequire((iTarget != 0) && xrtMutexInit(&pBarrier->Lock),
		"test barrier mutex init failed");
	testRequire(xrtCondInit(&pBarrier->Ready),
		"test barrier cond init failed");
}



/* 工作线程登记就绪并等待主线程放行。 */
static bool testThreadBarrierWait(testthreadbarrier* pBarrier)
{
	(void)xrtMutexLock(&pBarrier->Lock);
	pBarrier->Waiting++;
	(void)xrtCondBroadcast(&pBarrier->Ready);
	while ( !pBarrier->Go ) {
		if ( xrtCondWait(&pBarrier->Ready, &pBarrier->Lock) != XWAIT_OK ) {
			(void)xrtMutexUnlock(&pBarrier->Lock);
			return false;
		}
	}
	(void)xrtMutexUnlock(&pBarrier->Lock);
	return true;
}



/* 主线程等待全部参与者就绪后一次性放行。 */
static void testThreadBarrierOpen(testthreadbarrier* pBarrier)
{
	(void)xrtMutexLock(&pBarrier->Lock);
	while ( pBarrier->Waiting != pBarrier->Target ) {
		testRequire(xrtCondWait(&pBarrier->Ready, &pBarrier->Lock) == XWAIT_OK,
			"test barrier open wait failed");
	}
	pBarrier->Go = true;
	(void)xrtCondBroadcast(&pBarrier->Ready);
	(void)xrtMutexUnlock(&pBarrier->Lock);
}



/* 回收一次性测试屏障。 */
static void testThreadBarrierUnit(testthreadbarrier* pBarrier)
{
	testRequire(xrtCondUnit(&pBarrier->Ready),
		"test barrier cond unit failed");
	testRequire(xrtMutexUnit(&pBarrier->Lock),
		"test barrier mutex unit failed");
}

#endif
