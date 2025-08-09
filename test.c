


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
	wstr sRetW = xrtHexEncodeW(L"HIJKLMN abcdefg 1234567890", 0);
	printf("xrtHexEncodeW : %S\n", sRetW);
	printf("xrtHexDecodeW : %S\n", xrtHexDecodeW(sRetW, 0));
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
	//*
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
	/*
	printf("%s\n", Path_GetAbsA("file.ext", xCore_AppPathA()));
	printf("%s\n", Path_GetRelA(xCore_AppFileA(), xCore_AppPathA()));
	printf("%s\n", Path_RandomFileA("c:\\123\\456\\789\\", ".ext", 8));
	char sPath[] = "c:\\123\\456\\789.\\.file.ext ";
	printf("%d\n", Path_SafeCheckA(sPath, TRUE));
	printf("%s\n", sPath);
	char sPath2[] = "c:\\123\\456\\789\\file.ext";
	printf("%d\n", Path_SafeCheckA(sPath2, TRUE));
	printf("%s\n", sPath2);
	printf("%s\n", Path_JoinA("c:\\123\\456\\789", "file.ext"));
	printf("%s\n", Path_JoinA(NULL, "file.ext"));
	//*/
	
	/* Dialog 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\nDialog 库测试 :\n\n");
	//printf("%s\n", xCore_W2A(xxInputBoxW(0, L"输入一个数字：", L"请输入", L"默认值", -1, -1, 0), 0));
	//printf("%s\n", xInputBoxA(0, "输入一个数字：", "请输入", "默认值", -1, -1, 0));
	//printf("%s\n", xOpenFileDialogA(0, NULL, NULL, NULL, 0));
	//printf("%s\n", xOpenFileDialogA(0, "c:\\*.txt", "文本文档|*.txt|所有文件|*", "标题", 0));
	//printf("%s\n", xSaveFileDialogA(0, "c:\\*.txt", "文本文档|*.txt|所有文件|*", "标题", 0));
	printf("%s\n", xSelectFolderDialogA(0, "c:\\windows", "标题"));
	//*/
	
	/* xTable 库测试 */
	/*
	printf("\n\n\n------------------------------------\n\nxTable 库测试 :\n\n");
	
	xTableObject tblTest = xTable_Create();
	printf("Create xTable Object : %d\n", tblTest);
	
	pTableNode objNode;
	
	objNode = xTable_InsertA(tblTest, "xTable", "xTable 库测试");
	printf("Insert Node Object : %d\n", objNode);
	
	objNode = xTable_InsertA(tblTest, "String", "String 库测试");
	printf("Insert Node Object : %d\n", objNode);
	
	objNode = xTable_InsertA(tblTest, "Path", "Path 库测试");
	printf("Insert Node Object : %d\n", objNode);
	
	objNode = xTable_InsertA(tblTest, "Dialog", "Dialog 库测试");
	printf("Insert Node Object : %d\n", objNode);
	
	printf("\n查找 Key = String 的 Node ：\n");
	objNode = xTable_SearchA(tblTest, "String");
	printf("Scan Node : %d	%d	%s	%s\n", objNode, objNode->Hash, objNode->Key, objNode->Val);
	
	printf("\n删除 Key = Path 的 Node ：\n");
	xTable_RemoveA(tblTest, "Path");
	
	printf("\nTable 遍历测试：\n");
	xTable_Scan(tblTest, ProcScanTable);
	
	xTable_Destroy(tblTest);
	//*/
	
	
	
	printf("\n------------------------------------\n\n\n\n测试结束\n\n");
	xrtUnit();
	return 0;
}


