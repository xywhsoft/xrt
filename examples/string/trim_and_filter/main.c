/*
 * XRT Example - String Trim and Filter
 * XRT 范例 - 字符串裁剪与过滤
 *
 * Description / 说明:
 *   EN: Demonstrates string trimming functions (xrtLTrim/xrtRTrim/xrtTrim)
 *       and character filtering function (xrtFilterStr) with custom character sets.
 *   CN: 演示字符串裁剪函数（xrtLTrim/xrtRTrim/xrtTrim）和字符过滤函数（xrtFilterStr），
 *       支持自定义字符集。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_trim_and_filter.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_trim_and_filter -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtLTrim: trim leading whitespace
 *   - xrtRTrim: trim trailing whitespace  
 *   - xrtTrim: trim both leading and trailing whitespace
 *   - xrtFilterStr: remove specified characters from string
 *   - All functions modify string in-place and return the same pointer
 *   - Whitespace includes space, tab, newline, carriage return
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic trimming functions
// 测试基本裁剪函数
void TestBasicTrimming()
{
	// Test strings with various whitespace combinations
	// 测试各种空白字符组合的字符串
	str sTest1 = xrtCopyStr("   Hello World   ", strlen("   Hello World   ") + 1);
	str sTest2 = xrtCopyStr("\t\tTabbed Text\t\t", strlen("\t\tTabbed Text\t\t") + 1);
	str sTest3 = xrtCopyStr("\n\rNewline Text\n\r", strlen("\n\rNewline Text\n\r") + 1);
	str sTest4 = xrtCopyStr("  Mixed \t Whitespace  ", strlen("  Mixed \t Whitespace  ") + 1);
	str sTest5 = xrtCopyStr("NoWhitespace", strlen("NoWhitespace") + 1);
	
	printf("=== Basic Trimming Functions ===\n");
	printf("=== 基本裁剪函数 ===\n");
	
	// Test xrtLTrim (left trim)
	// 测试 xrtLTrim (左裁剪)
	printf("xrtLTrim (trim leading whitespace):\n");
	printf("  Before: \"%s\"\n", sTest1);
	printf("  After:  \"%s\"\n\n", xrtLTrim(sTest1, strlen(sTest1) + 1, NULL, 0, TRUE, NULL));
	
	printf("  Before: \"%s\"\n", sTest2);
	printf("  After:  \"%s\"\n\n", xrtLTrim(sTest2, strlen(sTest2) + 1, NULL, 0, TRUE, NULL));
	
	// Test xrtRTrim (right trim)  
	// 测试 xrtRTrim (右裁剪)
	printf("xrtRTrim (trim trailing whitespace):\n");
	printf("  Before: \"%s\"\n", sTest1);
	printf("  After:  \"%s\"\n\n", xrtRTrim(sTest1, strlen(sTest1) + 1, NULL, 0, TRUE, NULL));
	
	printf("  Before: \"%s\"\n", sTest3);
	printf("  After:  \"%s\"\n\n", xrtRTrim(sTest3, strlen(sTest3) + 1, NULL, 0, TRUE, NULL));
	
	// Test xrtTrim (both sides)
	// 测试 xrtTrim (两边裁剪)
	printf("xrtTrim (trim both leading and trailing):\n");
	printf("  Before: \"%s\"\n", sTest1);
	printf("  After:  \"%s\"\n\n", xrtTrim(sTest1, strlen(sTest1) + 1, NULL, 0, TRUE, NULL));
	
	printf("  Before: \"%s\"\n", sTest4);
	printf("  After:  \"%s\"\n\n", xrtTrim(sTest4, strlen(sTest4) + 1, NULL, 0, TRUE, NULL));
	
	// Test with no whitespace
	// 测试无空白字符的情况
	printf("No whitespace to trim:\n");
	printf("  Before: \"%s\"\n", sTest5);
	printf("  After:  \"%s\"\n\n", xrtTrim(sTest5, strlen(sTest5) + 1, NULL, 0, TRUE, NULL));
	
	// Clean up
	xrtFree(sTest1);
	xrtFree(sTest2);
	xrtFree(sTest3);
	xrtFree(sTest4);
	xrtFree(sTest5);
}

// Test character filtering
// 测试字符过滤
void TestCharacterFiltering()
{
	str sTest1 = xrtCopyStr("Hello, World! 123", strlen("Hello, World! 123") + 1);
	str sTest2 = xrtCopyStr("Remove ### Special *** Characters @@@@", strlen("Remove ### Special *** Characters @@@@") + 1);
	str sTest3 = xrtCopyStr("   Spaces   And\tTabs\t", strlen("   Spaces   And\tTabs\t") + 1);
	
	printf("=== Character Filtering ===\n");
	printf("=== 字符过滤 ===\n");
	
	// Filter digits
	// 过滤数字
	str sDigits = "0123456789";
	printf("Filter digits from \"%s\":\n", sTest1);
	printf("  Before: \"%s\"\n", sTest1);
	printf("  Filter: \"%s\"\n", sDigits);
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sTest1, strlen(sTest1) + 1, sDigits, strlen(sDigits), TRUE, NULL));
	
	// Filter punctuation
	// 过滤标点符号
	str sPunct = "!@#$%^&*(),./;:'\"[]{}<>?`~";
	printf("Filter punctuation:\n");
	printf("  Before: \"%s\"\n", sTest2);
	printf("  Filter: \"%s\"\n", sPunct);
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sTest2, strlen(sTest2) + 1, sPunct, strlen(sPunct), TRUE, NULL));
	
	// Filter whitespace
	// 过滤空白字符
	str sWhitespace = " \t\n\r";
	printf("Filter whitespace:\n");
	printf("  Before: \"%s\"\n", sTest3);
	printf("  Filter: \" \\t\\n\\r\"\n");
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sTest3, strlen(sTest3) + 1, sWhitespace, strlen(sWhitespace), TRUE, NULL));
	
	// Custom character set
	// 自定义字符集
	str sCustom = "aeiouAEIOU";  // Vowels
	printf("Filter vowels:\n");
	printf("  Before: \"%s\"\n", sTest1);
	printf("  Filter: \"%s\"\n", sCustom);
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sTest1, strlen(sTest1) + 1, sCustom, strlen(sCustom), TRUE, NULL));
	
	// Clean up
	xrtFree(sTest1);
	xrtFree(sTest2);
	xrtFree(sTest3);
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("=== Edge Cases ===\n");
	printf("=== 边界情况 ===\n");
	
	// Empty string
	// 空字符串
	str sEmpty = xrtCopyStr("", 1);
	printf("Empty string:\n");
	printf("  xrtTrim:   \"%s\"\n", xrtTrim(sEmpty, 1, NULL, 0, TRUE, NULL));
	printf("  xrtLTrim:  \"%s\"\n", xrtLTrim(sEmpty, 1, NULL, 0, TRUE, NULL));
	printf("  xrtRTrim:  \"%s\"\n\n", xrtRTrim(sEmpty, 1, NULL, 0, TRUE, NULL));
	xrtFree(sEmpty);
	
	// Only whitespace
	// 仅有空白字符
	str sOnlySpace = xrtCopyStr("   \t\n\r   ", strlen("   \t\n\r   ") + 1);
	printf("Only whitespace:\n");
	printf("  Before: \"%s\"\n", sOnlySpace);
	printf("  xrtTrim:   \"%s\"\n", xrtTrim(sOnlySpace, strlen(sOnlySpace) + 1, NULL, 0, TRUE, NULL));
	printf("  xrtLTrim:  \"%s\"\n", xrtLTrim(sOnlySpace, strlen(sOnlySpace) + 1, NULL, 0, TRUE, NULL));
	printf("  xrtRTrim:  \"%s\"\n\n", xrtRTrim(sOnlySpace, strlen(sOnlySpace) + 1, NULL, 0, TRUE, NULL));
	xrtFree(sOnlySpace);
	
	// NULL pointer
	// 空指针
	str sNull = NULL;
	printf("NULL pointer:\n");
	printf("  xrtTrim(NULL):   %p\n", xrtTrim(sNull, 0, NULL, 0, TRUE, NULL));
	printf("  xrtLTrim(NULL):  %p\n", xrtLTrim(sNull, 0, NULL, 0, TRUE, NULL));
	printf("  xrtRTrim(NULL):  %p\n", xrtRTrim(sNull, 0, NULL, 0, TRUE, NULL));
	printf("  xrtFilterStr(NULL): %p\n\n", xrtFilterStr(sNull, 0, "abc", 3, TRUE, NULL));
	
	// Single character
	// 单个字符
	str sSingle1 = xrtCopyStr(" ", 2);  // Space
	str sSingle2 = xrtCopyStr("A", 2);  // Letter
	str sSingle3 = xrtCopyStr("5", 2);  // Digit
	
	printf("Single characters:\n");
	printf("  Space:     \"%s\" -> Trim: \"%s\"\n", sSingle1, xrtTrim(sSingle1, 2, NULL, 0, TRUE, NULL));
	printf("  Letter:    \"%s\" -> Trim: \"%s\"\n", sSingle2, xrtTrim(sSingle2, 2, NULL, 0, TRUE, NULL));
	printf("  Digit:     \"%s\" -> Trim: \"%s\"\n\n", sSingle3, xrtTrim(sSingle3, 2, NULL, 0, TRUE, NULL));
	
	xrtFree(sSingle1);
	xrtFree(sSingle2);
	xrtFree(sSingle3);
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// User input cleaning
	// 用户输入清理
	str sInput1 = xrtCopyStr("   John Doe   ", strlen("   John Doe   ") + 1);
	str sInput2 = xrtCopyStr("\t\tuser@example.com\t\t", strlen("\t\tuser@example.com\t\t") + 1);
	str sInput3 = xrtCopyStr("\n\r  Password123  \n\r", strlen("\n\r  Password123  \n\r") + 1);
	
	printf("User input cleaning:\n");
	printf("  Name:     \"%s\" -> \"%s\"\n", sInput1, xrtTrim(sInput1, strlen(sInput1) + 1, NULL, 0, TRUE, NULL));
	printf("  Email:    \"%s\" -> \"%s\"\n", sInput2, xrtTrim(sInput2, strlen(sInput2) + 1, NULL, 0, TRUE, NULL));
	printf("  Password: \"%s\" -> \"%s\"\n\n", sInput3, xrtTrim(sInput3, strlen(sInput3) + 1, NULL, 0, TRUE, NULL));
	
	xrtFree(sInput1);
	xrtFree(sInput2);
	xrtFree(sInput3);
	
	// File path cleaning
	// 文件路径清理
	str sPath1 = xrtCopyStr("  /home/user/  ", strlen("  /home/user/  ") + 1);
	str sPath2 = xrtCopyStr("\tC:\\Users\\Name\\\t", strlen("\tC:\\Users\\Name\\\t") + 1);
	
	printf("File path cleaning:\n");
	printf("  Unix:  \"%s\" -> \"%s\"\n", sPath1, xrtTrim(sPath1, strlen(sPath1) + 1, NULL, 0, TRUE, NULL));
	printf("  Win:   \"%s\" -> \"%s\"\n\n", sPath2, xrtTrim(sPath2, strlen(sPath2) + 1, NULL, 0, TRUE, NULL));
	
	xrtFree(sPath1);
	xrtFree(sPath2);
	
	// Data sanitization
	// 数据净化
	str sData = xrtCopyStr("User***Name###123!!!", strlen("User***Name###123!!!") + 1);
	str sSpecialChars = "*#!";
	
	printf("Data sanitization:\n");
	printf("  Before: \"%s\"\n", sData);
	printf("  Filter: \"%s\"\n", sSpecialChars);
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sData, strlen(sData) + 1, sSpecialChars, strlen(sSpecialChars), TRUE, NULL));
	
	xrtFree(sData);
	
	// Phone number formatting
	// 电话号码格式化
	str sPhone = xrtCopyStr("  +1 (555) 123-4567  ", strlen("  +1 (555) 123-4567  ") + 1);
	str sNonDigits = "+() -";
	
	printf("Phone number cleaning:\n");
	printf("  Before: \"%s\"\n", sPhone);
	printf("  Filter non-digits: \"%s\"\n", sNonDigits);
	printf("  After:  \"%s\"\n\n", xrtFilterStr(sPhone, strlen(sPhone) + 1, sNonDigits, strlen(sNonDigits), TRUE, NULL));
	
	xrtFree(sPhone);
}

// Demonstrate in-place modification
// 演示原地修改
void DemonstrateInPlaceModification()
{
	printf("=== In-Place Modification ===\n");
	printf("=== 原地修改演示 ===\n");
	
	str sOriginal = xrtCopyStr("  Test String  ", strlen("  Test String  ") + 1);
	str sPointer = sOriginal;  // Save original pointer
	
	printf("Original string: \"%s\" at %p\n", sOriginal, sOriginal);
	
	// Apply multiple operations
	// 应用多个操作
	str sResult1 = xrtLTrim(sOriginal, strlen(sOriginal) + 1, NULL, 0, TRUE, NULL);
	printf("After xrtLTrim:  \"%s\" at %p\n", sResult1, sResult1);
	
	str sResult2 = xrtRTrim(sOriginal, strlen(sOriginal) + 1, NULL, 0, TRUE, NULL);
	printf("After xrtRTrim:  \"%s\" at %p\n", sResult2, sResult2);
	
	str sResult3 = xrtTrim(sOriginal, strlen(sOriginal) + 1, NULL, 0, TRUE, NULL);
	printf("After xrtTrim:   \"%s\" at %p\n", sResult3, sResult3);
	
	// Verify original pointer unchanged
	// 验证原始指针未改变
	printf("Original pointer unchanged: %s\n", sPointer == sOriginal ? "YES" : "NO");
	printf("All results same pointer: %s\n\n", 
	       (sOriginal == sResult1 && sOriginal == sResult2 && sOriginal == sResult3) ? "YES" : "NO");
	
	xrtFree(sOriginal);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Trim and Filter Demo\n");
	printf("XRT 字符串裁剪与过滤演示\n");
	printf("=============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicTrimming();
	TestCharacterFiltering();
	TestEdgeCases();
	TestPracticalExamples();
	DemonstrateInPlaceModification();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}