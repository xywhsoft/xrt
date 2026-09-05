/*
 * 范例：containers/array —— 值类型动态数组：批量追加、排序与连续遍历
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtArrayInit    按元素大小初始化（值语义：元素直接内嵌存储）
 *   xrtArrayAppend  一次追加多个元素（memcpy 连续写入）
 *   xrtArraySort    原地排序（qsort 兼容的比较函数）
 *   xrtArrayGet     按下标取元素指针（越界返回 NULL）
 *   xrtArrayUnit    归还数组缓冲（Init 的逆操作）
 *   tRecords.Count  公开字段：当前元素数（无需函数查询）
 * 模块宏：XRT_MODULE_ARRAY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/array/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   id=102 priority=10
 *   id=103 priority=20
 *   id=101 priority=30
 *
 * 值语义与指针数组（ptr_array）的区别：
 *   xarray 把元素字节直接连续存放——缓存友好、零逐元素分配；
 *   需要存指针/多态对象时用 xptrarray（见 ptr_array 范例）。
 */

#include <stdio.h>

#include <xrt.h>



/* 示例记录：整条记录（而非指针）会直接放进数组内存。 */
typedef struct examplerecord {
	int ID;
	int Priority;
} examplerecord;



/*
 * qsort 兼容比较函数：按 Priority 升序。
 * 用 (a>b)-(a<b) 而不是 a-b，避免减法溢出破坏排序稳定性前提。
 */
static int exampleCompareRecord(const void* pLeft, const void* pRight)
{
	const examplerecord* pLeftRecord = (const examplerecord*)pLeft;
	const examplerecord* pRightRecord = (const examplerecord*)pRight;

	return (pLeftRecord->Priority > pRightRecord->Priority) -
		(pLeftRecord->Priority < pRightRecord->Priority);
}



int main(void)
{
	xarray tRecords;         /* 栈上句柄；缓冲由数组自己管理 */
	examplerecord pInput[] = {
		{ 101, 30 },
		{ 102, 10 },
		{ 103, 20 }
	};

	/* 初始化只需要元素大小——数组对元素类型完全泛型。 */
	if ( !xrtArrayInit(&tRecords, sizeof(examplerecord)) ) {
		return 1;
	}

	/* 一次追加 3 条（单次 memcpy + 可能的扩容），再原地排序。 */
	if (
		!xrtArrayAppend(&tRecords, pInput, 3) ||
		!xrtArraySort(&tRecords, exampleCompareRecord)
	) {
		xrtArrayUnit(&tRecords);
		return 2;
	}

	/*
	 * 遍历：Count 是公开字段；Get 返回元素在数组内存中的
	 * 直接指针（可读写）。存储连续，这段循环对缓存最友好。
	 */
	for ( size_t i = 0; i < tRecords.Count; i++ ) {
		examplerecord* pRecord = (examplerecord*)xrtArrayGet(&tRecords, i);

		printf("id=%d priority=%d\n", pRecord->ID, pRecord->Priority);
	}
	xrtArrayUnit(&tRecords);
	return 0;
}
