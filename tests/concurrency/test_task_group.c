#include "../test.h"



/* 创建一个独立 Future/Promise 对并压实测试前置条件。 */
static xpromise* testTaskGroupPair(xfuture** ppFuture)
{
	xpromise* pPromise = xrtPromiseCreate(ppFuture, NULL);

	testRequire((pPromise != NULL) && (*ppFuture != NULL),
		"task group source create failed");
	return pPromise;
}



/* 释放一个测试源的两个调用方端点。 */
static void testTaskGroupPairDestroy(
	xpromise* pPromise,
	xfuture* pFuture
)
{
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 通用启动器测试记录调用次数并保存新建 Promise 端点。 */
typedef struct testtaskgroupstart {
	xpromise* Promise;
	xfuture* Future;
	int Calls;
	bool Fail;
} testtaskgroupstart;



/* 同步创建一个 Future，或返回带结构化错误的启动失败。 */
static xfuture* testTaskGroupStartProc(ptr pData)
{
	testtaskgroupstart* pContext = (testtaskgroupstart*)pData;

	pContext->Calls++;
	if ( pContext->Fail ) {
		xerror* pError = xrtErrorCreate(
			XERR_PROTOCOL,
			"test.task.group.start",
			41,
			"start failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	pContext->Promise = xrtPromiseCreate(&pContext->Future, NULL);
	return pContext->Promise != NULL ? pContext->Future : NULL;
}



/* 回收启动器成功创建的 Promise 和调用方 Future 引用。 */
static void testTaskGroupStartDestroy(testtaskgroupstart* pContext)
{
	xrtPromiseDestroy(pContext->Promise);
	xrtFutureDestroy(pContext->Future);
	pContext->Promise = NULL;
	pContext->Future = NULL;
}



/* 验证预留式启动的成功、关闭、限流和启动失败回滚。 */
static void testTaskGroupStart(void)
{
	testtaskgroupstart tFirst;
	testtaskgroupstart tSecond;
	xtaskgroupconfig tConfig = { NULL, 1, 0 };
	xtaskgroupstats tStats;
	xtaskgroup* pGroup;
	xfuture* pFuture;

	memset(&tFirst, 0, sizeof(tFirst));
	memset(&tSecond, 0, sizeof(tSecond));
	memset(&tStats, 0, sizeof(tStats));
	pGroup = xrtTaskGroupCreate(&tConfig);
	testRequire(pGroup != NULL, "task group start create failed");
	pFuture = xrtTaskGroupStart(pGroup, testTaskGroupStartProc, &tFirst);
	testRequire((pFuture != NULL) && (pFuture == tFirst.Future) &&
		(tFirst.Calls == 1), "task group start success mismatch");
	testRequire(xrtTaskGroupStart(
		pGroup,
		testTaskGroupStartProc,
		&tSecond
	) == NULL, "task group start exceeded active limit");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(tSecond.Calls == 0), "limited start invoked callback");
	testRequire(xrtPromiseResolve(tFirst.Promise, NULL),
		"task group started source resolve failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"task group started source wait failed");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Active == 0) && (tStats.Added == 1) &&
		(tStats.Completed == 1) && (tStats.Rejected == 1),
		"task group start stats mismatch");
	xrtClearError();
	testRequire(xrtTaskGroupStart(
		pGroup,
		testTaskGroupStartProc,
		&tSecond
	) == NULL, "closed task group started source");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(tSecond.Calls == 0), "closed start invoked callback");
	testTaskGroupStartDestroy(&tFirst);
	xrtTaskGroupDestroy(pGroup);

	memset(&tFirst, 0, sizeof(tFirst));
	memset(&tStats, 0, sizeof(tStats));
	tFirst.Fail = true;
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire(pGroup != NULL, "task group failed start create failed");
	testRequire(xrtTaskGroupStart(
		pGroup,
		testTaskGroupStartProc,
		&tFirst
	) == NULL, "failing task group start succeeded");
	testRequire((tFirst.Calls == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"task group start error was not preserved");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"failed start left task group active");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Active == 0) && (tStats.Added == 0) &&
		(tStats.Completed == 0) && (tStats.Rejected == 1),
		"failed start rollback stats mismatch");
	xrtTaskGroupDestroy(pGroup);
}



