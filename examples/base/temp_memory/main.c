/*
 * XRT Example - Temporary Memory (Ring Buffer)
 * XRT 范例 - 临时内存 (环形缓冲区)
 *
 * Description / 说明:
 *   EN: Demonstrates xrtTempMemory ring buffer mechanism for automatic
 *       memory management without explicit free calls.
 *   CN: 演示 xrtTempMemory 环形缓冲区机制，实现无需显式释放的自动内存管理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_temp_memory.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/base_temp_memory              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtTempMemory allocates memory that auto-expires after 32 allocations
 *   - NOT thread-safe - use only in single-threaded contexts
 *   - xrtFreeTempMemory() manually clears all temporary memory
 *   - Useful for short-lived strings and buffers
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Function that returns a temporary string
// 返回临时字符串的函数
str GetTempGreeting(int iIndex)
{
	// Allocate temporary memory for greeting
	// 为问候语分配临时内存
	char* psBuffer = (char*)xrtTempMemory(64);
	
	if ( psBuffer == NULL ) {
		return xCore.sNull;
	}
	
	sprintf(psBuffer, "Hello #%d from temp memory!", iIndex);
	return psBuffer;
}

// Function that builds a formatted string in temp memory
// 在临时内存中构建格式化字符串的函数
str BuildTempPath(str psDir, str psFile)
{
	size_t iLen = strlen(psDir) + strlen(psFile) + 2;
	char* psBuffer = (char*)xrtTempMemory(iLen);
	
	if ( psBuffer == NULL ) {
		return xCore.sNull;
	}
	
	sprintf(psBuffer, "%s/%s", psDir, psFile);
	return psBuffer;
}

// Test basic ring buffer behavior
// 测试基本环形缓冲区行为
void TestRingBuffer()
{
	printf("=== Ring Buffer Behavior ===\n");
	printf("=== 环形缓冲区行为 ===\n");
	
	printf("TempMemIdx starts at: %u\n", xCore.TempMemIdx);
	printf("TempMemIdx 开始于: %u\n", xCore.TempMemIdx);
	
	// Allocate first 5 temporary strings
	// 分配前 5 个临时字符串
	printf("\nAllocating 5 temporary strings:\n");
	printf("分配 5 个临时字符串:\n");
	
	str psStrs[5];
	int i;
	for ( i = 0; i < 5; i++ ) {
		psStrs[i] = GetTempGreeting(i);
		printf("[%u] %s\n", xCore.TempMemIdx - 1, psStrs[i]);
	}
	
	// Verify all strings are still valid
	// 验证所有字符串仍然有效
	printf("\nAll strings still valid:\n");
	printf("所有字符串仍有效:\n");
	for ( i = 0; i < 5; i++ ) {
		printf("[%d] %s\n", i, psStrs[i]);
	}
	printf("\n");
}

// Test ring buffer wrap-around (32 slots)
// 测试环形缓冲区回绕 (32 槽位)
void TestWrapAround()
{
	printf("=== Ring Buffer Wrap-Around (32 slots) ===\n");
	printf("=== 环形缓冲区回绕 (32 槽位) ===\n");
	
	printf("Current TempMemIdx: %u\n", xCore.TempMemIdx);
	printf("当前 TempMemIdx: %u\n", xCore.TempMemIdx);
	
	// Allocate 35 temp strings (will wrap around)
	// 分配 35 个临时字符串 (将回绕)
	printf("\nAllocating 35 more temporary strings...\n");
	printf("再分配 35 个临时字符串...\n");
	
	int i;
	for ( i = 0; i < 35; i++ ) {
		char* psTemp = (char*)xrtTempMemory(32);
		sprintf(psTemp, "Item-%d", i);
		
		// Show wrap-around points
		// 显示回绕点
		if ( i < 5 || i >= 30 ) {
			printf("[%2d] TempMemIdx=%2u, value=%s\n", i, xCore.TempMemIdx, psTemp);
		} else if ( i == 5 ) {
			printf("... (skipping middle allocations)\n");
		}
	}
	
	printf("\nAfter 35 allocations, TempMemIdx: %u\n", xCore.TempMemIdx);
	printf("35 次分配后，TempMemIdx: %u\n\n", xCore.TempMemIdx);
}

// Test memory expiration
// 测试内存过期
void TestMemoryExpiration()
{
	printf("=== Memory Expiration Test ===\n");
	printf("=== 内存过期测试 ===\n");
	
	// Allocate first string
	// 分配第一个字符串
	char* psFirst = (char*)xrtTempMemory(32);
	sprintf(psFirst, "FIRST STRING");
	printf("Allocated: %s at index 0\n", psFirst);
	printf("分配: %s 在索引 0\n", psFirst);
	
	// Store pointer for later verification
	// 保存指针用于稍后验证
	char* pSavedPtr = psFirst;
	
	// Allocate 31 more strings (fill the buffer)
	// 再分配 31 个字符串 (填满缓冲区)
	int i;
	for ( i = 0; i < 31; i++ ) {
		char* psTemp = (char*)xrtTempMemory(32);
		sprintf(psTemp, "String-%d", i + 1);
	}
	
	printf("Allocated 31 more strings...\n");
	printf("再分配 31 个字符串...\n");
	
	// Now allocate one more - should overwrite first
	// 现在再分配一个 - 应该覆盖第一个
	char* psNew = (char*)xrtTempMemory(32);
	sprintf(psNew, "NEW OVERWRITE");
	printf("\nFinal allocation: %s\n", psNew);
	printf("最终分配: %s\n", psNew);
	
	// Check if original pointer is now invalid
	// 检查原始指针是否已无效
	printf("\nNote: The first string was automatically freed.\n");
	printf("注意: 第一个字符串已自动释放。\n");
	printf("Original pointer %p may now contain: %s\n", 
	       (void*)pSavedPtr, pSavedPtr);
	printf("\n");
}

// Test manual cleanup
// 测试手动清理
void TestManualCleanup()
{
	printf("=== Manual Cleanup Test ===\n");
	printf("=== 手动清理测试 ===\n");
	
	// Allocate several temporary strings
	// 分配多个临时字符串
	int i;
	for ( i = 0; i < 10; i++ ) {
		char* psTemp = (char*)xrtTempMemory(64);
		sprintf(psTemp, "Temporary data block #%d with some content", i);
	}
	
	printf("Allocated 10 temporary strings.\n");
	printf("分配了 10 个临时字符串。\n");
	printf("TempMemIdx before cleanup: %u\n", xCore.TempMemIdx);
	printf("清理前 TempMemIdx: %u\n", xCore.TempMemIdx);
	
	// Check how many slots are used
	// 检查使用了多少槽位
	int iUsed = 0;
	for ( i = 0; i < 32; i++ ) {
		if ( xCore.TempMem[i] != NULL ) {
			iUsed++;
		}
	}
	printf("Slots in use: %d\n", iUsed);
	printf("使用中的槽位: %d\n", iUsed);
	
	// Manual cleanup
	// 手动清理
	xrtFreeTempMemory();
	
	printf("\nAfter xrtFreeTempMemory():\n");
	printf("调用 xrtFreeTempMemory() 后:\n");
	printf("TempMemIdx: %u\n", xCore.TempMemIdx);
	printf("TempMemIdx: %u\n", xCore.TempMemIdx);
	
	// Verify all slots are cleared
	// 验证所有槽位已清除
	iUsed = 0;
	for ( i = 0; i < 32; i++ ) {
		if ( xCore.TempMem[i] != NULL ) {
			iUsed++;
		}
	}
	printf("Slots in use: %d (should be 0)\n", iUsed);
	printf("使用中的槽位: %d (应为 0)\n\n", iUsed);
}

// Test practical use case - path building
// 测试实际用例 - 路径构建
void TestPathBuilding()
{
	printf("=== Practical Use Case: Path Building ===\n");
	printf("=== 实际用例: 路径构建 ===\n");
	
	// Build multiple paths without worrying about freeing
	// 构建多个路径而无需担心释放
	str psPath1 = BuildTempPath("/home/user", "document.txt");
	str psPath2 = BuildTempPath("/home/user", "images/photo.jpg");
	str psPath3 = BuildTempPath("/var/log", "app.log");
	
	printf("Path 1: %s\n", psPath1);
	printf("路径 1: %s\n", psPath1);
	printf("Path 2: %s\n", psPath2);
	printf("路径 2: %s\n", psPath2);
	printf("Path 3: %s\n", psPath3);
	printf("路径 3: %s\n", psPath3);
	
	printf("\nAll paths built without manual free needed.\n");
	printf("所有路径已构建，无需手动释放。\n\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Base - Temporary Memory (Ring Buffer) Demo\n");
	printf("XRT 基础 - 临时内存 (环形缓冲区) 演示\n");
	printf("===============================================\n\n");
	
	TestRingBuffer();
	TestWrapAround();
	TestMemoryExpiration();
	TestManualCleanup();
	TestPathBuilding();
	
	// Cleanup XRT library (also frees temp memory)
	// 清理 XRT 库 (同时释放临时内存)
	xrtUnit();
	
	printf("Library cleanup complete (temp memory auto-freed).\n");
	printf("库清理完成 (临时内存自动释放)。\n");
	return 0;
}
