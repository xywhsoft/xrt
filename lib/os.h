


// 运行程序
XXAPI ptr xrtRun(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return NULL;
		}
		BOOL bOK = CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		if ( bOK == FALSE ) {
			return NULL;
		}
		if ( pi.hThread ) {
			CloseHandle(pi.hThread);
		}
		return (ptr)pi.hProcess;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}



// 打开文件（ Windows 系统使用 ShellExecute，Linux 系统使用 xdg-open ）
XXAPI ptr xrtStart(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return NULL;
		}
		ptr hRet = (ptr)ShellExecuteW(0, NULL, sPathW, NULL, NULL, SW_SHOW);
		xrtFree(sPathW);
		return hRet;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execlp("xdg-open", "xdg-open", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}



// 运行程序并等待程序运行结束
XXAPI int xrtChain(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return -1;
		}
		BOOL bOK = CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		if ( bOK == FALSE ) {
			return -1;
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
		if ( pi.hProcess ) {
			GetExitCodeProcess(pi.hProcess, &iRet);
			CloseHandle(pi.hProcess);
		}
		if ( pi.hThread ) {
			CloseHandle(pi.hThread);
		}
		return (int)iRet;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			int status;
			waitpid(pid, &status, 0);
			if ( WIFEXITED(status) ) {
				return WEXITSTATUS(status);
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	#endif
}
