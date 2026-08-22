#include "../test.h"



/* 可切换失败分配器允许先建立任务池，再精确拒绝任务作业。 */
typedef struct testtaskoom {
	bool Fail;
	uint64 Allocations;
	uint64 Frees;
} testtaskoom;



/* 允许阶段转发到系统 malloc，失败阶段拒绝全部新分配。 */
static ptr testTaskOomAlloc(ptr pData, size_t iSize)
{
	testtaskoom* pContext = (testtaskoom*)pData;

	pContext->Allocations++;
	return pContext->Fail ? NULL : malloc(iSize);
}



/* 测试不需要重分配，但仍提供完整分配器契约。 */
static ptr testTaskOomRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testtaskoom* pContext = (testtaskoom*)pData;

	pContext->Allocations++;
	return pContext->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放成功阶段创建的全部 XRT 对象。 */
static void testTaskOomFree(ptr pData, ptr pMemory)
{
	testtaskoom* pContext = (testtaskoom*)pData;

	pContext->Frees++;
	free(pMemory);
}



/* OOM 路径不应执行或取得任务数据。 */
static xtaskoutcome testTaskOomRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	int* pHits = (int*)pData;

	(void)pCancel;
	(void)pResult;
	(*pHits)++;
	return XTASK_SUCCESS;
}



/* 若拒绝路径错误取得所有权，独立计数会暴露问题。 */
static void testTaskOomDestroy(ptr pValue, ptr pData)
{
	int* pDestroyed = (int*)pData;

	(void)pValue;
	(*pDestroyed)++;
}



/* 验证池创建和任务创建 OOM 都完整回滚且保留调用方所有权。 */
int main(void)
{
	testtaskoom tContext;
	xallocator tAllocator;
	xtaskpoolconfig tConfig = { 1, 4, 0 };
	xtaskargs tArgs;
	xtaskpool* pPool;
	int iHits = 0;
	int iDestroyed = 0;

	memset(&tContext, 0, sizeof(tContext));
	memset(&tAllocator, 0, sizeof(tAllocator));
	memset(&tArgs, 0, sizeof(tArgs));
	tAllocator.Context = &tContext;
	tAllocator.Alloc = testTaskOomAlloc;
	tAllocator.Realloc = testTaskOomRealloc;
	tAllocator.Free = testTaskOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "failed to install task OOM allocator");

	tContext.Fail = true;
	testRequire(xrtTaskPoolCreate(&tConfig) == NULL,
		"task pool create succeeded under initial OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"task pool create OOM error mismatch");
	xrtClearError();

	tContext.Fail = false;
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "task pool create after OOM failed");
	tArgs.Destroy = testTaskOomDestroy;
	tArgs.DestroyData = &iDestroyed;
	tContext.Fail = true;
	testRequire(xrtTaskSubmit(pPool, testTaskOomRun, &iHits, &tArgs) == NULL,
		"task submit succeeded under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"task submit OOM error mismatch");
	testRequire((iHits == 0) && (iDestroyed == 0),
		"task OOM consumed caller data or ran callback");

	tContext.Fail = false;
	testRequire(xrtTaskPoolDestroy(pPool), "task OOM pool destroy failed");
	testRequire(tContext.Allocations != 0, "task OOM allocator observed no allocation");

	printf("[PASS] task pool OOM\n");
	return 0;
}
