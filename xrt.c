


#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/shellapi.h>
		#include <winapi/iphlpapi.h>
	#else
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
	#include <dirent.h>
	#include <sys/wait.h>
#endif



// 全局数据
static const unsigned char __xrt_sNullBytes[4] = "\0\0\0";
xrtGlobalData xCore = { FALSE };

#if defined(_WIN32) || defined(_WIN64)
	#if !defined(XRT_NO_NETWORK) || !defined(XRT_NO_NETSOCK) || !defined(XRT_NO_NETPOLL) || !defined(XRT_NO_NETTLS) || !defined(XRT_NO_NETLOOP) || !defined(XRT_NO_NETPROXY) || !defined(XRT_NO_NETTCP) || !defined(XRT_NO_NETUDP) || !defined(XRT_NO_NETHTTP) || !defined(XRT_NO_NETWS) || !defined(XRT_NO_NETHTTPD)
		#define __XRT_RUNTIME_NEED_WSA	1
	#else
		#define __XRT_RUNTIME_NEED_WSA	0
	#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
	static SRWLOCK __xrtRuntimeLockObj = SRWLOCK_INIT;
	#define __xrtRuntimeLock()		AcquireSRWLockExclusive(&__xrtRuntimeLockObj)
	#define __xrtRuntimeUnlock()	ReleaseSRWLockExclusive(&__xrtRuntimeLockObj)
	#if defined(__TINYC__)
		#define XRT_TLS_STORAGE		__declspec(thread)
	#elif defined(__GNUC__)
		#define XRT_TLS_STORAGE		__thread
	#else
		#define XRT_TLS_STORAGE		__declspec(thread)
	#endif
#else
	static pthread_mutex_t __xrtRuntimeLockObj = PTHREAD_MUTEX_INITIALIZER;
	#define __xrtRuntimeLock()		pthread_mutex_lock(&__xrtRuntimeLockObj)
	#define __xrtRuntimeUnlock()	pthread_mutex_unlock(&__xrtRuntimeLockObj)
	#define XRT_TLS_STORAGE			__thread
#endif

static XRT_TLS_STORAGE xrtThreadData* __xrtThreadState = NULL;

static uint64 __xrtGetSeedTick();
static void __xrtSeedThreadRand(xrtThreadData* pThreadData);
static xrtThreadData* __xrtCreateThreadState(struct xthread_struct* pThread);
static void __xrtInitThreadMemState(xrtThreadData* pThreadData);
static void __xrtUnitThreadMemState(xrtThreadData* pThreadData);
static void __xrtFreeThreadError(xrtThreadData* pThreadData);
static void __xrtFreeThreadTempMemory(xrtThreadData* pThreadData);
static void __xrtRunThreadCleanup(xrtThreadData* pThreadData);
static xrtThreadData* __xrtThreadAttachManaged(struct xthread_struct* pThread);
static void __xrtThreadExitManaged(struct xthread_struct* pThread, uint32 iExitCode);



// 引入补充依赖库
#include "lib/suplib.h"



// 引入子库 - 按依赖关系和裁剪支持重新组织
#include "lib/base.h"           // 核心基础，始终包含
#include "lib/string.h"         // 字符串处理，始终包含
#include "lib/os.h"             // 操作系统接口，始终包含
#include "lib/hash.h"           // 哈希算法，始终包含
#include "lib/charset.h"		// 编码转换，始终包含
#include "lib/math.h"			// 数学库，随机数很多地方都用到了，始终包含
#include "lib/path.h"			// 路径处理库，xCore 初始化使用，始终包含

#ifndef XRT_NO_TIME
#include "lib/time.h"
#endif

#ifndef XRT_NO_FILE
#include "lib/file.h"
#endif

#ifndef XRT_NO_THREAD
#include "lib/thread.h"
#endif

#ifndef XRT_NO_COROUTINE
#include "lib/coroutine.h"
#endif

