#include "../test.h"

#include <stdint.h>



/* 验证变长池访问器收到完整且有效的块信息。 */
static bool testMemPoolVisit(
	ptr pMemory,
	size_t iSize,
	size_t iAlignment,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	testRequire(pMemory != NULL, "memory pool visitor received null block");
	testRequire(iSize != 0, "memory pool visitor received zero size");
	testRequire(iAlignment != 0, "memory pool visitor received zero alignment");
	(*pCount)++;
	return true;
}



/* 在访问到第一个块后停止。 */
static bool testMemPoolVisitStop(
	ptr pMemory,
	size_t iSize,
	size_t iAlignment,
	ptr pUserData
)
{
	(void)pMemory;
	(void)iSize;
	(void)iAlignment;
	(void)pUserData;
	return false;
}



/* 验证上一个变长池操作被活动访问器以稳定错误拒绝。 */
static void testMemPoolRequireVisitError(void)
{
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"memory pool visit error kind mismatch"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_VISIT_ACTIVE,
		"memory pool visit error code mismatch"
	);
	xrtClearError();
}



/* 在访问回调中验证结构性修改被拒绝，而标记仍然可用。 */
static bool testMemPoolVisitMutation(
	ptr pMemory,
	size_t iSize,
	size_t iAlignment,
	ptr pUserData
)
{
	xmempool* pPool = (xmempool*)pUserData;
	xpoolpage* pPage = pPool->Pages[0];
	xpool* pClass = (xpool*)pPage->Parent;

	(void)iSize;
	(void)iAlignment;
	testRequire(xrtMemPoolAlloc(pPool, 32) == NULL, "memory alloc should fail during visit");
	testMemPoolRequireVisitError();
	testRequire(
		xrtMemPoolAllocAligned(pPool, 32, 64) == NULL,
		"aligned memory alloc should fail during visit"
	);
	testMemPoolRequireVisitError();
	testRequire(!xrtMemPoolFree(pPool, pMemory), "memory free should fail during visit");
	testMemPoolRequireVisitError();
	testRequire(
		xrtMemPoolRealloc(pPool, pMemory, 64) == NULL,
		"memory realloc should fail during visit"
	);
	testMemPoolRequireVisitError();
	testRequire(xrtMemPoolSweep(pPool) == 0, "memory sweep should fail during visit");
	testMemPoolRequireVisitError();
	testRequire(
		xrtMemPoolFreeMarked(pPool) == 0,
		"memory free-marked should fail during visit"
	);
	testMemPoolRequireVisitError();
	testRequire(xrtMemPoolReset(pPool) == 0, "memory reset should fail during visit");
	testMemPoolRequireVisitError();
	testRequire(xrtMemPoolTrim(pPool, 0) == 0, "memory trim should fail during visit");
	testMemPoolRequireVisitError();
	xrtMemPoolUnit(pPool);
	testMemPoolRequireVisitError();
	xrtMemPoolDestroy(pPool);
	testMemPoolRequireVisitError();
	testRequire(
		!xrtPoolFree(pClass, pMemory),
		"size-class free should fail during memory visit"
	);
	testMemPoolRequireVisitError();
	testRequire(
		!xrtPoolPageFree(pPage, pMemory),
		"page free should fail during memory visit"
	);
	testMemPoolRequireVisitError();
	xrtPoolPageUnit(pPage);
	testMemPoolRequireVisitError();
	xrtPoolDestroy(pClass);
	testMemPoolRequireVisitError();
	testRequire(
		xrtMemPoolVisit(pPool, testMemPoolVisitStop, NULL) == 0,
		"nested memory pool visit should fail"
	);
	testMemPoolRequireVisitError();
	testRequire(
		xrtMemPoolMark(pPool, pMemory),
		"memory pool mark should remain available during visit"
	);
	return false;
}



