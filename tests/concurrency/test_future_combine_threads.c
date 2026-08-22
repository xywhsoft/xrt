#include "../test.h"
#include "../test_thread.h"
#include "../test_thread_barrier.h"



/* 一个压力工作槽执行 Promise 完成或组合 Future 取消。 */
typedef struct testfuturecombineworker {
	testthreadbarrier* Barrier;
	xpromise* Promise;
	xfuture* Future;
	ptr Value;
	bool Cancel;
} testfuturecombineworker;



/* 到达屏障后执行一个并发完成或取消动作。 */
static int testFutureCombineWorker(ptr pData)
{
	testfuturecombineworker* pWorker = (testfuturecombineworker*)pData;

	if ( !testThreadBarrierWait(pWorker->Barrier) ) {
		return 1;
	}
	if ( pWorker->Cancel ) {
		(void)xrtFutureCancel(pWorker->Future);
		return 0;
	}
	return xrtPromiseResolve(pWorker->Promise, pWorker->Value) ? 0 : 2;
}



/* 反复竞争两个 Race 源，验证唯一选择、败者请求和上下文回收。 */
static void testFutureCombineRaceStress(void)
{
	for ( size_t iRound = 0; iRound < 100; iRound++ ) {
		testthreadbarrier tBarrier;
		testfuturecombineworker arrWorker[2];
		testthread arrThread[2];
		xfuture* arrSource[2];
		xpromise* arrPromise[2];
		xfuture* pRace;
		const xfuturepick* pPick;
		int arrValue[2] = { 71, 72 };

		arrPromise[0] = xrtPromiseCreate(&arrSource[0], NULL);
		arrPromise[1] = xrtPromiseCreate(&arrSource[1], NULL);
		testRequire((arrPromise[0] != NULL) && (arrPromise[1] != NULL),
			"future Race stress source create failed");
		pRace = xrtFutureRace(arrSource, 2);
		testRequire(pRace != NULL, "future Race stress create failed");
		testThreadBarrierInit(&tBarrier, 2);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		for ( size_t i = 0; i < 2; i++ ) {
			arrWorker[i].Barrier = &tBarrier;
			arrWorker[i].Promise = arrPromise[i];
			arrWorker[i].Value = &arrValue[i];
			arrThread[i].Proc = testFutureCombineWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		testThreadsStart(arrThread, 2);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(arrThread, 2);
		testRequire((arrThread[0].Result == 0) &&
			(arrThread[1].Result == 0), "future Race stress worker failed");
		testRequire(xrtFutureWaitFor(pRace, UINT64_C(2000000)) == XWAIT_OK,
			"future Race stress wait failed");
		pPick = (const xfuturepick*)xrtFutureValue(pRace);
		testRequire((pPick != NULL) && (pPick->Index < 2) &&
			(pPick->Future == arrSource[pPick->Index]),
			"future Race stress selection corrupted");

		testThreadBarrierUnit(&tBarrier);
		xrtFutureDestroy(pRace);
		for ( size_t i = 0; i < 2; i++ ) {
			xrtPromiseDestroy(arrPromise[i]);
			xrtFutureDestroy(arrSource[i]);
		}
	}
}



/* 反复竞争源完成和输出取消，验证两种合法终态均不破坏源对象。 */
static void testFutureCombineCancelStress(void)
{
	for ( size_t iRound = 0; iRound < 100; iRound++ ) {
		testthreadbarrier tBarrier;
		testfuturecombineworker arrWorker[2];
		testthread arrThread[2];
		xfuture* arrSource[2];
		xpromise* arrPromise[2];
		xfuture* pAny;
		xfuturestate State;
		int iValue = 81;

		arrPromise[0] = xrtPromiseCreate(&arrSource[0], NULL);
		arrPromise[1] = xrtPromiseCreate(&arrSource[1], NULL);
		testRequire((arrPromise[0] != NULL) && (arrPromise[1] != NULL),
			"future cancel stress source create failed");
		pAny = xrtFutureAny(arrSource, 2);
		testRequire(pAny != NULL, "future cancel stress Any create failed");
		testThreadBarrierInit(&tBarrier, 2);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		arrWorker[0].Barrier = &tBarrier;
		arrWorker[0].Promise = arrPromise[0];
		arrWorker[0].Value = &iValue;
		arrWorker[1].Barrier = &tBarrier;
		arrWorker[1].Future = pAny;
		arrWorker[1].Cancel = true;
		for ( size_t i = 0; i < 2; i++ ) {
			arrThread[i].Proc = testFutureCombineWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		testThreadsStart(arrThread, 2);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(arrThread, 2);
		testRequire((arrThread[0].Result == 0) &&
			(arrThread[1].Result == 0), "future cancel stress worker failed");
		testRequire(xrtFutureWaitFor(pAny, UINT64_C(2000000)) == XWAIT_OK,
			"future cancel stress wait failed");
		State = xrtFutureState(pAny);
		testRequire((State == XFUTURE_RESOLVED) ||
			(State == XFUTURE_CANCELLED),
			"future cancel stress terminal state mismatch");
		if ( State == XFUTURE_RESOLVED ) {
			const xfuturepick* pPick =
				(const xfuturepick*)xrtFutureValue(pAny);

			testRequire((pPick != NULL) && (pPick->Index == 0) &&
				(pPick->Future == arrSource[0]),
				"future cancel stress selection corrupted");
		}
		testRequire(xrtPromiseResolve(arrPromise[1], NULL),
			"future cancel stress second source resolve failed");

		testThreadBarrierUnit(&tBarrier);
		xrtFutureDestroy(pAny);
		for ( size_t i = 0; i < 2; i++ ) {
			xrtPromiseDestroy(arrPromise[i]);
			xrtFutureDestroy(arrSource[i]);
		}
	}
}



/* 并发完成全部输入，验证 All 只在最后一个终态后按原顺序发布。 */
static void testFutureCombineAllStress(void)
{
	enum { TEST_FUTURE_COMBINE_ALL_COUNT = 8 };

	for ( size_t iRound = 0; iRound < 50; iRound++ ) {
		testthreadbarrier tBarrier;
		testfuturecombineworker arrWorker[TEST_FUTURE_COMBINE_ALL_COUNT];
		testthread arrThread[TEST_FUTURE_COMBINE_ALL_COUNT];
		xfuture* arrSource[TEST_FUTURE_COMBINE_ALL_COUNT];
		xpromise* arrPromise[TEST_FUTURE_COMBINE_ALL_COUNT];
		int arrValue[TEST_FUTURE_COMBINE_ALL_COUNT];
		xfuture* pAll;
		const xfutureall* pResult;

		for ( size_t i = 0; i < TEST_FUTURE_COMBINE_ALL_COUNT; i++ ) {
			arrValue[i] = (int)((iRound * 100) + i);
			arrPromise[i] = xrtPromiseCreate(&arrSource[i], NULL);
			testRequire(arrPromise[i] != NULL,
				"future All stress source create failed");
		}
		pAll = xrtFutureAll(
			arrSource,
			TEST_FUTURE_COMBINE_ALL_COUNT
		);
		testRequire(pAll != NULL, "future All stress create failed");
		testThreadBarrierInit(
			&tBarrier,
			TEST_FUTURE_COMBINE_ALL_COUNT
		);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		for ( size_t i = 0; i < TEST_FUTURE_COMBINE_ALL_COUNT; i++ ) {
			arrWorker[i].Barrier = &tBarrier;
			arrWorker[i].Promise = arrPromise[i];
			arrWorker[i].Value = &arrValue[i];
			arrThread[i].Proc = testFutureCombineWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		testThreadsStart(
			arrThread,
			TEST_FUTURE_COMBINE_ALL_COUNT
		);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(
			arrThread,
			TEST_FUTURE_COMBINE_ALL_COUNT
		);
		testRequire(xrtFutureWaitFor(
			pAll,
			UINT64_C(2000000)
		) == XWAIT_OK, "future All stress wait failed");
		pResult = (const xfutureall*)xrtFutureValue(pAll);
		testRequire((pResult != NULL) &&
			(pResult->Count == TEST_FUTURE_COMBINE_ALL_COUNT),
			"future All stress result count mismatch");
		for ( size_t i = 0; i < TEST_FUTURE_COMBINE_ALL_COUNT; i++ ) {
			testRequire((arrThread[i].Result == 0) &&
				(pResult->Futures[i] == arrSource[i]) &&
				(xrtFutureValue(pResult->Futures[i]) == &arrValue[i]),
				"future All stress ordered result corrupted");
		}

		testThreadBarrierUnit(&tBarrier);
		xrtFutureDestroy(pAll);
		for ( size_t i = 0; i < TEST_FUTURE_COMBINE_ALL_COUNT; i++ ) {
			xrtPromiseDestroy(arrPromise[i]);
			xrtFutureDestroy(arrSource[i]);
		}
	}
}



/* 覆盖组合完成、取消、监听摘除和释放之间的并发竞争。 */
int main(void)
{
	testFutureCombineRaceStress();
	testFutureCombineCancelStress();
	testFutureCombineAllStress();
	printf("[PASS] future combine threads\n");
	return 0;
}
