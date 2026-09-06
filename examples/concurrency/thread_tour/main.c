/*
 * 范例：concurrency/thread_tour —— 线程生命周期/TLS/停止协作补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【生命周期】  xrtThreadRef / WaitFor / WaitUntil / State /
 *                ExitCode / ThreadId
 *   【停止协作】  xrtThreadStop（请求停止）/ StopRequested（对象侧）/
 *                Stopping（线程内自检）
 *   【TLS 补集】  xrtThreadKeyTake（取出并清空）/ KeysClear（全部清空）
 *   【当前线程】  xrtThreadCurrent（工作线程内自检）
 * 模块宏：XRT_MODULE_THREAD
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/thread_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   thread: wait-for/until state=1 exit=42 id!=0
 *   thread: stop-requested inside=1 outside=1
 *   thread: tls take/clear ok
 *   thread: current inside=1 main=0
 *
 * 停止是协作式：主线程 Stop 请求 → 工作线程轮询
 *   StopRequested 自愿退出；Stopping 只在目标线程内为真。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* TLS 值哨兵。 */
static uint64 g_Sentinel = 0xABCD;

/* 简单工作线程：睡眠后返回退出码 42。 */
static int32 exampleWorker(ptr pData)
{
	(void)pData;
	xrtSleep(50u);  /* 毫秒：50ms */
	return 42;
}

/* 停止协作线程：轮询 StopRequested，自检 Stopping 与 Current。 */
typedef struct examplestop {
	xthread* pSelf;
	volatile bool bStopped;
	volatile bool bStoppingInside;
	volatile bool bCurrentInside;
	volatile bool bDone;
} examplestop;

static int32 exampleStopWorker(ptr pData)
{
	examplestop* pJob = (examplestop*)pData;
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( xrtDeadlineExpired(iDeadline) == false ) {
		/* StopRequested(NULL) 恒为假——线程内自检必须用
		 * Stopping()（等价于当前对象的 StopRequested）。 */
		if ( xrtThreadStopping() ) {
			pJob->bStopped =
				xrtThreadStopRequested(pJob->pSelf);
			pJob->bStoppingInside = xrtThreadStopping();
			pJob->bCurrentInside = xrtThreadCurrent() != NULL;
			pJob->bDone = true;
			return 7;
		}
		xrtThreadYield();
	}
	return -1;
}

/* TLS 工作线程：设置 → Take 取出 → 验证清空。 */
static xthreadkey* g_pKey = NULL;
static volatile bool g_TakeOk = false;

static int32 exampleTlsWorker(ptr pData)
{
	(void)pData;
	if ( xrtThreadKeySet(g_pKey, &g_Sentinel) ) {
		ptr pTaken = xrtThreadKeyTake(g_pKey);

		g_TakeOk = (pTaken == &g_Sentinel) &&
			(xrtThreadKeyGet(g_pKey) == NULL);
	}
	/* 再次设置后由 KeysClear 在本线程清空。 */
	(void)xrtThreadKeySet(g_pKey, &g_Sentinel);
	xrtThreadKeysClear();
	return g_TakeOk ? 1 : 0;
}

int main(void)
{
	xthread* pThread = NULL;
	xthread* pRef = NULL;
	xthread* pStop = NULL;
	xthread* pTls = NULL;
	examplestop StopJob;
	int iResult = 1;

	/* ---- 生命周期：WaitFor 超时 → WaitUntil 成功 ---- */
	pThread = xrtThreadCreate(exampleWorker, NULL, 0u);
	if ( (pThread == NULL) ||
		(xrtThreadWaitFor(pThread, 1000u) == XWAIT_OK) ) {
		goto Cleanup;  /* 50ms 睡眠：1ms 窗口内必未完成 */
	}
	if ( (xrtThreadWaitUntil(pThread,
			xrtDeadlineAfter(UINT64_C(3000000))) !=
			XWAIT_OK) ||
		(xrtThreadState(pThread) != XTHREAD_FINISHED) ||
		(xrtThreadExitCode(pThread) != 42) ||
		(xrtThreadId(pThread) == 0u) ) {
		goto Cleanup;
	}
	pRef = xrtThreadRef(pThread);
	if ( pRef != pThread ) {
		goto Cleanup;
	}
	xrtThreadDestroy(pRef);  /* Ref 那份 */
	pRef = NULL;
	printf("thread: wait-for/until state=1 exit=42 id!=0\n");
	xrtThreadDestroy(pThread);
	pThread = NULL;

	/* ---- 停止协作 ---- */
	memset(&StopJob, 0, sizeof(StopJob));
	pStop = xrtThreadCreate(exampleStopWorker, &StopJob, 0u);
	if ( pStop == NULL ) {
		goto Cleanup;
	}
	StopJob.pSelf = pStop;
	{
		xdeadline iGrace = xrtDeadlineAfter(UINT64_C(100000));

		while ( xrtDeadlineExpired(iGrace) == false ) {
			xrtThreadYield();
		}
	}
	if ( !xrtThreadStop(pStop) ) {
		goto Cleanup;
	}
	{
		xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

		while ( StopJob.bDone == false ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}
	if ( (void)xrtThreadWait(pStop), false ) {
		goto Cleanup;
	}
	if ( !StopJob.bStopped || !StopJob.bStoppingInside ||
		!StopJob.bCurrentInside ||
		(xrtThreadExitCode(pStop) != 7) ) {
		goto Cleanup;
	}
	printf("thread: stop-requested inside=1 outside=1\n");
	xrtThreadDestroy(pStop);
	pStop = NULL;

	/* ---- TLS：Take / KeysClear ---- */
	g_pKey = xrtThreadKeyCreate(NULL);
	g_TakeOk = false;
	if ( g_pKey == NULL ) {
		goto Cleanup;
	}
	pTls = xrtThreadCreate(exampleTlsWorker, NULL, 0u);
	if ( (pTls == NULL) ||
		(xrtThreadWait(pTls) != XWAIT_OK) ||
		!g_TakeOk ||
		(xrtThreadExitCode(pTls) != 1) ||
		(xrtThreadKeyGet(g_pKey) != NULL) ) {
		goto Cleanup;  /* 主线程自己的槽位为空 */
	}
	printf("thread: tls take/clear ok\n");
	printf("thread: current inside=1 main=%d\n",
		xrtThreadCurrent() != NULL ? 1 : 0);
	xrtThreadDestroy(pTls);
	pTls = NULL;
	xrtThreadKeyDestroy(g_pKey);
	g_pKey = NULL;
	iResult = 0;

Cleanup:
	xrtThreadDestroy(pTls);
	xrtThreadDestroy(pStop);
	xrtThreadDestroy(pRef);
	xrtThreadDestroy(pThread);
	if ( g_pKey != NULL ) {
		xrtThreadKeyDestroy(g_pKey);
	}
	return iResult;
}
