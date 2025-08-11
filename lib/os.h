


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



/*
// 运行程序，返回控制台输出
XXAPI astr xShellA(astr sCMD)
{
	char buffer[1024];
	FILE *fp = popen(sCMD, "r");
	if ( fp == NULL ) {
		return xCore.sNull;
	}
	MBMU_Object objBuf = MBMU_Create(16384, 16384);
	while ( fgets(buffer, 1024, fp) != NULL ) {
		MBMU_Append(objBuf, buffer, 0, MBMU_UTF8);
	}
	pclose(fp);
	astr sRet = malloc(objBuf->Length + 1);
	memcpy(sRet, objBuf->Buffer, objBuf->Length);
	sRet[objBuf->Length] = 0;
	MBMU_Destroy(objBuf);
	return sRet;
}
XXAPI ustr xShellU(ustr sCMD)
{
	char buffer[1024];
	FILE *fp = popen(sCMD, "r");
	if ( fp == NULL ) {
		return xCore.sNull;
	}
	MBMU_Object objBuf = MBMU_Create(16384, 16384);
	while ( fgets(buffer, 1024, fp) != NULL ) {
		MBMU_Append(objBuf, buffer, 0, MBMU_UTF8);
	}
	pclose(fp);
	ustr sRet = xCore_A2U(objBuf->Buffer, objBuf->Length);
	MBMU_Destroy(objBuf);
	return sRet;
}
XXAPI wstr xShellW(wstr sCMD)
{
	char buffer[1024];
	FILE *fp = wpopen(sCMD, L"r");
	if ( fp == NULL ) {
		return (wstr)xCore.sNull;
	}
	MBMU_Object objBuf = MBMU_Create(16384, 16384);
	while ( fgets(buffer, 1024, fp) != NULL ) {
		MBMU_Append(objBuf, buffer, 0, MBMU_UTF8);
	}
	pclose(fp);
	wstr sRet = xCore_A2W(objBuf->Buffer, objBuf->Length);
	MBMU_Destroy(objBuf);
	return sRet;
}
*/


