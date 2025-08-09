


#include "xrt.h"
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif





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
	printf("\n\n\n------------------------------------\n\nBase 库测试 :\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		printf("AppFile : %S\n", xCore->AppFile);
		printf("AppPath : %S\n", xCore->AppPath);
	#else
		printf("AppFile : %s\n", xCore->AppFile);
		printf("AppPath : %s\n", xCore->AppPath);
	#endif
	//*/
	
	//printf("%s\n", Path_GetRelA("c:\\123\\1.txt", "c:\\123"));
	
	
	
	/* Math 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\nMath 库测试 :\n\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("Rand 0 - 100 : %d\n", xrtRand(0, 100));
	}
	//*/
	
	
	
	/* String 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\nString 库测试 :\n\n");
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
	
	ustr sRet = xrtHexEncode("HIJKLMN abcdefg 1234567890", 0);
	printf("xrtHexEncode : %s\n", sRet);
	printf("xrtHexDecode : %s\n", xrtHexDecode(sRet, 0));
	wstr sRetW = xrtHexEncodeW(L"HIJKLMN abcdefg 1234567890", 0);
	printf("xrtHexEncodeW : %s\n", sRetW);
	printf("xrtHexDecodeW : %s\n", xrtHexDecodeW(sRetW, 0));
	
	printf("xrtRandStr : %s\n", xrtRandStr(NULL, 0, 32));
	printf("xrtRandStrW : %S\n", xrtRandStrW(NULL, 0, 32));
	
	ustr* arrRet = xrtSplit("a1b1c1d1e1f1g", 0, "1", 1, FALSE);
	printf("\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet[i], arrRet[i]);
	}
	
	ustr* arrRet2 = xrtSplit("a123b1c123d1e123f1g", 0, "123", 3, FALSE);
	printf("\n\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet2);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet2[i], arrRet2[i]);
	}
	
	ustr sTemp = xrtCopyStr("a1b1c1d1e1f1g", 0);
	ustr* arrRet3 = xrtSplit(sTemp, 0, "1", 1, TRUE);
	printf("\n\nxrtSplit : return array len = %d ( return ptr : %p )", xCore->iRet, arrRet3);
	for ( int i = 0; i <= xCore->iRet; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet3[i], arrRet3[i]);
	}
	
	ustr sTemp2 = xrtCopyStr("a123b1c123d1e123f1g", 0);
	ustr* arrRet4 = xrtSplit(sTemp2, 0, "123", 3, TRUE);
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
	printf("\n\n\n------------------------------------\n\nPath 库测试 :\n\n");
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
	printf("xrtPathJoin : %s\n", xrtPathJoin(4, "c:\\123", 0, "456", 0, "789", 0, "file.ext", 0));
	printf("xrtPathJoinW : %S\n", xrtPathJoinW(4, L"c:\\123", 0, L"456", 0, L"789", 0, L"file.ext", 0));
	//*/
	
	
	
	/* Time 库测试 */
	//*
	printf("\n\n\n------------------------------------\n\nPath 库测试 :\n\n");
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
	printf("xrtMinute (2012-03-15 12:20:40) : %d\n", xrtMinute(iTime));
	printf("xrtHour (2012-03-15 12:20:40) : %d\n", xrtHour(iTime));
	printf("xrtSecond (2012-03-15 12:20:40) : %d\n", xrtSecond(iTime));
	printf("xrtWeekday (2012-03-15 12:20:40) : %d\n", xrtWeekday(iTime));
	
	//*/
	
	
	
	printf("\n------------------------------------\n\n\n\n测试结束\n\n");
	xrtUnit();
	return 0;
}


