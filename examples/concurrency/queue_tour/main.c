/*
 * 范例：concurrency/queue_tour —— 三族无锁队列全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【容量】    xrtQueueCapacity（最小容量向上取整为 2 次幂）
 *   【SPSC】    xrtSPSCQueueInit / Unit（内嵌存储生命周期）/
 *                Create / Destroy（堆分配）/
 *                TryPush / TryPop / PushBatch / PopBatch /
 *                Count / Close / IsClosed / IsDrained / Drain / Reset
 *   【MPSC】    xrtMPSCQueueInit / Unit / Create / Destroy /
 *                TryPush / TryPop / Count / Close / IsClosed /
 *                IsDrained / Drain / Reset
 *   【MPMC】    xrtMPMCQueueInit / Unit / Create / Destroy /
 *                TryPush / TryPop / Count / Close / IsClosed /
 *                IsDrained / Drain / Reset
 * 模块宏：XRT_MODULE_QUEUE_SPSC / _MPSC / _MPMC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/queue_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   queue: capacity(3)=4 capacity(16)=16
 *   spsc: push/pop=ok batch=6+3 count=0 closed+drained reset=ok
 *   mpsc: 2 producers x 500 -> 1000 received (drained)
 *   mpmc: 2x2 threads 1000 -> 1000 received (drained)
 *
 * 三族队列接口同构：流控用 OK/EMPTY/FULL/CLOSED 表达；
 *   MPSC/MPMC 段用真实多线程按总收发数核对正确性，
 *   Drain 回调累计丢弃量，Close 后 Push 返回 CLOSED。
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xrt.h>

#define EXAMPLE_ITEMS	500
#define EXAMPLE_THREADS	2

/* 求和上下文：Drain 回调累计值。 */
typedef struct examplesum {
	xatomic64 Total;
	size_t Count;
} examplesum;

static void exampleDrainAdd(ptr pItem, ptr pContext)
{
	examplesum* pSum = (examplesum*)pContext;

	pSum->Count = pSum->Count + 1u;
	(void)xrtAtomic64FetchAdd(&pSum->Total, (int64)pItem,
		XMEMORY_RELAXED);
}

/* MPSC 生产者：向队列压入 [1, iCount] 的数值。 */
typedef struct exampleproducer {
	xmpscqueue* pQueue;
	int iOffset;
	int iCount;
	xatomic32* pFinished;
} exampleproducer;

static int32 exampleMpscProducer(ptr pArg)
{
	exampleproducer* pJob = (exampleproducer*)pArg;
	int i;

	for ( i = 0; i < pJob->iCount; ++i ) {
		ptr Value = (ptr)(intptr_t)(pJob->iOffset + i);

		while ( xrtMPSCQueueTryPush(pJob->pQueue, Value) ==
			XQUEUE_FULL ) {
			xrtThreadYield();  /* 容量有限：等消费者腾出空间 */
		}
	}
	(void)xrtAtomic32FetchAdd(pJob->pFinished, 1, XMEMORY_RELEASE);
	return 0;
}

/* MPMC 生产者。 */
typedef struct examplempmcproducer {
	xmpmcqueue* pQueue;
	int iOffset;
	int iCount;
} examplempmcproducer;

static int32 exampleMpmcProducer(ptr pArg)
{
	examplempmcproducer* pJob = (examplempmcproducer*)pArg;
	int i;

	for ( i = 0; i < pJob->iCount; ++i ) {
		ptr Value = (ptr)(intptr_t)(pJob->iOffset + i);

		while ( xrtMPMCQueueTryPush(pJob->pQueue, Value) ==
			XQUEUE_FULL ) {
			xrtThreadYield();
		}
	}
	return 0;
}

/* MPMC 消费者：累计弹出数量。 */
typedef struct exampleconsumer {
	xmpmcqueue* pQueue;
	xatomic64 Got;
} exampleconsumer;

static int32 exampleMpmcConsumer(ptr pArg)
{
	exampleconsumer* pJob = (exampleconsumer*)pArg;

	for ( ;; ) {
		ptr Value;

		if ( xrtMPMCQueueTryPop(pJob->pQueue, &Value) ==
			XQUEUE_OK ) {
			(void)xrtAtomic64FetchAdd(&pJob->Got, 1,
				XMEMORY_RELAXED);
		}
		else if ( xrtMPMCQueueIsClosed(pJob->pQueue) &&
			(xrtMPMCQueueCount(pJob->pQueue) == 0u) ) {
			break;
		}
		else {
			xrtThreadYield();
		}
	}
	return 0;
}

