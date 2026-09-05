/*
 * 范例：memory/pool_page —— 单页固定池：零元数据、索引寻址的极致形态
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPoolPageInitAligned      单页初始化（对象大小 + 对齐）
 *   xrtPoolPageCalloc/Alloc/Free   槽位取放（带边界检查）
 *   xrtPoolPageIndex / Get      指针↔槽下标互转（O(1) 指针算术）
 *   xrtPoolPageFreeAt          按下标释放
 *   xrtPoolPageMark/Sweep/FreeMarked   两种标记回收（同 pool）
 *   xrtPoolPageReset           整页复位（返回清出的对象数）
 *   xrtPoolPageCreateLayout    显式布局创建（大对象不受默认 256 槽限制）
 * 模块宏：XRT_MODULE_MEMORY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/pool_page/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   stride=32 alignment=32 live=0 capacity=256
 *
 * 与多页 pool 的取舍：单页 = 一块连续内存 + 空闲表，
 *   无页链表、无跨页寻址——每个操作都是纯指针算术，
 *   嵌入式/热路径对象管理（AST 节点、消息帧）首选；
 *   容量到顶即止（本例 256 槽），需要自动增长用 pool。
 * 指针与下标互换的能力让"句柄序列化"成为可能：
 *   存下标而非指针，重启后按下标取回对象。
 */

#include <stdio.h>

#include <xrt.h>



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

	/* int 槽 + 32 对齐：Stride 会被垫到 32（见输出）。 */
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

	/* Sweep：标记 pKeep → 回收未标记的 pDrop（清出 1 个）。 */
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageSweep(&tPage) != 1) ) {
		xrtPoolPageUnit(&tPage);
		return 3;
	}

	/*
	 * 指针 ↔ 下标互转：Index 给出槽位号，
	 * Get(下标) 必须还原同一指针——句柄寻址的闭环验证。
	 */
	if (
		!xrtPoolPageOwns(&tPage, pKeep) ||
		!xrtPoolPageIndex(&tPage, pKeep, &iIndex) ||
		(xrtPoolPageGet(&tPage, iIndex) != pKeep)
	) {
		xrtPoolPageUnit(&tPage);
		return 4;
	}

	/* FreeMarked：与 Sweep 方向相反——释放标记者。 */
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageFreeMarked(&tPage) != 1) ) {
		xrtPoolPageUnit(&tPage);
		return 5;
	}

	/* 按下标释放：边界检查内建，越界下标直接拒绝。 */
	pValue = (int*)xrtPoolPageAlloc(&tPage);
	if (
		(pValue == NULL) ||
		!xrtPoolPageIndex(&tPage, pValue, &iIndex) ||
		!xrtPoolPageFreeAt(&tPage, iIndex)
	) {
		xrtPoolPageUnit(&tPage);
		return 6;
	}

	/* 按指针释放：内部同样做归属与边界校验。 */
	pValue = (int*)xrtPoolPageAlloc(&tPage);
	if ( (pValue == NULL) || !xrtPoolPageFree(&tPage, pValue) ) {
		xrtPoolPageUnit(&tPage);
		return 7;
	}

	/* Reset 整页复位：清出剩余 2 个活动对象，容量不变可复用。 */
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

	/*
	 * 显式布局：8KB 对象 × 4 槽——总内存恰好 32KB，
	 * 不会按默认 256 槽预留（那要 2MB）。大对象页必用显式布局。
	 */
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

	/* Create/Destroy：页对象放堆上的场景。 */
	pCreated = xrtPoolPageCreate(sizeof(uint64));
	if ( pCreated == NULL ) {
		return 10;
	}
	xrtPoolPageDestroy(pCreated);
	return 0;
}
