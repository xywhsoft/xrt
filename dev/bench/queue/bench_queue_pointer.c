#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

typedef struct {
	xspscq hQueue;
	uint32 iCount;
	volatile long iStart;
	volatile long iConsumed;
	volatile long iFailure;
} __bench_spsc_ctx;

typedef struct {
	xmpscq hQueue;
	uint32 iCount;
	uint32 iBaseValue;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpsc_prod_ctx;

typedef struct {
	xmpscq hQueue;
	volatile long* pStart;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpsc_cons_ctx;

typedef struct {
	xmpmcq hQueue;
	uint32 iCount;
	uint32 iBaseValue;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpmc_prod_ctx;

typedef struct {
	xmpmcq hQueue;
	volatile long* pStart;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpmc_cons_ctx;

static uint32 __benchSPSCProducer(ptr pArg)
{
	__bench_spsc_ctx* pCtx = (__bench_spsc_ctx*)pArg;
	uint32 i;

	if ( !pCtx || !pCtx->hQueue ) {
		return 11u;
	}

	while ( xbenchAtomicLoad(&pCtx->iStart) == 0 ) {
		xrtThreadYield();
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueue_result iRet = xrtSPSCQTryPush(pCtx->hQueue, (ptr)(uintptr_t)(i + 1u));
			if ( iRet == XQUEUE_OK ) {
				break;
			}
			if ( iRet == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			xbenchAtomicMax(&pCtx->iFailure, (long)(100 + iRet));
			return 12u;
		}
	}

	xrtSPSCQClose(pCtx->hQueue);
	return 0u;
}

static uint32 __benchSPSCConsumer(ptr pArg)
{
	__bench_spsc_ctx* pCtx = (__bench_spsc_ctx*)pArg;
	ptr pItem = NULL;

	if ( !pCtx || !pCtx->hQueue ) {
		return 21u;
	}

	while ( xbenchAtomicLoad(&pCtx->iStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueue_result iRet = xrtSPSCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			xbenchAtomicInc(&pCtx->iConsumed);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0u;
		}
		xbenchAtomicMax(&pCtx->iFailure, (long)(200 + iRet));
		return 22u;
	}
}

static uint32 __benchMPSCProducer(ptr pArg)
{
	__bench_mpsc_prod_ctx* pCtx = (__bench_mpsc_prod_ctx*)pArg;
	uint32 i;

	if ( !pCtx || !pCtx->hQueue || !pCtx->pStart || !pCtx->pFailure ) {
		return 31u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
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
			xbenchAtomicMax(pCtx->pFailure, (long)(300 + iRet));
			return 32u;
		}
	}

	return 0u;
}

static uint32 __benchMPSCConsumer(ptr pArg)
{
	__bench_mpsc_cons_ctx* pCtx = (__bench_mpsc_cons_ctx*)pArg;
	ptr pItem = NULL;

	if ( !pCtx || !pCtx->hQueue || !pCtx->pStart || !pCtx->pConsumed || !pCtx->pFailure ) {
		return 41u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueue_result iRet = xrtMPSCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			xbenchAtomicInc(pCtx->pConsumed);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0u;
		}
		xbenchAtomicMax(pCtx->pFailure, (long)(400 + iRet));
		return 42u;
	}
}

static uint32 __benchMPMCProducer(ptr pArg)
{
	__bench_mpmc_prod_ctx* pCtx = (__bench_mpmc_prod_ctx*)pArg;
	uint32 i;

	if ( !pCtx || !pCtx->hQueue || !pCtx->pStart || !pCtx->pFailure ) {
		return 51u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
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
			xbenchAtomicMax(pCtx->pFailure, (long)(500 + iRet));
			return 52u;
		}
	}

	return 0u;
}

static uint32 __benchMPMCConsumer(ptr pArg)
{
	__bench_mpmc_cons_ctx* pCtx = (__bench_mpmc_cons_ctx*)pArg;
	ptr pItem = NULL;

	if ( !pCtx || !pCtx->hQueue || !pCtx->pStart || !pCtx->pConsumed || !pCtx->pFailure ) {
		return 61u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueue_result iRet = xrtMPMCQTryPop(pCtx->hQueue, &pItem);
		if ( iRet == XQUEUE_OK ) {
			xbenchAtomicInc(pCtx->pConsumed);
			continue;
		}
		if ( iRet == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		if ( iRet == XQUEUE_CLOSED ) {
			return 0u;
		}
		xbenchAtomicMax(pCtx->pFailure, (long)(600 + iRet));
		return 62u;
	}
}

static int __benchRunSPSC(uint32 iCapacity, uint32 iCount, uint64_t* pElapsedNs)
{
	xqueue_config tCfg;
	__bench_spsc_ctx tCtx;
	xthread hProducer = NULL;
	xthread hConsumer = NULL;
	xbenchtimer tTimer;
	int iResult = 1;

	if ( !pElapsedNs ) {
		return 1;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tCtx, 0, sizeof(tCtx));
	tCfg.iCapacity = iCapacity;
	tCtx.hQueue = xrtSPSCQCreate(&tCfg);
	tCtx.iCount = iCount;
	if ( !tCtx.hQueue ) {
		return 2;
	}

	hProducer = xrtThreadCreate(__benchSPSCProducer, &tCtx, 0);
	hConsumer = xrtThreadCreate(__benchSPSCConsumer, &tCtx, 0);
	if ( !hProducer || !hConsumer ) {
		goto cleanup;
	}

	xbenchTimerStart(&tTimer);
	tCtx.iStart = 1;
	xrtThreadWait(hProducer);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if ( xrtThreadGetExitCode(hProducer) != 0u ||
		xrtThreadGetExitCode(hConsumer) != 0u ||
		xbenchAtomicLoad(&tCtx.iFailure) != 0 ||
		xbenchAtomicLoad(&tCtx.iConsumed) != (long)iCount ) {
		goto cleanup;
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	if ( hProducer ) xrtThreadDestroy(hProducer);
	if ( hConsumer ) xrtThreadDestroy(hConsumer);
	if ( tCtx.hQueue ) xrtSPSCQDestroy(tCtx.hQueue);
	return iResult;
}

static int __benchRunMPSC(uint32 iCapacity, uint32 iProducerCount, uint32 iCountPerProducer, uint64_t* pElapsedNs)
{
	xqueue_config tCfg;
	xmpscq hQueue = NULL;
	xthread* arrProducer = NULL;
	__bench_mpsc_prod_ctx* arrCtx = NULL;
	__bench_mpsc_cons_ctx tConsCtx;
	xthread hConsumer = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount = iProducerCount * iCountPerProducer;
	int iResult = 1;

	if ( !pElapsedNs || iProducerCount == 0u ) {
		return 1;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tConsCtx, 0, sizeof(tConsCtx));
	tCfg.iCapacity = iCapacity;
	hQueue = xrtMPSCQCreate(&tCfg);
	if ( !hQueue ) {
		return 2;
	}

	arrProducer = (xthread*)calloc(iProducerCount, sizeof(xthread));
	arrCtx = (__bench_mpsc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpsc_prod_ctx));
	if ( !arrProducer || !arrCtx ) {
		goto cleanup;
	}

	tConsCtx.hQueue = hQueue;
	tConsCtx.pStart = &iStart;
	tConsCtx.pConsumed = &iConsumed;
	tConsCtx.pFailure = &iFailure;
	hConsumer = xrtThreadCreate(__benchMPSCConsumer, &tConsCtx, 0);
	if ( !hConsumer ) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		arrCtx[i].hQueue = hQueue;
		arrCtx[i].iCount = iCountPerProducer;
		arrCtx[i].iBaseValue = i * iCountPerProducer;
		arrCtx[i].pStart = &iStart;
		arrCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPSCProducer, &arrCtx[i], 0);
		if ( !arrProducer[i] ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	iStart = 1;
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPSCQClose(hQueue);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if ( xrtThreadGetExitCode(hConsumer) != 0u ||
		xbenchAtomicLoad(&iFailure) != 0 ||
		xbenchAtomicLoad(&iConsumed) != (long)iTotalCount ) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadGetExitCode(arrProducer[i]) != 0u ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	if ( arrProducer ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] ) xrtThreadDestroy(arrProducer[i]);
		}
		free(arrProducer);
	}
	if ( hConsumer ) xrtThreadDestroy(hConsumer);
	if ( arrCtx ) free(arrCtx);
	if ( hQueue ) xrtMPSCQDestroy(hQueue);
	return iResult;
}

