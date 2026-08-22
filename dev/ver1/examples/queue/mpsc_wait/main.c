/*
 * XRT Example - MPSC Wait Queue
 * XRT 范例 - 可等待 MPSC 队列
 *
 * Description / 说明:
 *   EN: Demonstrates timeout wait and blocking pop on the MPSC wait wrapper.
 *   CN: 演示 MPSC wait wrapper 的超时等待和阻塞式弹出。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/queue_mpsc_wait.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/queue_mpsc_wait -lm -lpthread
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdint.h>
#include <stdio.h>


typedef struct {
	xmpscqwait hQueue;
	xqueue_result iResult;
	uintptr_t iValue;
} wait_ctx;


static uint32 WaitConsumer(ptr pArg)
{
	wait_ctx* pCtx = (wait_ctx*)pArg;
	ptr pItem = NULL;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 11u;
	}

	pCtx->iResult = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, 1000u);
	pCtx->iValue = (uintptr_t)pItem;
	return 0u;
}


int main(void)
{
	xqueue_config tCfg;
	xmpscqwait pQueue = NULL;
	wait_ctx tCtx;
	xthread hConsumer = NULL;
	ptr pItem = NULL;
	xqueue_result iTimeoutRet = XQUEUE_ERROR;
	xqueue_result iPushRet = XQUEUE_ERROR;
	int iRet = 0;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tCtx, 0, sizeof(tCtx));
	tCfg.iCapacity = 4u;
	pQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( pQueue == NULL ) {
		printf("create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iTimeoutRet = xrtMPSCQWaitPopTimeout(pQueue, &pItem, 0u);
	tCtx.hQueue = pQueue;
	hConsumer = xrtThreadCreate(WaitConsumer, &tCtx, 0);
	xrtSleep(100u);
	iPushRet = xrtMPSCQWaitTryPush(pQueue, (ptr)(uintptr_t)12345u);

	if ( hConsumer != NULL ) {
		xrtThreadWait(hConsumer);
		xrtThreadDestroy(hConsumer);
		hConsumer = NULL;
	}

	printf("timeout_result = %d\n", (int)iTimeoutRet);
	printf("push_result = %d\n", (int)iPushRet);
	printf("wait_result = %d\n", (int)tCtx.iResult);
	printf("wait_value = %llu\n", (unsigned long long)tCtx.iValue);
	printf("count_after_pop = %u\n", xrtMPSCQWaitApproxCount(pQueue));

cleanup:
	if ( hConsumer != NULL ) {
		xrtThreadWait(hConsumer);
		xrtThreadDestroy(hConsumer);
	}
	if ( pQueue != NULL ) {
		xrtMPSCQWaitClose(pQueue);
		xrtMPSCQWaitDestroy(pQueue);
	}
	xrtUnit();
	return iRet;
}
