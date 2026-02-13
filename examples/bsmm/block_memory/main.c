/**
 * @file main.c
 * @brief Block Memory Manager Example - xrtBsmm operations
 *        块内存管理器示例 - xrtBsmm 操作
 * 
 * This example demonstrates:
 * - Creating and destroying block memory managers
 * - Allocating and freeing fixed-size blocks
 * - Memory is managed in pages for efficiency
 * 
 * 本示例演示：
 * - 创建和销毁块内存管理器
 * - 分配和释放固定大小的块
 * - 内存以页为单位高效管理
 * 
 * Build: tcc main.c -o ../../bin/bsmm_block_memory.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	char szName[32];
	double dScore;
} Student;

void test_basic_bsmm(void)
{
	printf("=== Test: Basic Block Memory Manager ===\n");
	printf("=== 测试：基本块内存管理器 ===\n");
	
	xbsmm pBSMM = xrtBsmmCreate(sizeof(Student));
	
	printf("Created BSMM with item size: %u bytes\n", (uint32)sizeof(Student));
	printf("创建BSMM，项大小: %u 字节\n", (uint32)sizeof(Student));
	
	printf("\nAllocating 5 students...\n");
	printf("\n分配5个学生...\n");
	
	Student* pStudents[5];
	for (int i = 0; i < 5; i++)
	{
		pStudents[i] = (Student*)xrtBsmmAlloc(pBSMM);
		pStudents[i]->nID = 100 + i;
		sprintf(pStudents[i]->szName, "Student_%d", i + 1);
		pStudents[i]->dScore = 80.0 + i * 2.5;
	}
	
	for (int i = 0; i < 5; i++)
	{
		printf("  [%d] ID=%d, Name=%s, Score=%.1f\n", 
			i, pStudents[i]->nID, pStudents[i]->szName, pStudents[i]->dScore);
	}
	
	printf("\nFreeing students 1 and 3...\n");
	printf("\n释放学生1和3...\n");
	
	xrtBsmmFree(pBSMM, pStudents[0]);
	xrtBsmmFree(pBSMM, pStudents[2]);
	
	printf("Freed. Remaining allocated: students 0, 2, 4\n");
	printf("已释放。剩余分配：学生0, 2, 4\n");
	
	xrtBsmmDestroy(pBSMM);
}

void test_reuse_blocks(void)
{
	printf("\n=== Test: Block Reuse ===\n");
	printf("=== 测试：块重用 ===\n");
	
	xbsmm pBSMM = xrtBsmmCreate(sizeof(int));
	
	printf("Allocating 3 integer blocks...\n");
	printf("分配3个整数块...\n");
	
	int* pA = (int*)xrtBsmmAlloc(pBSMM);
	int* pB = (int*)xrtBsmmAlloc(pBSMM);
	int* pC = (int*)xrtBsmmAlloc(pBSMM);
	
	*pA = 111;
	*pB = 222;
	*pC = 333;
	
	printf("  pA=%p, *pA=%d\n", (void*)pA, *pA);
	printf("  pB=%p, *pB=%d\n", (void*)pB, *pB);
	printf("  pC=%p, *pC=%d\n", (void*)pC, *pC);
	
	printf("\nFreeing pB...\n");
	printf("\n释放 pB...\n");
	xrtBsmmFree(pBSMM, pB);
	
	printf("Allocating new block (should reuse pB)...\n");
	printf("分配新块（应重用pB）...\n");
	
	int* pD = (int*)xrtBsmmAlloc(pBSMM);
	*pD = 444;
	
	printf("  pD=%p, *pD=%d\n", (void*)pD, *pD);
	
	if (pD == pB)
	{
		printf("  pD == pB: Block was reused!\n");
		printf("  pD == pB: 块被重用了！\n");
	}
	
	xrtBsmmDestroy(pBSMM);
}

void test_get_ptr_by_index(void)
{
	printf("\n=== Test: Get Pointer by Index ===\n");
	printf("=== 测试：通过索引获取指针 ===\n");
	
	xbsmm pBSMM = xrtBsmmCreate(sizeof(int));
	
	printf("Allocating 5 integers...\n");
	printf("分配5个整数...\n");
	
	for (int i = 0; i < 5; i++)
	{
		int* p = (int*)xrtBsmmAlloc(pBSMM);
		*p = (i + 1) * 10;
	}
	
	printf("Accessing by index (0-4):\n");
	printf("通过索引访问（0-4）:\n");
	for (uint32 i = 0; i < 5; i++)
	{
		int* p = (int*)xrtBsmmGetPtr_Inline(pBSMM, i);
		printf("  Index %u: value = %d\n", i, *p);
	}
	
	xrtBsmmDestroy(pBSMM);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Block Memory Example / 块内存示例\n");
	printf("========================================\n");
	
	test_basic_bsmm();
	test_reuse_blocks();
	test_get_ptr_by_index();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
