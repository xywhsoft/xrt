/*
 * XRT Example - String Encoding
 * XRT 范例 - 字符串编码
 *
 * Description / 说明:
 *   EN: Demonstrates hexadecimal, Base64 and percent encoding helpers.
 *   CN: 演示十六进制、Base64 与百分号编码辅助函数。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_encoding.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/string_encoding -lm -lpthread
 *
 * Note / 注意:
 *   - Dynamic strings returned by encode/decode helpers must be freed with xrtFree.
 *   - Percent encoding uses fixed output buffers in this example.
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


static void DemoHex(void)
{
	uint8 arrBytes[] = { 0x41, 0x42, 0x43, 0x00, 0x7F, 0x80, 0xFF };
	str sHex = xrtHexEncode(arrBytes, sizeof(arrBytes));
	str sDecoded = xrtHexDecode(sHex, strlen(sHex));

	printf("Hex encode: ");
	exPrintHex(arrBytes, sizeof(arrBytes));
	printf(" -> %s\n", sHex);

	printf("Hex decode matches: %s\n", exBoolText(exBytesEqual(arrBytes, sDecoded, sizeof(arrBytes))));

	xrtFree(sHex);
	xrtFree(sDecoded);
}



static void DemoBase64(void)
{
	str sText = "XRT Base64 Demo";
	str sEncoded = xrtBase64Encode(sText, strlen(sText), NULL);
	str sDecoded = xrtBase64Decode(sEncoded, strlen(sEncoded), NULL);

	printf("Base64 encode: %s -> %s\n", sText, sEncoded);
	printf("Base64 decode: %s\n", sDecoded);

	xrtFree(sEncoded);
	xrtFree(sDecoded);
}



static void DemoPercent(void)
{
	str sText = "name=张三 & city=Shanghai";
	char sEncoded[256];
	char sDecoded[256];
	size_t iEncoded = 0;
	size_t iDecoded = 0;

	if ( xrtPercentEncodeTo(sText, strlen(sText), sEncoded, sizeof(sEncoded) - 1, &iEncoded, FALSE) ) {
		sEncoded[iEncoded] = '\0';
		printf("Percent encode: %s -> %s\n", sText, sEncoded);
	}

	if ( xrtPercentDecodeTo(sEncoded, iEncoded, sDecoded, sizeof(sDecoded) - 1, &iDecoded, TRUE) ) {
		sDecoded[iDecoded] = '\0';
		printf("Percent decode: %s\n", sDecoded);
	}
}



int main()
{
	xrtInit();

	printf("=== String Encoding ===\n");
	printf("=== 字符串编码 ===\n");

	DemoHex();
	printf("\n");

	DemoBase64();
	printf("\n");

	DemoPercent();

	xrtUnit();
	return 0;
}
