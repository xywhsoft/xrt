


// 申请内存
XXAPI ptr xrtMalloc(size_t iSize)
{
	return xCore.malloc(iSize);
}



// 申请类内存
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	return xCore.calloc(iNum, iSize);
}



// 重新申请内存
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	return xCore.realloc(pMem, iSize);
}



// 释放内存（ 会先判断是否为 null ）
XXAPI void xrtFree(ptr pmem)
{
	if ( pmem && (pmem != xCore.sNull) ) { xCore.free(pmem); }
}



// 设置错误
XXAPI void xrtSetError(str sError, int bFree)
{
	#ifdef DebugMode
		#if defined(_WIN32) || defined(_WIN64)
			printf("SetError : %S\n", sError);
		#else
			printf("SetError : %s\n", sError);
		#endif
	#endif
	if ( xCore.__pri_FreeError && xCore.LastError ) {
		xrtFree(xCore.LastError);
	}
	xCore.LastError = sError;
	xCore.__pri_FreeError = bFree;
}



// 清除错误
XXAPI void xrtClearError()
{
	if ( xCore.__pri_FreeError && xCore.LastError ) {
		xrtFree(xCore.LastError);
	}
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
}



/*
// 运行程序
XXAPI HANDLE xCore_RunW(wstr sPath, int iShow)
{
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	GetStartupInfoW(&si);
	si.wShowWindow = iShow;
	CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	return pi.hProcess;
}
XXAPI HANDLE xCore_RunA(astr sPath, int iShow)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	GetStartupInfoA(&si);
	si.wShowWindow = iShow;
	CreateProcessA(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	return pi.hProcess;
}
XXAPI HANDLE xCore_RunU(ustr sPath, int iShow)
{
	wstr sPathW = xCore_U2W(sPath, 0);
	HANDLE hRet = xCore_RunW(sPathW, iShow);
	free(sPathW);
	return hRet;
}



// 打开文件
XXAPI HANDLE xCore_StartW(wstr sPath, int iShow)
{
	HANDLE hRet = ShellExecuteW(0, NULL, sPath, NULL, NULL, iShow);
	return hRet;
}
XXAPI HANDLE xCore_StartA(astr sPath, int iShow)
{
	HANDLE hRet = ShellExecuteA(0, NULL, sPath, NULL, NULL, iShow);
	return hRet;
}
XXAPI HANDLE xCore_StartU(ustr sPath, int iShow)
{
	wstr sPathW = xCore_U2W(sPath, 0);
	HANDLE hRet = xCore_StartW(sPathW, iShow);
	free(sPathW);
	return hRet;
}



// 运行程序并等待程序运行结束
XXAPI int xCore_ChainW(wstr sPath, int iShow)
{
	DWORD iRet = 0;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	GetStartupInfoW(&si);
	si.wShowWindow = iShow;
	CreateProcessW(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &iRet);
	return iRet;
}
XXAPI int xCore_ChainA(astr sPath, int iShow)
{
	DWORD iRet = 0;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	GetStartupInfoA(&si);
	si.wShowWindow = iShow;
	CreateProcessA(NULL, sPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &iRet);
	return iRet;
}
XXAPI int xCore_ChainU(ustr sPath, int iShow)
{
	wstr sPathW = xCore_U2W(sPath, 0);
	int iRet = xCore_ChainW(sPathW, iShow);
	free(sPathW);
	return iRet;
}



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


