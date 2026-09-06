/*
 * 范例：concurrency/sync_tour —— 同步原语堆形态全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【互斥量】  xrtMutexCreate / Destroy / TryLock（Lock/Unlock 复用）
 *   【条件变量】 xrtCondCreate / Destroy / Wait / WaitFor
 *   【信号量】  xrtSemCreate / Destroy / Wait / TryWait / WaitFor /
 *              PostMany（一次补充多枚）
 *   【读写锁】  xrtRWLockCreate / Destroy / TryWrite（读族复用）
 *   【事件】    xrtEventInit / Unit / Create / Destroy / Set / Reset /
 *              TryWait / Wait / WaitFor / WaitUntil（自动/手动复位）
 * 模块宏：XRT_MODULE_SYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/sync_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   sync: mutex try=0/1 (heap)
 *   sync: cond wait=0 wait-for=1 signaled=1
 *   sync: sem try=1 post-many=3 wait=0
 *   sync: rwlock try-write=0/1
 *   sync: event auto try/wait + manual reset = 6 ops
 *
 * 本范例补齐堆形态（Create/Destroy）与 Try/批量族；
 *   内嵌形态与升级降级见 sync/rwlock/semaphore/condition 范例。
 *   条件变量的 WaitFor 超时与 Signal 唤醒各演示一次。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_TIMEOUT_US	UINT64_C(200000)

/* 条件变量工作线程：先短窗 WaitFor 到期，再无限 Wait 等 Signal。 */
typedef struct examplecond {
	xmutex* pMutex;
	xcond* pCond;
	volatile xwaitresult iForResult;
	volatile xwaitresult iWaitResult;
	volatile bool bPhase2;
	volatile bool bDone;
	volatile bool bSignaled;  /* 谓词：防丢失唤醒 */
} examplecond;

static int32 exampleCondThread(ptr pArg)
{
	examplecond* pJob = (examplecond*)pArg;

	(void)xrtMutexLock(pJob->pMutex);
	pJob->iForResult = xrtCondWaitFor(pJob->pCond, pJob->pMutex,
		EXAMPLE_TIMEOUT_US);  /* 无人 Signal：到期 */
	pJob->bPhase2 = true;
	/* 谓词循环：先查后等——即使 Signal 先于 Wait 到达也不丢失。 */
	pJob->iWaitResult = XWAIT_OK;
	while ( !pJob->bSignaled ) {
		pJob->iWaitResult = xrtCondWait(pJob->pCond,
			pJob->pMutex);
	}
	(void)xrtMutexUnlock(pJob->pMutex);
	pJob->bDone = true;
	return 0;
}

