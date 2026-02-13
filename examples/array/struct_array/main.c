/*
 * XRT Example - Struct Array
 * XRT 范例 - 结构体数组
 *
 * Description / 说明:
 *   EN: Demonstrates xrtArrayCreate, xrtArrayInsert, xrtArrayAppend, 
 *       xrtArrayGet, xrtArrayRemove for struct arrays.
 *   CN: 演示 xrtArrayCreate、xrtArrayInsert、xrtArrayAppend、xrtArrayGet、
 *       xrtArrayRemove 进行结构体数组操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/array_struct_array.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/array_struct_array              (Linux, GCC)
 *
 * Note / 注意:
 *   - xarray is a pointer type, use -> to access members
 *   - ItemLength specifies size of each element
 *   - Array indices are 1-based (first element at index 1)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Define a test struct
// 定义测试结构体
typedef struct {
	int iID;
	char sName[32];
	float fScore;
} Student;

// Test array creation
// 测试数组创建
void TestArrayCreate()
{
	printf("=== Array Creation ===\n");
	printf("=== 数组创建 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Student));
	printf("Created array for Student struct\n");
	printf("为 Student 结构体创建数组\n");
	printf("  ItemLength: %u bytes\n", pArr->ItemLength);
	printf("  Count: %u\n", pArr->Count);
	printf("  AllocCount: %u\n", pArr->AllocCount);
	printf("\n");
	
	xrtArrayDestroy(pArr);
}

// Test array append
// 测试数组追加
void TestArrayAppend()
{
	printf("=== Array Append ===\n");
	printf("=== 数组追加 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Student));
	
	// Append students
	// 追加学生
	Student s1 = {1, "Alice", 95.5f};
	Student s2 = {2, "Bob", 87.0f};
	Student s3 = {3, "Charlie", 92.3f};
	
	uint32 uiIdx1 = xrtArrayAppend(pArr, 1);
	Student* pS1 = (Student*)xrtArrayGet(pArr, uiIdx1);
	*pS1 = s1;
	
	uint32 uiIdx2 = xrtArrayAppend(pArr, 1);
	Student* pS2 = (Student*)xrtArrayGet(pArr, uiIdx2);
	*pS2 = s2;
	
	uint32 uiIdx3 = xrtArrayAppend(pArr, 1);
	Student* pS3 = (Student*)xrtArrayGet(pArr, uiIdx3);
	*pS3 = s3;
	
	printf("Appended 3 students:\n");
	printf("追加 3 个学生:\n");
	
	uint32 i;
	for ( i = 1; i <= pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		printf("  [%u] ID=%d, Name=%s, Score=%.1f\n", 
		       i, pS->iID, pS->sName, pS->fScore);
	}
	printf("\n");
	
	xrtArrayDestroy(pArr);
}

// Test array insert
// 测试数组插入
void TestArrayInsert()
{
	printf("=== Array Insert ===\n");
	printf("=== 数组插入 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Student));
	
	// Add initial students
	// 添加初始学生
	Student s1 = {1, "Alice", 95.0f};
	Student s2 = {3, "Charlie", 92.0f};
	
	uint32 uiIdx = xrtArrayAppend(pArr, 1);
	*((Student*)xrtArrayGet(pArr, uiIdx)) = s1;
	
	uiIdx = xrtArrayAppend(pArr, 1);
	*((Student*)xrtArrayGet(pArr, uiIdx)) = s2;
	
	printf("Before insert:\n");
	printf("插入前:\n");
	uint32 i;
	for ( i = 1; i <= pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		printf("  [%u] %s\n", i, pS->sName);
	}
	
	// Insert Bob at index 2
	// 在索引 2 处插入 Bob
	Student sNew = {2, "Bob", 88.0f};
	uiIdx = xrtArrayInsert(pArr, 2, 1);
	*((Student*)xrtArrayGet(pArr, uiIdx)) = sNew;
	
	printf("\nAfter insert at index 2:\n");
	printf("在索引 2 处插入后:\n");
	for ( i = 1; i <= pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		printf("  [%u] %s\n", i, pS->sName);
	}
	printf("\n");
	
	xrtArrayDestroy(pArr);
}

// Test array remove
// 测试数组删除
void TestArrayRemove()
{
	printf("=== Array Remove ===\n");
	printf("=== 数组删除 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Student));
	
	// Add students
	// 添加学生
	Student arrStudents[] = {
		{1, "Alice", 95.0f},
		{2, "Bob", 88.0f},
		{3, "Charlie", 92.0f},
		{4, "David", 85.0f},
	};
	
	int i;
	for ( i = 0; i < 4; i++ ) {
		uint32 uiIdx = xrtArrayAppend(pArr, 1);
		*((Student*)xrtArrayGet(pArr, uiIdx)) = arrStudents[i];
	}
	
	printf("Before remove (count=%u):\n", pArr->Count);
	printf("删除前 (数量=%u):\n", pArr->Count);
	for ( i = 1; i <= (int)pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		printf("  [%d] %s\n", i, pS->sName);
	}
	
	// Remove at index 2 (Bob)
	// 删除索引 2 (Bob)
	xrtArrayRemove(pArr, 2, 1);
	
	printf("\nAfter removing index 2 (count=%u):\n", pArr->Count);
	printf("删除索引 2 后 (数量=%u):\n", pArr->Count);
	for ( i = 1; i <= (int)pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		printf("  [%d] %s\n", i, pS->sName);
	}
	printf("\n");
	
	xrtArrayDestroy(pArr);
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Case: Score Management ===\n");
	printf("=== 实际用例: 成绩管理 ===\n");
	
	xarray pArr = xrtArrayCreate(sizeof(Student));
	
	// Add students
	// 添加学生
	Student arrStudents[] = {
		{101, "Zhang San", 85.5f},
		{102, "Li Si", 92.0f},
		{103, "Wang Wu", 78.5f},
		{104, "Zhao Liu", 88.0f},
		{105, "Qian Qi", 95.5f},
	};
	
	int i;
	for ( i = 0; i < 5; i++ ) {
		uint32 uiIdx = xrtArrayAppend(pArr, 1);
		*((Student*)xrtArrayGet(pArr, uiIdx)) = arrStudents[i];
	}
	
	// Calculate average
	// 计算平均分
	float fTotal = 0;
	for ( i = 1; i <= (int)pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		fTotal += pS->fScore;
	}
	float fAvg = fTotal / pArr->Count;
	
	printf("Student count: %u\n", pArr->Count);
	printf("学生数量: %u\n", pArr->Count);
	printf("Average score: %.2f\n", fAvg);
	printf("平均分: %.2f\n", fAvg);
	
	// Find highest score
	// 查找最高分
	float fMax = 0;
	int iMaxIdx = 1;
	for ( i = 1; i <= (int)pArr->Count; i++ ) {
		Student* pS = (Student*)xrtArrayGet(pArr, i);
		if ( pS->fScore > fMax ) {
			fMax = pS->fScore;
			iMaxIdx = i;
		}
	}
	
	Student* pTop = (Student*)xrtArrayGet(pArr, iMaxIdx);
	printf("Top student: %s (%.1f)\n", pTop->sName, pTop->fScore);
	printf("最高分学生: %s (%.1f)\n", pTop->sName, pTop->fScore);
	printf("\n");
	
	xrtArrayDestroy(pArr);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Array - Struct Array Demo\n");
	printf("XRT 数组 - 结构体数组演示\n");
	printf("=============================\n\n");
	
	TestArrayCreate();
	TestArrayAppend();
	TestArrayInsert();
	TestArrayRemove();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
