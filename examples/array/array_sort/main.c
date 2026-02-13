/**
 * @file main.c
 * @brief Array Sort Example - xrtArraySort and xrtPtrArraySort
 *        数组排序示例 - xrtArraySort 和 xrtPtrArraySort
 * 
 * This example demonstrates:
 * - Sorting struct arrays with comparison function
 * - Sorting pointer arrays with comparison function
 * 
 * 本示例演示：
 * - 使用比较函数对结构体数组排序
 * - 使用比较函数对指针数组排序
 * 
 * Build: tcc main.c -o ../../bin/array_array_sort.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	char szName[32];
} Person;

int compare_persons(const void* pA, const void* pB)
{
	const Person* p1 = (const Person*)pA;
	const Person* p2 = (const Person*)pB;
	return p1->nID - p2->nID;
}

int compare_int_ptrs(const void* pA, const void* pB)
{
	int* p1 = *(int**)pA;
	int* p2 = *(int**)pB;
	return *p1 - *p2;
}

void test_struct_array_sort(void)
{
	printf("=== Test: Struct Array Sort ===\n");
	printf("=== 测试：结构体数组排序 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Person));
	
	Person people[] = {{5, "Alice"}, {2, "Bob"}, {8, "Charlie"}, {1, "David"}, {3, "Eve"}};
	int nCount = sizeof(people) / sizeof(people[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		uint32 pos = xrtArrayAppend(pArr, 1);
		Person* pPerson = (Person*)xrtArrayGet(pArr, pos);
		pPerson->nID = people[i].nID;
		strcpy(pPerson->szName, people[i].szName);
	}
	
	printf("Before sorting (by ID):\n");
	printf("排序前（按ID）:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		Person* pPerson = (Person*)xrtArrayGet(pArr, i);
		printf("  [%u] ID=%d, Name=%s\n", i, pPerson->nID, pPerson->szName);
	}
	
	xrtArraySort(pArr, compare_persons);
	
	printf("After sorting (by ID ascending):\n");
	printf("排序后（按ID升序）:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		Person* pPerson = (Person*)xrtArrayGet(pArr, i);
		printf("  [%u] ID=%d, Name=%s\n", i, pPerson->nID, pPerson->szName);
	}
	
	xrtArrayDestroy(pArr);
}

void test_ptr_array_sort(void)
{
	printf("\n=== Test: Pointer Array Sort ===\n");
	printf("=== 测试：指针数组排序 ===\n");
	
	xparray pArr = xrtPtrArrayCreate();
	
	int vals[] = {50, 20, 80, 10, 30, 60, 40};
	int nCount = sizeof(vals) / sizeof(vals[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		xrtPtrArrayAppend(pArr, &vals[i]);
	}
	
	printf("Before sorting:\n");
	printf("排序前:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArraySort(pArr, compare_int_ptrs);
	
	printf("After sorting (ascending):\n");
	printf("排序后（升序）:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayDestroy(pArr);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Array Sort Example / 数组排序示例\n");
	printf("========================================\n");
	
	test_struct_array_sort();
	test_ptr_array_sort();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
