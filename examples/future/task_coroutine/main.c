/*
 * XRT Example - Future Task Coroutine
 * XRT 范例 - 协程任务 Future
 *
 * Description / 说明:
 *   EN: Demonstrates running a task inside a coroutine scheduler and waiting on its future.
 *   CN: 演示在协程调度器中运行任务，并等待其 future 结果。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_task_coroutine.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_task_coroutine -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static int32 CoTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	xrtCoSleep(20u);
	*pValue += 7;
	pOut->pValue = pValue;
	return XRT_NET_OK;
}


int main(void)
{
	xcosched* pSched = NULL;
	xfuture* pFuture = NULL;
	int iValue = 5;
	int iRet = 0;

	xrtInit();

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		printf("scheduler = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pFuture = xTaskRunCo(pSched, CoTask, &iValue, 0u);
	if ( pFuture == NULL ) {
		printf("task = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtCoSchedRun(pSched);
	if ( !xFutureWaitTimeout(pFuture, 1000) ) {
		printf("wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("status = %d\n", (int)xFutureStatus(pFuture));
	printf("value = %d\n", *(int*)xFutureValue(pFuture));
	printf("alive_after_run = %d\n", xrtCoSchedGetAlive(pSched));

cleanup:
	if ( pFuture != NULL ) {
		xFutureRelease(pFuture);
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
	}
	xrtUnit();
	return iRet;
}
