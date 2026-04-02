/*
 * XRT Example - MPSC Queue Batch
 * XRT 范例 - MPSC 队列批量操作
 *
 * Description / 说明:
 *   EN: Demonstrates batch push/pop, close, drain, and reset on an MPSC queue.
 *   CN: 演示 MPSC 队列的批量 push/pop、关闭、drain 和 reset。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/queue_mpsc_batch.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/queue_mpsc_batch -lm -lpthread
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
	xmpscq pQueue = NULL;
	ptr arrPush[5];
	ptr arrPop[4] = {0};
	drain_ctx tDrain = { 0 };
	uint32 iPushCount = 0u;
	uint32 iPopCount = 0u;
	uint32 iDrainCount = 0u;
	bool bReset = FALSE;
	uint32 i;
	int iRet = 0;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	memset(arrPush, 0, sizeof(arrPush));
	tCfg.iCapacity = 4u;
	pQueue = xrtMPSCQCreate(&tCfg);
	if ( pQueue == NULL ) {
		printf("create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	for ( i = 0u; i < 5u; i++ ) {
		arrPush[i] = (ptr)(uintptr_t)(100u + i);
	}

	iPushCount = xrtMPSCQPushBatch(pQueue, arrPush, 5u);
	iPopCount = xrtMPSCQPopBatch(pQueue, arrPop, 3u);
	(void)xrtMPSCQTryPush(pQueue, (ptr)(uintptr_t)200u);

	xrtMPSCQClose(pQueue);
	iDrainCount = xrtMPSCQDrain(pQueue, DrainProc, &tDrain);
	bReset = xrtMPSCQReset(pQueue);

	printf("capacity = %u\n", pQueue->iCapacity);
	printf("push_batch_count = %u\n", iPushCount);
	printf("pop_batch_count = %u\n", iPopCount);
	for ( i = 0u; i < iPopCount; i++ ) {
		printf("pop[%u] = %llu\n", i, (unsigned long long)(uintptr_t)arrPop[i]);
	}
	printf("drain_count = %u\n", iDrainCount);
	for ( i = 0u; i < tDrain.iCount; i++ ) {
		printf("drain[%u] = %llu\n", i, (unsigned long long)tDrain.arrItems[i]);
	}
	printf("reset_result = %s\n", bReset ? "TRUE" : "FALSE");
	printf("count_after_reset = %u\n", xrtMPSCQApproxCount(pQueue));

cleanup:
	if ( pQueue != NULL ) {
		xrtMPSCQDestroy(pQueue);
	}
	xrtUnit();
	return iRet;
}
