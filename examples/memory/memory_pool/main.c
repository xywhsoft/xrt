#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 统计变长池访问器看到的活动块。 */
static bool exampleVisitMemory(
	ptr pMemory,
	size_t iSize,
	size_t iAlignment,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	if ( (pMemory == NULL) || (iSize == 0) || (iAlignment == 0) ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 演示尺寸类、大块、对齐、重分配、访问和两种标记回收。 */
int main(void)
{
	xmempool tPool;
	xmempool* pCreated;
	char* sText;
	bytes pPacket;
	bytes pAligned;
	xmempoolinfo tInfo;
	size_t iVisited = 0;

	if ( !xrtMemPoolInit(&tPool, 128) ) {
		return 1;
	}
	sText = (char*)xrtMemPoolCalloc(&tPool, 1, 24);
	pPacket = (bytes)xrtMemPoolAlloc(&tPool, 4096);
	pAligned = (bytes)xrtMemPoolAllocAligned(&tPool, 7, 64);
	if ( (sText == NULL) || (pPacket == NULL) || (pAligned == NULL) ) {
		xrtMemPoolUnit(&tPool);
		return 2;
	}
	memcpy(sText, "xrt memory pool", 16);
	sText = (char*)xrtMemPoolRealloc(&tPool, sText, 80);
	if ( (sText == NULL) || (strcmp(sText, "xrt memory pool") != 0) ) {
		xrtMemPoolUnit(&tPool);
		return 3;
	}
	if (
		!xrtMemPoolOwns(&tPool, sText) ||
		(xrtMemPoolSize(&tPool, sText) < 80) ||
		(((uintptr_t)pAligned % 64) != 0)
	) {
		xrtMemPoolUnit(&tPool);
		return 4;
	}
	if ( xrtMemPoolVisit(&tPool, exampleVisitMemory, &iVisited) != 3 ) {
		xrtMemPoolUnit(&tPool);
		return 5;
	}

	/* Sweep 保留文本与数据包，并回收未标记的对齐块。 */
	if (
		!xrtMemPoolMark(&tPool, sText) ||
		!xrtMemPoolMark(&tPool, pPacket) ||
		(xrtMemPoolSweep(&tPool) != 1)
	) {
		xrtMemPoolUnit(&tPool);
		return 6;
	}
	if (
		!xrtMemPoolMark(&tPool, pPacket) ||
		(xrtMemPoolFreeMarked(&tPool) != 1) ||
		!xrtMemPoolFree(&tPool, sText)
	) {
		xrtMemPoolUnit(&tPool);
		return 7;
	}

	/* Reset 批量释放活动块，Trim 释放尺寸类保留的空页。 */
	if (
		(xrtMemPoolAlloc(&tPool, 16) == NULL) ||
		(xrtMemPoolCalloc(&tPool, 2, 32) == NULL) ||
		(xrtMemPoolReset(&tPool) != 2)
	) {
		xrtMemPoolUnit(&tPool);
		return 8;
	}
	(void)xrtMemPoolTrim(&tPool, 0);
	xrtMemPoolGet(&tPool, &tInfo);
	printf(
		"visited=%zu small=%zu large=%zu live=%zu peak_bytes=%zu\n",
		iVisited,
		tInfo.SmallCount,
		tInfo.LargeCount,
		tInfo.LiveCount,
		tInfo.PeakBytes
	);
	xrtMemPoolUnit(&tPool);

	/* Create/Destroy 适合独立拥有变长池对象的场景。 */
	pCreated = xrtMemPoolCreate(0);
	if ( pCreated == NULL ) {
		return 9;
	}
	xrtMemPoolDestroy(pCreated);
	return 0;
}
