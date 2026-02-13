/*
 * XRT Example - Array Operations
 * XRT 范例 - 数组操作
 *
 * Description / 说明:
 *   EN: Demonstrates comprehensive array operations including dynamic resizing,
 *       sorting algorithms, searching techniques, insertion/deletion operations,
 *       and performance comparisons. Covers both basic and advanced array manipulation.
 *   CN: 演示全面的数组操作，包括动态调整大小、排序算法、搜索技术、
 *       插入/删除操作和性能比较。涵盖基本和高级数组操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_array_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_array_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Dynamic array implementation
 *   - Multiple sorting algorithms
 *   - Efficient search methods
 *   - Memory management techniques
 *   - Performance benchmarking
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Dynamic array structure
// 动态数组结构
typedef struct {
	int* piData;		// Array data / 数组数据
	size_t iSize;		// Current number of elements / 当前元素数量
	size_t iCapacity;	// Maximum capacity / 最大容量
} DynamicArray;

// Sorting algorithm types
// 排序算法类型
typedef enum {
	SORT_BUBBLE = 1,
	SORT_SELECTION,
	SORT_INSERTION,
	SORT_QUICK,
	SORT_MERGE,
	SORT_HEAP
} SortAlgorithm;

// Search algorithm types
// 搜索算法类型
typedef enum {
	SEARCH_LINEAR = 1,
	SEARCH_BINARY,
	SEARCH_INTERPOLATION
} SearchAlgorithm;

// Initialize dynamic array
// 初始化动态数组
DynamicArray* ArrayCreate(size_t iInitialCapacity)
{
	DynamicArray* pArr = xrtMalloc(sizeof(DynamicArray));
	pArr->piData = xrtMalloc(iInitialCapacity * sizeof(int));
	pArr->iSize = 0;
	pArr->iCapacity = iInitialCapacity;
	return pArr;
}

// Destroy dynamic array
// 销毁动态数组
void ArrayDestroy(DynamicArray* pArr)
{
	if ( pArr ) {
		if ( pArr->piData ) {
			xrtFree(pArr->piData);
		}
		xrtFree(pArr);
	}
}

// Resize array if needed
// 如需要则调整数组大小
void ArrayResize(DynamicArray* pArr, size_t iNewSize)
{
	if ( iNewSize > pArr->iCapacity ) {
		size_t iNewCapacity = pArr->iCapacity * 2;
		if ( iNewCapacity < iNewSize ) {
			iNewCapacity = iNewSize;
		}
		
		int* piNewData = xrtMalloc(iNewCapacity * sizeof(int));
		memcpy(piNewData, pArr->piData, pArr->iSize * sizeof(int));
		xrtFree(pArr->piData);
		
		pArr->piData = piNewData;
		pArr->iCapacity = iNewCapacity;
	}
}

// Add element to array
// 向数组添加元素
void ArrayPush(DynamicArray* pArr, int iValue)
{
	ArrayResize(pArr, pArr->iSize + 1);
	pArr->piData[pArr->iSize] = iValue;
	pArr->iSize++;
}

// Remove element from array
// 从数组移除元素
int ArrayPop(DynamicArray* pArr)
{
	if ( pArr->iSize == 0 ) {
		return 0;  // Array is empty / 数组为空
	}
	
	int iValue = pArr->piData[pArr->iSize - 1];
	pArr->iSize--;
	return iValue;
}

// Insert element at position
// 在位置插入元素
void ArrayInsert(DynamicArray* pArr, size_t iIndex, int iValue)
{
	if ( iIndex > pArr->iSize ) {
		iIndex = pArr->iSize;  // Append if index too large / 如果索引太大则追加
	}
	
	ArrayResize(pArr, pArr->iSize + 1);
	
	// Shift elements right
	// 向右移动元素
	for ( size_t i = pArr->iSize; i > iIndex; i-- ) {
		pArr->piData[i] = pArr->piData[i - 1];
	}
	
	pArr->piData[iIndex] = iValue;
	pArr->iSize++;
}

// Delete element at position
// 删除指定位置的元素
int ArrayDelete(DynamicArray* pArr, size_t iIndex)
{
	if ( iIndex >= pArr->iSize ) {
		return 0;  // Invalid index / 无效索引
	}
	
	int iValue = pArr->piData[iIndex];
	
	// Shift elements left
	// 向左移动元素
	for ( size_t i = iIndex; i < pArr->iSize - 1; i++ ) {
		pArr->piData[i] = pArr->piData[i + 1];
	}
	
	pArr->iSize--;
	return iValue;
}

// Bubble sort implementation
// 冒泡排序实现
void BubbleSort(int* piArr, size_t iSize)
{
	for ( size_t i = 0; i < iSize - 1; i++ ) {
		int iSwapped = 0;
		for ( size_t j = 0; j < iSize - i - 1; j++ ) {
			if ( piArr[j] > piArr[j + 1] ) {
				// Swap elements
				// 交换元素
				int iTemp = piArr[j];
				piArr[j] = piArr[j + 1];
				piArr[j + 1] = iTemp;
				iSwapped = 1;
			}
		}
		if ( !iSwapped ) break;  // Early termination / 提前终止
	}
}

// Selection sort implementation
// 选择排序实现
void SelectionSort(int* piArr, size_t iSize)
{
	for ( size_t i = 0; i < iSize - 1; i++ ) {
		size_t iMinIndex = i;
		for ( size_t j = i + 1; j < iSize; j++ ) {
			if ( piArr[j] < piArr[iMinIndex] ) {
				iMinIndex = j;
			}
		}
		
		// Swap minimum element to position i
		// 将最小元素交换到位置i
		if ( iMinIndex != i ) {
			int iTemp = piArr[i];
			piArr[i] = piArr[iMinIndex];
			piArr[iMinIndex] = iTemp;
		}
	}
}

// Insertion sort implementation
// 插入排序实现
void InsertionSort(int* piArr, size_t iSize)
{
	for ( size_t i = 1; i < iSize; i++ ) {
		int iKey = piArr[i];
		size_t j = i;
		
		// Move elements greater than key one position ahead
		// 将大于键的元素向前移动一个位置
		while ( j > 0 && piArr[j - 1] > iKey ) {
			piArr[j] = piArr[j - 1];
			j--;
		}
		
		piArr[j] = iKey;
	}
}

// Quick sort implementation
// 快速排序实现
void QuickSort(int* piArr, size_t iLow, size_t iHigh)
{
	if ( iLow < iHigh ) {
		// Partition array and get pivot index
		// 分割数组并获取枢轴索引
		size_t i = iLow - 1;
		int iPivot = piArr[iHigh];
		
		for ( size_t j = iLow; j < iHigh; j++ ) {
			if ( piArr[j] <= iPivot ) {
				i++;
				// Swap elements
				// 交换元素
				int iTemp = piArr[i];
				piArr[i] = piArr[j];
				piArr[j] = iTemp;
			}
		}
		
		// Place pivot in correct position
		// 将枢轴放在正确位置
		int iTemp = piArr[i + 1];
		piArr[i + 1] = piArr[iHigh];
		piArr[iHigh] = iTemp;
		
		size_t iPivotIndex = i + 1;
		
		// Recursively sort subarrays
		// 递归排序子数组
		QuickSort(piArr, iLow, iPivotIndex - 1);
		QuickSort(piArr, iPivotIndex + 1, iHigh);
	}
}

// Merge sort implementation
// 归并排序实现
void Merge(int* piArr, size_t iLeft, size_t iMid, size_t iRight)
{
	size_t iLeftSize = iMid - iLeft + 1;
	size_t iRightSize = iRight - iMid;
	
	// Create temporary arrays
	// 创建临时数组
	int* piLeftArr = xrtMalloc(iLeftSize * sizeof(int));
	int* piRightArr = xrtMalloc(iRightSize * sizeof(int));
	
	// Copy data to temporary arrays
	// 复制数据到临时数组
	memcpy(piLeftArr, &piArr[iLeft], iLeftSize * sizeof(int));
	memcpy(piRightArr, &piArr[iMid + 1], iRightSize * sizeof(int));
	
	// Merge temporary arrays back
	// 将临时数组合并回去
	size_t i = 0, j = 0, k = iLeft;
	while ( i < iLeftSize && j < iRightSize ) {
		if ( piLeftArr[i] <= piRightArr[j] ) {
			piArr[k] = piLeftArr[i];
			i++;
		} else {
			piArr[k] = piRightArr[j];
			j++;
		}
		k++;
	}
	
	// Copy remaining elements
	// 复制剩余元素
	while ( i < iLeftSize ) {
		piArr[k] = piLeftArr[i];
		i++;
		k++;
	}
	
	while ( j < iRightSize ) {
		piArr[k] = piRightArr[j];
		j++;
		k++;
	}
	
	// Cleanup
	// 清理
	xrtFree(piLeftArr);
	xrtFree(piRightArr);
}

void MergeSort(int* piArr, size_t iLeft, size_t iRight)
{
	if ( iLeft < iRight ) {
		size_t iMid = iLeft + (iRight - iLeft) / 2;
		MergeSort(piArr, iLeft, iMid);
		MergeSort(piArr, iMid + 1, iRight);
		Merge(piArr, iLeft, iMid, iRight);
	}
}

// Linear search implementation
// 线性搜索实现
int LinearSearch(int* piArr, size_t iSize, int iTarget)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( piArr[i] == iTarget ) {
			return (int)i;  // Return index / 返回索引
		}
	}
	return -1;  // Not found / 未找到
}

// Binary search implementation
// 二分搜索实现
int BinarySearch(int* piArr, size_t iSize, int iTarget)
{
	size_t iLeft = 0;
	size_t iRight = iSize - 1;
	
	while ( iLeft <= iRight ) {
		size_t iMid = iLeft + (iRight - iLeft) / 2;
		
		if ( piArr[iMid] == iTarget ) {
			return (int)iMid;
		} else if ( piArr[iMid] < iTarget ) {
			iLeft = iMid + 1;
		} else {
			iRight = iMid - 1;
		}
	}
	
	return -1;  // Not found / 未找到
}

// Interpolation search implementation
// 插值搜索实现
int InterpolationSearch(int* piArr, size_t iSize, int iTarget)
{
	size_t iLeft = 0;
	size_t iRight = iSize - 1;
	
	while ( iLeft <= iRight && iTarget >= piArr[iLeft] && iTarget <= piArr[iRight] ) {
		if ( iLeft == iRight ) {
			if ( piArr[iLeft] == iTarget ) return (int)iLeft;
			return -1;
		}
		
		// Calculate probable position
		// 计算可能位置
		size_t iPos = iLeft + ((double)(iRight - iLeft) / (piArr[iRight] - piArr[iLeft])) * (iTarget - piArr[iLeft]);
		
		if ( piArr[iPos] == iTarget ) {
			return (int)iPos;
		} else if ( piArr[iPos] < iTarget ) {
			iLeft = iPos + 1;
		} else {
			iRight = iPos - 1;
		}
	}
	
	return -1;  // Not found / 未找到
}

// Sort array using specified algorithm
// 使用指定算法排序数组
void ArraySort(DynamicArray* pArr, SortAlgorithm eAlgorithm)
{
	switch ( eAlgorithm ) {
		case SORT_BUBBLE:
			BubbleSort(pArr->piData, pArr->iSize);
			break;
		case SORT_SELECTION:
			SelectionSort(pArr->piData, pArr->iSize);
			break;
		case SORT_INSERTION:
			InsertionSort(pArr->piData, pArr->iSize);
			break;
		case SORT_QUICK:
			QuickSort(pArr->piData, 0, pArr->iSize - 1);
			break;
		case SORT_MERGE:
			MergeSort(pArr->piData, 0, pArr->iSize - 1);
			break;
		case SORT_HEAP:
			// Heap sort would be implemented here
			// 堆排序将在此处实现
			break;
	}
}

// Search in array using specified algorithm
// 使用指定算法在数组中搜索
int ArraySearch(DynamicArray* pArr, int iTarget, SearchAlgorithm eAlgorithm)
{
	// For binary and interpolation search, array must be sorted
	// 对于二分和插值搜索，数组必须已排序
	switch ( eAlgorithm ) {
		case SEARCH_LINEAR:
			return LinearSearch(pArr->piData, pArr->iSize, iTarget);
		case SEARCH_BINARY:
			return BinarySearch(pArr->piData, pArr->iSize, iTarget);
		case SEARCH_INTERPOLATION:
			return InterpolationSearch(pArr->piData, pArr->iSize, iTarget);
		default:
			return -1;
	}
}

// Print array contents
// 打印数组内容
void ArrayPrint(DynamicArray* pArr, str sLabel)
{
	printf("%s: [", sLabel);
	for ( size_t i = 0; i < pArr->iSize; i++ ) {
		printf("%d", pArr->piData[i]);
		if ( i < pArr->iSize - 1 ) {
			printf(", ");
		}
	}
	printf("] (size: %zu)\n", pArr->iSize);
}

// Test array operations
// 测试数组操作
void TestArrayOperations()
{
	printf("=== Array Operations Test ===\n");
	printf("=== 数组操作测试 ===\n");
	
	// Create array
	// 创建数组
	DynamicArray* pArr = ArrayCreate(5);
	
	// Test basic operations
	// 测试基本操作
	printf("Testing basic operations:\n");
	
	ArrayPush(pArr, 10);
	ArrayPush(pArr, 20);
	ArrayPush(pArr, 30);
	ArrayPrint(pArr, "After pushes");
	
	ArrayInsert(pArr, 1, 15);
	ArrayPrint(pArr, "After insert at index 1");
	
	int iDeleted = ArrayDelete(pArr, 2);
	printf("Deleted element: %d\n", iDeleted);
	ArrayPrint(pArr, "After delete at index 2");
	
	int iPopped = ArrayPop(pArr);
	printf("Popped element: %d\n", iPopped);
	ArrayPrint(pArr, "After pop");
	
	// Test sorting
	// 测试排序
	printf("\nTesting sorting algorithms:\n");
	
	// Reset array with unsorted data
	// 用未排序数据重置数组
	ArrayDestroy(pArr);
	pArr = ArrayCreate(10);
	
	int arrTestData[] = {64, 34, 25, 12, 22, 11, 90, 5};
	size_t iTestSize = sizeof(arrTestData) / sizeof(arrTestData[0]);
	
	for ( size_t i = 0; i < iTestSize; i++ ) {
		ArrayPush(pArr, arrTestData[i]);
	}
	
	ArrayPrint(pArr, "Original array");
	
	// Test different sorting algorithms
	// 测试不同排序算法
	DynamicArray* pBubbleArr = ArrayCreate(10);
	DynamicArray* pQuickArr = ArrayCreate(10);
	DynamicArray* pMergeArr = ArrayCreate(10);
	
	// Copy test data
	// 复制测试数据
	for ( size_t i = 0; i < iTestSize; i++ ) {
		ArrayPush(pBubbleArr, arrTestData[i]);
		ArrayPush(pQuickArr, arrTestData[i]);
		ArrayPush(pMergeArr, arrTestData[i]);
	}
	
	// Bubble sort
	// 冒泡排序
	clock_t clkStart = clock();
	ArraySort(pBubbleArr, SORT_BUBBLE);
	clock_t clkEnd = clock();
	double dBubbleTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	ArrayPrint(pBubbleArr, "Bubble sorted");
	printf("Bubble sort time: %.6f seconds\n", dBubbleTime);
	
	// Quick sort
	// 快速排序
	clkStart = clock();
	ArraySort(pQuickArr, SORT_QUICK);
	clkEnd = clock();
	double dQuickTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	ArrayPrint(pQuickArr, "Quick sorted");
	printf("Quick sort time: %.6f seconds\n", dQuickTime);
	
	// Merge sort
	// 归并排序
	clkStart = clock();
	ArraySort(pMergeArr, SORT_MERGE);
	clkEnd = clock();
	double dMergeTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	ArrayPrint(pMergeArr, "Merge sorted");
	printf("Merge sort time: %.6f seconds\n", dMergeTime);
	
	// Test searching
	// 测试搜索
	printf("\nTesting search algorithms:\n");
	ArrayPrint(pMergeArr, "Sorted array for search");
	
	int iSearchTarget = 22;
	printf("Searching for: %d\n", iSearchTarget);
	
	int iLinearResult = ArraySearch(pMergeArr, iSearchTarget, SEARCH_LINEAR);
	printf("Linear search result: %d\n", iLinearResult);
	
	int iBinaryResult = ArraySearch(pMergeArr, iSearchTarget, SEARCH_BINARY);
	printf("Binary search result: %d\n", iBinaryResult);
	
	int iInterpolationResult = ArraySearch(pMergeArr, iSearchTarget, SEARCH_INTERPOLATION);
	printf("Interpolation search result: %d\n", iInterpolationResult);
	
	// Performance comparison
	// 性能比较
	printf("\nPerformance comparison with larger dataset:\n");
	
	ArrayDestroy(pArr);
	pArr = ArrayCreate(1000);
	
	// Generate random data
	// 生成随机数据
	srand((unsigned int)time(NULL));
	for ( int i = 0; i < 1000; i++ ) {
		ArrayPush(pArr, rand() % 10000);
	}
	
	// Test sorting performance
	// 测试排序性能
	DynamicArray* pTestArr1 = ArrayCreate(1000);
	DynamicArray* pTestArr2 = ArrayCreate(1000);
	
	// Copy data
	// 复制数据
	for ( size_t i = 0; i < pArr->iSize; i++ ) {
		ArrayPush(pTestArr1, pArr->piData[i]);
		ArrayPush(pTestArr2, pArr->piData[i]);
	}
	
	// Selection sort
	// 选择排序
	clkStart = clock();
	ArraySort(pTestArr1, SORT_SELECTION);
	clkEnd = clock();
	double dSelectionTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("Selection sort (1000 elements): %.6f seconds\n", dSelectionTime);
	
	// Insertion sort
	// 插入排序
	clkStart = clock();
	ArraySort(pTestArr2, SORT_INSERTION);
	clkEnd = clock();
	double dInsertionTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("Insertion sort (1000 elements): %.6f seconds\n", dInsertionTime);
	
	// Cleanup
	// 清理
	ArrayDestroy(pArr);
	ArrayDestroy(pBubbleArr);
	ArrayDestroy(pQuickArr);
	ArrayDestroy(pMergeArr);
	ArrayDestroy(pTestArr1);
	ArrayDestroy(pTestArr2);
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("\n=== Edge Cases Test ===\n");
	printf("=== 边界情况测试 ===\n");
	
	// Test empty array
	// 测试空数组
	DynamicArray* pEmpty = ArrayCreate(0);
	ArrayPrint(pEmpty, "Empty array");
	
	// Test operations on empty array
	// 测试空数组上的操作
	int iResult = ArrayPop(pEmpty);
	printf("Pop from empty array: %d\n", iResult);
	
	iResult = ArrayDelete(pEmpty, 0);
	printf("Delete from empty array: %d\n", iResult);
	
	// Test single element array
	// 测试单元素数组
	DynamicArray* pSingle = ArrayCreate(1);
	ArrayPush(pSingle, 42);
	ArrayPrint(pSingle, "Single element array");
	
	ArrayDelete(pSingle, 0);
	ArrayPrint(pSingle, "After deleting single element");
	
	// Test large array
	// 测试大数组
	DynamicArray* pLarge = ArrayCreate(10000);
	
	// Add many elements
	// 添加许多元素
	for ( int i = 0; i < 5000; i++ ) {
		ArrayPush(pLarge, i);
	}
	
	printf("Large array size: %zu\n", pLarge->iSize);
	printf("Large array capacity: %zu\n", pLarge->iCapacity);
	
	// Test automatic resizing
	// 测试自动调整大小
	ArrayPush(pLarge, 9999);
	printf("After adding one more element:\n");
	printf("  Size: %zu\n", pLarge->iSize);
	printf("  Capacity: %zu\n", pLarge->iCapacity);
	
	// Cleanup
	// 清理
	ArrayDestroy(pEmpty);
	ArrayDestroy(pSingle);
	ArrayDestroy(pLarge);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Array Operations Demo\n");
	printf("XRT 数组操作演示\n");
	printf("=======================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestArrayOperations();
	TestEdgeCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}