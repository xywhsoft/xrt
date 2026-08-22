#include "../bench_common.h"

#define XRT_FEATURE_TIME
#define XRT_FEATURE_WAIT
#define XRT_FEATURE_THREAD
#define XRT_FEATURE_ATOMIC
#define XRT_FEATURE_QUEUE
#define XRT_FEATURE_QUEUE_MPSC
#define XRT_FEATURE_QUEUE_MPMC
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"

#include <math.h>



/* 保存一条进入队列时带单调时间戳的测量消息。 */
typedef struct {
	uint64_t iEnqueueNs;
	uint32_t iProducerId;
	uint32_t iSequence;
} __bench_queue_msg;



/* 保存一组延迟样本的汇总指标。 */
typedef struct {
	uint64_t iElapsedNs;
	uint64_t iItemCount;
	double fAvgUs;
	double fP50Us;
	double fP95Us;
	double fP99Us;
} __bench_latency_metrics;



/* 保存 MPMC 消费者分配的公平性指标。 */
typedef struct {
	uint64_t iMinItems;
	uint64_t iMaxItems;
	double fSpreadPct;
	double fMaxToMinRatio;
} __bench_fairness_metrics;



/* 保存一个 MPSC 延迟生产者的测量参数。 */
typedef struct {
	xmpscqueue* hQueue;
	__bench_queue_msg* arrMessages;
	uint32_t iCount;
	uint32_t iProducerId;
	uint32_t iBaseSequence;
	uint32_t iBatchSize;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpsc_latency_prod_ctx;



/* 保存 MPSC 延迟消费者的测量参数。 */
typedef struct {
	xmpscqueue* hQueue;
	uint32_t iBatchSize;
	uint64_t* arrLatencyNs;
	volatile long* pStart;
	volatile long* pLatencyIndex;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpsc_latency_cons_ctx;



/* 保存一个 MPMC 延迟生产者的测量参数。 */
typedef struct {
	xmpmcqueue* hQueue;
	__bench_queue_msg* arrMessages;
	uint32_t iCount;
	uint32_t iProducerId;
	uint32_t iBaseSequence;
	uint32_t iBatchSize;
	volatile long* pStart;
	volatile long* pFailure;
} __bench_mpmc_latency_prod_ctx;



/* 保存一个 MPMC 延迟消费者的测量参数。 */
typedef struct {
	xmpmcqueue* hQueue;
	uint32_t iBatchSize;
	uint32_t iConsumerId;
	uint64_t* arrLatencyNs;
	uint64_t* arrConsumerCounts;
	volatile long* pStart;
	volatile long* pLatencyIndex;
	volatile long* pConsumed;
	volatile long* pFailure;
} __bench_mpmc_latency_cons_ctx;



/* 为 qsort 比较两个纳秒延迟样本。 */
static int __benchLatencyCmp(const void* pA, const void* pB)
{
	const uint64_t* pLeft = (const uint64_t*)pA;
	const uint64_t* pRight = (const uint64_t*)pB;

	if ( *pLeft < *pRight ) {
		return -1;
	}
	if ( *pLeft > *pRight ) {
		return 1;
	}
	return 0;
}



/* 从已经排序的纳秒样本中读取指定分位数并换算为微秒。 */
static double __benchLatencyPercentileUs(
	const uint64_t* arrLatencyNs,
	uint32_t iCount,
	double fQuantile
)
{
	uint32_t iIndex;

	if ( (arrLatencyNs == NULL) || (iCount == 0u) ) {
		return 0.0;
	}
	if ( fQuantile <= 0.0 ) {
		return (double)arrLatencyNs[0] / 1000.0;
	}
	if ( fQuantile >= 1.0 ) {
		return (double)arrLatencyNs[iCount - 1u] / 1000.0;
	}
	iIndex = (uint32_t)((double)(iCount - 1u) * fQuantile);
	return (double)arrLatencyNs[iIndex] / 1000.0;
}



/* 排序延迟样本并生成平均值和关键分位数。 */
static int __benchFinalizeLatencyMetrics(
	uint64_t* arrLatencyNs,
	uint32_t iCount,
	uint64_t iElapsedNs,
	__bench_latency_metrics* pMetrics
)
{
	long double fSumNs = 0.0;
	uint32_t i;

	if (
		(arrLatencyNs == NULL) ||
		(pMetrics == NULL) ||
		(iCount == 0u)
	) {
		return 1;
	}

	for ( i = 0; i < iCount; ++i ) {
		fSumNs += (long double)arrLatencyNs[i];
	}

	qsort(arrLatencyNs, iCount, sizeof(uint64_t), __benchLatencyCmp);

	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iElapsedNs = iElapsedNs;
	pMetrics->iItemCount = iCount;
	pMetrics->fAvgUs = (double)(fSumNs / (long double)iCount / 1000.0);
	pMetrics->fP50Us = __benchLatencyPercentileUs(arrLatencyNs, iCount, 0.50);
	pMetrics->fP95Us = __benchLatencyPercentileUs(arrLatencyNs, iCount, 0.95);
	pMetrics->fP99Us = __benchLatencyPercentileUs(arrLatencyNs, iCount, 0.99);
	return 0;
}



/* 根据每个消费者的处理数量计算负载离散程度。 */
static void __benchComputeFairnessMetrics(
	const uint64_t* arrCounts,
	uint32_t iCount,
	uint64_t iTotalCount,
	__bench_fairness_metrics* pMetrics
)
{
	uint64_t iMinItems = 0u;
	uint64_t iMaxItems = 0u;
	double fIdeal = 0.0;
	double fSpreadPct = 0.0;
	uint32_t i;

	if ( pMetrics == NULL ) {
		return;
	}

	memset(pMetrics, 0, sizeof(*pMetrics));
	if ( (arrCounts == NULL) || (iCount == 0u) ) {
		return;
	}

	iMinItems = arrCounts[0];
	iMaxItems = arrCounts[0];
	fIdeal = (double)iTotalCount / (double)iCount;

	for ( i = 0; i < iCount; ++i ) {
		double fDeviationPct;

		if ( arrCounts[i] < iMinItems ) {
			iMinItems = arrCounts[i];
		}
		if ( arrCounts[i] > iMaxItems ) {
			iMaxItems = arrCounts[i];
		}
		if ( fIdeal <= 0.0 ) {
			continue;
		}
		fDeviationPct = fabs(((double)arrCounts[i] - fIdeal) / fIdeal) * 100.0;
		if ( fDeviationPct > fSpreadPct ) {
			fSpreadPct = fDeviationPct;
		}
	}

	pMetrics->iMinItems = iMinItems;
	pMetrics->iMaxItems = iMaxItems;
	pMetrics->fSpreadPct = fSpreadPct;
	pMetrics->fMaxToMinRatio = iMinItems == 0u ?
		0.0 :
		((double)iMaxItems / (double)iMinItems);
}



/* 向 MPSC 队列写入带时间戳的单元素或批量消息。 */
static int32 __benchMPSCLatencyProducer(ptr pArg)
{
	__bench_mpsc_latency_prod_ctx* pCtx = (__bench_mpsc_latency_prod_ctx*)pArg;
	ptr* arrItems;
	uint32_t iIndex = 0u;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->arrMessages == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 11u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	if ( pCtx->iBatchSize <= 1u ) {
		for ( iIndex = 0u; iIndex < pCtx->iCount; ++iIndex ) {
			__bench_queue_msg* pMsg = &pCtx->arrMessages[iIndex];
			pMsg->iProducerId = pCtx->iProducerId;
			pMsg->iSequence = pCtx->iBaseSequence + iIndex + 1u;
			for ( ;; ) {
				xqueueresult iRet;

				pMsg->iEnqueueNs = xbenchNowNs();
				iRet = xrtMPSCQueueTryPush(pCtx->hQueue, pMsg);
				if ( iRet == XQUEUE_OK ) {
					break;
				}
				if ( iRet == XQUEUE_FULL ) {
					xrtThreadYield();
					continue;
				}
				xbenchAtomicMax(pCtx->pFailure, (long)(1100 + iRet));
				return 12u;
			}
		}
		return 0u;
	}

	arrItems = (ptr*)malloc(sizeof(ptr) * pCtx->iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 1301);
		return 14u;
	}

	for ( iIndex = 0u; iIndex < pCtx->iCount; ) {
		uint32_t iChunk = pCtx->iCount - iIndex;
		uint32_t iOffset = 0u;
		uint32_t i;
		uint64_t iNowNs;

		if ( iChunk > pCtx->iBatchSize ) {
			iChunk = pCtx->iBatchSize;
		}

		for ( i = 0u; i < iChunk; ++i ) {
			__bench_queue_msg* pMsg = &pCtx->arrMessages[iIndex + i];
			pMsg->iProducerId = pCtx->iProducerId;
			pMsg->iSequence = pCtx->iBaseSequence + iIndex + i + 1u;
			arrItems[i] = pMsg;
		}

		while ( iOffset < iChunk ) {
			xqueuebatchresult Batch;

			iNowNs = xbenchNowNs();
			for ( i = iOffset; i < iChunk; ++i ) {
				((__bench_queue_msg*)arrItems[i])->iEnqueueNs = iNowNs;
			}
			Batch = xrtMPSCQueuePushBatch(
				pCtx->hQueue,
				&arrItems[iOffset],
				iChunk - iOffset
			);
			if ( Batch.Result == XQUEUE_OK ) {
				if ( Batch.Count == 0u ) {
					xbenchAtomicMax(pCtx->pFailure, 1302);
					free(arrItems);
					return 15u;
				}
				iOffset += (uint32)Batch.Count;
				continue;
			}
			if ( Batch.Result == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			xbenchAtomicMax(pCtx->pFailure, 1300);
			free(arrItems);
			return 13u;
		}

		iIndex += iChunk;
	}
	free(arrItems);

	return 0u;
}



/* 从 MPSC 队列读取消息并记录单元素或批量排队延迟。 */
static int32 __benchMPSCLatencyConsumer(ptr pArg)
{
	__bench_mpsc_latency_cons_ctx* pCtx = (__bench_mpsc_latency_cons_ctx*)pArg;
	ptr* arrItems;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->arrLatencyNs == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pLatencyIndex == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 21u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	if ( pCtx->iBatchSize <= 1u ) {
		for ( ;; ) {
			ptr pItem = NULL;
			xqueueresult iRet = xrtMPSCQueueTryPop(pCtx->hQueue, &pItem);
			if ( iRet == XQUEUE_OK ) {
				__bench_queue_msg* pMsg = (__bench_queue_msg*)pItem;
				long iSample;
				uint64_t iNowNs;

				if ( (pMsg == NULL) || (pMsg->iEnqueueNs == 0u) ) {
					xbenchAtomicMax(pCtx->pFailure, 2100);
					return 22u;
				}
				iNowNs = xbenchNowNs();
				iSample = xbenchAtomicAdd(pCtx->pLatencyIndex, 1) - 1;
				pCtx->arrLatencyNs[iSample] = iNowNs - pMsg->iEnqueueNs;
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
			xbenchAtomicMax(pCtx->pFailure, (long)(2200 + iRet));
			return 23u;
		}
	}

	arrItems = (ptr*)malloc(sizeof(ptr) * pCtx->iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 2301);
		return 25u;
	}

	for ( ;; ) {
		xqueuebatchresult Batch;
		uint32_t i;
		uint64_t iNowNs;
		long iSample;

		Batch = xrtMPSCQueuePopBatch(
			pCtx->hQueue,
			arrItems,
			pCtx->iBatchSize
		);
		if ( Batch.Result == XQUEUE_OK ) {
			if ( Batch.Count == 0u ) {
				xbenchAtomicMax(pCtx->pFailure, 2303);
				free(arrItems);
				return 27u;
			}
			iNowNs = xbenchNowNs();
			iSample = xbenchAtomicAdd(
				pCtx->pLatencyIndex,
				(long)Batch.Count
			) - (long)Batch.Count;
			for ( i = 0u; i < (uint32)Batch.Count; ++i ) {
				__bench_queue_msg* pMsg = (__bench_queue_msg*)arrItems[i];

				if ( (pMsg == NULL) || (pMsg->iEnqueueNs == 0u) ) {
					xbenchAtomicMax(pCtx->pFailure, 2300);
					free(arrItems);
					return 24u;
				}
				pCtx->arrLatencyNs[iSample + (long)i] = iNowNs - pMsg->iEnqueueNs;
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
		xbenchAtomicMax(pCtx->pFailure, 2302);
		free(arrItems);
		return 26u;
	}
}



/* 向 MPMC 队列写入带时间戳的单元素或批量消息。 */
static int32 __benchMPMCLatencyProducer(ptr pArg)
{
	__bench_mpmc_latency_prod_ctx* pCtx = (__bench_mpmc_latency_prod_ctx*)pArg;
	ptr* arrItems;
	uint32_t iIndex = 0u;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->arrMessages == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 31u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	if ( pCtx->iBatchSize <= 1u ) {
		for ( iIndex = 0u; iIndex < pCtx->iCount; ++iIndex ) {
			__bench_queue_msg* pMsg = &pCtx->arrMessages[iIndex];
			pMsg->iProducerId = pCtx->iProducerId;
			pMsg->iSequence = pCtx->iBaseSequence + iIndex + 1u;
			for ( ;; ) {
				xqueueresult iRet;

				pMsg->iEnqueueNs = xbenchNowNs();
				iRet = xrtMPMCQueueTryPush(pCtx->hQueue, pMsg);
				if ( iRet == XQUEUE_OK ) {
					break;
				}
				if ( iRet == XQUEUE_FULL ) {
					xrtThreadYield();
					continue;
				}
				xbenchAtomicMax(pCtx->pFailure, (long)(3100 + iRet));
				return 32u;
			}
		}
		return 0u;
	}

	arrItems = (ptr*)malloc(sizeof(ptr) * pCtx->iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 3301);
		return 34u;
	}

	for ( iIndex = 0u; iIndex < pCtx->iCount; ) {
		uint32_t iChunk = pCtx->iCount - iIndex;
		uint32_t iOffset = 0u;
		uint32_t i;
		uint64_t iNowNs;

		if ( iChunk > pCtx->iBatchSize ) {
			iChunk = pCtx->iBatchSize;
		}

		for ( i = 0u; i < iChunk; ++i ) {
			__bench_queue_msg* pMsg = &pCtx->arrMessages[iIndex + i];
			pMsg->iProducerId = pCtx->iProducerId;
			pMsg->iSequence = pCtx->iBaseSequence + iIndex + i + 1u;
			arrItems[i] = pMsg;
		}

		while ( iOffset < iChunk ) {
			xqueuebatchresult Batch;

			iNowNs = xbenchNowNs();
			for ( i = iOffset; i < iChunk; ++i ) {
				((__bench_queue_msg*)arrItems[i])->iEnqueueNs = iNowNs;
			}
			Batch = xrtMPMCQueuePushBatch(
				pCtx->hQueue,
				&arrItems[iOffset],
				iChunk - iOffset
			);
			if ( Batch.Result == XQUEUE_OK ) {
				if ( Batch.Count == 0u ) {
					xbenchAtomicMax(pCtx->pFailure, 3302);
					free(arrItems);
					return 35u;
				}
				iOffset += (uint32)Batch.Count;
				continue;
			}
			if ( Batch.Result == XQUEUE_FULL ) {
				xrtThreadYield();
				continue;
			}
			xbenchAtomicMax(pCtx->pFailure, 3300);
			free(arrItems);
			return 33u;
		}

		iIndex += iChunk;
	}
	free(arrItems);

	return 0u;
}



/* 从 MPMC 队列读取消息并记录延迟和消费者公平性。 */
static int32 __benchMPMCLatencyConsumer(ptr pArg)
{
	__bench_mpmc_latency_cons_ctx* pCtx = (__bench_mpmc_latency_cons_ctx*)pArg;
	ptr* arrItems;
	uint64_t iLocalCount = 0u;

	if (
		(pCtx == NULL) ||
		(pCtx->hQueue == NULL) ||
		(pCtx->arrLatencyNs == NULL) ||
		(pCtx->arrConsumerCounts == NULL) ||
		(pCtx->pStart == NULL) ||
		(pCtx->pLatencyIndex == NULL) ||
		(pCtx->pConsumed == NULL) ||
		(pCtx->pFailure == NULL)
	) {
		return 41u;
	}

	while ( xbenchAtomicLoad(pCtx->pStart) == 0 ) {
		xrtThreadYield();
	}

	if ( pCtx->iBatchSize <= 1u ) {
		for ( ;; ) {
			ptr pItem = NULL;
			xqueueresult iRet = xrtMPMCQueueTryPop(pCtx->hQueue, &pItem);
			if ( iRet == XQUEUE_OK ) {
				__bench_queue_msg* pMsg = (__bench_queue_msg*)pItem;
				long iSample;
				uint64_t iNowNs;

				if ( (pMsg == NULL) || (pMsg->iEnqueueNs == 0u) ) {
					xbenchAtomicMax(pCtx->pFailure, 4100);
					return 42u;
				}
				iNowNs = xbenchNowNs();
				iSample = xbenchAtomicAdd(pCtx->pLatencyIndex, 1) - 1;
				pCtx->arrLatencyNs[iSample] = iNowNs - pMsg->iEnqueueNs;
				xbenchAtomicInc(pCtx->pConsumed);
				iLocalCount++;
				continue;
			}
			if ( iRet == XQUEUE_EMPTY ) {
				xrtThreadYield();
				continue;
			}
			if ( iRet == XQUEUE_CLOSED ) {
				pCtx->arrConsumerCounts[pCtx->iConsumerId] = iLocalCount;
				return 0u;
			}
			xbenchAtomicMax(pCtx->pFailure, (long)(4200 + iRet));
			return 43u;
		}
	}

	arrItems = (ptr*)malloc(sizeof(ptr) * pCtx->iBatchSize);
	if ( arrItems == NULL ) {
		xbenchAtomicMax(pCtx->pFailure, 4301);
		return 45u;
	}

	for ( ;; ) {
		xqueuebatchresult Batch;
		uint32_t i;
		uint64_t iNowNs;
		long iSample;

		Batch = xrtMPMCQueuePopBatch(
			pCtx->hQueue,
			arrItems,
			pCtx->iBatchSize
		);
		if ( Batch.Result == XQUEUE_OK ) {
			if ( Batch.Count == 0u ) {
				xbenchAtomicMax(pCtx->pFailure, 4303);
				free(arrItems);
				return 47u;
			}
			iNowNs = xbenchNowNs();
			iSample = xbenchAtomicAdd(
				pCtx->pLatencyIndex,
				(long)Batch.Count
			) - (long)Batch.Count;
			for ( i = 0u; i < (uint32)Batch.Count; ++i ) {
				__bench_queue_msg* pMsg = (__bench_queue_msg*)arrItems[i];

				if ( (pMsg == NULL) || (pMsg->iEnqueueNs == 0u) ) {
					xbenchAtomicMax(pCtx->pFailure, 4300);
					free(arrItems);
					return 44u;
				}
				pCtx->arrLatencyNs[iSample + (long)i] = iNowNs - pMsg->iEnqueueNs;
			}
			xbenchAtomicAdd(pCtx->pConsumed, (long)Batch.Count);
			iLocalCount += Batch.Count;
			continue;
		}
		if ( Batch.Result == XQUEUE_CLOSED ) {
			pCtx->arrConsumerCounts[pCtx->iConsumerId] = iLocalCount;
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



/* 运行一次 MPSC 延迟测量并汇总样本。 */
static int __benchRunMPSCLatency(
	uint32_t iCapacity,
	uint32_t iProducerCount,
	uint32_t iCountPerProducer,
	uint32_t iBatchSize,
	__bench_latency_metrics* pMetrics
)
{
	xmpscqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	__bench_mpsc_latency_prod_ctx* arrProdCtx = NULL;
	__bench_mpsc_latency_cons_ctx tConsCtx;
	xthread* hConsumer = NULL;
	__bench_queue_msg* arrMessages = NULL;
	uint64_t* arrLatencyNs = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	volatile long iLatencyIndex = 0;
	uint32_t i;
	uint32_t iTotalCount;
	int iResult = 1;

	if (
		(pMetrics == NULL) ||
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
	arrProdCtx = (__bench_mpsc_latency_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpsc_latency_prod_ctx));
	arrMessages = (__bench_queue_msg*)calloc(iTotalCount, sizeof(__bench_queue_msg));
	arrLatencyNs = (uint64_t*)calloc(iTotalCount, sizeof(uint64_t));
	if (
		(arrProducer == NULL) ||
		(arrProdCtx == NULL) ||
		(arrMessages == NULL) ||
		(arrLatencyNs == NULL)
	) {
		goto cleanup;
	}

	tConsCtx.hQueue = hQueue;
	tConsCtx.iBatchSize = iBatchSize;
	tConsCtx.arrLatencyNs = arrLatencyNs;
	tConsCtx.pStart = &iStart;
	tConsCtx.pLatencyIndex = &iLatencyIndex;
	tConsCtx.pConsumed = &iConsumed;
	tConsCtx.pFailure = &iFailure;
	hConsumer = xrtThreadCreate(__benchMPSCLatencyConsumer, &tConsCtx, 0);
	if ( hConsumer == NULL ) {
		goto cleanup;
	}

	for ( i = 0u; i < iProducerCount; ++i ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].arrMessages = &arrMessages[i * iCountPerProducer];
		arrProdCtx[i].iCount = iCountPerProducer;
		arrProdCtx[i].iProducerId = i;
		arrProdCtx[i].iBaseSequence = i * iCountPerProducer;
		arrProdCtx[i].iBatchSize = iBatchSize;
		arrProdCtx[i].pStart = &iStart;
		arrProdCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPSCLatencyProducer, &arrProdCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0u; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPSCQueueClose(hQueue);
	xrtThreadWait(hConsumer);
	xbenchTimerStop(&tTimer);

	if (
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount) ||
		(xbenchAtomicLoad(&iLatencyIndex) != (long)iTotalCount) ||
		(xrtThreadExitCode(hConsumer) != 0)
	) {
		goto cleanup;
	}

	for ( i = 0u; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}

	if (
		__benchFinalizeLatencyMetrics(
			arrLatencyNs,
			iTotalCount,
			xbenchTimerElapsedNs(&tTimer),
			pMetrics
		) != 0
	) {
		goto cleanup;
	}
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPSCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0u; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( hConsumer != NULL ) {
		xrtThreadDestroy(hConsumer);
	}
	if ( arrProdCtx != NULL ) {
		free(arrProdCtx);
	}
	if ( arrMessages != NULL ) {
		free(arrMessages);
	}
	if ( arrLatencyNs != NULL ) {
		free(arrLatencyNs);
	}
	if ( hQueue != NULL ) {
		xrtMPSCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 运行一次 MPMC 延迟与消费者公平性测量。 */
static int __benchRunMPMCLatency(
	uint32_t iCapacity,
	uint32_t iProducerCount,
	uint32_t iConsumerCount,
	uint32_t iCountPerProducer,
	uint32_t iBatchSize,
	__bench_latency_metrics* pLatencyMetrics,
	__bench_fairness_metrics* pFairnessMetrics
)
{
	xmpmcqueue* hQueue = NULL;
	xthread** arrProducer = NULL;
	xthread** arrConsumer = NULL;
	__bench_mpmc_latency_prod_ctx* arrProdCtx = NULL;
	__bench_mpmc_latency_cons_ctx* arrConsCtx = NULL;
	__bench_queue_msg* arrMessages = NULL;
	uint64_t* arrLatencyNs = NULL;
	uint64_t* arrConsumerCounts = NULL;
	xbenchtimer tTimer;
	volatile long iStart = 0;
	volatile long iConsumed = 0;
	volatile long iFailure = 0;
	volatile long iLatencyIndex = 0;
	uint32_t i;
	uint32_t iTotalCount;
	int iResult = 1;

	if (
		(pLatencyMetrics == NULL) ||
		(pFairnessMetrics == NULL) ||
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
	arrProdCtx = (__bench_mpmc_latency_prod_ctx*)calloc(iProducerCount, sizeof(__bench_mpmc_latency_prod_ctx));
	arrConsCtx = (__bench_mpmc_latency_cons_ctx*)calloc(iConsumerCount, sizeof(__bench_mpmc_latency_cons_ctx));
	arrMessages = (__bench_queue_msg*)calloc(iTotalCount, sizeof(__bench_queue_msg));
	arrLatencyNs = (uint64_t*)calloc(iTotalCount, sizeof(uint64_t));
	arrConsumerCounts = (uint64_t*)calloc(iConsumerCount, sizeof(uint64_t));
	if (
		(arrProducer == NULL) ||
		(arrConsumer == NULL) ||
		(arrProdCtx == NULL) ||
		(arrConsCtx == NULL) ||
		(arrMessages == NULL) ||
		(arrLatencyNs == NULL) ||
		(arrConsumerCounts == NULL)
	) {
		goto cleanup;
	}

	for ( i = 0u; i < iConsumerCount; ++i ) {
		arrConsCtx[i].hQueue = hQueue;
		arrConsCtx[i].iBatchSize = iBatchSize;
		arrConsCtx[i].iConsumerId = i;
		arrConsCtx[i].arrLatencyNs = arrLatencyNs;
		arrConsCtx[i].arrConsumerCounts = arrConsumerCounts;
		arrConsCtx[i].pStart = &iStart;
		arrConsCtx[i].pLatencyIndex = &iLatencyIndex;
		arrConsCtx[i].pConsumed = &iConsumed;
		arrConsCtx[i].pFailure = &iFailure;
		arrConsumer[i] = xrtThreadCreate(__benchMPMCLatencyConsumer, &arrConsCtx[i], 0);
		if ( arrConsumer[i] == NULL ) {
			goto cleanup;
		}
	}

	for ( i = 0u; i < iProducerCount; ++i ) {
		arrProdCtx[i].hQueue = hQueue;
		arrProdCtx[i].arrMessages = &arrMessages[i * iCountPerProducer];
		arrProdCtx[i].iCount = iCountPerProducer;
		arrProdCtx[i].iProducerId = i;
		arrProdCtx[i].iBaseSequence = i * iCountPerProducer;
		arrProdCtx[i].iBatchSize = iBatchSize;
		arrProdCtx[i].pStart = &iStart;
		arrProdCtx[i].pFailure = &iFailure;
		arrProducer[i] = xrtThreadCreate(__benchMPMCLatencyProducer, &arrProdCtx[i], 0);
		if ( arrProducer[i] == NULL ) {
			goto cleanup;
		}
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&iStart, 1);
	for ( i = 0u; i < iProducerCount; ++i ) {
		xrtThreadWait(arrProducer[i]);
	}
	xrtMPMCQueueClose(hQueue);
	for ( i = 0u; i < iConsumerCount; ++i ) {
		xrtThreadWait(arrConsumer[i]);
	}
	xbenchTimerStop(&tTimer);

	if (
		(xbenchAtomicLoad(&iFailure) != 0) ||
		(xbenchAtomicLoad(&iConsumed) != (long)iTotalCount) ||
		(xbenchAtomicLoad(&iLatencyIndex) != (long)iTotalCount)
	) {
		goto cleanup;
	}

	for ( i = 0u; i < iProducerCount; ++i ) {
		if ( xrtThreadExitCode(arrProducer[i]) != 0 ) {
			goto cleanup;
		}
	}
	for ( i = 0u; i < iConsumerCount; ++i ) {
		if ( xrtThreadExitCode(arrConsumer[i]) != 0 ) {
			goto cleanup;
		}
	}

	if (
		__benchFinalizeLatencyMetrics(
			arrLatencyNs,
			iTotalCount,
			xbenchTimerElapsedNs(&tTimer),
			pLatencyMetrics
		) != 0
	) {
		goto cleanup;
	}
	__benchComputeFairnessMetrics(arrConsumerCounts, iConsumerCount, iTotalCount, pFairnessMetrics);
	iResult = 0;

cleanup:
	xbenchAtomicStore(&iStart, 1);
	if ( hQueue != NULL ) {
		xrtMPMCQueueClose(hQueue);
	}
	if ( arrProducer != NULL ) {
		for ( i = 0u; i < iProducerCount; ++i ) {
			if ( arrProducer[i] != NULL ) {
				xrtThreadDestroy(arrProducer[i]);
			}
		}
		free(arrProducer);
	}
	if ( arrConsumer != NULL ) {
		for ( i = 0u; i < iConsumerCount; ++i ) {
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
	if ( arrMessages != NULL ) {
		free(arrMessages);
	}
	if ( arrLatencyNs != NULL ) {
		free(arrLatencyNs);
	}
	if ( arrConsumerCounts != NULL ) {
		free(arrConsumerCounts);
	}
	if ( hQueue != NULL ) {
		xrtMPMCQueueDestroy(hQueue);
	}
	return iResult;
}



/* 解析测量矩阵并依次输出所有队列延迟与公平性指标。 */
int main(int argc, char** argv)
{
	uint32_t iItemsPerProducer = xbenchArgU32(argc, argv, 1, 100000u);
	uint32_t iCapacity = xbenchArgU32(argc, argv, 2, 4096u);
	uint32_t iMpscProducers = xbenchArgU32(argc, argv, 3, 4u);
	uint32_t iMpmcProducers = xbenchArgU32(argc, argv, 4, 4u);
	uint32_t iMpmcConsumers = xbenchArgU32(argc, argv, 5, 4u);
	uint32_t iBatchSize = xbenchArgU32(argc, argv, 6, 32u);
	__bench_latency_metrics tMpscSingle;
	__bench_latency_metrics tMpscBatch;
	__bench_latency_metrics tMpmcSingle;
	__bench_latency_metrics tMpmcBatch;
	__bench_fairness_metrics tMpmcSingleFairness;
	__bench_fairness_metrics tMpmcBatchFairness;

	memset(&tMpscSingle, 0, sizeof(tMpscSingle));
	memset(&tMpscBatch, 0, sizeof(tMpscBatch));
	memset(&tMpmcSingle, 0, sizeof(tMpmcSingle));
	memset(&tMpmcBatch, 0, sizeof(tMpmcBatch));
	memset(&tMpmcSingleFairness, 0, sizeof(tMpmcSingleFairness));
	memset(&tMpmcBatchFairness, 0, sizeof(tMpmcBatchFairness));

	if ( iItemsPerProducer == 0u ) {
		iItemsPerProducer = 100000u;
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

	printf("xrt queue bench bench_queue_latency\n");
	printf("items_per_producer=%" PRIu32 "\n", iItemsPerProducer);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	printf("mpsc_producers=%" PRIu32 "\n", iMpscProducers);
	printf("mpmc_producers=%" PRIu32 "\n", iMpmcProducers);
	printf("mpmc_consumers=%" PRIu32 "\n", iMpmcConsumers);
	printf("batch_size=%" PRIu32 "\n", iBatchSize);

	xbenchApplyCpuPinFromEnv();

	if (
		__benchRunMPSCLatency(
			iCapacity,
			iMpscProducers,
			iItemsPerProducer,
			1u,
			&tMpscSingle
		) != 0
	) {
		fprintf(stderr, "mpsc single latency bench failed\n");
		return 2;
	}
	if (
		__benchRunMPSCLatency(
			iCapacity,
			iMpscProducers,
			iItemsPerProducer,
			iBatchSize,
			&tMpscBatch
		) != 0
	) {
		fprintf(stderr, "mpsc batch latency bench failed\n");
		return 3;
	}
	if (
		__benchRunMPMCLatency(
			iCapacity,
			iMpmcProducers,
			iMpmcConsumers,
			iItemsPerProducer,
			1u,
			&tMpmcSingle,
			&tMpmcSingleFairness
		) != 0
	) {
		fprintf(stderr, "mpmc single latency bench failed\n");
		return 4;
	}
	if (
		__benchRunMPMCLatency(
			iCapacity,
			iMpmcProducers,
			iMpmcConsumers,
			iItemsPerProducer,
			iBatchSize,
			&tMpmcBatch,
			&tMpmcBatchFairness
		) != 0
	) {
		fprintf(stderr, "mpmc batch latency bench failed\n");
		return 5;
	}

	xbenchPrintMetricU64("mpsc_latency_single_items", tMpscSingle.iItemCount);
	xbenchPrintMetricU64("mpsc_latency_single_elapsed_ns", tMpscSingle.iElapsedNs);
	xbenchPrintMetricDouble("mpsc_latency_single_avg_us", tMpscSingle.fAvgUs);
	xbenchPrintMetricDouble("mpsc_latency_single_p50_us", tMpscSingle.fP50Us);
	xbenchPrintMetricDouble("mpsc_latency_single_p95_us", tMpscSingle.fP95Us);
	xbenchPrintMetricDouble("mpsc_latency_single_p99_us", tMpscSingle.fP99Us);

	xbenchPrintMetricU64("mpsc_latency_batch_size", iBatchSize);
	xbenchPrintMetricU64("mpsc_latency_batch_items", tMpscBatch.iItemCount);
	xbenchPrintMetricU64("mpsc_latency_batch_elapsed_ns", tMpscBatch.iElapsedNs);
	xbenchPrintMetricDouble("mpsc_latency_batch_avg_us", tMpscBatch.fAvgUs);
	xbenchPrintMetricDouble("mpsc_latency_batch_p50_us", tMpscBatch.fP50Us);
	xbenchPrintMetricDouble("mpsc_latency_batch_p95_us", tMpscBatch.fP95Us);
	xbenchPrintMetricDouble("mpsc_latency_batch_p99_us", tMpscBatch.fP99Us);

	xbenchPrintMetricU64("mpmc_latency_single_items", tMpmcSingle.iItemCount);
	xbenchPrintMetricU64("mpmc_latency_single_elapsed_ns", tMpmcSingle.iElapsedNs);
	xbenchPrintMetricDouble("mpmc_latency_single_avg_us", tMpmcSingle.fAvgUs);
	xbenchPrintMetricDouble("mpmc_latency_single_p50_us", tMpmcSingle.fP50Us);
	xbenchPrintMetricDouble("mpmc_latency_single_p95_us", tMpmcSingle.fP95Us);
	xbenchPrintMetricDouble("mpmc_latency_single_p99_us", tMpmcSingle.fP99Us);
	xbenchPrintMetricU64("mpmc_latency_single_consumer_min_items", tMpmcSingleFairness.iMinItems);
	xbenchPrintMetricU64("mpmc_latency_single_consumer_max_items", tMpmcSingleFairness.iMaxItems);
	xbenchPrintMetricDouble("mpmc_latency_single_consumer_spread_pct", tMpmcSingleFairness.fSpreadPct);
	xbenchPrintMetricDouble("mpmc_latency_single_consumer_max_to_min", tMpmcSingleFairness.fMaxToMinRatio);

	xbenchPrintMetricU64("mpmc_latency_batch_size", iBatchSize);
	xbenchPrintMetricU64("mpmc_latency_batch_items", tMpmcBatch.iItemCount);
	xbenchPrintMetricU64("mpmc_latency_batch_elapsed_ns", tMpmcBatch.iElapsedNs);
	xbenchPrintMetricDouble("mpmc_latency_batch_avg_us", tMpmcBatch.fAvgUs);
	xbenchPrintMetricDouble("mpmc_latency_batch_p50_us", tMpmcBatch.fP50Us);
	xbenchPrintMetricDouble("mpmc_latency_batch_p95_us", tMpmcBatch.fP95Us);
	xbenchPrintMetricDouble("mpmc_latency_batch_p99_us", tMpmcBatch.fP99Us);
	xbenchPrintMetricU64("mpmc_latency_batch_consumer_min_items", tMpmcBatchFairness.iMinItems);
	xbenchPrintMetricU64("mpmc_latency_batch_consumer_max_items", tMpmcBatchFairness.iMaxItems);
	xbenchPrintMetricDouble("mpmc_latency_batch_consumer_spread_pct", tMpmcBatchFairness.fSpreadPct);
	xbenchPrintMetricDouble("mpmc_latency_batch_consumer_max_to_min", tMpmcBatchFairness.fMaxToMinRatio);

	return 0;
}
