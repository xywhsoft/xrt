


// Charset 库测试
void Test_Charset(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Charset 库测试（windows 测 utf16，linux 测 utf32） :\n\n");
	
	str stru8 = "𠀀𫝑😀�";
	
	#if defined(_WIN32) || defined(_WIN64)
		u16str stru16 = L"𠀀𫝑😀�";
		
		str sConv8 = xrtUTF16to8(stru16, 0, NULL);
		u16str sConv16 = xrtUTF8to16(stru8, 0, NULL);
		str aHex = xrtHexEncode(stru8, 0);
		str bHex = xrtHexEncode(stru16, u16len(stru16) * 2);
		str cHex = xrtHexEncode(sConv8, 0);
		str dHex = xrtHexEncode(sConv16, u16len(sConv16) * 2);
		
		printf("src utf8 : %s", stru8);
		printf("\nsrc utf16 : %s", stru16);
		printf("\nconv utf8 : %s", sConv8);
		printf("\nconv utf16 : %s", sConv16);
		printf("\nsrc utf8 Hex : %s", aHex);
		printf("\nsrc utf16 Hex : %s", bHex);
		printf("\nconv utf8 Hex : %s", cHex);
		printf("\nconv utf16 Hex : %s", dHex);
	#else
		u32str stru32 = L"𠀀𫝑😀�";
		
		str sConv8 = xrtUTF32to8(stru32, 0, NULL);
		u32str sConv32 = xrtUTF8to32(stru8, 0, NULL);
		
		str aHex = xrtHexEncode(stru8, 0);
		str bHex = xrtHexEncode(stru32, u32len(stru32) * 4);
		str cHex = xrtHexEncode(sConv8, 0);
		str dHex = xrtHexEncode(sConv32, u32len(sConv32) * 4);
		
		printf("src utf8 : %s", stru8);
		printf("\nsrc utf32 : %s", stru32);
		printf("\nconv utf8 : %s", sConv8);
		printf("\nconv utf32 : %s", sConv32);
		printf("\nsrc utf8 Hex : %s", aHex);
		printf("\nsrc utf32 Hex : %s", bHex);
		printf("\nconv utf8 Hex : %s", cHex);
		printf("\nconv utf32 Hex : %s", dHex);
	#endif
	
	// 补充，测试 utf16 和 utf32 互相转换
	size_t iSize16 = 0, iSize32 = 0;
	u16str su16 = xrtUTF8to16(stru8, 0, NULL);
	u32str sRet32 = xrtUTF16to32(su16, 0, &iSize32);
	u16str sRet16 = xrtUTF32to16(sRet32, 0, &iSize16);
	str fHex = xrtHexEncode(sRet32, iSize32 * 4);
	str eHex = xrtHexEncode(sRet16, iSize16 * 2);
	printf("\nconv utf16 -> utf32 Hex : %s", fHex);
	printf("\nconv utf32 -> utf16 Hex : %s", eHex);
	
	// 猜测编码功能测试
	printf("\n\n---------------- 编码猜测功能\n");
	printf("xrtIsUTF8 (utf8): %d\n", xrtIsUTF8(stru8, 0));
	printf("xrtIsUTF8 (utf16): %d\n", xrtIsUTF8((ptr)su16, 0));
	printf("xrtDetectCharset (utf8): %d\n", xrtDetectCharset(stru8, strlen(stru8), FALSE));
	printf("xrtDetectCharset (utf16): %d\n", xrtDetectCharset(su16, u16len(su16), FALSE));
	printf("xrtDetectCharset (utf32): %d\n", xrtDetectCharset(sRet32, u32len(sRet32), FALSE));
	
	// 任意编码组合转换功能
	printf("\n---------------- 编码组合转换\n");
	u16str cc1 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF16, NULL);
	str ch1 = xrtHexEncode(cc1, u16len(cc1) * 2);
	printf("xrtConvCharset (utf8 to utf16): %s\n", ch1);
	u32str cc3 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF32, NULL);
	str ch3 = xrtHexEncode(cc3, u32len(cc3) * 4);
	printf("xrtConvCharset (utf8 to utf32): %s\n", ch3);
	str cc4 = xrtConvCharset(cc1, u16len(cc1), XRT_CP_UTF16, XRT_CP_UTF8, NULL);
	str ch4 = xrtHexEncode(cc4, strlen(cc4));
	printf("xrtConvCharset (utf16 to utf8): %s\n", ch4);
	u32str cc2 = xrtConvCharset(cc1, u16len(cc1), XRT_CP_UTF16, XRT_CP_UTF32, NULL);
	str ch2 = xrtHexEncode(cc2, u32len(cc2) * 4);
	printf("xrtConvCharset (utf16 to utf32): %s\n", ch2);
	str cc5 = xrtConvCharset(cc2, u32len(cc2), XRT_CP_UTF32, XRT_CP_UTF8, NULL);
	str ch5 = xrtHexEncode(cc5, strlen(cc5));
	printf("xrtConvCharset (utf32 to utf8): %s\n", ch5);
	u16str cc6 = xrtConvCharset(cc2, u32len(cc2), XRT_CP_UTF32, XRT_CP_UTF16, NULL);
	str ch6 = xrtHexEncode(cc6, u16len(cc6) * 2);
	printf("xrtConvCharset (utf32 to utf16): %s\n", ch6);
	
	u16str cc7 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF16_BE, NULL);
	str ch7 = xrtHexEncode(cc7, u16len(cc7) * 2);
	printf("xrtConvCharset (utf8 to utf16_be): %s\n", ch7);
	u32str cc8 = xrtConvCharset(cc7, u16len(cc7), XRT_CP_UTF16_BE, XRT_CP_UTF32_BE, NULL);
	str ch8 = xrtHexEncode(cc8, u32len(cc8) * 4);
	printf("xrtConvCharset (utf16_be to utf32_be): %s\n", ch8);
	u16str cc9 = xrtConvCharset(cc8, u32len(cc8), XRT_CP_UTF32_BE, XRT_CP_UTF16, NULL);
	str ch9 = xrtHexEncode(cc9, u16len(cc9) * 2);
	printf("xrtConvCharset (utf32_be to utf16): %s\n", ch9);
}


