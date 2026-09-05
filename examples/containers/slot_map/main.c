/*
 * 范例：containers/slot_map —— 带代际的稳定下标容器（防句柄复用误命中）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSlotMapInit      初始化（元素为 ptr，槽位自动增长）
 *   xrtSlotMapInsert    插入并返回 xslot 句柄（下标 + 代际）
 *   xrtSlotMapRemove    按句柄移除（代际不符会拒绝）
 *   xrtSlotMapContains  判断句柄是否仍然有效
 *   xrtSlotIndex        从句柄中取出裸下标
 * 模块宏：XRT_MODULE_SLOT_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/slot_map/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   old=4294967297 replacement=8589934593 same-index=yes stale-valid=no
 *
 * 代际机制解决"下标句柄"的经典陷阱：
 *   槽 1 的旧对象被删、新对象复用同一槽后，旧句柄若只存下标，
 *   会错误地"命中"新对象。xslot 把下标与代际计数打包——
 *   槽每被复用一次代际 +1，旧句柄代际不符即失效。
 * 这是连接表、资源池、ECS 实体管理的标准底座。
 */

#include <stdio.h>
#include <xrt.h>



/* 连接对象仅用于演示稳定代际句柄。 */
typedef struct connection {
	int ID;
} connection;



int main(void)
{
	xslotmap tConnections;
	connection tFirst = { 100 };
	connection tSecond = { 200 };
	connection tReplacement = { 300 };
	xslot First;
	xslot Second;
	xslot Replacement;

	if ( !xrtSlotMapInit(&tConnections) ) {
		return 1;
	}

	/* 插入两个连接：得到两个不同下标的句柄。 */
	First = xrtSlotMapInsert(&tConnections, &tFirst);
	Second = xrtSlotMapInsert(&tConnections, &tSecond);
	if ( (First == XRT_SLOT_INVALID) || (Second == XRT_SLOT_INVALID) ) {
		xrtSlotMapUnit(&tConnections);
		return 2;
	}

	/* 删除第一个连接：末参可回调被摘除的指针，这里传 NULL 不需要。 */
	if ( !xrtSlotMapRemove(&tConnections, First, NULL) ) {
		xrtSlotMapUnit(&tConnections);
		return 3;
	}

	/*
	 * 插入替补连接：空闲槽 1 被复用（same-index=yes）。
	 * 句柄编码为 代际<<32|下标：
	 *   old=0x1_00000001（代际 1，下标 1）
	 *   new=0x2_00000001（代际 2，下标 1）——代已递增，句柄不同。
	 */
	Replacement = xrtSlotMapInsert(&tConnections, &tReplacement);
	if ( Replacement == XRT_SLOT_INVALID ) {
		xrtSlotMapUnit(&tConnections);
		return 4;
	}

	/*
	 * 两项验证：
	 *   same-index   —— 新旧句柄裸下标相同（槽被复用）；
	 *   stale-valid  —— 旧句柄 Contains 判定失效（no），
	 *                   它绝不会误指向替补连接。
	 */
	printf(
		"old=%llu replacement=%llu same-index=%s stale-valid=%s\n",
		(unsigned long long)First,
		(unsigned long long)Replacement,
		xrtSlotIndex(First) == xrtSlotIndex(Replacement) ? "yes" : "no",
		xrtSlotMapContains(&tConnections, First) ? "yes" : "no"
	);
	xrtSlotMapUnit(&tConnections);
	return 0;
}
