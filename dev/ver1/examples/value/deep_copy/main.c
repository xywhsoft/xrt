/**
 * @file main.c
 * @brief Deep Copy Example - xvoCopy/xvoDeepCopy
 *        深拷贝示例 - xvoCopy/xvoDeepCopy
 * 
 * This example demonstrates:
 * - Shallow copy vs deep copy
 * - Copying complex nested structures
 * 
 * 本示例演示：
 * - 浅拷贝与深拷贝
 * - 复制复杂嵌套结构
 * 
 * Build: tcc main.c -o ../../bin/value_deep_copy.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_shallow_vs_deep_copy(void)
{
	printf("=== Test: Shallow vs Deep Copy ===\n");
	printf("=== 测试：浅拷贝与深拷贝 ===\n");
	
	xvalue pOriginal = xvoCreateArray();
	xvoArrayAppendInt(pOriginal, 1);
	xvoArrayAppendInt(pOriginal, 2);
	xvoArrayAppendInt(pOriginal, 3);
	
	printf("Original array: [1, 2, 3]\n");
	printf("原始数组: [1, 2, 3]\n");
	
	printf("\nNote: xvoCopy creates shallow copy\n");
	printf("注意: xvoCopy 创建浅拷贝\n");
	printf("xvoDeepCopy creates completely independent copy\n");
	printf("xvoDeepCopy 创建完全独立的拷贝\n");
	
	xvalue pDeepCopy = xvoDeepCopy(pOriginal);
	printf("\nDeep copy created\n");
	printf("深拷贝已创建\n");
	
	xvoArraySetInt(pDeepCopy, 0, 999);
	printf("Modified deep copy: first element = 999\n");
	printf("修改深拷贝: 第一个元素 = 999\n");
	
	xvalue pVal = xvoArrayGetValue(pOriginal, 0);
	if (pVal)
	{
		printf("Original first element (unchanged): %lld\n", (long long)pVal->vInt);
		printf("原始第一个元素（未改变）: %lld\n", (long long)pVal->vInt);
		xvoUnref(pVal);
	}
	
	pVal = xvoArrayGetValue(pDeepCopy, 0);
	if (pVal)
	{
		printf("Deep copy first element: %lld\n", (long long)pVal->vInt);
		printf("深拷贝第一个元素: %lld\n", (long long)pVal->vInt);
		xvoUnref(pVal);
	}
	
	xvoUnref(pOriginal);
	xvoUnref(pDeepCopy);
}

void test_nested_copy(void)
{
	printf("\n=== Test: Nested Structure Copy ===\n");
	printf("=== 测试：嵌套结构拷贝 ===\n");
	
	xvalue pOuter = xvoCreateTable();
	xvalue pInner = xvoCreateArray();
	
	xvoArrayAppendInt(pInner, 10);
	xvoArrayAppendInt(pInner, 20);
	
	xvoTableSetValue(pOuter, "numbers", 7, pInner, true);
	printf("Created table with nested array: {numbers: [10, 20]}\n");
	printf("创建带嵌套数组的表: {numbers: [10, 20]}\n");
	
	xvoUnref(pInner);
	
	xvalue pDeepCopy = xvoDeepCopy(pOuter);
	printf("\nDeep copy of nested structure created\n");
	printf("嵌套结构的深拷贝已创建\n");
	printf("Both levels are independent copies\n");
	printf("两个层级都是独立的拷贝\n");
	
	xvoUnref(pOuter);
	xvoUnref(pDeepCopy);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Deep Copy Example / 深拷贝示例\n");
	printf("========================================\n");
	
	test_shallow_vs_deep_copy();
	test_nested_copy();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
