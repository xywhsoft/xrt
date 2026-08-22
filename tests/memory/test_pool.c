#include "../test.h"

#include <stdint.h>



/* 统计固定池访问器看到的活动对象。 */
static bool testPoolVisit(ptr pObject, size_t iIndex, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	(void)iIndex;
	testRequire(pObject != NULL, "pool visitor received null object");
	(*pCount)++;
	return true;
}



/* 验证上一个操作被活动访问器以稳定错误拒绝。 */
static void testPoolRequireVisitError(void)
{
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "pool visit error kind mismatch");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_VISIT_ACTIVE,
		"pool visit error code mismatch"
	);
	xrtClearError();
}



/* 在访问回调中验证结构性修改被拒绝，而标记仍然可用。 */
static bool testPoolVisitMutation(ptr pObject, size_t iIndex, ptr pUserData)
{
	xpool* pPool = (xpool*)pUserData;
	xpoolpage* pPage = pPool->Pages;

	(void)iIndex;
	testRequire(xrtPoolAlloc(pPool) == NULL, "pool alloc should fail during visit");
	testPoolRequireVisitError();
	testRequire(!xrtPoolFree(pPool, pObject), "pool free should fail during visit");
	testPoolRequireVisitError();
	testRequire(xrtPoolSweep(pPool) == 0, "pool sweep should fail during visit");
	testPoolRequireVisitError();
	testRequire(xrtPoolFreeMarked(pPool) == 0, "pool free-marked should fail during visit");
	testPoolRequireVisitError();
	testRequire(xrtPoolReset(pPool) == 0, "pool reset should fail during visit");
	testPoolRequireVisitError();
	testRequire(xrtPoolTrim(pPool, 0) == 0, "pool trim should fail during visit");
	testPoolRequireVisitError();
	xrtPoolSetRetain(pPool, 0);
	testPoolRequireVisitError();
	xrtPoolUnit(pPool);
	testPoolRequireVisitError();
	xrtPoolDestroy(pPool);
	testPoolRequireVisitError();
	testRequire(xrtPoolPageAlloc(pPage) == NULL, "page alloc should fail during pool visit");
	testPoolRequireVisitError();
	testRequire(
		!xrtPoolPageFree(pPage, pObject),
		"page free should fail during pool visit"
	);
	testPoolRequireVisitError();
	testRequire(xrtPoolPageReset(pPage) == 0, "page reset should fail during pool visit");
	testPoolRequireVisitError();
	xrtPoolPageUnit(pPage);
	testPoolRequireVisitError();
	xrtPoolPageDestroy(pPage);
	testPoolRequireVisitError();
	testRequire(
		xrtPoolVisit(pPool, testPoolVisit, NULL) == 0,
		"nested pool visit should fail"
	);
	testPoolRequireVisitError();
	testRequire(xrtPoolMark(pPool, pObject), "pool mark should remain available during visit");
	return false;
}



