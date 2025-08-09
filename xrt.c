


#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#include "windows.h"
#else
#endif



// 全局数据
const int sNullValue = 0;
xrtGlobalData xCore = { FALSE };



// 引入补充依赖库
#include "lib/suplib.h"



// 引入子库
#include "lib/base.h"
#include "lib/charset.h"
#include "lib/math.h"
#include "lib/string.h"
#include "lib/time.h"
#include "lib/path.h"



// 初始化 xCore
XXAPI xrtGlobalData* xrtInit()
{
	if ( xCore.bInit ) {
		return &xCore;
	}
	
	// 初始化数据
	xCore.bInit = TRUE;
	xCore.sNull = (str)&sNullValue;
	xCore.sRet = xCore.sNull;
	xCore.iRet = 0;
	xCore.nRet = 0.0;
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
	xCore.DebugMode = FALSE;
	
	// 获取程序文件名和路径
	#if defined(_WIN32) || defined(_WIN64)
		str sTemp = malloc(8192);
		int iSize = GetModuleFileNameW(NULL, sTemp, 4096);
		xCore.AppFile = malloc((iSize + 1) * 2);
		memcpy(xCore.AppFile, sTemp, iSize * 2);
		xCore.AppFile[iSize] = 0;
		free(sTemp);
		sTemp = wcsrchr(xCore.AppFile, L'\\');
		iSize = (sTemp - xCore.AppFile);
		xCore.AppPath = malloc((iSize + 1) * 2);
		memcpy(xCore.AppPath, xCore.AppFile, iSize * 2);
		xCore.AppPath[iSize] = 0;
	#else
		ustr sTemp = malloc(4096);
		size_t iSize = readlink("/proc/self/exe", sTemp, 4096);
		if ( iSize == -1 ) {
			// 无法读取程序路径
			free(sTemp);
			xCore.AppFile = xCore.sNull;
			xCore.AppPath = xCore.sNull;
		} else {
			xCore.AppFile = malloc(iSize + 1);
			memcpy(xCore.AppFile, sTemp, iSize);
			xCore.AppFile[iSize] = 0;
			free(sTemp);
			sTemp = strrchr(xCore.AppFile, '/');
			iSize = (sTemp - xCore.AppFile);
			xCore.AppPath = malloc(iSize + 1);
			memcpy(xCore.AppPath, xCore.AppFile, iSize);
			xCore.AppPath[iSize] = 0;
		}
	#endif
	
	// 初始化内存函数
	xCore.malloc = malloc;
	xCore.calloc = calloc;
	xCore.realloc = realloc;
	xCore.free = free;
	
	// 初始化随机数序列
	srand(time(NULL));
	
	// 设置内置的错误描述（便于复用）
	#if defined(_WIN32) || defined(_WIN64)
		xCore.ERROR_DESC.MALLOC = L"Memory allocate error !";
	#else
		xCore.ERROR_DESC.MALLOC = "Memory allocate error !";
	#endif
	
	return &xCore;
}



// 释放 xCore
XXAPI void xrtUnit()
{
	if ( xCore.bInit ) {
		xrtFree(xCore.AppFile);
		xCore.AppFile = xCore.sNull;
		xrtFree(xCore.AppPath);
		xCore.AppPath = xCore.sNull;
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
			xCore.LastError = xCore.sNull;
			xCore.__pri_FreeError = FALSE;
		}
		xCore.bInit = FALSE;
	}
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


