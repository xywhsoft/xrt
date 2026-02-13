/*
 * XRT Example - String Split and Manual Join
 * XRT 范例 - 字符串分割与手动连接
 *
 * Description / 说明:
 *   EN: Demonstrates string splitting function (xrtSplit) and manual
 *       string concatenation for processing delimited data.
 *   CN: 演示字符串分割函数（xrtSplit）和手动字符串拼接，
 *       用于处理分隔符数据。
 */
/*
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_split_and_join.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_split_and_join -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtSplitStr: split string by delimiter, returns array of substrings
 *   - xrtSplitLine: split string by line breaks (\n or \r\n)
 *   - xrtJoinStr: join array of strings with separator
 *   - All functions allocate new memory that must be freed
 *   - Result arrays are null-terminated
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic string splitting
// 测试基本字符串分割
void TestBasicSplitting()
{
	str sCSV = "apple,banana,cherry,date";
	str sPipe = "red|green|blue|yellow|purple";
	str sSpace = "one two three four five";
	
	printf("=== Basic String Splitting ===\n");
	printf("=== 基本字符串分割 ===\n");
	
	// Split by comma
	// 按逗号分割
	size_t iCount1 = 0;
	str* arrParts1 = xrtSplit(sCSV, strlen(sCSV), ",", 1, FALSE, &iCount1);
	
	printf("Split by comma (\",\"):\n");
	printf("  Source: \"%s\"\n", sCSV);
	printf("  Parts: %zu\n", iCount1);
	for ( size_t i = 0; i < iCount1 && arrParts1[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrParts1[i]);
	}
	printf("\n");
	
	// Split by pipe
	// 按竖线分割
	size_t iCount2 = 0;
	str* arrParts2 = xrtSplit(sPipe, strlen(sPipe), "|", 1, FALSE, &iCount2);
	
	printf("Split by pipe (\"|\"):\n");
	printf("  Source: \"%s\"\n", sPipe);
	printf("  Parts: %zu\n", iCount2);
	for ( size_t i = 0; i < iCount2 && arrParts2[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrParts2[i]);
	}
	printf("\n");
	
	// Split by space
	// 按空格分割
	size_t iCount3 = 0;
	str* arrParts3 = xrtSplit(sSpace, strlen(sSpace), " ", 1, FALSE, &iCount3);
	
	printf("Split by space (\" \"):\n");
	printf("  Source: \"%s\"\n", sSpace);
	printf("  Parts: %zu\n", iCount3);
	for ( size_t i = 0; i < iCount3 && arrParts3[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrParts3[i]);
	}
	printf("\n");
	
	// Cleanup
	// 清理
	xrtFree(arrParts1);
	xrtFree(arrParts2);
	xrtFree(arrParts3);
}

// Test line splitting
// 测试行分割
void TestLineSplitting()
{
	str sText1 = "Line 1\nLine 2\nLine 3\nLine 4";
	str sText2 = "First line\r\nSecond line\r\nThird line";
	str sText3 = "Single line without breaks";
	
	printf("=== Line Splitting ===\n");
	printf("=== 行分割 ===\n");
	
	// Split by \n
	// 按 \n 分割
	size_t iLineCount1 = 0;
	str* arrLines1 = xrtSplit(sText1, strlen(sText1), "\n", 1, FALSE, &iLineCount1);
	
	printf("Split by \\n:\n");
	printf("  Source: \"%s\"\n", sText1);
	printf("  Lines: %zu\n", iLineCount1);
	for ( size_t i = 0; i < iLineCount1 && arrLines1[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrLines1[i]);
	}
	printf("\n");
	
	// Split by \r\n
	// 按 \r\n 分割
	size_t iLineCount2 = 0;
	str* arrLines2 = xrtSplit(sText2, strlen(sText2), "\r\n", 2, FALSE, &iLineCount2);
	
	printf("Split by \\r\\n:\n");
	printf("  Source: \"%s\"\n", sText2);
	printf("  Lines: %zu\n", iLineCount2);
	for ( size_t i = 0; i < iLineCount2 && arrLines2[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrLines2[i]);
	}
	printf("\n");
	
	// Single line
	// 单行
	size_t iLineCount3 = 0;
	str* arrLines3 = xrtSplit(sText3, strlen(sText3), "\n", 1, FALSE, &iLineCount3);
	
	printf("Single line:\n");
	printf("  Source: \"%s\"\n", sText3);
	printf("  Lines: %zu\n", iLineCount3);
	for ( size_t i = 0; i < iLineCount3 && arrLines3[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrLines3[i]);
	}
	printf("\n");
	
	// Cleanup
	// 清理
	xrtFree(arrLines1);
	xrtFree(arrLines2);
	xrtFree(arrLines3);
}

// Manual string joining demonstration
// 手动字符串连接演示
void TestManualJoining()
{
	printf("=== Manual String Joining ===\n");
	printf("=== 手动字符串连接 ===\n");
	
	// Demonstrate manual joining using buffer operations
	// 演示使用缓冲区操作的手动连接
	str arrWords[] = { "Hello", "World", "from", "XRT" };
	size_t iCount = sizeof(arrWords) / sizeof(arrWords[0]);
	
	printf("Manual joining demonstration:\n");
	printf("  Parts: ");
	for ( size_t i = 0; i < iCount; i++ ) {
		printf("\"%s\" ", arrWords[i]);
	}
	printf("\n");
	
	// Calculate total length needed
	// 计算所需总长度
	size_t iTotalLen = 0;
	for ( size_t i = 0; i < iCount; i++ ) {
		iTotalLen += strlen(arrWords[i]);
	}
	iTotalLen += (iCount - 1) * 1;  // Space separators
	
	// Allocate and build result
	// 分配并构建结果
	str sResult = xrtMalloc(iTotalLen + 1);
	sResult[0] = '\0';
	
	for ( size_t i = 0; i < iCount; i++ ) {
		strcat(sResult, arrWords[i]);
		if ( i < iCount - 1 ) {
			strcat(sResult, " ");
		}
	}
	
	printf("  Joined with spaces: \"%s\"\n\n", sResult);
	
	// Cleanup
	// 清理
	xrtFree(sResult);
}

// Test edge cases and special scenarios
// 测试边界情况和特殊场景
void TestEdgeCases()
{
	printf("=== Edge Cases ===\n");
	printf("=== 边界情况 ===\n");
	
	// Empty string splitting
	// 空字符串分割
	str sEmpty = "";
	size_t iEmptyCount = 0;
	str* arrEmpty = xrtSplit(sEmpty, strlen(sEmpty), ",", 1, FALSE, &iEmptyCount);
	printf("Split empty string:\n");
	printf("  Source: \"\"\n");
	printf("  Parts: %zu\n", iEmptyCount);
	if ( iEmptyCount > 0 && arrEmpty[0] ) {
		printf("  Result: \"%s\"\n", arrEmpty[0]);
	}
	printf("\n");
	
	// Consecutive delimiters
	// 连续分隔符
	str sConsecutive = "a,,b,,,c,d";
	size_t iConsecCount = 0;
	str* arrConsec = xrtSplit(sConsecutive, strlen(sConsecutive), ",", 1, FALSE, &iConsecCount);
	printf("Consecutive delimiters:\n");
	printf("  Source: \"%s\"\n", sConsecutive);
	printf("  Parts: %zu\n", iConsecCount);
	for ( size_t i = 0; i < iConsecCount; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrConsec[i] ? arrConsec[i] : "(empty)");
	}
	printf("\n");
	
	// Leading/trailing delimiters
	// 前导/尾随分隔符
	str sLeading = ",apple,banana,";
	size_t iLeadCount = 0;
	str* arrLead = xrtSplit(sLeading, strlen(sLeading), ",", 1, FALSE, &iLeadCount);
	printf("Leading/trailing delimiters:\n");
	printf("  Source: \"%s\"\n", sLeading);
	printf("  Parts: %zu\n", iLeadCount);
	for ( size_t i = 0; i < iLeadCount; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrLead[i] ? arrLead[i] : "(empty)");
	}
	printf("\n");
	
	// Multi-character delimiter
	// 多字符分隔符
	str sMulti = "name::value::data::info";
	size_t iMultiCount = 0;
	str* arrMulti = xrtSplit(sMulti, strlen(sMulti), "::", 2, FALSE, &iMultiCount);
	printf("Multi-character delimiter (\"::\"):\n");
	printf("  Source: \"%s\"\n", sMulti);
	printf("  Parts: %zu\n", iMultiCount);
	for ( size_t i = 0; i < iMultiCount; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrMulti[i]);
	}
	printf("\n");
	
	// NULL pointer tests
	// 空指针测试
	size_t iNullCount = 0;
	str* arrNull = xrtSplit(NULL, 0, ",", 1, FALSE, &iNullCount);
	printf("NULL pointer split:\n");
	printf("  Parts count: %zu\n\n", iNullCount);
	
	printf("NULL pointer join:\n");
	printf("  Result: \"(manual implementation required)\"\n\n");
	
	// Cleanup
	// 清理
	xrtFree(arrEmpty);
	xrtFree(arrConsec);
	xrtFree(arrLead);
	xrtFree(arrMulti);
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// CSV parsing
	// CSV 解析
	str sCSV = "John,Doe,25,Engineer";
	size_t iCSVCount = 0;
	str* arrCSV = xrtSplit(sCSV, strlen(sCSV), ",", 1, FALSE, &iCSVCount);
	
	printf("CSV Parsing:\n");
	printf("  Record: \"%s\"\n", sCSV);
	if ( iCSVCount >= 4 ) {
		printf("  Parsed:\n");
		printf("    First Name: %s\n", arrCSV[0]);
		printf("    Last Name:  %s\n", arrCSV[1]);
		printf("    Age:        %s\n", arrCSV[2]);
		printf("    Job:        %s\n", arrCSV[3]);
	}
	printf("\n");
	
	// Path processing
	// 路径处理
	str sPath = "/usr/local/bin/program";
	size_t iPathCount = 0;
	str* arrPath = xrtSplit(sPath, strlen(sPath), "/", 1, FALSE, &iPathCount);
	
	printf("Path Processing:\n");
	printf("  Path: \"%s\"\n", sPath);
	printf("  Components (%zu):\n", iPathCount);
	for ( size_t i = 0; i < iPathCount; i++ ) {
		if ( arrPath[i] && strlen(arrPath[i]) > 0 ) {
			printf("    [%zu]: \"%s\"\n", i, arrPath[i]);
		}
	}
	
	// Reconstruct path manually
	// 手动重建路径
	size_t iTotalLen = 0;
	for ( size_t i = 0; i < iPathCount && arrPath[i]; i++ ) {
		iTotalLen += strlen(arrPath[i]) + 1;  // +1 for '/'
	}
	str sReconstructed = xrtMalloc(iTotalLen + 1);
	sReconstructed[0] = '\0';
	for ( size_t i = 0; i < iPathCount && arrPath[i]; i++ ) {
		if ( i > 0 ) strcat(sReconstructed, "/");
		strcat(sReconstructed, arrPath[i]);
	}
	printf("  Reconstructed: \"%s\"\n\n", sReconstructed);
	xrtFree(sReconstructed);
	
	// Configuration parsing
	// 配置解析
	str sConfig = "host=localhost;port=8080;timeout=30";
	size_t iConfigCount = 0;
	str* arrConfig = xrtSplit(sConfig, strlen(sConfig), ";", 1, FALSE, &iConfigCount);
	
	printf("Configuration Parsing:\n");
	printf("  Config: \"%s\"\n", sConfig);
	printf("  Settings:\n");
	for ( size_t i = 0; i < iConfigCount && arrConfig[i]; i++ ) {
		size_t iSettingCount = 0;
		str* arrSetting = xrtSplit(arrConfig[i], strlen(arrConfig[i]), "=", 1, FALSE, &iSettingCount);
		if ( iSettingCount >= 2 && arrSetting[0] && arrSetting[1] ) {
			printf("    %s = %s\n", arrSetting[0], arrSetting[1]);
		}
		// Cleanup setting
		xrtFree(arrSetting);
	}
	printf("\n");
	
	// Text processing
	// 文本处理
	str sParagraph = "This is the first sentence. This is the second sentence. And this is the third.";
	size_t iSentenceCount = 0;
	str* arrSentences = xrtSplit(sParagraph, strlen(sParagraph), ". ", 2, FALSE, &iSentenceCount);
	
	printf("Text Processing (sentence splitting):\n");
	printf("  Paragraph: \"%s\"\n", sParagraph);
	printf("  Sentences (%zu):\n", iSentenceCount);
	for ( size_t i = 0; i < iSentenceCount && arrSentences[i]; i++ ) {
		printf("    [%zu]: \"%s\"\n", i, arrSentences[i]);
	}
	printf("\n");
	
	// Cleanup
	// 清理
	xrtFree(arrCSV);
	xrtFree(arrPath);
	
	for ( size_t i = 0; i < iConfigCount && arrConfig[i]; i++ ) {
		xrtFree(arrConfig[i]);
	}
	xrtFree(arrConfig);
	
	xrtFree(arrSentences);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Split and Join Demo\n");
	printf("XRT 字符串分割与连接演示\n");
	printf("============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicSplitting();
	TestLineSplitting();
	TestManualJoining();
	TestEdgeCases();
	TestPracticalExamples();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}