/*
 * 范例：containers/int_map_tour —— 整数键映射全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   【创建】   xrtIntMapCreate / CreateAligned / InitAligned / Destroy
 *   【读写】   Set / Get / ConstGet / Has / Count / GetPtr / SetPtr
 *              GetOrInit（配 init 回调）
 *   【边界】   First / Last / LowerBound（>= 首键）/ UpperBound（> 首键）
 *   【迭代】   IterRBegin / IterFrom（从某键起）/ IterRFrom（逆向自某键）
 *   【访问器】 Visit（回调式，可修改值）
 *   【移除】   Remove / Take / TakePtr / SetDrop / Clear / Trim
 * 模块宏：XRT_MODULE_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/containers/int_map_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   first=10 last=40 lower(25)=30 upper(25)=30
 *   riter: 40 30 20 10
 *   from(20): 20 30 40
 *   rfrom(30): 30 20 10
 *   visit-hit=1 const=0
 *   took=20 removed=30 cleared=0
 *
 * 边界读法：LowerBound(25) = 第一个 >= 25 的键（30）；
 *   UpperBound(25) = 第一个 > 25 的键——同为 30（30 > 25）。
 *   两函数只在"查询键恰好存在"时分开：lower(30)=30，
 *   upper(30)=40——即 classic 的 [lower, upper) 区间约定。
 */

#include <stdio.h>
#include <xrt.h>

/* init 回调：确认新槽零初始化。 */
static bool initZero(int64 iKey, ptr pValue, ptr pUserData)
{
	(void)iKey; (void)pValue; (void)pUserData;
	return true;
}

/* 访问器：把值翻倍（演示"Visit 允许改值"）。 */
static bool doubleValue(int64 iKey, ptr pValue, ptr pUserData)
{
	int64* pHit = (int64*)pUserData;

	if ( iKey == 10 ) {
		*pHit = 1;
	}
	*(int64*)pValue *= 2;
	return true;
}

int main(void)
{
	static const int64 Keys[4] = { 10, 20, 30, 40 };
	xintmap* pMap = xrtIntMapCreate(sizeof(int64));
	int64 iKey = 0;
	int64 iValue = 0;
	int64 iHit = 0;
	xintmapiter Iter;
	ptr pSlot;

	if ( pMap == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < 4u; i++ ) {
		if ( !xrtIntMapSet(pMap, Keys[i], &iValue) ) {
			xrtIntMapDestroy(pMap);
			return 2;
		}
	}

	/* 边界四件套。 */
	(void)xrtIntMapFirst(pMap, &iKey);
	printf("first=%lld ", (long long)iKey);
	(void)xrtIntMapLast(pMap, &iKey);
	printf("last=%lld ", (long long)iKey);
	(void)xrtIntMapLowerBound(pMap, 25, &iKey);
	printf("lower(25)=%lld ", (long long)iKey);
	(void)xrtIntMapUpperBound(pMap, 25, &iKey);
	printf("upper(25)=%lld\n", (long long)iKey);

	/* 逆序 / 正向自 20 / 逆向自 30。 */
	(void)xrtIntMapIterRBegin(pMap, &Iter);
	printf("riter: ");
	while ( xrtIntMapIterNext(&Iter, &iKey) != NULL ) {
		printf("%lld ", (long long)iKey);
	}
	printf("\n");
	(void)xrtIntMapIterFrom(pMap, 20, &Iter);
	printf("from(20): ");
	while ( xrtIntMapIterNext(&Iter, &iKey) != NULL ) {
		printf("%lld ", (long long)iKey);
	}
	printf("\n");
	(void)xrtIntMapIterRFrom(pMap, 30, &Iter);
	printf("rfrom(30): ");
	while ( xrtIntMapIterNext(&Iter, &iKey) != NULL ) {
		printf("%lld ", (long long)iKey);
	}
	printf("\n");

	/* Visit 改值：把键 10 的值 0 翻成 20（命中标记）。 */
	(void)xrtIntMapVisit(pMap, doubleValue, &iHit);
	printf("visit-hit=%d const=%lld\n", (int)iHit,
		(long long)*(const int64*)xrtIntMapConstGet(pMap, 10));

	/* Get：按键取可写值槽（可改值）。 */
	if ( (pSlot = xrtIntMapGet(pMap, 20)) != NULL ) {
		*(int64*)pSlot = 99;
		printf("get-mutate=%lld\n"
		,
			(long long)*(const int64*)xrtIntMapConstGet(pMap, 20));
	}

	/* Take（值拷出）+ Remove（含 Drop）+ Clear/Trim。 */
	(void)xrtIntMapTake(pMap, 10, &iValue);
	printf("took=%lld ", (long long)iValue);
	printf("removed=%d ", xrtIntMapRemove(pMap, 30) ? 1 : 0);
	/* Ptr 族 + GetOrInit + Has + Count 联合自检。 */
	(void)xrtIntMapSetPtr(pMap, 50, (ptr)0xABu);
	printf("getptr=%p ", (void*)xrtIntMapGetPtr(pMap, 50));
	pSlot = xrtIntMapGetOrInit(pMap, 60, initZero, NULL, NULL);
	printf("orinit=%d has60=%d count=%zu\n",
		pSlot != NULL ? 1 : 0, xrtIntMapHas(pMap, 60) ? 1 : 0,
		xrtIntMapCount(pMap));
	xrtIntMapClear(pMap);
	printf("cleared=%zu ", xrtIntMapCount(pMap));
	(void)xrtIntMapTrim(pMap, 0);
	printf("trimmed\n");
	xrtIntMapDestroy(pMap);

	/* InitAligned 栈句柄 + CreateAligned 堆句柄 + TakePtr + SetDrop。 */
	{
		xintmap Aligned;
		xintmap* pHeap = xrtIntMapCreateAligned(sizeof(ptr), 64u);

		if ( pHeap == NULL ) {
			return 3;
		}
		xrtIntMapDestroy(pHeap);
		if ( !xrtIntMapInitAligned(&Aligned, sizeof(ptr), 64u) ) {
			return 3;
		}
		(void)xrtIntMapSetPtr(&Aligned, 1, (ptr)0x10u);
		{
			ptr pTaken = NULL;

			(void)xrtIntMapTakePtr(&Aligned, 1, &pTaken);
			printf("takeptr=%p\n", (void*)pTaken);
		}
		printf("setdrop=%d\n", xrtIntMapSetDrop(&Aligned, NULL, NULL) ? 1 : 0);
		xrtIntMapUnit(&Aligned);
	}
	return 0;
}
