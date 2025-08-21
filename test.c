


#include "xrt.h"
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif





// 扫描文件回调函数
int FileScanProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	if ( bDir == 1 ) {
		printf("\tdir+ : %s\n", sPath);
	} else if ( bDir == 2 ) {
		printf("\tdir- : %s\n", sPath);
	} else {
		printf("\tfile : %s\n", sPath);
	}
	return FALSE;
}
int FileScanProcW(wstr sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	if ( bDir == 1 ) {
		printf("\tdir+ : %S\n", sPath);
	} else if ( bDir == 2 ) {
		printf("\tdir- : %S\n", sPath);
	} else {
		printf("\tfile : %S\n", sPath);
	}
	return FALSE;
}





int main(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		SetConsoleOutputCP(65001);
	#endif
	
	xrtGlobalData* xCore = xrtInit();
	xCore->DebugMode = TRUE;
	printf("测试开始\n\n");
	
	
	
	/* Base 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\n Base 库测试 :\n\n");
	printf("AppFile : %s\n", xCore->AppFile);
	printf("AppPath : %s\n", xCore->AppPath);
	//*/
	
	//printf("%s\n", Path_GetRelA("c:\\123\\1.txt", "c:\\123"));
	
	
	
	/* Charset 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\n Charset 库测试（windows 测 utf16，linux 测 utf32） :\n\n");
	
	str stru8 = "𠀀𫝑😀�";
	
	#if defined(_WIN32) || defined(_WIN64)
		wstr stru16 = L"𠀀𫝑😀�";
		
		str sConv8 = xrtUTF16to8(stru16, 0);
		wstr sConv16 = xrtUTF8to16(stru8, 0);
		
		str aHex = xrtHexEncode(stru8, 0);
		wstr bHex = xrtHexEncodeW(stru16, 0);
		str cHex = xrtHexEncode(sConv8, 0);
		wstr dHex = xrtHexEncodeW(sConv16, 0);
		
		printf("src utf8 : %s", stru8);
		printf("\nsrc utf16 : %S", stru16);
		printf("\nconv utf8 : %s", sConv8);
		printf("\nconv utf16 : %S", sConv16);
		printf("\nsrc utf8 Hex : %s", aHex);
		printf("\nsrc utf16 Hex : %S", bHex);
		printf("\nconv utf8 Hex : %s", cHex);
		printf("\nconv utf16 Hex : %S", dHex);
	#else
		wstr stru32 = L"𠀀𫝑😀�";
		
		str sConv8 = xrtUTF32to8(stru32, 0);
		wstr sConv32 = xrtUTF8to32(stru8, 0);
		
		str aHex = xrtHexEncode(stru8, 0);
		wstr bHex = xrtHexEncodeW(stru32, 0);
		str cHex = xrtHexEncode(sConv8, 0);
		wstr dHex = xrtHexEncodeW(sConv32, 0);
		
		printf("src utf8 : %s", stru8);
		printf("\nsrc utf32 : %S", stru32);
		printf("\nconv utf8 : %s", sConv8);
		printf("\nconv utf32 : %S", sConv32);
		printf("\nsrc utf8 Hex : %s", aHex);
		printf("\nsrc utf32 Hex : %S", bHex);
		printf("\nconv utf8 Hex : %s", cHex);
		printf("\nconv utf32 Hex : %S", dHex);
	#endif
	
	// 补充，测试 utf16 和 utf32 互相转换
	u16str su16 = xrtUTF8to16(stru8, 0);
	u32str sRet32 = xrtUTF16to32(su16, 0);
	int iSize32 = xCore->iRet;
	u16str sRet16 = xrtUTF32to16(sRet32, 0);
	int iSize16 = xCore->iRet;
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
	u16str cc1 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF16);
	str ch1 = xrtHexEncode(cc1, u16len(cc1) * 2);
	printf("xrtConvCharset (utf8 to utf16): %s\n", ch1);
	u32str cc3 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF32);
	str ch3 = xrtHexEncode(cc3, u32len(cc3) * 4);
	printf("xrtConvCharset (utf8 to utf32): %s\n", ch3);
	str cc4 = xrtConvCharset(cc1, u16len(cc1), XRT_CP_UTF16, XRT_CP_UTF8);
	str ch4 = xrtHexEncode(cc4, strlen(cc4));
	printf("xrtConvCharset (utf16 to utf8): %s\n", ch4);
	u32str cc2 = xrtConvCharset(cc1, u16len(cc1), XRT_CP_UTF16, XRT_CP_UTF32);
	str ch2 = xrtHexEncode(cc2, u32len(cc2) * 4);
	printf("xrtConvCharset (utf16 to utf32): %s\n", ch2);
	str cc5 = xrtConvCharset(cc2, u32len(cc2), XRT_CP_UTF32, XRT_CP_UTF8);
	str ch5 = xrtHexEncode(cc5, strlen(cc5));
	printf("xrtConvCharset (utf32 to utf8): %s\n", ch5);
	u16str cc6 = xrtConvCharset(cc2, u32len(cc2), XRT_CP_UTF32, XRT_CP_UTF16);
	str ch6 = xrtHexEncode(cc6, u16len(cc6) * 2);
	printf("xrtConvCharset (utf32 to utf16): %s\n", ch6);
	
	u16str cc7 = xrtConvCharset(stru8, strlen(stru8), XRT_CP_UTF8, XRT_CP_UTF16_BE);
	str ch7 = xrtHexEncode(cc7, u16len(cc7) * 2);
	printf("xrtConvCharset (utf8 to utf16_be): %s\n", ch7);
	u32str cc8 = xrtConvCharset(cc7, u16len(cc7), XRT_CP_UTF16_BE, XRT_CP_UTF32_BE);
	str ch8 = xrtHexEncode(cc8, u32len(cc8) * 4);
	printf("xrtConvCharset (utf16_be to utf32_be): %s\n", ch8);
	u16str cc9 = xrtConvCharset(cc8, u32len(cc8), XRT_CP_UTF32_BE, XRT_CP_UTF16);
	str ch9 = xrtHexEncode(cc9, u16len(cc9) * 2);
	printf("xrtConvCharset (utf32_be to utf16): %s\n", ch9);
	
	//*/
	
	
	
	/* Math 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\n Math 库测试 :\n\n");
	xrtSetRandSeed32(xrtNow(), 0xda3e39cb94b95bdbULL);
	for ( int i = 0; i < 10; i++ ) {
		printf("Rand 0 - 100 : %d\n", xrtRandRange(0, 100));
	}
	//*/
	
	
	
	/* String 库测试 */
	
	/*
	printf("\n\n\n------------------------------------\n\n String 库测试 :\n\n");
	printf("xrtLCase : %s\n", xrtLCase("aBcDeFg", 0, FALSE));
	printf("xrtLCaseW : %S\n", xrtLCaseW(L"aBcDeFg", 0, FALSE));
	printf("xrtUCase : %s\n", xrtUCase("aBcDeFg", 0, FALSE));
	printf("xrtUCaseW : %S\n", xrtUCaseW(L"aBcDeFg", 0, FALSE));
	printf("xrtFindStr : %s\n", xrtFindStr("aBcDeFg", 0, "CDE", 0, TRUE));
	printf("xrtInStr : %d\n", xrtInStr("aBcDeFg", 0, "CDE", 0, TRUE));
	printf("xrtFindStrW : %S\n", xrtFindStrW(L"aBcDeFg", 0, L"CDE", 0, TRUE));
	printf("xrtInStrW : %d\n", xrtInStrW(L"aBcDeFg", 0, L"CDE", 0, TRUE));
	printf("xrtCheckStr : %s\n", xrtCheckStr("xrt?Library", 0, "\\/:*?\"<>|", 0));
	printf("xrtCheckStrW : %S\n", xrtCheckStrW(L"xrt?Library", 0, L"\\/:*?\"<>|", 0));
	printf("xrtLTrim : %s\n", xrtLTrim("12321abcdefg12321", 0, "123", 0, FALSE));
	printf("xrtRTrim : %s\n", xrtRTrim("12321abcdefg12321", 0, "123", 0, FALSE));
	printf("xrtTrim : %s\n", xrtTrim("12321abcdefg12321", 0, "123", 0, FALSE));
	printf("xrtTrim : |%s|\t(应该返回空字符串)\n", xrtTrim("123212321", 0, "123", 0, FALSE));
	printf("xrtLTrimW : %S\n", xrtLTrimW(L"12321abcdefg12321", 0, L"123", 0, FALSE));
	printf("xrtRTrimW : %S\n", xrtRTrimW(L"12321abcdefg12321", 0, L"123", 0, FALSE));
	printf("xrtTrimW : %S\n", xrtTrimW(L"12321abcdefg12321", 0, L"123", 0, FALSE));
	printf("xrtTrimW : |%S|\t(应该返回空字符串)\n", xrtTrimW(L"123212321", 0, L"123", 0, FALSE));
	printf("xrtFilterStr : %s\n", xrtFilterStr("1a2b3c1d2e3f1g2", 0, "123", 0, FALSE));
	printf("xrtFilterStrW : %S\n", xrtFilterStrW(L"1a2b3c1d2e3f1g2", 0, L"123", 0, FALSE));
	printf("xrtFormat : %s\n", xrtFormat("%s - %s", "Hello", "World ~!"));
	#if defined(_WIN32) || defined(_WIN64)
		printf("xrtFormatW : %S\n", xrtFormatW(L"%s - %s", L"Hello", L"World ~!"));
	#else
		printf("xrtFormatW : %S\n", xrtFormatW(L"%S - %S", L"Hello", L"World ~!"));
	#endif
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 0, "1", 0, "_", 0));
	printf("xrtReplaceW : %S\n", xrtReplaceW(L"1a1b1c1d1e1f1g1", 0, L"1", 0, L"_", 0));
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 8, "1", 0, "_", 0));
	printf("xrtReplaceW : %S\n", xrtReplaceW(L"1a1b1c1d1e1f1g1", 8, L"1", 0, L"_", 0));
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 9, "1", 0, "_", 0));
	printf("xrtReplaceW : %S\n", xrtReplaceW(L"1a1b1c1d1e1f1g1", 9, L"1", 0, L"_", 0));
	
	str sRet = xrtHexEncode("HIJKLMN abcdefg 1234567890", 0);
	printf("xrtHexEncode : %s\n", sRet);
	printf("xrtHexDecode : %s\n", xrtHexDecode(sRet, 0));
	wstr sRetW = xrtHexEncodeW(L"HIJKLMN abcdefg 1234567890", 0);
	printf("xrtHexEncodeW : %S\n", sRetW);
	printf("xrtHexDecodeW : %S\n", xrtHexDecodeW(sRetW, 0));
	
	str sRet2 = xrtBase64Encode("HIJKLMN abcdefg 1234567890", 0);
	printf("xrtBase64Encode : %s\n", sRet2);
	printf("xrtBase64Decode : %s\n", xrtBase64Decode(sRet2, 0));
	wstr sRet2W = xrtBase64EncodeW(L"HIJKLMN abcdefg 1234567890", 0);
	printf("xrtBase64EncodeW : %S\n", sRet2W);
	printf("xrtBase64DecodeW : %S\n", xrtBase64DecodeW(sRet2W, 0));
	
	printf("xrtRandStr : %s\n", xrtRandStr(NULL, 0, 32));
	printf("xrtRandStrW : %S\n", xrtRandStrW(NULL, 0, 32));
	
	str* arrRet = xrtSplit("a1b1c1d1e1f1g", 0, "1", 1, FALSE);
	printf("\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet[i], arrRet[i]);
	}
	
	str* arrRet2 = xrtSplit("a123b1c123d1e123f1g", 0, "123", 3, FALSE);
	printf("\n\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet2);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet2[i], arrRet2[i]);
	}
	
	str sTemp = xrtCopyStr("a1b1c1d1e1f1g", 0);
	str* arrRet3 = xrtSplit(sTemp, 0, "1", 1, TRUE);
	printf("\n\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet3);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet3[i], arrRet3[i]);
	}
	
	str sTemp2 = xrtCopyStr("a123b1c123d1e123f1g", 0);
	str* arrRet4 = xrtSplit(sTemp2, 0, "123", 3, TRUE);
	printf("\n\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet4);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet4[i], arrRet4[i]);
	}
	
	wstr* arrRet5 = xrtSplitW(L"a1b1c1d1e1f1g", 0, L"1", 1, FALSE);
	printf("\n\nxrtSplitW : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet5);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%S", i + 1, arrRet5[i], arrRet5[i]);
	}
	
	wstr* arrRet6 = xrtSplitW(L"a123b1c123d1e123f1g", 0, L"123", 3, FALSE);
	printf("\n\nxrtSplitW : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet6);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%S", i + 1, arrRet6[i], arrRet6[i]);
	}
	
	wstr sTemp3 = xrtCopyStrW(L"a1b1c1d1e1f1g", 0);
	wstr* arrRet7 = xrtSplitW(sTemp3, 0, L"1", 1, TRUE);
	printf("\n\nxrtSplitW : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet7);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%S", i + 1, arrRet7[i], arrRet7[i]);
	}
	
	wstr sTemp4 = xrtCopyStrW(L"a123b1c123d1e123f1g", 0);
	wstr* arrRet8 = xrtSplitW(sTemp4, 0, L"123", 3, TRUE);
	printf("\n\nxrtSplitW : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet8);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%S", i + 1, arrRet8[i], arrRet8[i]);
	}
	//*/
	
	
	
	/* Path 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\n Path 库测试 :\n\n");
	printf("xrtPathGetNameExt : %s\n", xrtPathGetNameExt("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetNameExtW : %S\n", xrtPathGetNameExtW(L"c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetName : %s\n", xrtPathGetName("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetNameW : %S\n", xrtPathGetNameW(L"c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetExt : %s\n", xrtPathGetExt("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetExtW : %S\n", xrtPathGetExtW(L"c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetDir : %s\n", xrtPathGetDir("c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetDirW : %S\n", xrtPathGetDirW(L"c:\\123\\456\\789\\file.ext", 0));
	printf("xrtPathGetDir : %s\n", xrtPathGetDir("c:\\123\\456\\789\\", 0));
	printf("xrtPathGetDirW : %S\n", xrtPathGetDirW(L"c:\\123\\456\\789\\", 0));
	printf("xrtPathIsAbs : %d\n", xrtPathIsAbs("c:\\123\\456\\789\\", 0));
	printf("xrtPathIsAbsW : %d\n", xrtPathIsAbsW(L"c:\\123\\456\\789\\", 0));
	printf("xrtPathRandom : %s\n", xrtPathRandom("c:\\123\\456\\789\\Rand_", 0, ".jpg", 4, 32));
	printf("xrtPathRandomW : %S\n", xrtPathRandomW(L"c:\\123\\456\\789\\Rand_", 0, L".jpg", 4, 32));
	printf("xrtPathJoin : %s\n", xrtPathJoin(4, "c:\\123", "456", "789", "file.ext"));
	printf("xrtPathJoinW : %S\n", xrtPathJoinW(4, L"c:\\123", L"456", L"789", L"file.ext"));
	//*/
	
	
	
	/* Time 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\n Time 库测试 :\n\n");
	for ( int i = 1; i <= 12; i++ ) {
		printf("xrtDateSerial (0-%02d-01 00:00:00) : %d\n", i, xrtDateSerial(0, i, 1));
	}
	printf("xrtDateSerial (-5-01-01 00:00:00) : %d\n", xrtDateSerial(-5, 1, 1));
	printf("xrtDateSerial (-2-01-01 00:00:00) : %d\n", xrtDateSerial(-2, 1, 1));
	printf("xrtDateSerial (-1-01-01 00:00:00) : %d\n", xrtDateSerial(-1, 1, 1));
	printf("xrtDateSerial (0-01-01 00:00:00) : %d\n", xrtDateSerial(0, 1, 1));
	printf("xrtDateSerial (1-01-01 00:00:00) : %d\n", xrtDateSerial(1, 1, 1));
	printf("xrtDateSerial (2-01-01 00:00:00) : %d\n", xrtDateSerial(2, 1, 1));
	printf("xrtDateSerial (5-01-01 00:00:00) : %d\n", xrtDateSerial(5, 1, 1));
	printf("xrtDateSerial (1970-01-01 00:00:00) : %lld\n", xrtDateSerial(1970, 1, 1));
	printf("xrtTimeSerial (12:00:00) : %d\n", xrtTimeSerial(12, 0, 0));
	xtime iTime = xrtDateTimeSerial(2012, 3, 15, 12, 20, 40);
	printf("xrtDateTimeSerial (2012-03-15 12:20:40) : %lld\n", iTime);
	printf("xrtYear (2012-03-15 12:20:40) : %d\n", xrtYear(iTime));
	printf("xrtMonth (2012-03-15 12:20:40) : %d\n", xrtMonth(iTime));
	printf("xrtDay (2012-03-15 12:20:40) : %d\n", xrtDay(iTime));
	printf("xrtHour (2012-03-15 12:20:40) : %d\n", xrtHour(iTime));
	printf("xrtMinute (2012-03-15 12:20:40) : %d\n", xrtMinute(iTime));
	printf("xrtSecond (2012-03-15 12:20:40) : %d\n", xrtSecond(iTime));
	printf("xrtWeekday (2012-03-15 12:20:40) : %d\n", xrtWeekday(iTime));
	printf("xrtDayOfYear (2012-03-15 12:20:40) : %d\n", xrtDayOfYear(iTime));
	int64 iYear;
	int iMonth, iDay, iHour, iMinute, iSecond, iWeekday, iDayOfYear;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, &iDayOfYear);
	printf("xrtDecodeSerial : %d-%d-%d %d:%d:%d ( %d - %d )\n", iYear, iMonth, iDay, iHour, iMinute, iSecond, iWeekday, iDayOfYear);
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME));
	printf("xrtTimeToStrW : %S\n", xrtTimeToStrW(iTime, XRT_TIME_FORMAT_DATETIME));
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATE));
	printf("xrtTimeToStrW : %S\n", xrtTimeToStrW(iTime, XRT_TIME_FORMAT_DATE));
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_TIME));
	printf("xrtTimeToStrW : %S\n", xrtTimeToStrW(iTime, XRT_TIME_FORMAT_TIME));
	
	xtime tRet = xrtDateAdd(XRT_TIME_INTERVAL_SECOND, 30, iTime);
	int64 iDiff = xrtDateDiff(XRT_TIME_INTERVAL_SECOND, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 sec : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 sec : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_MINUTE, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 min : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 min : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_HOUR, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 hour : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 hour : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_DAY, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 day : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 day : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 mon : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 mon : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 year : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 year : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_DAY, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 week : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 week : %d (day)\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 quar : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 quar : %d\n", iDiff);
	
	double st = xrtTimer();
	printf("xrtTimer Start : %f\n", st);
	xrtSleep(1000);
	double et = xrtTimer();
	printf("xrtTimer Stop : %f\n", et);
	printf("time diff (s) : %f\n", et - st);
	
	//*/
	
	
	
	/* File 库测试 */
	printf("\n\n\n------------------------------------\n\n File 库测试 :\n\n");
	
	/*
	printf("---------------- 编码自动识别\n\n");
	str sPath = xrtPathJoin(3, xCore->AppPath, "test", "ascii.txt");
	printf("test : %s\n", sPath);
	xfile f_a = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	size_t iFileSize = xrtGetEOF(f_a);
	str sText = xrtRead(f_a, iFileSize - f_a->BOM);
	printf("Charset (65501) : %d\n", f_a->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_a->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "gb2312.txt");
	printf("test : %s\n", sPath);
	xfile f_b = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_b);
	sText = xrtRead(f_b, iFileSize - f_b->BOM);
	printf("Charset (0) : %d\n", f_b->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_b->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf8.txt");
	printf("test : %s\n", sPath);
	xfile f_c = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_c);
	sText = xrtRead(f_c, iFileSize - f_c->BOM);
	printf("Charset (65501) : %d\n", f_c->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_c->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf8_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_d = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_d);
	sText = xrtRead(f_d, iFileSize - f_d->BOM);
	printf("Charset (65501) : %d\n", f_d->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_d->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16.txt");
	printf("test : %s\n", sPath);
	xfile f_e = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_e);
	sText = xrtRead(f_e, iFileSize - f_e->BOM);
	printf("Charset (1200) : %d\n", f_e->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_e->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_be.txt");
	printf("test : %s\n", sPath);
	xfile f_f = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_f);
	sText = xrtRead(f_f, iFileSize - f_f->BOM);
	printf("Charset (1201) : %d\n", f_f->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_f->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_g = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_g);
	sText = xrtRead(f_g, iFileSize - f_g->BOM);
	printf("Charset (1200) : %d\n", f_g->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_g->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_be_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_h = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_h);
	sText = xrtRead(f_h, iFileSize - f_h->BOM);
	printf("Charset (1201) : %d\n", f_h->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_h->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32.txt");
	printf("test : %s\n", sPath);
	xfile f_i = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_i);
	sText = xrtRead(f_i, iFileSize - f_i->BOM);
	printf("Charset (65005) : %d\n", f_i->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_i->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_be.txt");
	printf("test : %s\n", sPath);
	xfile f_j = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_j);
	sText = xrtRead(f_j, iFileSize - f_j->BOM);
	printf("Charset (65006) : %d\n", f_j->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_j->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_k = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_k);
	sText = xrtRead(f_k, iFileSize - f_k->BOM);
	printf("Charset (65005) : %d\n", f_k->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_k->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_be_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_l = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_l);
	sText = xrtRead(f_l, iFileSize - f_l->BOM);
	printf("Charset (65006) : %d\n", f_l->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_l->BOM);
	printf("Text : %s\n\n", sText);
	//*/
	
	/*
	printf("---------------- 文件读取和写入\n");
	str sPath2 = xrtPathJoin(3, xCore->AppPath, "test", "test.txt");
	printf("test : %s\n", sPath2);
	xfile objFile = xrtOpen(sPath2, TRUE, XRT_CP_AUTO);
	str sText2 = xrtRead(objFile, xrtGetEOF(objFile) - objFile->BOM);
	printf("Text : %s\n\n", sText2);
	
	str sPath3 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u16be.txt");
	xrtFileDelete(sPath3);
	xfile objFile2 = xrtOpen(sPath3, FALSE, XRT_CP_UTF16_BE | XRT_CP_BOM);
	xrtWrite(objFile2, sText2, 0);
	xrtWrite(objFile2, "\n1234567", 0);
	xrtClose(objFile2);
	
	str sPath4 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u16.txt");
	xrtFileDelete(sPath4);
	xfile objFile3 = xrtOpen(sPath4, FALSE, XRT_CP_UTF16 | XRT_CP_BOM);
	xrtWrite(objFile3, sText2, 0);
	xrtWrite(objFile3, "\n7654321", 0);
	xrtClose(objFile3);
	
	str sPath5 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u32.txt");
	xrtFileWriteAll(sPath5, "xxrpa.com 清浅池塘边，重生破土的冲动", 0, XRT_CP_UTF32 | XRT_CP_BOM);
	xrtFileAppend(sPath5, "\nxdoc.online 天地正玲珑，殡葬了飞虫", 0, XRT_CP_UTF32 | XRT_CP_BOM);
	str sRetPath5 = xrtFileReadAll(sPath5, XRT_CP_AUTO);
	printf("xrtFileReadAll : %s\n", sRetPath5);
	//*/
	
	/*
	printf("---------------- 文件操作测试\n");
	str sPathA1 = xrtPathJoin(3, xCore->AppPath, "test", "test.txt");
	str sPathA2 = xrtPathJoin(3, xCore->AppPath, "test", "test_copy.txt");
	str sPathA3 = xrtPathJoin(3, xCore->AppPath, "test", "test_movesrc.txt");
	str sPathA4 = xrtPathJoin(3, xCore->AppPath, "test", "test_move.txt");
	str sPathA5 = xrtPathJoin(3, xCore->AppPath, "test", "test_delete.txt");
	str sPathA6 = xrtPathJoin(3, xCore->AppPath, "test", "test222.txt");
	str sPathA7 = xrtPathJoin(3, xCore->AppPath, "test", "test_size.txt");
	str sPathAX = xrtPathJoin(2, xCore->AppPath, "test");
	xrtFileCopy(sPathA1, sPathA2, TRUE);
	xrtFileCopy(sPathA1, sPathA3, TRUE);
	xrtFileCopy(sPathA1, sPathA7, TRUE);
	xrtFileMove(sPathA3, sPathA4, TRUE);
	xrtFileDelete(sPathA5);
	xrtFileSetSize(sPathA7, 10 * 1024 * 1024);
	printf("xrtPathExists : %s = %d\n", sPathA1, xrtPathExists(sPathA1));
	printf("xrtPathExists : %s = %d\n", sPathA6, xrtPathExists(sPathA6));
	printf("xrtPathExists : %s = %d\n", sPathAX, xrtPathExists(sPathAX));
	printf("xrtFileExists : %s = %d\n", sPathA1, xrtFileExists(sPathA1));
	printf("xrtFileExists : %s = %d\n", sPathA6, xrtFileExists(sPathA6));
	printf("xrtFileExists : %s = %d\n", sPathAX, xrtFileExists(sPathAX));
	printf("xrtDirExists : %s = %d\n", sPathA1, xrtDirExists(sPathA1));
	printf("xrtDirExists : %s = %d\n", sPathA6, xrtDirExists(sPathA6));
	printf("xrtDirExists : %s = %d\n", sPathAX, xrtDirExists(sPathAX));
	printf("xrtFileGetSize : %s = %d\n", sPathA1, xrtFileGetSize(sPathA1));
	printf("xrtFileSetSize : %s = 10MB\n", sPathA7);
	printf("xrtFileGetSize : %s = %d\n", sPathA7, xrtFileGetSize(sPathA7));
	printf("xrtFileGetAttr : %s = %d\n", sPathA1, xrtFileGetAttr(sPathA1));
	printf("xrtFileCopy : %s\n", sPathA2);
	printf("xrtFileMove : %s\n", sPathA4);
	printf("xrtFileDelete : %s\n", sPathA5);
	//*/
	
	/*
	printf("---------------- 遍历文件测试\n");
	printf("xrtDirScan : %s\n", xCore->AppPath);
	printf("FileCount : %d\n\n", xrtDirScan(xCore->AppPath, FALSE, FileScanProc, NULL));
	printf("xrtDirScan [Recu] : %s\n", xCore->AppPath);
	printf("FileCount : %d\n\n", xrtDirScan(xCore->AppPath, TRUE, FileScanProc, NULL));
	//*/
	
	/*
	printf("---------------- 目录操作测试\n");
	#if defined(_WIN32) || defined(_WIN64)
		printf("xrtDirCopy : %s -> c:\\123\n", xCore->AppPath);
		printf("FileCount : %d\n\n", xrtDirCopy(xCore->AppPath, "c:\\123", FALSE));
		system("pause");
		printf("xrtDirMove : c:\\123 -> c:\\456\n");
		printf("FileCount : %d\n\n", xrtDirMove("c:\\123", "c:\\456", TRUE));
		system("pause");
		printf("xrtDirDelete : c:\\456\n");
		printf("FileCount : %d\n\n", xrtDirDelete("c:\\456"));
	#else
		printf("xrtDirCopy : %s -> /home/123\n", xCore->AppPath);
		printf("FileCount : %d\n\n", xrtDirCopy(xCore->AppPath, "/home/123", FALSE));
		printf("Press any key to continue...\n");
		getchar();
		printf("xrtDirMove : /home/123 -> /home/456\n");
		printf("FileCount : %d\n\n", xrtDirMove("/home/123", "/home/456", TRUE));
		printf("Press any key to continue...\n");
		getchar();
		printf("xrtDirDelete : /home/456\n");
		printf("FileCount : %d\n\n", xrtDirDelete("/home/456"));
	#endif
	//*/
	
	
	
	/* XID 库测试 */
	//*
	printf("\n\n\n------------------------------------\n\n XID 库测试 :\n\n");
	xid xida = xrtMakeXID(xrtRand32(), xrtRand32());
	printf("xrtMakeXID - Data : %d\n", xida->Data);
	printf("xrtMakeXID - Tick : %d\n", xida->Tick);
	printf("xrtMakeXID - Time : %lld\n", xida->Time);
	printf("xrtMakeXID - Addr : %d\n", xida->Addr);
	printf("xrtMakeXID - Rand : %d\n", xida->Rand);
	str sXID = xrtEncodeXID(xida);
	printf("xrtEncodeXID : %s\n", sXID);
	xida = xrtDecodeXID(sXID);
	printf("xrtDecodeXID - Data : %d\n", xida->Data);
	printf("xrtDecodeXID - Tick : %d\n", xida->Tick);
	printf("xrtDecodeXID - Time : %lld\n", xida->Time);
	printf("xrtDecodeXID - Addr : %d\n", xida->Addr);
	printf("xrtDecodeXID - Rand : %d\n", xida->Rand);
	//*/
	
	
	
	/* 自定义测试 */
	//*
	printf("\n\n\n------------------------------------\n\n 自定义测试 :\n\n");
	
	printf("xvalue_struct : %d\n", sizeof(xvalue_struct));
	printf("xvalue12_struct : %d\n", sizeof(xvalue12_struct));
	printf("xcustom_struct : %d\n", sizeof(xcustom_struct));
	
	//*/
	
	
	
	printf("\n------------------------------------\n\n\n\n测试结束\n\n");
	xrtUnit();
	return 0;
}


