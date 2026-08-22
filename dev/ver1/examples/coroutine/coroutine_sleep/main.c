/*
 * XRT Example - Coroutine Sleep
 * XRT 范例 - 协程休眠
 *
 * Description / 说明:
 *   EN: Demonstrates xrtCoSleep within scheduler-managed coroutines.
 *   CN: 演示在调度器管理的协程中使用 xrtCoSleep。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_coroutine_sleep.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_coroutine_sleep -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	str sName;
	double fStart;
	double fEnd;
} sleep_ctx;


static void SleepTask(ptr pParam)
{
	sleep_ctx* pCtx = (sleep_ctx*)pParam;

	pCtx->fStart = xrtTimer();
	xrtCoSleep(50);
	pCtx->fEnd = xrtTimer();
	printf("%s slept %.3f sec\n", pCtx->sName, pCtx->fEnd - pCtx->fStart);
}



int main()
{
	xcosched* pSched = NULL;
	sleep_ctx tA = { "sleep_a", 0, 0 };
	sleep_ctx tB = { "sleep_b", 0, 0 };

	xrtInit();

	pSched = xrtCoSchedCreate();
	xrtCoSchedSpawn(pSched, SleepTask, &tA, 0);
	xrtCoSchedSpawn(pSched, SleepTask, &tB, 0);
	xrtCoSchedRun(pSched);

	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return 0;
}
