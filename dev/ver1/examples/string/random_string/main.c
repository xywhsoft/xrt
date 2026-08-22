/*
 * XRT Example - Random String Generation
 * XRT 范例 - 随机字符串生成
 *
 * Description / 说明:
 *   EN: Demonstrates xrtRandStr for generating random strings based on
 *       character templates.
 *   CN: 演示 xrtRandStr 基于字符模板生成随机字符串。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_random_string.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_random_string -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Template characters define character pools: a-z, A-Z, 0-9, etc.
 *   - Each template character represents a character class
 *   - Returns newly allocated string, must be freed with xrtFree
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic random string templates
// 测试基本随机字符串模板
void TestBasicTemplates()
{
	printf("=== Basic Random String Templates ===\n");
	printf("=== 基本随机字符串模板 ===\n");
	
	// 'a' = lowercase letters
	// 'a' = 小写字母
	printf("Lowercase (a): ");
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("a", 0, 8);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n小写字母: 见上\n\n");
	
	// 'A' = uppercase letters
	// 'A' = 大写字母
	printf("Uppercase (A): ");
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("A", 0, 8);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n大写字母: 见上\n\n");
	
	// '0' = digits
	// '0' = 数字
	printf("Digits (0): ");
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("0", 0, 8);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n数字: 见上\n\n");
}

// Test mixed templates
// 测试混合模板
void TestMixedTemplates()
{
	printf("=== Mixed Templates ===\n");
	printf("=== 混合模板 ===\n");
	
	// Alphanumeric (lowercase + digits)
	// 字母数字 (小写 + 数字)
	printf("Alphanumeric (a0): ");
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("a0", 0, 10);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n字母数字 (a0): 见上\n\n");
	
	// Full alphanumeric (upper + lower + digits)
	// 完整字母数字 (大写 + 小写 + 数字)
	printf("Full Alphanumeric (Aa0): ");
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("Aa0", 0, 12);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n完整字母数字 (Aa0): 见上\n\n");
	
	// Hex characters
	// 十六进制字符
	printf("Hex (0A): ");
	for ( i = 0; i < 5; i++ ) {
		str sRand = xrtRandStr("0A", 0, 8);
		printf("%s ", sRand);
		xrtFree(sRand);
	}
	printf("\n十六进制 (0A): 见上\n\n");
}

// Test password generation
// 测试密码生成
void TestPasswordGeneration()
{
	printf("=== Password Generation ===\n");
	printf("=== 密码生成 ===\n");
	
	// Simple password (8 chars)
	// 简单密码 (8 字符)
	printf("Simple passwords (8 chars):\n");
	printf("简单密码 (8 字符):\n");
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sPass = xrtRandStr("Aa0", 0, 8);
		printf("  %s\n", sPass);
		xrtFree(sPass);
	}
	printf("\n");
	
	// Strong password (16 chars)
	// 强密码 (16 字符)
	printf("Strong passwords (16 chars):\n");
	printf("强密码 (16 字符):\n");
	for ( i = 0; i < 5; i++ ) {
		str sPass = xrtRandStr("Aa0", 0, 16);
		printf("  %s\n", sPass);
		xrtFree(sPass);
	}
	printf("\n");
}

// Test ID generation
// 测试 ID 生成
void TestIDGeneration()
{
	printf("=== ID Generation ===\n");
	printf("=== ID 生成 ===\n");
	
	// Short ID
	// 短 ID
	printf("Short IDs (8 chars):\n");
	printf("短 ID (8 字符):\n");
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sID = xrtRandStr("a0", 0, 8);
		printf("  %s\n", sID);
		xrtFree(sID);
	}
	printf("\n");
	
	// UUID-like (32 hex chars)
	// 类 UUID (32 个十六进制字符)
	printf("UUID-like (32 hex chars):\n");
	printf("类 UUID (32 个十六进制字符):\n");
	for ( i = 0; i < 3; i++ ) {
		str sUUID = xrtRandStr("0A", 0, 32);
		printf("  %.8s-%.4s-%.4s-%.4s-%.12s\n", 
		       sUUID, sUUID+8, sUUID+12, sUUID+16, sUUID+20);
		xrtFree(sUUID);
	}
	printf("\n");
}

// Test various lengths
// 测试各种长度
void TestVariousLengths()
{
	printf("=== Various Lengths ===\n");
	printf("=== 各种长度 ===\n");
	
	int iLengths[] = {4, 8, 16, 32, 64};
	int iNumLengths = 5;
	int i;
	
	for ( i = 0; i < iNumLengths; i++ ) {
		str sRand = xrtRandStr("Aa0", 0, iLengths[i]);
		printf("Length %2d: %s\n", iLengths[i], sRand);
		printf("长度 %2d: %s\n", iLengths[i], sRand);
		xrtFree(sRand);
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Generate verification code
	// 生成验证码
	printf("Verification codes (6 digits):\n");
	printf("验证码 (6 位数字):\n");
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sCode = xrtRandStr("0", 0, 6);
		printf("  %s\n", sCode);
		xrtFree(sCode);
	}
	printf("\n");
	
	// Generate coupon code
	// 生成优惠券代码
	printf("Coupon codes (8 uppercase):\n");
	printf("优惠券代码 (8 位大写):\n");
	for ( i = 0; i < 5; i++ ) {
		str sCoupon = xrtRandStr("A0", 0, 8);
		printf("  %s\n", sCoupon);
		xrtFree(sCoupon);
	}
	printf("\n");
	
	// Generate session token
	// 生成会话令牌
	printf("Session tokens (32 alphanumeric):\n");
	printf("会话令牌 (32 位字母数字):\n");
	for ( i = 0; i < 3; i++ ) {
		str sToken = xrtRandStr("a0", 0, 32);
		printf("  %s\n", sToken);
		xrtFree(sToken);
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String - Random String Generation Demo\n");
	printf("XRT 字符串 - 随机字符串生成演示\n");
	printf("===========================================\n\n");
	
	TestBasicTemplates();
	TestMixedTemplates();
	TestPasswordGeneration();
	TestIDGeneration();
	TestVariousLengths();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