/* 验证固定池跨页增长、回收、安全边界、访问和标记语义。 */
int main(void)
{
	xpool tPool;
	xpool tOther;
	xpool tLarge;
	xpool tHuge;
	xpool tLayout;
	xpool* pLayout;
	xpool* pGuard;
	xpoolinfo tInfo;
	ptr arrObject[600];
	ptr pGuardObject;
	size_t iVisited = 0;
	int iForeign = 0;

	testRequire(xrtPoolInitAligned(&tPool, 24, 32), "fixed pool init failed");
	for ( size_t i = 0; i < 600; i++ ) {
		arrObject[i] = xrtPoolAlloc(&tPool);
		testRequire(arrObject[i] != NULL, "fixed pool allocation failed");
		testRequire(((uintptr_t)arrObject[i] % 32) == 0, "fixed pool alignment mismatch");
		memset(arrObject[i], (int)(i & 0xFFu), 24);
	}
	xrtPoolGet(&tPool, &tInfo);
	testRequire(tInfo.PageCount == 3, "fixed pool should grow to three pages");
	testRequire(tInfo.LiveCount == 600, "fixed pool live count mismatch");
	testRequire(tInfo.Capacity == 768, "fixed pool capacity mismatch");
	testRequire(tInfo.PageCapacity == XRT_POOL_PAGE_CAPACITY, "small pool page capacity mismatch");

	testRequire(xrtPoolFree(&tPool, arrObject[257]), "fixed pool free failed");
	testRequire(!xrtPoolFree(&tPool, arrObject[257]), "fixed pool double free should fail");
	xrtClearError();
	testRequire(xrtPoolAlloc(&tPool) == arrObject[257], "fixed pool did not reuse slot");

	testRequire(xrtPoolInit(&tOther, 24), "second fixed pool init failed");
	testRequire(!xrtPoolFree(&tOther, arrObject[0]), "cross-pool free should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "cross-pool error kind mismatch");
	xrtClearError();
	testRequire(!xrtPoolFree(&tPool, &iForeign), "foreign fixed-pool free should fail");
	xrtClearError();
	testRequire(
		!xrtPoolFree(&tPool, (bytes)arrObject[0] + 1),
		"interior fixed-pool pointer free should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "fixed interior error mismatch");
	xrtClearError();

	for ( size_t i = 0; i < 600; i += 3 ) {
		testRequire(xrtPoolMark(&tPool, arrObject[i]), "fixed pool mark failed");
	}
	testRequire(xrtPoolSweep(&tPool) == 400, "fixed pool sweep count mismatch");
	xrtPoolGet(&tPool, &tInfo);
	testRequire(tInfo.LiveCount == 200, "fixed pool sweep live count mismatch");
	testRequire(
		xrtPoolVisit(&tPool, testPoolVisit, &iVisited) == 200,
		"fixed pool visit count mismatch"
	);
	testRequire(iVisited == 200, "fixed pool visitor state mismatch");
	testRequire(xrtPoolSweep(&tPool) == 200, "fixed pool survivor marks were not cleared");
	xrtPoolGet(&tPool, &tInfo);
	testRequire(tInfo.PageCount == 1, "fixed pool should retain one empty page");
	testRequire(tInfo.EmptyPages == 1, "fixed pool empty page count mismatch");
	testRequire(xrtPoolTrim(&tPool, 0) == 1, "fixed pool trim count mismatch");
	xrtPoolGet(&tPool, &tInfo);
	testRequire(tInfo.PageCount == 0, "fixed pool trim did not release all empty pages");

	xrtPoolSetRetain(&tPool, 2);
	for ( size_t i = 0; i < 300; i++ ) {
		arrObject[i] = xrtPoolCalloc(&tPool);
		testRequire(arrObject[i] != NULL, "fixed pool calloc failed");
	}
	testRequire(xrtPoolReset(&tPool) == 300, "fixed pool reset count mismatch");
	xrtPoolGet(&tPool, &tInfo);
	testRequire(tInfo.PageCount == 2, "fixed pool retain policy mismatch");

	/* 旧版正反两种 GC 语义分别映射到 Sweep 和 FreeMarked。 */
	arrObject[0] = xrtPoolAlloc(&tOther);
	arrObject[1] = xrtPoolAlloc(&tOther);
	testRequire(
		(arrObject[0] != NULL) && (arrObject[1] != NULL),
		"fixed pool GC setup failed"
	);
	testRequire(xrtPoolMark(&tOther, arrObject[0]), "fixed pool free-marked setup failed");
	testRequire(xrtPoolFreeMarked(&tOther) == 1, "fixed pool free-marked count mismatch");
	testRequire(!xrtPoolOwns(&tOther, arrObject[0]), "marked fixed object was not released");
	testRequire(xrtPoolOwns(&tOther, arrObject[1]), "unmarked fixed object was released");

	/* 访问器建立稳定迭代边界，并允许回调执行标记。 */
	pGuard = xrtPoolCreate(sizeof(uint32));
	testRequire(pGuard != NULL, "pool visit guard create failed");
	pGuardObject = xrtPoolAlloc(pGuard);
	testRequire(pGuardObject != NULL, "pool visit guard allocation failed");
	testRequire(
		xrtPoolVisit(pGuard, testPoolVisitMutation, pGuard) == 1,
		"pool visit guard count mismatch"
	);
	testRequire(xrtPoolOwns(pGuard, pGuardObject), "pool visitor changed allocation set");
	testRequire(xrtPoolSweep(pGuard) == 0, "pool visitor mark was not preserved");
	testRequire(xrtPoolSweep(pGuard) == 1, "pool visitor mark was not cleared");
	xrtPoolDestroy(pGuard);

	/* 自动页布局把大对象首批成本限制在约 64 KiB。 */
	testRequire(xrtPoolInit(&tLarge, 8192), "large-object pool init failed");
	testRequire(tLarge.PageCapacity == 8, "large-object automatic page capacity mismatch");
	for ( size_t i = 0; i < 9; i++ ) {
		arrObject[i] = xrtPoolAlloc(&tLarge);
		testRequire(arrObject[i] != NULL, "large-object pool allocation failed");
	}
	xrtPoolGet(&tLarge, &tInfo);
	testRequire(
		(tInfo.PageCapacity == 8) &&
		(tInfo.PageCount == 2) &&
		(tInfo.Capacity == 16) &&
		(tLarge.Pages->MemorySize <= XRT_POOL_PAGE_BYTES_DEFAULT),
		"large-object automatic page layout mismatch"
	);
	xrtPoolUnit(&tLarge);

	/* 单槽超过目标页大小时，自动布局稳定退化为一页一槽。 */
	testRequire(
		xrtPoolInit(&tHuge, XRT_POOL_PAGE_BYTES_DEFAULT + 1u),
		"huge-object pool init failed"
	);
	testRequire(tHuge.PageCapacity == 1, "huge-object page capacity mismatch");
	testRequire(xrtPoolAlloc(&tHuge) != NULL, "huge-object pool allocation failed");
	testRequire(
		(tHuge.Pages != NULL) &&
		(tHuge.Pages->Capacity == 1) &&
		(tHuge.Pages->MemorySize >= (XRT_POOL_PAGE_BYTES_DEFAULT + 1u)),
		"huge-object one-slot page mismatch"
	);
	xrtPoolUnit(&tHuge);

	/* 显式布局允许调用方用更多页换取更低的单次分配延迟。 */
	testRequire(
		xrtPoolInitLayout(&tLayout, 8192, 64, 2),
		"explicit fixed pool layout init failed"
	);
	for ( size_t i = 0; i < 3; i++ ) {
		arrObject[i] = xrtPoolAlloc(&tLayout);
		testRequire(arrObject[i] != NULL, "explicit fixed pool allocation failed");
	}
	xrtPoolGet(&tLayout, &tInfo);
	testRequire(
		(tInfo.PageCapacity == 2) &&
		(tInfo.PageCount == 2) &&
		(tInfo.Capacity == 4),
		"explicit fixed pool layout mismatch"
	);
	xrtPoolUnit(&tLayout);
	pLayout = xrtPoolCreateLayout(4096, 64, 3);
	testRequire(pLayout != NULL, "explicit fixed pool layout create failed");
	testRequire(pLayout->PageCapacity == 3, "created fixed pool layout mismatch");
	xrtPoolDestroy(pLayout);

	xrtPoolUnit(&tOther);
	xrtPoolUnit(&tPool);
	testRequire(!xrtPoolInit(&tPool, 0), "zero-size fixed pool should fail");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_SIZE,
		"zero-size fixed pool error code mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPoolInitLayout(&tPool, 8, 16, 0),
		"zero fixed pool page capacity should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_CAPACITY,
		"fixed pool page capacity error code mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPoolInitLayout(
			&tPool,
			8,
			16,
			XRT_POOL_PAGE_CAPACITY + 1u
		),
		"oversized fixed pool page capacity should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_CAPACITY,
		"oversized fixed pool page capacity error code mismatch"
	);
	xrtClearError();
	testRequire(!xrtPoolInit(&tPool, SIZE_MAX), "overflowing fixed pool layout should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "fixed pool overflow mismatch");
	xrtClearError();
	printf("[PASS] fixed pool\n");
	return 0;
}
