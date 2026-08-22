/*
 * XRT Example - Endian Conversion
 * XRT 范例 - 字节序转换
 *
 * Description / 说明:
 *   EN: Demonstrates xrtUTF16LEtoBE and xrtUTF32LEtoBE for converting
 *       between little-endian and big-endian encodings.
 *   CN: 演示 xrtUTF16LEtoBE 和 xrtUTF32LEtoBE 在小端序和大端序之间转换。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/charset_endian_convert.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/charset_endian_convert              (Linux, GCC)
 *
 * Note / 注意:
 *   - Conversion is in-place (modifies source buffer)
 *   - bSrcRevise=TRUE means source is in opposite endianness
 *   - Windows is typically little-endian
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Print 16-bit values in hex
// 以十六进制打印 16 位值
void PrintHex16(u16str sData, size_t iCount, str sLabel)
{
	printf("%s: ", sLabel);
	size_t i;
	for ( i = 0; i < iCount && i < 8; i++ ) {
		printf("%04X ", sData[i]);
	}
	printf("\n");
}

// Print 32-bit values in hex
// 以十六进制打印 32 位值
void PrintHex32(u32str sData, size_t iCount, str sLabel)
{
	printf("%s: ", sLabel);
	size_t i;
	for ( i = 0; i < iCount && i < 8; i++ ) {
		printf("%08X ", sData[i]);
	}
	printf("\n");
}

// Test UTF-16 endianness conversion
// 测试 UTF-16 字节序转换
void TestUTF16Endian()
{
	printf("=== UTF-16 Endian Conversion ===\n");
	printf("=== UTF-16 字节序转换 ===\n");
	
	// Create UTF-16LE string: "AB" (native on x86)
	// 创建 UTF-16LE 字符串: "AB" (x86 原生)
	wchar_t sWide[] = L"AB";
	u16str sUTF16 = (u16str)sWide;
	
	printf("Original UTF-16LE: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	printf("原始 UTF-16LE: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	
	// Convert LE to BE (in-place)
	// 将 LE 转换为 BE (原地)
	xrtUTF16LEtoBE(sUTF16, 2, FALSE);  // FALSE = source is LE
	
	printf("After LE->BE: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	printf("LE->BE 后: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	
	// Convert back BE to LE
	// 将 BE 转回 LE
	xrtUTF16LEtoBE(sUTF16, 2, TRUE);  // TRUE = source is BE (needs revision)
	
	printf("After BE->LE: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	printf("BE->LE 后: A=%04X, B=%04X\n", sUTF16[0], sUTF16[1]);
	printf("\n");
}

// Test UTF-32 endianness conversion
// 测试 UTF-32 字节序转换
void TestUTF32Endian()
{
	printf("=== UTF-32 Endian Conversion ===\n");
	printf("=== UTF-32 字节序转换 ===\n");
	
	// Create UTF-32LE string: "AB"
	// 创建 UTF-32LE 字符串: "AB"
	uint32_t sUTF32[] = {0x41, 0x42, 0};  // 'A', 'B', null
	
	printf("Original UTF-32LE: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	printf("原始 UTF-32LE: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	
	// Convert LE to BE
	// 将 LE 转换为 BE
	xrtUTF32LEtoBE((u32str)sUTF32, 2, FALSE);
	
	printf("After LE->BE: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	printf("LE->BE 后: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	
	// Convert back
	// 转回
	xrtUTF32LEtoBE((u32str)sUTF32, 2, TRUE);
	
	printf("After BE->LE: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	printf("BE->LE 后: A=%08X, B=%08X\n", sUTF32[0], sUTF32[1]);
	printf("\n");
}

// Test with Chinese characters
// 测试中文字符
void TestChineseUTF16()
{
	printf("=== UTF-16 Chinese Characters ===\n");
	printf("=== UTF-16 中文字符 ===\n");
	
	// Convert UTF-8 "你好" to UTF-16
	// 将 UTF-8 "你好" 转换为 UTF-16
	str sChinese = "你好";
	u16str sUTF16 = xrtUTF8to16(sChinese, 0, NULL);
	
	printf("Chinese: %s\n", sChinese);
	printf("中文: %s\n", sChinese);
	PrintHex16(sUTF16, 2, "UTF-16LE codes");
	
	// Show byte representation
	// 显示字节表示
	unsigned char* pBytes = (unsigned char*)sUTF16;
	printf("Bytes: %02X %02X %02X %02X\n", 
	       pBytes[0], pBytes[1], pBytes[2], pBytes[3]);
	
	// Convert to big-endian
	// 转换为大端序
	xrtUTF16LEtoBE(sUTF16, 2, FALSE);
	PrintHex16(sUTF16, 2, "UTF-16BE codes");
	
	printf("Bytes after BE: %02X %02X %02X %02X\n", 
	       pBytes[0], pBytes[1], pBytes[2], pBytes[3]);
	
	xrtFree(sUTF16);
	printf("\n");
}

// Test BOM handling
// 测试 BOM 处理
void TestBOM()
{
	printf("=== BOM (Byte Order Mark) ===\n");
	printf("=== BOM (字节顺序标记) ===\n");
	
	// UTF-8 BOM
	// UTF-8 BOM
	unsigned char sBOM8[] = {0xEF, 0xBB, 0xBF};
	printf("UTF-8 BOM: %02X %02X %02X\n", sBOM8[0], sBOM8[1], sBOM8[2]);
	printf("UTF-8 BOM: %02X %02X %02X\n", sBOM8[0], sBOM8[1], sBOM8[2]);
	
	// UTF-16 LE BOM
	// UTF-16 LE BOM
	unsigned char sBOM16LE[] = {0xFF, 0xFE};
	printf("UTF-16LE BOM: %02X %02X\n", sBOM16LE[0], sBOM16LE[1]);
	printf("UTF-16LE BOM: %02X %02X\n", sBOM16LE[0], sBOM16LE[1]);
	
	// UTF-16 BE BOM
	// UTF-16 BE BOM
	unsigned char sBOM16BE[] = {0xFE, 0xFF};
	printf("UTF-16BE BOM: %02X %02X\n", sBOM16BE[0], sBOM16BE[1]);
	printf("UTF-16BE BOM: %02X %02X\n", sBOM16BE[0], sBOM16BE[1]);
	
	// UTF-32 LE BOM
	// UTF-32 LE BOM
	unsigned char sBOM32LE[] = {0xFF, 0xFE, 0x00, 0x00};
	printf("UTF-32LE BOM: %02X %02X %02X %02X\n", 
	       sBOM32LE[0], sBOM32LE[1], sBOM32LE[2], sBOM32LE[3]);
	printf("UTF-32LE BOM: %02X %02X %02X %02X\n", 
	       sBOM32LE[0], sBOM32LE[1], sBOM32LE[2], sBOM32LE[3]);
	
	// UTF-32 BE BOM
	// UTF-32 BE BOM
	unsigned char sBOM32BE[] = {0x00, 0x00, 0xFE, 0xFF};
	printf("UTF-32BE BOM: %02X %02X %02X %02X\n", 
	       sBOM32BE[0], sBOM32BE[1], sBOM32BE[2], sBOM32BE[3]);
	printf("UTF-32BE BOM: %02X %02X %02X %02X\n", 
	       sBOM32BE[0], sBOM32BE[1], sBOM32BE[2], sBOM32BE[3]);
	
	printf("\n");
}

// Test practical use case
// 测试实际用例
void TestPracticalUseCase()
{
	printf("=== Practical: Cross-Platform Data Exchange ===\n");
	printf("=== 实际: 跨平台数据交换 ===\n");
	
	printf("Scenario: Send UTF-16 string from x86 (LE) to network (BE)\n");
	printf("场景: 从 x86 (LE) 发送 UTF-16 字符串到网络 (BE)\n\n");
	
	// Original data in native format
	// 原生格式的原始数据
	str sText = "Data";
	u16str sUTF16 = xrtUTF8to16(sText, 0, NULL);
	
	printf("1. Original UTF-16LE (x86 native):\n");
	printf("1. 原始 UTF-16LE (x86 原生):\n");
	PrintHex16(sUTF16, 4, "   Data");
	
	// Convert to network byte order (big-endian)
	// 转换为网络字节序 (大端序)
	printf("\n2. Convert to network byte order (BE):\n");
	printf("2. 转换为网络字节序 (BE):\n");
	xrtUTF16LEtoBE(sUTF16, 4, FALSE);
	PrintHex16(sUTF16, 4, "   Data");
	
	// Simulate receiving on another system
	// 模拟在另一系统上接收
	printf("\n3. Received data, convert back to LE:\n");
	printf("3. 接收数据，转回 LE:\n");
	xrtUTF16LEtoBE(sUTF16, 4, TRUE);
	PrintHex16(sUTF16, 4, "   Data");
	
	// Convert back to UTF-8
	// 转回 UTF-8
	str sBack = xrtUTF16to8(sUTF16, 0, NULL);
	printf("\n4. Final result: \"%s\"\n", sBack);
	printf("4. 最终结果: \"%s\"\n", sBack);
	
	xrtFree(sUTF16);
	xrtFree(sBack);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Charset - Endian Conversion Demo\n");
	printf("XRT 字符集 - 字节序转换演示\n");
	printf("====================================\n\n");
	
	TestUTF16Endian();
	TestUTF32Endian();
	TestChineseUTF16();
	TestBOM();
	TestPracticalUseCase();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
