#include "../test.h"
#include "../../src/internal/xrt_task.h"



/* 失败分配器拒绝任务作业的首次分配。 */
static ptr testTaskCoreOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器不支持重分配。 */
static ptr testTaskCoreOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 首次分配失败不会产生需要回收的任务内存。 */
static void testTaskCoreOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* OOM 路径绝不能进入用户任务过程。 */
static xtaskoutcome testTaskCoreOomRun(
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



/* 若创建失败错误取得所有权，此计数会暴露回归。 */
static void testTaskCoreOomDestroy(ptr pValue, ptr pData)
{
	int* pDestroyed = (int*)pData;

	(void)pValue;
	(*pDestroyed)++;
}



/* 验证任务核心 OOM 清空输出并保留调用方数据所有权。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testTaskCoreOomAlloc,
		testTaskCoreOomRealloc,
		testTaskCoreOomFree
	};
	xtaskargs tArgs;
	xfuture* pFuture = (xfuture*)(uintptr_t)1;
	int iHits = 0;
	int iDestroyed = 0;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskCoreOomDestroy;
	tArgs.DestroyData = &iDestroyed;
	testRequire(xrtSetAllocator(&tAllocator),
		"failed to install task core OOM allocator");
	testRequire(
		__xrtTaskCreate(
			testTaskCoreOomRun,
			&iHits,
			&tArgs,
			&pFuture
		) == NULL,
		"task core create succeeded under OOM"
	);
	testRequire(pFuture == NULL, "task core OOM did not clear Future output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"task core OOM error mismatch");
	testRequire((iHits == 0) && (iDestroyed == 0),
		"task core OOM consumed caller data");

	printf("[PASS] task core OOM\n");
	return 0;
}
