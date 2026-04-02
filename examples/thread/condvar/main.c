/*
 * XRT Example - Condition Variable
 * XRT 范例 - 条件变量
 *
 * Description / 说明:
 *   EN: Demonstrates signal and broadcast with waiting worker threads.
 *   CN: 演示条件变量的 Signal 与 Broadcast 唤醒。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_condvar.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_condvar -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	xmutex hMutex;
	xcond hCond;
	volatile int iStage;
	volatile int iAwakeCount;
} cond_ctx;


static uint32 WaitWorker(ptr pParam)
{
	cond_ctx* pCtx = (cond_ctx*)pParam;

	xrtMutexLock(pCtx->hMutex);
	while ( pCtx->iStage == 0 ) {
		xrtCondWait(pCtx->hCond, pCtx->hMutex);
	}
	pCtx->iAwakeCount++;
	xrtMutexUnlock(pCtx->hMutex);

	return 0;
}



int main()
{
	cond_ctx tCtx = { 0 };
	xthread hA = NULL;
	xthread hB = NULL;

	xrtInit();

	tCtx.hMutex = xrtMutexCreate();
	tCtx.hCond = xrtCondCreate();

	hA = xrtThreadCreate((ptr)WaitWorker, &tCtx, 0);
	hB = xrtThreadCreate((ptr)WaitWorker, &tCtx, 0);

	xrtSleep(50);

	xrtMutexLock(tCtx.hMutex);
	tCtx.iStage = 1;
	xrtCondSignal(tCtx.hCond);
	xrtMutexUnlock(tCtx.hMutex);

	xrtSleep(50);

	xrtMutexLock(tCtx.hMutex);
	xrtCondBroadcast(tCtx.hCond);
	xrtMutexUnlock(tCtx.hMutex);

	xrtThreadWait(hA);
	xrtThreadWait(hB);

	printf("awake_count = %d\n", tCtx.iAwakeCount);

	xrtThreadDestroy(hA);
	xrtThreadDestroy(hB);
	xrtCondDestroy(tCtx.hCond);
	xrtMutexDestroy(tCtx.hMutex);
	xrtUnit();
	return 0;
}