#ifndef XRT_NO_NETWORK
#include "lib/network.h"
#endif

#if defined(__GNUC__)
	#pragma GCC push_options
	#pragma GCC optimize ("no-strict-aliasing")
#endif

#ifndef XRT_NO_CRYPTO
#include "lib/crypto.h"
#endif

#ifndef XRT_NO_NETSOCK
#include "lib/netsock.h"
#endif

#ifndef XRT_NO_NETPOLL
#include "lib/netpoll.h"
#endif

#ifndef XRT_NO_NETTLS
#include "lib/nettls.h"
#endif

#if defined(__GNUC__)
	#pragma GCC pop_options
#endif

#ifndef XRT_NO_NETLOOP
#include "lib/netloop.h"
#endif

#ifndef XRT_NO_NETPROXY
#include "lib/netproxy.h"
#endif

#ifndef XRT_NO_NETTCP
#include "lib/nettcp.h"
#endif

#ifndef XRT_NO_NETUDP
#include "lib/netudp.h"
#endif

#ifndef XRT_NO_XID
#include "lib/xid.h"
#endif

#ifndef XRT_NO_BUFFER
#include "lib/buffer.h"
#endif

#ifndef XRT_NO_NETHTTP
#include "lib/nethttp.h"
#endif

#ifndef XRT_NO_NETWS
#include "lib/netws.h"
#endif

#ifndef XRT_NO_NETHTTPD
#include "lib/nethttpd.h"
#endif

#ifndef XRT_NO_ARRAY
#include "lib/array_point.h"
#include "lib/array.h"
#endif

#ifndef XRT_NO_BSMN
#include "lib/bsmm.h"
#endif

#ifndef XRT_NO_MEMUNIT
#include "lib/memunit.h"
#endif

#ifndef XRT_NO_MEMPOOL_FS
#include "lib/mempool_fs.h"
#endif

#ifndef XRT_NO_STACK
#include "lib/stack.h"
#include "lib/stack_dyn.h"
#endif

#ifndef XRT_NO_AVLTREE
#include "lib/avltree_base.h"
#include "lib/avltree.h"
#endif

#ifndef XRT_NO_MEMPOOL
#include "lib/mempool.h"
#endif

#ifndef XRT_NO_DICT
#include "lib/dict.h"
#endif

#ifndef XRT_NO_LIST
#include "lib/list.h"
#endif

#ifndef XRT_NO_REGEX
#include "lib/regex.h"
#endif

#ifndef XRT_NO_VALUE
#include "lib/value.h"
#endif

#ifndef XRT_NO_JNUM
#include "lib/jnum.h"
#endif

#ifndef XRT_NO_JSON
#include "lib/json.h"
#endif

#ifndef XRT_NO_TEMPLATE
#include "lib/template.h"
#endif



static uint64 __xrtGetCurrentThreadId()
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetCurrentThreadId();
	#else
		return (uint64)(uintptr_t)pthread_self();
	#endif
}



static uint64 __xrtGetSeedTick()
{
	uint64 iTick = 0;

	#if defined(_WIN32) || defined(_WIN64)
		iTick = GetTickCount64() ^ (uint64)GetCurrentProcessId();
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		iTick = ((uint64)timer.tv_sec << 30) | (uint64)timer.tv_nsec;
		iTick ^= (uint64)getpid();
	#endif

	return iTick;
}



static void __xrtSeedThreadRand(xrtThreadData* pThreadData)
{
	uint64 iTick = __xrtGetSeedTick();

	if ( pThreadData == NULL ) {
		return;
	}

	iTick ^= (uint64)(uintptr_t)pThreadData;
	iTick ^= pThreadData->iThreadId;

	xrtRandSeed(&pThreadData->rand32, iTick, 0xda3e39cb94b95bdbULL ^ iTick);
	xrtRandSeed(&pThreadData->rand64_low, iTick * 0x5851f42d4c957f2dULL, 0xda3e39cb94b95bdbULL);
	xrtRandSeed(&pThreadData->rand64_high, iTick ^ 0x14057b7ef767814fULL, 0x14057b7ef767814fULL);
}



