#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

/*
 * XRT Example - String Encoding Utilities
 *
 * Demonstrates:
 * - Base64 encode/decode
 * - Percent encode/decode with fixed output buffers
 * - Hex encode/decode
 * - A few practical composition scenarios
 */

static bool DemoPercentEncode(str sText, char* sOut, size_t iOutCap)
{
	size_t iOutLen = 0;

	if ( !sText || !sOut || iOutCap == 0 ) return FALSE;
	if ( !xrtPercentEncodeTo(sText, strlen(sText), sOut, iOutCap, &iOutLen, FALSE) ) return FALSE;
	sOut[iOutLen] = '\0';
	return TRUE;
}

static bool DemoPercentDecode(str sText, char* sOut, size_t iOutCap)
{
	size_t iOutLen = 0;

	if ( !sText || !sOut || iOutCap == 0 ) return FALSE;
	if ( !xrtPercentDecodeTo(sText, strlen(sText), sOut, iOutCap, &iOutLen, TRUE) ) return FALSE;
	sOut[iOutLen] = '\0';
	return TRUE;
}

static void TestBase64Encoding(void)
{
	str sText1 = "Hello World";
	str sText2 = "XRT Library";
	str sText3 = "Base64 Test";
	unsigned char arrBinary[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
	size_t iBinSize = sizeof(arrBinary);
	str sEncoded1 = xrtBase64Encode(sText1, strlen(sText1), NULL);
	str sEncoded2 = xrtBase64Encode(sText2, strlen(sText2), NULL);
	str sEncoded3 = xrtBase64Encode(sText3, strlen(sText3), NULL);
	str sDecoded1 = xrtBase64Decode(sEncoded1, strlen(sEncoded1), NULL);
	str sDecoded2 = xrtBase64Decode(sEncoded2, strlen(sEncoded2), NULL);
	str sDecoded3 = xrtBase64Decode(sEncoded3, strlen(sEncoded3), NULL);
	str sBinEncoded = xrtBase64Encode(arrBinary, iBinSize, NULL);
	str sBinDecoded = xrtBase64Decode(sBinEncoded, strlen(sBinEncoded), NULL);

	printf("=== Base64 Encoding/Decoding ===\n");
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sText1, sEncoded1, sDecoded1);
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sText2, sEncoded2, sDecoded2);
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sText3, sEncoded3, sDecoded3);
	printf("Binary -> \"%s\" -> ", sBinEncoded);
	if ( sBinDecoded ) {
		for ( size_t i = 0; i < iBinSize; ++i ) {
			printf("%02X ", (unsigned char)sBinDecoded[i]);
		}
	}
	printf("\n\n");

	xrtFree(sEncoded1);
	xrtFree(sEncoded2);
	xrtFree(sEncoded3);
	xrtFree(sDecoded1);
	xrtFree(sDecoded2);
	xrtFree(sDecoded3);
	xrtFree(sBinEncoded);
	xrtFree(sBinDecoded);
}

static void TestPercentEncoding(void)
{
	str sParam1 = "hello world";
	str sParam2 = "user@email.com";
	str sParam3 = "special chars: !@#$%^&*()";
	str sName = "John Doe";
	str sEmail = "john@example.com";
	char sURLEncoded1[256];
	char sURLEncoded2[256];
	char sURLEncoded3[256];
	char sURLDecoded1[256];
	char sURLDecoded2[256];
	char sURLDecoded3[256];
	char sNameEncoded[256];
	char sEmailEncoded[256];
	char sQueryString[512];

	DemoPercentEncode(sParam1, sURLEncoded1, sizeof(sURLEncoded1));
	DemoPercentEncode(sParam2, sURLEncoded2, sizeof(sURLEncoded2));
	DemoPercentEncode(sParam3, sURLEncoded3, sizeof(sURLEncoded3));
	DemoPercentDecode(sURLEncoded1, sURLDecoded1, sizeof(sURLDecoded1));
	DemoPercentDecode(sURLEncoded2, sURLDecoded2, sizeof(sURLDecoded2));
	DemoPercentDecode(sURLEncoded3, sURLDecoded3, sizeof(sURLDecoded3));
	DemoPercentEncode(sName, sNameEncoded, sizeof(sNameEncoded));
	DemoPercentEncode(sEmail, sEmailEncoded, sizeof(sEmailEncoded));
	sprintf(sQueryString, "name=%s&email=%s&age=%d", sNameEncoded, sEmailEncoded, 30);

	printf("=== Percent Encoding/Decoding ===\n");
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sParam1, sURLEncoded1, sURLDecoded1);
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sParam2, sURLEncoded2, sURLDecoded2);
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sParam3, sURLEncoded3, sURLDecoded3);
	printf("Query string: \"%s\"\n\n", sQueryString);
}

static void TestHexEncoding(void)
{
	str sText = "Hello";
	unsigned char arrData[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0xFF, 0xDE, 0xAD};
	size_t iDataSize = sizeof(arrData);
	str sHex = xrtHexEncode(sText, strlen(sText));
	str sDecodedText = xrtHexDecode(sHex, strlen(sHex));
	str sDataHex = xrtHexEncode(arrData, iDataSize);
	str sDecodedData = xrtHexDecode(sDataHex, strlen(sDataHex));

	printf("=== Hexadecimal Encoding ===\n");
	printf("\"%s\" -> \"%s\" -> \"%s\"\n", sText, sHex, sDecodedText);
	printf("Binary -> \"%s\" -> ", sDataHex);
	if ( sDecodedData ) {
		for ( size_t i = 0; i < iDataSize; ++i ) {
			printf("%02X ", (unsigned char)sDecodedData[i]);
		}
	}
	printf("\n\n");

	xrtFree(sHex);
	xrtFree(sDecodedText);
	xrtFree(sDataHex);
	xrtFree(sDecodedData);
}

static void TestPracticalScenarios(void)
{
	str sJSONData = "{\"name\":\"John Doe\",\"email\":\"john@example.com\",\"active\":true}";
	str sFileName = "my document (version 1).txt";
	str sUserID = "user123";
	unsigned char arrPacket[] = {0x01, 0x02, 0x03, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF};
	char sFileNameEncoded[256];
	str sJSONEncoded = xrtBase64Encode(sJSONData, strlen(sJSONData), NULL);
	str sJSONDecoded = xrtBase64Decode(sJSONEncoded, strlen(sJSONEncoded), NULL);
	str sTokenData = xrtFormat("%s:%u", sUserID, (unsigned int)time(NULL));
	str sToken = xrtBase64Encode(sTokenData, strlen(sTokenData), NULL);
	str sHexDump = xrtHexEncode(arrPacket, sizeof(arrPacket));

	DemoPercentEncode(sFileName, sFileNameEncoded, sizeof(sFileNameEncoded));

	printf("=== Practical Encoding Scenarios ===\n");
	printf("JSON Base64: %s\n", sJSONEncoded);
	printf("JSON Decoded: %s\n", sJSONDecoded);
	printf("File name encoded: %s\n", sFileNameEncoded);
	printf("Session token: %s\n", sToken);
	printf("Packet hex dump: %s\n\n", sHexDump);

	xrtFree(sJSONEncoded);
	xrtFree(sJSONDecoded);
	xrtFree(sTokenData);
	xrtFree(sToken);
	xrtFree(sHexDump);
}

int main(void)
{
	xrtInit();

	printf("XRT String Encoding Utilities Demo\n");
	printf("==================================\n\n");

	TestBase64Encoding();
	TestPercentEncoding();
	TestHexEncoding();
	TestPracticalScenarios();

	xrtUnit();
	return 0;
}
