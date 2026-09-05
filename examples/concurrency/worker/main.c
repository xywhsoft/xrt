#include <stdio.h>

#include <xrt.h>



#define WORKER_PRODUCER_COUNT 2u
#define WORKER_JOB_COUNT 3u



/* 保存一次提交、对应 Promise 和 worker 写入的结果。 */
typedef struct workerjob {
	uint32 Producer;
	uint32 Job;
	int32 Input;
	int32 Output;
	xpromise* Promise;
} workerjob;



/* 保存一个生产者独占的提交区间。 */
typedef struct producercontext {
	xchannel* Channel;
	uint32 Producer;
	int32 Base;
	xfuture** Futures;
} producercontext;



/* 在 Future 最后释放 owned 结果时销毁任务对象。 */
static void workerJobFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 关闭尚未交给 worker 的任务，并释放生产端资源。 */
static void workerJobClose(workerjob* pJob)
{
	if ( pJob == NULL ) {
		return;
	}
	(void)xrtPromiseClose(pJob->Promise);
	xrtPromiseDestroy(pJob->Promise);
	xrtFree(pJob);
}



/* 在 worker 异常退出时关闭仍留在 Channel 中的任务。 */
static void workerJobDrain(ptr pItem, ptr pData)
{
	(void)pData;
	workerJobClose((workerjob*)pItem);
}



/* 串行消费任务，完成每个任务自己的 Future，直到 Channel 关闭并排空。 */
static int32 workerRun(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	ptr pItem = NULL;
	xwaitresult eWait;

	for ( ;; ) {
		eWait = xrtChannelRecv(pChannel, &pItem);
		if ( eWait == XWAIT_CLOSED ) {
			return 0;
		}
		if ( eWait != XWAIT_OK ) {
			return 1;
		}

		/* Future 取消只请求停止，由真正的生产端确认取消终态。 */
		workerjob* pJob = (workerjob*)pItem;
		xpromise* pPromise = pJob->Promise;
		xcancel* pCancel = xrtPromiseCancelToken(pPromise);

		if ( (pCancel != NULL) && xrtCancelRequested(pCancel) ) {
			(void)xrtPromiseCancel(pPromise);
			xrtPromiseDestroy(pPromise);
			xrtCancelDestroy(pCancel);
			xrtFree(pJob);
			continue;
		}
		xrtCancelDestroy(pCancel);

		pJob->Output = pJob->Input * pJob->Input;
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pJob,
			workerJobFree,
			NULL
		) ) {
			xrtPromiseDestroy(pPromise);
			xrtFree(pJob);
			continue;
		}
		/* 发布成功后不能再访问可能被 Future 消费者销毁的 pJob。 */
		xrtPromiseDestroy(pPromise);
	}
}



/* 创建任务和 Future，通过有界 Channel 把任务所有权交给 worker。 */
static int32 producerRun(ptr pData)
{
	producercontext* pContext = (producercontext*)pData;

	for ( uint32 i = 0; i < WORKER_JOB_COUNT; i++ ) {
		xfuture* pFuture = NULL;
		xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
		workerjob* pJob = NULL;

		if ( pPromise == NULL ) {
			return 1;
		}
		pContext->Futures[i] = pFuture;
		pJob = (workerjob*)xrtMalloc(sizeof(workerjob));
		if ( pJob == NULL ) {
			(void)xrtPromiseClose(pPromise);
			xrtPromiseDestroy(pPromise);
			return 2;
		}

		pJob->Producer = pContext->Producer;
		pJob->Job = i + 1u;
		pJob->Input = pContext->Base + (int32)i + 1;
		pJob->Output = 0;
		pJob->Promise = pPromise;

		/* Send 成功后 worker 拥有任务；失败时仍由生产者关闭。 */
		if ( xrtChannelSendFor(
			pContext->Channel, pJob, UINT64_C(2000000)
		) != XWAIT_OK ) {
			workerJobClose(pJob);
			return 3;
		}
	}
	return 0;
}



/*
 * 范例：concurrency/worker —— 专用 Worker：多生产者单消费者
 * ----------------------------------------------------------------
 * 演示 API：
 *   多生产者 → 单 worker 的任务投递
 *   关闭 / 排空 / 逐任务结果验证
 * 模块宏：XRT_MODULE_EXECUTOR
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/worker/main.c -lws2_32 -liphlpapi
 * 预期输出：（逐任务结果，见运行）
 *
 * 单消费者模式：所有任务亲和同一 worker——
 *   状态无锁（只有它会碰）、顺序可控；
 *   关闭后排空保证已提交任务不丢。
 */


