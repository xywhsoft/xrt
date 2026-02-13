/*
 * XRT Example - String Buffer Operations
 * XRT 范例 - 字符串缓冲区操作
 *
 * Description / 说明:
 *   EN: Demonstrates string buffer operations including dynamic buffer management,
 *       safe string appending, buffer resizing, and memory-efficient string building.
 *   CN: 演示字符串缓冲区操作，包括动态缓冲区管理、安全字符串追加、
 *       缓冲区调整大小和内存高效字符串构建。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_buffer_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_buffer_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Dynamic buffer allocation and automatic resizing
 *   - Safe string operations with bounds checking
 *   - Efficient memory usage through buffer reuse
 *   - Buffer state tracking (position, capacity, used space)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic buffer creation and management
// 测试基本缓冲区创建和管理
void TestBasicBufferManagement()
{
	printf("=== Basic Buffer Management ===\n");
	printf("=== 基本缓冲区管理 ===\n");
	
	// Create buffers with different initial sizes
	// 创建不同初始大小的缓冲区
	size_t iInitialSize1 = 32;
	size_t iInitialSize2 = 128;
	size_t iInitialSize3 = 1024;
	
	// Allocate buffers
	// 分配缓冲区
	str pBuffer1 = xrtMalloc(iInitialSize1);
	str pBuffer2 = xrtMalloc(iInitialSize2);
	str pBuffer3 = xrtMalloc(iInitialSize3);
	
	// Initialize buffers
	// 初始化缓冲区
	memset(pBuffer1, 0, iInitialSize1);
	memset(pBuffer2, 0, iInitialSize2);
	memset(pBuffer3, 0, iInitialSize3);
	
	printf("Buffer creation:\n");
	printf("  Buffer 1: size %zu bytes at %p\n", iInitialSize1, pBuffer1);
	printf("  Buffer 2: size %zu bytes at %p\n", iInitialSize2, pBuffer2);
	printf("  Buffer 3: size %zu bytes at %p\n\n", iInitialSize3, pBuffer3);
	
	// Test buffer filling
	// 测试缓冲区填充
	const char* sTestData = "Hello World! This is test data for buffer operations.";
	size_t iTestDataLen = strlen(sTestData);
	
	// Copy data to buffers with size checking
	// 带大小检查地复制数据到缓冲区
	size_t iCopyLen1 = (iTestDataLen < iInitialSize1 - 1) ? iTestDataLen : iInitialSize1 - 1;
	size_t iCopyLen2 = (iTestDataLen < iInitialSize2 - 1) ? iTestDataLen : iInitialSize2 - 1;
	size_t iCopyLen3 = (iTestDataLen < iInitialSize3 - 1) ? iTestDataLen : iInitialSize3 - 1;
	
	memcpy(pBuffer1, sTestData, iCopyLen1);
	pBuffer1[iCopyLen1] = '\0';
	
	memcpy(pBuffer2, sTestData, iCopyLen2);
	pBuffer2[iCopyLen2] = '\0';
	
	memcpy(pBuffer3, sTestData, iCopyLen3);
	pBuffer3[iCopyLen3] = '\0';
	
	printf("Buffer content:\n");
	printf("  Buffer 1 (%zu/%zu): \"%s\"\n", iCopyLen1, iInitialSize1, pBuffer1);
	printf("  Buffer 2 (%zu/%zu): \"%s\"\n", iCopyLen2, iInitialSize2, pBuffer2);
	printf("  Buffer 3 (%zu/%zu): \"%s\"\n\n", iCopyLen3, iInitialSize3, pBuffer3);
	
	// Test buffer expansion
	// 测试缓冲区扩展
	size_t iNewSize = iInitialSize1 * 2;
	str pExpandedBuffer = xrtRealloc(pBuffer1, iNewSize);
	
	if ( pExpandedBuffer ) {
		printf("Buffer expansion:\n");
		printf("  Original: %zu bytes at %p\n", iInitialSize1, pBuffer1);
		printf("  Expanded: %zu bytes at %p\n", iNewSize, pExpandedBuffer);
		printf("  Content preserved: \"%s\"\n\n", pExpandedBuffer);
		pBuffer1 = pExpandedBuffer;  // Update pointer
	}
	
	// Cleanup
	// 清理
	xrtFree(pBuffer1);
	xrtFree(pBuffer2);
	xrtFree(pBuffer3);
}

// Test safe string appending
// 测试安全字符串追加
void TestSafeStringAppending()
{
	printf("=== Safe String Appending ===\n");
	printf("=== 安全字符串追加 ===\n");
	
	// Create a buffer for building strings
	// 创建用于构建字符串的缓冲区
	size_t iBufferSize = 256;
	str pBuffer = xrtMalloc(iBufferSize);
	pBuffer[0] = '\0';  // Initialize as empty string
	
	size_t iUsedSpace = 0;
	size_t iCapacity = iBufferSize - 1;  // Reserve space for null terminator
	
	printf("String building buffer:\n");
	printf("  Capacity: %zu characters\n", iCapacity);
	printf("  Initial content: \"%s\"\n\n", pBuffer);
	
	// Append strings safely
	// 安全追加字符串
	const char* arrStrings[] = {
		"Hello",
		" ",
		"World",
		"!",
		" This",
		" is",
		" a",
		" test",
		" of",
		" safe",
		" string",
		" appending",
		"."
	};
	
	int iStringCount = sizeof(arrStrings) / sizeof(arrStrings[0]);
	
	for ( int i = 0; i < iStringCount; i++ ) {
		size_t iStrLen = strlen(arrStrings[i]);
		
		// Check if there's enough space
		// 检查是否有足够空间
		if ( iUsedSpace + iStrLen <= iCapacity ) {
			strcat(pBuffer, arrStrings[i]);
			iUsedSpace += iStrLen;
			printf("  Appended \"%s\" -> \"%s\" (used: %zu/%zu)\n", 
			       arrStrings[i], pBuffer, iUsedSpace, iCapacity);
		} else {
			printf("  Cannot append \"%s\" - insufficient space\n", arrStrings[i]);
			break;
		}
	}
	
	printf("\nFinal result: \"%s\"\n", pBuffer);
	printf("Total used space: %zu characters\n\n", iUsedSpace);
	
	// Test overflow prevention
	// 测试溢出预防
	printf("Overflow prevention test:\n");
	size_t iRemainingSpace = iCapacity - iUsedSpace;
	printf("  Remaining space: %zu characters\n", iRemainingSpace);
	
	const char* sLongString = "This is a very long string that might exceed buffer capacity";
	size_t iLongLen = strlen(sLongString);
	
	if ( iLongLen <= iRemainingSpace ) {
		strcat(pBuffer, sLongString);
		iUsedSpace += iLongLen;
		printf("  Successfully appended long string\n");
		printf("  New content: \"%s\"\n", pBuffer);
	} else {
		printf("  Long string (%zu chars) exceeds remaining space (%zu chars)\n", 
		       iLongLen, iRemainingSpace);
		printf("  Would cause buffer overflow - prevented\n");
	}
	
	printf("\n");
	
	// Cleanup
	// 清理
	xrtFree(pBuffer);
}

// Test buffer resizing strategies
// 测试缓冲区调整大小策略
void TestBufferResizing()
{
	printf("=== Buffer Resizing Strategies ===\n");
	printf("=== 缓冲区调整大小策略 ===\n");
	
	// Strategy 1: Fixed growth (double size each time)
	// 策略1：固定增长（每次翻倍）
	printf("Strategy 1: Exponential growth (double each time)\n");
	size_t iCurrentSize = 16;
	str pBuffer = xrtMalloc(iCurrentSize);
	pBuffer[0] = '\0';
	
	const char* sAppendData = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";  // 26 chars
	size_t iAppendLen = strlen(sAppendData);
	
	for ( int i = 0; i < 5; i++ ) {
		if ( strlen(pBuffer) + iAppendLen >= iCurrentSize - 1 ) {
			// Need to resize
			size_t iNewSize = iCurrentSize * 2;
			str pNewBuffer = xrtRealloc(pBuffer, iNewSize);
			if ( pNewBuffer ) {
				pBuffer = pNewBuffer;
				iCurrentSize = iNewSize;
				printf("  Resized to %zu bytes\n", iCurrentSize);
			} else {
				printf("  Resize failed!\n");
				break;
			}
		}
		strcat(pBuffer, sAppendData);
		printf("  Iteration %d: \"%s\" (length: %zu)\n", i+1, pBuffer, strlen(pBuffer));
	}
	
	xrtFree(pBuffer);
	printf("\n");
	
	// Strategy 2: Incremental growth (add fixed amount)
	// 策略2：增量增长（增加固定量）
	printf("Strategy 2: Incremental growth (+64 bytes each time)\n");
	iCurrentSize = 16;
	pBuffer = xrtMalloc(iCurrentSize);
	pBuffer[0] = '\0';
	
	for ( int i = 0; i < 5; i++ ) {
		if ( strlen(pBuffer) + iAppendLen >= iCurrentSize - 1 ) {
			// Need to resize
			size_t iNewSize = iCurrentSize + 64;
			str pNewBuffer = xrtRealloc(pBuffer, iNewSize);
			if ( pNewBuffer ) {
				pBuffer = pNewBuffer;
				iCurrentSize = iNewSize;
				printf("  Resized to %zu bytes\n", iCurrentSize);
			} else {
				printf("  Resize failed!\n");
				break;
			}
		}
		strcat(pBuffer, sAppendData);
		printf("  Iteration %d: \"%s\" (length: %zu)\n", i+1, pBuffer, strlen(pBuffer));
	}
	
	xrtFree(pBuffer);
	printf("\n");
	
	// Strategy 3: Pre-calculated size
	// 策略3：预计算大小
	printf("Strategy 3: Pre-calculated optimal size\n");
	int iIterations = 5;
	size_t iOptimalSize = 16 + (iIterations * iAppendLen) + 1;  // +1 for null terminator
	
	pBuffer = xrtMalloc(iOptimalSize);
	pBuffer[0] = '\0';
	
	printf("  Allocated %zu bytes upfront\n", iOptimalSize);
	
	for ( int i = 0; i < iIterations; i++ ) {
		strcat(pBuffer, sAppendData);
		printf("  Iteration %d: \"%s\" (length: %zu)\n", i+1, pBuffer, strlen(pBuffer));
	}
	
	printf("  No resizing needed - memory efficient\n\n");
	
	xrtFree(pBuffer);
}

// Test circular buffer operations
// 测试环形缓冲区操作
void TestCircularBufferOperations()
{
	printf("=== Circular Buffer Operations ===\n");
	printf("=== 环形缓冲区操作 ===\n");
	
	// Create circular buffer
	// 创建环形缓冲区
	size_t iBufferSize = 16;
	str pBuffer = xrtMalloc(iBufferSize);
	memset(pBuffer, 0, iBufferSize);
	
	size_t iHead = 0;  // Write position
	size_t iTail = 0;  // Read position
	size_t iCount = 0; // Number of elements
	
	printf("Circular buffer created: size %zu\n", iBufferSize);
	printf("Initial state: head=%zu, tail=%zu, count=%zu\n\n", iHead, iTail, iCount);
	
	// Write operations
	// 写入操作
	const char* sChars = "0123456789ABCDEF";
	size_t iCharsLen = strlen(sChars);
	
	printf("Writing to circular buffer:\n");
	for ( size_t i = 0; i < iCharsLen; i++ ) {
		if ( iCount < iBufferSize ) {
			pBuffer[iHead] = sChars[i];
			iHead = (iHead + 1) % iBufferSize;
			iCount++;
			printf("  Wrote '%c': head=%zu, count=%zu\n", sChars[i], iHead, iCount);
		} else {
			printf("  Buffer full - cannot write '%c'\n", sChars[i]);
		}
	}
	
	printf("\nBuffer content: \"");
	for ( size_t i = 0; i < iBufferSize; i++ ) {
		size_t iPos = (iTail + i) % iBufferSize;
		printf("%c", pBuffer[iPos] ? pBuffer[iPos] : '.');
	}
	printf("\"\n\n");
	
	// Read operations
	// 读取操作
	printf("Reading from circular buffer:\n");
	while ( iCount > 0 ) {
		char c = pBuffer[iTail];
		pBuffer[iTail] = '\0';  // Clear the position
		iTail = (iTail + 1) % iBufferSize;
		iCount--;
		printf("  Read '%c': tail=%zu, count=%zu\n", c, iTail, iCount);
	}
	
	printf("\nBuffer after reading: \"");
	for ( size_t i = 0; i < iBufferSize; i++ ) {
		printf("%c", pBuffer[i] ? pBuffer[i] : '.');
	}
	printf("\"\n\n");
	
	// Cleanup
	// 清理
	xrtFree(pBuffer);
}

// Test practical buffer usage scenarios
// 测试实际缓冲区使用场景
void TestPracticalScenarios()
{
	printf("=== Practical Buffer Usage Scenarios ===\n");
	printf("=== 实际缓冲区使用场景 ===\n");
	
	// Scenario 1: Log message building
	// 场景1：日志消息构建
	printf("Scenario 1: Log message building\n");
	
	size_t iLogBufferSize = 512;
	str sLogBuffer = xrtMalloc(iLogBufferSize);
	sLogBuffer[0] = '\0';
	
	// Simulate building a log message
	// 模拟构建日志消息
	time_t tNow = time(NULL);
	str sTimestamp = ctime(&tNow);
	sTimestamp[strlen(sTimestamp)-1] = '\0';  // Remove newline
	
	const char* sLevel = "INFO";
	const char* sModule = "Database";
	const char* sMessage = "Connection established successfully";
	
	int iWritten = snprintf(sLogBuffer, iLogBufferSize, 
	                       "[%s] [%s] %s: %s", 
	                       sTimestamp, sLevel, sModule, sMessage);
	
	printf("  Built log message (%d chars):\n", iWritten);
	printf("    \"%s\"\n\n", sLogBuffer);
	
	xrtFree(sLogBuffer);
	
	// Scenario 2: CSV row building
	// 场景2：CSV行构建
	printf("Scenario 2: CSV row building\n");
	
	size_t iCSVBufferSize = 256;
	str sCSVBuffer = xrtMalloc(iCSVBufferSize);
	sCSVBuffer[0] = '\0';
	
	// Sample data
	// 示例数据
	const char* sName = "John Doe";
	int iAge = 30;
	float fSalary = 50000.50f;
	const char* sDepartment = "Engineering";
	
	// Build CSV row
	// 构建CSV行
	snprintf(sCSVBuffer, iCSVBufferSize, "\"%s\",%d,%.2f,\"%s\"", 
	         sName, iAge, fSalary, sDepartment);
	
	printf("  CSV row: %s\n", sCSVBuffer);
	printf("  Parsed back:\n");
	printf("    Name: %s\n", sName);
	printf("    Age: %d\n", iAge);
	printf("    Salary: %.2f\n", fSalary);
	printf("    Department: %s\n\n", sDepartment);
	
	xrtFree(sCSVBuffer);
	
	// Scenario 3: Network packet assembly
	// 场景3：网络包组装
	printf("Scenario 3: Network packet assembly\n");
	
	size_t iPacketBufferSize = 128;
	str sPacketBuffer = xrtMalloc(iPacketBufferSize);
	
	// Simulate packet header and data
	// 模拟包头和数据
	unsigned char arrHeader[] = {0x01, 0x02, 0x03, 0x04};  // 4 bytes header
	unsigned char arrPayload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
	
	size_t iHeaderSize = sizeof(arrHeader);
	size_t iPayloadSize = sizeof(arrPayload);
	size_t iTotalSize = iHeaderSize + iPayloadSize;
	
	if ( iTotalSize <= iPacketBufferSize ) {
		// Assemble packet
		// 组装包
		memcpy(sPacketBuffer, arrHeader, iHeaderSize);
		memcpy(sPacketBuffer + iHeaderSize, arrPayload, iPayloadSize);
		
		printf("  Packet assembled (%zu bytes total):\n", iTotalSize);
		printf("    Header: ");
		for ( size_t i = 0; i < iHeaderSize; i++ ) {
			printf("%02X ", (unsigned char)sPacketBuffer[i]);
		}
		printf("\n    Payload: ");
		for ( size_t i = iHeaderSize; i < iTotalSize; i++ ) {
			printf("%02X ", (unsigned char)sPacketBuffer[i]);
		}
		printf("\n    ASCII: ");
		for ( size_t i = iHeaderSize; i < iTotalSize; i++ ) {
			char c = sPacketBuffer[i];
			printf("%c", (c >= 32 && c <= 126) ? c : '.');
		}
		printf("\n\n");
	}
	
	xrtFree(sPacketBuffer);
	
	// Scenario 4: Configuration string building
	// 场景4：配置字符串构建
	printf("Scenario 4: Configuration string building\n");
	
	size_t iConfigBufferSize = 1024;
	str sConfigBuffer = xrtMalloc(iConfigBufferSize);
	sConfigBuffer[0] = '\0';
	
	// Build configuration
	// 构建配置
	const char* sAppName = "MyApplication";
	int iVersion = 2;
	bool bDebug = TRUE;
	int iPort = 8080;
	const char* sHost = "localhost";
	
	size_t iUsed = 0;
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "[Application]\n");
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "name=%s\n", sAppName);
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "version=%d\n", iVersion);
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "debug=%s\n\n", bDebug ? "true" : "false");
	
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "[Server]\n");
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "host=%s\n", sHost);
	iUsed += snprintf(sConfigBuffer + iUsed, iConfigBufferSize - iUsed,
	                 "port=%d\n", iPort);
	
	printf("  Configuration built (%zu characters):\n%s\n", iUsed, sConfigBuffer);
	
	xrtFree(sConfigBuffer);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Buffer Operations Demo\n");
	printf("XRT 字符串缓冲区操作演示\n");
	printf("===============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicBufferManagement();
	TestSafeStringAppending();
	TestBufferResizing();
	TestCircularBufferOperations();
	TestPracticalScenarios();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}