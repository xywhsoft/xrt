


// String 库测试
void Test_String(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n String 库测试 :\n\n");
	printf("xrtLCase : %s\n", xrtLCase("aBcDeFg", 0, FALSE));
	printf("xrtUCase : %s\n", xrtUCase("aBcDeFg", 0, FALSE));
	printf("xrtFindStr : %s\n", xrtFindStr("aBcDeFg", 0, "CDE", 0, TRUE));
	printf("xrtInStr : %d\n", xrtInStr("aBcDeFg", 0, "CDE", 0, TRUE));
	printf("xrtCheckStr : %s\n", xrtCheckStr("xrt?Library", 0, "\\/:*?\"<>|", 0));
	printf("xrtLTrim : %s\n", xrtLTrim("12321abcdefg12321", 0, "123", 0, FALSE, NULL));
	printf("xrtRTrim : %s\n", xrtRTrim("12321abcdefg12321", 0, "123", 0, FALSE, NULL));
	printf("xrtTrim : %s\n", xrtTrim("12321abcdefg12321", 0, "123", 0, FALSE, NULL));
	printf("xrtTrim : |%s|\t(应该返回空字符串)\n", xrtTrim("123212321", 0, "123", 0, FALSE, NULL));
	printf("xrtFilterStr : %s\n", xrtFilterStr("1a2b3c1d2e3f1g2", 0, "123", 0, FALSE, NULL));
	printf("xrtFormat : %s\n", xrtFormat("%s - %s", "Hello", "World ~!"));
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 0, "1", 0, "_", 0, NULL));
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 8, "1", 0, "_", 0, NULL));
	printf("xrtReplace : %s\n", xrtReplace("1a1b1c1d1e1f1g1", 9, "1", 0, "_", 0, NULL));
	
	str sRet = xrtHexEncode("HIJKLMN abcdefg 1234567890", 0, NULL);
	printf("xrtHexEncode : %s\n", sRet);
	printf("xrtHexDecode : %s\n", xrtHexDecode(sRet, 0, NULL));
	
	str sRet2 = xrtBase64Encode("HIJKLMN abcdefg 1234567890", 0, NULL, NULL);
	printf("xrtBase64Encode : %s\n", sRet2, NULL);
	printf("xrtBase64Decode : %s\n", xrtBase64Decode(sRet2, 0, NULL, NULL));
	
	printf("xrtRandStr : %s\n", xrtRandStr(NULL, 0, 32));
	
	size_t iSplitCount = 0;
	str* arrRet = xrtSplit("a1b1c1d1e1f1g", 0, "1", 1, FALSE, &iSplitCount);
	printf("\nxrtSplit : return array len = %zu ( return ptr : %p )", iSplitCount, arrRet);
	for ( int i = 0; i < iSplitCount; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet[i], arrRet[i]);
	}
	
	str* arrRet2 = xrtSplit("a123b1c123d1e123f1g", 0, "123", 3, FALSE, &iSplitCount);
	printf("\n\nxrtSplit : return array len = %zu ( return ptr : %p )", iSplitCount, arrRet2);
	for ( int i = 0; i < iSplitCount; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet2[i], arrRet2[i]);
	}
	
	str sTemp = xrtCopyStr("a1b1c1d1e1f1g", 0);
	str* arrRet3 = xrtSplit(sTemp, 0, "1", 1, TRUE, &iSplitCount);
	printf("\n\nxrtSplit : return array len = %zu ( return ptr : %p )", iSplitCount, arrRet3);
	for ( int i = 0; i < iSplitCount; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet3[i], arrRet3[i]);
	}
	
	str sTemp2 = xrtCopyStr("a123b1c123d1e123f1g", 0);
	str* arrRet4 = xrtSplit(sTemp2, 0, "123", 3, TRUE, &iSplitCount);
	printf("\n\nxrtSplit : return array len = %zu ( return ptr : %p )", iSplitCount, arrRet4);
	for ( int i = 0; i < iSplitCount; i++ ) {
		printf("\n\t%d\t%p\t%s", i + 1, arrRet4[i], arrRet4[i]);
	}
}