/* 运行多生产者、单 worker，并验证关闭、排空和逐任务结果。 */
int main(void)
{
	xchannel* pChannel = NULL;
	xthread* pWorker = NULL;
	xthread* arrProducer[WORKER_PRODUCER_COUNT] = { NULL, NULL };
	producercontext arrContext[WORKER_PRODUCER_COUNT] = { 0 };
	xfuture* arrFuture[WORKER_PRODUCER_COUNT][WORKER_JOB_COUNT] = { 0 };
	uint32 iProducerStarted = 0;
	bool bProducerOk = true;
	bool bWorkerOk = false;
	bool bResultsOk = true;
	int iResult = 1;

	pChannel = xrtChannelCreate(4u);
	if ( pChannel == NULL ) {
		goto cleanup;
	}
	pWorker = xrtThreadCreate(workerRun, pChannel, 0);
	if ( pWorker == NULL ) {
		goto cleanup;
	}

	/* 每个生产者只写自己的 Future 行，不需要共享结果锁。 */
	for ( uint32 i = 0; i < WORKER_PRODUCER_COUNT; i++ ) {
		arrContext[i].Channel = pChannel;
		arrContext[i].Producer = i + 1u;
		arrContext[i].Base = (int32)i * 100;
		arrContext[i].Futures = arrFuture[i];
		arrProducer[i] = xrtThreadCreate(producerRun, &arrContext[i], 0);
		if ( arrProducer[i] == NULL ) {
			bProducerOk = false;
			break;
		}
		iProducerStarted++;
	}

	/* 先停止生产，再关闭输入；worker 会继续排空已提交任务。 */
	for ( uint32 i = 0; i < iProducerStarted; i++ ) {
		if ( (xrtThreadWait(arrProducer[i]) != XWAIT_OK) ||
			(xrtThreadExitCode(arrProducer[i]) != 0) ) {
			bProducerOk = false;
		}
		xrtThreadDestroy(arrProducer[i]);
		arrProducer[i] = NULL;
	}
	xrtChannelClose(pChannel);
	if ( xrtThreadWait(pWorker) == XWAIT_OK ) {
		bWorkerOk = (xrtThreadExitCode(pWorker) == 0);
	}
	(void)xrtChannelDrain(pChannel, workerJobDrain, NULL);

	/* Future 自己拥有任务结果，调用方只读取并最终释放 Future。 */
	for ( uint32 i = 0; i < WORKER_PRODUCER_COUNT; i++ ) {
		for ( uint32 j = 0; j < WORKER_JOB_COUNT; j++ ) {
			xfuture* pFuture = arrFuture[i][j];
			workerjob* pJob = NULL;

			if ( (pFuture == NULL) ||
				(xrtFutureWaitFor(pFuture, UINT64_C(2000000)) != XWAIT_OK) ||
				(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
				bResultsOk = false;
				continue;
			}
			pJob = (workerjob*)xrtFutureValue(pFuture);
			if ( (pJob == NULL) ||
				(pJob->Output != (pJob->Input * pJob->Input)) ) {
				bResultsOk = false;
				continue;
			}
			printf(
				"producer=%u job=%u input=%d output=%d\n",
				(unsigned)pJob->Producer,
				(unsigned)pJob->Job,
				(int)pJob->Input,
				(int)pJob->Output
			);
		}
	}
	if ( bProducerOk && bWorkerOk && bResultsOk &&
		(iProducerStarted == WORKER_PRODUCER_COUNT) ) {
		iResult = 0;
	}

cleanup:
	/* 所有线程结束后再释放 Future、worker 和 Channel。 */
	if ( pChannel != NULL ) {
		xrtChannelClose(pChannel);
	}
	for ( uint32 i = 0; i < iProducerStarted; i++ ) {
		if ( arrProducer[i] != NULL ) {
			(void)xrtThreadWait(arrProducer[i]);
			xrtThreadDestroy(arrProducer[i]);
		}
	}
	if ( pWorker != NULL ) {
		(void)xrtThreadWait(pWorker);
		xrtThreadDestroy(pWorker);
	}
	if ( pChannel != NULL ) {
		(void)xrtChannelDrain(pChannel, workerJobDrain, NULL);
	}
	for ( uint32 i = 0; i < WORKER_PRODUCER_COUNT; i++ ) {
		for ( uint32 j = 0; j < WORKER_JOB_COUNT; j++ ) {
			xrtFutureDestroy(arrFuture[i][j]);
		}
	}
	(void)xrtChannelDestroy(pChannel);
	return iResult;
}
