/*
 * XRT Example - String Encoding Utilities
 * XRT 范例 - 字符串编码工具
 *
 * Description / 说明:
 *   EN: Demonstrates available string encoding utilities in XRT library
 *       including Base64, URL encoding, and hexadecimal conversion.
 *   CN: 演示 XRT 库中可用的字符串编码工具，
 *       包括 Base64、URL 编码和十六进制转换。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_encoding_conversion.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_encoding_conversion -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Base64 encoding/decoding with xrtBase64Encode/xrtBase64Decode
 *   - URL encoding/decoding with xrtUrlEncode/xrtUrlDecode
 *   - Hexadecimal encoding with xrtHexEncode/xrtHexDecode
 *   - All encoded strings are null-terminated and must be freed
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test Base64 encoding and decoding
// 测试 Base64 编码和解码
void TestBase64Encoding()
{
	printf("=== Base64 Encoding/Decoding ===\n");
	printf("=== Base64 编码/解码 ===\n");
	
	// Simple text encoding
	// 简单文本编码
	str sText1 = "Hello World";
	str sText2 = "XRT Library";
	str sText3 = "Base64 Test";
	
	// Encode to Base64
	// 编码为 Base64
	str sEncoded1 = xrtBase64Encode(sText1, strlen(sText1), NULL);
	str sEncoded2 = xrtBase64Encode(sText2, strlen(sText2), NULL);
	str sEncoded3 = xrtBase64Encode(sText3, strlen(sText3), NULL);
	
	printf("Base64 encoding:\n");
	printf("  \"%s\" -> \"%s\"\n", sText1, sEncoded1);
	printf("  \"%s\" -> \"%s\"\n", sText2, sEncoded2);
	printf("  \"%s\" -> \"%s\"\n\n", sText3, sEncoded3);
	
	// Decode from Base64
	// 从 Base64 解码
	str sDecoded1 = xrtBase64Decode(sEncoded1, strlen(sEncoded1), NULL);
	str sDecoded2 = xrtBase64Decode(sEncoded2, strlen(sEncoded2), NULL);
	str sDecoded3 = xrtBase64Decode(sEncoded3, strlen(sEncoded3), NULL);
	
	printf("Base64 decoding:\n");
	printf("  \"%s\" -> \"%s\"\n", sEncoded1, sDecoded1);
	printf("  \"%s\" -> \"%s\"\n", sEncoded2, sDecoded2);
	printf("  \"%s\" -> \"%s\"\n\n", sEncoded3, sDecoded3);
	
	// Binary data encoding
	// 二进制数据编码
	unsigned char arrBinary[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
	size_t iBinSize = sizeof(arrBinary);
	
	str sBinEncoded = xrtBase64Encode(arrBinary, iBinSize, NULL);
	printf("Binary data encoding:\n");
	printf("  Bytes: ");
	for ( size_t i = 0; i < iBinSize; i++ ) {
		printf("%02X ", arrBinary[i]);
	}
	printf("\n  Base64: \"%s\"\n\n", sBinEncoded);
	
	// Decode binary data
	// 解码二进制数据
	str sBinDecoded = xrtBase64Decode(sBinEncoded, strlen(sBinEncoded), NULL);
	
	printf("Binary data decoding:\n");
	printf("  Base64: \"%s\"\n", sBinEncoded);
	printf("  Bytes: ");
	if ( sBinDecoded ) {
		for ( size_t i = 0; i < iBinSize; i++ ) {
			printf("%02X ", (unsigned char)sBinDecoded[i]);
		}
	}
	printf("\n\n");
	
	// Cleanup
	// 清理
	xrtFree(sEncoded1);
	xrtFree(sEncoded2);
	xrtFree(sEncoded3);
	xrtFree(sDecoded1);
	xrtFree(sDecoded2);
	xrtFree(sDecoded3);
	xrtFree(sBinEncoded);
	xrtFree(sBinDecoded);
}

// Test URL encoding and decoding
// 测试 URL 编码和解码
void TestURLEncoding()
{
	printf("=== URL Encoding/Decoding ===\n");
	printf("=== URL 编码/解码 ===\n");
	
	// URL encoding examples
	// URL 编码示例
	str sParam1 = "hello world";
	str sParam2 = "user@email.com";
	str sParam3 = "special chars: !@#$%^&*()";
	
	// Encode URLs
	// 编码 URL
	str sURLEncoded1 = xrtUrlEncode(sParam1, strlen(sParam1));
	str sURLEncoded2 = xrtUrlEncode(sParam2, strlen(sParam2));
	str sURLEncoded3 = xrtUrlEncode(sParam3, strlen(sParam3));
	
	printf("URL encoding:\n");
	printf("  \"%s\" -> \"%s\"\n", sParam1, sURLEncoded1);
	printf("  \"%s\" -> \"%s\"\n", sParam2, sURLEncoded2);
	printf("  \"%s\" -> \"%s\"\n\n", sParam3, sURLEncoded3);
	
	// Decode URLs
	// 解码 URL
	str sURLDecoded1 = xrtUrlDecode(sURLEncoded1, strlen(sURLEncoded1));
	str sURLDecoded2 = xrtUrlDecode(sURLEncoded2, strlen(sURLEncoded2));
	str sURLDecoded3 = xrtUrlDecode(sURLEncoded3, strlen(sURLEncoded3));
	
	printf("URL decoding:\n");
	printf("  \"%s\" -> \"%s\"\n", sURLEncoded1, sURLDecoded1);
	printf("  \"%s\" -> \"%s\"\n", sURLEncoded2, sURLDecoded2);
	printf("  \"%s\" -> \"%s\"\n\n", sURLEncoded3, sURLDecoded3);
	
	// Query string construction
	// 查询字符串构造
	str sName = "John Doe";
	str sEmail = "john@example.com";
	int iAge = 30;
	
	str sNameEncoded = xrtUrlEncode(sName, strlen(sName));
	str sEmailEncoded = xrtUrlEncode(sEmail, strlen(sEmail));
	
	str sQueryString = xrtMalloc(strlen(sNameEncoded) + strlen(sEmailEncoded) + 64);
	sprintf(sQueryString, "name=%s&email=%s&age=%d", sNameEncoded, sEmailEncoded, iAge);
	
	printf("Query string construction:\n");
	printf("  Parameters:\n");
	printf("    name: %s\n", sName);
	printf("    email: %s\n", sEmail);
	printf("    age: %d\n", iAge);
	printf("  Encoded query: \"%s\"\n\n", sQueryString);
	
	// Cleanup
	// 清理
	xrtFree(sURLEncoded1);
	xrtFree(sURLEncoded2);
	xrtFree(sURLEncoded3);
	xrtFree(sURLDecoded1);
	xrtFree(sURLDecoded2);
	xrtFree(sURLDecoded3);
	xrtFree(sNameEncoded);
	xrtFree(sEmailEncoded);
	xrtFree(sQueryString);
}

// Test hexadecimal encoding
// 测试十六进制编码
void TestHexEncoding()
{
	printf("=== Hexadecimal Encoding ===\n");
	printf("=== 十六进制编码 ===\n");
	
	// Text to hex conversion
	// 文本到十六进制转换
	str sText = "Hello";
	
	// Convert to hex string
	// 转换为十六进制字符串
	str sHex = xrtHexEncode(sText, strlen(sText));
	
	printf("Text to hex:\n");
	printf("  Text: \"%s\"\n", sText);
	printf("  Hex:  \"%s\"\n\n", sHex);
	
	// Hex to text conversion
	// 十六进制到文本转换
	str sDecodedText = xrtHexDecode(sHex, strlen(sHex));
	
	printf("Hex to text:\n");
	printf("  Hex:  \"%s\"\n", sHex);
	printf("  Text: \"%s\"\n\n", sDecodedText);
	
	// Binary data hex encoding
	// 二进制数据十六进制编码
	unsigned char arrData[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0xFF, 0xDE, 0xAD};
	size_t iDataSize = sizeof(arrData);
	
	str sDataHex = xrtHexEncode(arrData, iDataSize);
	
	printf("Binary data hex encoding:\n");
	printf("  Data bytes: ");
	for ( size_t i = 0; i < iDataSize; i++ ) {
		printf("%02X ", arrData[i]);
	}
	printf("\n  Hex: \"%s\"\n\n", sDataHex);
	
	// Decode binary hex
	// 解码二进制十六进制
	str sDecodedData = xrtHexDecode(sDataHex, strlen(sDataHex));
	
	printf("Binary hex decoding:\n");
	printf("  Hex: \"%s\"\n", sDataHex);
	printf("  Decoded bytes: ");
	if ( sDecodedData ) {
		for ( size_t i = 0; i < iDataSize; i++ ) {
			printf("%02X ", (unsigned char)sDecodedData[i]);
		}
	}
	printf("\n\n");
	
	// Cleanup
	// 清理
	xrtFree(sHex);
	xrtFree(sDecodedText);
	xrtFree(sDataHex);
	xrtFree(sDecodedData);
}

// Test ASCII validation and conversion
// 测试 ASCII 验证和转换
void TestASCIIValidation()
{
	printf("=== ASCII Validation and Conversion ===\n");
	printf("=== ASCII 验证和转换 ===\n");
	
	// ASCII validation (manual implementation)
	// ASCII 验证（手动实现）
	str sPureASCII = "Hello World 123!";
	str sWithUnicode = "Hello 世界";
	
	bool bASCII1 = TRUE, bASCII2 = TRUE;
	for ( size_t i = 0; i < strlen(sPureASCII); i++ ) {
		if ( (unsigned char)sPureASCII[i] > 127 ) { bASCII1 = FALSE; break; }
	}
	for ( size_t i = 0; i < strlen(sWithUnicode); i++ ) {
		if ( (unsigned char)sWithUnicode[i] > 127 ) { bASCII2 = FALSE; break; }
	}
	
	printf("ASCII validation:\n");
	printf("  Pure ASCII: \"%s\" -> %s\n", sPureASCII, bASCII1 ? "VALID" : "INVALID");
	printf("  With Unicode: \"%s\" -> %s\n\n", sWithUnicode, bASCII2 ? "VALID" : "INVALID");
	
	// Printable ASCII check
	// 可打印 ASCII 检查
	bool bPrint1 = TRUE;
	for ( size_t i = 0; i < strlen(sPureASCII); i++ ) {
		unsigned char c = (unsigned char)sPureASCII[i];
		if ( c < 32 || c == 127 ) { bPrint1 = FALSE; break; }
	}
	
	printf("Printable ASCII check:\n");
	printf("  \"%s\" -> %s\n\n", sPureASCII, bPrint1 ? "PRINTABLE" : "CONTAINS CONTROL CHARS");
	
	// ASCII to numeric conversion
	// ASCII 到数字转换
	str sNumbers = "12345";
	str sMixed = "123abc";
	
	int iNum1 = atoi(sNumbers);
	int iNum2 = atoi(sMixed);  // Will stop at 'a'
	
	printf("ASCII to integer conversion:\n");
	printf("  \"%s\" -> %d\n", sNumbers, iNum1);
	printf("  \"%s\" -> %d (stops at non-digit)\n\n", sMixed, iNum2);
}

// Test practical encoding scenarios
// 测试实际编码场景
void TestPracticalScenarios()
{
	printf("=== Practical Encoding Scenarios ===\n");
	printf("=== 实际编码场景 ===\n");
	
	// JSON data encoding
	// JSON 数据编码
	str sJSONData = "{\"name\":\"John Doe\",\"email\":\"john@example.com\",\"active\":true}";
	
	// Base64 encode for transmission
	// Base64 编码用于传输
	str sJSONEncoded = xrtBase64Encode(sJSONData, strlen(sJSONData), NULL);
	
	printf("JSON data encoding:\n");
	printf("  Original: %s\n", sJSONData);
	printf("  Base64:   %s\n\n", sJSONEncoded);
	
	// Decode and verify
	// 解码并验证
	str sJSONDecoded = xrtBase64Decode(sJSONEncoded, strlen(sJSONEncoded), NULL);
	
	printf("  Decoded:  %s\n\n", sJSONDecoded);
	
	// File name encoding
	// 文件名编码
	str sFileName = "my document (version 1).txt";
	
	// URL encode for safe transmission
	// URL 编码用于安全传输
	str sFileNameEncoded = xrtUrlEncode(sFileName, strlen(sFileName));
	
	printf("File name encoding:\n");
	printf("  Original: \"%s\"\n", sFileName);
	printf("  URL encoded: \"%s\"\n\n", sFileNameEncoded);
	
	// Session token generation
	// 会话令牌生成
	str sUserID = "user123";
	unsigned int iTimestamp = (unsigned int)time(NULL);
	
	// Create token data
	// 创建令牌数据
	str sTokenData = xrtFormat("%s:%u", sUserID, iTimestamp);
	
	// Encode token
	// 编码令牌
	str sToken = xrtBase64Encode(sTokenData, strlen(sTokenData), NULL);
	
	printf("Session token generation:\n");
	printf("  User ID: %s\n", sUserID);
	printf("  Timestamp: %u\n", iTimestamp);
	printf("  Token data: \"%s\"\n", sTokenData);
	printf("  Final token: \"%s\"\n\n", sToken);
	
	// Hex dump for debugging
	// 十六进制转储用于调试
	unsigned char arrPacket[] = {0x01, 0x02, 0x03, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF};
	size_t iPacketSize = sizeof(arrPacket);
	
	str sHexDump = xrtHexEncode(arrPacket, iPacketSize);
	
	printf("Network packet hex dump:\n");
	printf("  Bytes: ");
	for ( size_t i = 0; i < iPacketSize; i++ ) {
		printf("%02X ", arrPacket[i]);
	}
	printf("\n  Hex: %s\n\n", sHexDump);
	
	// Cleanup
	// 清理
	xrtFree(sJSONEncoded);
	xrtFree(sJSONDecoded);
	xrtFree(sFileNameEncoded);
	xrtFree(sTokenData);
	xrtFree(sToken);
	xrtFree(sHexDump);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Encoding Utilities Demo\n");
	printf("XRT 字符串编码工具演示\n");
	printf("================================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBase64Encoding();
	TestURLEncoding();
	TestHexEncoding();
	TestASCIIValidation();
	TestPracticalScenarios();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}