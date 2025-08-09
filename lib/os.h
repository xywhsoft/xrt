


// 运行程序
XXAPI ptr xrtRunW(wstr sPath, int iShow)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = iShow;
		CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		return (ptr)pi.hProcess;
	#else
		return NULL;
	#endif
}



// 打开文件（ 仅支持 windows 系统 ）
XXAPI ptr xrtStartW(wstr sPath, int iShow)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (ptr)ShellExecuteW(0, NULL, sPath, NULL, NULL, iShow);
	#else
		return NULL;
	#endif
}



// 运行程序并等待程序运行结束
XXAPI int xrtChainW(wstr sPath, int iShow)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = iShow;
		CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &iRet);
		return iRet;
	#else
		return 0;
	#endif
}


