/*
 * XRT Example - Thread Basic
 * XRT 范例 - 基础线程
 *
 * Description / 说明:
 *   EN: Demonstrates thread creation, cooperative stop and wait.
 *   CN: 演示线程创建、协作停止与等待结束。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_thread_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_thread_basic -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	xthread hThread;
	volatile int iLoopCount;
} demo_thread_ctx;


static uint32 Worker(ptr pParam)
{
	demo_thread_ctx* pCtx = (demo_thread_ctx*)pParam;

	while ( pCtx->hThread == NULL ) {
		xrtSleep(1);
	}

	while ( !xrtThreadShouldStop(pCtx->hThread) ) {
		pCtx->iLoopCount++;
		xrtSleep(20);
	}

	return (uint32)pCtx->iLoopCount;
}



int main()
{
	demo_thread_ctx tCtx = { 0 };

	xrtInit();

	tCtx.hThread = xrtThreadCreate((ptr)Worker, &tCtx, 0);
	xrtSleep(120);
	xrtThreadStop(tCtx.hThread);
	xrtThreadWait(tCtx.hThread);

	printf("loop_count = %d\n", tCtx.iLoopCount);
	printf("thread_state = %d\n", xrtThreadGetState(tCtx.hThread));

	xrtThreadDestroy(tCtx.hThread);
	xrtUnit();
	return 0;
}
