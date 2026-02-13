/*
 * XRT Example - String to Number
 * XRT 范例 - 字符串转数字
 *
 * Description / 说明:
 *   EN: Demonstrates xrtStrToI32, xrtStrToI64, xrtStrToNum, xrtParseNum
 *       for parsing numbers from strings.
 *   CN: 演示 xrtStrToI32、xrtStrToI64、xrtStrToNum、xrtParseNum 从字符串解析数字。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/jnum_str_to_num.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/jnum_str_to_num              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtStrToI32/I64/Num parse entire string
 *   - xrtParseNum returns type and value, supports JSON-style numbers
 *   - Handles various formats: decimal, hex, scientific notation
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test string to int32 conversion
// 测试字符串到 int32 转换
void TestStrToI32()
{
	printf("=== xrtStrToI32 (string to int32) ===\n");
	printf("=== xrtStrToI32 (字符串到 int32) ===\n");
	
	char* psStrings[] = {
		"0",
		"42",
		"-42",
		"2147483647",
		"-2147483648",
		"123abc",      // Partial parse
		"3.14",        // Float string
		"0xFF",        // Hex prefix
	};
	
	int iNumStrings = 8;
	int i;
	
	for ( i = 0; i < iNumStrings; i++ ) {
		int32_t iValue = xrtStrToI32(psStrings[i]);
		printf("  \"%-15s\" -> %d\n", psStrings[i], iValue);
	}
	printf("\n");
}

// Test string to int64 conversion
// 测试字符串到 int64 转换
void TestStrToI64()
{
	printf("=== xrtStrToI64 (string to int64) ===\n");
	printf("=== xrtStrToI64 (字符串到 int64) ===\n");
	
	char* psStrings[] = {
		"0",
		"42",
		"-42",
		"9223372036854775807",
		"-9223372036854775808",
		"12345678901234",
		"-98765432109876",
	};
	
	int iNumStrings = 7;
	int i;
	
	for ( i = 0; i < iNumStrings; i++ ) {
		int64_t iValue = xrtStrToI64(psStrings[i]);
		printf("  \"%-22s\" -> %lld\n", psStrings[i], iValue);
	}
	printf("\n");
}

// Test string to double conversion
// 测试字符串到 double 转换
void TestStrToNum()
{
	printf("=== xrtStrToNum (string to double) ===\n");
	printf("=== xrtStrToNum (字符串到 double) ===\n");
	
	char* psStrings[] = {
		"0",
		"3.14159",
		"-2.71828",
		"1.23e10",
		"1.23e-10",
		"123.456",
		"-0.001",
		"1e5",
	};
	
	int iNumStrings = 8;
	int i;
	
	for ( i = 0; i < iNumStrings; i++ ) {
		double fValue = xrtStrToNum(psStrings[i]);
		printf("  \"%-12s\" -> %.15g\n", psStrings[i], fValue);
	}
	printf("\n");
}

// Test xrtParseNum with type detection
// 测试带类型检测的 xrtParseNum
void TestParseNum()
{
	printf("=== xrtParseNum (with type detection) ===\n");
	printf("=== xrtParseNum (带类型检测) ===\n");
	
	char* psStrings[] = {
		"42",
		"3.14",
		"0xFF",
		"1.23e5",
		"true",
		"false",
		"12345678901234567890",  // Large number
	};
	
	char* psTypes[] = {
		"JNUM_NULL",
		"JNUM_INT",
		"JNUM_HEX",
		"JNUM_LINT",
		"JNUM_LHEX",
		"JNUM_DOUBLE",
		"JNUM_BOOL",
	};
	
	int iNumStrings = 7;
	int i;
	
	for ( i = 0; i < iNumStrings; i++ ) {
		jnum_type_t eType = JNUM_NULL;
		jnum_value_t sValue;
		
		int iLen = xrtParseNum(psStrings[i], &eType, &sValue);
		
		printf("  \"%s\"\n", psStrings[i]);
		printf("    Type: %s (len=%d)\n", psTypes[eType], iLen);
		
		// Print value based on type
		// 根据类型打印值
		switch ( eType ) {
			case JNUM_INT:
				printf("    Value: %d (int)\n", sValue.vint);
				break;
			case JNUM_HEX:
				printf("    Value: 0x%X (hex)\n", sValue.vhex);
				break;
			case JNUM_LINT:
				printf("    Value: %lld (lint)\n", sValue.vlint);
				break;
			case JNUM_LHEX:
				printf("    Value: 0x%llX (lhex)\n", sValue.vlhex);
				break;
			case JNUM_DOUBLE:
				printf("    Value: %.15g (double)\n", sValue.vdbl);
				break;
			case JNUM_BOOL:
				printf("    Value: %s (bool)\n", sValue.vbool ? "true" : "false");
				break;
			default:
				printf("    Value: (null or unknown)\n");
		}
		printf("\n");
	}
}

// Test JSON-style number parsing
// 测试 JSON 风格数字解析
void TestJSONNumbers()
{
	printf("=== JSON-Style Numbers ===\n");
	printf("=== JSON 风格数字 ===\n");
	
	char* psJSON[] = {
		"{\"count\": 42}",
		"{\"price\": 19.99}",
		"{\"ratio\": -0.5}",
		"{\"big\": 1.23e+10}",
		"{\"small\": 1e-10}",
	};
	
	int iNumJSON = 5;
	int i;
	
	for ( i = 0; i < iNumJSON; i++ ) {
		printf("JSON: %s\n", psJSON[i]);
		
		// Find number in string
		// 在字符串中查找数字
		char* pStart = strchr(psJSON[i], ':');
		if ( pStart ) {
			pStart++;  // Skip colon
			while ( *pStart == ' ' ) pStart++;  // Skip spaces
			
			jnum_type_t eType;
			jnum_value_t sValue;
			int iLen = xrtParseNum(pStart, &eType, &sValue);
			
			printf("  Number at offset %d, type=%d, len=%d\n",
			       (int)(pStart - psJSON[i]), eType, iLen);
		}
		printf("\n");
	}
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Parse config values
	// 解析配置值
	printf("Configuration parsing:\n");
	printf("配置解析:\n");
	
	char* psConfig[] = {
		"port=8080",
		"timeout=30000",
		"ratio=0.85",
		"max_size=1073741824",
	};
	
	int iNumConfig = 4;
	int i;
	
	for ( i = 0; i < iNumConfig; i++ ) {
		char* pEq = strchr(psConfig[i], '=');
		if ( pEq ) {
			*pEq = '\0';
			char* psKey = psConfig[i];
			char* psValue = pEq + 1;
			
			// Try to parse as number
			// 尝试解析为数字
			jnum_type_t eType;
			jnum_value_t sValue;
			xrtParseNum(psValue, &eType, &sValue);
			
			printf("  %s = %s (type=%d)\n", psKey, psValue, eType);
		}
	}
	printf("\n");
	
	// Parse CSV data
	// 解析 CSV 数据
	printf("CSV parsing:\n");
	printf("CSV 解析:\n");
	
	char* sCSVLine = "1001,42.5,-17,3.14159";
	printf("Line: %s\n", sCSVLine);
	printf("行: %s\n", sCSVLine);
	
	// Simple CSV parsing
	// 简单 CSV 解析
	char sCopy[64];
	strcpy(sCopy, sCSVLine);
	char* pToken = strtok(sCopy, ",");
	int iCol = 0;
	
	while ( pToken != NULL ) {
		double fValue = xrtStrToNum(pToken);
		printf("  Column %d: \"%s\" -> %.2f\n", iCol, pToken, fValue);
		pToken = strtok(NULL, ",");
		iCol++;
	}
	printf("\n");
}

// Test error handling
// 测试错误处理
void TestErrorHandling()
{
	printf("=== Error Handling ===\n");
	printf("=== 错误处理 ===\n");
	
	char* psInvalid[] = {
		"",
		"abc",
		"not a number",
		"123abc456",
		"++123",
		"--456",
	};
	
	int iNumInvalid = 6;
	int i;
	
	for ( i = 0; i < iNumInvalid; i++ ) {
		int32_t i32 = xrtStrToI32(psInvalid[i]);
		int64_t i64 = xrtStrToI64(psInvalid[i]);
		double fNum = xrtStrToNum(psInvalid[i]);
		
		printf("  \"%s\"\n", psInvalid[i]);
		printf("    I32: %d, I64: %lld, Num: %g\n", i32, i64, fNum);
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT JNUM - String to Number Demo\n");
	printf("XRT JNUM - 字符串转数字演示\n");
	printf("================================\n\n");
	
	TestStrToI32();
	TestStrToI64();
	TestStrToNum();
	TestParseNum();
	TestJSONNumbers();
	TestPracticalUseCases();
	TestErrorHandling();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