/* 验证关闭屏障、累计状态、首个异常和源引用生命周期。 */
static void testTaskGroupBasic(void)
{
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pDone;
	xfuture* arrFuture[3];
	xpromise* arrPromise[3];
	xtaskgroupstats tStats;
	xerror* pError;
	int iValue = 19;

	testRequire(pGroup != NULL, "task group create failed");
	pDone = xrtTaskGroupFuture(pGroup);
	testRequire((pDone != NULL) &&
		(xrtFutureState(pDone) == XFUTURE_PENDING),
		"new task group Done Future mismatch");
	for ( size_t i = 0; i < 3; i++ ) {
		arrPromise[i] = testTaskGroupPair(&arrFuture[i]);
		testRequire(xrtTaskGroupAdd(pGroup, arrFuture[i]),
			"task group source add failed");
	}
	pError = xrtErrorCreate(
		XERR_PROTOCOL,
		"test.task.group",
		23,
		"group child failed"
	);
	testRequire(pError != NULL, "task group source error create failed");
	testRequire(xrtPromiseResolve(arrPromise[0], &iValue),
		"task group success complete failed");
	testRequire(xrtPromiseReject(arrPromise[1], pError),
		"task group failure complete failed");
	xrtErrorFree(pError);
	testRequire(xrtPromiseClose(arrPromise[2]),
		"task group closed source complete failed");
	testRequire(xrtFutureState(pDone) == XFUTURE_PENDING,
		"open task group completed before Close");
	testRequire(xrtTaskGroupClose(pGroup), "task group close failed");
	testRequire(xrtFutureWait(pDone) == XWAIT_OK,
		"task group Done wait failed");
	testRequire(xrtTaskGroupGet(pGroup, &tStats),
		"task group stats failed");
	testRequire((tStats.Active == 0) && (tStats.Added == 3) &&
		(tStats.Completed == 3) && (tStats.Succeeded == 1) &&
		(tStats.Failed == 1) && (tStats.Closed == 1) &&
		(tStats.Cancelled == 0), "task group terminal stats mismatch");
	testRequire((tStats.FirstIndex == 1) &&
		(tStats.FirstState == XFUTURE_FAILED) && !tStats.Accepting,
		"task group first abnormal result mismatch");
	testRequire(xrtErrorKind(xrtTaskGroupError(pGroup)) == XERR_PROTOCOL,
		"task group first structured error mismatch");
	xrtClearError();
	testRequire(!xrtTaskGroupAdd(pGroup, arrFuture[0]),
		"closed task group accepted a source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"closed task group rejection mismatch");

	xrtFutureDestroy(pDone);
	for ( size_t i = 0; i < 3; i++ ) {
		testTaskGroupPairDestroy(arrPromise[i], arrFuture[i]);
	}
	xrtTaskGroupDestroy(pGroup);
}



/* 验证活动上限只限制并发项，已完成项不会形成历史内存负担。 */
static void testTaskGroupLimit(void)
{
	xtaskgroupconfig tConfig = { NULL, 1, 0 };
	xtaskgroup* pGroup = xrtTaskGroupCreate(&tConfig);
	xfuture* pA;
	xfuture* pB;
	xpromise* pPromiseA = testTaskGroupPair(&pA);
	xpromise* pPromiseB = testTaskGroupPair(&pB);
	xtaskgroupstats tStats;

	testRequire(pGroup != NULL, "limited task group create failed");
	testRequire(xrtTaskGroupAdd(pGroup, pA),
		"limited task group first add failed");
	xrtClearError();
	testRequire(!xrtTaskGroupAdd(pGroup, pB),
		"limited task group exceeded active limit");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"limited task group error mismatch");
	testRequire(xrtPromiseResolve(pPromiseA, NULL),
		"limited task group first complete failed");
	testRequire(xrtTaskGroupAdd(pGroup, pB),
		"limited task group did not reclaim completed slot");
	testRequire(xrtPromiseResolve(pPromiseB, NULL),
		"limited task group second complete failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"limited task group wait failed");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Added == 2) && (tStats.Rejected == 1),
		"limited task group counters mismatch");

	testTaskGroupPairDestroy(pPromiseA, pA);
	testTaskGroupPairDestroy(pPromiseB, pB);
	xrtTaskGroupDestroy(pGroup);
}



