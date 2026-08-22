#include "../bench_common.h"

#define XRT_FEATURE_TIME
#define XRT_FEATURE_WAIT
#define XRT_FEATURE_THREAD
#define XRT_FEATURE_ATOMIC
#define XRT_FEATURE_QUEUE
#define XRT_FEATURE_QUEUE_SPSC
#define XRT_FEATURE_QUEUE_MPSC
#define XRT_FEATURE_QUEUE_MPMC
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 保存 SPSC 吞吐量测量的共享状态。 */
typedef struct {
	xspscqueue* hQueue;
	uint32 iCount;
	volatile long iStart;
	volatile long iConsumed;
	volatile long iFailure;
} __bench_spsc_ctx;



/* 保存一个 MPSC 生产者的测量参数。 */
typedef struct {
	xmpscqueue* hQueue;
	uint32 iCount;
	uint32 iBaseValue;
	uint32 iBatchSize;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpsc_prod_ctx;



/* 保存 MPSC 消费者的测量参数。 */
typedef struct {
	xmpscqueue* hQueue;
	uint32 iBatchSize;
	volatile long* pStart;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpsc_cons_ctx;



/* 保存一个 MPMC 生产者的测量参数。 */
typedef struct {
	xmpmcqueue* hQueue;
	uint32 iCount;
	uint32 iBaseValue;
	uint32 iBatchSize;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpmc_prod_ctx;



/* 保存一个 MPMC 消费者的测量参数。 */
typedef struct {
	xmpmcqueue* hQueue;
	uint32 iBatchSize;
	volatile long* pStart;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpmc_cons_ctx;



/* 持续向 SPSC 队列写入唯一的非空指针值。 */
static int32 __benchSPSCProducer(ptr pArg)
{
	__bench_spsc_ctx* pCtx = (__bench_spsc_ctx*)pArg;
	uint32 i;

	if ( (pCtx == NULL) || (pCtx->hQueue == NULL) ) {
		return 11u;
	}

	while ( xbenchAtomicLoad(&pCtx->iStart) == 0 ) {
		xrtThreadYield();
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueueresult iRet = xrtSPSCQueueTryPush(pCtx->hQueue, (ptr)(uintptr_t)(i + 1u));
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

	xrtSPSCQueueClose(pCtx->hQueue);
	return 0u;
}



/* 持续排空 SPSC 队列并记录成功消费数量。 */
static int32 __benchSPSCConsumer(ptr pArg)
{
	__bench_spsc_ctx* pCtx = (__bench_spsc_ctx*)pArg;
	ptr pItem = NULL;

	if ( (pCtx == NULL) || (pCtx->hQueue == NULL) ) {
		return 21u;
	}

	while ( xbenchAtomicLoad(&pCtx->iStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueueresult iRet = xrtSPSCQueueTryPop(pCtx->hQueue, &pItem);
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



/* 持续向 MPSC 队列写入当前生产者负责的值域。 */
static int32 __benchMPSCProducer(ptr pArg)
{
	__bench_mpsc_prod_ctx* pCtx = (__bench_mpsc_prod_ctx*)pArg;
	uint32 i;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 31u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueueresult iRet = xrtMPSCQueueTryPush(pCtx->hQueue, (ptr)(uintptr_t)(pCtx->iBaseValue + i + 1u));
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



/* 持续排空 MPSC 队列并记录成功消费数量。 */
static int32 __benchMPSCConsumer(ptr pArg)
{
	__bench_mpsc_cons_ctx* pCtx = (__bench_mpsc_cons_ctx*)pArg;
	ptr pItem = NULL;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 41u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueueresult iRet = xrtMPSCQueueTryPop(pCtx->hQueue, &pItem);
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



/* 使用批量接口向 MPSC 队列写入当前生产者负责的值域。 */
static int32 __benchMPSCBatchProducer(ptr pArg)
{
	__bench_mpsc_prod_ctx* pCtx = (__bench_mpsc_prod_ctx*)pArg;
	ptr* arrItems = NULL;
	uint32 iBatchSize;
	uint32 iIndex = 0u;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 33u;
	}

	iBatchSize = pCtx->iBatchSize ? pCtx->iBatchSize : 1u;
	arrItems = (ptr*)malloc(sizeof(ptr) * iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 3300);
		return 34u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	while ( iIndex < pCtx->iCount ) {
		uint32 iRemain = pCtx->iCount - iIndex;
		uint32 iChunk = iRemain < iBatchSize ? iRemain : iBatchSize;
		uint32 iOffset = 0u;
		uint32 i;

		for ( i = 0; i < iChunk; ++i ) {
			arrItems[i] = (ptr)(uintptr_t)(pCtx->iBaseValue + iIndex + i + 1u);
		}

		while ( iOffset < iChunk ) {
			xqueuebatchresult Batch = xrtMPSCQueuePushBatch(
				pCtx->hQueue,
				&arrItems[iOffset],
				iChunk - iOffset
			);

			if ( Batch.Result == XQUEUE_OK ) {
				if ( Batch.Count == 0u ) {
					xbenchAtomicMax(pCtx->pFailure, 3302);
					free(arrItems);
					return 36u;
				}
				iOffset += (uint32)Batch.Count;
				continue;
			}
			if ( Batch.Result == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			xbenchAtomicMax(pCtx->pFailure, 3301);
			free(arrItems);
			return 35u;
		}

		iIndex += iChunk;
	}

	free(arrItems);
	return 0u;
}



/* 使用批量接口排空 MPSC 队列并校验返回的元素。 */
static int32 __benchMPSCBatchConsumer(ptr pArg)
{
	__bench_mpsc_cons_ctx* pCtx = (__bench_mpsc_cons_ctx*)pArg;
	ptr* arrItems = NULL;
	uint32 iBatchSize;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 43u;
	}

	iBatchSize = pCtx->iBatchSize ? pCtx->iBatchSize : 1u;
	arrItems = (ptr*)malloc(sizeof(ptr) * iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 4300);
		return 44u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueuebatchresult Batch = xrtMPSCQueuePopBatch(
			pCtx->hQueue,
			arrItems,
			iBatchSize
		);

		if ( Batch.Result == XQUEUE_OK ) {
			uint32 i;

			if ( Batch.Count == 0u ) {
				xbenchAtomicMax(pCtx->pFailure, 4303);
				free(arrItems);
				return 47u;
			}
			for ( i = 0; i < (uint32)Batch.Count; ++i ) {
				if ( arrItems[i] == NULL ) {
					xbenchAtomicMax(pCtx->pFailure, 4301);
					free(arrItems);
					return 45u;
				}
			}
			xbenchAtomicAdd(pCtx->pConsumed, (long)Batch.Count);
			continue;
		}
		if ( Batch.Result == XQUEUE_CLOSED ) {
			free(arrItems);
			return 0u;
		}
		if ( Batch.Result == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		xbenchAtomicMax(pCtx->pFailure, 4302);
		free(arrItems);
		return 46u;
	}
}



/* 持续向 MPMC 队列写入当前生产者负责的值域。 */
static int32 __benchMPMCProducer(ptr pArg)
{
	__bench_mpmc_prod_ctx* pCtx = (__bench_mpmc_prod_ctx*)pArg;
	uint32 i;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 51u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( i = 0; i < pCtx->iCount; ++i ) {
		for ( ;; ) {
			xqueueresult iRet = xrtMPMCQueueTryPush(pCtx->hQueue, (ptr)(uintptr_t)(pCtx->iBaseValue + i + 1u));
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



/* 持续排空 MPMC 队列并记录成功消费数量。 */
static int32 __benchMPMCConsumer(ptr pArg)
{
	__bench_mpmc_cons_ctx* pCtx = (__bench_mpmc_cons_ctx*)pArg;
	ptr pItem = NULL;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 61u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueueresult iRet = xrtMPMCQueueTryPop(pCtx->hQueue, &pItem);
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



/* 使用批量接口向 MPMC 队列写入当前生产者负责的值域。 */
static int32 __benchMPMCBatchProducer(ptr pArg)
{
	__bench_mpmc_prod_ctx* pCtx = (__bench_mpmc_prod_ctx*)pArg;
	ptr* arrItems = NULL;
	uint32 iBatchSize;
	uint32 iIndex = 0u;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 53u;
	}

	iBatchSize = pCtx->iBatchSize ? pCtx->iBatchSize : 1u;
	arrItems = (ptr*)malloc(sizeof(ptr) * iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 5300);
		return 54u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	while ( iIndex < pCtx->iCount ) {
		uint32 iRemain = pCtx->iCount - iIndex;
		uint32 iChunk = iRemain < iBatchSize ? iRemain : iBatchSize;
		uint32 iOffset = 0u;
		uint32 i;

		for ( i = 0; i < iChunk; ++i ) {
			arrItems[i] = (ptr)(uintptr_t)(pCtx->iBaseValue + iIndex + i + 1u);
		}

		while ( iOffset < iChunk ) {
			xqueuebatchresult Batch = xrtMPMCQueuePushBatch(
				pCtx->hQueue,
				&arrItems[iOffset],
				iChunk - iOffset
			);

			if ( Batch.Result == XQUEUE_OK ) {
				if ( Batch.Count == 0u ) {
					xbenchAtomicMax(pCtx->pFailure, 5302);
					free(arrItems);
					return 56u;
				}
				iOffset += (uint32)Batch.Count;
				continue;
			}
			if ( Batch.Result == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			xbenchAtomicMax(pCtx->pFailure, 5301);
			free(arrItems);
			return 55u;
		}

		iIndex += iChunk;
	}

	free(arrItems);
	return 0u;
}



/* 使用批量接口排空 MPMC 队列并校验返回的元素。 */
static int32 __benchMPMCBatchConsumer(ptr pArg)
{
	__bench_mpmc_cons_ctx* pCtx = (__bench_mpmc_cons_ctx*)pArg;
	ptr* arrItems = NULL;
	uint32 iBatchSize;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 63u;
	}

	iBatchSize = pCtx->iBatchSize ? pCtx->iBatchSize : 1u;
	arrItems = (ptr*)malloc(sizeof(ptr) * iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 6300);
		return 64u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	for ( ;; ) {
		xqueuebatchresult Batch = xrtMPMCQueuePopBatch(
			pCtx->hQueue,
			arrItems,
			iBatchSize
		);

		if ( Batch.Result == XQUEUE_OK ) {
			uint32 i;

			if ( Batch.Count == 0u ) {
				xbenchAtomicMax(pCtx->pFailure, 6303);
				free(arrItems);
				return 67u;
			}
			for ( i = 0; i < (uint32)Batch.Count; ++i ) {
				if ( arrItems[i] == NULL ) {
					xbenchAtomicMax(pCtx->pFailure, 6301);
					free(arrItems);
					return 65u;
				}
			}
			xbenchAtomicAdd(pCtx->pConsumed, (long)Batch.Count);
			continue;
		}
		if ( Batch.Result == XQUEUE_CLOSED ) {
			free(arrItems);
			return 0u;
		}
		if ( Batch.Result == XQUEUE_EMPTY ) {
			xrtThreadYield();
			continue;
		}
		xbenchAtomicMax(pCtx->pFailure, 6302);
		free(arrItems);
		return 66u;
	}
}



/* 运行一次 SPSC 单元素吞吐量测量。 */
static int __benchRunSPSC(
	uint32 iCapacity,
	uint32 iCount,
	uint64_t* pElapsedNs
)
{
	__bench_spsc_ctx tCtx;
	xthread* hProducer = NULL;
	xthread* hConsumer = NULL;
	xbenchtimer tTimer;
	int iResult = 1;

	if (
		(pElapsedNs == NULL) ||
		(iCount == 0u)
	) {
		return 1;
	}

#if LONG_MAX < UINT32_MAX
	/* 32 位 long 无法记录超过 LONG_MAX 的消费计数。 */
	if ( iCount > (uint32)LONG_MAX ) {
		return 1;
	}
#endif

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = xrtSPSCQueueCreate(iCapacity);
	tCtx.iCount = iCount;
	if ( !tCtx.hQueue ) {
		return 2;
	}

	hProducer = xrtThreadCreate(__benchSPSCProducer, &tCtx, 0);
	hConsumer = xrtThreadCreate(__benchSPSCConsumer, &tCtx, 0);
	if ( (hProducer == NULL) || (hConsumer == NULL) ) {
		goto cleanup;
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&tCtx.iStart, 1);
	xrtThreadWait(hProducer);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if ( xrtThreadExitCode(hProducer) != 0 ||
		xrtThreadExitCode(hConsumer) != 0 ||
		xbenchAtomicLoad(&tCtx.iFailure) != 0 ||
		xbenchAtomicLoad(&tCtx.iConsumed) != (long)iCount ) {
		goto cleanup;
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&tCtx.iStart, 1);
	if ( tCtx.hQueue != NULL ) {
		xrtSPSCQueueClose(tCtx.hQueue);
	}
	if ( hProducer != NULL ) {
		xrtThreadDestroy(hProducer);
	}
	if ( hConsumer != NULL ) {
		xrtThreadDestroy(hConsumer);
	}
	if ( tCtx.hQueue != NULL ) {
		xrtSPSCQueueDestroy(tCtx.hQueue);
	}
	return iResult;
}



/* 运行一次 MPSC 单元素吞吐量测量。 */
static int __benchRunMPSC(
	uint32 iCapacity,
	uint32 iProducerCount,
	uint32 iCountPerProducer,
	uint64_t* pElapsedNs
)
{
	xmpscqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	__bench_mpsc_prod_ctx* arrCtx = NULL;
	__bench_mpsc_cons_ctx tConsCtx;
	xthread* hConsumer = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount;
	int iResult = 1;

	if (
		(pElapsedNs == NULL) ||
		!xbenchCountProductU32(
			iProducerCount,
			iCountPerProducer,
			&iTotalCount
		)
	) {
		return 1;
	}

	memset(&tConsCtx, 0, sizeof(tConsCtx));
	hQueue = xrtMPSCQueueCreate(iCapacity);
	if ( hQueue == NULL ) {
		return 2;
	}

	arrProducer = (xthread**)calloc(iProducerCount, sizeof(xthread*));
	arrCtx = (__bench_mpsc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpsc_prod_ctx));
	if ( (arrProducer == NULL) || (arrCtx == NULL) ) {
		goto cleanup;
	}

	tConsCtx.hQueue = hQueue;
	tConsCtx.pStart = &iStart;
	tConsCtx.pConsumed = &iConsumed;
	tConsCtx.pFailure = &iFailure;
	hConsumer = xrtThreadCreate(__benchMPSCConsumer, &tConsCtx, 0);
	if ( hConsumer == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		arrCtx[i].hQueue = hQueue;
		arrCtx[i].iCount = iCountPerProducer;
		arrCtx[i].iBaseValue = i * iCountPerProducer;
		arrCtx[i].pStart = &iStart;
		arrCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPSCProducer, &arrCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPSCQueueClose(hQueue);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if (
		(xrtThreadExitCode(hConsumer) != 0) ||
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPSCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( hConsumer != NULL ) {
		xrtThreadDestroy(hConsumer);
	}
	if ( arrCtx != NULL ) {
		free(arrCtx);
	}
	if ( hQueue != NULL ) {
		xrtMPSCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 运行一次 MPSC 批量吞吐量测量。 */
static int __benchRunMPSCBatch(
	uint32 iCapacity,
	uint32 iProducerCount,
	uint32 iCountPerProducer,
	uint32 iBatchSize,
	uint64_t* pElapsedNs
)
{
	xmpscqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	__bench_mpsc_prod_ctx* arrCtx = NULL;
	__bench_mpsc_cons_ctx tConsCtx;
	xthread* hConsumer = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount;
	int iResult = 1;

	if (
		(pElapsedNs == NULL) ||
		(iBatchSize == 0u) ||
		!xbenchCountProductU32(
			iProducerCount,
			iCountPerProducer,
			&iTotalCount
		)
	) {
		return 1;
	}

	memset(&tConsCtx, 0, sizeof(tConsCtx));
	hQueue = xrtMPSCQueueCreate(iCapacity);
	if ( hQueue == NULL ) {
		return 2;
	}

	arrProducer = (xthread**)calloc(iProducerCount, sizeof(xthread*));
	arrCtx = (__bench_mpsc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpsc_prod_ctx));
	if ( (arrProducer == NULL) || (arrCtx == NULL) ) {
		goto cleanup;
	}

	tConsCtx.hQueue = hQueue;
	tConsCtx.iBatchSize = iBatchSize;
	tConsCtx.pStart = &iStart;
	tConsCtx.pConsumed = &iConsumed;
	tConsCtx.pFailure = &iFailure;
	hConsumer = xrtThreadCreate(__benchMPSCBatchConsumer, &tConsCtx, 0);
	if ( hConsumer == NULL ) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		arrCtx[i].hQueue = hQueue;
		arrCtx[i].iCount = iCountPerProducer;
		arrCtx[i].iBaseValue = i * iCountPerProducer;
		arrCtx[i].iBatchSize = iBatchSize;
		arrCtx[i].pStart = &iStart;
		arrCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPSCBatchProducer, &arrCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPSCQueueClose(hQueue);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if (
		(xrtThreadExitCode(hConsumer) != 0) ||
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPSCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( hConsumer != NULL ) {
		xrtThreadDestroy(hConsumer);
	}
	if ( arrCtx != NULL ) {
		free(arrCtx);
	}
	if ( hQueue != NULL ) {
		xrtMPSCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 运行一次 MPMC 单元素吞吐量与完整性测量。 */
static int __benchRunMPMC(
	uint32 iCapacity,
	uint32 iProducerCount,
	uint32 iConsumerCount,
	uint32 iCountPerProducer,
	uint64_t* pElapsedNs
)
{
	xmpmcqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	xthread** arrConsumer = NULL;
	__bench_mpmc_prod_ctx* arrProdCtx = NULL;
	__bench_mpmc_cons_ctx* arrConsCtx = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount;
	int iResult = 1;

	if (
		(pElapsedNs == NULL) ||
		(iConsumerCount == 0u) ||
		!xbenchCountProductU32(
			iProducerCount,
			iCountPerProducer,
			&iTotalCount
		)
	) {
		return 1;
	}

	hQueue = xrtMPMCQueueCreate(iCapacity);
	if ( hQueue == NULL ) {
		return 2;
	}

	arrProducer = (xthread**)calloc(iProducerCount, sizeof(xthread*));
	arrConsumer = (xthread**)calloc(iConsumerCount, sizeof(xthread*));
	arrProdCtx = (__bench_mpmc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpmc_prod_ctx));
	arrConsCtx = (__bench_mpmc_cons_ctx*)calloc(iConsumerCount, sizeof(__bench_mpmc_cons_ctx));
	if (
		(arrProducer == NULL) ||
		(arrConsumer == NULL) ||
		(arrProdCtx == NULL) ||
		(arrConsCtx == NULL)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iConsumerCount; ++i ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsCtx[i].pStart = &iStart;
		arrConsCtx[i].pConsumed = &iConsumed;
		arrConsCtx[i].pFailure = &iFailure;
		arrConsumer[i] = xrtThreadCreate(__benchMPMCConsumer, &arrConsCtx[i], 0);
		if ( arrConsumer[i] == NULL ) {
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
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPMCQueueClose(hQueue);
	for ( i = 0; i < iConsumerCount; ++i ) {
		xrtThreadWait(arrConsumer[i]);
	}
	xbenchTimerStop(&tTimer);

	if (
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}
	for ( i = 0; i < iConsumerCount; ++i ) {
		if ( xrtThreadExitCode(arrConsumer[i]) != 0 ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPMCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( arrConsumer != NULL ) {
		for ( i = 0; i < iConsumerCount; ++i ) {
			if ( arrConsumer[i] != NULL ) {
				xrtThreadDestroy(arrConsumer[i]);
			}
		}
		free(arrConsumer);
	}
	if ( arrProdCtx != NULL ) {
		free(arrProdCtx);
	}
	if ( arrConsCtx != NULL ) {
		free(arrConsCtx);
	}
	if ( hQueue != NULL ) {
		xrtMPMCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 运行一次 MPMC 批量吞吐量与完整性测量。 */
static int __benchRunMPMCBatch(
	uint32 iCapacity,
	uint32 iProducerCount,
	uint32 iConsumerCount,
	uint32 iCountPerProducer,
	uint32 iBatchSize,
	uint64_t* pElapsedNs
)
{
	xmpmcqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	xthread** arrConsumer = NULL;
	__bench_mpmc_prod_ctx* arrProdCtx = NULL;
	__bench_mpmc_cons_ctx* arrConsCtx = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	uint32 i;
	uint32 iTotalCount;
	int iResult = 1;

	if (
		(pElapsedNs == NULL) ||
		(iConsumerCount == 0u) ||
		(iBatchSize == 0u) ||
		!xbenchCountProductU32(
			iProducerCount,
			iCountPerProducer,
			&iTotalCount
		)
	) {
		return 1;
	}

	hQueue = xrtMPMCQueueCreate(iCapacity);
	if ( hQueue == NULL ) {
		return 2;
	}

	arrProducer = (xthread**)calloc(iProducerCount, sizeof(xthread*));
	arrConsumer = (xthread**)calloc(iConsumerCount, sizeof(xthread*));
	arrProdCtx = (__bench_mpmc_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpmc_prod_ctx));
	arrConsCtx = (__bench_mpmc_cons_ctx*)calloc(iConsumerCount, sizeof(__bench_mpmc_cons_ctx));
	if (
		(arrProducer == NULL) ||
		(arrConsumer == NULL) ||
		(arrProdCtx == NULL) ||
		(arrConsCtx == NULL)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iConsumerCount; ++i ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsCtx[i].iBatchSize = iBatchSize;
		arrConsCtx[i].pStart = &iStart;
		arrConsCtx[i].pConsumed = &iConsumed;
		arrConsCtx[i].pFailure = &iFailure;
		arrConsumer[i] = xrtThreadCreate(__benchMPMCBatchConsumer, &arrConsCtx[i], 0);
		if ( arrConsumer[i] == NULL ) {
			goto cleanup;
		}
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].iCount = iCountPerProducer;
		arrProdCtx[i].iBaseValue = i * iCountPerProducer;
		arrProdCtx[i].iBatchSize = iBatchSize;
		arrProdCtx[i].pStart = &iStart;
		arrProdCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPMCBatchProducer, &arrProdCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPMCQueueClose(hQueue);
	for ( i = 0; i < iConsumerCount; ++i ) {
		xrtThreadWait(arrConsumer[i]);
	}
	xbenchTimerStop(&tTimer);

	if (
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount)
	) {
		goto cleanup;
	}

	for ( i = 0; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}
	for ( i = 0; i < iConsumerCount; ++i ) {
		if ( xrtThreadExitCode(arrConsumer[i]) != 0 ) {
			goto cleanup;
		}
	}

	*pElapsedNs = xbenchTimerElapsedNs(&tTimer);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPMCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( arrConsumer != NULL ) {
		for ( i = 0; i < iConsumerCount; ++i ) {
			if ( arrConsumer[i] != NULL ) {
				xrtThreadDestroy(arrConsumer[i]);
			}
		}
		free(arrConsumer);
	}
	if ( arrProdCtx != NULL ) {
		free(arrProdCtx);
	}
	if ( arrConsCtx != NULL ) {
		free(arrConsCtx);
	}
	if ( hQueue != NULL ) {
		xrtMPMCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 解析测量矩阵并依次输出所有队列吞吐量指标。 */
int main(int argc, char** argv)
{
	uint32 iItemsPerProducer = xbenchArgU32(argc, argv, 1, 500000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 4096u);
	uint32 iMpscProducers = xbenchArgU32(argc, argv, 3, 4u);
	uint32 iMpmcProducers = xbenchArgU32(argc, argv, 4, 4u);
	uint32 iMpmcConsumers = xbenchArgU32(argc, argv, 5, 4u);
	uint32 iBatchSize = xbenchArgU32(argc, argv, 6, 32u);
	uint64_t iSpscElapsedNs = 0u;
	uint64_t iMpscElapsedNs = 0u;
	uint64_t iMpmcElapsedNs = 0u;
	uint64_t iMpscBatchElapsedNs = 0u;
	uint64_t iMpmcBatchElapsedNs = 0u;
	uint64_t iSpscItems;
	uint64_t iMpscItems;
	uint64_t iMpmcItems;

	if ( iItemsPerProducer == 0u ) {
		iItemsPerProducer = 500000u;
	}
	if ( iCapacity == 0u ) {
		iCapacity = 4096u;
	}
	if ( iMpscProducers == 0u ) {
		iMpscProducers = 4u;
	}
	if ( iMpmcProducers == 0u ) {
		iMpmcProducers = 4u;
	}
	if ( iMpmcConsumers == 0u ) {
		iMpmcConsumers = 4u;
	}
	if ( iBatchSize == 0u ) {
		iBatchSize = 32u;
	}

	printf("xrt queue bench bench_queue_pointer\n");
	printf("items_per_producer=%" PRIu32 "\n", iItemsPerProducer);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	printf("mpsc_producers=%" PRIu32 "\n", iMpscProducers);
	printf("mpmc_producers=%" PRIu32 "\n", iMpmcProducers);
	printf("mpmc_consumers=%" PRIu32 "\n", iMpmcConsumers);
	printf("batch_size=%" PRIu32 "\n", iBatchSize);

	xbenchApplyCpuPinFromEnv();

	if ( __benchRunSPSC(iCapacity, iItemsPerProducer, &iSpscElapsedNs) != 0 ) {
		fprintf(stderr, "spsc bench failed\n");
		return 2;
	}
	if ( __benchRunMPSC(iCapacity, iMpscProducers, iItemsPerProducer, &iMpscElapsedNs) != 0 ) {
		fprintf(stderr, "mpsc bench failed\n");
		return 3;
	}
	if ( __benchRunMPMC(iCapacity, iMpmcProducers, iMpmcConsumers, iItemsPerProducer, &iMpmcElapsedNs) != 0 ) {
		fprintf(stderr, "mpmc bench failed\n");
		return 4;
	}
	if ( __benchRunMPSCBatch(iCapacity, iMpscProducers, iItemsPerProducer, iBatchSize, &iMpscBatchElapsedNs) != 0 ) {
		fprintf(stderr, "mpsc batch bench failed\n");
		return 5;
	}
	if ( __benchRunMPMCBatch(iCapacity, iMpmcProducers, iMpmcConsumers, iItemsPerProducer, iBatchSize, &iMpmcBatchElapsedNs) != 0 ) {
		fprintf(stderr, "mpmc batch bench failed\n");
		return 6;
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
	xbenchPrintMetricU64("mpsc_batch_size", iBatchSize);
	xbenchPrintMetricU64("mpsc_batch_items", iMpscItems);
	xbenchPrintMetricU64("mpsc_batch_elapsed_ns", iMpscBatchElapsedNs);
	xbenchPrintMetricDouble("mpsc_batch_items_per_sec", xbenchSafeRate(iMpscItems, iMpscBatchElapsedNs));

	xbenchPrintMetricU64("mpmc_items", iMpmcItems);
	xbenchPrintMetricU64("mpmc_elapsed_ns", iMpmcElapsedNs);
	xbenchPrintMetricDouble("mpmc_items_per_sec", xbenchSafeRate(iMpmcItems, iMpmcElapsedNs));
	xbenchPrintMetricU64("mpmc_batch_size", iBatchSize);
	xbenchPrintMetricU64("mpmc_batch_items", iMpmcItems);
	xbenchPrintMetricU64("mpmc_batch_elapsed_ns", iMpmcBatchElapsedNs);
	xbenchPrintMetricDouble("mpmc_batch_items_per_sec", xbenchSafeRate(iMpmcItems, iMpmcBatchElapsedNs));

	return 0;
}