static void __xrtInitThreadMemState(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return;
	}

	pThreadData->tMem.iOwnerThreadId = pThreadData->iThreadId;
	pThreadData->tMem.iFlags = 0;
	pThreadData->tMem.iReserved = 0;
	pThreadData->tMem.pLocalAlloc = NULL;
	pThreadData->tMem.pSharedAlloc = NULL;
	pThreadData->tMem.pReserved = NULL;
}



static void __xrtUnitThreadMemState(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return;
	}

	pThreadData->tMem.iOwnerThreadId = 0;
	pThreadData->tMem.iFlags = 0;
	pThreadData->tMem.iReserved = 0;
	pThreadData->tMem.pLocalAlloc = NULL;
	pThreadData->tMem.pSharedAlloc = NULL;
	pThreadData->tMem.pReserved = NULL;
}



static xrtThreadData* __xrtCreateThreadState(struct xthread_struct* pThread)
{
	xrtThreadData* pThreadData = NULL;
	ptr (*procCalloc)(size_t, size_t) = xCore.calloc ? xCore.calloc : calloc;

	pThreadData = procCalloc(1, sizeof(xrtThreadData));
	if ( pThreadData == NULL ) {
		return NULL;
	}

	pThreadData->pGlobal = &xCore;
	pThreadData->iThreadId = __xrtGetCurrentThreadId();
	pThreadData->pSelf = pThread;
	pThreadData->iAttachDepth = 1;
	pThreadData->LastError = xCore.sNull ? xCore.sNull : (str)__xrt_sNullBytes;
	pThreadData->bFreeLastError = FALSE;
	pThreadData->TempMemIdx = 0;
	pThreadData->pCleanupTop = NULL;
	__xrtInitThreadMemState(pThreadData);
	#ifndef XRT_NO_COROUTINE
		__xrtCoroRuntimeInitThread(pThreadData);
	#endif

	for ( int i = 0; i < XRT_TEMP_SLOT_COUNT; i++ ) {
		pThreadData->TempMem[i] = NULL;
	}

	__xrtSeedThreadRand(pThreadData);

	return pThreadData;
}



static void __xrtFreeThreadError(xrtThreadData* pThreadData)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		procFree(pThreadData->LastError);
	}

	pThreadData->LastError = xCore.sNull ? xCore.sNull : (str)__xrt_sNullBytes;
	pThreadData->bFreeLastError = FALSE;
}



static void __xrtFreeThreadTempMemory(xrtThreadData* pThreadData)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL ) {
		return;
	}

	for ( int i = 0; i < XRT_TEMP_SLOT_COUNT; i++ ) {
		if ( pThreadData->TempMem[i] ) {
			procFree(pThreadData->TempMem[i]);
			pThreadData->TempMem[i] = NULL;
		}
	}

	pThreadData->TempMemIdx = 0;
}



static void __xrtRunThreadCleanup(xrtThreadData* pThreadData)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL ) {
		return;
	}

	while ( pThreadData->pCleanupTop ) {
		xrtThreadCleanup* pCleanup = pThreadData->pCleanupTop;

		pThreadData->pCleanupTop = pCleanup->pPrev;

		if ( pCleanup->Proc ) {
			pCleanup->Proc(pThreadData, pCleanup->Arg);
		}

		procFree(pCleanup);
	}
}



XXAPI xrtThreadData* xrtThreadGetCurrent()
{
	return __xrtThreadState;
}



XXAPI bool xrtThreadIsAttached()
{
	return __xrtThreadState != NULL;
}