/* 验证组取消只请求源停止，源生产端仍确认各自真实终态。 */
static void testTaskGroupCancel(void)
{
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pDone;
	xfuture* pA;
	xfuture* pB;
	xpromise* pPromiseA = testTaskGroupPair(&pA);
	xpromise* pPromiseB = testTaskGroupPair(&pB);
	xcancel* pCancelA = xrtFutureCancelToken(pA);
	xcancel* pCancelB = xrtFutureCancelToken(pB);
	xtaskgroupstats tStats;

	testRequire((pGroup != NULL) && (pCancelA != NULL) &&
		(pCancelB != NULL), "task group cancel setup failed");
	testRequire(xrtTaskGroupAdd(pGroup, pA) &&
		xrtTaskGroupAdd(pGroup, pB), "task group cancel add failed");
	pDone = xrtTaskGroupFuture(pGroup);
	testRequire(xrtTaskGroupCancel(pGroup), "task group cancel failed");
	testRequire(xrtCancelRequested(pCancelA) &&
		xrtCancelRequested(pCancelB),
		"task group cancel did not reach sources");
	testRequire((xrtFutureState(pA) == XFUTURE_PENDING) &&
		(xrtFutureState(pB) == XFUTURE_PENDING),
		"task group cancel forged source terminal state");
	testRequire(xrtFutureState(pDone) == XFUTURE_PENDING,
		"task group cancel completed before sources stopped");
	testRequire(xrtPromiseResolve(pPromiseA, NULL) &&
		xrtPromiseCancel(pPromiseB),
		"task group source cancellation outcomes failed");
	testRequire(xrtFutureWait(pDone) == XWAIT_OK,
		"cancelled task group Done wait failed");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		tStats.Cancelling && (tStats.Succeeded == 1) &&
		(tStats.Cancelled == 1), "cancelled task group stats mismatch");

	xrtFutureDestroy(pDone);
	xrtCancelDestroy(pCancelA);
	xrtCancelDestroy(pCancelB);
	testTaskGroupPairDestroy(pPromiseA, pA);
	testTaskGroupPairDestroy(pPromiseB, pB);
	xrtTaskGroupDestroy(pGroup);
}



/* 验证失败即取消策略关闭组并请求仍活动的兄弟项。 */
static void testTaskGroupFailFast(void)
{
	xtaskgroupconfig tConfig = {
		NULL,
		0,
		XRT_TASK_GROUP_CANCEL_ON_FAILED
	};
	xtaskgroup* pGroup = xrtTaskGroupCreate(&tConfig);
	xfuture* pA;
	xfuture* pB;
	xpromise* pPromiseA = testTaskGroupPair(&pA);
	xpromise* pPromiseB = testTaskGroupPair(&pB);
	xcancel* pCancelB = xrtFutureCancelToken(pB);
	xerror* pError = xrtErrorCreate(
		XERR_IO,
		"test.task.group",
		29,
		"fail fast"
	);
	xtaskgroupstats tStats;

	testRequire((pGroup != NULL) && (pCancelB != NULL) &&
		(pError != NULL), "fail-fast task group setup failed");
	testRequire(xrtTaskGroupAdd(pGroup, pA) &&
		xrtTaskGroupAdd(pGroup, pB), "fail-fast task group add failed");
	testRequire(xrtPromiseReject(pPromiseA, pError),
		"fail-fast source reject failed");
	xrtErrorFree(pError);
	testRequire(xrtCancelRequested(pCancelB),
		"fail-fast group did not cancel sibling");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		!tStats.Accepting && tStats.Cancelling,
		"fail-fast group lifecycle mismatch");
	testRequire(xrtPromiseCancel(pPromiseB),
		"fail-fast sibling cancel completion failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"fail-fast task group wait failed");

	xrtCancelDestroy(pCancelB);
	testTaskGroupPairDestroy(pPromiseA, pA);
	testTaskGroupPairDestroy(pPromiseB, pB);
	xrtTaskGroupDestroy(pGroup);
}



