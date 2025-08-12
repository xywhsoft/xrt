


// 运行程序
XXAPI ptr xrtRun(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		return (ptr)pi.hProcess;
	#else
		return NULL;
	#endif
}
XXAPI ptr xrtRunW(wstr sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		return (ptr)pi.hProcess;
	#else
		return NULL;
	#endif
}



// 打开文件（ 仅支持 windows 系统 ）
XXAPI ptr xrtStart(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		return (ptr)ShellExecuteW(0, NULL, sPathW, NULL, NULL, SW_SHOW);
		xrtFree(sPathW);
	#else
		return NULL;
	#endif
}
XXAPI ptr xrtStartW(wstr sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (ptr)ShellExecuteW(0, NULL, sPath, NULL, NULL, SW_SHOW);
	#else
		return NULL;
	#endif
}



// 运行程序并等待程序运行结束
XXAPI int xrtChain(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &iRet);
		return iRet;
	#else
		return 0;
	#endif
}
XXAPI int xrtChainW(wstr sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &iRet);
		return iRet;
	#else
		return 0;
	#endif
}