int main(void)
{
	examplesum Sum;
	int i;

	/* 容量换算：向上取整到 2 次幂。 */
	printf("queue: capacity(3)=%zu capacity(16)=%zu\n",
		xrtQueueCapacity(3u), xrtQueueCapacity(16u));

	/* ---- SPSC：内嵌存储 + 批量 + 关闭/排空/重置全流程 ---- */
	{
		xspscqueue Queue;
		ptr Items[7];
		ptr Out[4];
		xqueuebatchresult Batch;

		if ( !xrtSPSCQueueInit(&Queue, 4u) ) {
			return 1;
		}
		if ( (xrtSPSCQueueTryPush(&Queue, (ptr)1) != XQUEUE_OK) ||
			(xrtSPSCQueueTryPush(&Queue, (ptr)2) != XQUEUE_OK) ||
			(xrtSPSCQueueCount(&Queue) != 2u) ) {
			return 2;
		}
		{
			ptr Value = 0;

			if ( (xrtSPSCQueueTryPop(&Queue, &Value) !=
				XQUEUE_OK) || ((intptr_t)Value != 1) ) {
				return 3;
			}
		}
		/* 批量：一次压 3 个（剩余空间恰好）、一次弹 3 个。 */
		Items[0] = (ptr)3;
		Items[1] = (ptr)4;
		Items[2] = (ptr)5;
		Batch = xrtSPSCQueuePushBatch(&Queue, Items, 3u);
		if ( (Batch.Result != XQUEUE_OK) || (Batch.Count != 3u) ) {
			return 4;
		}
		Batch = xrtSPSCQueuePopBatch(&Queue, Out, 4u);
		/* 弹到恰好清空也返回 OK；Count 报实际弹出量。 */
		if ( (Batch.Result != XQUEUE_OK) || (Batch.Count != 4u) ||
			((intptr_t)Out[0] != 2) || ((intptr_t)Out[3] != 5) ) {
			return 5;
		}
		/* 关闭 → 再压返回 CLOSED；排空态由 IsDrained 报告。 */
		xrtSPSCQueueClose(&Queue);
		if ( (xrtSPSCQueueTryPush(&Queue, (ptr)9) !=
			XQUEUE_CLOSED) || !xrtSPSCQueueIsClosed(&Queue) ||
			!xrtSPSCQueueIsDrained(&Queue) ||
			(xrtSPSCQueueCount(&Queue) != 0u) ) {
			return 6;
		}
		printf("spsc: push/pop=ok batch=6+3 count=0"
			" closed+drained");
		/* 重置：独占且为空时可重开。 */
		if ( !xrtSPSCQueueReset(&Queue) ||
			(xrtSPSCQueueTryPush(&Queue, (ptr)7) !=
				XQUEUE_OK) ||
			xrtSPSCQueueIsClosed(&Queue) ) {
			return 7;
		}
		memset(&Sum, 0, sizeof(Sum));
		if ( (xrtSPSCQueueDrain(&Queue, exampleDrainAdd, &Sum) !=
			1u) || (Sum.Count != 1u) ||
			(xrtAtomic64Load(&Sum.Total, XMEMORY_RELAXED) != 7) ) {
			return 8;
		}
		printf(" reset=ok\n");
		xrtSPSCQueueUnit(&Queue);

		/* 堆分配形态：Create/Destroy 配对。 */
		{
			xspscqueue* pHeap = xrtSPSCQueueCreate(8u);

			if ( (pHeap == NULL) ||
				(xrtSPSCQueueTryPush(pHeap, (ptr)1) !=
					XQUEUE_OK) ) {
				return 8;
			}
			xrtSPSCQueueDestroy(pHeap);
		}
	}

	/* ---- MPSC：内嵌形态先走一遍基本流程 ---- */
	{
		xmpscqueue Queue;

		if ( !xrtMPSCQueueInit(&Queue, 8u) ||
			(xrtMPSCQueueTryPush(&Queue, (ptr)11) !=
				XQUEUE_OK) ) {
			return 9;
		}
		xrtMPSCQueueClose(&Queue);
		memset(&Sum, 0, sizeof(Sum));
		(void)xrtMPSCQueueDrain(&Queue, exampleDrainAdd, &Sum);
		if ( (Sum.Count != 1u) || !xrtMPSCQueueReset(&Queue) ) {
			return 9;
		}
		xrtMPSCQueueUnit(&Queue);
	}

	/* ---- MPSC：两个生产者线程 + 主线程消费 ---- */
	{
		xmpscqueue* pQueue = xrtMPSCQueueCreate(64u);
		exampleproducer Jobs[EXAMPLE_THREADS];
		xthread* Threads[EXAMPLE_THREADS];
		xatomic32 iFinished;
		int64 iTotal = 0;
		size_t iGot = 0;

		if ( pQueue == NULL ) {
			return 9;
		}
		(void)xrtAtomic32Store(&iFinished, 0, XMEMORY_RELEASE);
		for ( i = 0; i < EXAMPLE_THREADS; ++i ) {
			Jobs[i].pQueue = pQueue;
			Jobs[i].iOffset = i * EXAMPLE_ITEMS;
			Jobs[i].iCount = EXAMPLE_ITEMS;
			Jobs[i].pFinished = &iFinished;
			Threads[i] = xrtThreadCreate(exampleMpscProducer,
				&Jobs[i], 0u);
			if ( Threads[i] == NULL ) {
				return 10;
			}
		}
		/* 容量有限：主线程边生产边消费，直到两个生产者完成且队列清空。 */
		while ( (xrtAtomic32Load(&iFinished, XMEMORY_ACQUIRE) <
			EXAMPLE_THREADS) ||
			(xrtMPSCQueueCount(pQueue) != 0u) ) {
			ptr Value;

			while ( xrtMPSCQueueTryPop(pQueue, &Value) ==
				XQUEUE_OK ) {
				iTotal = iTotal + (int64)(intptr_t)Value;
				++iGot;
			}
			xrtThreadYield();
		}
		for ( i = 0; i < EXAMPLE_THREADS; ++i ) {
			(void)xrtThreadWait(Threads[i]);
			xrtThreadDestroy(Threads[i]);
		}
		xrtMPSCQueueClose(pQueue);
		memset(&Sum, 0, sizeof(Sum));
		(void)xrtMPSCQueueDrain(pQueue, exampleDrainAdd, &Sum);
		if ( !xrtMPSCQueueIsClosed(pQueue) ||
			!xrtMPSCQueueIsDrained(pQueue) ||
			!xrtMPSCQueueReset(pQueue) ) {
			return 11;
		}
		/* 等差数列核对：0..999 之和。 */
		if ( (iGot != (size_t)(EXAMPLE_THREADS *
			EXAMPLE_ITEMS)) ||
			(iTotal != (int64)EXAMPLE_THREADS * EXAMPLE_ITEMS *
				(EXAMPLE_THREADS * EXAMPLE_ITEMS - 1) / 2) ) {
			return 12;
		}
		printf("mpsc: %d producers x %d -> %zu received"
			" (drained)\n", EXAMPLE_THREADS, EXAMPLE_ITEMS,
			iGot);
		xrtMPMCQueueDestroy(NULL);  /* 空指针是空操作 */
		xrtMPSCQueueDestroy(pQueue);
	}

	/* ---- MPMC：2 生产者 + 2 消费者 ---- */
	{
		xmpmcqueue* pQueue = xrtMPMCQueueCreate(64u);
		examplempmcproducer Producers[EXAMPLE_THREADS];
		exampleconsumer Consumers[EXAMPLE_THREADS];
		xthread* ProducerThreads[EXAMPLE_THREADS];
		xthread* ConsumerThreads[EXAMPLE_THREADS];
		int64 iTotal = 0;
		size_t i;
		size_t iSent;

		if ( pQueue == NULL ) {
			return 13;
		}
		for ( i = 0; i < EXAMPLE_THREADS; ++i ) {
			Producers[i].pQueue = pQueue;
			Producers[i].iOffset = (int)i * EXAMPLE_ITEMS;
			Producers[i].iCount = EXAMPLE_ITEMS;
			Consumers[i].pQueue = pQueue;
			(void)xrtAtomic64Store(&Consumers[i].Got, 0,
				XMEMORY_RELAXED);
			ProducerThreads[i] = xrtThreadCreate(
				exampleMpmcProducer, &Producers[i], 0u);
			ConsumerThreads[i] = xrtThreadCreate(
				exampleMpmcConsumer, &Consumers[i], 0u);
			if ( (ProducerThreads[i] == NULL) ||
				(ConsumerThreads[i] == NULL) ) {
				return 14;
			}
		}
		for ( i = 0; i < EXAMPLE_THREADS; ++i ) {
			(void)xrtThreadWait(ProducerThreads[i]);
			xrtThreadDestroy(ProducerThreads[i]);
		}
		/* 生产完毕后关闭，消费者把余量取完即退出。 */
		xrtMPMCQueueClose(pQueue);
		for ( i = 0; i < EXAMPLE_THREADS; ++i ) {
			(void)xrtThreadWait(ConsumerThreads[i]);
			xrtThreadDestroy(ConsumerThreads[i]);
			iTotal = iTotal + xrtAtomic64Load(
				&Consumers[i].Got, XMEMORY_RELAXED);
		}
		iSent = (size_t)EXAMPLE_THREADS * EXAMPLE_ITEMS;
		if ( (iTotal != (int64)iSent) ||
			(xrtMPMCQueueCount(pQueue) != 0u) ||
			!xrtMPMCQueueIsDrained(pQueue) ) {
			return 15;
		}
		printf("mpmc: 2x2 threads %zu -> %lld received"
			" (drained)\n", iSent, (long long)iTotal);
		xrtMPMCQueueDestroy(pQueue);
	}

	/* ---- MPMC：内嵌形态 + Drain/Reset ---- */
	{
		xmpmcqueue Queue;

		if ( !xrtMPMCQueueInit(&Queue, 8u) ||
			(xrtMPMCQueueTryPush(&Queue, (ptr)21) !=
				XQUEUE_OK) ||
			(xrtMPMCQueueTryPush(&Queue, (ptr)22) !=
				XQUEUE_OK) ) {
			return 15;
		}
		xrtMPMCQueueClose(&Queue);
		memset(&Sum, 0, sizeof(Sum));
		if ( (xrtMPMCQueueDrain(&Queue, exampleDrainAdd, &Sum) !=
			2u) || (Sum.Count != 2u) ||
			(xrtAtomic64Load(&Sum.Total, XMEMORY_RELAXED) != 43) ||
			!xrtMPMCQueueReset(&Queue) ) {
			return 15;
		}
		xrtMPMCQueueUnit(&Queue);
	}
	return 0;
}
