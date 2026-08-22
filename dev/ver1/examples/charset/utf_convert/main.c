/*
 * XRT Example - UTF Conversion
 * XRT 范例 - UTF 编码转换
 *
 * Description / 说明:
 *   EN: Demonstrates UTF-8/16/32 encoding conversion functions.
 *   CN: 演示 UTF-8/16/32 编码转换函数。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/charset_utf_convert.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/charset_utf_convert              (Linux, GCC)
 *
 * Note / 注意:
 *   - All conversion functions allocate new memory, must free with xrtFree
 *   - Size=0 means auto-detect string length
 *   - Returns NULL on failure
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Print hex dump of bytes
// 打印字节十六进制转储
void PrintHex(str sData, size_t iSize, str sLabel)
{
	printf("%s: ", sLabel);
	size_t i;
	for ( i = 0; i < iSize && i < 32; i++ ) {
		printf("%02X ", (unsigned char)sData[i]);
	}
	if ( iSize > 32 ) printf("...");
	printf("\n");
}

// Test UTF-8 to UTF-16 conversion
// 测试 UTF-8 到 UTF-16 转换
void TestUTF8to16()
{
	printf("=== UTF-8 to UTF-16 ===\n");
	printf("=== UTF-8 到 UTF-16 ===\n");
	
	// ASCII text
	// ASCII 文本
	str sASCII = "Hello";
	size_t iRetSize;
	u16str sUTF16 = xrtUTF8to16(sASCII, 0, &iRetSize);
	
	printf("ASCII: \"%s\"\n", sASCII);
	printf("UTF-16 size: %zu chars\n", iRetSize);
	PrintHex((str)sUTF16, iRetSize * 2, "UTF-16 bytes");
	xrtFree(sUTF16);
	printf("\n");
	
	// Chinese text
	// 中文文本
	str sChinese = "你好世界";
	sUTF16 = xrtUTF8to16(sChinese, 0, &iRetSize);
	
	printf("Chinese: \"%s\"\n", sChinese);
	printf("UTF-16 size: %zu chars\n", iRetSize);
	PrintHex((str)sUTF16, iRetSize * 2, "UTF-16 bytes");
	xrtFree(sUTF16);
	printf("\n");
}

// Test UTF-8 to UTF-32 conversion
// 测试 UTF-8 到 UTF-32 转换
void TestUTF8to32()
{
	printf("=== UTF-8 to UTF-32 ===\n");
	printf("=== UTF-8 到 UTF-32 ===\n");
	
	str sText = "A中";  // 1 ASCII + 1 Chinese char
	size_t iRetSize;
	u32str sUTF32 = xrtUTF8to32(sText, 0, &iRetSize);
	
	printf("Text: \"%s\"\n", sText);
	printf("UTF-32 size: %zu chars\n", iRetSize);
	
	// Show code points
	// 显示码点
	printf("Code points: ");
	size_t i;
	for ( i = 0; i < iRetSize; i++ ) {
		printf("U+%04X ", sUTF32[i]);
	}
	printf("\n");
	
	PrintHex((str)sUTF32, iRetSize * 4, "UTF-32 bytes");
	xrtFree(sUTF32);
	printf("\n");
}

// Test UTF-16 to UTF-8 conversion
// 测试 UTF-16 到 UTF-8 转换
void TestUTF16to8()
{
	printf("=== UTF-16 to UTF-8 ===\n");
	printf("=== UTF-16 到 UTF-8 ===\n");
	
	// Create UTF-16 string manually: "Hi"
	// 手动创建 UTF-16 字符串: "Hi"
	wchar_t sWide[] = L"Hi";
	u16str sUTF16 = (u16str)sWide;
	
	size_t iRetSize;
	str sUTF8 = xrtUTF16to8(sUTF16, 2, &iRetSize);
	
	printf("UTF-16 source: %04X %04X\n", sUTF16[0], sUTF16[1]);
	printf("UTF-8 result: \"%s\"\n", sUTF8);
	printf("UTF-8 size: %zu bytes\n", iRetSize);
	xrtFree(sUTF8);
	printf("\n");
}

// Test UTF-32 to UTF-8 conversion
// 测试 UTF-32 到 UTF-8 转换
void TestUTF32to8()
{
	printf("=== UTF-32 to UTF-8 ===\n");
	printf("=== UTF-32 到 UTF-8 ===\n");
	
	// Create UTF-32 string manually: "AB"
	// 手动创建 UTF-32 字符串: "AB"
	uint32_t sUTF32[] = {0x41, 0x42, 0};  // 'A', 'B', null
	
	size_t iRetSize;
	str sUTF8 = xrtUTF32to8((u32str)sUTF32, 2, &iRetSize);
	
	printf("UTF-32 source: U+%04X U+%04X\n", sUTF32[0], sUTF32[1]);
	printf("UTF-8 result: \"%s\"\n", sUTF8);
	printf("UTF-8 size: %zu bytes\n", iRetSize);
	xrtFree(sUTF8);
	printf("\n");
}

// Test round-trip conversion
// 测试往返转换
void TestRoundTrip()
{
	printf("=== Round-Trip Conversion ===\n");
	printf("=== 往返转换 ===\n");
	
	str sOriginal = "Hello 世界! 🌍";  // Mix of ASCII, Chinese, emoji
	printf("Original: \"%s\"\n", sOriginal);
	printf("原始: \"%s\"\n", sOriginal);
	
	// UTF-8 -> UTF-16 -> UTF-8
	// UTF-8 -> UTF-16 -> UTF-8
	u16str sUTF16 = xrtUTF8to16(sOriginal, 0, NULL);
	str sBack8 = xrtUTF16to8(sUTF16, 0, NULL);
	
	printf("UTF-8 -> UTF-16 -> UTF-8: \"%s\"\n", sBack8);
	printf("Match: %s\n", strcmp(sOriginal, sBack8) == 0 ? "YES" : "NO");
	printf("匹配: %s\n", strcmp(sOriginal, sBack8) == 0 ? "是" : "否");
	xrtFree(sUTF16);
	xrtFree(sBack8);
	
	// UTF-8 -> UTF-32 -> UTF-8
	// UTF-8 -> UTF-32 -> UTF-8
	u32str sUTF32 = xrtUTF8to32(sOriginal, 0, NULL);
	sBack8 = xrtUTF32to8(sUTF32, 0, NULL);
	
	printf("UTF-8 -> UTF-32 -> UTF-8: \"%s\"\n", sBack8);
	printf("Match: %s\n", strcmp(sOriginal, sBack8) == 0 ? "YES" : "NO");
	printf("匹配: %s\n", strcmp(sOriginal, sBack8) == 0 ? "是" : "否");
	xrtFree(sUTF32);
	xrtFree(sBack8);
	
	printf("\n");
}

// Test character counting
// 测试字符计数
void TestCharCounting()
{
	printf("=== Character Counting ===\n");
	printf("=== 字符计数 ===\n");
	
	str sText = "Hello世界🌍";
	
	// UTF-8 byte count
	// UTF-8 字节数
	size_t iBytes = strlen(sText);
	printf("UTF-8 bytes: %zu\n", iBytes);
	printf("UTF-8 字节数: %zu\n", iBytes);
	
	// Convert to UTF-32 to count characters
	// 转换为 UTF-32 计数字符数
	size_t iChars;
	u32str sUTF32 = xrtUTF8to32(sText, 0, &iChars);
	printf("Character count (UTF-32): %zu\n", iChars);
	printf("字符数 (UTF-32): %zu\n", iChars);
	
	printf("\nCharacters:\n");
	printf("字符:\n");
	size_t i;
	for ( i = 0; i < iChars; i++ ) {
		printf("  [%zu] U+%04X\n", i, sUTF32[i]);
	}
	
	xrtFree(sUTF32);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Charset - UTF Conversion Demo\n");
	printf("XRT 字符集 - UTF 编码转换演示\n");
	printf("=================================\n\n");
	
	TestUTF8to16();
	TestUTF8to32();
	TestUTF16to8();
	TestUTF32to8();
	TestRoundTrip();
	TestCharCounting();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
