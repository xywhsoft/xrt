/*
 * XRT Example - Coroutine Scheduler
 * XRT 范例 - 协程调度器
 *
 * Description / 说明:
 *   EN: Demonstrates spawning multiple coroutines and stepping the scheduler.
 *   CN: 演示调度器管理多个协程并逐步推进执行。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_scheduler.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_scheduler -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	str sName;
	int iStep;
} co_sched_ctx;


static void ScheduledTask(ptr pParam)
{
	co_sched_ctx* pCtx = (co_sched_ctx*)pParam;
	int i;

	for ( i = 0; i < 3; i++ ) {
		pCtx->iStep++;
		printf("%s step %d\n", pCtx->sName, pCtx->iStep);
		xrtCoYield();
	}
}



int main()
{
	xcosched* pSched = NULL;
	co_sched_ctx tA = { "A", 0 };
	co_sched_ctx tB = { "B", 0 };

	xrtInit();

	pSched = xrtCoSchedCreate();
	xrtCoSchedSpawn(pSched, ScheduledTask, &tA, 0);
	xrtCoSchedSpawn(pSched, ScheduledTask, &tB, 0);

	printf("alive_before = %d\n", xrtCoSchedGetAlive(pSched));
	xrtCoSchedStep(pSched);
	printf("alive_after_step = %d\n", xrtCoSchedGetAlive(pSched));
	xrtCoSchedRun(pSched);
	printf("alive_after_run = %d\n", xrtCoSchedGetAlive(pSched));

	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return 0;
}
