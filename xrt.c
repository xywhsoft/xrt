


#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/winsock2.h>
		#include "windows.h"
		#include <winapi/shellapi.h>
		#include <winapi/iphlpapi.h>
		ULONGLONG GetTickCount64();
	#else
		#include <winsock2.h>
		#include "windows.h"
		#include <shellapi.h>
		#include <iphlpapi.h>
	#endif
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <wchar.h>
	#pragma comment (lib, "shell32")
	#pragma comment (lib, "Ws2_32")
	#pragma comment (lib, "IPHLPAPI")
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <net/if.h>
	#include <sys/ioctl.h>
	#include <errno.h>
	#include <wchar.h>
	#include <netdb.h>
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
#include "lib/os.h"
#include "lib/file.h"
#include "lib/hash.h"
#include "lib/network.h"
#include "lib/xid.h"
#include "lib/buffer.h"
#include "lib/stack.h"
#include "lib/llist.h"
#include "lib/avltree.h"
#include "lib/collect.h"
#include "lib/array.h"
#include "lib/mempool.h"
#include "lib/list.h"
#include "lib/table.h"
#include "lib/value.h"
#include "lib/json.h"
#include "lib/template.h"



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
	
	// 初始化内存函数
	xCore.malloc = malloc;
	xCore.calloc = calloc;
	xCore.realloc = realloc;
	xCore.free = free;
	
	// 初始化环形临时内存
	for ( int i = 0; i < 16; i++ ) {
		xCore.TempMem[i] = NULL;
	}
	xCore.TempMemIdx = 0;
	
	// 初始化高精度时钟频率单位
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER QPF;
		if ( QueryPerformanceFrequency( &QPF ) ) {
			xCore.Frequency = QPF.QuadPart;
		} else {
			xCore.Frequency = 0;
		}
	#endif
	
	// 初始化随机数序列
	uint64 iTick = 0;
	#if defined(_WIN32) || defined(_WIN64)
		if ( xCore.Frequency == 0.0 ) {
			iTick = GetTickCount64();
			xrtSetRandSeed32(xrtNow(), 0xda3e39cb94b95bdbULL ^ iTick);
		} else {
			LARGE_INTEGER QPC;
			QueryPerformanceCounter(&QPC);
			iTick = QPC.QuadPart;
			xrtSetRandSeed32(xrtNow(), 0xda3e39cb94b95bdbULL ^ iTick);
		}
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		iTick = (timer.tv_sec << 30) | timer.tv_nsec;
		xrtSetRandSeed32(xrtNow(), 0xda3e39cb94b95bdbULL ^ iTick);
	#endif
	
	// 初始化 64 位随机数序列 ( 这里打乱顺序生成，为了避免干扰，并使用不同的方式扰乱结果 )
	uint32 highseed_low = xrtRand32() * iTick;
	uint32 lowseed_high = xrtRand32() + xrtRand32();
	uint32 highseq_high = xrtRand32() * xrtRand32();
	uint32 lowseq_low = xrtRand32() ^ xrtRand32();
	uint32 highseed_high = xrtRand32() ^ iTick;
	uint32 lowseed_low = xrtRand32() * xrtRand32();
	uint32 highseq_low = xrtRand32() ^ xrtRand32();
	uint32 lowseq_high = xrtRand32() + xrtRand32();
	uint64 lowseed = ((uint64)lowseed_high << 32) | (uint64)lowseed_low;
	uint64 lowseq = ((uint64)lowseq_high << 32) | (uint64)lowseq_low;
	uint64 highseed = ((uint64)highseed_high << 32) | (uint64)highseed_low;
	uint64 highseq = ((uint64)highseq_high << 32) | (uint64)highseq_low;
	xrtSetRandSeed64(lowseed, lowseq, highseed, highseq);
	
	// 设置内置的错误描述（便于复用）
	xCore.ERROR_DESC.MALLOC = "Memory allocate error !";
	
	// 获取程序文件名和路径
	#if defined(_WIN32) || defined(_WIN64)
		u16str sTemp = malloc(4096 * sizeof(wchar_t));
		int iSize = GetModuleFileNameW(NULL, sTemp, 4096);
		xCore.AppFile = xrtUTF16to8(sTemp, iSize);
		free(sTemp);
		xCore.AppPath = xrtPathGetDir(xCore.AppFile, xCore.iRet);
	#else
		str sTemp = malloc(4096);
		ssize_t iSize = readlink("/proc/self/exe", sTemp, 4096);
		if ( iSize == -1 ) {
			// 无法读取程序路径
			free(sTemp);
			xCore.AppFile = xCore.sNull;
			xCore.AppPath = xCore.sNull;
		} else {
			xCore.AppFile = xrtCopyStr(sTemp, iSize);
			free(sTemp);
			xCore.AppPath = xrtPathGetDir(xCore.AppFile, xCore.iRet);
		}
	#endif
	
	// 初始化 socket
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	#endif
	
	// 获取本机 IP
	xCore.LocalAddr = xrtGetLocalRawIP();
	
	return &xCore;
}



// 释放 xCore
XXAPI void xrtUnit()
{
	if ( xCore.bInit ) {
		// 释放应用路径
		xrtFree(xCore.AppFile);
		xCore.AppFile = xCore.sNull;
		xrtFree(xCore.AppPath);
		xCore.AppPath = xCore.sNull;
		// 释放错误描述
		if ( xCore.__pri_FreeError && xCore.LastError ) {
			xrtFree(xCore.LastError);
			xCore.LastError = xCore.sNull;
			xCore.__pri_FreeError = FALSE;
		}
		// 释放环形临时内存
		for ( int i = 0; i < 16; i++ ) {
			if ( xCore.TempMem[i] ) {
				free(xCore.TempMem[i]);
				xCore.TempMem[i] = NULL;
			}
		}
		xCore.TempMemIdx = 0;
		xCore.bInit = FALSE;
		
		// 释放 socket
		#if defined(_WIN32) || defined(_WIN64)
			WSACleanup();
		#endif
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