/* 验证变长池尺寸类、大块、对齐、哈希登记、重分配和 GC。 */
int main(void)
{
	xmempool tPool;
	xmempool tOther;
	xmempool tCompact;
	xmempool* pGuard;
	xmempoolinfo tInfo;
	ptr p1;
	ptr p16;
	ptr p17;
	ptr p100;
	ptr p101;
	ptr pAligned;
	ptr pResize;
	ptr pZero;
	ptr pZeroed;
	ptr arrLarge[96];
	ptr arrCompact[12];
	ptr pGuardMemory;
	size_t iLargeCapacity;
	size_t iVisited = 0;
	int iForeign = 0;

	testRequire(xrtMemPoolInit(&tPool, 100), "variable pool init failed");
	p1 = xrtMemPoolAlloc(&tPool, 1);
	p16 = xrtMemPoolAlloc(&tPool, 16);
	p17 = xrtMemPoolAlloc(&tPool, 17);
	p100 = xrtMemPoolAlloc(&tPool, 100);
	p101 = xrtMemPoolAlloc(&tPool, 101);
	pAligned = xrtMemPoolAllocAligned(&tPool, 7, 64);
	testRequire(
		(p1 != NULL) && (p16 != NULL) && (p17 != NULL) &&
		(p100 != NULL) && (p101 != NULL) && (pAligned != NULL),
		"variable pool boundary allocation failed"
	);
	testRequire(xrtMemPoolSize(&tPool, p1) == 16, "size class 1 mismatch");
	testRequire(xrtMemPoolSize(&tPool, p16) == 16, "size class 16 mismatch");
	testRequire(xrtMemPoolSize(&tPool, p17) == 32, "size class 17 mismatch");
	testRequire(xrtMemPoolSize(&tPool, p100) == 112, "partial final size class mismatch");
	testRequire(xrtMemPoolSize(&tPool, p101) == 101, "large fallback size mismatch");
	testRequire(((uintptr_t)pAligned % 64) == 0, "large explicit alignment mismatch");
	testRequire(xrtMemPoolSize(&tPool, pAligned) == 7, "aligned block size mismatch");
	pZero = xrtMemPoolAlloc(&tPool, 0);
	testRequire(pZero != NULL, "zero-size pool allocation failed");
	testRequire(xrtMemPoolSize(&tPool, pZero) >= 1, "zero-size allocation has no usable byte");
	testRequire(xrtMemPoolFree(&tPool, pZero), "zero-size allocation free failed");
	pZeroed = xrtMemPoolCalloc(&tPool, 0, 0);
	testRequire(pZeroed != NULL, "zero-size pool calloc failed");
	testRequire(((bytes)pZeroed)[0] == 0, "zero-size pool calloc did not clear usable byte");
	testRequire(xrtMemPoolFree(&tPool, pZeroed), "zero-size calloc free failed");

	testRequire(xrtMemPoolInit(&tOther, 100), "second variable pool init failed");
	testRequire(!xrtMemPoolFree(&tOther, p1), "cross-variable-pool free should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "cross-pool error kind mismatch");
	xrtClearError();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		/* 调试堆拒绝显式池对象，并且不得破坏对象的池归属。 */
		xrtFree(p1);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
			"global heap accepted a variable-pool object"
		);
		xrtClearError();
		testRequire(
			xrtMemPoolOwns(&tPool, p1),
			"global heap ownership check changed the pool object"
		);
		testRequire(
			xrtRealloc(p1, 32) == NULL,
			"global heap reallocated a variable-pool object"
		);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
			"variable-pool realloc error kind mismatch"
		);
		xrtClearError();
		testRequire(
			xrtMemPoolOwns(&tPool, p1),
			"global heap realloc changed the pool object"
		);
		xrtFree(p101);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
			"global heap accepted a large variable-pool object"
		);
		xrtClearError();
		testRequire(
			xrtMemPoolOwns(&tPool, p101),
			"global heap ownership check changed the large pool object"
		);
	#endif
	testRequire(!xrtMemPoolFree(&tPool, &iForeign), "foreign variable-pool free should fail");
	xrtClearError();
	testRequire(
		!xrtMemPoolFree(&tPool, (bytes)p1 + 1),
		"interior small-block pointer free should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "small interior error mismatch");
	xrtClearError();
	testRequire(
		!xrtMemPoolFree(&tPool, (bytes)p101 + 1),
		"interior large-block pointer free should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "large interior error mismatch");
	xrtClearError();

	pResize = xrtMemPoolAlloc(&tPool, 20);
	testRequire(pResize != NULL, "realloc source allocation failed");
	memcpy(pResize, "pool-realloc", 13);
	testRequire(
		xrtMemPoolRealloc(&tPool, pResize, 12) == pResize,
		"in-class realloc should keep address"
	);
	pResize = xrtMemPoolRealloc(&tPool, pResize, 300);
	testRequire(pResize != NULL, "growing realloc failed");
	testRequire(memcmp(pResize, "pool-realloc", 12) == 0, "growing realloc lost contents");
	testRequire(xrtMemPoolSize(&tPool, pResize) == 300, "growing realloc size mismatch");

	for ( size_t i = 0; i < 96; i++ ) {
		arrLarge[i] = xrtMemPoolAlloc(&tPool, 200 + i);
		testRequire(arrLarge[i] != NULL, "large hash-table allocation failed");
	}
	for ( size_t i = 0; i < 96; i += 2 ) {
		testRequire(xrtMemPoolFree(&tPool, arrLarge[i]), "large hash-table free failed");
	}
	testRequire(!xrtMemPoolFree(&tPool, arrLarge[0]), "large block double free should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "large double-free error mismatch");
	xrtClearError();
	for ( size_t i = 1; i < 96; i += 2 ) {
		testRequire(xrtMemPoolOwns(&tPool, arrLarge[i]), "large hash-table lookup lost entry");
	}
	testRequire(
		xrtMemPoolVisit(&tPool, testMemPoolVisit, &iVisited) == 55,
		"memory pool visit count mismatch"
	);
	testRequire(iVisited == 55, "memory pool visitor state mismatch");
	testRequire(
		xrtMemPoolVisit(&tPool, testMemPoolVisitStop, NULL) == 1,
		"memory pool visitor did not stop"
	);

	testRequire(xrtMemPoolMark(&tPool, p1), "small mark failed");
	testRequire(xrtMemPoolMark(&tPool, p101), "large mark failed");
	(void)xrtMemPoolSweep(&tPool);
	testRequire(xrtMemPoolOwns(&tPool, p1), "small marked block was swept");
	testRequire(xrtMemPoolOwns(&tPool, p101), "large marked block was swept");
	testRequire(!xrtMemPoolOwns(&tPool, p16), "unmarked small block survived sweep");
	testRequire(!xrtMemPoolOwns(&tPool, pAligned), "unmarked aligned block survived sweep");
	testRequire(xrtMemPoolSweep(&tPool) == 2, "survivor marks were not cleared");

	p1 = xrtMemPoolCalloc(&tPool, 8, sizeof(uint32));
	testRequire(p1 != NULL, "variable pool calloc failed");
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(((uint32*)p1)[i] == 0, "variable pool calloc did not clear memory");
	}
	testRequire(xrtMemPoolMark(&tPool, p1), "free-marked mark failed");
	testRequire(xrtMemPoolFreeMarked(&tPool) == 1, "variable pool free-marked mismatch");

	xrtMemPoolGet(&tPool, &tInfo);
	testRequire(tInfo.Cutoff == 100, "variable pool cutoff info mismatch");
	testRequire(tInfo.ClassCount == 7, "variable pool class count mismatch");
	testRequire(tInfo.LiveCount == 0, "variable pool should be empty");
	testRequire(tInfo.LiveBytes == 0, "variable pool live bytes should be zero");
	testRequire(tInfo.AllocCount == tInfo.FreeCount, "variable pool counters are unbalanced");
	testRequire(xrtMemPoolTrim(&tPool, 0) <= tInfo.PageCount, "variable pool trim count invalid");

	testRequire(
		xrtMemPoolCalloc(&tPool, SIZE_MAX, 2) == NULL,
		"variable pool calloc overflow should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "calloc overflow error kind mismatch");
	xrtClearError();
	xrtMemPoolUnit(&tOther);
	testRequire(!xrtMemPoolInit(&tOther, SIZE_MAX), "overflowing cutoff should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "cutoff overflow error kind mismatch");
	xrtClearError();

	/* 删除密集的大块登记表原容量重建，不因墓碑而无意义扩容。 */
	testRequire(xrtMemPoolInit(&tCompact, 16), "compact memory pool init failed");
	for ( size_t i = 0; i < 11; i++ ) {
		arrCompact[i] = xrtMemPoolAlloc(&tCompact, 17);
		testRequire(arrCompact[i] != NULL, "compact registry allocation failed");
	}
	testRequire(tCompact.LargeCapacity == 16, "compact registry initial capacity mismatch");
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(xrtMemPoolFree(&tCompact, arrCompact[i]), "compact registry free failed");
	}
	arrCompact[11] = xrtMemPoolAlloc(&tCompact, 17);
	testRequire(arrCompact[11] != NULL, "compact registry reuse allocation failed");
	testRequire(tCompact.LargeCapacity == 16, "tombstones caused registry growth");
	testRequire(tCompact.LargeDeleted == 0, "registry tombstones were not compacted");
	iLargeCapacity = tCompact.LargeCapacity;
	testRequire(xrtMemPoolReset(&tCompact) == 4, "compact registry reset count mismatch");
	testRequire(tCompact.LargeDeleted == 0, "reset left registry tombstones");
	arrCompact[0] = xrtMemPoolAlloc(&tCompact, 17);
	testRequire(arrCompact[0] != NULL, "registry allocation after reset failed");
	testRequire(
		tCompact.LargeCapacity == iLargeCapacity,
		"registry grew after reset without pressure"
	);
	xrtMemPoolUnit(&tCompact);

	/* 访问器建立稳定迭代边界，并允许回调执行标记。 */
	pGuard = xrtMemPoolCreate(16);
	testRequire(pGuard != NULL, "memory pool visit guard create failed");
	pGuardMemory = xrtMemPoolAlloc(pGuard, 16);
	testRequire(pGuardMemory != NULL, "memory pool visit guard allocation failed");
	testRequire(
		xrtMemPoolVisit(pGuard, testMemPoolVisitMutation, pGuard) == 1,
		"memory pool visit guard count mismatch"
	);
	testRequire(
		xrtMemPoolOwns(pGuard, pGuardMemory),
		"memory pool visitor changed allocation set"
	);
	testRequire(xrtMemPoolSweep(pGuard) == 0, "memory pool visitor mark was not preserved");
	testRequire(xrtMemPoolSweep(pGuard) == 1, "memory pool visitor mark was not cleared");
	xrtMemPoolDestroy(pGuard);

	xrtMemPoolUnit(&tPool);
	printf("[PASS] variable memory pool\n");
	return 0;
}
