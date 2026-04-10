/*
 * XRT Example - Coroutine Event Wait
 * XRT 范例 - 协程事件等待
 *
 * Description / 说明:
 *   EN: Demonstrates event timeout, deadline waiting, and event signaling
 *       inside a scheduler-managed coroutine flow.
 *   CN: 演示在调度器协程流程中进行事件超时等待、deadline 等待和事件唤醒。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_event_wait.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_event_wait -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>

typedef struct {
	xcoevent pEvent;
	bool bTimeoutOk;
	bool bDeadlineOk;
	bool bSignalOk;
	volatile long iSignalCount;
} ex_co_event_ctx;


static void procSignaler(ptr pArg)
{
	ex_co_event_ctx* pCtx = (ex_co_event_ctx*)pArg;

	if ( pCtx == NULL ) {
		return;
	}

	xrtCoSleep(60u);
	xrtCoEventSet(pCtx->pEvent);
	++pCtx->iSignalCount;
}


static void procWaiter(ptr pArg)
{
	ex_co_event_ctx* pCtx = (ex_co_event_ctx*)pArg;
	int64 iDeadlineMs = 0;

	if ( pCtx == NULL ) {
		return;
	}

	pCtx->bTimeoutOk = !xrtCoWaitEventTimeout(pCtx->pEvent, 20u);
	iDeadlineMs = (int64)(xrtTimer() * 1000.0) + 25;
	pCtx->bDeadlineOk = xrtCoWaitDeadline(iDeadlineMs);
	pCtx->bSignalOk = xrtCoWaitEvent(pCtx->pEvent);
}


int main(void)
{
	ex_co_event_ctx tCtx;
	xcosched* pSched = NULL;
	xcoro pWaiter = NULL;
	xcoro pSignaler = NULL;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	xrtInit();

	pSched = xrtCoSchedCreate();
	tCtx.pEvent = xrtCoEventCreate(FALSE, FALSE);
	if ( pSched == NULL || tCtx.pEvent == NULL ) {
		printf("prepare = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pWaiter = xrtCoSchedSpawn(pSched, procWaiter, &tCtx, 0u);
	pSignaler = xrtCoSchedSpawn(pSched, procSignaler, &tCtx, 0u);
	if ( pWaiter == NULL || pSignaler == NULL ) {
		printf("spawn = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtCoSchedRun(pSched);

	printf("backend = %s\n", xrtCoGetBackendName());
	printf("timeout_ok = %s\n", tCtx.bTimeoutOk ? "TRUE" : "FALSE");
	printf("deadline_ok = %s\n", tCtx.bDeadlineOk ? "TRUE" : "FALSE");
	printf("signal_ok = %s\n", tCtx.bSignalOk ? "TRUE" : "FALSE");
	printf("signal_count = %ld\n", tCtx.iSignalCount);
	printf("alive_after_run = %d\n", xrtCoSchedGetAlive(pSched));

	if ( !tCtx.bTimeoutOk || !tCtx.bDeadlineOk || !tCtx.bSignalOk ) {
		iRet = 1;
	}

cleanup:
	if ( pWaiter != NULL ) {
		(void)xrtCoClose(pWaiter);
	}
	if ( pSignaler != NULL ) {
		(void)xrtCoClose(pSignaler);
	}
	if ( tCtx.pEvent != NULL ) {
		xrtCoEventDestroy(tCtx.pEvent);
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
	}
	xrtUnit();
	return iRet;
}
