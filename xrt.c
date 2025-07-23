


#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#include "windows.h"
#else
#endif



#include "lib/base.h"



// 初始化 xCore
xCoreStruct xCore;
XXAPI void xCoreInit(void)
{
	
	// 初始化数据
	xCore.nullstring = (str)"\0\0\0";
	xCore.sRet = xCore.nullstring;
	xCore.iRet = 0;
	xCore.dRet = 0.0;
	xCore.LastErrorID = 0;
	xCore.LastError = xCore.nullstring;
	
	// 获取程序文件名和路径
	#if defined(_WIN32) || defined(_WIN64)
		astr sTemp = malloc(1024);
		int iSize = GetModuleFileNameA(NULL, sTemp, 1024);
		xCore.AppFile = malloc(iSize + 1);
		memcpy(xCore.AppFile, sTemp, iSize);
		xCore.AppFile[iSize] = 0;
		xrtFree(sTemp);
		sTemp = strrchr(xCore.AppFile, L'\\');
		iSize = (sTemp - xCore.AppFile);
		xCore.AppPath = malloc(iSize + 1);
		memcpy(xCore.AppPath, xCore.AppFile, iSize);
		xCore.AppPath[iSize] = 0;
	#else
		astr sTemp = malloc(1024);
		size_t iSize = readlink("/proc/self/exe", sTemp, 1024);
		if ( iSize == -1 ) {
			// 无法读取程序路径
			xrtFree(sTemp);
			xCore.AppFile = xCore.nullstring;
			xCore.AppPath = xCore.nullstring;
		} else {
			xCore.AppFile = malloc(iSize + 1);
			memcpy(xCore.AppFile, sTemp, iSize);
			xCore.AppFile[iSize] = 0;
			xrtFree(sTemp);
			sTemp = strrchr(xCore.AppFile, L'/');
			iSize = (sTemp - xCore.AppFile);
			xCore.AppPath = malloc(iSize + 1);
			memcpy(xCore.AppPath, xCore.AppFile, iSize);
			xCore.AppPath[iSize] = 0;
		}
	#endif
	
	// 初始化随机数序列
	srand(time(NULL));
}



// 释放 xCore
XXAPI void xCoreUnit(void)
{
	xrtFree(xCore.AppFile);
	xrtFree(xCore.AppPath);
}



#ifdef DBUILD_DLL
	
	
	
	#if defined(_WIN32) || defined(_WIN64)
		BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
		{
			if ( fdwReason == DLL_PROCESS_ATTACH ) {
				//当进程加载dll时调用dllMain
				xCoreInit();
			} else if ( fdwReason == DLL_PROCESS_DETACH ) {
				//当进程卸载dll时调用dllMain
				xCoreUnit();
			} else if ( fdwReason == DLL_THREAD_ATTACH ) {
				//当线程加载dll时调用dllMain
				
			} else if ( fdwReason == DLL_THREAD_DETACH ) {
				//当线程卸载dll时调用dllMain
				
			}
			return (TRUE);
		}
	#else
	#endif
	
	
	
#endif


