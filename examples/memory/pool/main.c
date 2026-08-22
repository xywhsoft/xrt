#include <stdio.h>

#include <xrt.h>



typedef struct examplejob {
	uint32 Id;
	char Name[28];
} examplejob;



/* 统计固定池访问器看到的活动任务。 */
static bool exampleVisitJob(ptr pObject, size_t iIndex, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	if ( (pObject == NULL) || (iIndex != *pCount) ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 演示固定池跨页增长、复用、访问、标记回收和空页保留。 */
int main(void)
{
	xpool tPool;
	xpool* pCreated;
	xpool* pCompact;
	examplejob* arrJob[300];
	examplejob* pReleased;
	examplejob* pReused;
	xpoolinfo tInfo;
	size_t iVisited = 0;

	if ( !xrtPoolInitAligned(&tPool, sizeof(examplejob), 32) ) {
		return 1;
	}
	for ( size_t i = 0; i < 300; i++ ) {
		arrJob[i] = (examplejob*)xrtPoolCalloc(&tPool);
		if ( arrJob[i] == NULL ) {
			xrtPoolUnit(&tPool);
			return 2;
		}
		arrJob[i]->Id = (uint32)i;
	}
	pReleased = arrJob[10];
	if ( !xrtPoolFree(&tPool, pReleased) ) {
		xrtPoolUnit(&tPool);
		return 3;
	}
	pReused = (examplejob*)xrtPoolAlloc(&tPool);
	if ( pReused != pReleased ) {
		xrtPoolUnit(&tPool);
		return 4;
	}
	pReused->Id = 10;
	if ( !xrtPoolOwns(&tPool, pReused) ) {
		xrtPoolUnit(&tPool);
		return 5;
	}
	if ( xrtPoolVisit(&tPool, exampleVisitJob, &iVisited) != 300 ) {
		xrtPoolUnit(&tPool);
		return 6;
	}

	/* Sweep 保留标记对象，FreeMarked 则释放标记对象。 */
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolSweep(&tPool) != 299) ) {
		xrtPoolUnit(&tPool);
		return 7;
	}
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolFreeMarked(&tPool) != 1) ) {
		xrtPoolUnit(&tPool);
		return 8;
	}

	/* 调整保留策略后，Reset 会留下两个可立即复用的空页。 */
	if ( xrtPoolTrim(&tPool, 0) == 0 ) {
		xrtPoolUnit(&tPool);
		return 9;
	}
	xrtPoolSetRetain(&tPool, 2);
	for ( size_t i = 0; i < 300; i++ ) {
		arrJob[i] = (examplejob*)xrtPoolCalloc(&tPool);
		if ( arrJob[i] == NULL ) {
			xrtPoolUnit(&tPool);
			return 10;
		}
	}
	if ( xrtPoolReset(&tPool) != 300 ) {
		xrtPoolUnit(&tPool);
		return 11;
	}
	xrtPoolGet(&tPool, &tInfo);
	printf(
		"page_capacity=%zu pages=%zu empty=%zu live=%zu peak=%zu visited=%zu\n",
		tInfo.PageCapacity,
		tInfo.PageCount,
		tInfo.EmptyPages,
		tInfo.LiveCount,
		tInfo.PeakCount,
		iVisited
	);
	xrtPoolUnit(&tPool);

	/* 自动布局把 8 KiB 对象页限制在约 64 KiB。 */
	pCompact = xrtPoolCreate(8192);
	if (
		(pCompact == NULL) ||
		(pCompact->PageCapacity != 8) ||
		(xrtPoolAlloc(pCompact) == NULL) ||
		(pCompact->Pages->MemorySize > XRT_POOL_PAGE_BYTES_DEFAULT)
	) {
		xrtPoolDestroy(pCompact);
		return 12;
	}
	xrtPoolDestroy(pCompact);

	/* Create/Destroy 适合独立拥有固定池对象的场景。 */
	pCreated = xrtPoolCreateLayout(sizeof(examplejob), 32, 128);
	if ( pCreated == NULL ) {
		return 13;
	}
	if ( pCreated->PageCapacity != 128 ) {
		xrtPoolDestroy(pCreated);
		return 14;
	}
	xrtPoolDestroy(pCreated);
	return 0;
}
