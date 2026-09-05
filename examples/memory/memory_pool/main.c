/*
 * 范例：memory/memory_pool —— 变长内存池：尺寸类/大块/对齐/标记回收
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMemPoolInit/Calloc/Alloc/AllocAligned/Realloc   池化分配族
 *   xrtMemPoolOwns / Size        归属判断与块大小查询
 *   xrtMemPoolVisit              遍历活动块（回调式）
 *   xrtMemPoolMark / Sweep       标记-清除回收（保留标记块）
 *   xrtMemPoolReset / Trim       批量释放 / 归还空页
 *   xrtMemPoolGet / Create       状态查询 / 堆分配创建
 * 模块宏：XRT_MODULE_MEMORY（POOL 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/memory_pool/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   visited=3 small=0 large=0 live=0 peak_bytes=4215
 *
 * 变长池的工作方式（与固定池 pool 区分）：
 *   小块（≤ 阈值）走尺寸类缓存——释放只是"挂回空闲表"，
 *   再分配同尺寸零系统调用；
 *   大块（本例 4096）直通后端但同样纳入归属/访问/统计。
 * 基准数据：32B 固定尺寸下 xrtMalloc 池化路径 2,943 万 ops/s，
 *   略快于直接 malloc（MEMORY_POOL_BENCH_20260316）。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 访问器：每块回调一次（指针/大小/对齐）；返回 false 终止遍历。 */
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



int main(void)
{
	xmempool tPool;          /* 栈句柄；128 为初始保留字节提示 */
	xmempool* pCreated;
	char* sText;
	bytes pPacket;
	bytes pAligned;
	xmempoolinfo tInfo;
	size_t iVisited = 0;

	if ( !xrtMemPoolInit(&tPool, 128) ) {
		return 1;
	}

	/* 三种分配形态：清零小块 / 4KB 大块 / 64 字节对齐块。 */
	sText = (char*)xrtMemPoolCalloc(&tPool, 1, 24);
	pPacket = (bytes)xrtMemPoolAlloc(&tPool, 4096);
	pAligned = (bytes)xrtMemPoolAllocAligned(&tPool, 7, 64);
	if ( (sText == NULL) || (pPacket == NULL) || (pAligned == NULL) ) {
		xrtMemPoolUnit(&tPool);
		return 2;
	}

	/* Realloc 扩容：内容保留（"xrt memory pool" 原样可读）。 */
	memcpy(sText, "xrt memory pool", 16);
	sText = (char*)xrtMemPoolRealloc(&tPool, sText, 80);
	if ( (sText == NULL) || (strcmp(sText, "xrt memory pool") != 0) ) {
		xrtMemPoolUnit(&tPool);
		return 3;
	}

	/* 三项自检：池归属 / 块大小 ≥ 80 / 对齐块地址 % 64 == 0。 */
	if (
		!xrtMemPoolOwns(&tPool, sText) ||
		(xrtMemPoolSize(&tPool, sText) < 80) ||
		(((uintptr_t)pAligned % 64) != 0)
	) {
		xrtMemPoolUnit(&tPool);
		return 4;
	}

	/* 访问器应数到 3 个活动块。 */
	if ( xrtMemPoolVisit(&tPool, exampleVisitMemory, &iVisited) != 3 ) {
		xrtMemPoolUnit(&tPool);
		return 5;
	}

	/*
	 * 标记-清除回收：Mark 两个要保留的块，
	 * Sweep 释放所有未标记块——返回 1（只回收了对齐块）。
	 * 与逐块 Free 相比：不必维护"该释放谁"的清单，
	 * 一次扫描解决，批量处理器的标准姿势。
	 */
	if (
		!xrtMemPoolMark(&tPool, sText) ||
		!xrtMemPoolMark(&tPool, pPacket) ||
		(xrtMemPoolSweep(&tPool) != 1)
	) {
		xrtMemPoolUnit(&tPool);
		return 6;
	}

	/*
	 * FreeMarked：与 Sweep 方向相反——标记者被释放（pPacket），
	 * 未标记者保留；随后手工释放 sText，为 Reset 段准备干净起点。
	 */
	if (
		!xrtMemPoolMark(&tPool, pPacket) ||
		(xrtMemPoolFreeMarked(&tPool) != 1) ||
		!xrtMemPoolFree(&tPool, sText)
	) {
		xrtMemPoolUnit(&tPool);
		return 7;
	}

	/* Reset 批量释放活动块（返回释放数）；Trim 把尺寸类空页还给系统。 */
	if (
		(xrtMemPoolAlloc(&tPool, 16) == NULL) ||
		(xrtMemPoolCalloc(&tPool, 2, 32) == NULL) ||
		(xrtMemPoolReset(&tPool) != 2)
	) {
		xrtMemPoolUnit(&tPool);
		return 8;
	}
	(void)xrtMemPoolTrim(&tPool, 0);

	/* 状态查询：reset 后 live 归零，峰值仍记录历史最高水位。 */
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

	/* Create/Destroy：池对象本身放堆上的场景（跨函数传递）。 */
	pCreated = xrtMemPoolCreate(0);
	if ( pCreated == NULL ) {
		return 9;
	}
	xrtMemPoolDestroy(pCreated);
	return 0;
}
