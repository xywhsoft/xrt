/*
 * XRT Example - MPMC Queue Basic
 * XRT 范例 - MPMC 队列基础
 *
 * Description / 说明:
 *   EN: Demonstrates multi-producer multi-consumer queue usage with worker threads.
 *   CN: 演示使用工作线程驱动的多生产者多消费者队列。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/queue_mpmc_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/queue_mpmc_basic -lm -lpthread
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdint.h>
#include <stdio.h>


typedef struct {
	xmpmcq hQueue;
	uint32 iBase;
	uint32 iCount;
	uint32 iPushed;
	int iFailure;
} producer_ctx;


typedef struct {
	xmpmcq hQueue;
	uint32 iPopped;
	uint64 iSum;
	int iFailure;
} consumer_ctx;


static uint32 ProducerProc(ptr pArg)
{
	producer_ctx* pCtx = (producer_ctx*)pArg;
	uint32 i;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 11u;
	}

	for ( i = 0u; i < pCtx->iCount; i++ ) {
		for ( ; ; ) {
			xqueue_result iRet = xrtMPMCQTryPush(pCtx->hQueue, (ptr)(uintptr_t)(pCtx->iBase + i + 1u));

			if ( iRet == XQUEUE_OK ) {
				pCtx->iPushed++;
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}

			pCtx->iFailure = (int)iRet;
			return 21u;
		}
	}

	return 0u;
}


static uint32 ConsumerProc(ptr pArg)
{
	consumer_ctx* pCtx = (consumer_ctx*)pArg;
	ptr pItem = NULL;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 12u;
	}

	for ( ; ; ) {
		xqueue_result iRet = xrtMPMCQTryPop(pCtx->hQueue, &pItem);

		if ( iRet == XQUEUE_OK ) {
			pCtx->iPopped++;
			pCtx->iSum += (uint64)(uintptr_t)pItem;
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}

		pCtx->iFailure = (int)iRet;
		return 22u;
	}

	return 0u;
}


int main(void)
{
	xqueue_config tCfg;
	xmpmcq pQueue = NULL;
	producer_ctx arrProducer[2];
	consumer_ctx arrConsumer[2];
	xthread arrProducerThread[2] = {0};
	xthread arrConsumerThread[2] = {0};
	uint32 iProduced = 0u;
	uint32 iConsumed = 0u;
	uint64 iConsumedSum = 0u;
	uint32 i;
	int iRet = 0;

	xrtInit();

	memset(&tCfg, 0, sizeof(tCfg));
	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrConsumer, 0, sizeof(arrConsumer));
	tCfg.iCapacity = 64u;
	pQueue = xrtMPMCQCreate(&tCfg);
	if ( pQueue == NULL ) {
		printf("create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	for ( i = 0u; i < 2u; i++ ) {
		arrProducer[i].hQueue = pQueue;
		arrProducer[i].iBase = i * 1000u;
		arrProducer[i].iCount = 50u;
		arrProducerThread[i] = xrtThreadCreate(ProducerProc, &arrProducer[i], 0);

		arrConsumer[i].hQueue = pQueue;
		arrConsumerThread[i] = xrtThreadCreate(ConsumerProc, &arrConsumer[i], 0);
	}

	for ( i = 0u; i < 2u; i++ ) {
		if ( arrProducerThread[i] != NULL ) {
			xrtThreadWait(arrProducerThread[i]);
			xrtThreadDestroy(arrProducerThread[i]);
			arrProducerThread[i] = NULL;
		}
	}

	xrtMPMCQClose(pQueue);

	for ( i = 0u; i < 2u; i++ ) {
		if ( arrConsumerThread[i] != NULL ) {
			xrtThreadWait(arrConsumerThread[i]);
			xrtThreadDestroy(arrConsumerThread[i]);
			arrConsumerThread[i] = NULL;
		}
	}

	for ( i = 0u; i < 2u; i++ ) {
		iProduced += arrProducer[i].iPushed;
		iConsumed += arrConsumer[i].iPopped;
		iConsumedSum += arrConsumer[i].iSum;
	}

	printf("capacity = %u\n", pQueue->iCapacity);
	printf("produced = %u\n", iProduced);
	printf("consumed = %u\n", iConsumed);
	printf("consumed_sum = %llu\n", (unsigned long long)iConsumedSum);
	printf("producer0_failure = %d\n", arrProducer[0].iFailure);
	printf("producer1_failure = %d\n", arrProducer[1].iFailure);
	printf("consumer0_failure = %d\n", arrConsumer[0].iFailure);
	printf("consumer1_failure = %d\n", arrConsumer[1].iFailure);

cleanup:
	for ( i = 0u; i < 2u; i++ ) {
		if ( arrProducerThread[i] != NULL ) {
			xrtThreadWait(arrProducerThread[i]);
			xrtThreadDestroy(arrProducerThread[i]);
		}
		if ( arrConsumerThread[i] != NULL ) {
			xrtThreadWait(arrConsumerThread[i]);
			xrtThreadDestroy(arrConsumerThread[i]);
		}
	}
	if ( pQueue != NULL ) {
		xrtMPMCQDestroy(pQueue);
	}
	xrtUnit();
	return iRet;
}
