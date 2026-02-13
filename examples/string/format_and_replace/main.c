/*
 * XRT Example - Format and Replace
 * XRT 范例 - 格式化与替换
 *
 * Description / 说明:
 *   EN: Demonstrates xrtFormat for printf-style formatting and xrtReplace
 *       for substring replacement operations.
 *   CN: 演示 xrtFormat printf 风格格式化和 xrtReplace 子字符串替换操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_format_and_replace.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_format_and_replace -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtFormat returns newly allocated string, must be freed
 *   - xrtReplace: size=0 means auto-detect string length
 *   - All returned strings must be freed with xrtFree
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test xrtFormat basic usage
// 测试 xrtFormat 基本用法
void TestFormat()
{
	printf("=== xrtFormat Test ===\n");
	printf("=== xrtFormat 测试 ===\n");
	
	// Basic integer formatting
	// 基本整数格式化
	str sInt = xrtFormat("Integer: %d", 42);
	printf("Integer: %s\n", sInt);
	xrtFree(sInt);
	
	// Float formatting with precision
	// 带精度的浮点数格式化
	str sFloat = xrtFormat("Pi = %.6f", 3.14159265);
	printf("Float: %s\n", sFloat);
	printf("浮点数: %s\n", sFloat);
	xrtFree(sFloat);
	
	// Multiple arguments
	// 多参数
	str sMulti = xrtFormat("Name: %s, Age: %d, Score: %.1f", "Alice", 25, 95.5);
	printf("Multi: %s\n", sMulti);
	printf("多参数: %s\n", sMulti);
	xrtFree(sMulti);
	
	// Hex formatting
	// 十六进制格式化
	str sHex = xrtFormat("Hex: 0x%08X", 0xDEADBEEF);
	printf("Hex: %s\n", sHex);
	printf("十六进制: %s\n", sHex);
	xrtFree(sHex);
	
	printf("\n");
}

// Test xrtReplace basic usage
// 测试 xrtReplace 基本用法
void TestReplace()
{
	printf("=== xrtReplace Test ===\n");
	printf("=== xrtReplace 测试 ===\n");
	
	size_t iRetSize;
	
	// Simple replacement
	// 简单替换
	str sResult1 = xrtReplace("hello world", 0, "world", 0, "XRT", 0, &iRetSize);
	printf("Replace 'world' with 'XRT': %s\n", sResult1);
	printf("替换 'world' 为 'XRT': %s\n", sResult1);
	printf("Return size: %zu\n\n", iRetSize);
	xrtFree(sResult1);
	
	// Multiple occurrences
	// 多次出现
	str sResult2 = xrtReplace("1a1b1c1d1e", 0, "1", 0, "_", 0, &iRetSize);
	printf("Replace all '1' with '_': %s\n", sResult2);
	printf("替换所有 '1' 为 '_': %s\n", sResult2);
	xrtFree(sResult2);
	
	// Replace with longer string
	// 用更长的字符串替换
	str sResult3 = xrtReplace("cat", 0, "cat", 0, "elephant", 0, &iRetSize);
	printf("Replace 'cat' with 'elephant': %s\n", sResult3);
	printf("替换 'cat' 为 'elephant': %s\n", sResult3);
	xrtFree(sResult3);
	
	// Replace with shorter string
	// 用更短的字符串替换
	str sResult4 = xrtReplace("elephant", 0, "elephant", 0, "cat", 0, &iRetSize);
	printf("Replace 'elephant' with 'cat': %s\n", sResult4);
	printf("替换 'elephant' 为 'cat': %s\n", sResult4);
	xrtFree(sResult4);
	
	printf("\n");
}

// Test xrtReplace with size parameters
// 测试带大小参数的 xrtReplace
void TestReplaceWithSize()
{
	printf("=== xrtReplace with Size Parameters ===\n");
	printf("=== 带大小参数的 xrtReplace ===\n");
	
	size_t iRetSize;
	
	// Replace only first N characters of source
	// 只替换源字符串的前 N 个字符
	str sText = "1a1b1c1d1e1f1g1";
	
	// Replace in first 8 chars
	// 在前 8 个字符中替换
	str sResult1 = xrtReplace(sText, 8, "1", 0, "_", 0, &iRetSize);
	printf("Replace in first 8 chars: %s\n", sResult1);
	printf("在前 8 个字符中替换: %s\n", sResult1);
	xrtFree(sResult1);
	
	// Replace in first 9 chars (includes one more '1')
	// 在前 9 个字符中替换 (多包含一个 '1')
	str sResult2 = xrtReplace(sText, 9, "1", 0, "_", 0, &iRetSize);
	printf("Replace in first 9 chars: %s\n", sResult2);
	printf("在前 9 个字符中替换: %s\n", sResult2);
	xrtFree(sResult2);
	
	// Use 0 for auto-detect all lengths
	// 使用 0 自动检测所有长度
	str sResult3 = xrtReplace(sText, 0, "1", 0, "_", 0, &iRetSize);
	printf("Replace all (auto-detect): %s\n", sResult3);
	printf("替换所有 (自动检测): %s\n", sResult3);
	xrtFree(sResult3);
	
	printf("\n");
}

// Test combined format and replace
// 测试组合格式化和替换
void TestCombined()
{
	printf("=== Combined Format and Replace ===\n");
	printf("=== 组合格式化和替换 ===\n");
	
	// Create template with format
	// 用格式化创建模板
	str sTemplate = xrtFormat("Hello {name}, your score is {score}!");
	printf("Template: %s\n", sTemplate);
	printf("模板: %s\n", sTemplate);
	
	// Replace placeholders
	// 替换占位符
	str sStep1 = xrtReplace(sTemplate, 0, "{name}", 0, "Alice", 0, NULL);
	str sStep2 = xrtReplace(sStep1, 0, "{score}", 0, "95", 0, NULL);
	printf("Result: %s\n", sStep2);
	printf("结果: %s\n", sStep2);
	
	xrtFree(sTemplate);
	xrtFree(sStep1);
	xrtFree(sStep2);
	
	// Build complex string
	// 构建复杂字符串
	printf("\n");
	str sBase = "The quick brown fox jumps over the lazy dog.";
	printf("Original: %s\n", sBase);
	printf("原始: %s\n", sBase);
	
	str sMod1 = xrtReplace(sBase, 0, "quick", 0, "slow", 0, NULL);
	str sMod2 = xrtReplace(sMod1, 0, "brown", 0, "white", 0, NULL);
	str sMod3 = xrtReplace(sMod2, 0, "fox", 0, "rabbit", 0, NULL);
	printf("Modified: %s\n", sMod3);
	printf("修改后: %s\n", sMod3);
	
	xrtFree(sMod1);
	xrtFree(sMod2);
	xrtFree(sMod3);
	
	printf("\n");
}

// Test practical use case - template processing
// 测试实际用例 - 模板处理
void TestTemplateProcessing()
{
	printf("=== Template Processing Example ===\n");
	printf("=== 模板处理示例 ===\n");
	
	// Email template
	// 邮件模板
	str sEmailTemplate = 
		"Dear {customer},\n"
		"\n"
		"Thank you for your order #{order_id}.\n"
		"Your total is ${amount}.\n"
		"Expected delivery: {date}\n"
		"\n"
		"Best regards,\n"
		"{company}";
	
	printf("Template:\n%s\n\n", sEmailTemplate);
	printf("模板:\n%s\n\n", sEmailTemplate);
	
	// Replace variables
	// 替换变量
	str sStep1 = xrtReplace(sEmailTemplate, 0, "{customer}", 0, "John Smith", 0, NULL);
	str sStep2 = xrtReplace(sStep1, 0, "{order_id}", 0, "12345", 0, NULL);
	str sStep3 = xrtReplace(sStep2, 0, "{amount}", 0, "99.99", 0, NULL);
	str sStep4 = xrtReplace(sStep3, 0, "{date}", 0, "2024-12-25", 0, NULL);
	str sStep5 = xrtReplace(sStep4, 0, "{company}", 0, "XRT Store", 0, NULL);
	
	printf("Result:\n%s\n", sStep5);
	printf("结果:\n%s\n", sStep5);
	
	xrtFree(sStep1);
	xrtFree(sStep2);
	xrtFree(sStep3);
	xrtFree(sStep4);
	xrtFree(sStep5);
	
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String - Format and Replace Demo\n");
	printf("XRT 字符串 - 格式化与替换演示\n");
	printf("====================================\n\n");
	
	TestFormat();
	TestReplace();
	TestReplaceWithSize();
	TestCombined();
	TestTemplateProcessing();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
