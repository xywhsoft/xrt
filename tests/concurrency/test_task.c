#include "../test.h"
#include "../../src/internal/xrt_task.h"



/* 任务核心测试上下文记录过程、数据和值所有权的精确生命周期。 */
typedef struct testtaskcore {
	int ProcessHits;
	int DataDestroyed;
	int ValueDestroyed;
	int ResultValue;
	bool SawCleanError;
	bool SawTempArena;
} testtaskcore;



/* 记录执行器受理后取得的任务数据所有权。 */
static void testTaskCoreDestroyData(ptr pValue, ptr pData)
{
	testtaskcore* pContext = (testtaskcore*)pData;

	(void)pValue;
	pContext->DataDestroyed++;
}



/* 记录成功结果转移给 Future 后的最终释放。 */
static void testTaskCoreDestroyValue(ptr pValue, ptr pData)
{
	testtaskcore* pContext = (testtaskcore*)pData;

	testRequire(pValue == &pContext->ResultValue,
		"task owned result value mismatch");
	pContext->ValueDestroyed++;
}



/* 成功任务验证独立错误上下文、临时 arena 和 owned 结果。 */
static xtaskoutcome testTaskCoreSuccess(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcore* pContext = (testtaskcore*)pData;

	pContext->ProcessHits++;
	pContext->SawCleanError = xrtGetError() == NULL;
	pContext->SawTempArena = xrtTempCurrent() != NULL;
	testRequire(!xrtCancelRequested(pCancel),
		"fresh task unexpectedly observed cancellation");
	testRequire(xrtTemp(32) != NULL, "task temporary allocation failed");
	pResult->Value = &pContext->ResultValue;
	pResult->Destroy = testTaskCoreDestroyValue;
	pResult->DestroyData = pContext;
	return XTASK_SUCCESS;
}



/* 失败任务把当前结构化错误移交给 Future。 */
static xtaskoutcome testTaskCoreFail(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcore* pContext = (testtaskcore*)pData;
	xerror* pError;

	(void)pCancel;
	(void)pResult;
	pContext->ProcessHits++;
	pError = xrtErrorCreate(
		XERR_PROTOCOL,
		"test.task",
		37,
		"task failed"
	);
	testRequire(pError != NULL, "task failure error create failed");
	xrtSetError(pError);
	xrtErrorFree(pError);
	return XTASK_FAILED;
}



/* 非法任务返回值必须收敛为 INTERNAL 失败。 */
static xtaskoutcome testTaskCoreInvalid(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcore* pContext = (testtaskcore*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->ProcessHits++;
	return (xtaskoutcome)99;
}



/* 预取消和内部直接终结路径不得执行用户过程。 */
static xtaskoutcome testTaskCoreMustNotRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcore* pContext = (testtaskcore*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->ProcessHits++;
	return XTASK_SUCCESS;
}



/* 创建一个带数据析构的内部任务作业。 */
static xrt_task_job* testTaskCoreCreate(
	testtaskcore* pContext,
	xtaskproc pProc,
	xfuture** ppFuture
)
{
	xtaskargs tArgs;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskCoreDestroyData;
	tArgs.DestroyData = pContext;
	return __xrtTaskCreate(pProc, pContext, &tArgs, ppFuture);
}



/* 验证成功任务的上下文隔离和值、数据两条所有权链。 */
static void testTaskCoreRunSuccess(void)
{
	testtaskcore tContext;
	xrt_task_job* pJob;
	xfuture* pFuture;
	xerror* pOuterError;

	memset(&tContext, 0, sizeof(tContext));
	tContext.ResultValue = 73;
	pOuterError = xrtErrorCreate(
		XERR_STATE,
		"test.outer",
		11,
		"outer error"
	);
	testRequire(pOuterError != NULL, "outer task error create failed");
	xrtSetError(pOuterError);
	xrtErrorFree(pOuterError);
	pJob = testTaskCoreCreate(
		&tContext,
		testTaskCoreSuccess,
		&pFuture
	);
	testRequire((pJob != NULL) && (pFuture != NULL),
		"task core success create failed");
	__xrtTaskRun(pJob);
	testRequire(
		(tContext.ProcessHits == 1) &&
		(tContext.DataDestroyed == 1) &&
		(tContext.ValueDestroyed == 0) &&
		tContext.SawCleanError &&
		tContext.SawTempArena,
		"task success execution context mismatch"
	);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pFuture) == &tContext.ResultValue),
		"task success Future mismatch"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == 11),
		"task execution did not restore caller error context"
	);
	__xrtTaskDestroy(pJob, true);
	testRequire(tContext.DataDestroyed == 1,
		"task data was destroyed more than once");
	xrtFutureDestroy(pFuture);
	testRequire(tContext.ValueDestroyed == 1,
		"task owned result destructor mismatch");
	xrtClearError();
}



