


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
		
		printf("测试 xrtRun 函数:\n");
		ptr process = xrtRun("echo 'Hello from xrtRun!'", 0);
		if (process) {
			printf("xrtRun 成功启动进程: %p\n", process);
		} else {
			printf("xrtRun 启动进程失败\n");
		}
		
		// 测试 xrtChain 函数
		printf("\n测试 xrtChain 函数:\n");
		int result = xrtChain("echo 'Hello from xrtChain!'", 0);
		printf("xrtChain 执行结果: %d\n", result);
		
		// 测试 xrtStart 函数 (仅在桌面环境中有效)
		printf("\n测试 xrtStart 函数:\n");
		ptr start_result = xrtStart("/etc", 0);
		if (start_result) {
			printf("xrtStart 成功: %p\n", start_result);
		} else {
			printf("xrtStart 失败或不可用\n");
		}
		
	#endif
}


