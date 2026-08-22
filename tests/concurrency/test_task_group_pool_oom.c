#include "../test.h"



/* 单工作线程夹具让被测任务停留在队列中，隔离同步提交阶段的分配故障。 */
typedef struct testtaskgrouppooloom {
	xmutex Lock;
	xcond Changed;
	bool Running;
	bool Release;
	int CandidateRuns;
	int Destroyed;
} testtaskgrouppooloom;



/* 初始化每轮扫描独立使用的同步状态。 */
static void testTaskGroupPoolOomInit(testtaskgrouppooloom* pContext)
{
	memset(pContext, 0, sizeof(testtaskgrouppooloom));
	testRequire(xrtMutexInit(&pContext->Lock),
		"task group pool OOM lock init failed");
	testRequire(xrtCondInit(&pContext->Changed),
		"task group pool OOM condition init failed");
}



/* 释放每轮扫描建立的同步原语。 */
static void testTaskGroupPoolOomUnit(testtaskgrouppooloom* pContext)
{
	testRequire(xrtCondUnit(&pContext->Changed),
		"task group pool OOM condition unit failed");
	testRequire(xrtMutexUnit(&pContext->Lock),
		"task group pool OOM lock unit failed");
}



/* 首个任务占住唯一工作线程，直到故障注入已经解除。 */
static xtaskoutcome testTaskGroupPoolOomBlock(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgrouppooloom* pContext = (testtaskgrouppooloom*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Running = true;
	(void)xrtCondBroadcast(&pContext->Changed);
	while ( !pContext->Release ) {
		(void)xrtCondWait(&pContext->Changed, &pContext->Lock);
	}
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 被测任务只有在提交完整成功后才允许执行。 */
static xtaskoutcome testTaskGroupPoolOomCandidate(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgrouppooloom* pContext = (testtaskgrouppooloom*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->CandidateRuns++;
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 受理后的任务数据必须由任务生命周期恰好回收一次。 */
static void testTaskGroupPoolOomDestroy(ptr pValue, ptr pData)
{
	testtaskgrouppooloom* pContext = (testtaskgrouppooloom*)pData;

	(void)pValue;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Destroyed++;
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 等待阻塞任务确实占用工作线程，避免故障注入进入异步执行阶段。 */
static bool testTaskGroupPoolOomWaitRunning(
	testtaskgrouppooloom* pContext
)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));
	bool bRunning;

	(void)xrtMutexLock(&pContext->Lock);
	while ( !pContext->Running && !xrtDeadlineExpired(iDeadline) ) {
		(void)xrtCondWaitUntil(
			&pContext->Changed,
			&pContext->Lock,
			iDeadline
		);
	}
	bRunning = pContext->Running;
	(void)xrtMutexUnlock(&pContext->Lock);
	return bRunning;
}



/* 解除唯一工作线程并允许成功受理的候选任务执行。 */
static void testTaskGroupPoolOomRelease(testtaskgrouppooloom* pContext)
{
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Release = true;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 扫描组合取消、监听、组预留及池提交的全部同步分配点。 */
int main(void)
{
	bool bComplete = false;
	size_t iFailures = 0;

	for ( size_t i = 0; i < 32u; i++ ) {
		testtaskgrouppooloom tContext;
		xtaskpoolconfig tPoolConfig = { 1u, 1u, 0u };
		xtaskargs tArgs;
		xtaskgroupstats tStats;
		xtaskpool* pPool;
		xtaskgroup* pGroup;
		xfuture* pBlock;
		xfuture* pCandidate;
		xerrkind Error;
		bool bTriggered;

		testTaskGroupPoolOomInit(&tContext);
		pPool = xrtTaskPoolCreate(&tPoolConfig);
		pGroup = xrtTaskGroupCreate(NULL);
		testRequire((pPool != NULL) && (pGroup != NULL),
			"task group pool OOM fixture create failed");
		pBlock = xrtTaskSubmit(
			pPool,
			testTaskGroupPoolOomBlock,
			&tContext,
			NULL
		);
		testRequire((pBlock != NULL) &&
			testTaskGroupPoolOomWaitRunning(&tContext),
			"task group pool OOM blocker did not start");

		memset(&tArgs, 0, sizeof(tArgs));
		tArgs.Destroy = testTaskGroupPoolOomDestroy;
		tArgs.DestroyData = &tContext;
		testRequire(xrtMemDebugFailAfter((uint64)i),
			"task group pool OOM injection setup failed");
		xrtClearError();
		pCandidate = xrtTaskGroupSubmitUntilCancel(
			pGroup,
			pPool,
			testTaskGroupPoolOomCandidate,
			&tContext,
			&tArgs,
			XRT_DEADLINE_NEVER,
			NULL
		);
		bTriggered = xrtMemDebugFailTriggered();
		Error = xrtErrorKind(xrtGetError());
		xrtMemDebugFailClear();

		if ( pCandidate == NULL ) {
			testRequire(bTriggered && (Error == XERR_MEMORY),
				"task group pool failed without injected OOM");
			testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
				(tStats.Active == 0) &&
				(tContext.CandidateRuns == 0) &&
				(tContext.Destroyed == 0),
				"task group pool OOM consumed ownership or left a reservation");
			iFailures++;
		} else {
			testRequire(!bTriggered,
				"task group pool submission ignored injected OOM");
			bComplete = true;
		}

		testTaskGroupPoolOomRelease(&tContext);
		testRequire(xrtFutureWait(pBlock) == XWAIT_OK,
			"task group pool OOM blocker wait failed");
		xrtFutureDestroy(pBlock);
		if ( pCandidate != NULL ) {
			testRequire(xrtFutureWait(pCandidate) == XWAIT_OK,
				"task group pool OOM candidate wait failed");
			xrtFutureDestroy(pCandidate);
		}
		testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
			"task group pool OOM group drain failed");
		xrtTaskGroupDestroy(pGroup);
		testRequire(xrtTaskPoolClose(pPool) &&
			(xrtTaskPoolWait(pPool) == XWAIT_OK) &&
			xrtTaskPoolDestroy(pPool),
			"task group pool OOM pool cleanup failed");
		testRequire(tContext.CandidateRuns == tContext.Destroyed,
			"task group pool OOM accepted ownership count mismatch");
		testTaskGroupPoolOomUnit(&tContext);
		xrtClearError();
		testMemoryDebugDrain(
			"task group pool OOM sweep leaked storage"
		);
		if ( bComplete ) {
			break;
		}
	}
	testRequire(bComplete && (iFailures != 0),
		"task group pool OOM sweep did not converge");

	puts("[PASS] task group pool OOM");
	return 0;
}
