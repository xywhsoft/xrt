/*
 * XRT Example - File ReadAll/WriteAll
 * XRT 范例 - 文件一次性读写
 *
 * Description / 说明:
 *   EN: Demonstrates xrtFileReadAll, xrtFileWriteAll, xrtFileAppend
 *       for simple whole-file operations.
 *   CN: 演示 xrtFileReadAll、xrtFileWriteAll、xrtFileAppend 简单的全文件操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_file_readall.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_file_readall              (Linux, GCC)
 *
 * Note / 注意:
 *   - Functions handle open/close automatically
 *   - xrtFileReadAll allocates memory, must free with xrtFree
 *   - Returns NULL on failure
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test FileWriteAll
// 测试 FileWriteAll
void TestWriteAll()
{
	printf("=== xrtFileWriteAll ===\n");
	printf("=== xrtFileWriteAll ===\n");
	
	str sPath = "examples/bin/writeall_test.txt";
	str sContent = "This is content written with xrtFileWriteAll.\n"
	               "这是使用 xrtFileWriteAll 写入的内容。\n"
	               "Line 3 - 第三行\n";
	
	int iResult = xrtFileWriteAll(sPath, sContent, strlen(sContent), 0);
	
	if ( iResult >= 0 ) {
		printf("Successfully wrote to %s\n", sPath);
		printf("成功写入 %s\n", sPath);
		printf("Bytes written: %d\n", iResult);
		printf("写入字节数: %d\n", iResult);
	} else {
		printf("Failed to write file.\n");
		printf("写入文件失败。\n");
	}
	printf("\n");
}

// Test FileReadAll
// 测试 FileReadAll
void TestReadAll()
{
	printf("=== xrtFileReadAll ===\n");
	printf("=== xrtFileReadAll ===\n");
	
	str sPath = "examples/bin/writeall_test.txt";
	size_t iSize;
	
	str sContent = xrtFileReadAll(sPath, 0, &iSize);
	
	if ( sContent ) {
		printf("Read %zu bytes from %s\n", iSize, sPath);
		printf("从 %s 读取 %zu 字节\n", sPath, iSize);
		printf("--- Content ---\n");
		printf("%s", sContent);
		printf("--- End ---\n");
		xrtFree(sContent);
	} else {
		printf("Failed to read file.\n");
		printf("读取文件失败。\n");
	}
	printf("\n");
}

// Test FileAppend
// 测试 FileAppend
void TestAppend()
{
	printf("=== xrtFileAppend ===\n");
	printf("=== xrtFileAppend ===\n");
	
	str sPath = "examples/bin/append_test.txt";
	
	// Write initial content
	// 写入初始内容
	str sInitial = "Initial content.\n初始内容。\n";
	xrtFileWriteAll(sPath, sInitial, strlen(sInitial), 0);
	printf("Created file with initial content.\n");
	printf("创建文件并写入初始内容。\n");
	
	// Append more content
	// 追加更多内容
	str sAppend1 = "Appended line 1.\n追加的第1行。\n";
	str sAppend2 = "Appended line 2.\n追加的第2行。\n";
	
	xrtFileAppend(sPath, sAppend1, strlen(sAppend1), 0);
	xrtFileAppend(sPath, sAppend2, strlen(sAppend2), 0);
	
	printf("Appended 2 more blocks.\n");
	printf("追加了 2 个块。\n\n");
	
	// Read and show final content
	// 读取并显示最终内容
	size_t iSize;
	str sContent = xrtFileReadAll(sPath, 0, &iSize);
	if ( sContent ) {
		printf("Final content (%zu bytes):\n", iSize);
		printf("最终内容 (%zu 字节):\n", iSize);
		printf("%s", sContent);
		xrtFree(sContent);
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Save JSON data
	// 保存 JSON 数据
	str sJsonPath = "examples/bin/data.json";
	str sJson = "{\n"
	             "  \"name\": \"XRT Example\",\n"
	             "  \"version\": \"1.0.0\",\n"
	             "  \"enabled\": true,\n"
	             "  \"count\": 42\n"
	             "}\n";
	
	xrtFileWriteAll(sJsonPath, sJson, strlen(sJson), 0);
	printf("Saved JSON to %s\n", sJsonPath);
	printf("JSON 保存到 %s\n", sJsonPath);
	
	// Read and display
	// 读取并显示
	size_t iSize;
	str sReadJson = xrtFileReadAll(sJsonPath, 0, &iSize);
	if ( sReadJson ) {
		printf("JSON content:\n%s", sReadJson);
		xrtFree(sReadJson);
	}
	printf("\n");
	
	// Log file with append
	// 使用追加的日志文件
	str sLogPath = "examples/bin/activity.log";
	
	xtime tNow = xrtNow();
	str sTimestamp = xrtTimeFormat(tNow, "yyyy-MM-dd HH:mm:ss");
	str sLogEntry = xrtFormat("[%s] User logged in\n", sTimestamp);
	
	xrtFileAppend(sLogPath, sLogEntry, strlen(sLogEntry), 0);
	printf("Log entry appended to %s\n", sLogPath);
	printf("日志条目追加到 %s\n", sLogPath);
	
	xrtFree(sTimestamp);
	xrtFree(sLogEntry);
	
	// Show log
	// 显示日志
	str sLogContent = xrtFileReadAll(sLogPath, 0, &iSize);
	if ( sLogContent ) {
		printf("Log content:\n%s", sLogContent);
		xrtFree(sLogContent);
	}
	printf("\n");
}

// Test error handling
// 测试错误处理
void TestErrorHandling()
{
	printf("=== Error Handling ===\n");
	printf("=== 错误处理 ===\n");
	
	// Try to read non-existent file
	// 尝试读取不存在的文件
	str sPath = "examples/bin/nonexistent_file_xyz.txt";
	size_t iSize;
	
	str sContent = xrtFileReadAll(sPath, 0, &iSize);
	
	if ( sContent == NULL ) {
		str sError = xrtGetError();
		const char* sText = (sError && sError[0]) ? (const char*)sError : "(empty)";

		printf("Correctly returned NULL for non-existent file.\n");
		printf("对不存在的文件正确返回 NULL。\n");
		printf("LastError: %s\n", sText);
	} else {
		printf("Unexpected: got content from non-existent file.\n");
		xrtFree(sContent);
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File - ReadAll/WriteAll Demo\n");
	printf("XRT 文件 - 一次性读写演示\n");
	printf("================================\n\n");
	
	TestWriteAll();
	TestReadAll();
	TestAppend();
	TestPracticalUseCases();
	TestErrorHandling();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
