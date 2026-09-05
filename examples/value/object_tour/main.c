/*
 * 范例：value/object_tour —— 对象与整数键映射全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   【对象】   ObjectSet（借引用）/ ObjectHas / ObjectRemove
 *              ObjectTake（按键移交值）
 *   【IntMap】 IntMapSet / IntMapHas / IntMapEdit（就地改）
 *              IntMapRemove / IntMapSetTake / IntMapTrim
 *   【标量】   xrtValueUInt / xrtValueGetUInt
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/value/object_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   has=1 removed=1 taken=v2
 *   edit=100 set=9 has=9 trim=0
 *
 * ObjectSet 借引用（源仍归调用方）；ObjectTake 把键对应值
 *   的所有权移交给调用方（键随之删除）。IntMapEdit 返回
 *   可写值槽——先取槽再改字段是"就地修改"的标准姿势。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xvalue* pObject = xrtValueObject();
	xvalue* pMap = xrtValueIntMap();
	xvalue* pTaken;
	xvalue* pSlot;
	uint64 uValue = 0;

	/* 对象：Set 借引用写入 → Has 验证 → Remove 删除。 */
	{
		xvalue* pOwned = xrtValueString(SV("v1"));

		(void)xrtValueObjectSet(pObject, SV("k"), pOwned);
		xrtValueRelease(pOwned);
	}
	printf("has=%d ", xrtValueObjectHas(pObject, SV("k")) ? 1 : 0);
	printf("removed=%d ", xrtValueObjectRemove(pObject, SV("k")) ? 1 : 0);

	/* ObjectTake：再写一键并移交取出。 */
	{
		xvalue* pOwned = xrtValueString(SV("v2"));

		(void)xrtValueObjectSet(pObject, SV("k2"), pOwned);
		xrtValueRelease(pOwned);
	}
	pTaken = xrtValueObjectTake(pObject, SV("k2"));
	if ( pTaken != NULL ) {
		xstrview Text;

		(void)xrtValueGetString(pTaken, &Text);
		printf("taken=%.*s\n", (int)Text.Size, Text.Data);
		xrtValueRelease(pTaken);
	}

	/* UInt 标量往返。 */
	{
		xvalue* pU = xrtValueUInt(UINT64_C(4294967296));

		(void)xrtValueGetUInt(pU, &uValue);
		printf("uint=%llu\n", (unsigned long long)uValue);
		xrtValueRelease(pU);
	}

	/* IntMap：Set 借引用 → Edit 就地改 → Has/Remove/SetTake/Trim。 */
	{
		xvalue* pOwned = xrtValueInt(5);

		(void)xrtValueIntMapSet(pMap, 1, pOwned);
		xrtValueRelease(pOwned);
	}
	/* Set 已存在键 = 替换（借引用）；Edit 取可写槽就地改（容器场景）。 */
	{
		xvalue* pOwned = xrtValueInt(100);

		(void)xrtValueIntMapSet(pMap, 1, pOwned);
		xrtValueRelease(pOwned);
	}
	{
		int64 iValue = 0;

		(void)xrtValueGetInt(xrtValueIntMapGet(pMap, 1), &iValue);
		printf("replaced=%lld ", (long long)iValue);
	}
	{
		/* Edit 只服务子容器（COW 分离后可变）；标量报类型错误（NULL）。 */
		xvalue* pChild = xrtValueArray();

		(void)xrtValueArrayAppendNew(pChild, xrtValueInt(7));
		(void)xrtValueIntMapSetNew(pMap, 3, pChild);
		pSlot = xrtValueIntMapEdit(pMap, 3);
		printf("edit-borrow=%d ", pSlot != NULL ? 1 : 0);
		(void)xrtValueArrayAppendNew(pSlot, xrtValueInt(8));  /* 就地改 */
		printf("edit-count=%zu ", xrtValueCount(xrtValueIntMapGet(pMap, 3)));
	}
	{
		xvalue* pOwned = xrtValueInt(9);

		(void)xrtValueIntMapSetTake(pMap, 2, &pOwned);       /* 移交 */
	}
	printf("set=%d ", xrtValueIntMapHas(pMap, 2) ? 1 : 0);
	printf("has9=%d ", xrtValueIntMapHas(pMap, 2) ? 1 : 0);
	(void)xrtValueIntMapRemove(pMap, 1);
	printf("trim=%zu\n", xrtValueIntMapTrim(pMap, 0));

	xrtValueRelease(pMap);
	xrtValueRelease(pObject);
	return 0;
}
