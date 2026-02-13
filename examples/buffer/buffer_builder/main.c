/*
 * XRT Example - Buffer Builder
 * XRT 范例 - 缓冲区构建器
 *
 * Description / 说明:
 *   EN: Demonstrates using xbuffer as a StringBuilder for efficient
 *       string concatenation and manipulation.
 *   CN: 演示使用 xbuffer 作为 StringBuilder 进行高效字符串拼接和操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/buffer_buffer_builder.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/buffer_buffer_builder              (Linux, GCC)
 *
 * Note / 注意:
 *   - More efficient than repeated xrtFormat/strcat
 *   - Auto-expands as needed
 *   - Access result via buf->Buffer
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test HTML builder
// 测试 HTML 构建器
void TestHTMLBuilder()
{
	printf("=== HTML Builder ===\n");
	printf("=== HTML 构建器 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build HTML document
	// 构建 HTML 文档
	xrtBufferAppend(pBuf, "<!DOCTYPE html>\n", 15, FALSE);
	xrtBufferAppend(pBuf, "<html>\n", 7, FALSE);
	xrtBufferAppend(pBuf, "<head>\n", 7, FALSE);
	xrtBufferAppend(pBuf, "  <title>Test Page</title>\n", 26, FALSE);
	xrtBufferAppend(pBuf, "</head>\n", 8, FALSE);
	xrtBufferAppend(pBuf, "<body>\n", 7, FALSE);
	xrtBufferAppend(pBuf, "  <h1>Hello World</h1>\n", 22, FALSE);
	xrtBufferAppend(pBuf, "  <p>This is a test page.</p>\n", 29, FALSE);
	xrtBufferAppend(pBuf, "</body>\n", 8, FALSE);
	xrtBufferAppend(pBuf, "</html>", 7, TRUE);
	
	printf("HTML output:\n%s\n", pBuf->Buffer);
	printf("HTML 输出:\n%s\n", pBuf->Buffer);
	
	xrtBufferDestroy(pBuf);
}

// Test JSON builder
// 测试 JSON 构建器
void TestJSONBuilder()
{
	printf("=== JSON Builder ===\n");
	printf("=== JSON 构建器 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build JSON object
	// 构建 JSON 对象
	xrtBufferAppend(pBuf, "{\n", 2, FALSE);
	xrtBufferAppend(pBuf, "  \"name\": \"John Doe\",\n", 22, FALSE);
	xrtBufferAppend(pBuf, "  \"age\": 30,\n", 13, FALSE);
	xrtBufferAppend(pBuf, "  \"email\": \"john@example.com\",\n", 30, FALSE);
	xrtBufferAppend(pBuf, "  \"active\": true\n", 17, FALSE);
	xrtBufferAppend(pBuf, "}", 1, TRUE);
	
	printf("JSON output:\n%s\n\n", pBuf->Buffer);
	printf("JSON 输出:\n%s\n\n", pBuf->Buffer);
	
	xrtBufferDestroy(pBuf);
}

// Test log builder
// 测试日志构建器
void TestLogBuilder()
{
	printf("=== Log Builder ===\n");
	printf("=== 日志构建器 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build log entries
	// 构建日志条目
	xtime tNow = xrtNow();
	int i;
	for ( i = 1; i <= 5; i++ ) {
		str sTime = xrtTimeFormat(tNow, "HH:mm:ss");
		xrtBufferAppend(pBuf, "[", 1, FALSE);
		xrtBufferAppend(pBuf, sTime, strlen(sTime), FALSE);
		xrtBufferAppend(pBuf, "] Log entry ", 12, FALSE);
		
		char sNum[16];
		sprintf(sNum, "%d\n", i);
		xrtBufferAppend(pBuf, sNum, strlen(sNum), FALSE);
		
		xrtFree(sTime);
	}
	xrtBufferAppend(pBuf, "", 0, TRUE);
	
	printf("Log output:\n%s", pBuf->Buffer);
	printf("日志输出:\n%s", pBuf->Buffer);
	printf("\n");
	
	xrtBufferDestroy(pBuf);
}

// Test conditional building
// 测试条件构建
void TestConditionalBuilding()
{
	printf("=== Conditional Building ===\n");
	printf("=== 条件构建 ===\n");
	
	xbuffer pBuf = xrtBufferCreate(0);
	
	// Build SQL query conditionally
	// 条件构建 SQL 查询
	char* psName = "Alice";
	int iMinAge = 18;
	int iMaxAge = 0;  // 0 means no limit
	int bActive = TRUE;
	
	xrtBufferAppend(pBuf, "SELECT * FROM users WHERE 1=1", 29, FALSE);
	
	// Add name condition
	// 添加姓名条件
	if ( psName && strlen(psName) > 0 ) {
		xrtBufferAppend(pBuf, " AND name = '", 13, FALSE);
		xrtBufferAppend(pBuf, psName, strlen(psName), FALSE);
		xrtBufferAppend(pBuf, "'", 1, FALSE);
	}
	
	// Add min age condition
	// 添加最小年龄条件
	if ( iMinAge > 0 ) {
		char sCond[64];
		sprintf(sCond, " AND age >= %d", iMinAge);
		xrtBufferAppend(pBuf, sCond, strlen(sCond), FALSE);
	}
	
	// Add max age condition
	// 添加最大年龄条件
	if ( iMaxAge > 0 ) {
		char sCond[64];
		sprintf(sCond, " AND age <= %d", iMaxAge);
		xrtBufferAppend(pBuf, sCond, strlen(sCond), FALSE);
	}
	
	// Add active condition
	// 添加活跃条件
	if ( bActive ) {
		xrtBufferAppend(pBuf, " AND active = 1", 15, TRUE);
	}
	
	printf("SQL query:\n%s\n\n", pBuf->Buffer);
	printf("SQL 查询:\n%s\n\n", pBuf->Buffer);
	
	xrtBufferDestroy(pBuf);
}

// Test performance vs strcat
// 测试与 strcat 的性能对比
void TestPerformance()
{
	printf("=== Performance Test ===\n");
	printf("=== 性能测试 ===\n");
	
	int iIterations = 10000;
	
	// Using xbuffer
	// 使用 xbuffer
	double fStart = xrtTimer();
	int i;
	for ( i = 0; i < iIterations; i++ ) {
		xbuffer pBuf = xrtBufferCreate(0);
		xrtBufferAppend(pBuf, "Part1", 5, FALSE);
		xrtBufferAppend(pBuf, "Part2", 5, FALSE);
		xrtBufferAppend(pBuf, "Part3", 5, FALSE);
		xrtBufferAppend(pBuf, "Part4", 5, FALSE);
		xrtBufferAppend(pBuf, "Part5", 5, TRUE);
		xrtBufferDestroy(pBuf);
	}
	double fBufferTime = xrtTimer() - fStart;
	
	printf("xbuffer: %.3f ms for %d iterations\n", fBufferTime * 1000, iIterations);
	printf("xbuffer: %d 次迭代耗时 %.3f 毫秒\n", iIterations, fBufferTime * 1000);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Buffer - Buffer Builder Demo\n");
	printf("XRT 缓冲区 - 缓冲区构建器演示\n");
	printf("================================\n\n");
	
	TestHTMLBuilder();
	TestJSONBuilder();
	TestLogBuilder();
	TestConditionalBuilding();
	TestPerformance();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
