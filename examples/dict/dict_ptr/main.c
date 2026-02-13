/**
 * @file main.c
 * @brief Dictionary Pointer Example - xrtDictSetPtr/GetPtr/RemovePtr
 *        字典指针示例 - xrtDictSetPtr/GetPtr/RemovePtr
 * 
 * This example demonstrates:
 * - Storing pointers in dictionary
 * - Getting and removing pointer values
 * - Pointer-based dictionary operations
 * 
 * 本示例演示：
 * - 在字典中存储指针
 * - 获取和删除指针值
 * - 基于指针的字典操作
 * 
 * Build: tcc main.c -o ../../bin/dict_dict_ptr.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_dict_ptr(void)
{
	printf("=== Test: Dictionary Pointer Operations ===\n");
	printf("=== 测试：字典指针操作 ===\n");
	
	xdict pDict = xrtDictCreate(sizeof(ptr));
	
	printf("Storing pointers in dictionary...\n");
	printf("在字典中存储指针...\n");
	
	int a = 100, b = 200, c = 300;
	char str1[] = "Hello", str2[] = "World";
	
	xrtDictSetPtr(pDict, "int_a", 5, &a, NULL);
	xrtDictSetPtr(pDict, "int_b", 5, &b, NULL);
	xrtDictSetPtr(pDict, "int_c", 5, &c, NULL);
	xrtDictSetPtr(pDict, "str1", 4, str1, NULL);
	xrtDictSetPtr(pDict, "str2", 4, str2, NULL);
	
	printf("Dictionary count: %u\n", xrtDictCount(pDict));
	printf("字典计数: %u\n", xrtDictCount(pDict));
	
	printf("\nGetting pointer values:\n");
	printf("\n获取指针值:\n");
	
	int* pInt = (int*)xrtDictGetPtr(pDict, "int_a", 5);
	if (pInt) printf("  int_a = %d\n", *pInt);
	
	pInt = (int*)xrtDictGetPtr(pDict, "int_b", 5);
	if (pInt) printf("  int_b = %d\n", *pInt);
	
	char* pStr = (char*)xrtDictGetPtr(pDict, "str1", 4);
	if (pStr) printf("  str1 = %s\n", pStr);
	
	printf("\nRemoving 'int_b'...\n");
	printf("\n删除 'int_b'...\n");
	ptr pOld = xrtDictRemovePtr(pDict, "int_b", 5);
	printf("  Removed pointer: %p (value was %d)\n", pOld, *(int*)pOld);
	printf("  'int_b' exists after removal: %s\n", xrtDictExists(pDict, "int_b", 5) ? "yes" : "no");
	
	printf("\nUpdating 'str1' with old value retrieval:\n");
	printf("\n更新 'str1' 并获取旧值:\n");
	char newStr[] = "Updated!";
	ptr pOldStr = NULL;
	xrtDictSetPtr(pDict, "str1", 4, newStr, &pOldStr);
	if (pOldStr) printf("  Old value: %s\n", (char*)pOldStr);
	pStr = (char*)xrtDictGetPtr(pDict, "str1", 4);
	if (pStr) printf("  New value: %s\n", pStr);
	
	xrtDictDestroy(pDict);
}

void test_ptr_with_structs(void)
{
	printf("\n=== Test: Pointer with Structs ===\n");
	printf("=== 测试：结构体指针 ===\n");
	
	typedef struct {
		int nID;
		char szName[32];
	} User;
	
	xdict pDict = xrtDictCreate(sizeof(ptr));
	
	User users[] = {
		{1, "Alice"},
		{2, "Bob"},
		{3, "Charlie"}
	};
	
	printf("Storing User struct pointers...\n");
	printf("存储User结构体指针...\n");
	
	char key[16];
	for (int i = 0; i < 3; i++)
	{
		sprintf(key, "user_%d", users[i].nID);
		xrtDictSetPtr(pDict, key, strlen(key), &users[i], NULL);
		printf("  Stored %s (ID=%d)\n", users[i].szName, users[i].nID);
	}
	
	printf("\nRetrieving User structs:\n");
	printf("\n检索User结构体:\n");
	
	for (int i = 1; i <= 3; i++)
	{
		sprintf(key, "user_%d", i);
		User* pUser = (User*)xrtDictGetPtr(pDict, key, strlen(key));
		if (pUser)
			printf("  %s (ID=%d)\n", pUser->szName, pUser->nID);
	}
	
	xrtDictDestroy(pDict);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Dict Pointer Example / 字典指针示例\n");
	printf("========================================\n");
	
	test_dict_ptr();
	test_ptr_with_structs();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
