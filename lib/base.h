


/* ------------------------------------ C 函数库 ------------------------------------ */



// 释放内存（ 释放 malloc 申请的内存，会先判断是否为 nullstring ）
XXAPI void xrtFree(ptr pmem)
{
	if ( pmem && (pmem != xCore.nullstring) ) { free(pmem); }
}



// 随机数
XXAPI int xrtRand(int min, int max)
{
	if ( min == max ) {
		return min;
	} else if ( min > max ) {
		return max + (rand() % (min - max));
	} else {
		return min + (rand() % (max - min));
	}
}



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
		return xCore.nullstring;
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
		return xCore.nullstring;
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
		return (wstr)xCore.nullstring;
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


