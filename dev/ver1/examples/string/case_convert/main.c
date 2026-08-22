/*
 * XRT Example - String Case Conversion
 * XRT 范例 - 字符串大小写转换
 *
 * Description / 说明:
 *   EN: Demonstrates string case conversion functions xrtLCase and xrtUCase
 *       with UTF-8 safe character skipping capability.
 *   CN: 演示字符串大小写转换函数 xrtLCase 和 xrtUCase，
 *       支持 UTF-8 安全的字符跳过功能。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_case_convert.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_case_convert -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtLCase converts to lowercase, xrtUCase converts to uppercase
 *   - Both functions modify string in-place and return the same pointer
 *   - UTF-8 multi-byte characters are safely skipped (not converted)
 *   - Functions handle ASCII letters only (A-Z, a-z)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic case conversion
// 测试基本大小写转换
void TestBasicConversion()
{
	str sTest1 = xrtCopyStr("Hello World!", strlen("Hello World!") + 1);
	str sTest2 = xrtCopyStr("hello world!", strlen("hello world!") + 1);
	str sTest3 = xrtCopyStr("HELLO WORLD!", strlen("HELLO WORLD!") + 1);
	
	printf("=== Basic Case Conversion ===\n");
	printf("=== 基本大小写转换 ===\n");
	
	printf("Original:     \"%s\"\n", sTest1);
	printf("xrtLCase():   \"%s\"\n", xrtLCase(sTest1, strlen(sTest1)+1, TRUE));
	printf("xrtUCase():   \"%s\"\n\n", xrtUCase(sTest1, strlen(sTest1)+1, TRUE));
	
	printf("Original:     \"%s\"\n", sTest2);
	printf("xrtUCase():   \"%s\"\n", xrtUCase(sTest2, strlen(sTest2)+1, TRUE));
	printf("xrtLCase():   \"%s\"\n\n", xrtLCase(sTest2, strlen(sTest2)+1, TRUE));
	
	printf("Original:     \"%s\"\n", sTest3);
	printf("xrtLCase():   \"%s\"\n", xrtLCase(sTest3, strlen(sTest3)+1, TRUE));
	printf("xrtUCase():   \"%s\"\n\n", xrtUCase(sTest3, strlen(sTest3)+1, TRUE));
	
	// Clean up
	xrtFree(sTest1);
	xrtFree(sTest2);
	xrtFree(sTest3);
}

// Test UTF-8 safety
// 测试 UTF-8 安全性
void TestUTF8Safety()
{
	// Chinese characters (UTF-8 multi-byte)
	// 中文字符（UTF-8 多字节）
	str sChinese = xrtCopyStr("Hello 世界 World", strlen("Hello 世界 World") + 1);
	
	// Mixed with special characters
	// 混合特殊字符
	str sMixed = xrtCopyStr("Test123!@# 中文 ABC xyz", strlen("Test123!@# 中文 ABC xyz") + 1);
	
	// Numbers and symbols only
	// 仅数字和符号
	str sNumbers = xrtCopyStr("12345 !@#$% 你好67890", strlen("12345 !@#$% 你好67890") + 1);
	
	printf("=== UTF-8 Safety Test ===\n");
	printf("=== UTF-8 安全性测试 ===\n");
	
	printf("Chinese text: \"%s\"\n", sChinese);
	printf("After xrtLCase(): \"%s\"\n", xrtLCase(sChinese, strlen(sChinese)+1, TRUE));
	printf("After xrtUCase(): \"%s\"\n\n", xrtUCase(sChinese, strlen(sChinese)+1, TRUE));
	
	printf("Mixed text:   \"%s\"\n", sMixed);
	printf("After xrtLCase(): \"%s\"\n", xrtLCase(sMixed, strlen(sMixed)+1, TRUE));
	printf("After xrtUCase(): \"%s\"\n\n", xrtUCase(sMixed, strlen(sMixed)+1, TRUE));
	
	printf("Numbers/Symbols: \"%s\"\n", sNumbers);
	printf("After xrtLCase(): \"%s\"\n", xrtLCase(sNumbers, strlen(sNumbers)+1, TRUE));
	printf("After xrtUCase(): \"%s\"\n\n", xrtUCase(sNumbers, strlen(sNumbers)+1, TRUE));
	
	// Clean up
	xrtFree(sChinese);
	xrtFree(sMixed);
	xrtFree(sNumbers);
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
	printf("Empty string: \"%s\"\n", sEmpty);
	printf("xrtLCase():   \"%s\"\n", xrtLCase(sEmpty, 1, TRUE));
	printf("xrtUCase():   \"%s\"\n\n", xrtUCase(sEmpty, 1, TRUE));
	xrtFree(sEmpty);
	
	// NULL pointer
	// 空指针
	str sNull = NULL;
	printf("NULL pointer:\n");
	printf("xrtLCase(NULL): %p\n", xrtLCase(sNull, 0, TRUE));
	printf("xrtUCase(NULL): %p\n\n", xrtUCase(sNull, 0, TRUE));
	
	// Single character
	// 单个字符
	str sSingle1 = xrtCopyStr("A", 2);
	str sSingle2 = xrtCopyStr("z", 2);
	str sSingle3 = xrtCopyStr("5", 2);
	
	printf("Single chars:\n");
	printf("  \"%s\" -> LCase: \"%s\", UCase: \"%s\"\n", sSingle1, xrtLCase(sSingle1, 2, TRUE), xrtUCase(sSingle1, 2, TRUE));
	printf("  \"%s\" -> LCase: \"%s\", UCase: \"%s\"\n", sSingle2, xrtLCase(sSingle2, 2, TRUE), xrtUCase(sSingle2, 2, TRUE));
	printf("  \"%s\" -> LCase: \"%s\", UCase: \"%s\"\n\n", sSingle3, xrtLCase(sSingle3, 2, TRUE), xrtUCase(sSingle3, 2, TRUE));
	
	xrtFree(sSingle1);
	xrtFree(sSingle2);
	xrtFree(sSingle3);
	
	// Only non-alphabetic characters
	// 仅非字母字符
	str sNonAlpha = xrtCopyStr("123 !@# $%^", strlen("123 !@# $%^") + 1);
	printf("Non-alphabetic: \"%s\"\n", sNonAlpha);
	printf("xrtLCase():     \"%s\"\n", xrtLCase(sNonAlpha, strlen(sNonAlpha)+1, TRUE));
	printf("xrtUCase():     \"%s\"\n\n", xrtUCase(sNonAlpha, strlen(sNonAlpha)+1, TRUE));
	xrtFree(sNonAlpha);
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// Username normalization
	// 用户名标准化
	str sUsername1 = xrtCopyStr("JohnDoe", strlen("JohnDoe") + 1);
	str sUsername2 = xrtCopyStr("john_doe", strlen("john_doe") + 1);
	str sUsername3 = xrtCopyStr("JOHN.DOE", strlen("JOHN.DOE") + 1);
	
	printf("Username normalization:\n");
	printf("  \"%s\" -> lowercase: \"%s\"\n", sUsername1, xrtLCase(sUsername1, strlen(sUsername1)+1, TRUE));
	printf("  \"%s\" -> lowercase: \"%s\"\n", sUsername2, xrtLCase(sUsername2, strlen(sUsername2)+1, TRUE));
	printf("  \"%s\" -> lowercase: \"%s\"\n\n", sUsername3, xrtLCase(sUsername3, strlen(sUsername3)+1, TRUE));
	
	xrtFree(sUsername1);
	xrtFree(sUsername2);
	xrtFree(sUsername3);
	
	// Case-insensitive comparison helper
	// 不区分大小写比较辅助
	str sInput = xrtCopyStr("HeLLo WoRLd!", strlen("HeLLo WoRLd!") + 1);
	str sExpected = xrtCopyStr("hello world!", strlen("hello world!") + 1);
	
	printf("Case-insensitive comparison:\n");
	printf("  Input:    \"%s\"\n", sInput);
	printf("  Expected: \"%s\"\n", sExpected);
	
	// Convert both to same case for comparison
	xrtLCase(sInput, strlen(sInput)+1, TRUE);
	xrtLCase(sExpected, strlen(sExpected)+1, TRUE);
	
	printf("  After LCase:\n");
	printf("    Input:    \"%s\"\n", sInput);
	printf("    Expected: \"%s\"\n", sExpected);
	printf("    Equal: %s\n\n", strcmp(sInput, sExpected) == 0 ? "YES" : "NO");
	
	xrtFree(sInput);
	xrtFree(sExpected);
	
	// File extension normalization
	// 文件扩展名标准化
	str sFile1 = xrtCopyStr("document.TXT", strlen("document.TXT") + 1);
	str sFile2 = xrtCopyStr("image.jpg", strlen("image.jpg") + 1);
	str sFile3 = xrtCopyStr("DATA.XML", strlen("DATA.XML") + 1);
	
	printf("File extension normalization:\n");
	printf("  \"%s\" -> uppercase ext: \"%s\"\n", sFile1, xrtUCase(sFile1, strlen(sFile1)+1, TRUE));
	printf("  \"%s\" -> lowercase ext: \"%s\"\n", sFile2, xrtLCase(sFile2, strlen(sFile2)+1, TRUE));
	printf("  \"%s\" -> uppercase ext: \"%s\"\n\n", sFile3, xrtUCase(sFile3, strlen(sFile3)+1, TRUE));
	
	xrtFree(sFile1);
	xrtFree(sFile2);
	xrtFree(sFile3);
}

// Demonstrate in-place modification
// 演示原地修改
void DemonstrateInPlaceModification()
{
	printf("=== In-Place Modification ===\n");
	printf("=== 原地修改演示 ===\n");
	
	str sOriginal = xrtCopyStr("Test String", strlen("Test String") + 1);
	str sPointer = sOriginal;  // Save original pointer
	
	printf("Original string: \"%s\" at %p\n", sOriginal, sOriginal);
	
	// Convert to lowercase
	str sResult1 = xrtLCase(sOriginal, strlen(sOriginal)+1, TRUE);
	printf("After xrtLCase(): \"%s\" at %p\n", sResult1, sResult1);
	printf("Same pointer: %s\n", sOriginal == sResult1 ? "YES" : "NO");
	
	// Convert back to uppercase
	str sResult2 = xrtUCase(sOriginal, strlen(sOriginal)+1, TRUE);
	printf("After xrtUCase(): \"%s\" at %p\n", sResult2, sResult2);
	printf("Same pointer: %s\n", sOriginal == sResult2 ? "YES" : "NO");
	
	// Verify original pointer unchanged
	printf("Original pointer unchanged: %s\n\n", sPointer == sOriginal ? "YES" : "NO");
	
	xrtFree(sOriginal);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Case Conversion Demo\n");
	printf("XRT 字符串大小写转换演示\n");
	printf("=============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicConversion();
	TestUTF8Safety();
	TestEdgeCases();
	TestPracticalExamples();
	DemonstrateInPlaceModification();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}