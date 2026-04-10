/*
 * XRT Example - Producer Consumer
 * XRT 范例 - 生产者消费者
 *
 * Description / 说明:
 *   EN: Demonstrates a classic producer-consumer queue using mutex and condition variable.
 *   CN: 演示使用互斥体和条件变量实现经典生产者消费者队列。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_producer_consumer.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_producer_consumer -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#define QUEUE_CAP 8

typedef struct {
	xmutex hMutex;
	xcond hCond;
	int arrQueue[QUEUE_CAP];
	int iHead;
	int iTail;
	int iCount;
	bool bDone;
	int iConsumedSum;
} queue_ctx;


static uint32 Producer(ptr pParam)
{
	queue_ctx* pCtx = (queue_ctx*)pParam;
	int i;

	for ( i = 1; i <= 5; i++ ) {
		xrtMutexLock(pCtx->hMutex);
		pCtx->arrQueue[pCtx->iTail] = i * 10;
		pCtx->iTail = (pCtx->iTail + 1) % QUEUE_CAP;
		pCtx->iCount++;
		xrtCondSignal(pCtx->hCond);
		xrtMutexUnlock(pCtx->hMutex);
		xrtSleep(10);
	}

	xrtMutexLock(pCtx->hMutex);
	pCtx->bDone = TRUE;
	xrtCondBroadcast(pCtx->hCond);
	xrtMutexUnlock(pCtx->hMutex);
	return 0;
}



static uint32 Consumer(ptr pParam)
{
	queue_ctx* pCtx = (queue_ctx*)pParam;

	for ( ; ; ) {
		xrtMutexLock(pCtx->hMutex);
		while ( (pCtx->iCount == 0) && (!pCtx->bDone) ) {
			xrtCondWait(pCtx->hCond, pCtx->hMutex);
		}
		if ( (pCtx->iCount == 0) && pCtx->bDone ) {
			xrtMutexUnlock(pCtx->hMutex);
			break;
		}
		pCtx->iConsumedSum += pCtx->arrQueue[pCtx->iHead];
		pCtx->iHead = (pCtx->iHead + 1) % QUEUE_CAP;
		pCtx->iCount--;
		xrtMutexUnlock(pCtx->hMutex);
	}

	return 0;
}



int main()
{
	queue_ctx tCtx = { 0 };
	xthread hProducer = NULL;
	xthread hConsumer = NULL;

	xrtInit();

	tCtx.hMutex = xrtMutexCreate();
	tCtx.hCond = xrtCondCreate();

	hProducer = xrtThreadCreate((ptr)Producer, &tCtx, 0);
	hConsumer = xrtThreadCreate((ptr)Consumer, &tCtx, 0);

	xrtThreadWait(hProducer);
	xrtThreadWait(hConsumer);

	printf("consumed_sum = %d\n", tCtx.iConsumedSum);

	xrtThreadDestroy(hProducer);
	xrtThreadDestroy(hConsumer);
	xrtCondDestroy(tCtx.hCond);
	xrtMutexDestroy(tCtx.hMutex);
	xrtUnit();
	return 0;
}
