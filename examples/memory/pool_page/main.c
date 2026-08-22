#include <stdio.h>

#include <xrt.h>



/* 演示单页池的对齐、索引、安全释放、两种标记回收和重置。 */
int main(void)
{
	xpoolpage tPage;
	xpoolpage* pCreated;
	xpoolpage* pCompact;
	xpoolpageinfo tInfo;
	int* pKeep;
	int* pDrop;
	int* pValue;
	size_t iIndex;

	if ( !xrtPoolPageInitAligned(&tPage, sizeof(int), 32) ) {
		return 1;
	}
	pKeep = (int*)xrtPoolPageCalloc(&tPage);
	pDrop = (int*)xrtPoolPageAlloc(&tPage);
	if ( (pKeep == NULL) || (pDrop == NULL) ) {
		xrtPoolPageUnit(&tPage);
		return 2;
	}
	*pKeep = 7;
	*pDrop = 9;
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageSweep(&tPage) != 1) ) {
		xrtPoolPageUnit(&tPage);
		return 3;
	}
	if (
		!xrtPoolPageOwns(&tPage, pKeep) ||
		!xrtPoolPageIndex(&tPage, pKeep, &iIndex) ||
		(xrtPoolPageGet(&tPage, iIndex) != pKeep)
	) {
		xrtPoolPageUnit(&tPage);
		return 4;
	}
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageFreeMarked(&tPage) != 1) ) {
		xrtPoolPageUnit(&tPage);
		return 5;
	}

	/* 指针释放与索引释放都保留精确边界检查。 */
	pValue = (int*)xrtPoolPageAlloc(&tPage);
	if (
		(pValue == NULL) ||
		!xrtPoolPageIndex(&tPage, pValue, &iIndex) ||
		!xrtPoolPageFreeAt(&tPage, iIndex)
	) {
		xrtPoolPageUnit(&tPage);
		return 6;
	}
	pValue = (int*)xrtPoolPageAlloc(&tPage);
	if ( (pValue == NULL) || !xrtPoolPageFree(&tPage, pValue) ) {
		xrtPoolPageUnit(&tPage);
		return 7;
	}
	if (
		(xrtPoolPageAlloc(&tPage) == NULL) ||
		(xrtPoolPageCalloc(&tPage) == NULL) ||
		(xrtPoolPageReset(&tPage) != 2)
	) {
		xrtPoolPageUnit(&tPage);
		return 8;
	}
	xrtPoolPageGetInfo(&tPage, &tInfo);
	printf(
		"stride=%zu alignment=%zu live=%zu capacity=%zu\n",
		tInfo.Stride,
		tInfo.Alignment,
		tInfo.LiveCount,
		tInfo.Capacity
	);
	xrtPoolPageUnit(&tPage);

	/* 显式布局避免大对象仍按默认 256 槽整页预留。 */
	pCompact = xrtPoolPageCreateLayout(8192, 64, 4);
	if (
		(pCompact == NULL) ||
		(pCompact->Capacity != 4) ||
		(pCompact->MemorySize > (8192u * 4u))
	) {
		xrtPoolPageDestroy(pCompact);
		return 9;
	}
	xrtPoolPageDestroy(pCompact);

	/* Create/Destroy 适合不把页结构嵌入其他对象的场景。 */
	pCreated = xrtPoolPageCreate(sizeof(uint64));
	if ( pCreated == NULL ) {
		return 10;
	}
	xrtPoolPageDestroy(pCreated);
	return 0;
}
