/*
 * XRT Example - Basic Memory Operations
 * XRT 范例 - 基本内存操作
 *
 * Description / 说明:
 *   EN: Demonstrates xrtMalloc, xrtCalloc, xrtRealloc, xrtFree
 *       for safe memory allocation with error handling.
 *   CN: 演示 xrtMalloc、xrtCalloc、xrtRealloc、xrtFree 安全内存分配和错误处理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_memory_basic.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/base_memory_basic              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtMalloc returns NULL and sets error on failure
 *   - xrtCalloc initializes memory to zero
 *   - xrtRealloc can expand or shrink memory
 *   - xrtFree safely handles NULL and xCore.sNull pointers
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Custom error handler to catch allocation failures
// 自定义错误处理器捕获分配失败
void MyErrorHandler(str sError)
{
	printf("[Memory Error] %s\n", sError);
	printf("[内存错误] %s\n", sError);
}

// Test xrtMalloc - allocate uninitialized memory
// 测试 xrtMalloc - 分配未初始化内存
void TestMalloc()
{
	printf("=== xrtMalloc Test ===\n");
	printf("=== xrtMalloc 测试 ===\n");
	
	// Allocate memory for 10 integers
	// 为 10 个整数分配内存
	int* piData = (int*)xrtMalloc(10 * sizeof(int));
	
	if ( piData == NULL ) {
		printf("Allocation failed!\n");
		printf("分配失败！\n");
		return;
	}
	
	printf("Allocated %zu bytes for 10 integers at %p\n", 
	       10 * sizeof(int), (void*)piData);
	printf("为 10 个整数分配了 %zu 字节，地址 %p\n", 
	       10 * sizeof(int), (void*)piData);
	
	// Initialize and use the memory
	// 初始化并使用内存
	int i;
	for ( i = 0; i < 10; i++ ) {
		piData[i] = i * i;  // Store squares
	}
	
	// Print values
	// 打印值
	printf("Values: ");
	for ( i = 0; i < 10; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n数值: ");
	for ( i = 0; i < 10; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n\n");
	
	// Free the memory
	// 释放内存
	xrtFree(piData);
	printf("Memory freed.\n");
	printf("内存已释放。\n\n");
}

// Test xrtCalloc - allocate zero-initialized memory
// 测试 xrtCalloc - 分配零初始化内存
void TestCalloc()
{
	printf("=== xrtCalloc Test ===\n");
	printf("=== xrtCalloc 测试 ===\n");
	
	// Allocate memory for 5 floats, initialized to zero
	// 为 5 个浮点数分配内存，初始化为零
	float* pfData = (float*)xrtCalloc(5, sizeof(float));
	
	if ( pfData == NULL ) {
		printf("Allocation failed!\n");
		printf("分配失败！\n");
		return;
	}
	
	printf("Allocated %zu bytes for 5 floats at %p\n",
	       5 * sizeof(float), (void*)pfData);
	printf("为 5 个浮点数分配了 %zu 字节，地址 %p\n",
	       5 * sizeof(float), (void*)pfData);
	
	// Verify zero initialization
	// 验证零初始化
	printf("Initial values (should be 0): ");
	int i;
	int bAllZero = TRUE;
	for ( i = 0; i < 5; i++ ) {
		printf("%.1f ", pfData[i]);
		if ( pfData[i] != 0.0f ) {
			bAllZero = FALSE;
		}
	}
	printf("\n初始值 (应为 0): ");
	for ( i = 0; i < 5; i++ ) {
		printf("%.1f ", pfData[i]);
	}
	printf("\n");
	
	if ( bAllZero ) {
		printf("All values are zero (correctly initialized).\n");
		printf("所有值为零 (正确初始化)。\n");
	}
	printf("\n");
	
	// Assign values
	// 赋值
	for ( i = 0; i < 5; i++ ) {
		pfData[i] = (float)(i + 1) * 0.5f;
	}
	
	printf("After assignment: ");
	for ( i = 0; i < 5; i++ ) {
		printf("%.1f ", pfData[i]);
	}
	printf("\n赋值后: ");
	for ( i = 0; i < 5; i++ ) {
		printf("%.1f ", pfData[i]);
	}
	printf("\n\n");
	
	// Free the memory
	// 释放内存
	xrtFree(pfData);
	printf("Memory freed.\n");
	printf("内存已释放。\n\n");
}

// Test xrtRealloc - resize memory
// 测试 xrtRealloc - 调整内存大小
void TestRealloc()
{
	printf("=== xrtRealloc Test ===\n");
	printf("=== xrtRealloc 测试 ===\n");
	
	// Start with 5 elements
	// 从 5 个元素开始
	int* piData = (int*)xrtMalloc(5 * sizeof(int));
	if ( piData == NULL ) {
		printf("Initial allocation failed!\n");
		printf("初始分配失败！\n");
		return;
	}
	
	printf("Initial allocation: 5 integers\n");
	printf("初始分配: 5 个整数\n");
	
	// Fill initial data
	// 填充初始数据
	int i;
	for ( i = 0; i < 5; i++ ) {
		piData[i] = i + 1;
	}
	
	// Expand to 10 elements
	// 扩展到 10 个元素
	int* piNewData = (int*)xrtRealloc(piData, 10 * sizeof(int));
	if ( piNewData == NULL ) {
		printf("Reallocation failed!\n");
		printf("重新分配失败！\n");
		xrtFree(piData);
		return;
	}
	piData = piNewData;
	
	printf("Expanded to: 10 integers\n");
	printf("扩展到: 10 个整数\n");
	
	// Fill new elements
	// 填充新元素
	for ( i = 5; i < 10; i++ ) {
		piData[i] = (i + 1) * 10;
	}
	
	printf("All values: ");
	for ( i = 0; i < 10; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n所有值: ");
	for ( i = 0; i < 10; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n");
	
	// Shrink to 3 elements
	// 缩减到 3 个元素
	piNewData = (int*)xrtRealloc(piData, 3 * sizeof(int));
	if ( piNewData == NULL ) {
		printf("Shrink failed!\n");
		printf("缩减失败！\n");
		xrtFree(piData);
		return;
	}
	piData = piNewData;
	
	printf("Shrunk to: 3 integers\n");
	printf("缩减到: 3 个整数\n");
	printf("Remaining values: ");
	for ( i = 0; i < 3; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n剩余值: ");
	for ( i = 0; i < 3; i++ ) {
		printf("%d ", piData[i]);
	}
	printf("\n\n");
	
	// Free the memory
	// 释放内存
	xrtFree(piData);
	printf("Memory freed.\n");
	printf("内存已释放。\n\n");
}

// Test xrtFree - safe free behavior
// 测试 xrtFree - 安全释放行为
void TestFreeSafe()
{
	printf("=== xrtFree Safety Test ===\n");
	printf("=== xrtFree 安全测试 ===\n");
	
	// Test 1: Free NULL pointer (safe)
	// 测试 1: 释放 NULL 指针 (安全)
	printf("Freeing NULL pointer...\n");
	printf("释放 NULL 指针...\n");
	xrtFree(NULL);
	printf("Success (no crash).\n");
	printf("成功 (未崩溃)。\n");
	
	// Test 2: Free xCore.sNull pointer (safe)
	// 测试 2: 释放 xCore.sNull 指针 (安全)
	printf("Freeing xCore.sNull pointer...\n");
	printf("释放 xCore.sNull 指针...\n");
	xrtFree(xCore.sNull);
	printf("Success (no crash).\n");
	printf("成功 (未崩溃)。\n");
	
	// Test 3: Normal free
	// 测试 3: 正常释放
	int* piData = (int*)xrtMalloc(sizeof(int));
	printf("Allocated memory at %p\n", (void*)piData);
	printf("分配内存地址 %p\n", (void*)piData);
	xrtFree(piData);
	printf("Freed successfully.\n");
	printf("成功释放。\n\n");
}

// Test dynamic string building with realloc
// 测试使用 realloc 动态构建字符串
void TestStringBuilding()
{
	printf("=== Dynamic String Building ===\n");
	printf("=== 动态字符串构建 ===\n");
	
	size_t iCapacity = 16;
	size_t iLength = 0;
	char* psBuffer = (char*)xrtMalloc(iCapacity);
	
	if ( psBuffer == NULL ) {
		printf("Allocation failed!\n");
		printf("分配失败！\n");
		return;
	}
	
	psBuffer[0] = '\0';
	
	// Append multiple strings
	// 追加多个字符串
	char* psParts[] = {"Hello", ", ", "World", "!", " ", "XRT", " ", "Library"};
	int iNumParts = 8;
	int i;
	
	for ( i = 0; i < iNumParts; i++ ) {
		size_t iPartLen = strlen(psParts[i]);
		
		// Check if we need to expand
		// 检查是否需要扩展
		if ( iLength + iPartLen + 1 > iCapacity ) {
			iCapacity *= 2;
			char* psNewBuffer = (char*)xrtRealloc(psBuffer, iCapacity);
			if ( psNewBuffer == NULL ) {
				printf("Reallocation failed!\n");
				printf("重新分配失败！\n");
				xrtFree(psBuffer);
				return;
			}
			psBuffer = psNewBuffer;
			printf("Expanded buffer to %zu bytes\n", iCapacity);
			printf("扩展缓冲区到 %zu 字节\n", iCapacity);
		}
		
		strcpy(psBuffer + iLength, psParts[i]);
		iLength += iPartLen;
	}
	
	printf("Final string: \"%s\"\n", psBuffer);
	printf("最终字符串: \"%s\"\n", psBuffer);
	printf("Length: %zu, Capacity: %zu\n\n", iLength, iCapacity);
	printf("长度: %zu, 容量: %zu\n\n", iLength, iCapacity);
	
	xrtFree(psBuffer);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	// Set custom error handler
	// 设置自定义错误处理器
	xCore.OnError = MyErrorHandler;
	
	printf("XRT Base - Basic Memory Operations Demo\n");
	printf("XRT 基础 - 基本内存操作演示\n");
	printf("========================================\n\n");
	
	TestMalloc();
	TestCalloc();
	TestRealloc();
	TestFreeSafe();
	TestStringBuilding();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	printf("All memory operations completed.\n");
	printf("所有内存操作完成。\n");
	return 0;
}
