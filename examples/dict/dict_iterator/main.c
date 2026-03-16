/**
 * @file main.c
 * @brief Dictionary Iterator Example - xrtDictIterBegin/Next
 *        字典迭代器示例 - xrtDictIterBegin/Next
 * 
 * This example demonstrates:
 * - Iterating through dictionary entries
 * - Using iterator pattern for traversal
 * 
 * 本示例演示：
 * - 遍历字典条目
 * - 使用迭代器模式进行遍历
 * 
 * Build: tcc main.c -o ../../bin/dict_dict_iterator.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_dict_iterator(void)
{
	printf("=== Test: Dictionary Iterator ===\n");
	printf("=== 测试：字典迭代器 ===\n");
	
	xdict pDict = xrtDictCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	
	printf("Setting values...\n");
	printf("设置值...\n");
	
	char* keys[] = {"one", "two", "three", "four", "five"};
	int values[] = {1, 2, 3, 4, 5};
	int nCount = sizeof(keys) / sizeof(keys[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		int* pVal = (int*)xrtDictSet(pDict, keys[i], strlen(keys[i]), NULL);
		*pVal = values[i];
	}
	
	printf("Dictionary count: %u\n", xrtDictCount(pDict));
	printf("字典计数: %u\n", xrtDictCount(pDict));
	
	printf("\nIterating through dictionary:\n");
	printf("\n遍历字典:\n");
	
	xrtDictIterBegin(pDict);
	int* pVal;
	while ((pVal = (int*)xrtDictIterNext(pDict)) != NULL)
	{
		printf("  Value: %d\n", *pVal);
	}
	
	xrtDictDestroy(pDict);
}

void test_walk_function(void)
{
	printf("\n=== Test: Dictionary Walk Function ===\n");
	printf("=== 测试：字典遍历函数 ===\n");
	
	xdict pDict = xrtDictCreate(sizeof(double), XRT_OBJMODE_LOCAL);
	
	printf("Setting double values...\n");
	printf("设置双精度值...\n");
	
	double vals[] = {3.14, 2.718, 1.414, 1.732};
	char* keys[] = {"pi", "e", "sqrt2", "sqrt3"};
	int nCount = sizeof(vals) / sizeof(vals[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		double* pVal = (double*)xrtDictSet(pDict, keys[i], strlen(keys[i]), NULL);
		*pVal = vals[i];
	}
	
	printf("Dictionary count: %u\n", xrtDictCount(pDict));
	printf("字典计数: %u\n", xrtDictCount(pDict));
	
	xrtDictDestroy(pDict);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Dict Iterator Example / 字典迭代器示例\n");
	printf("========================================\n");
	
	test_dict_iterator();
	test_walk_function();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
