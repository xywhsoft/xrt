#include "../test.h"



/* 分组协程任务记录执行、析构和值返回。 */
typedef struct testtaskgroupco {
	int Value;
	int Hits;
	int Destroyed;
} testtaskgroupco;



/* 受理后的协程任务数据恰好析构一次。 */
static void testTaskGroupCoDestroy(ptr pValue, ptr pData)
{
	testtaskgroupco* pContext = (testtaskgroupco*)pData;

	(void)pValue;
	pContext->Destroyed++;
}



/* 分组协程任务返回借用值。 */
static xtaskoutcome testTaskGroupCoRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgroupco* pContext = (testtaskgroupco*)pData;

	(void)pCancel;
	pContext->Hits++;
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 验证协程提交的成功、预取消和关闭拒绝都保持组与所有权契约。 */
int main(void)
{
	testtaskgroupco tSuccess;
	testtaskgroupco tCancelled;
	testtaskgroupco tRejected;
	xtaskargs tArgs;
	xtaskgroupstats tStats;
	xtaskgroup* pGroup;
	xcosched* pSched;
	xfuture* pFuture;

	memset(&tSuccess, 0, sizeof(tSuccess));
	memset(&tCancelled, 0, sizeof(tCancelled));
	memset(&tRejected, 0, sizeof(tRejected));
	memset(&tArgs, 0, sizeof(tArgs));
	memset(&tStats, 0, sizeof(tStats));
	tSuccess.Value = 91;
	tCancelled.Value = 92;
	tRejected.Value = 93;
	tArgs.Destroy = testTaskGroupCoDestroy;

	pSched = xrtCoSchedCreate();
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire((pSched != NULL) && (pGroup != NULL),
		"task group coroutine setup failed");
	tArgs.DestroyData = &tSuccess;
	pFuture = xrtTaskGroupCo(
		pGroup,
		pSched,
		testTaskGroupCoRun,
		&tSuccess,
		&tArgs,
		0
	);
	testRequire(pFuture != NULL, "task group coroutine submit failed");
	testRequire(xrtCoSchedRun(pSched), "task group coroutine run failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"task group coroutine wait failed");
	testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Succeeded == 1) && (tSuccess.Hits == 1) &&
		(tSuccess.Destroyed == 1) &&
		(xrtFutureValue(pFuture) == &tSuccess.Value),
		"task group coroutine success mismatch");
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	testRequire(xrtCoSchedDestroy(pSched),
		"task group coroutine scheduler destroy failed");

	pSched = xrtCoSchedCreate();
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire((pSched != NULL) && (pGroup != NULL),
		"task group coroutine cancel setup failed");
	tArgs.DestroyData = &tCancelled;
	pFuture = xrtTaskGroupCo(
		pGroup,
		pSched,
		testTaskGroupCoRun,
		&tCancelled,
		&tArgs,
		0
	);
	testRequire((pFuture != NULL) && xrtTaskGroupCancel(pGroup),
		"task group coroutine cancel failed");
	testRequire(xrtCoSchedRun(pSched),
		"cancelled task group coroutine run failed");
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"cancelled task group coroutine wait failed");
	testRequire((xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		(tCancelled.Hits == 0) && (tCancelled.Destroyed == 1),
		"task group coroutine cancellation mismatch");
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	testRequire(xrtCoSchedDestroy(pSched),
		"cancelled task group scheduler destroy failed");

	pSched = xrtCoSchedCreate();
	pGroup = xrtTaskGroupCreate(NULL);
	testRequire((pSched != NULL) && (pGroup != NULL) &&
		xrtTaskGroupClose(pGroup),
		"closed task group coroutine setup failed");
	tArgs.DestroyData = &tRejected;
	testRequire(xrtTaskGroupCo(
		pGroup,
		pSched,
		testTaskGroupCoRun,
		&tRejected,
		&tArgs,
		0
	) == NULL, "closed group accepted coroutine task");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(tRejected.Hits == 0) && (tRejected.Destroyed == 0),
		"closed group coroutine ownership mismatch");
	xrtTaskGroupDestroy(pGroup);
	testRequire(xrtCoSchedDestroy(pSched),
		"closed task group scheduler destroy failed");
	testRequire(xrtCoThreadDetach(), "task group coroutine detach failed");

	printf("[PASS] task group coroutine\n");
	return 0;
}
