/*
 * XRT Example - Charset Detection
 * XRT 范例 - 字符集检测
 *
 * Description / 说明:
 *   EN: Demonstrates xrtIsUTF8 for validating UTF-8 encoded strings.
 *   CN: 演示 xrtIsUTF8 验证 UTF-8 编码字符串。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/charset_charset_detect.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/charset_charset_detect              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtIsUTF8 validates proper UTF-8 encoding
 *   - Returns TRUE for valid UTF-8, FALSE otherwise
 *   - Size=0 means auto-detect string length
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test valid UTF-8 strings
// 测试有效的 UTF-8 字符串
void TestValidUTF8()
{
	printf("=== Valid UTF-8 Strings ===\n");
	printf("=== 有效的 UTF-8 字符串 ===\n");
	
	char* psStrings[] = {
		"Hello World",           // ASCII only
		"你好世界",              // Chinese
		"Привет мир",            // Russian
		"こんにちは",            // Japanese
		"🌍🌎🌏",                // Emojis
		"Mixed: A中🀄",          // Mixed
		"",                      // Empty string
	};
	
	char* psDesc[] = {
		"ASCII only",
		"Chinese",
		"Russian",
		"Japanese",
		"Emojis",
		"Mixed",
		"Empty",
	};
	
	char* psDescCN[] = {
		"纯 ASCII",
		"中文",
		"俄语",
		"日语",
		"表情符号",
		"混合",
		"空字符串",
	};
	
	int iNumStrings = 7;
	int i;
	for ( i = 0; i < iNumStrings; i++ ) {
		bool bValid = xrtIsUTF8(psStrings[i], 0);
		printf("  %s (%s): %s\n", 
		       psDesc[i], psDescCN[i], 
		       bValid ? "VALID" : "INVALID");
	}
	printf("\n");
}

// Test invalid UTF-8 sequences
// 测试无效的 UTF-8 序列
void TestInvalidUTF8()
{
	printf("=== Invalid UTF-8 Sequences ===\n");
	printf("=== 无效的 UTF-8 序列 ===\n");
	
	// Manually crafted invalid sequences
	// 手工构造的无效序列
	unsigned char sInvalid1[] = {0x80, 0x00};  // Invalid start byte
	unsigned char sInvalid2[] = {0xC0, 0x80, 0x00};  // Overlong encoding
	unsigned char sInvalid3[] = {0xE0, 0x80, 0x80, 0x00};  // Overlong encoding
	unsigned char sInvalid4[] = {0xC2, 0x00};  // Truncated sequence
	unsigned char sInvalid5[] = {0xE2, 0x82, 0x00};  // Truncated sequence
	
	struct {
		str sData;
		str sDesc;
		str sDescCN;
	} sTests[] = {
		{(str)sInvalid1, "Invalid start byte (0x80)", "无效起始字节 (0x80)"},
		{(str)sInvalid2, "Overlong encoding", "过长编码"},
		{(str)sInvalid3, "Overlong 3-byte", "过长 3 字节编码"},
		{(str)sInvalid4, "Truncated 2-byte", "截断的 2 字节序列"},
		{(str)sInvalid5, "Truncated 3-byte", "截断的 3 字节序列"},
	};
	
	int iNumTests = 5;
	int i;
	for ( i = 0; i < iNumTests; i++ ) {
		bool bValid = xrtIsUTF8(sTests[i].sData, 0);
		printf("  %s (%s): %s\n", 
		       sTests[i].sDesc, sTests[i].sDescCN,
		       bValid ? "VALID" : "INVALID (expected)");
	}
	printf("\n");
}

// Test partial string validation
// 测试部分字符串验证
void TestPartialValidation()
{
	printf("=== Partial String Validation ===\n");
	printf("=== 部分字符串验证 ===\n");
	
	// Valid UTF-8 with invalid continuation
	// 有效 UTF-8 后跟无效续字节
	unsigned char sPartial[] = {'H', 'e', 'l', 'l', 'o', 0xC2, 0x41, 0x00};
	// The 0xC2 expects a continuation byte, but 0x41 is 'A' (ASCII)
	// 0xC2 期望续字节，但 0x41 是 'A' (ASCII)
	
	printf("String: \"Hello\" + invalid sequence\n");
	printf("字符串: \"Hello\" + 无效序列\n");
	printf("Full string: %s\n", xrtIsUTF8((str)sPartial, 0) ? "VALID" : "INVALID");
	printf("完整字符串: %s\n", xrtIsUTF8((str)sPartial, 0) ? "有效" : "无效");
	
	// Validate only first 5 bytes (valid ASCII)
	// 只验证前 5 字节 (有效 ASCII)
	printf("First 5 bytes: %s\n", xrtIsUTF8((str)sPartial, 5) ? "VALID" : "INVALID");
	printf("前 5 字节: %s\n", xrtIsUTF8((str)sPartial, 5) ? "有效" : "无效");
	
	printf("\n");
}

// Test boundary conditions
// 测试边界条件
void TestBoundaries()
{
	printf("=== Boundary Conditions ===\n");
	printf("=== 边界条件 ===\n");
	
	// 1-byte range (ASCII)
	// 1 字节范围 (ASCII)
	printf("ASCII range:\n");
	printf("ASCII 范围:\n");
	unsigned char s1[] = {0x00, 0x7F, 0x00};  // NULL and DEL
	printf("  0x00-0x7F: %s\n", xrtIsUTF8((str)s1, 2) ? "VALID" : "INVALID");
	
	// 2-byte range
	// 2 字节范围
	printf("\n2-byte range:\n");
	printf("2 字节范围:\n");
	unsigned char s2a[] = {0xC2, 0x80, 0x00};  // First valid 2-byte
	unsigned char s2b[] = {0xDF, 0xBF, 0x00};  // Last valid 2-byte
	printf("  First (C2 80): %s\n", xrtIsUTF8((str)s2a, 2) ? "VALID" : "INVALID");
	printf("  Last (DF BF): %s\n", xrtIsUTF8((str)s2b, 2) ? "VALID" : "INVALID");
	
	// 3-byte range
	// 3 字节范围
	printf("\n3-byte range:\n");
	printf("3 字节范围:\n");
	unsigned char s3a[] = {0xE0, 0xA0, 0x80, 0x00};  // First valid 3-byte
	unsigned char s3b[] = {0xEF, 0xBF, 0xBF, 0x00};  // Last valid 3-byte
	printf("  First (E0 A0 80): %s\n", xrtIsUTF8((str)s3a, 3) ? "VALID" : "INVALID");
	printf("  Last (EF BF BF): %s\n", xrtIsUTF8((str)s3b, 3) ? "VALID" : "INVALID");
	
	// 4-byte range
	// 4 字节范围
	printf("\n4-byte range:\n");
	printf("4 字节范围:\n");
	unsigned char s4a[] = {0xF0, 0x90, 0x80, 0x80, 0x00};  // First valid 4-byte
	unsigned char s4b[] = {0xF4, 0x8F, 0xBF, 0xBF, 0x00};  // Last valid 4-byte
	printf("  First (F0 90 80 80): %s\n", xrtIsUTF8((str)s4a, 4) ? "VALID" : "INVALID");
	printf("  Last (F4 8F BF BF): %s\n", xrtIsUTF8((str)s4b, 4) ? "VALID" : "INVALID");
	
	printf("\n");
}

// Test practical use case - file content validation
// 测试实际用例 - 文件内容验证
void TestFileContentValidation()
{
	printf("=== Practical: Content Validation ===\n");
	printf("=== 实际: 内容验证 ===\n");
	
	// Simulate different file contents
	// 模拟不同的文件内容
	struct {
		str sContent;
		str sDesc;
	} sFiles[] = {
		{"{ \"name\": \"测试\", \"value\": 123 }", "JSON with Chinese"},
		{"<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>", "XML header"},
		{"# Configuration file\nkey=value\nname=应用", "Config file"},
		{"function test() { return \"hello\"; }", "JavaScript code"},
	};
	
	int iNumFiles = 4;
	int i;
	for ( i = 0; i < iNumFiles; i++ ) {
		bool bValid = xrtIsUTF8(sFiles[i].sContent, 0);
		printf("  %s: %s\n", sFiles[i].sDesc, bValid ? "UTF-8 OK" : "NOT UTF-8");
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Charset - Charset Detection Demo\n");
	printf("XRT 字符集 - 字符集检测演示\n");
	printf("====================================\n\n");
	
	TestValidUTF8();
	TestInvalidUTF8();
	TestPartialValidation();
	TestBoundaries();
	TestFileContentValidation();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
