#ifndef TEST_QUEUE_CORE_H
#define TEST_QUEUE_CORE_H

#include "../xrt.h"
#include "test_atomic_compat.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	uint32 iCount;
	ptr arrItems[8];
} __test_queue_core_drain_ctx;

typedef struct {
	xspscq hQueue;
	uint32 iCount;
	volatile int iFailure;
} __test_queue_core_spsc_ctx;

typedef struct {
	xmpscq hQueue;
	uint32 iBaseValue;
	uint32 iCount;
	volatile int iFailure;
} __test_queue_core_mpsc_ctx;

typedef struct {
	xmpmcq hQueue;
	uint32 iBaseValue;
	uint32 iCount;
	volatile int iFailure;
} __test_queue_core_mpmc_prod_ctx;

typedef struct {
	xmpmcq hQueue;
	volatile long* arrSeen;
	volatile long* pConsumed;
	volatile long* pFailure;
	uint32 iTotalCount;
} __test_queue_core_mpmc_cons_ctx;

typedef struct {
	volatile uint32 iValue;
	uint32 iGuard;
} __test_queue_core_u32_atomic_probe;

#ifndef XRT_NO_QUEUE_WAIT
typedef struct {
	xmpscqwait hQueue;
	xsem hReady;
	uint32 iTimeoutMs;
	xqueue_result iResult;
	uintptr_t iValue;
	volatile int iFailure;
} __test_queue_core_mpscwait_cons_ctx;
#endif

#define __TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT 4u
#define __TEST_QUEUE_CORE_MPSC_ITEMS_PER_PRODUCER 1024u
#define __TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS (__TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT * __TEST_QUEUE_CORE_MPSC_ITEMS_PER_PRODUCER)
#define __TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT 4u
#define __TEST_QUEUE_CORE_MPMC_CONSUMER_COUNT 4u
#define __TEST_QUEUE_CORE_MPMC_ITEMS_PER_PRODUCER 1024u
#define __TEST_QUEUE_CORE_MPMC_TOTAL_ITEMS (__TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT * __TEST_QUEUE_CORE_MPMC_ITEMS_PER_PRODUCER)

static void __Test_QueueCore_DrainProc(ptr pItem, ptr pUserData)
{
	__test_queue_core_drain_ctx* pCtx = (__test_queue_core_drain_ctx*)pUserData;

	if ( pCtx == NULL || pCtx->iCount >= 8 ) {
		return;
	}

	pCtx->arrItems[pCtx->iCount] = pItem;
	pCtx->iCount++;
}

static long __Test_QueueCore_AtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}

static long __Test_QueueCore_AtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}

static long __Test_QueueCore_AtomicCAS(volatile long* pValue, long iExchange, long iComparand)
{
	return __xrtTestAtomicCompareExchangeLong(pValue, iExchange, iComparand);
}

static uint32 __Test_QueueCore_SPSCProducer(ptr pArg)
{
	__test_queue_core_spsc_ctx* pCtx = (__test_queue_core_spsc_ctx*)pArg;
	uint32 i;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 10;
	}

	for ( i = 1; i <= pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueue_result iRet = xrtSPSCQTryPush(pCtx->hQueue, (ptr)(uintptr_t)i);
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			pCtx->iFailure = (int)iRet;
			return 11;
		}
	}

	xrtSPSCQClose(pCtx->hQueue);
	return 0;
}

static uint32 __Test_QueueCore_MPSCProducer(ptr pArg)
{
	__test_queue_core_mpsc_ctx* pCtx = (__test_queue_core_mpsc_ctx*)pArg;
	uint32 i;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 20;
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueue_result iRet = xrtMPSCQTryPush(pCtx->hQueue, (ptr)(uintptr_t)(pCtx->iBaseValue + i + 1u));
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			pCtx->iFailure = (int)iRet;
			return 21;
		}
	}

	return 0;
}

static uint32 __Test_QueueCore_MPMCProducer(ptr pArg)
{
	__test_queue_core_mpmc_prod_ctx* pCtx = (__test_queue_core_mpmc_prod_ctx*)pArg;
	uint32 i;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 30;
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueue_result iRet = xrtMPMCQTryPush(pCtx->hQueue, (ptr)(uintptr_t)(pCtx->iBaseValue + i + 1u));
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			pCtx->iFailure = (int)iRet;
			return 31;
		}
	}

	return 0;
}

static uint32 __Test_QueueCore_MPMCConsumer(ptr pArg)
{
	__test_queue_core_mpmc_cons_ctx* pCtx = (__test_queue_core_mpmc_cons_ctx*)pArg;
	ptr pItem = NULL;

	if ( pCtx == NULL || pCtx->hQueue == NULL || pCtx->arrSeen == NULL || pCtx->pConsumed == NULL || pCtx->pFailure == NULL ) {
		return 40;
	}

	for ( ;; ) {
		xqueue_result iRet = xrtMPMCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			uint32 iValue = (uint32)(uintptr_t)pItem;
			if ( iValue == 0u || iValue > pCtx->iTotalCount ) {
				(void)__Test_QueueCore_AtomicCAS(pCtx->pFailure, 1, 0);
				return 41;
			}
			if ( __Test_QueueCore_AtomicCAS(&pCtx->arrSeen[iValue - 1u], 1, 0) != 0 ) {
				(void)__Test_QueueCore_AtomicCAS(pCtx->pFailure, 2, 0);
				return 42;
			}
			(void)__Test_QueueCore_AtomicInc(pCtx->pConsumed);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			if ( __Test_QueueCore_AtomicLoad(pCtx->pFailure) != 0 ) {
				return 43;
			}
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0;
		}

		(void)__Test_QueueCore_AtomicCAS(pCtx->pFailure, 3, 0);
		return 44;
	}
}

#ifndef XRT_NO_QUEUE_WAIT
static uint32 __Test_QueueCore_MPSCWaitConsumer(ptr pArg)
{
	__test_queue_core_mpscwait_cons_ctx* pCtx = (__test_queue_core_mpscwait_cons_ctx*)pArg;
	ptr pItem = NULL;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 50;
	}

	if ( pCtx->hReady != NULL ) {
		(void)xrtSemPost(pCtx->hReady);
	}

	pCtx->iResult = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, pCtx->iTimeoutMs);
	pCtx->iValue = (uintptr_t)pItem;
	if ( pCtx->iResult == XQUEUE_ERROR ) {
		pCtx->iFailure = 1;
		return 51;
	}
	return 0;
}
#endif

static int __Test_QueueCore_Check(const char* sName, int bOk)
{
	printf("  %s : %s\n", sName, bOk ? "PASS" : "FAIL");
	return bOk ? 0 : 1;
}

