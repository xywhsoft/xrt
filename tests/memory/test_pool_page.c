#include "../test.h"

#include <stdint.h>



/* 验证单页容量、复用、索引、安全释放、标记回收和显式对齐。 */
int main(void)
{
	xpoolpage tPage;
	xpoolpage tAligned;
	xpoolpage tCompact;
	xpoolpage* pCreated;
	xpoolpageinfo tInfo;
	ptr arrObject[XRT_POOL_PAGE_CAPACITY];
	int iForeign = 0;
	size_t iIndex;

	testRequire(xrtPoolPageInit(&tPage, 1), "default page init failed");
	testRequire(
		tPage.Memory != tPage.Allocation,
		"pool page user range aliases its global allocation"
	);
	for ( size_t i = 0; i < XRT_POOL_PAGE_CAPACITY; i++ ) {
		arrObject[i] = xrtPoolPageAlloc(&tPage);
		testRequire(arrObject[i] != NULL, "page allocation failed before capacity");
		testRequire(
			((uintptr_t)arrObject[i] % XRT_POOL_ALIGNMENT_DEFAULT) == 0,
			"default page alignment mismatch"
		);
		testRequire(
			xrtPoolPageIndex(&tPage, arrObject[i], &iIndex) && (iIndex == i),
			"page index mismatch"
		);
	}
	testRequire(xrtPoolPageAlloc(&tPage) == NULL, "full page allocation should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "full page error kind mismatch");
	xrtClearError();

	testRequire(xrtPoolPageFree(&tPage, arrObject[10]), "page free failed");
	testRequire(!xrtPoolPageOwns(&tPage, arrObject[10]), "freed slot still reported live");
	testRequire(!xrtPoolPageFree(&tPage, arrObject[10]), "double free should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "double free error kind mismatch");
	xrtClearError();
	testRequire(!xrtPoolPageFree(&tPage, &iForeign), "foreign pointer free should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "foreign pointer error kind mismatch");
	xrtClearError();
	testRequire(
		!xrtPoolPageFree(&tPage, (bytes)arrObject[0] + 1),
		"interior page pointer free should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "interior pointer error mismatch");
	xrtClearError();
	testRequire(
		xrtPoolPageAlloc(&tPage) == arrObject[10],
		"released page slot was not reused"
	);
	testRequire(
		!xrtPoolPageFreeAt(&tPage, XRT_POOL_PAGE_CAPACITY),
		"out-of-range page index should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "page index error kind mismatch");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INDEX_OUT_OF_RANGE,
		"page index error code mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPoolPageIndex(&tPage, arrObject[0], NULL),
		"null page index output should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "page index output error mismatch");
	xrtClearError();
	testRequire(
		!xrtPoolPageIndex(&tPage, &iForeign, &iIndex),
		"foreign page index lookup should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_POINTER,
		"foreign page index error code mismatch"
	);
	xrtClearError();

	testRequire(xrtPoolPageMark(&tPage, arrObject[0]), "page mark failed");
	testRequire(
		xrtPoolPageSweep(&tPage) == (XRT_POOL_PAGE_CAPACITY - 1),
		"page sweep count mismatch"
	);
	testRequire(xrtPoolPageOwns(&tPage, arrObject[0]), "marked object was not preserved");
	testRequire(xrtPoolPageSweep(&tPage) == 1, "survivor mark was not cleared");
	testRequire(tPage.LiveCount == 0, "page should be empty after second sweep");

	arrObject[0] = xrtPoolPageCalloc(&tPage);
	testRequire(arrObject[0] != NULL, "page calloc failed");
	for ( size_t i = 0; i < tPage.ItemSize; i++ ) {
		testRequire(((bytes)arrObject[0])[i] == 0, "page calloc did not clear item bytes");
	}
	testRequire(xrtPoolPageMark(&tPage, arrObject[0]), "page mark before free-marked failed");
	testRequire(xrtPoolPageFreeMarked(&tPage) == 1, "page free-marked count mismatch");

	xrtPoolPageGetInfo(&tPage, &tInfo);
	testRequire(tInfo.Capacity == XRT_POOL_PAGE_CAPACITY, "page info capacity mismatch");
	testRequire(tInfo.Stride == XRT_POOL_ALIGNMENT_DEFAULT, "page info stride mismatch");
	xrtPoolPageUnit(&tPage);

	testRequire(xrtPoolPageInitAligned(&tAligned, 33, 64), "aligned page init failed");
	testRequire(
		tAligned.Memory != tAligned.Allocation,
		"aligned pool page aliases its global allocation"
	);
	arrObject[0] = xrtPoolPageAlloc(&tAligned);
	testRequire(arrObject[0] != NULL, "aligned page allocation failed");
	testRequire(((uintptr_t)arrObject[0] % 64) == 0, "explicit page alignment mismatch");
	testRequire(tAligned.Stride == 64, "aligned page stride mismatch");
	testRequire(xrtPoolPageReset(&tAligned) == 1, "page reset count mismatch");
	xrtPoolPageUnit(&tAligned);

	/* 显式小页只分配所需槽数，并保持完整的满页和索引边界。 */
	testRequire(
		xrtPoolPageInitLayout(&tCompact, 8192, 64, 3),
		"compact page init failed"
	);
	xrtPoolPageGetInfo(&tCompact, &tInfo);
	testRequire(
		(tInfo.Capacity == 3) &&
		(tCompact.MemorySize == (tCompact.Stride * 3)),
		"compact page layout mismatch"
	);
	for ( size_t i = 0; i < 3; i++ ) {
		arrObject[i] = xrtPoolPageAlloc(&tCompact);
		testRequire(arrObject[i] != NULL, "compact page allocation failed");
	}
	testRequire(xrtPoolPageAlloc(&tCompact) == NULL, "compact page should be full");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "compact page full error");
	xrtClearError();
	testRequire(
		!xrtPoolPageFreeAt(&tCompact, 3),
		"compact page out-of-range index should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "compact page index error");
	xrtPoolPageUnit(&tCompact);

	/* 堆创建入口必须复用相同的显式布局合同。 */
	pCreated = xrtPoolPageCreateLayout(4096, 64, 5);
	testRequire(pCreated != NULL, "compact page create failed");
	testRequire(
		(pCreated->Capacity == 5) &&
		(pCreated->MemorySize == (pCreated->Stride * 5)),
		"compact created page layout mismatch"
	);
	xrtPoolPageDestroy(pCreated);

	testRequire(
		!xrtPoolPageInitAligned(&tPage, 8, 3),
		"non-power-of-two alignment should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "alignment error kind mismatch");
	xrtClearError();
	testRequire(!xrtPoolPageInit(&tPage, 0), "zero-size page should fail");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_SIZE,
		"zero-size page error code mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPoolPageInitLayout(&tPage, 8, 16, 0),
		"zero page capacity should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_CAPACITY,
		"zero page capacity error code mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPoolPageInitLayout(
			&tPage,
			8,
			16,
			XRT_POOL_PAGE_CAPACITY + 1u
		),
		"oversized page capacity should fail"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPOOL_ERROR_INVALID_CAPACITY,
		"oversized page capacity error code mismatch"
	);
	xrtClearError();
	testRequire(!xrtPoolPageInit(&tPage, SIZE_MAX), "overflowing page layout should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "page overflow error kind mismatch");
	xrtClearError();

	printf("[PASS] pool page\n");
	return 0;
}
