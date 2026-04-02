/*
 * XRT Example - SPSC Queue Basic
 * XRT 范例 - SPSC 队列基础
 *
 * Description / 说明:
 *   EN: Demonstrates create, push, pop, close, drain, and reset on an SPSC queue.
 *   CN: 演示 SPSC 队列的创建、压入、弹出、关闭、drain 和 reset。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/queue_spsc_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/queue_spsc_basic -lm -lpthread
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdint.h>
#include <stdio.h>


typedef struct {
	uintptr_t arrItems[8];
	uint32 iCount;
} drain_ctx;


static void DrainProc(ptr pItem, ptr pUserData)
{
	drain_ctx* pCtx = (drain_ctx*)pUserData;

	if ( (pCtx != NULL) && (pCtx->iCount < 8u) ) {
		pCtx->arrItems[pCtx->iCount++] = (uintptr_t)pItem;
	}
}


int main(void)
{
	xqueue_config tCfg;
	xspscq pQueue = NULL;
	drain_ctx tDrain = { 0 };
	ptr pItem = NULL;
	xqueue_result iPush3 = XQUEUE_ERROR;
	xqueue_result iPop1 = XQUEUE_ERROR;
	uint32 iDrainCount = 0u;
	bool bReset = FALSE;
	uint32 i;
	int iRet = 0;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 4u;
	pQueue = xrtSPSCQCreate(&tCfg);
	if ( pQueue == NULL ) {
		printf("create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xrtSPSCQTryPush(pQueue, (ptr)(uintptr_t)11u);
	(void)xrtSPSCQTryPush(pQueue, (ptr)(uintptr_t)22u);
	iPush3 = xrtSPSCQTryPush(pQueue, (ptr)(uintptr_t)33u);
	iPop1 = xrtSPSCQTryPop(pQueue, &pItem);

	xrtSPSCQClose(pQueue);
	iDrainCount = xrtSPSCQDrain(pQueue, DrainProc, &tDrain);
	bReset = xrtSPSCQReset(pQueue);

	printf("capacity = %u\n", pQueue->iCapacity);
	printf("push_3_result = %d\n", (int)iPush3);
	printf("pop_1_result = %d\n", (int)iPop1);
	printf("pop_1_value = %llu\n", (unsigned long long)(uintptr_t)pItem);
	printf("closed = %s\n", xrtQueueIsClosed(&pQueue->tBase) ? "TRUE" : "FALSE");
	printf("drain_count = %u\n", iDrainCount);
	for ( i = 0u; i < tDrain.iCount; i++ ) {
		printf("drain[%u] = %llu\n", i, (unsigned long long)tDrain.arrItems[i]);
	}
	printf("reset_result = %s\n", bReset ? "TRUE" : "FALSE");
	printf("count_after_reset = %u\n", xrtSPSCQApproxCount(pQueue));

cleanup:
	if ( pQueue != NULL ) {
		xrtSPSCQDestroy(pQueue);
	}
	xrtUnit();
	return iRet;
}
