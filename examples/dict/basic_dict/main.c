/**
 * @file main.c
 * @brief Basic Dictionary Example - xrtDict operations
 *        基本字典示例 - xrtDict 操作
 * 
 * This example demonstrates:
 * - Creating and destroying dictionaries
 * - Setting and getting values
 * - Checking key existence
 * - Removing keys
 * 
 * 本示例演示：
 * - 创建和销毁字典
 * - 设置和获取值
 * - 检查键是否存在
 * - 删除键
 * 
 * Build: tcc main.c -o ../../bin/dict_basic_dict.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nAge;
	double dScore;
} PersonData;

void test_basic_dict(void)
{
	printf("=== Test: Basic Dictionary Operations ===\n");
	printf("=== 测试：基本字典操作 ===\n");
	
	xdict pDict = xrtDictCreate(sizeof(PersonData), XRT_OBJMODE_LOCAL);
	
	printf("Created dictionary with PersonData value size: %u\n", (uint32)sizeof(PersonData));
	printf("创建字典，PersonData值大小: %u\n", (uint32)sizeof(PersonData));
	
	printf("\nSetting values...\n");
	printf("\n设置值...\n");
	
	PersonData* pData = (PersonData*)xrtDictSet(pDict, "alice", 5, NULL);
	pData->nAge = 25;
	pData->dScore = 95.5;
	printf("  Set 'alice': age=%d, score=%.1f\n", pData->nAge, pData->dScore);
	
	pData = (PersonData*)xrtDictSet(pDict, "bob", 3, NULL);
	pData->nAge = 30;
	pData->dScore = 88.0;
	printf("  Set 'bob': age=%d, score=%.1f\n", pData->nAge, pData->dScore);
	
	pData = (PersonData*)xrtDictSet(pDict, "charlie", 7, NULL);
	pData->nAge = 28;
	pData->dScore = 92.3;
	printf("  Set 'charlie': age=%d, score=%.1f\n", pData->nAge, pData->dScore);
	
	printf("\nGetting values...\n");
	printf("\n获取值...\n");
	
	pData = (PersonData*)xrtDictGet(pDict, "alice", 5);
	if (pData)
		printf("  'alice': age=%d, score=%.1f\n", pData->nAge, pData->dScore);
	
	pData = (PersonData*)xrtDictGet(pDict, "bob", 3);
	if (pData)
		printf("  'bob': age=%d, score=%.1f\n", pData->nAge, pData->dScore);
	
	printf("\nChecking key existence...\n");
	printf("\n检查键是否存在...\n");
	printf("  'alice' exists: %s\n", xrtDictExists(pDict, "alice", 5) ? "yes" : "no");
	printf("  'unknown' exists: %s\n", xrtDictExists(pDict, "unknown", 7) ? "yes" : "no");
	
	printf("\nDictionary count: %u\n", xrtDictCount(pDict));
	printf("字典计数: %u\n", xrtDictCount(pDict));
	
	printf("\nRemoving 'bob'...\n");
	printf("\n删除 'bob'...\n");
	xrtDictRemove(pDict, "bob", 3);
	printf("  'bob' exists after removal: %s\n", xrtDictExists(pDict, "bob", 3) ? "yes" : "no");
	printf("  Dictionary count: %u\n", xrtDictCount(pDict));
	
	xrtDictDestroy(pDict);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Basic Dictionary Example / 基本字典示例\n");
	printf("========================================\n");
	
	test_basic_dict();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
