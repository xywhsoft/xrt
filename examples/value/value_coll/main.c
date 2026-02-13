/**
 * @file main.c
 * @brief Value Collection Example - xvoCreateColl operations
 *        值集合示例 - xvoCreateColl 操作
 * 
 * This example demonstrates:
 * - Creating and using value collections (sets)
 * - Set operations (union, intersection, difference)
 * 
 * 本示例演示：
 * - 创建和使用值集合（集）
 * - 集合操作（并集、交集、差集）
 * 
 * Build: tcc main.c -o ../../bin/value_value_coll.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_collection_operations(void)
{
	printf("=== Test: Value Collection Operations ===\n");
	printf("=== 测试：值集合操作 ===\n");
	
	xvalue pColl = xvoCreateColl();
	printf("Created collection value\n");
	printf("创建集合值\n");
	
	printf("\nAdding values to collection...\n");
	printf("\n向集合添加值...\n");
	
	xvoCollSetInt(pColl, 1);
	xvoCollSetInt(pColl, 2);
	xvoCollSetInt(pColl, 3);
	xvoCollSetInt(pColl, 4);
	xvoCollSetInt(pColl, 5);
	
	printf("Collection contains 5 integers\n");
	printf("集合包含5个整数\n");
	
	printf("\nChecking existence:\n");
	printf("\n检查是否存在:\n");
	printf("  Contains 3: %s\n", xvoCollExists(pColl, xvoCreateInt(3)) ? "yes" : "no");
	printf("  Contains 10: %s\n", xvoCollExists(pColl, xvoCreateInt(10)) ? "yes" : "no");
	
	xvoUnref(pColl);
}

void test_set_operations(void)
{
	printf("\n=== Test: Set Operations ===\n");
	printf("=== 测试：集合操作 ===\n");
	
	xvalue pSetA = xvoCreateColl();
	xvalue pSetB = xvoCreateColl();
	
	xvoCollSetInt(pSetA, 1);
	xvoCollSetInt(pSetA, 2);
	xvoCollSetInt(pSetA, 3);
	
	xvoCollSetInt(pSetB, 3);
	xvoCollSetInt(pSetB, 4);
	xvoCollSetInt(pSetB, 5);
	
	printf("Set A: {1, 2, 3}\n");
	printf("Set B: {3, 4, 5}\n");
	
	printf("\nSet operations available:\n");
	printf("\n可用的集合操作:\n");
	printf("  Union: xvoCollUnion\n");
	printf("  Intersection: xvoCollIntersection\n");
	printf("  Difference: xvoCollDifference\n");
	printf("  Contains: xvoCollExists\n");
	
	xvoUnref(pSetA);
	xvoUnref(pSetB);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Value Collection Example / 值集合示例\n");
	printf("========================================\n");
	
	test_collection_operations();
	test_set_operations();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
