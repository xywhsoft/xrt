/*
 * 范例：memory/pool —— 固定对象池：分页增长、LIFO 复用与标记回收
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPoolInitAligned   按对象大小+对齐初始化（每页自动算容量）
 *   xrtPoolCalloc/Alloc/Free   取放对象槽（Free 即时入空闲表）
 *   xrtPoolOwns          指针是否属于本池（防外来指针误释放）
 *   xrtPoolVisit         按槽序遍历活动对象
 *   xrtPoolMark/Sweep/FreeMarked   标记-清除 / 标记-释放两种回收
 *   xrtPoolSetRetain / Trim / Reset   空页保留数 / 归还空页 / 批量释放
 *   xrtPoolCreate / CreateLayout     堆创建（可显式指定每页槽容量）
 *   XRT_POOL_PAGE_BYTES_DEFAULT      默认页大小上限（约 64KB）
 * 模块宏：XRT_MODULE_MEMORY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/pool/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   page_capacity=256 pages=2 empty=2 live=0 peak=300 visited=300
 *
 * 固定池模型：等大对象 → 按"槽"分配，页满自动加页；
 *   Free 的槽进 LIFO 空闲表——下一次 Alloc 拿到的是最近释放的槽
 *   （本例验证 pReused == pReleased），缓存还热。
 * 两种标记回收的语义方向：
 *   Sweep   保留标记、回收未标记（批量处理器：标记存活者）；
 *   FreeMarked  释放标记、保留其余（引用计数归零者标记后放回）。
 */

#include <stdio.h>

#include <xrt.h>


/* 固定大小任务对象：32 字节（4 + 28），池按此步长切槽。 */
typedef struct examplejob {
	uint32 Id;
	char Name[28];
} examplejob;



/* 访问器：按槽下标顺序回调；本例同时校验下标连续性。 */
static bool exampleVisitJob(ptr pObject, size_t iIndex, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	if ( (pObject == NULL) || (iIndex != *pCount) ) {
		return false;
	}
	(*pCount)++;
	return true;
}



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

	/* 32 字节对齐：适合 SIMD 或缓存行对齐敏感的对象。 */
	if ( !xrtPoolInitAligned(&tPool, sizeof(examplejob), 32) ) {
		return 1;
	}

	/* 300 个对象触发跨页增长（每页约 256 槽 → 2 页）。 */
	for ( size_t i = 0; i < 300; i++ ) {
		arrJob[i] = (examplejob*)xrtPoolCalloc(&tPool);
		if ( arrJob[i] == NULL ) {
			xrtPoolUnit(&tPool);
			return 2;
		}
		arrJob[i]->Id = (uint32)i;
	}

	/* LIFO 复用验证：释放 10 号槽后立即取回——必须是同一个地址。 */
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

	/* 归属自检 + 遍历计数：300 个全部在池且下标连续。 */
	if ( !xrtPoolOwns(&tPool, pReused) ) {
		xrtPoolUnit(&tPool);
		return 5;
	}
	if ( xrtPoolVisit(&tPool, exampleVisitJob, &iVisited) != 300 ) {
		xrtPoolUnit(&tPool);
		return 6;
	}

	/* Sweep：只标记 0 号 → 回收其余 299 个。 */
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolSweep(&tPool) != 299) ) {
		xrtPoolUnit(&tPool);
		return 7;
	}

	/* FreeMarked：标记 0 号 → 释放它（与 Sweep 方向相反）。 */
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolFreeMarked(&tPool) != 1) ) {
		xrtPoolUnit(&tPool);
		return 8;
	}

	/*
	 * 空页策略：Trim(0) 先还掉全部空页，
	 * SetRetain(2) 设定 Reset 后保留 2 页备复用——
	 * 高峰过后留两页热内存，下批任务零页分配直接开跑。
	 */
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

	/* Reset 批量释放全部 300 个，保留 2 个空页（见输出 empty=2）。 */
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

	/*
	 * 大对象的自动布局：8KB 对象时每页只放 8 个，
	 * 页总大小仍受 XRT_POOL_PAGE_BYTES_DEFAULT（约 64KB）约束——
	 * 不会为一个大对象预留整页默认容量。
	 */
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

	/* 显式布局：每页恰好 128 槽（容量可查证）。 */
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

