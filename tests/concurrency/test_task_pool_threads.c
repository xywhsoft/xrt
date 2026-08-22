#include "../test.h"
#include "../test_thread.h"



#define TEST_TASK_PRODUCERS 8u
#define TEST_TASKS_PER_PRODUCER 100u



/* 多生产者共享计数由 mutex 保护，任务值保存在各生产者栈帧中。 */
typedef struct testtaskshared {
	xtaskpool* Pool;
	xmutex Lock;
	uint32 Completed;
} testtaskshared;



/* 每个生产者持有共享池和自己的稳定值。 */
typedef struct testtaskproducer {
	testtaskshared* Shared;
	uint32 Value;
} testtaskproducer;



/* 并发任务只更新受保护计数并返回生产者的借用值。 */
static xtaskoutcome testTaskConcurrentRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskproducer* pProducer = (testtaskproducer*)pData;

	(void)pCancel;
	(void)xrtMutexLock(&pProducer->Shared->Lock);
	pProducer->Shared->Completed++;
	(void)xrtMutexUnlock(&pProducer->Shared->Lock);
	pResult->Value = &pProducer->Value;
	return XTASK_SUCCESS;
}



/* 外部生产者并发提交、等待并释放自己的全部 Future。 */
static int testTaskProducerRun(ptr pData)
{
	testtaskproducer* pProducer = (testtaskproducer*)pData;

	for ( uint32 i = 0; i < TEST_TASKS_PER_PRODUCER; i++ ) {
		xfuture* pFuture = xrtTaskSubmit(
			pProducer->Shared->Pool,
			testTaskConcurrentRun,
			pProducer,
			NULL
		);

		if ( pFuture == NULL ) {
			return 1;
		}
		if ( xrtFutureWaitFor(pFuture, UINT64_C(5000000)) != XWAIT_OK ) {
			xrtFutureDestroy(pFuture);
			return 2;
		}
		if ( xrtFutureValue(pFuture) != &pProducer->Value ) {
			xrtFutureDestroy(pFuture);
			return 3;
		}
		xrtFutureDestroy(pFuture);
	}
	return 0;
}



/* 验证多生产者并发提交和完成统计不会丢失。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 4, 256, 0 };
	testtaskshared tShared;
	testtaskproducer arrProducer[TEST_TASK_PRODUCERS];
	testthread arrThread[TEST_TASK_PRODUCERS];
	xtaskpoolstats tStats;
	uint64 iExpected = (uint64)TEST_TASK_PRODUCERS * TEST_TASKS_PER_PRODUCER;

	memset(&tShared, 0, sizeof(tShared));
	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrThread, 0, sizeof(arrThread));
	memset(&tStats, 0, sizeof(tStats));
	testRequire(xrtMutexInit(&tShared.Lock), "task producer lock init failed");
	tShared.Pool = xrtTaskPoolCreate(&tConfig);
	testRequire(tShared.Pool != NULL, "concurrent task pool create failed");
	for ( uint32 i = 0; i < TEST_TASK_PRODUCERS; i++ ) {
		arrProducer[i].Shared = &tShared;
		arrProducer[i].Value = i + 1u;
		arrThread[i].Proc = testTaskProducerRun;
		arrThread[i].Data = &arrProducer[i];
	}
	testThreadsStart(arrThread, TEST_TASK_PRODUCERS);
	testThreadsJoin(arrThread, TEST_TASK_PRODUCERS);
	for ( uint32 i = 0; i < TEST_TASK_PRODUCERS; i++ ) {
		testRequire(arrThread[i].Result == 0, "concurrent task producer failed");
	}
	testRequire(xrtTaskPoolClose(tShared.Pool), "concurrent task pool close failed");
	testRequire(xrtTaskPoolWaitFor(tShared.Pool, UINT64_C(5000000)) == XWAIT_OK,
		"concurrent task pool drain failed");
	testRequire(xrtTaskPoolGet(tShared.Pool, &tStats), "concurrent task stats failed");
	testRequire(
		(tShared.Completed == iExpected) &&
		(tStats.Submitted == iExpected) &&
		(tStats.Completed == iExpected) &&
		(tStats.Succeeded == iExpected) &&
		(tStats.Rejected == 0),
		"concurrent task counters mismatch"
	);
	testRequire(xrtTaskPoolDestroy(tShared.Pool), "concurrent task pool destroy failed");
	testRequire(xrtMutexUnit(&tShared.Lock), "task producer lock unit failed");

	printf("[PASS] task pool threads\n");
	return 0;
}
