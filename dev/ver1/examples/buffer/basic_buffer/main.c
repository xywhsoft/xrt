/*
 * XRT Example - Basic Buffer
 * XRT 范例 - 基本缓冲区
 *
 * Description / 说明:
 *   EN: Demonstrates xrtBufferCreate, xrtBufferAppend, xrtBufferInsert,
 *       xrtBufferDestroy for dynamic buffer operations.
 *   CN: 演示 xrtBufferCreate、xrtBufferAppend、xrtBufferInsert、xrtBufferDestroy
 *       进行动态缓冲区操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/buffer_basic_buffer.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/buffer_basic_buffer              (Linux, GCC)
 *
 * Note / 注意:
 *   - xbuffer is a pointer type, use -> to access members
 *   - Step=0 uses default step size (64KB)
 *   - bStrMode=TRUE adds null terminator
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test buffer creation
// 测试缓冲区创建
void TestBufferCreate()
{
	printf("=== Buffer Creation ===\n");
	printf("=== 缓冲区创建 ===\n");
	
	// Create with default step
	// 使用默认步长创建
	xbuffer pBuf = xrtBufferCreate(0);
	printf("Created buffer with default step\n");
	printf("使用默认步长创建缓冲区\n");
	printf("  Buffer: %p\n", pBuf->Buffer);
	printf("  Length: %u\n", pBuf->Length);
	printf("  AllocLength: %u\n", pBuf->AllocLength);
	printf("\n");
	
	xrtBufferDestroy(pBuf);
	
	// Create with custom step
	// 使用自定义步长创建
	xbuffer pBuf2 = xrtBufferCreate(256);
	printf("Created buffer with step=256\n");
	printf("使用步长=256创建缓冲区\n");
	printf("  AllocLength: %u\n", pBuf2->AllocLength);
	printf("\n");
	
	xrtBufferDestroy(pBuf2);
}

// Test buffer append
// 测试缓冲区追加
void TestBufferAppend()
{
	printf("=== Buffer Append ===\n");
	printf("=== 缓冲区追加 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Append strings
	// 追加字符串
	xrtBufferAppend(pBuf, "Hello", 5, FALSE);
	xrtBufferAppend(pBuf, " ", 1, FALSE);
	xrtBufferAppend(pBuf, "World", 5, FALSE);
	xrtBufferAppend(pBuf, "!", 1, TRUE);  // Add null terminator
	
	printf("Content: \"%s\"\n", pBuf->Buffer);
	printf("内容: \"%s\"\n", pBuf->Buffer);
	printf("Length: %u, AllocLength: %u\n", pBuf->Length, pBuf->AllocLength);
	printf("长度: %u, 分配长度: %u\n", pBuf->Length, pBuf->AllocLength);
	printf("\n");
	
	xrtBufferDestroy(pBuf);
}

// Test buffer insert
// 测试缓冲区插入
void TestBufferInsert()
{
	printf("=== Buffer Insert ===\n");
	printf("=== 缓冲区插入 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build initial content
	// 构建初始内容
	xrtBufferAppend(pBuf, "Hello World", 11, TRUE);
	printf("Initial: \"%s\"\n", pBuf->Buffer);
	printf("初始: \"%s\"\n", pBuf->Buffer);
	
	// Insert at beginning
	// 在开头插入
	xrtBufferInsert(pBuf, 0, ">>> ", 4, FALSE);
	printf("After insert at 0: \"%s\"\n", pBuf->Buffer);
	printf("在 0 处插入后: \"%s\"\n", pBuf->Buffer);
	
	// Insert in middle
	// 在中间插入
	xrtBufferInsert(pBuf, 11, "BEAUTIFUL ", 10, FALSE);
	printf("After insert at 11: \"%s\"\n", pBuf->Buffer);
	printf("在 11 处插入后: \"%s\"\n", pBuf->Buffer);
	printf("\n");
	
	xrtBufferDestroy(pBuf);
}

// Test string builder pattern
// 测试字符串构建器模式
void TestStringBuilder()
{
	printf("=== String Builder Pattern ===\n");
	printf("=== 字符串构建器模式 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build multiline string
	// 构建多行字符串
	xrtBufferAppend(pBuf, "Line 1\n", 7, FALSE);
	xrtBufferAppend(pBuf, "Line 2\n", 7, FALSE);
	xrtBufferAppend(pBuf, "Line 3\n", 7, FALSE);
	xrtBufferAppend(pBuf, "Line 4\n", 7, TRUE);
	
	printf("Built string:\n%s", pBuf->Buffer);
	printf("构建的字符串:\n%s", pBuf->Buffer);
	printf("Total length: %u\n\n", pBuf->Length);
	
	xrtBufferDestroy(pBuf);
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// CSV builder
	// CSV 构建器
	xbuffer pCSV = xrtBufferCreate(0);
	
	char* psFields[] = {"Name", "Age", "City"};
	char* psValues1[] = {"Alice", "25", "Beijing"};
	char* psValues2[] = {"Bob", "30", "Shanghai"};
	
	// Header row
	// 表头行
	int i;
	for ( i = 0; i < 3; i++ ) {
		if ( i > 0 ) xrtBufferAppend(pCSV, ",", 1, FALSE);
		xrtBufferAppend(pCSV, psFields[i], strlen(psFields[i]), FALSE);
	}
	xrtBufferAppend(pCSV, "\n", 1, FALSE);
	
	// Data row 1
	// 数据行 1
	for ( i = 0; i < 3; i++ ) {
		if ( i > 0 ) xrtBufferAppend(pCSV, ",", 1, FALSE);
		xrtBufferAppend(pCSV, psValues1[i], strlen(psValues1[i]), FALSE);
	}
	xrtBufferAppend(pCSV, "\n", 1, FALSE);
	
	// Data row 2
	// 数据行 2
	for ( i = 0; i < 3; i++ ) {
		if ( i > 0 ) xrtBufferAppend(pCSV, ",", 1, FALSE);
		xrtBufferAppend(pCSV, psValues2[i], strlen(psValues2[i]), FALSE);
	}
	xrtBufferAppend(pCSV, "", 0, TRUE);
	
	printf("CSV output:\n%s\n", pCSV->Buffer);
	printf("CSV 输出:\n%s\n", pCSV->Buffer);
	
	xrtBufferDestroy(pCSV);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Buffer - Basic Buffer Demo\n");
	printf("XRT 缓冲区 - 基本缓冲区演示\n");
	printf("==============================\n\n");
	
	TestBufferCreate();
	TestBufferAppend();
	TestBufferInsert();
	TestStringBuilder();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
