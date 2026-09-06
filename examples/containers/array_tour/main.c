/*
 * 范例：containers/array_tour —— 通用数组 + 指针数组全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【xarray 构建】  InitAligned / Create / CreateAligned / Destroy /
 *                    Clear / Reserve / Resize / Trim
 *   【xarray 访问】  ConstGet / Add（返回追加槽位指针）/
 *                    InsertSpace（中段腾位）
 *   【xarray 编辑】  Push / Insert / Set / Remove / RemoveSwap /
 *                    Pop / Swap / Reverse
 *   【xarray 查找】  Find（等值）/ FindBy（谓词比较）/ BSearch
 *   【xptrarray】    Create / Destroy / Clear / Reserve / Resize / Trim /
 *                    Data / ConstData / Get / Set /
 *                    Push / Append / Insert / InsertMany /
 *                    Remove / RemoveSwap / Pop / Swap / Reverse /
 *                    Sort / Find / Unit
 * 模块宏：XRT_MODULE_ARRAY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/containers/array_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   array: lifecycle+edit = 10 items ok
 *   array: search find(3)=2 findby(1)=0 miss=(none)
 *   ptr-array: 22 APIs tour ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* int32 元素比较（qsort 兼容：收元素指针）。 */
static int exampleCompareInt(const void* pLeft, const void* pRight)
{
	int iLeft = *(const int*)pLeft;
	int iRight = *(const int*)pRight;

	return (iLeft > iRight) - (iLeft < iRight);
}

/* 指针数组排序：元素是 ptr 槽位，比较解引用后的整数值。 */
static int exampleCompareValue(const void* pLeft, const void* pRight)
{
	intptr_t iLeft = (intptr_t)*(ptr const*)pLeft;
	intptr_t iRight = (intptr_t)*(ptr const*)pRight;

	return (iLeft > iRight) - (iLeft < iRight);
}

