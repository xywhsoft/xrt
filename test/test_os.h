


// OS 库测试
void Test_OS(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n OS 库测试 :\n\n");
	
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		
		ptr pHdr = xrtRun("notepad.exe", 0);
		printf("xrtRun : %p\n", pHdr);
		
		str sPath = xrtPathJoin(3, xCore->AppPath, "test", "测试.txt");
		printf("xrtStart Path : %s\n", sPath);
		pHdr = xrtStart(sPath, 0);
		printf("xrtStart : %p\n", pHdr);
		
		int iExitCode = xrtChain("calc.exe", 0);
		printf("xrtChain : %d\n", iExitCode);
		
	#else
		// 其他平台方案
		
		
		
	#endif
}