static int __benchRunMPMC(uint32 iCapacity, uint32 iProducerCount, uint32 iConsumerCount, uint32 iCountPerProducer, uint64_t* pElapsedNs)
{
	xqueue_config tCfg;
	xmpmcq hQueue = NULL;
	xthread* arrProducer = NULL;
	xthread* arrConsumer = NULL;
	__bench_mpmc_prod_ctx* arrProdCtx = NULL;
	__bench_mpmc_cons_ctx* arrConsCtx = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount = iProducerCount * iCountPerProducer;
	int iResult = 1;

	if ( !pElapsedNs || iProducerCount == 0u || iConsumerCount == 0u ) {
		return 1;
	}

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = iCapacity;
	hQueue = xrtMPMCQCreate(&tCfg);
	if ( !hQueue ) {
		return 2;
	}

	arrProducer = (xthread*)calloc(iProducerCount, sizeof(xthread));
	arrConsumer = (xthread*)calloc(iConsumerCount, sizeof(xthread));
	arrProdCtx = (__bench_mpmc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpmc_prod_ctx));
	arrConsCtx = (__bench_mpmc_cons_ctx*)calloc(iConsumerCount, sizeof(__bench_mpmc_cons_ctx));
	if ( !arrProducer || !arrConsumer || !arrProdCtx || !arrConsCtx ) {
		goto cleanup;
	}

	for ( i = 0; i < iConsumerCount; ++i ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsCtx[i].pStart = &iStart;
		arrConsCtx[i].pConsumed = &iConsumed;
		arrConsCtx[i].pFailure = &iFailure;
		arrConsumer[i] = xrtThreadCreate(__benchMPMCConsumer, &arrConsCtx[i], 0);
		if ( !arrConsumer[i] ) {
			goto cleanup;
		}
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].iCount = iCountPerProducer;
		arrProdCtx[i].iBaseValue = i * iCountPerProducer;
		arrProdCtx[i].pStart = &iStart;
		arrProdCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPMCProducer, &arrProdCtx[i], 0);
		if ( !arrProducer[i] ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	iStart = 1;
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPMCQClose(hQueue);
	for ( i = 0; i < iConsumerCount; ++i ) {
		xrtThreadWait(arrConsumer[i]);
	}
	xbenchTimerStop(&tTimer);

	if ( xbenchAtomicLoad(&iFailure) != 0 ||
		xbenchAtomicLoad(&iConsumed) != (long)iTotalCount ) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadGetExitCode(arrProducer[i]) != 0u ) {
			goto cleanup;
		}
	}
	for ( i = 0; i < iConsumerCount; ++i ) {
		if ( xrtThreadGetExitCode(arrConsumer[i]) != 0u ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	if ( arrProducer ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] ) xrtThreadDestroy(arrProducer[i]);
		}
		free(arrProducer);
	}
	if ( arrConsumer ) {
		for ( i = 0; i < iConsumerCount; ++i ) {
			if ( arrConsumer[i] ) xrtThreadDestroy(arrConsumer[i]);
		}
		free(arrConsumer);
	}
	if ( arrProdCtx ) free(arrProdCtx);
	if ( arrConsCtx ) free(arrConsCtx);
	if ( hQueue ) xrtMPMCQDestroy(hQueue);
	return iResult;
}