int main(void)
{
	int Values[10];
	int Out = 0;
	int i;

	/* ---- xarray：对齐内嵌形态 + 编辑族全流程 ---- */
	{
		xarray Array;
		int* pSlot;

		/* 对齐必须整除元素大小（int 只能 1/2/4）。 */
		if ( !xrtArrayInitAligned(&Array, sizeof(int),
			 sizeof(int)) ||
			!xrtArrayReserve(&Array, 8u) ) {
			return 1;
		}
		/* Add：返回追加槽位指针（调用方直接写入）。 */
		for ( i = 0; i < 4; ++i ) {
			pSlot = (int*)xrtArrayAdd(&Array, 1u);
			if ( pSlot == NULL ) {
				return 2;
			}
			*pSlot = i * 10;  /* 0,10,20,30 */
		}
		/* InsertSpace：在下标 2 腾出两个未初始化槽（返回插入位，
		 * 由调用方写入——不做零填充）。 */
		pSlot = (int*)xrtArrayInsertSpace(&Array, 2u, 2u);
		if ( (pSlot == NULL) || (Array.Count != 6u) ) {
			return 3;
		}
		pSlot[0] = 2;
		pSlot[1] = 2;
		/* Insert / Set：填入并覆盖。 */
		Values[0] = 5;
		if ( !xrtArrayInsert(&Array, 2u, Values, 1u) ||
			!xrtArraySet(&Array, 3u, Values) ) {
			return 4;
		}
		/* 现在序列：0,10,5,5,0,20,30。 */
		/* Remove：删去下标 2 的一个 → 0,10,5,0,20,30。 */
		if ( !xrtArrayRemove(&Array, 2u, 1u) ) {
			return 5;
		}
		/* RemoveSwap：末位交换到下标 1 → 0,30,5,0,20。 */
		if ( !xrtArrayRemoveSwap(&Array, 1u) ||
			(*(int*)xrtArrayConstGet(&Array, 1u) != 30) ||
			(Array.Count != 5u) ) {
			return 6;
		}
		/* Swap + Reverse：先交换首尾，再整体反转。 */
		if ( !xrtArraySwap(&Array, 0u, 4u) ||
			!xrtArrayReverse(&Array) ||
			(*(int*)xrtArrayConstGet(&Array, 0u) != 0) ) {
			return 7;
		}
		/* Pop：取走末位并核对。 */
		if ( !xrtArrayPop(&Array, &Out) || (Out != 20) ) {
			return 8;
		}
		/* Push 一项再 Clear：Count 归零、容量保留。 */
		if ( !xrtArrayPush(&Array, &Out) ) {
			return 9;
		}
		xrtArrayClear(&Array);  /* void 返回：Count 直接核对 */
		if ( Array.Count != 0u ) {
			return 9;
		}
		/* Resize 重设 3 项 + Trim 收缩空闲容量。 */
		if ( !xrtArrayResize(&Array, 3u) || (Array.Count != 3u) ||
			!xrtArrayTrim(&Array) ) {
			return 10;
		}
		printf("array: lifecycle+edit = 10 items ok\n");
		xrtArrayUnit(&Array);
	}

	/* ---- xarray：查找族（有序数据）+ 堆形态 ---- */
	{
		xarray* pArray = xrtArrayCreate(sizeof(int));

		if ( pArray == NULL ) {
			return 11;
		}
		Values[0] = 3;
		Values[1] = 1;
		Values[2] = 4;
		Values[3] = 1;
		Values[4] = 5;
		if ( !xrtArrayAppend(pArray, Values, 5u) ||
			!xrtArraySort(pArray, exampleCompareInt) ||
			/* 排序后：1,1,3,4,5；Values[0]=3 在下标 2。 */
			(xrtArrayFind(pArray, &Values[0]) != 2u) ||
			(xrtArrayBSearch(pArray, &Values[0],
				exampleCompareInt) != 2u) ) {
			xrtArrayDestroy(pArray);
			return 12;
		}
		/* FindBy：谓词式线性查找（key 与元素交给比较器），
		 * 等值比较下与 Find 一致——首项 1 在下标 0。 */
		if ( xrtArrayFindBy(pArray, &Values[1],
				exampleCompareInt) != 0u ) {
			xrtArrayDestroy(pArray);
			return 13;
		}
		printf("array: search find(3)=2 findby(1)=0");
		Values[5] = 9;
		printf(" miss=%s\n",
			xrtArrayFind(pArray, &Values[5]) == XRT_NPOS ?
			"(none)" : "err");
		if ( xrtArrayFind(pArray, &Values[5]) != XRT_NPOS ) {
			xrtArrayDestroy(pArray);
			return 14;
		}
		xrtArrayDestroy(pArray);
	}
	/* 对齐堆形态：CreateAligned + Destroy。 */
	{
		xarray* pAligned = xrtArrayCreateAligned(sizeof(int),
			sizeof(int));

		if ( (pAligned == NULL) ||
			!xrtArrayPush(pAligned, &Out) ) {
			return 15;
		}
		xrtArrayDestroy(pAligned);
	}

	/* ---- xptrarray：全接口 ---- */
	{
		xptrarray* pList = xrtPtrArrayCreate();
		ptr Batch[4];

		if ( (pList == NULL) || !xrtPtrArrayReserve(pList, 8u) ) {
			return 16;
		}
		if ( !xrtPtrArrayPush(pList, (ptr)30) ||
			!xrtPtrArrayPush(pList, (ptr)10) ||
			!xrtPtrArrayPush(pList, (ptr)20) ||
			(xrtPtrArrayGet(pList, 2u) != (ptr)20) ||
			!xrtPtrArraySet(pList, 2u, (ptr)25) ) {
			return 17;
		}
		Batch[0] = (ptr)5;
		Batch[1] = (ptr)15;
		if ( !xrtPtrArrayAppend(pList, Batch, 2u) ||
			!xrtPtrArrayInsert(pList, 1u, (ptr)12) ||
			!xrtPtrArrayInsertMany(pList, 1u, Batch, 2u) ) {
			return 18;
		}
		/* 现序列：30,5,15,12,10,25,5,15。 */
		if ( !xrtPtrArraySwap(pList, 0u, 5u) ||
			(xrtPtrArrayGet(pList, 0u) != (ptr)25) ||
			!xrtPtrArrayReverse(pList) ||
			(xrtPtrArrayGet(pList, 0u) != (ptr)15) ) {
			return 19;
		}
		if ( !xrtPtrArraySort(pList, exampleCompareValue) ||
			(xrtPtrArrayGet(pList, 0u) != (ptr)5) ||
			(xrtPtrArrayGet(pList, 7u) != (ptr)30) ) {
			return 20;
		}
		{
			ptr Slot = 0;

			/* 排序后尾部是最大值 30。 */
			if ( !xrtPtrArrayPop(pList, &Slot) ||
				(Slot != (ptr)30) ) {
				return 21;
			}
		}
		/* Data / ConstData：连续槽位视图。 */
		if ( (xrtPtrArrayData(pList) == NULL) ||
			(xrtPtrArrayConstData(pList)[0] != (ptr)5) ) {
			return 22;
		}
		/* Remove（保序删一段：去掉头部两个 5）与
		 * RemoveSwap（O(1) 换删：尾位换到头部）。 */
		if ( !xrtPtrArrayRemove(pList, 0u, 2u) ||
			(xrtPtrArrayGet(pList, 0u) != (ptr)10) ||
			!xrtPtrArrayRemoveSwap(pList, 0u) ||
			(pList->Count != 4u) ) {
			return 23;
		}
		/* PtrArrayFind 的参数即关键字值本身（非指向它的指针）：
		 * 序列 [25,12,15,15]，15 在下标 2。 */
		if ( xrtPtrArrayFind(pList, (const void*)(ptr)15) != 2u ) {
			return 24;
		}
		if ( !xrtPtrArrayResize(pList, 8u) ||
			!xrtPtrArrayTrim(pList) ) {
			return 25;
		}
		xrtPtrArrayClear(pList);  /* void 返回 */
		if ( pList->Count != 0u ) {
			return 25;
		}
		printf("ptr-array: 22 APIs tour ok\n");
		xrtPtrArrayDestroy(pList);
	}
	/* xptrarray 内嵌形态配对。 */
	{
		xptrarray Embedded;

		if ( !xrtPtrArrayInit(&Embedded) ) {
			return 26;
		}
		xrtPtrArrayUnit(&Embedded);
	}
	return 0;
}