/* 验证父关闭只关闭子作用域，父取消才会向叶任务传播取消。 */
static void testTaskGroupChild(void)
{
	xtaskgroup* pParent = xrtTaskGroupCreate(NULL);
	xtaskgroup* pChild = xrtTaskGroupChild(pParent, NULL);
	xfuture* pLeaf;
	xfuture* pParentDone;
	xpromise* pLeafPromise = testTaskGroupPair(&pLeaf);
	xtaskgroupstats tParentStats;

	testRequire((pParent != NULL) && (pChild != NULL),
		"nested task group create failed");
	testRequire(xrtTaskGroupAdd(pChild, pLeaf),
		"nested task group leaf add failed");
	pParentDone = xrtTaskGroupFuture(pParent);
	testRequire(xrtTaskGroupClose(pParent),
		"nested parent close failed");
	testRequire(xrtFutureState(pLeaf) == XFUTURE_PENDING,
		"parent Close unexpectedly cancelled leaf");
	testRequire(xrtFutureState(pParentDone) == XFUTURE_PENDING,
		"nested parent completed before leaf");
	testRequire(xrtPromiseResolve(pLeafPromise, NULL),
		"nested leaf resolve failed");
	testRequire(xrtFutureWait(pParentDone) == XWAIT_OK,
		"nested parent Done wait failed");
	testRequire(xrtTaskGroupGet(pParent, &tParentStats) &&
		(tParentStats.Added == 1) && (tParentStats.Succeeded == 1),
		"nested parent aggregate stats mismatch");

	xrtFutureDestroy(pParentDone);
	testTaskGroupPairDestroy(pLeafPromise, pLeaf);
	xrtTaskGroupDestroy(pChild);
	xrtTaskGroupDestroy(pParent);

	pParent = xrtTaskGroupCreate(NULL);
	pChild = xrtTaskGroupChild(pParent, NULL);
	pLeafPromise = testTaskGroupPair(&pLeaf);
	testRequire((pParent != NULL) && (pChild != NULL) &&
		xrtTaskGroupAdd(pChild, pLeaf),
		"nested cancel setup failed");
	{
		xcancel* pLeafCancel = xrtFutureCancelToken(pLeaf);

		testRequire((pLeafCancel != NULL) && xrtTaskGroupCancel(pParent),
			"nested parent cancel failed");
		testRequire(xrtCancelRequested(pLeafCancel),
			"nested parent cancel did not reach leaf");
		xrtCancelDestroy(pLeafCancel);
	}
	testRequire(xrtPromiseCancel(pLeafPromise),
		"nested cancelled leaf completion failed");
	testRequire(xrtTaskGroupWait(pParent) == XWAIT_OK,
		"nested cancelled parent wait failed");
	testTaskGroupPairDestroy(pLeafPromise, pLeaf);
	xrtTaskGroupDestroy(pChild);
	xrtTaskGroupDestroy(pParent);
}



/* 验证外部父令牌和提前 Destroy 都保留延迟完成所需的内部引用。 */
static void testTaskGroupParentAndDestroy(void)
{
	xcancel* pParentCancel = xrtCancelCreate();
	xtaskgroupconfig tConfig = { pParentCancel, 0, 0 };
	xtaskgroup* pGroup = xrtTaskGroupCreate(&tConfig);
	xfuture* pSource;
	xfuture* pDone;
	xpromise* pPromise = testTaskGroupPair(&pSource);
	xcancel* pSourceCancel = xrtFutureCancelToken(pSource);

	testRequire((pParentCancel != NULL) && (pGroup != NULL) &&
		(pSourceCancel != NULL), "parented task group setup failed");
	testRequire(xrtTaskGroupAdd(pGroup, pSource),
		"parented task group add failed");
	pDone = xrtTaskGroupFuture(pGroup);
	testRequire(xrtCancelRequest(pParentCancel),
		"task group parent cancel request failed");
	testRequire(xrtCancelRequested(pSourceCancel),
		"task group parent cancel did not reach source");
	xrtTaskGroupDestroy(pGroup);
	testRequire(xrtFutureState(pDone) == XFUTURE_PENDING,
		"destroyed task group completed before source confirmation");
	testRequire(xrtPromiseCancel(pPromise),
		"destroyed task group source cancel failed");
	testRequire(xrtFutureWait(pDone) == XWAIT_OK,
		"destroyed task group deferred Done failed");

	xrtFutureDestroy(pDone);
	xrtCancelDestroy(pSourceCancel);
	testTaskGroupPairDestroy(pPromise, pSource);
	xrtCancelDestroy(pParentCancel);
}



/* 覆盖结构化任务组的结果、背压、取消、嵌套和延迟回收契约。 */
int main(void)
{
	testTaskGroupStart();
	testTaskGroupBasic();
	testTaskGroupLimit();
	testTaskGroupCancel();
	testTaskGroupFailFast();
	testTaskGroupChild();
	testTaskGroupParentAndDestroy();
	printf("[PASS] task group\n");
	return 0;
}