static bool exampleSpinUntil(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xmutex* pMutex = NULL;
	xcond* pCond = NULL;
	xsem* pSem = NULL;
	xrwlock* pLock = NULL;
	xevent* pAuto = NULL;
	xevent tManual;
	xthread* pThread = NULL;
	examplecond CondJob;
	xwaitresult Wait;
	int iResult = 1;

	memset(&CondJob, 0, sizeof(CondJob));

	/* ---- 互斥量堆形态 + TryLock ---- */
	pMutex = xrtMutexCreate();
	if ( (pMutex == NULL) ||
		!xrtMutexLock(pMutex) ||
		xrtMutexTryLock(pMutex) ||  /* 已持有：Try 必失败 */
		!xrtMutexUnlock(pMutex) ||
		!xrtMutexTryLock(pMutex) ||  /* 释放后：Try 必成功 */
		!xrtMutexUnlock(pMutex) ) {
		goto Cleanup;
	}
	printf("sync: mutex try=0/1 (heap)\n");

	/* ---- 条件变量堆形态：WaitFor 超时 + Signal 唤醒 ---- */
	pCond = xrtCondCreate();
	if ( pCond == NULL ) {
		goto Cleanup;
	}
	/* 工作线程两相：For 到期（TIMEOUT）→ 无限 Wait 被主线程
	 * Signal 唤醒（OK）。主线程不直接等待条件变量。 */
	memset(&CondJob, 0, sizeof(CondJob));
	CondJob.pMutex = pMutex;
	CondJob.pCond = pCond;
	pThread = xrtThreadCreate(exampleCondThread, &CondJob, 0u);
	if ( (pThread == NULL) ||
		!exampleSpinUntil(&CondJob.bPhase2) ) {
		goto Cleanup;  /* 第一相到期，线程已进入第二相 */
	}
	/* 在互斥锁内改谓词再 Signal：与工作线程的先查后等
	 * 配对，任何交错都不会丢失唤醒。 */
	if ( !xrtMutexLock(pMutex) ) {
		goto Cleanup;
	}
	CondJob.bSignaled = true;
	(void)xrtMutexUnlock(pMutex);
	(void)xrtCondSignal(pCond);
	if ( !exampleSpinUntil(&CondJob.bDone) ||
		(CondJob.iForResult != XWAIT_TIMEOUT) ||
		(CondJob.iWaitResult != XWAIT_OK) ) {
		goto Cleanup;
	}
	(void)xrtThreadWait(pThread);
	xrtThreadDestroy(pThread);
	pThread = NULL;
	printf("sync: cond wait=0 wait-for=1 signaled=1\n");

	/* ---- 信号量堆形态：TryWait/PostMany/Wait ---- */
	pSem = xrtSemCreate(0u, 4u);
	if ( (pSem == NULL) ||
		(xrtSemTryWait(pSem) != XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	if ( !xrtSemPostMany(pSem, 3u) ||
		(xrtSemTryWait(pSem) != XWAIT_OK) ||
		(xrtSemWait(pSem) != XWAIT_OK) ||
		(xrtSemWaitFor(pSem, EXAMPLE_TIMEOUT_US) != XWAIT_OK) ) {
		goto Cleanup;  /* 3 枚：Try + Wait + WaitFor 各消耗一枚 */
	}
	printf("sync: sem try=1 post-many=3 wait=0\n");

	/* ---- 读写锁堆形态：TryWrite ---- */
	pLock = xrtRWLockCreate();
	if ( (pLock == NULL) ||
		!xrtRWLockRead(pLock) ||
		xrtRWLockTryWrite(pLock) ||  /* 读持有时写必失败 */
		!xrtRWLockReadUnlock(pLock) ||
		!xrtRWLockTryWrite(pLock) ||  /* 空闲：TryWrite 成功 */
		!xrtRWLockWriteUnlock(pLock) ) {
		goto Cleanup;
	}
	printf("sync: rwlock try-write=0/1\n");

	/* ---- 事件：自动复位堆形态 + 手动复位内嵌形态 ---- */
	pAuto = xrtEventCreate(false, false);
	if ( (pAuto == NULL) ||
		(xrtEventTryWait(pAuto) != XWAIT_TIMEOUT) ||
		!xrtEventSet(pAuto) ||
		(xrtEventTryWait(pAuto) != XWAIT_OK) ||  /* 消费信号 */
		(xrtEventTryWait(pAuto) != XWAIT_TIMEOUT) ||
		(xrtEventWaitFor(pAuto, EXAMPLE_TIMEOUT_US) !=
			XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	if ( !xrtEventSet(pAuto) ||
		(xrtEventWaitFor(pAuto, EXAMPLE_TIMEOUT_US) !=
			XWAIT_OK) ) {
		goto Cleanup;
	}
	if ( !xrtEventInit(&tManual, true, false) ) {
		goto Cleanup;
	}
	/* 手动复位：Set 后多次 Wait 都立即通过，Reset 后恢复阻塞。 */
	if ( !xrtEventSet(&tManual) ||
		(xrtEventWait(&tManual) != XWAIT_OK) ||
		(xrtEventWaitFor(&tManual, UINT64_C(1)) != XWAIT_OK) ||
		(xrtEventWaitUntil(&tManual,
			xrtDeadlineAfter(UINT64_C(1))) != XWAIT_OK) ||
		!xrtEventReset(&tManual) ||
		(xrtEventTryWait(&tManual) != XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	printf("sync: event auto try/wait + manual reset = 6 ops\n");
	iResult = 0;

Cleanup:
	xrtEventUnit(&tManual);
	xrtEventDestroy(pAuto);
	xrtRWLockDestroy(pLock);
	xrtSemDestroy(pSem);
	xrtCondDestroy(pCond);
	xrtMutexDestroy(pMutex);
	return iResult;
}