int main(int argc, char** argv)
{
	uint32 iItemsPerProducer = xbenchArgU32(argc, argv, 1, 500000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 4096u);
	uint32 iMpscProducers = xbenchArgU32(argc, argv, 3, 4u);
	uint32 iMpmcProducers = xbenchArgU32(argc, argv, 4, 4u);
	uint32 iMpmcConsumers = xbenchArgU32(argc, argv, 5, 4u);
	uint64_t iSpscElapsedNs = 0u;
	uint64_t iMpscElapsedNs = 0u;
	uint64_t iMpmcElapsedNs = 0u;
	uint64_t iSpscItems;
	uint64_t iMpscItems;
	uint64_t iMpmcItems;

	if ( iItemsPerProducer == 0u ) iItemsPerProducer = 500000u;
	if ( iCapacity == 0u ) iCapacity = 4096u;
	if ( iMpscProducers == 0u ) iMpscProducers = 4u;
	if ( iMpmcProducers == 0u ) iMpmcProducers = 4u;
	if ( iMpmcConsumers == 0u ) iMpmcConsumers = 4u;

	printf("xrt queue bench bench_queue_pointer\n");
	printf("items_per_producer=%" PRIu32 "\n", iItemsPerProducer);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	printf("mpsc_producers=%" PRIu32 "\n", iMpscProducers);
	printf("mpmc_producers=%" PRIu32 "\n", iMpmcProducers);
	printf("mpmc_consumers=%" PRIu32 "\n", iMpmcConsumers);

	if ( !xrtInit() ) {
		fprintf(stderr, "xrt init failed\n");
		return 1;
	}

	if ( __benchRunSPSC(iCapacity, iItemsPerProducer, &iSpscElapsedNs) != 0 ) {
		fprintf(stderr, "spsc bench failed\n");
		xrtUnit();
		return 2;
	}
	if ( __benchRunMPSC(iCapacity, iMpscProducers, iItemsPerProducer, &iMpscElapsedNs) != 0 ) {
		fprintf(stderr, "mpsc bench failed\n");
		xrtUnit();
		return 3;
	}
	if ( __benchRunMPMC(iCapacity, iMpmcProducers, iMpmcConsumers, iItemsPerProducer, &iMpmcElapsedNs) != 0 ) {
		fprintf(stderr, "mpmc bench failed\n");
		xrtUnit();
		return 4;
	}

	iSpscItems = (uint64_t)iItemsPerProducer;
	iMpscItems = (uint64_t)iMpscProducers * (uint64_t)iItemsPerProducer;
	iMpmcItems = (uint64_t)iMpmcProducers * (uint64_t)iItemsPerProducer;

	xbenchPrintMetricU64("spsc_items", iSpscItems);
	xbenchPrintMetricU64("spsc_elapsed_ns", iSpscElapsedNs);
	xbenchPrintMetricDouble("spsc_items_per_sec", xbenchSafeRate(iSpscItems, iSpscElapsedNs));

	xbenchPrintMetricU64("mpsc_items", iMpscItems);
	xbenchPrintMetricU64("mpsc_elapsed_ns", iMpscElapsedNs);
	xbenchPrintMetricDouble("mpsc_items_per_sec", xbenchSafeRate(iMpscItems, iMpscElapsedNs));

	xbenchPrintMetricU64("mpmc_items", iMpmcItems);
	xbenchPrintMetricU64("mpmc_elapsed_ns", iMpmcElapsedNs);
	xbenchPrintMetricDouble("mpmc_items_per_sec", xbenchSafeRate(iMpmcItems, iMpmcElapsedNs));

	xrtUnit();
	return 0;
}
