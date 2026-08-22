#include "../test.h"



/* 可切换失败分配器分别覆盖组创建和活动项接纳失败。 */
typedef struct testtaskgroupoom {
	bool Fail;
	size_t Calls;
} testtaskgroupoom;



/* 正常阶段转发系统堆，失败阶段拒绝全部新块。 */
static ptr testTaskGroupOomAlloc(ptr pData, size_t iSize)
{
	testtaskgroupoom* pState = (testtaskgroupoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 保持完整重分配契约，任务组本身不依赖重分配。 */
static ptr testTaskGroupOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testtaskgroupoom* pState = (testtaskgroupoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段建立的底层块。 */
static void testTaskGroupOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 启动器上下文验证预留 OOM 不调用过程，过程 OOM 则完整回滚。 */
typedef struct testtaskgroupoomstart {
	xerror* Error;
	int Calls;
	bool FailInside;
} testtaskgroupoomstart;



/* 可在启动器内部切换分配失败并尝试创建 Future。 */
static xfuture* testTaskGroupOomStart(ptr pData)
{
	testtaskgroupoomstart* pStart =
		(testtaskgroupoomstart*)pData;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	pStart->Calls++;
	if ( pStart->FailInside ) {
		xrtSetError(pStart->Error);
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		return NULL;
	}
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 验证 OOM 不改变源状态、不取得调用方引用且统计拒绝。 */
int main(void)
{
	enum { TEST_TASK_GROUP_OOM_SOURCES = 4096 };
	testtaskgroupoom tState = { false, 0 };
	xallocator tAllocator = {
		&tState,
		testTaskGroupOomAlloc,
		testTaskGroupOomRealloc,
		testTaskGroupOomFree
	};
	xtaskgroup* pGroup;
	xfuture** arrFuture;
	xpromise** arrPromise;
	xtaskgroupstats tStats;
	testtaskgroupoomstart tStart;
	size_t iFailed = SIZE_MAX;
	size_t iActive;

	testRequire(xrtSetAllocator(&tAllocator),
		"failed to install task group OOM allocator");
	tState.Fail = true;
	testRequire(xrtTaskGroupCreate(NULL) == NULL,
		"task group create succeeded under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"task group create OOM error mismatch");

	tState.Fail = false;
	pGroup = xrtTaskGroupCreate(NULL);
	arrFuture = (xfuture**)malloc(
		TEST_TASK_GROUP_OOM_SOURCES * sizeof(xfuture*)
	);
	arrPromise = (xpromise**)malloc(
		TEST_TASK_GROUP_OOM_SOURCES * sizeof(xpromise*)
	);
	testRequire((pGroup != NULL) && (arrFuture != NULL) &&
		(arrPromise != NULL),
		"task group OOM fixture create failed");
	for ( size_t i = 0; i < TEST_TASK_GROUP_OOM_SOURCES; i++ ) {
		arrPromise[i] = xrtPromiseCreate(&arrFuture[i], NULL);
		testRequire(arrPromise[i] != NULL,
			"task group OOM source array create failed");
	}
	memset(&tStart, 0, sizeof(tStart));
	tStart.Error = xrtErrorCreate(
		XERR_MEMORY,
		"test.task_group",
		1,
		"task group starter memory failure"
	);
	testRequire(tStart.Error != NULL,
		"task group starter error create failed");
	tState.Fail = true;
	xrtClearError();
	for ( size_t i = 0; i < TEST_TASK_GROUP_OOM_SOURCES; i++ ) {
		if ( !xrtTaskGroupAdd(pGroup, arrFuture[i]) ) {
			iFailed = i;
			break;
		}
	}
	testRequire(iFailed != SIZE_MAX,
		"task group node cache did not reach bottom OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"task group add OOM error mismatch");
	testRequire((xrtFutureState(arrFuture[iFailed]) == XFUTURE_PENDING) &&
		xrtTaskGroupGet(pGroup, &tStats) && (tStats.Rejected == 1),
		"task group add OOM changed source or counters");
	iActive = tStats.Active;

	testRequire(xrtTaskGroupStart(
		pGroup,
		testTaskGroupOomStart,
		&tStart
	) == NULL, "task group start reserved under OOM");
	testRequire((tStart.Calls == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"reservation OOM invoked task group starter");

	tState.Fail = false;
	tStart.FailInside = true;
	xrtClearError();
	testRequire(xrtTaskGroupStart(
		pGroup,
		testTaskGroupOomStart,
		&tStart
	) == NULL, "task group starter OOM succeeded");
	tState.Fail = false;
	testRequire((tStart.Calls == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Active == iActive) && (tStats.Rejected == 3),
		"starter OOM did not preserve error or rollback reservation");

	for ( size_t i = 0; i < TEST_TASK_GROUP_OOM_SOURCES; i++ ) {
		testRequire(xrtPromiseResolve(arrPromise[i], NULL),
			"task group OOM source resolve failed");
		xrtPromiseDestroy(arrPromise[i]);
		xrtFutureDestroy(arrFuture[i]);
	}
	testRequire(xrtTaskGroupWait(pGroup) == XWAIT_OK,
		"task group OOM accepted-source drain failed");
	xrtTaskGroupDestroy(pGroup);
	xrtErrorFree(tStart.Error);
	free(arrPromise);
	free(arrFuture);
	testRequire(tState.Calls != 0,
		"task group OOM allocator observed no allocation");

	printf("[PASS] task group OOM\n");
	return 0;
}