static int __Test_QueueCore_U32AtomicLoadStoreGuard(void)
{
	__test_queue_core_u32_atomic_probe tProbe;

	tProbe.iValue = 1u;
	tProbe.iGuard = 0x5c6d7e8fu;
	__xrtAtomicStoreU32(&tProbe.iValue, 7u);
	return __xrtAtomicLoadU32(&tProbe.iValue) == 7u &&
		tProbe.iValue == 7u &&
		tProbe.iGuard == 0x5c6d7e8fu;
}

static int Test_QueueCore(void)
{
	int iFail = 0;
	xqueue_config tCfg;
	xqueue_config tBadCfg;
	xspscq hQueue = NULL;
	xspscq_struct tEmbedded;
	xspscq_struct tInvalidSPSC;
	__test_queue_core_drain_ctx tDrainCtx;
	__test_queue_core_spsc_ctx tSpscCtx;
	xmpscq hMPSC = NULL;
	xmpscq_struct tEmbeddedMPSC;
	xmpscq_struct tInvalidMPSC;
	__test_queue_core_mpsc_ctx arrMpscCtx[__TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT];
	xmpmcq hMPMC = NULL;
	xmpmcq_struct tEmbeddedMPMC;
	xmpmcq_struct tInvalidMPMC;
	__test_queue_core_mpmc_prod_ctx arrMpmcProdCtx[__TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT];
	__test_queue_core_mpmc_cons_ctx arrMpmcConsCtx[__TEST_QUEUE_CORE_MPMC_CONSUMER_COUNT];
	xthread hProducer = NULL;
	xthread arrMpscProducer[__TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT];
	xthread arrMpmcProducer[__TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT];
	xthread arrMpmcConsumer[__TEST_QUEUE_CORE_MPMC_CONSUMER_COUNT];
	uint8 arrMpscSeen[__TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS];
	volatile long arrMpmcSeen[__TEST_QUEUE_CORE_MPMC_TOTAL_ITEMS];
	ptr arrBatchIn[5];
	ptr arrBatchOut[5];
	ptr pItem = NULL;
	uint32 iExpected = 0;
	uint32 iBatchCount = 0;
	uint32 i;
	uint32 iReceived = 0;
	int bMpscThreadedOk = TRUE;
	volatile long iMpmcConsumed = 0;
	volatile long iMpmcFailure = 0;
	int bMpmcThreadedOk = TRUE;
	xqueuebase tUnknownBase;
	ptr* pSpscItems = NULL;
	__test_queue_core_u32_atomic_probe tAtomicProbe;
	#ifndef XRT_NO_QUEUE_WAIT
		xmpscqwait hMPSCWait = NULL;
		xthread hMPSCWaitThread = NULL;
		xthread arrMPSCWaitRaceThread[2] = { 0 };
		xsem hMPSCWaitReady = NULL;
		__test_queue_core_mpscwait_cons_ctx tMPSCWaitCtx;
		__test_queue_core_mpscwait_cons_ctx arrMPSCWaitRaceCtx[2];
	#endif

	printf("\n\n------------------------------------\n\n Queue Core Test:\n\n");

	memset(&tBadCfg, 0, sizeof(tBadCfg));
	memset(&tInvalidSPSC, 0xaa, sizeof(tInvalidSPSC));
	memset(&tInvalidMPSC, 0xaa, sizeof(tInvalidMPSC));
	memset(&tInvalidMPMC, 0xaa, sizeof(tInvalidMPMC));
	memset(&tUnknownBase, 0, sizeof(tUnknownBase));
	tBadCfg.iCapacity = 0;
	iFail += __Test_QueueCore_Check("queue helper null closed", xrtQueueIsClosed(NULL) == FALSE);
	iFail += __Test_QueueCore_Check("queue helper null drained", xrtQueueIsDrained(NULL) == FALSE);
	iFail += __Test_QueueCore_Check("spsc create zero capacity fails", xrtSPSCQCreate(&tBadCfg) == NULL);
	iFail += __Test_QueueCore_Check("spsc init zero capacity fails", xrtSPSCQInit(&tInvalidSPSC, &tBadCfg) == FALSE && tInvalidSPSC.arrItems == NULL);
	iFail += __Test_QueueCore_Check("mpsc create zero capacity fails", xrtMPSCQCreate(&tBadCfg) == NULL);
	iFail += __Test_QueueCore_Check("mpsc init zero capacity fails", xrtMPSCQInit(&tInvalidMPSC, &tBadCfg) == FALSE && tInvalidMPSC.arrSlots == NULL);
	iFail += __Test_QueueCore_Check("mpmc create zero capacity fails", xrtMPMCQCreate(&tBadCfg) == NULL);
	iFail += __Test_QueueCore_Check("mpmc init zero capacity fails", xrtMPMCQInit(&tInvalidMPMC, &tBadCfg) == FALSE && tInvalidMPMC.arrSlots == NULL);
	tBadCfg.iCapacity = 0x40000001u;
	iFail += __Test_QueueCore_Check("spsc create oversized capacity fails", xrtSPSCQCreate(&tBadCfg) == NULL);
	iFail += __Test_QueueCore_Check("mpsc create oversized capacity fails", xrtMPSCQCreate(&tBadCfg) == NULL);
	iFail += __Test_QueueCore_Check("mpmc create oversized capacity fails", xrtMPMCQCreate(&tBadCfg) == NULL);
	tUnknownBase.iKind = 99u;
	tUnknownBase.bClosed = 1u;
	iFail += __Test_QueueCore_Check("queue helper unknown kind not drained", xrtQueueIsDrained(&tUnknownBase) == FALSE);
	iFail += __Test_QueueCore_Check("u32 atomic load/store guard", __Test_QueueCore_U32AtomicLoadStoreGuard());
	tAtomicProbe.iValue = 1u;
	tAtomicProbe.iGuard = 0x6b7c8d9eu;
	iFail += __Test_QueueCore_Check("xvo atomic width guard", __xvoAtomicCompareExchange32(&tAtomicProbe.iValue, 2u, 1u) == 1u && tAtomicProbe.iValue == 2u && tAtomicProbe.iGuard == 0x6b7c8d9eu);

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;
	iFail += __Test_QueueCore_Check("spsc create null cfg fails", xrtSPSCQCreate(NULL) == NULL);
	iFail += __Test_QueueCore_Check("spsc init null queue fails", xrtSPSCQInit(NULL, &tCfg) == FALSE);
	memset(&tInvalidSPSC, 0xaa, sizeof(tInvalidSPSC));
	iFail += __Test_QueueCore_Check("spsc init null cfg fails", xrtSPSCQInit(&tInvalidSPSC, NULL) == FALSE && tInvalidSPSC.arrItems == NULL);
	iFail += __Test_QueueCore_Check("spsc push null queue error", xrtSPSCQTryPush(NULL, (ptr)(uintptr_t)1u) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("spsc pop null queue error", xrtSPSCQTryPop(NULL, &pItem) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("spsc approx null zero", xrtSPSCQApproxCount(NULL) == 0u);
	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("spsc drain null queue zero", xrtSPSCQDrain(NULL, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u && tDrainCtx.iCount == 0u);
	iFail += __Test_QueueCore_Check("spsc reset null fails", xrtSPSCQReset(NULL) == FALSE);

	hQueue = xrtSPSCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("spsc create", hQueue != NULL);
	if ( hQueue == NULL ) {
		return 1;
	}
	pSpscItems = hQueue->arrItems;

	iFail += __Test_QueueCore_Check("spsc rounded capacity", hQueue->iCapacity == 4u);
	iFail += __Test_QueueCore_Check("spsc buffer init", pSpscItems != NULL);
	iFail += __Test_QueueCore_Check("queue initially open", xrtQueueIsClosed(&hQueue->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("queue initially not drained", xrtQueueIsDrained(&hQueue->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("spsc pop null out error", xrtSPSCQTryPop(hQueue, NULL) == XQUEUE_ERROR);
	xrtSPSCQClose(hQueue);
	iFail += __Test_QueueCore_Check("queue close empty", xrtQueueIsClosed(&hQueue->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("queue close empty drained", xrtQueueIsDrained(&hQueue->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("queue close empty pop", xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("queue close empty drain", xrtSPSCQDrain(hQueue, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u);
	iFail += __Test_QueueCore_Check("queue close empty reset", xrtSPSCQReset(hQueue) == TRUE);
	iFail += __Test_QueueCore_Check("pop empty", xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_EMPTY);

	iFail += __Test_QueueCore_Check("push #1", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)1u) == XQUEUE_OK && hQueue->arrItems == pSpscItems);
	iFail += __Test_QueueCore_Check("push #2", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)2u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("push #3", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)3u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("push #4", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)4u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("push full", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)5u) == XQUEUE_FULL);
	iFail += __Test_QueueCore_Check("spsc drain null proc zero", xrtSPSCQDrain(hQueue, NULL, &tDrainCtx) == 0u && xrtSPSCQApproxCount(hQueue) == 4u);
	iFail += __Test_QueueCore_Check("count full", xrtSPSCQApproxCount(hQueue) == 4u);

	iFail += __Test_QueueCore_Check("pop #1", xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 1u);
	iFail += __Test_QueueCore_Check("pop #2", xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 2u);
	iFail += __Test_QueueCore_Check("count after pops", xrtSPSCQApproxCount(hQueue) == 2u);

	xrtSPSCQClose(hQueue);
	iFail += __Test_QueueCore_Check("queue closed", xrtQueueIsClosed(&hQueue->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("push closed", xrtSPSCQTryPush(hQueue, (ptr)(uintptr_t)6u) == XQUEUE_CLOSED);

	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("drain count", xrtSPSCQDrain(hQueue, __Test_QueueCore_DrainProc, &tDrainCtx) == 2u);
	iFail += __Test_QueueCore_Check("drain order", tDrainCtx.iCount == 2u && (uintptr_t)tDrainCtx.arrItems[0] == 3u && (uintptr_t)tDrainCtx.arrItems[1] == 4u);
	iFail += __Test_QueueCore_Check("pop closed empty", xrtSPSCQTryPop(hQueue, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("queue drained", xrtQueueIsDrained(&hQueue->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("reset drained queue", xrtSPSCQReset(hQueue) == TRUE);
	iFail += __Test_QueueCore_Check("queue reopened after reset", xrtQueueIsClosed(&hQueue->tBase) == FALSE && xrtSPSCQApproxCount(hQueue) == 0u);
	xrtSPSCQDestroy(hQueue);

	memset(&tEmbedded, 0, sizeof(tEmbedded));
	iFail += __Test_QueueCore_Check("embedded init", xrtSPSCQInit(&tEmbedded, &tCfg) == TRUE);
	iFail += __Test_QueueCore_Check("embedded push", xrtSPSCQTryPush(&tEmbedded, (ptr)(uintptr_t)9u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("reset busy queue fails", xrtSPSCQReset(&tEmbedded) == FALSE);
	iFail += __Test_QueueCore_Check("embedded pop", xrtSPSCQTryPop(&tEmbedded, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 9u);
	iFail += __Test_QueueCore_Check("embedded reset drained", xrtSPSCQReset(&tEmbedded) == TRUE);
	xrtSPSCQUnit(&tEmbedded);

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64;
	hQueue = xrtSPSCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("spsc create for threaded run", hQueue != NULL);
	if ( hQueue == NULL ) {
		return iFail ? 1 : 0;
	}

	memset(&tSpscCtx, 0, sizeof(tSpscCtx));
	tSpscCtx.hQueue = hQueue;
	tSpscCtx.iCount = 2048;
	hProducer = xrtThreadCreate(__Test_QueueCore_SPSCProducer, &tSpscCtx, 0);
	iFail += __Test_QueueCore_Check("producer thread create", hProducer != NULL);
	if ( hProducer != NULL ) {
		iExpected = 1;
		for ( ;; ) {
			xqueue_result iRet = xrtSPSCQTryPop(hQueue, &pItem);
			if ( iRet == XQUEUE_OK ) {
				if ( (uintptr_t)pItem != (uintptr_t)iExpected ) {
					iFail += __Test_QueueCore_Check("spsc threaded order", FALSE);
					break;
				}
				iExpected++;
				continue;
			}
			if ( iRet == XQUEUE_EMPTY ) {
				xrtThreadYield();
				continue;
			}
			iFail += __Test_QueueCore_Check("spsc threaded closed", iRet == XQUEUE_CLOSED);
			break;
		}

		iFail += __Test_QueueCore_Check("producer thread join", xrtThreadWaitTimeout(hProducer, 5000) == XRT_WAIT_OK);
		iFail += __Test_QueueCore_Check("producer thread exit", xrtThreadGetExitCode(hProducer) == 0u);
		iFail += __Test_QueueCore_Check("producer thread no failure", tSpscCtx.iFailure == 0);
		iFail += __Test_QueueCore_Check("spsc threaded count", iExpected == (tSpscCtx.iCount + 1u));
		xrtThreadDestroy(hProducer);
	}
	xrtSPSCQDestroy(hQueue);

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;
	iFail += __Test_QueueCore_Check("mpsc create null cfg fails", xrtMPSCQCreate(NULL) == NULL);
	iFail += __Test_QueueCore_Check("mpsc init null queue fails", xrtMPSCQInit(NULL, &tCfg) == FALSE);
	memset(&tInvalidMPSC, 0xaa, sizeof(tInvalidMPSC));
	iFail += __Test_QueueCore_Check("mpsc init null cfg fails", xrtMPSCQInit(&tInvalidMPSC, NULL) == FALSE && tInvalidMPSC.arrSlots == NULL);
	iFail += __Test_QueueCore_Check("mpsc push null queue error", xrtMPSCQTryPush(NULL, (ptr)(uintptr_t)1u) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("mpsc pop null queue error", xrtMPSCQTryPop(NULL, &pItem) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("mpsc push batch null queue zero", xrtMPSCQPushBatch(NULL, arrBatchIn, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpsc push batch null items zero", xrtMPSCQPushBatch((xmpscq)(uintptr_t)1u, NULL, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpsc pop batch null queue zero", xrtMPSCQPopBatch(NULL, arrBatchOut, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpsc pop batch null items zero", xrtMPSCQPopBatch((xmpscq)(uintptr_t)1u, NULL, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpsc approx null zero", xrtMPSCQApproxCount(NULL) == 0u);
	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("mpsc drain null queue zero", xrtMPSCQDrain(NULL, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u && tDrainCtx.iCount == 0u);
	iFail += __Test_QueueCore_Check("mpsc reset null fails", xrtMPSCQReset(NULL) == FALSE);

	hMPSC = xrtMPSCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("mpsc create", hMPSC != NULL);
	if ( hMPSC == NULL ) {
		return iFail ? 1 : 0;
	}

	iFail += __Test_QueueCore_Check("mpsc rounded capacity", hMPSC->iCapacity == 4u);
	iFail += __Test_QueueCore_Check("mpsc initially open", xrtQueueIsClosed(&hMPSC->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("mpsc initially not drained", xrtQueueIsDrained(&hMPSC->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("mpsc pop null out error", xrtMPSCQTryPop(hMPSC, NULL) == XQUEUE_ERROR);
	xrtMPSCQClose(hMPSC);
	iFail += __Test_QueueCore_Check("mpsc close empty", xrtQueueIsClosed(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc close empty drained", xrtQueueIsDrained(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc close empty pop", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpsc close empty drain", xrtMPSCQDrain(hMPSC, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u);
	iFail += __Test_QueueCore_Check("mpsc close empty reset", xrtMPSCQReset(hMPSC) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc pop empty", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_EMPTY);

	iFail += __Test_QueueCore_Check("mpsc push #1", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)1u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpsc push #2", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)2u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpsc push #3", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)3u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpsc push #4", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)4u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpsc push full", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)5u) == XQUEUE_FULL);
	iFail += __Test_QueueCore_Check("mpsc drain null proc zero", xrtMPSCQDrain(hMPSC, NULL, &tDrainCtx) == 0u && xrtMPSCQApproxCount(hMPSC) == 4u);
	iFail += __Test_QueueCore_Check("mpsc count full", xrtMPSCQApproxCount(hMPSC) == 4u);

	iFail += __Test_QueueCore_Check("mpsc pop #1", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 1u);
	iFail += __Test_QueueCore_Check("mpsc pop #2", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 2u);
	iFail += __Test_QueueCore_Check("mpsc count after pops", xrtMPSCQApproxCount(hMPSC) == 2u);
	arrBatchIn[0] = (ptr)(uintptr_t)5u;
	arrBatchIn[1] = (ptr)(uintptr_t)6u;
	arrBatchIn[2] = (ptr)(uintptr_t)7u;
	iBatchCount = xrtMPSCQPushBatch(hMPSC, arrBatchIn, 3u);
	iFail += __Test_QueueCore_Check("mpsc push batch partial", iBatchCount == 2u && xrtMPSCQApproxCount(hMPSC) == 4u);
	arrBatchOut[0] = (ptr)(uintptr_t)90u;
	arrBatchOut[1] = (ptr)(uintptr_t)91u;
	arrBatchOut[2] = (ptr)(uintptr_t)92u;
	arrBatchOut[3] = (ptr)(uintptr_t)93u;
	iBatchCount = xrtMPSCQPopBatch(hMPSC, arrBatchOut, 3u);
	iFail += __Test_QueueCore_Check("mpsc pop batch fifo", iBatchCount == 3u &&
		(uintptr_t)arrBatchOut[0] == 3u &&
		(uintptr_t)arrBatchOut[1] == 4u &&
		(uintptr_t)arrBatchOut[2] == 5u &&
		(uintptr_t)arrBatchOut[3] == 93u);
	iFail += __Test_QueueCore_Check("mpsc count after batch pop", xrtMPSCQApproxCount(hMPSC) == 1u);

	xrtMPSCQClose(hMPSC);
	iFail += __Test_QueueCore_Check("mpsc queue closed", xrtQueueIsClosed(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc push closed", xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)6u) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpsc push batch closed zero", xrtMPSCQPushBatch(hMPSC, arrBatchIn, 2u) == 0u);

	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("mpsc drain count", xrtMPSCQDrain(hMPSC, __Test_QueueCore_DrainProc, &tDrainCtx) == 1u);
	iFail += __Test_QueueCore_Check("mpsc drain order", tDrainCtx.iCount == 1u && (uintptr_t)tDrainCtx.arrItems[0] == 6u);
	iFail += __Test_QueueCore_Check("mpsc pop closed empty", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpsc pop batch closed empty zero", xrtMPSCQPopBatch(hMPSC, arrBatchOut, 2u) == 0u);
	iFail += __Test_QueueCore_Check("mpsc queue drained", xrtQueueIsDrained(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc reset drained queue", xrtMPSCQReset(hMPSC) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc reopened after reset", xrtQueueIsClosed(&hMPSC->tBase) == FALSE && xrtMPSCQApproxCount(hMPSC) == 0u);
	xrtMPSCQDestroy(hMPSC);

	memset(&tEmbeddedMPSC, 0, sizeof(tEmbeddedMPSC));
	iFail += __Test_QueueCore_Check("mpsc embedded init", xrtMPSCQInit(&tEmbeddedMPSC, &tCfg) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc embedded push", xrtMPSCQTryPush(&tEmbeddedMPSC, (ptr)(uintptr_t)9u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpsc reset busy queue fails", xrtMPSCQReset(&tEmbeddedMPSC) == FALSE);
	iFail += __Test_QueueCore_Check("mpsc embedded pop", xrtMPSCQTryPop(&tEmbeddedMPSC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 9u);
	iFail += __Test_QueueCore_Check("mpsc embedded reset drained", xrtMPSCQReset(&tEmbeddedMPSC) == TRUE);
	xrtMPSCQUnit(&tEmbeddedMPSC);

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64;
	hMPSC = xrtMPSCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("mpsc create for threaded run", hMPSC != NULL);
	if ( hMPSC == NULL ) {
		return iFail ? 1 : 0;
	}

	memset(arrMpscProducer, 0, sizeof(arrMpscProducer));
	memset(arrMpscCtx, 0, sizeof(arrMpscCtx));
	memset(arrMpscSeen, 0, sizeof(arrMpscSeen));
	for ( i = 0; i < __TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT; ++i ) {
		arrMpscCtx[i].hQueue = hMPSC;
		arrMpscCtx[i].iBaseValue = i * __TEST_QUEUE_CORE_MPSC_ITEMS_PER_PRODUCER;
		arrMpscCtx[i].iCount = __TEST_QUEUE_CORE_MPSC_ITEMS_PER_PRODUCER;
		arrMpscProducer[i] = xrtThreadCreate(__Test_QueueCore_MPSCProducer, &arrMpscCtx[i], 0);
		iFail += __Test_QueueCore_Check("mpsc producer thread create", arrMpscProducer[i] != NULL);
		if ( arrMpscProducer[i] == NULL ) {
			bMpscThreadedOk = FALSE;
			break;
		}
	}

	while ( bMpscThreadedOk && (iReceived < __TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS) ) {
		xqueue_result iRet = xrtMPSCQTryPop(hMPSC, &pItem);
		if ( iRet == XQUEUE_OK ) {
			uint32 iValue = (uint32)(uintptr_t)pItem;
			if ( iValue == 0u || iValue > __TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS ) {
				iFail += __Test_QueueCore_Check("mpsc threaded range", FALSE);
				bMpscThreadedOk = FALSE;
				break;
			}
			if ( arrMpscSeen[iValue - 1u] != 0u ) {
				iFail += __Test_QueueCore_Check("mpsc threaded duplicate", FALSE);
				bMpscThreadedOk = FALSE;
				break;
			}
			arrMpscSeen[iValue - 1u] = 1u;
			iReceived++;
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		iFail += __Test_QueueCore_Check("mpsc threaded pop state", FALSE);
		bMpscThreadedOk = FALSE;
		break;
	}

	for ( i = 0; i < __TEST_QUEUE_CORE_MPSC_PRODUCER_COUNT; ++i ) {
		if ( arrMpscProducer[i] == NULL ) {
			continue;
		}
		iFail += __Test_QueueCore_Check("mpsc producer thread join", xrtThreadWaitTimeout(arrMpscProducer[i], 5000) == XRT_WAIT_OK);
		iFail += __Test_QueueCore_Check("mpsc producer thread exit", xrtThreadGetExitCode(arrMpscProducer[i]) == 0u);
		iFail += __Test_QueueCore_Check("mpsc producer thread no failure", arrMpscCtx[i].iFailure == 0);
		xrtThreadDestroy(arrMpscProducer[i]);
	}

	if ( bMpscThreadedOk ) {
		for ( i = 0; i < __TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS; ++i ) {
			if ( arrMpscSeen[i] == 0u ) {
				bMpscThreadedOk = FALSE;
				break;
			}
		}
	}

	iFail += __Test_QueueCore_Check("mpsc threaded received all", bMpscThreadedOk && iReceived == __TEST_QUEUE_CORE_MPSC_TOTAL_ITEMS);
	iFail += __Test_QueueCore_Check("mpsc threaded still open", xrtQueueIsClosed(&hMPSC->tBase) == FALSE);
	xrtMPSCQClose(hMPSC);
	iFail += __Test_QueueCore_Check("mpsc threaded close", xrtQueueIsClosed(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc threaded drained", xrtQueueIsDrained(&hMPSC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpsc threaded pop closed", xrtMPSCQTryPop(hMPSC, &pItem) == XQUEUE_CLOSED);
	xrtMPSCQDestroy(hMPSC);

	#ifndef XRT_NO_QUEUE_WAIT
		memset(&tCfg, 0, sizeof(tCfg));
		tCfg.iCapacity = 3;
		iFail += __Test_QueueCore_Check("mpsc wait create null cfg fails", xrtMPSCQWaitCreate(NULL) == NULL);
		iFail += __Test_QueueCore_Check("mpsc wait init null queue fails", xrtMPSCQWaitInit(NULL, &tCfg) == FALSE);
		iFail += __Test_QueueCore_Check("mpsc wait try push null queue error", xrtMPSCQWaitTryPush(NULL, (ptr)(uintptr_t)1u) == XQUEUE_ERROR);
		iFail += __Test_QueueCore_Check("mpsc wait try pop null queue error", xrtMPSCQWaitTryPop(NULL, &pItem) == XQUEUE_ERROR);
		iFail += __Test_QueueCore_Check("mpsc wait pop timeout null queue error", xrtMPSCQWaitPopTimeout(NULL, &pItem, 1u) == XQUEUE_ERROR);
		iFail += __Test_QueueCore_Check("mpsc wait approx null zero", xrtMPSCQWaitApproxCount(NULL) == 0u);

		hMPSCWait = xrtMPSCQWaitCreate(&tCfg);
		iFail += __Test_QueueCore_Check("mpsc wait create", hMPSCWait != NULL);
		if ( hMPSCWait == NULL ) {
			return iFail ? 1 : 0;
		}

		iFail += __Test_QueueCore_Check("mpsc wait initial empty", xrtMPSCQWaitTryPop(hMPSCWait, &pItem) == XQUEUE_EMPTY);
		iFail += __Test_QueueCore_Check("mpsc wait timeout empty", xrtMPSCQWaitPopTimeout(hMPSCWait, &pItem, 0u) == XQUEUE_TIMEOUT);
		iFail += __Test_QueueCore_Check("mpsc wait push #1", xrtMPSCQWaitTryPush(hMPSCWait, (ptr)(uintptr_t)101u) == XQUEUE_OK);
		iFail += __Test_QueueCore_Check("mpsc wait approx count #1", xrtMPSCQWaitApproxCount(hMPSCWait) == 1u);
		iFail += __Test_QueueCore_Check("mpsc wait try pop #1", xrtMPSCQWaitTryPop(hMPSCWait, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 101u);

		hMPSCWaitReady = xrtSemCreate(0u, 1u);
		iFail += __Test_QueueCore_Check("mpsc wait ready sem create", hMPSCWaitReady != NULL);
		memset(&tMPSCWaitCtx, 0, sizeof(tMPSCWaitCtx));
		tMPSCWaitCtx.hQueue = hMPSCWait;
		tMPSCWaitCtx.hReady = hMPSCWaitReady;
		tMPSCWaitCtx.iTimeoutMs = 1000u;
		hMPSCWaitThread = xrtThreadCreate(__Test_QueueCore_MPSCWaitConsumer, &tMPSCWaitCtx, 0);
		iFail += __Test_QueueCore_Check("mpsc wait consumer create", hMPSCWaitThread != NULL);
		if ( hMPSCWaitReady != NULL ) {
			iFail += __Test_QueueCore_Check("mpsc wait consumer ready", xrtSemWaitTimeout(hMPSCWaitReady, 1000u) == XRT_WAIT_OK);
		}
		iFail += __Test_QueueCore_Check("mpsc wait push wakes consumer", xrtMPSCQWaitTryPush(hMPSCWait, (ptr)(uintptr_t)202u) == XQUEUE_OK);
		if ( hMPSCWaitThread != NULL ) {
			iFail += __Test_QueueCore_Check("mpsc wait consumer join", xrtThreadWaitTimeout(hMPSCWaitThread, 5000u) == XRT_WAIT_OK);
			iFail += __Test_QueueCore_Check("mpsc wait consumer exit", xrtThreadGetExitCode(hMPSCWaitThread) == 0u);
			iFail += __Test_QueueCore_Check("mpsc wait consumer result", tMPSCWaitCtx.iFailure == 0 && tMPSCWaitCtx.iResult == XQUEUE_OK && tMPSCWaitCtx.iValue == 202u);
			xrtThreadDestroy(hMPSCWaitThread);
			hMPSCWaitThread = NULL;
		}
		if ( hMPSCWaitReady != NULL ) {
			xrtSemDestroy(hMPSCWaitReady);
			hMPSCWaitReady = NULL;
		}

		hMPSCWaitReady = xrtSemCreate(0u, 1u);
		iFail += __Test_QueueCore_Check("mpsc wait close sem create", hMPSCWaitReady != NULL);
		memset(&tMPSCWaitCtx, 0, sizeof(tMPSCWaitCtx));
		tMPSCWaitCtx.hQueue = hMPSCWait;
		tMPSCWaitCtx.hReady = hMPSCWaitReady;
		tMPSCWaitCtx.iTimeoutMs = 1000u;
		hMPSCWaitThread = xrtThreadCreate(__Test_QueueCore_MPSCWaitConsumer, &tMPSCWaitCtx, 0);
		iFail += __Test_QueueCore_Check("mpsc wait close consumer create", hMPSCWaitThread != NULL);
		if ( hMPSCWaitReady != NULL ) {
			iFail += __Test_QueueCore_Check("mpsc wait close consumer ready", xrtSemWaitTimeout(hMPSCWaitReady, 1000u) == XRT_WAIT_OK);
		}
		xrtMPSCQWaitClose(hMPSCWait);
		if ( hMPSCWaitThread != NULL ) {
			iFail += __Test_QueueCore_Check("mpsc wait close consumer join", xrtThreadWaitTimeout(hMPSCWaitThread, 5000u) == XRT_WAIT_OK);
			iFail += __Test_QueueCore_Check("mpsc wait close consumer exit", xrtThreadGetExitCode(hMPSCWaitThread) == 0u);
			iFail += __Test_QueueCore_Check("mpsc wait close wakes consumer", tMPSCWaitCtx.iFailure == 0 && tMPSCWaitCtx.iResult == XQUEUE_CLOSED);
			xrtThreadDestroy(hMPSCWaitThread);
			hMPSCWaitThread = NULL;
		}
		if ( hMPSCWaitReady != NULL ) {
			xrtSemDestroy(hMPSCWaitReady);
			hMPSCWaitReady = NULL;
		}

		iFail += __Test_QueueCore_Check("mpsc wait push closed", xrtMPSCQWaitTryPush(hMPSCWait, (ptr)(uintptr_t)303u) == XQUEUE_CLOSED);
		xrtMPSCQWaitDestroy(hMPSCWait);

		memset(&tCfg, 0, sizeof(tCfg));
		tCfg.iCapacity = 4u;
		hMPSCWait = xrtMPSCQWaitCreate(&tCfg);
		iFail += __Test_QueueCore_Check("mpsc wait race create", hMPSCWait != NULL);
		if ( hMPSCWait == NULL ) {
			return iFail ? 1 : 0;
		}

		hMPSCWaitReady = xrtSemCreate(0u, 2u);
		iFail += __Test_QueueCore_Check("mpsc wait race ready sem create", hMPSCWaitReady != NULL);
		if ( hMPSCWaitReady == NULL ) {
			xrtMPSCQWaitDestroy(hMPSCWait);
			return iFail ? 1 : 0;
		}

		xrtMutexLock(hMPSCWait->hPopLock);
		iFail += __Test_QueueCore_Check("mpsc wait race push item", xrtMPSCQWaitTryPush(hMPSCWait, (ptr)(uintptr_t)404u) == XQUEUE_OK);
		xrtMPSCQWaitClose(hMPSCWait);
		memset(arrMPSCWaitRaceCtx, 0, sizeof(arrMPSCWaitRaceCtx));
		for ( i = 0; i < 2u; ++i ) {
			arrMPSCWaitRaceCtx[i].hQueue = hMPSCWait;
			arrMPSCWaitRaceCtx[i].hReady = hMPSCWaitReady;
			arrMPSCWaitRaceCtx[i].iTimeoutMs = 1000u;
			arrMPSCWaitRaceThread[i] = xrtThreadCreate(__Test_QueueCore_MPSCWaitConsumer, &arrMPSCWaitRaceCtx[i], 0);
			iFail += __Test_QueueCore_Check("mpsc wait race consumer create", arrMPSCWaitRaceThread[i] != NULL);
		}
		for ( i = 0; i < 2u; ++i ) {
			iFail += __Test_QueueCore_Check("mpsc wait race consumer ready", xrtSemWaitTimeout(hMPSCWaitReady, 1000u) == XRT_WAIT_OK);
		}
		{
			uint64 iDeadline = (uint64)(xrtTimer() * 1000.0) + 1000u;
			while ( __Test_QueueCore_AtomicLoad(&hMPSCWait->iWaiters) == 0 ) {
				if ( (uint64)(xrtTimer() * 1000.0) >= iDeadline ) {
					break;
				}
				xrtThreadYield();
			}
		}
		iFail += __Test_QueueCore_Check("mpsc wait race waiter queued", __Test_QueueCore_AtomicLoad(&hMPSCWait->iWaiters) > 0);
		xrtMutexUnlock(hMPSCWait->hPopLock);
		for ( i = 0; i < 2u; ++i ) {
			if ( arrMPSCWaitRaceThread[i] != NULL ) {
				iFail += __Test_QueueCore_Check("mpsc wait race consumer join", xrtThreadWaitTimeout(arrMPSCWaitRaceThread[i], 5000u) == XRT_WAIT_OK);
				iFail += __Test_QueueCore_Check("mpsc wait race consumer exit", xrtThreadGetExitCode(arrMPSCWaitRaceThread[i]) == 0u);
				xrtThreadDestroy(arrMPSCWaitRaceThread[i]);
				arrMPSCWaitRaceThread[i] = NULL;
			}
		}
		{
			uint32 iOkCount = 0u;
			uint32 iClosedCount = 0u;
			for ( i = 0; i < 2u; ++i ) {
				if ( arrMPSCWaitRaceCtx[i].iFailure != 0 ) {
					continue;
				}
				if ( arrMPSCWaitRaceCtx[i].iResult == XQUEUE_OK && arrMPSCWaitRaceCtx[i].iValue == 404u ) {
					iOkCount++;
				}
				else if ( arrMPSCWaitRaceCtx[i].iResult == XQUEUE_CLOSED ) {
					iClosedCount++;
				}
			}
			iFail += __Test_QueueCore_Check("mpsc wait race results", iOkCount == 1u && iClosedCount == 1u);
		}
		iFail += __Test_QueueCore_Check("mpsc wait race drained", xrtQueueIsDrained(&hMPSCWait->tQueue.tBase) == TRUE);
		if ( hMPSCWaitReady != NULL ) {
			xrtSemDestroy(hMPSCWaitReady);
			hMPSCWaitReady = NULL;
		}
		xrtMPSCQWaitDestroy(hMPSCWait);
	#endif

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;
	iFail += __Test_QueueCore_Check("mpmc create null cfg fails", xrtMPMCQCreate(NULL) == NULL);
	iFail += __Test_QueueCore_Check("mpmc init null queue fails", xrtMPMCQInit(NULL, &tCfg) == FALSE);
	memset(&tInvalidMPMC, 0xaa, sizeof(tInvalidMPMC));
	iFail += __Test_QueueCore_Check("mpmc init null cfg fails", xrtMPMCQInit(&tInvalidMPMC, NULL) == FALSE && tInvalidMPMC.arrSlots == NULL);
	iFail += __Test_QueueCore_Check("mpmc push null queue error", xrtMPMCQTryPush(NULL, (ptr)(uintptr_t)1u) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("mpmc pop null queue error", xrtMPMCQTryPop(NULL, &pItem) == XQUEUE_ERROR);
	iFail += __Test_QueueCore_Check("mpmc push batch null queue zero", xrtMPMCQPushBatch(NULL, arrBatchIn, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpmc push batch null items zero", xrtMPMCQPushBatch((xmpmcq)(uintptr_t)1u, NULL, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpmc pop batch null queue zero", xrtMPMCQPopBatch(NULL, arrBatchOut, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpmc pop batch null items zero", xrtMPMCQPopBatch((xmpmcq)(uintptr_t)1u, NULL, 1u) == 0u);
	iFail += __Test_QueueCore_Check("mpmc approx null zero", xrtMPMCQApproxCount(NULL) == 0u);
	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("mpmc drain null queue zero", xrtMPMCQDrain(NULL, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u && tDrainCtx.iCount == 0u);
	iFail += __Test_QueueCore_Check("mpmc reset null fails", xrtMPMCQReset(NULL) == FALSE);

	hMPMC = xrtMPMCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("mpmc create", hMPMC != NULL);
	if ( hMPMC == NULL ) {
		return iFail ? 1 : 0;
	}

	iFail += __Test_QueueCore_Check("mpmc rounded capacity", hMPMC->iCapacity == 4u);
	iFail += __Test_QueueCore_Check("mpmc initially open", xrtQueueIsClosed(&hMPMC->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("mpmc initially not drained", xrtQueueIsDrained(&hMPMC->tBase) == FALSE);
	iFail += __Test_QueueCore_Check("mpmc pop null out error", xrtMPMCQTryPop(hMPMC, NULL) == XQUEUE_ERROR);
	xrtMPMCQClose(hMPMC);
	iFail += __Test_QueueCore_Check("mpmc close empty", xrtQueueIsClosed(&hMPMC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc close empty drained", xrtQueueIsDrained(&hMPMC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc close empty pop", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpmc close empty drain", xrtMPMCQDrain(hMPMC, __Test_QueueCore_DrainProc, &tDrainCtx) == 0u);
	iFail += __Test_QueueCore_Check("mpmc close empty reset", xrtMPMCQReset(hMPMC) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc pop empty", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_EMPTY);

	iFail += __Test_QueueCore_Check("mpmc push #1", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)1u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpmc push #2", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)2u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpmc push #3", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)3u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpmc push #4", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)4u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpmc push full", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)5u) == XQUEUE_FULL);
	iFail += __Test_QueueCore_Check("mpmc drain null proc zero", xrtMPMCQDrain(hMPMC, NULL, &tDrainCtx) == 0u && xrtMPMCQApproxCount(hMPMC) == 4u);
	iFail += __Test_QueueCore_Check("mpmc count full", xrtMPMCQApproxCount(hMPMC) == 4u);

	iFail += __Test_QueueCore_Check("mpmc pop #1", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 1u);
	iFail += __Test_QueueCore_Check("mpmc pop #2", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 2u);
	iFail += __Test_QueueCore_Check("mpmc count after pops", xrtMPMCQApproxCount(hMPMC) == 2u);
	arrBatchIn[0] = (ptr)(uintptr_t)5u;
	arrBatchIn[1] = (ptr)(uintptr_t)6u;
	arrBatchIn[2] = (ptr)(uintptr_t)7u;
	iBatchCount = xrtMPMCQPushBatch(hMPMC, arrBatchIn, 3u);
	iFail += __Test_QueueCore_Check("mpmc push batch partial", iBatchCount == 2u && xrtMPMCQApproxCount(hMPMC) == 4u);
	arrBatchOut[0] = (ptr)(uintptr_t)190u;
	arrBatchOut[1] = (ptr)(uintptr_t)191u;
	arrBatchOut[2] = (ptr)(uintptr_t)192u;
	arrBatchOut[3] = (ptr)(uintptr_t)193u;
	iBatchCount = xrtMPMCQPopBatch(hMPMC, arrBatchOut, 3u);
	iFail += __Test_QueueCore_Check("mpmc pop batch fifo", iBatchCount == 3u &&
		(uintptr_t)arrBatchOut[0] == 3u &&
		(uintptr_t)arrBatchOut[1] == 4u &&
		(uintptr_t)arrBatchOut[2] == 5u &&
		(uintptr_t)arrBatchOut[3] == 193u);
	iFail += __Test_QueueCore_Check("mpmc count after batch pop", xrtMPMCQApproxCount(hMPMC) == 1u);

	xrtMPMCQClose(hMPMC);
	iFail += __Test_QueueCore_Check("mpmc queue closed", xrtQueueIsClosed(&hMPMC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc push closed", xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)6u) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpmc push batch closed zero", xrtMPMCQPushBatch(hMPMC, arrBatchIn, 2u) == 0u);

	memset(&tDrainCtx, 0, sizeof(tDrainCtx));
	iFail += __Test_QueueCore_Check("mpmc drain count", xrtMPMCQDrain(hMPMC, __Test_QueueCore_DrainProc, &tDrainCtx) == 1u);
	iFail += __Test_QueueCore_Check("mpmc drain order", tDrainCtx.iCount == 1u && (uintptr_t)tDrainCtx.arrItems[0] == 6u);
	iFail += __Test_QueueCore_Check("mpmc pop closed empty", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_CLOSED);
	iFail += __Test_QueueCore_Check("mpmc pop batch closed empty zero", xrtMPMCQPopBatch(hMPMC, arrBatchOut, 2u) == 0u);
	iFail += __Test_QueueCore_Check("mpmc queue drained", xrtQueueIsDrained(&hMPMC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc reset drained queue", xrtMPMCQReset(hMPMC) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc reopened after reset", xrtQueueIsClosed(&hMPMC->tBase) == FALSE && xrtMPMCQApproxCount(hMPMC) == 0u);
	xrtMPMCQDestroy(hMPMC);

	memset(&tEmbeddedMPMC, 0, sizeof(tEmbeddedMPMC));
	iFail += __Test_QueueCore_Check("mpmc embedded init", xrtMPMCQInit(&tEmbeddedMPMC, &tCfg) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc embedded push", xrtMPMCQTryPush(&tEmbeddedMPMC, (ptr)(uintptr_t)9u) == XQUEUE_OK);
	iFail += __Test_QueueCore_Check("mpmc reset busy queue fails", xrtMPMCQReset(&tEmbeddedMPMC) == FALSE);
	iFail += __Test_QueueCore_Check("mpmc embedded pop", xrtMPMCQTryPop(&tEmbeddedMPMC, &pItem) == XQUEUE_OK && (uintptr_t)pItem == 9u);
	iFail += __Test_QueueCore_Check("mpmc embedded reset drained", xrtMPMCQReset(&tEmbeddedMPMC) == TRUE);
	xrtMPMCQUnit(&tEmbeddedMPMC);

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 64;
	hMPMC = xrtMPMCQCreate(&tCfg);
	iFail += __Test_QueueCore_Check("mpmc create for threaded run", hMPMC != NULL);
	if ( hMPMC == NULL ) {
		return iFail ? 1 : 0;
	}

	memset(arrMpmcProducer, 0, sizeof(arrMpmcProducer));
	memset(arrMpmcConsumer, 0, sizeof(arrMpmcConsumer));
	memset(arrMpmcProdCtx, 0, sizeof(arrMpmcProdCtx));
	memset(arrMpmcConsCtx, 0, sizeof(arrMpmcConsCtx));
	memset((void*)arrMpmcSeen, 0, sizeof(arrMpmcSeen));
	iMpmcConsumed = 0;
	iMpmcFailure = 0;

	for ( i = 0; i < __TEST_QUEUE_CORE_MPMC_CONSUMER_COUNT; ++i ) {
		arrMpmcConsCtx[i].hQueue = hMPMC;
		arrMpmcConsCtx[i].arrSeen = arrMpmcSeen;
		arrMpmcConsCtx[i].pConsumed = &iMpmcConsumed;
		arrMpmcConsCtx[i].pFailure = &iMpmcFailure;
		arrMpmcConsCtx[i].iTotalCount = __TEST_QUEUE_CORE_MPMC_TOTAL_ITEMS;
		arrMpmcConsumer[i] = xrtThreadCreate(__Test_QueueCore_MPMCConsumer, &arrMpmcConsCtx[i], 0);
		iFail += __Test_QueueCore_Check("mpmc consumer thread create", arrMpmcConsumer[i] != NULL);
		if ( arrMpmcConsumer[i] == NULL ) {
			bMpmcThreadedOk = FALSE;
			break;
		}
	}

	for ( i = 0; bMpmcThreadedOk && i < __TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT; ++i ) {
		arrMpmcProdCtx[i].hQueue = hMPMC;
		arrMpmcProdCtx[i].iBaseValue = i * __TEST_QUEUE_CORE_MPMC_ITEMS_PER_PRODUCER;
		arrMpmcProdCtx[i].iCount = __TEST_QUEUE_CORE_MPMC_ITEMS_PER_PRODUCER;
		arrMpmcProducer[i] = xrtThreadCreate(__Test_QueueCore_MPMCProducer, &arrMpmcProdCtx[i], 0);
		iFail += __Test_QueueCore_Check("mpmc producer thread create", arrMpmcProducer[i] != NULL);
		if ( arrMpmcProducer[i] == NULL ) {
			bMpmcThreadedOk = FALSE;
			break;
		}
	}

	for ( i = 0; i < __TEST_QUEUE_CORE_MPMC_PRODUCER_COUNT; ++i ) {
		if ( arrMpmcProducer[i] == NULL ) {
			continue;
		}
		iFail += __Test_QueueCore_Check("mpmc producer thread join", xrtThreadWaitTimeout(arrMpmcProducer[i], 5000) == XRT_WAIT_OK);
		iFail += __Test_QueueCore_Check("mpmc producer thread exit", xrtThreadGetExitCode(arrMpmcProducer[i]) == 0u);
		iFail += __Test_QueueCore_Check("mpmc producer thread no failure", arrMpmcProdCtx[i].iFailure == 0);
		xrtThreadDestroy(arrMpmcProducer[i]);
	}

	xrtMPMCQClose(hMPMC);
	iFail += __Test_QueueCore_Check("mpmc threaded close", xrtQueueIsClosed(&hMPMC->tBase) == TRUE);

	for ( i = 0; i < __TEST_QUEUE_CORE_MPMC_CONSUMER_COUNT; ++i ) {
		if ( arrMpmcConsumer[i] == NULL ) {
			continue;
		}
		iFail += __Test_QueueCore_Check("mpmc consumer thread join", xrtThreadWaitTimeout(arrMpmcConsumer[i], 5000) == XRT_WAIT_OK);
		iFail += __Test_QueueCore_Check("mpmc consumer thread exit", xrtThreadGetExitCode(arrMpmcConsumer[i]) == 0u);
		xrtThreadDestroy(arrMpmcConsumer[i]);
	}

	if ( __Test_QueueCore_AtomicLoad(&iMpmcFailure) != 0 ) {
		bMpmcThreadedOk = FALSE;
	}
	if ( __Test_QueueCore_AtomicLoad(&iMpmcConsumed) != __TEST_QUEUE_CORE_MPMC_TOTAL_ITEMS ) {
		bMpmcThreadedOk = FALSE;
	}
	for ( i = 0; i < __TEST_QUEUE_CORE_MPMC_TOTAL_ITEMS; ++i ) {
		if ( __Test_QueueCore_AtomicLoad(&arrMpmcSeen[i]) != 1 ) {
			bMpmcThreadedOk = FALSE;
			break;
		}
	}

	iFail += __Test_QueueCore_Check("mpmc threaded received all", bMpmcThreadedOk);
	iFail += __Test_QueueCore_Check("mpmc threaded drained", xrtQueueIsDrained(&hMPMC->tBase) == TRUE);
	iFail += __Test_QueueCore_Check("mpmc threaded pop closed", xrtMPMCQTryPop(hMPMC, &pItem) == XQUEUE_CLOSED);
	xrtMPMCQDestroy(hMPMC);

	return iFail ? 1 : 0;
}

#endif
