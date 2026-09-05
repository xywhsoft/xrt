/*
 * 范例：containers/map_tour —— 字节键哈希映射全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   【创建/容量】 xrtMapCreate / CreateAligned / Destroy
 *                xrtMapInitAligned / xrtMapCapacity / xrtMapReserve / Trim
 *   【读写】     xrtMapSet / Get / ConstGet / GetPtr / SetPtr / TakePtr
 *                xrtMapGetOrInit / Has / Count / StoredKey
 *   【移除】     xrtMapRemove / Take / SetDrop / Clear
 *   【迭代】     xrtMapIterRBegin（插入序逆序）
 *   【访问器】   xrtMapVisit（回调式，可提前停止）
 *   【键策略】   xrtMapSetKeyPolicy（空映射装自定义哈希/相等）
 * 模块宏：XRT_MODULE_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/containers/map_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   count=2 cap>=12 ptr=0000000000000010 stored=a
 *   get-or-init=1 new=1 remove=1
 *   riter: b a
 *   visit: a b
 *   took=yes has-after=0
 *   cleared=0 trimmed-cap=0
 *
 * Ptr 族约定：sizeof(ptr) 值映射的指针友好入口——
 *   GetPtr 空指针与缺失键都用 Has 区分；
 *   TakePtr 移交指针且不调值释放器（SetDrop 装的 Drop
 *   只在 Clear/Destroy/Remove 时触发）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) ((xbytesview){ x, sizeof(x) - 1u })

/* 访问器：打印每个键；返回 false 可提前停止（本例走满）。 */
static bool visitPair(xbytesview Key, ptr pValue, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	(void)pValue;
	printf("%.*s ", (int)Key.Size, (const char*)Key.Data);
	(*pCount)++;
	return true;
}

/* 最小 init 回调：新槽已被零初始化，直接确认。 */
static bool initSlot(xbytesview Key, ptr pValue, ptr pUserData)
{
	(void)Key; (void)pValue; (void)pUserData;
	return true;
}

int main(void)
{
	xmap* pMap = xrtMapCreate(sizeof(ptr));
	xmapiter Iter;
	xbytesview Key;
	ptr pSlot;
	size_t iVisited = 0;

	if ( pMap == NULL ) {
		return 1;
	}

	/* Set 写入两键（值是 uint64 槽）；Capacity 随增长 >= 键数。 */
	{
		uint64 iZero = 0;

		if ( !xrtMapSet(pMap, SV("a"), &iZero) ) {
			xrtMapDestroy(pMap);
			return 2;
		}
		(void)xrtMapReserve(pMap, 8u);
		if ( !xrtMapSet(pMap, SV("b"), &iZero) ) {
			xrtMapDestroy(pMap);
			return 3;
		}
	}
	printf("count=%zu cap>=%zu ", xrtMapCount(pMap), xrtMapCapacity(pMap));

	/* Get：按名取可写值槽（未找到是正常结果，返回 NULL）。 */
	printf("get=%d\n"
		, xrtMapGet(pMap, SV("b")) != NULL ? 1 : 0);

	/* Ptr 族读写 + StoredKey：值是指针时免去 ptr 装拆样板。 */
	(void)xrtMapSetPtr(pMap, SV("a"), (ptr)0x10u);
	printf("ptr=%p", (void*)xrtMapGetPtr(pMap, SV("a")));
	if ( xrtMapStoredKey(pMap, SV("a"), &Key) ) {
		printf(" stored=%.*s", (int)Key.Size, (const char*)Key.Data);
	}
	printf("\n");

	/* ConstGet：const 映射只读面（打印存在即证明）。 */
	printf("const-get=%d\n",
		xrtMapConstGet(pMap, SV("b")) != NULL ? 1 : 0);

	/* 逆序迭代：插入序 a,b → 输出 b,a。 */
	(void)xrtMapIterRBegin(pMap, &Iter);
	printf("riter: ");
	while ( xrtMapIterNext(&Iter, &Key) != NULL ) {
		printf("%.*s ", (int)Key.Size, (const char*)Key.Data);
	}
	printf("\n");

	/* Visit 回调式访问。 */
	printf("visit: ");
	(void)xrtMapVisit(pMap, visitPair, &iVisited);
	printf("\n");

	/* TakePtr：移交指针后键消失（Has 验证）。 */
	{
		ptr pTaken = NULL;
		bool bTook = xrtMapTakePtr(pMap, SV("a"), &pTaken);

		printf("took=%s has-after=%d\n", bTook ? "yes" : "no",
			xrtMapHas(pMap, SV("a")) ? 1 : 0);
	}

	/* GetOrInit：缺失键原子复制键并原位初始化（配 init 回调）；
	 * Remove：删除并调用 Drop（本例未装 Drop，仅删除）。 */
	{
		bool bNew = false;

		pSlot = xrtMapGetOrInit(pMap, SV("c"), initSlot, NULL, &bNew);
		printf("get-or-init=%d new=%d\n"
		,
			pSlot != NULL ? 1 : 0, bNew ? 1 : 0);
		printf("remove=%d\n"
		, xrtMapRemove(pMap, SV("c")) ? 1 : 0);
	}

	/* Clear 保留桶；Trim 收缩到最小。 */
	xrtMapClear(pMap);
	printf("cleared=%zu ", xrtMapCount(pMap));
	(void)xrtMapTrim(pMap);
	printf("trimmed-cap=%zu\n", xrtMapCapacity(pMap));
	xrtMapDestroy(pMap);

	/* 键策略 + 对齐创建（堆/栈两种）+ Drop 释放器一次演示。 */
	{
		xmap* pAlignedHeap = xrtMapCreateAligned(sizeof(uint64), 64u);
		xmap Aligned;

		if ( pAlignedHeap == NULL ) {
			return 4;
		}
		xrtMapDestroy(pAlignedHeap);
		if ( !xrtMapInitAligned(&Aligned, sizeof(uint64), 64u) ) {
			return 4;
		}
		/* SetDrop：装值释放器（值含堆资源时 Clear/Destroy/Remove 逐值回调）。 */
		printf("set-drop-installed=%d\n"
		, xrtMapSetDrop(&Aligned, NULL, NULL) ? 1 : 0);
		/* 空映射才能装策略；两个 NULL 恢复默认（装了再卸）。 */
		if ( !xrtMapSetKeyPolicy(&Aligned, NULL, NULL, NULL) ) {
			xrtMapUnit(&Aligned);
			return 5;
		}
		/* Take：把值移出到调用方变量（值语义版的移交）。 */
		{
			uint64 iValue = 7;

			(void)xrtMapSet(&Aligned, SV("k"), &iValue);
			iValue = 0;
			(void)xrtMapTake(&Aligned, SV("k"), &iValue);
			printf("take-value=%llu\n", (unsigned long long)iValue);
		}
		xrtMapUnit(&Aligned);
	}
	return 0;
}