XXAPI xrtThreadData* xrtThreadAttachCurrent()
{
	xrtThreadData* pThreadData = __xrtThreadState;

	if ( pThreadData ) {
		pThreadData->iAttachDepth++;
		return pThreadData;
	}

	if ( !xCore.bInit ) {
		return NULL;
	}

	pThreadData = __xrtCreateThreadState(NULL);
	if ( pThreadData == NULL ) {
		return NULL;
	}

	__xrtThreadState = pThreadData;
	return pThreadData;
}



static xrtThreadData* __xrtThreadAttachManaged(struct xthread_struct* pThread)
{
	xrtThreadData* pThreadData = xrtThreadAttachCurrent();

	if ( pThreadData ) {
		pThreadData->pSelf = pThread;
		pThreadData->iThreadId = __xrtGetCurrentThreadId();
		pThreadData->tMem.iOwnerThreadId = pThreadData->iThreadId;
		#ifndef XRT_NO_COROUTINE
			pThreadData->tCoro.iOwnerThreadId = pThreadData->iThreadId;
		#endif
	}

	return pThreadData;
}



XXAPI void xrtThreadDetachCurrent()
{
	xrtThreadData* pThreadData = __xrtThreadState;
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->iAttachDepth > 1 ) {
		pThreadData->iAttachDepth--;
		return;
	}

	__xrtRunThreadCleanup(pThreadData);
	__xrtFreeThreadError(pThreadData);
	__xrtFreeThreadTempMemory(pThreadData);
	#ifndef XRT_NO_COROUTINE
		__xrtCoroRuntimeUnitThread(pThreadData);
	#endif
	__xrtUnitThreadMemState(pThreadData);

	__xrtThreadState = NULL;
	procFree(pThreadData);
}



XXAPI str xrtGetError()
{
	xrtThreadData* pThreadData = __xrtThreadState;

	if ( pThreadData ) {
		return pThreadData->LastError;
	}

	if ( xCore.sNull ) {
		return xCore.sNull;
	}

	return (str)__xrt_sNullBytes;
}



XXAPI bool xrtThreadPushCleanup(xrtThreadCleanupProc proc, ptr pArg)
{
	xrtThreadData* pThreadData = __xrtThreadState;
	xrtThreadCleanup* pCleanup = NULL;
	ptr (*procMalloc)(size_t) = xCore.malloc ? xCore.malloc : malloc;

	if ( pThreadData == NULL || proc == NULL ) {
		return FALSE;
	}

	pCleanup = procMalloc(sizeof(xrtThreadCleanup));
	if ( pCleanup == NULL ) {
		return FALSE;
	}

	pCleanup->Proc = proc;
	pCleanup->Arg = pArg;
	pCleanup->pPrev = pThreadData->pCleanupTop;
	pThreadData->pCleanupTop = pCleanup;

	return TRUE;
}



XXAPI bool xrtThreadPopCleanup(xrtThreadCleanupProc proc, ptr pArg)
{
	xrtThreadData* pThreadData = __xrtThreadState;
	xrtThreadCleanup* pCleanup = NULL;
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL || pThreadData->pCleanupTop == NULL ) {
		return FALSE;
	}

	pCleanup = pThreadData->pCleanupTop;
	if ( pCleanup->Proc != proc || pCleanup->Arg != pArg ) {
		return FALSE;
	}

	pThreadData->pCleanupTop = pCleanup->pPrev;
	procFree(pCleanup);

	return TRUE;
}



static void __xrtThreadExitManaged(struct xthread_struct* pThread, uint32 iExitCode)
{
	if ( pThread ) {
		pThread->ExitCode = iExitCode;
		pThread->bFinished = TRUE;
	}

	xrtThreadDetachCurrent();
}



