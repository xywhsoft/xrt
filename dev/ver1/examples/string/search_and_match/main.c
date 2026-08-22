/*
 * XRT Example - String Search and Match
 * XRT 范例 - 字符串搜索与匹配
 *
 * Description / 说明:
 *   EN: Demonstrates string search functions (xrtFindStr/xrtInStr) and
 *       pattern matching functions (xrtCheckStr/xrtStrLike) with wildcard support.
 *   CN: 演示字符串搜索函数（xrtFindStr/xrtInStr）和模式匹配函数（xrtCheckStr/xrtStrLike），
 *       支持通配符匹配。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_search_and_match.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_search_and_match -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtFindStr: find first occurrence, returns position (1-based) or 0 if not found
 *   - xrtInStr: case-insensitive version of xrtFindStr
 *   - xrtCheckStr: exact string matching (bCase controls case sensitivity)
 *   - xrtStrLike: wildcard pattern matching (* = any sequence, ? = single UTF-8 char)
 *   - All positions are 1-based (0 means not found)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic string search
// 测试基本字符串搜索
void TestBasicSearch()
{
	str sText = "Hello World! This is a test string for searching.";
	str sPattern1 = "World";
	str sPattern2 = "test";
	str sPattern3 = "xyz";
	
	printf("=== Basic String Search ===\n");
	printf("=== 基本字符串搜索 ===\n");
	printf("Text: \"%s\"\n", sText);
	printf("Length: %zu\n\n", strlen(sText));
	
	// xrtFindStr - case sensitive
	// xrtFindStr - 区分大小写
	str sResult1 = xrtFindStr(sText, strlen(sText), sPattern1, strlen(sPattern1), TRUE);
	str sResult2 = xrtFindStr(sText, strlen(sText), sPattern2, strlen(sPattern2), TRUE);
	str sResult3 = xrtFindStr(sText, strlen(sText), sPattern3, strlen(sPattern3), TRUE);
	
	int iPos1 = sResult1 ? (sResult1 - sText + 1) : 0;  // Convert to 1-based position
	int iPos2 = sResult2 ? (sResult2 - sText + 1) : 0;
	int iPos3 = sResult3 ? (sResult3 - sText + 1) : 0;
	
	printf("xrtFindStr (case-sensitive):\n");
	printf("  Find \"%s\": position %d%s\n", 
	       sPattern1, iPos1, iPos1 > 0 ? "" : " (NOT FOUND)");
	printf("  Find \"%s\": position %d%s\n", 
	       sPattern2, iPos2, iPos2 > 0 ? "" : " (NOT FOUND)");
	printf("  Find \"%s\": position %d%s\n\n", 
	       sPattern3, iPos3, iPos3 > 0 ? "" : " (NOT FOUND)");
	
	// xrtInStr - case insensitive
	// xrtInStr - 不区分大小写
	uint iPos4_u = xrtInStr(sText, strlen(sText), "world", 5, FALSE);  // Should find "World"
	uint iPos5_u = xrtInStr(sText, strlen(sText), "TEST", 4, FALSE);   // Should find "test"
	
	int iPos4 = (int)iPos4_u;
	int iPos5 = (int)iPos5_u;
	
	printf("xrtInStr (case-insensitive):\n");
	printf("  Find \"world\": position %d\n", iPos4);
	printf("  Find \"TEST\": position %d\n\n", iPos5);
	
	// Search with offset
	// 带偏移量搜索
	// Note: xrtFindStr doesn't support offset directly, need manual implementation
	// 注意：xrtFindStr 不直接支持偏移量，需要手动实现
	str sOffset1 = sText + 9;  // Start from position 10 (0-based index 9)
	str sOffset2 = sText + 19; // Start from position 20 (0-based index 19)
	
	str sResult6 = xrtFindStr(sOffset1, strlen(sOffset1), "is", 2, TRUE);
	str sResult7 = xrtFindStr(sOffset2, strlen(sOffset2), "is", 2, TRUE);
	
	int iPos6 = sResult6 ? (sResult6 - sText + 1) : 0;
	int iPos7 = sResult7 ? (sResult7 - sText + 1) : 0;
	
	printf("Search with offset:\n");
	printf("  Find \"is\" starting from pos 10: position %d\n", iPos6);
	printf("  Find \"is\" starting from pos 20: position %d\n\n", iPos7);
}

// Test exact string matching
// 测试精确字符串匹配
void TestExactMatching()
{
	str sText = "Hello World";
	str sMatch1 = "Hello World";
	str sMatch2 = "hello world";
	str sMatch3 = "Hello";
	
	printf("=== Exact String Matching ===\n");
	printf("=== 精确字符串匹配 ===\n");
	printf("Text: \"%s\"\n\n", sText);
	
	// Case sensitive matching
	// 区分大小写匹配
	str sCheck1 = xrtCheckStr(sText, strlen(sText), sMatch1, strlen(sMatch1));
	str sCheck2 = xrtCheckStr(sText, strlen(sText), sMatch2, strlen(sMatch2));
	str sCheck3 = xrtCheckStr(sText, strlen(sText), sMatch3, strlen(sMatch3));
	
	bool bMatch1 = (sCheck1 != NULL);
	bool bMatch2 = (sCheck2 != NULL);
	bool bMatch3 = (sCheck3 != NULL);
	
	printf("xrtCheckStr (case-sensitive):\n");
	printf("  \"%s\" == \"%s\": %s\n", sText, sMatch1, bMatch1 ? "TRUE" : "FALSE");
	printf("  \"%s\" == \"%s\": %s\n", sText, sMatch2, bMatch2 ? "TRUE" : "FALSE");
	printf("  \"%s\" == \"%s\": %s\n\n", sText, sMatch3, bMatch3 ? "TRUE" : "FALSE");
	
	// Case insensitive matching
	// 不区分大小写匹配
	// Note: xrtCheckStr is always case-sensitive, need to convert both strings
	// 注意：xrtCheckStr 总是区分大小写，需要转换字符串
	str sLowerText = xrtCopyStr(sText, strlen(sText) + 1);
	xrtLCase(sLowerText, strlen(sLowerText) + 1, TRUE);
	
	str sLowerMatch2 = xrtCopyStr(sMatch2, strlen(sMatch2) + 1);
	xrtLCase(sLowerMatch2, strlen(sLowerMatch2) + 1, TRUE);
	
	str sLowerMatch3 = xrtCopyStr(sMatch3, strlen(sMatch3) + 1);
	xrtLCase(sLowerMatch3, strlen(sLowerMatch3) + 1, TRUE);
	
	str sCheck4 = xrtCheckStr(sLowerText, strlen(sLowerText), sLowerMatch2, strlen(sLowerMatch2));
	str sCheck5 = xrtCheckStr(sLowerText, strlen(sLowerText), sLowerMatch3, strlen(sLowerMatch3));
	
	bool bMatch4 = (sCheck4 != NULL);
	bool bMatch5 = (sCheck5 != NULL);
	
	xrtFree(sLowerText);
	xrtFree(sLowerMatch2);
	xrtFree(sLowerMatch3);
	
	printf("xrtCheckStr (case-insensitive):\n");
	printf("  \"%s\" == \"%s\": %s\n", sText, sMatch2, bMatch4 ? "TRUE" : "FALSE");
	printf("  \"%s\" == \"%s\": %s\n\n", sText, sMatch3, bMatch5 ? "TRUE" : "FALSE");
}

// Test wildcard pattern matching
// 测试通配符模式匹配
void TestWildcardMatching()
{
	str sText1 = "Hello World";
	str sText2 = "test123.txt";
	str sText3 = "config.ini";
	str sText4 = "abc123def";
	
	printf("=== Wildcard Pattern Matching ===\n");
	printf("=== 通配符模式匹配 ===\n");
	
	// Simple wildcard patterns
	// 简单通配符模式
	printf("Simple patterns:\n");
	printf("  Text: \"%s\"\n", sText1);
	printf("  Pattern \"Hello*\": %s\n", xrtStrLike(sText1, strlen(sText1), "Hello*", 6, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"*World\": %s\n", xrtStrLike(sText1, strlen(sText1), "*World", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"*o W*\": %s\n", xrtStrLike(sText1, strlen(sText1), "*o W*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"Hello?World\": %s\n", xrtStrLike(sText1, strlen(sText1), "Hello?World", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"H*o*W*d\": %s\n\n", xrtStrLike(sText1, strlen(sText1), "H*o*W*d", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	// File pattern matching
	// 文件模式匹配
	printf("File patterns:\n");
	printf("  Text: \"%s\"\n", sText2);
	printf("  Pattern \"*.txt\": %s\n", xrtStrLike(sText2, strlen(sText2), "*.txt", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"test*.txt\": %s\n", xrtStrLike(sText2, strlen(sText2), "test*.txt", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"test12?.txt\": %s\n", xrtStrLike(sText2, strlen(sText2), "test12?.txt", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"test*.doc\": %s\n\n", xrtStrLike(sText2, strlen(sText2), "test*.doc", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	printf("  Text: \"%s\"\n", sText3);
	printf("  Pattern \"*.ini\": %s\n", xrtStrLike(sText3, strlen(sText3), "*.ini", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"conf*.ini\": %s\n", xrtStrLike(sText3, strlen(sText3), "conf*.ini", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"config.*\": %s\n\n", xrtStrLike(sText3, strlen(sText3), "config.*", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	// Complex patterns with multiple wildcards
	// 复杂模式（多个通配符）
	printf("Complex patterns:\n");
	printf("  Text: \"%s\"\n", sText4);
	printf("  Pattern \"*123*\": %s\n", xrtStrLike(sText4, strlen(sText4), "*123*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"a*c*f\": %s\n", xrtStrLike(sText4, strlen(sText4), "a*c*f", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"a?c?e?f\": %s\n", xrtStrLike(sText4, strlen(sText4), "a?c?e?f", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"a*b*c*d*e*f*\": %s\n\n", xrtStrLike(sText4, strlen(sText4), "a*b*c*d*e*f*", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	// Case sensitivity in wildcards
	// 通配符中的大小写敏感性
	printf("Case sensitivity:\n");
	printf("  Text: \"%s\"\n", sText1);
	printf("  Pattern \"hello*\" (case-sensitive): %s\n", 
	       xrtStrLike(sText1, strlen(sText1), "hello*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"hello*\" (case-insensitive): %s\n\n", 
	       xrtStrLike(sText1, strlen(sText1), "hello*", 1, TRUE) ? "MATCH" : "NO MATCH");
}

// Test edge cases and special patterns
// 测试边界情况和特殊模式
void TestEdgeCases()
{
	printf("=== Edge Cases and Special Patterns ===\n");
	printf("=== 边界情况和特殊模式 ===\n");
	
	// Empty strings
	// 空字符串
	printf("Empty string tests:\n");
	printf("  \"\" vs \"\": %s\n", xrtStrLike("", 0, "", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  \"\" vs \"*\": %s\n", xrtStrLike("", 0, "*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  \"\" vs \"?\": %s\n", xrtStrLike("", 0, "?", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  \"abc\" vs \"\": %s\n\n", xrtStrLike("abc", 0, "", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	// Only wildcards
	// 仅通配符
	printf("Only wildcards:\n");
	str sTest = "anything";
	printf("  Text: \"%s\"\n", sTest);
	printf("  Pattern \"*\": %s\n", xrtStrLike(sTest, 0, "*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"***\": %s\n", xrtStrLike(sTest, 0, "***", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"?????????\": %s (%zu chars)\n\n", 
	       xrtStrLike(sTest, 0, "?????????", 0, TRUE) ? "MATCH" : "NO MATCH", strlen(sTest));
	
	// Multiple consecutive wildcards
	// 连续多个通配符
	printf("Consecutive wildcards:\n");
	str sText = "abcdef";
	printf("  Text: \"%s\"\n", sText);
	printf("  Pattern \"a**f\": %s\n", xrtStrLike(sText, 0, "a**f", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"a??f\": %s\n", xrtStrLike(sText, 0, "a??f", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"*b*d*\": %s\n", xrtStrLike(sText, 0, "*b*d*", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  Pattern \"a???ef\": %s\n\n", xrtStrLike(sText, 0, "a???ef", 0, TRUE) ? "MATCH" : "NO MATCH");
	
	// NULL pointers
	// 空指针
	printf("NULL pointer tests:\n");
	printf("  NULL vs \"test\": %s\n", xrtStrLike(NULL, 0, "test", 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  \"test\" vs NULL: %s\n", xrtStrLike("test", 0, NULL, 0, TRUE) ? "MATCH" : "NO MATCH");
	printf("  NULL vs NULL: %s\n\n", xrtStrLike(NULL, 0, NULL, 0, TRUE) ? "MATCH" : "NO MATCH");
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// File filtering
	// 文件过滤
	str arrFiles[] = {
		"document.txt",
		"image.jpg",
		"data.xml",
		"backup.bak",
		"readme.md",
		"config.ini"
	};
	int iFileCount = sizeof(arrFiles) / sizeof(arrFiles[0]);
	
	printf("File filtering (*.txt or *.md):\n");
	for ( int i = 0; i < iFileCount; i++ )
	{
		bool bMatch = xrtStrLike(arrFiles[i], 0, "*.txt", 0, TRUE) || 
		              xrtStrLike(arrFiles[i], 0, "*.md", 0, TRUE);
		printf("  %-15s -> %s\n", arrFiles[i], bMatch ? "SELECT" : "SKIP");
	}
	printf("\n");
	
	// Log level filtering
	// 日志级别过滤
	str arrLogs[] = {
		"[ERROR] Something went wrong",
		"[INFO] Process started",
		"[DEBUG] Variable value: 123",
		"[WARNING] Disk space low",
		"[ERROR] Connection failed"
	};
	int iLogCount = sizeof(arrLogs) / sizeof(arrLogs[0]);
	
	printf("Error log filtering ([ERROR]*):\n");
	for ( int i = 0; i < iLogCount; i++ )
	{
		bool bIsError = xrtStrLike(arrLogs[i], 0, "[ERROR]*", 0, TRUE);
		printf("  %-35s -> %s\n", arrLogs[i], bIsError ? "ERROR" : "OTHER");
	}
	printf("\n");
	
	// User input validation
	// 用户输入验证
	str arrInputs[] = {
		"user123",
		"User_Name",
		"user-name",
		"user@domain",
		"123user"
	};
	int iInputCount = sizeof(arrInputs) / sizeof(arrInputs[0]);
	
	printf("Valid username pattern ([a-zA-Z][a-zA-Z0-9_]*) - simulated:\n");
	for ( int i = 0; i < iInputCount; i++ )
	{
		// Simulate pattern: starts with letter, then alphanumeric or underscore
		// 模拟模式：以字母开头，后跟字母数字或下划线
		bool bValid = FALSE;
		if ( strlen(arrInputs[i]) > 0 )
		{
			char cFirst = arrInputs[i][0];
			if ( (cFirst >= 'a' && cFirst <= 'z') || (cFirst >= 'A' && cFirst <= 'Z') )
			{
				bValid = TRUE;
				for ( size_t j = 1; j < strlen(arrInputs[i]); j++ )
				{
					char c = arrInputs[i][j];
					if ( !((c >= 'a' && c <= 'z') || 
					       (c >= 'A' && c <= 'Z') || 
					       (c >= '0' && c <= '9') || 
					       c == '_') )
					{
						bValid = FALSE;
						break;
					}
				}
			}
		}
		printf("  %-12s -> %s\n", arrInputs[i], bValid ? "VALID" : "INVALID");
	}
	printf("\n");
	
	// URL pattern matching
	// URL 模式匹配
	str arrUrls[] = {
		"http://example.com",
		"https://secure.site.org",
		"ftp://files.server.net",
		"mailto:user@domain.com",
		"invalid-url"
	};
	int iUrlCount = sizeof(arrUrls) / sizeof(arrUrls[0]);
	
	printf("HTTP/HTTPS URL detection:\n");
	for ( int i = 0; i < iUrlCount; i++ )
	{
		bool bIsHttp = xrtStrLike(arrUrls[i], 0, "http://*", 0, TRUE) || 
		               xrtStrLike(arrUrls[i], 0, "https://*", 0, TRUE);
		printf("  %-25s -> %s\n", arrUrls[i], bIsHttp ? "HTTP(S)" : "OTHER");
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Search and Match Demo\n");
	printf("XRT 字符串搜索与匹配演示\n");
	printf("==============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicSearch();
	TestExactMatching();
	TestWildcardMatching();
	TestEdgeCases();
	TestPracticalExamples();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}