/* 验证任务失败、缺省错误和非法返回值映射。 */
static void testTaskCoreRunFailures(void)
{
	testtaskcore tContext;
	xrt_task_job* pJob;
	xfuture* pFuture;

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(&tContext, testTaskCoreFail, &pFuture);
	testRequire(pJob != NULL, "task core failure create failed");
	__xrtTaskRun(pJob);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pFuture)) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtFutureError(pFuture)) == 37),
		"task failure error mapping mismatch"
	);
	testRequire(
		(tContext.ProcessHits == 1) &&
		(tContext.DataDestroyed == 1),
		"task failure lifecycle mismatch"
	);
	__xrtTaskDestroy(pJob, true);
	xrtFutureDestroy(pFuture);

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(&tContext, testTaskCoreInvalid, &pFuture);
	testRequire(pJob != NULL, "invalid task outcome create failed");
	__xrtTaskRun(pJob);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pFuture)) == XERR_INTERNAL),
		"invalid task outcome did not become INTERNAL"
	);
	testRequire(
		(tContext.ProcessHits == 1) &&
		(tContext.DataDestroyed == 1),
		"invalid task outcome lifecycle mismatch"
	);
	__xrtTaskDestroy(pJob, true);
	xrtFutureDestroy(pFuture);
}



/* 验证预取消、直接取消和执行器失败都只完成一次。 */
static void testTaskCoreRunStops(void)
{
	testtaskcore tContext;
	xrt_task_job* pJob;
	xfuture* pFuture;
	xerror* pFailure;

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(
		&tContext,
		testTaskCoreMustNotRun,
		&pFuture
	);
	testRequire(pJob != NULL, "pre-cancel task create failed");
	testRequire(xrtFutureCancel(pFuture), "pre-cancel request failed");
	__xrtTaskRun(pJob);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		(tContext.ProcessHits == 0) &&
		(tContext.DataDestroyed == 1),
		"pre-cancel task execution mismatch"
	);
	__xrtTaskDestroy(pJob, true);
	xrtFutureDestroy(pFuture);

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(
		&tContext,
		testTaskCoreMustNotRun,
		&pFuture
	);
	testRequire(pJob != NULL, "direct-cancel task create failed");
	__xrtTaskCancel(pJob);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		(tContext.ProcessHits == 0) &&
		(tContext.DataDestroyed == 1),
		"direct-cancel task execution mismatch"
	);
	__xrtTaskDestroy(pJob, true);
	xrtFutureDestroy(pFuture);

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(
		&tContext,
		testTaskCoreMustNotRun,
		&pFuture
	);
	pFailure = xrtErrorCreate(
		XERR_CLOSED,
		"test.executor",
		19,
		"executor closed"
	);
	testRequire((pJob != NULL) && (pFailure != NULL),
		"executor failure task setup failed");
	__xrtTaskFail(pJob, pFailure);
	xrtErrorFree(pFailure);
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pFuture)) == XERR_CLOSED) &&
		(xrtErrorCode(xrtFutureError(pFuture)) == 19) &&
		(tContext.ProcessHits == 0) &&
		(tContext.DataDestroyed == 1),
		"executor task failure mismatch"
	);
	__xrtTaskDestroy(pJob, true);
	xrtFutureDestroy(pFuture);
}



/* 验证提交失败回滚不会取得调用方任务数据所有权。 */
static void testTaskCoreRejectOwnership(void)
{
	testtaskcore tContext;
	xrt_task_job* pJob;
	xfuture* pFuture;

	memset(&tContext, 0, sizeof(tContext));
	pJob = testTaskCoreCreate(
		&tContext,
		testTaskCoreMustNotRun,
		&pFuture
	);
	testRequire((pJob != NULL) && (pFuture != NULL),
		"rejected task setup failed");
	__xrtTaskDestroy(pJob, false);
	testRequire(
		(tContext.ProcessHits == 0) &&
		(tContext.DataDestroyed == 0),
		"rejected task consumed caller data"
	);
}



/* 验证任务核心状态机及执行器共享的所有权合同。 */
int main(void)
{
	testTaskCoreRunSuccess();
	testTaskCoreRunFailures();
	testTaskCoreRunStops();
	testTaskCoreRejectOwnership();

	printf("[PASS] task core\n");
	return 0;
}
