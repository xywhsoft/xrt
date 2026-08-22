/*
 * XRT Example - Future Task Thread
 * XRT 范例 - 线程任务 Future
 *
 * Description / 说明:
 *   EN: Demonstrates running a task in a worker thread and reading its future result.
 *   CN: 演示在线程中运行任务，并读取返回的 future 结果。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_task_thread.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_task_thread -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static int32 ThreadTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(50u);
	*pValue += 23;
	pOut->pValue = pValue;
	return XRT_NET_OK;
}


int main(void)
{
	xfuture* pFuture = NULL;
	int iValue = 100;
	uint64 iCreateMs = 0u;
	uint64 iCompleteMs = 0u;
	int iRet = 0;

	xrtInit();

	pFuture = xTaskRunThread(ThreadTask, &iValue, 0u);
	if ( pFuture == NULL ) {
		printf("task = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !xFutureWaitTimeout(pFuture, 2000) ) {
		printf("wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	iCreateMs = xFutureGetCreateTimeMs(pFuture);
	iCompleteMs = xFutureGetCompleteTimeMs(pFuture);

	printf("status = %d\n", (int)xFutureStatus(pFuture));
	printf("value = %d\n", *(int*)xFutureValue(pFuture));
	printf("elapsed_ms = %llu\n", (unsigned long long)(iCompleteMs - iCreateMs));

cleanup:
	if ( pFuture != NULL ) {
		xFutureRelease(pFuture);
	}
	xrtUnit();
	return iRet;
}