// 初始化 xCore
XXAPI xrtGlobalData* xrtInit()
{
	__xrtRuntimeLock();

	xCore.iInitRef++;

	if ( !xCore.bInit ) {
		// 初始化数据
		xCore.bInit = TRUE;
		xCore.sNull = (str)__xrt_sNullBytes;
		xCore.OnError = NULL;

		// 初始化内存函数
		xCore.malloc = malloc;
		xCore.calloc = calloc;
		xCore.realloc = realloc;
		xCore.free = free;

		// 初始化高精度时钟频率单位
		#if defined(_WIN32) || defined(_WIN64)
			LARGE_INTEGER QPF;
			if ( QueryPerformanceFrequency( &QPF ) ) {
				xCore.Frequency = QPF.QuadPart;
			} else {
				xCore.Frequency = 0;
			}
		#endif

		// 初始化约等于配置（整数容差万分之一、小数容差0.01、时间容差 10 秒、字符串相似度95%）
		xCore.iApproxIntMode = XRT_APPROX_PERCENT;
		xCore.fApproxIntTol = 0.0001;
		xCore.iApproxNumMode = XRT_APPROX_DIFF;
		xCore.fApproxNumTol = 0.01;
		xCore.iApproxTimeTol = 10;
		xCore.iApproxStrMode = XRT_STR_APPROX_SIM;  // 默认相似度模式
		xCore.fApproxStrTol = 0.95;                 // 默认95%相似度
		xCore.bApproxStrCase = FALSE;               // 默认区分大小写

		// 获取程序文件名和路径
		#if defined(_WIN32) || defined(_WIN64)
			u16str sTemp = malloc(4096 * sizeof(wchar_t));
			int iSize = GetModuleFileNameW(NULL, sTemp, 4096);
			size_t iRetSize = 0;
			xCore.AppFile = xrtUTF16to8(sTemp, iSize, &iRetSize);
			free(sTemp);
			xCore.AppPath = xrtPathGetDir(xCore.AppFile, iRetSize);
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
				xCore.AppPath = xrtPathGetDir(xCore.AppFile, iSize);
			}
		#endif

		// 初始化 socket
		#if defined(_WIN32) || defined(_WIN64)
			#if __XRT_RUNTIME_NEED_WSA
				WSADATA wsaData;
				WSAStartup(MAKEWORD(2, 2), &wsaData);
			#endif
		#endif

		// 获取本机 IP
		#ifndef XRT_NO_NETWORK
			xCore.LocalAddr = xrtGetLocalRawIP();
		#else
			xCore.LocalAddr = 0;
		#endif

		// 初始化模板引擎
		#ifndef XRT_NO_TEMPLATE
			xte_private_init();
		#endif
	}

	__xrtRuntimeUnlock();

	xrtThreadAttachCurrent();

	return &xCore;
}



// 释放 xCore
XXAPI void xrtUnit()
{
	xrtThreadDetachCurrent();

	__xrtRuntimeLock();

	if ( xCore.iInitRef > 0 ) {
		xCore.iInitRef--;
	}

	// 只有引用计数为 0 时才真正释放资源
	if ( (xCore.iInitRef == 0) && (xCore.bInit) ) {
		// 清理模板引擎
		#ifndef XRT_NO_TEMPLATE
			xte_private_unit();
		#endif

		// 释放应用路径
		xrtFree(xCore.AppFile);
		xCore.AppFile = xCore.sNull;
		xrtFree(xCore.AppPath);
		xCore.AppPath = xCore.sNull;

		// 重置初始化标记
		xCore.bInit = FALSE;

		// 释放 socket
		#if defined(_WIN32) || defined(_WIN64)
			#if __XRT_RUNTIME_NEED_WSA
				WSACleanup();
			#endif
		#endif
	}

	__xrtRuntimeUnlock();
}



#ifdef DBUILD_DLL
	
	
	
	#if defined(_WIN32) || defined(_WIN64)
		BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
		{
			if ( fdwReason == DLL_PROCESS_ATTACH ) {
				//当进程加载dll时调用dllMain
				xrtInit();
			} else if ( fdwReason == DLL_PROCESS_DETACH ) {
				//当进程卸载dll时调用dllMain
				xrtUnit();
			}
			return (TRUE);
		}
	#else
	#endif
	
	
	
#endif


