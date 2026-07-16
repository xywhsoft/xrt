
#ifndef XRT_BUILD_CORE
	#define XRT_BUILD_CORE
#endif
#if defined(XRT_MEM_DEBUG)
	#include "lib/memdebug_site_macros_undef.h"
#endif
#include "xrt.h"



#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/windows.h>
		#include <winapi/shellapi.h>
		#include <winapi/iphlpapi.h>
	#else
		#include <shellapi.h>
		#include <iphlpapi.h>
	#endif
	#if defined(XNET_DEBUG_IOCP_NATIVE)
		#include <stdio.h>
	#endif
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <wchar.h>
	#if defined(_MSC_VER)
		#pragma comment (lib, "shell32")
		#pragma comment (lib, "Ws2_32")
		#pragma comment (lib, "IPHLPAPI")
	#endif
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <net/if.h>
	#include <sys/ioctl.h>
	#if defined(__linux__)
		#if !defined(__ANDROID__) || !defined(__ANDROID_API__) || __ANDROID_API__ >= 24
			#include <ifaddrs.h>
		#endif
		#include <sys/epoll.h>
		#include <sys/syscall.h>
		#include <sys/eventfd.h>
		#include <sys/mman.h>
		#include <netpacket/packet.h>
		#include <sched.h>
	#endif
	#if defined(__APPLE__) && defined(__MACH__)
		#include <ifaddrs.h>
		#include <mach-o/dyld.h>
		#include <net/if_dl.h>
		#include <net/if_types.h>
		#include <sys/event.h>
	#endif
	#include <poll.h>
	#include <sys/select.h>
	#include <sys/uio.h>
	#include <errno.h>
	#include <wchar.h>
	#include <netdb.h>
	#include <dirent.h>
	#include <sys/wait.h>
#endif

#if defined(__TINYC__)
	#ifndef UINT32_C
		#define UINT32_C(x) x ## U
	#endif
	#ifndef UINT64_C
		#define UINT64_C(x) x ## ULL
	#endif
	#undef SIZE_MAX
	#define SIZE_MAX ((size_t)-1)
#endif


#if defined(__APPLE__) && defined(__MACH__)
	extern char** environ;
	static int UNUSED_ATTR clearenv(void)
	{
		environ = NULL;
		return 0;
	}
#endif


// 全局数据
static const unsigned char __xrt_sNullBytes[4] = "\0\0\0";
xrtGlobalData xCore = { 0 };

#if defined(_WIN32) || defined(_WIN64)
	#if !defined(XRT_NO_NETWORK) || !defined(XRT_NO_NETTLS)
		#define __XRT_RUNTIME_NEED_WSA	1
	#else
		#define __XRT_RUNTIME_NEED_WSA	0
	#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
	static SRWLOCK __xrtRuntimeLockObj = SRWLOCK_INIT;
	#define __xrtRuntimeLock()		AcquireSRWLockExclusive(&__xrtRuntimeLockObj)
	#define __xrtRuntimeUnlock()	ReleaseSRWLockExclusive(&__xrtRuntimeLockObj)
	#if defined(__GNUC__)
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

// 线程状态存储适配层
#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		static DWORD __xrtThreadTlsSlot = TLS_OUT_OF_INDEXES;


		// 为当前线程准备 TLS 存储槽
		static bool __xrtThreadStateInitStorage()
		{
			if ( __xrtThreadTlsSlot != TLS_OUT_OF_INDEXES ) {
				return TRUE;
			}

			__xrtThreadTlsSlot = TlsAlloc();
			return __xrtThreadTlsSlot != TLS_OUT_OF_INDEXES;
		}


		// 释放线程状态使用的 TLS 存储槽
		static void __xrtThreadStateUnitStorage()
		{
			if ( __xrtThreadTlsSlot != TLS_OUT_OF_INDEXES ) {
				TlsFree(__xrtThreadTlsSlot);
				__xrtThreadTlsSlot = TLS_OUT_OF_INDEXES;
			}
		}


		// 读取当前线程绑定的运行时状态
		static xrtThreadData* __xrtThreadStateGet()
		{
			if ( __xrtThreadTlsSlot == TLS_OUT_OF_INDEXES ) {
				return NULL;
			}

			return (xrtThreadData*)TlsGetValue(__xrtThreadTlsSlot);
		}


		// 绑定当前线程的运行时状态
		static void __xrtThreadStateSet(xrtThreadData* pThreadData)
		{
			if ( __xrtThreadTlsSlot != TLS_OUT_OF_INDEXES ) {
				(void)TlsSetValue(__xrtThreadTlsSlot, pThreadData);
			}
		}
	#else
		static XRT_TLS_STORAGE xrtThreadData* __xrtThreadState = NULL;


		// TLS 变量模式下无需额外初始化
		static bool __xrtThreadStateInitStorage()
		{
			return TRUE;
		}


		// TLS 变量模式下无需额外释放
		static void __xrtThreadStateUnitStorage()
		{
		}


		// 读取当前线程绑定的运行时状态
		static xrtThreadData* __xrtThreadStateGet()
		{
			return __xrtThreadState;
		}


		// 绑定当前线程的运行时状态
		static void __xrtThreadStateSet(xrtThreadData* pThreadData)
		{
			__xrtThreadState = pThreadData;
		}
	#endif
#else
	#if defined(__TINYC__)
		static pthread_key_t __xrtThreadTlsKey;
		static bool __xrtThreadTlsKeyReady = FALSE;


		// 为当前线程准备 pthread TLS 键
		static bool __xrtThreadStateInitStorage()
		{
			if ( __xrtThreadTlsKeyReady ) {
				return TRUE;
			}

			if ( pthread_key_create(&__xrtThreadTlsKey, NULL) != 0 ) {
				return FALSE;
			}

			__xrtThreadTlsKeyReady = TRUE;
			return TRUE;
		}


		// 释放线程状态使用的 pthread TLS 键
		static void __xrtThreadStateUnitStorage()
		{
			if ( __xrtThreadTlsKeyReady ) {
				(void)pthread_key_delete(__xrtThreadTlsKey);
				__xrtThreadTlsKeyReady = FALSE;
			}
		}


		// 读取当前线程绑定的运行时状态
		static xrtThreadData* __xrtThreadStateGet()
		{
			if ( !__xrtThreadTlsKeyReady ) {
				return NULL;
			}

			return (xrtThreadData*)pthread_getspecific(__xrtThreadTlsKey);
		}


		// 绑定当前线程的运行时状态
		static void __xrtThreadStateSet(xrtThreadData* pThreadData)
		{
			if ( __xrtThreadTlsKeyReady ) {
				(void)pthread_setspecific(__xrtThreadTlsKey, pThreadData);
			}
		}
	#else
		static XRT_TLS_STORAGE xrtThreadData* __xrtThreadState = NULL;


		// TLS 变量模式下无需额外初始化
		static bool __xrtThreadStateInitStorage()
		{
			return TRUE;
		}


		// TLS 变量模式下无需额外释放
		static void __xrtThreadStateUnitStorage()
		{
		}


		// 读取当前线程绑定的运行时状态
		static xrtThreadData* __xrtThreadStateGet()
		{
			return __xrtThreadState;
		}


		// 绑定当前线程的运行时状态
		static void __xrtThreadStateSet(xrtThreadData* pThreadData)
		{
			__xrtThreadState = pThreadData;
		}
	#endif
#endif

#ifndef XRT_MEM_DEBUG
	/*
	 * 发布版仍需识别显式内存池指针，防止误交给 xrtFree/xrtRealloc。
	 * 使用固定桶避免每次池分配都扫描全部活动记录；桶数组位于 BSS，
	 * 不会把同等大小的数据写入二进制文件。
	 */
	#define __XRT_MEM_FOREIGN_BUCKET_COUNT 1024u
	static volatile long __xrtMemForeignAllocLock = 0;
	static xrtMemDebugForeignAlloc* __xrtMemForeignAllocBuckets[__XRT_MEM_FOREIGN_BUCKET_COUNT] = { 0 };
#endif

static uint32 __xrtRuntimeThreadRefCount = 0;

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
static void __xrtRuntimeFinalizeLocked();



// 引入补充依赖库
#include "lib/suplib.h"



// 引入子库 - 按依赖关系和裁剪支持重新组织
#include "lib/memglobal.h"      // 全局内存池核心
#include "lib/base.h"           // 核心基础，始终包含
#if defined(XRT_MEM_DEBUG)
	#include "lib/memdebug_site_macros_base.h"
#endif
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
	#if !defined(XRT_NO_FILE_ASYNC)
		#include "lib/file_async.h"
	#endif
#endif

#ifndef XRT_NO_THREAD
	#include "lib/thread.h"
#endif

#if !defined(XRT_NO_THREAD) && !defined(XRT_NO_TIME)
	#include "lib/signal.h"
#endif

#if !defined(XRT_NO_LOGGER) && !defined(XRT_NO_TIME)
	#include "lib/logger.h"
#endif

#ifndef XRT_NO_QUEUE
	#include "lib/queue.h"
	#ifndef XRT_NO_QUEUE_WAIT
		#include "lib/channel.h"
	#endif
#endif

#ifndef XRT_NO_COROUTINE
	#include "lib/coroutine.h"
#endif

#ifndef XRT_NO_NETWORK
	#ifndef XRT_NO_XURL
		#include "lib/xurl.h"
	#endif
	#ifndef XRT_NO_HTTP_UTIL
		#include "lib/xhttp_util.h"
		#include "lib/xhttp_semantics.h"
		#include "lib/xhttp_search_params.h"
		#include "lib/xhttp_context_body.h"
		#include "lib/xhttp_form_data.h"
	#endif
	#include "lib/xnet_base.h"
	#include "lib/xnet_mem.h"
	#include "lib/xnet_port.h"
	#include "lib/xnet_port_iocp.h"
	#include "lib/xnet_port_uring.h"
	#include "lib/xnet_port_epoll.h"
	#include "lib/xnet_port_kqueue.h"
	#include "lib/xnet_port_select.h"
	#ifndef XRT_NO_XCODEC
		#include "lib/xcodec.h"
		#include "lib/xcodec_http1.h"
		#include "lib/xcodec_ws.h"
	#endif
	#include "lib/xnet_engine.h"
#endif

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC push_options
	#pragma GCC optimize ("no-strict-aliasing")
#endif

#ifndef XRT_NO_CRYPTO
	#include "lib/crypto.h"
#endif

#ifndef XRT_NO_NETTLS
	#include "lib/nettls.h"
#endif

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC pop_options
#endif

#ifndef XRT_NO_NETWORK
	#include "lib/xnet_proxy.h"
	#include "lib/xnet_stream.h"
	#include "lib/xnet_dgram.h"
	#include "lib/xnet_sync.h"
	#ifndef XRT_NO_XINFLATE
		#include "lib/xinflate.h"
	#endif
	#ifndef XRT_NO_XDEFLATE
		#include "lib/xdeflate.h"
	#endif
	#ifndef XRT_NO_HTTP_UTIL
		#include "lib/xhttp_cookie.h"
	#endif
	#ifndef XRT_NO_XHTTP
		#include "lib/xhttp.h"
	#endif
	#ifndef XRT_NO_XHTTPD
		#include "lib/xhttpd.h"
	#endif
	#ifndef XRT_NO_XWEB
		#include "lib/xweb.h"
	#endif
	#ifndef XRT_NO_XWS
		#include "lib/xws.h"
	#endif
	#include "lib/network.h"
#endif

#ifndef XRT_NO_SUBPROCESS
	#include "lib/subprocess.h"
#endif

#ifndef XRT_NO_XID
	#include "lib/xid.h"
#endif

#ifndef XRT_NO_BUFFER
	#include "lib/buffer.h"
#endif

#if !defined(XRT_NO_STREAM) && !defined(XRT_NO_FILE) && !defined(XRT_NO_BUFFER)
	#include "lib/stream.h"
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

#include "lib/set.h"

#ifndef XRT_NO_VALUE
	#include "lib/typed_container.h"
	#ifndef XRT_NO_TYPED_SPECIAL
		#include "lib/typed_special.h"
	#endif
	#include "lib/value.h"
	#include "lib/type.h"
#endif

#ifndef XRT_NO_JNUM
	#include "lib/jnum.h"
#endif

#ifndef XRT_NO_JSON
	#include "lib/json.h"
	#if !defined(XRT_NO_XSON)
		#include "lib/xson.h"
	#endif
#endif

#ifndef XRT_NO_TEMPLATE
	#include "lib/template.h"
#endif



// 线程状态与运行时辅助函数

// 获取当前执行线程的稳定标识
static uint64 __xrtGetCurrentThreadId()
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetCurrentThreadId();
	#else
		return (uint64)(uintptr_t)pthread_self();
	#endif
}



// 收集线程随机种子使用的时钟值
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



// 为线程初始化随机数状态
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



// 初始化线程本地内存上下文
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
	__xrtMemGlobalInitThreadCache(pThreadData);
}



// 保留启用状态并清空内存遥测计数
static void __xrtResetMemTelemetryState(xrtMemTelemetryState* pState)
{
	long bEnabled = 0;

	if ( pState == NULL ) {
		return;
	}

	bEnabled = pState->bEnabled;
	memset(pState, 0, sizeof(xrtMemTelemetryState));
	pState->bEnabled = bEnabled;
}



// 回收线程本地内存上下文
static void __xrtUnitThreadMemState(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return;
	}

	pThreadData->tMem.iOwnerThreadId = 0;
	pThreadData->tMem.iFlags = 0;
	pThreadData->tMem.iReserved = 0;
	__xrtMemGlobalUnitThreadCache(pThreadData);
	pThreadData->tMem.pLocalAlloc = NULL;
	pThreadData->tMem.pSharedAlloc = NULL;
	pThreadData->tMem.pReserved = NULL;
}



// 命名线程本地数据节点只保存普通内存，不持有外部模块回调。
struct xrtThreadLocalEntry {
	struct xrtThreadLocalEntry* pNext;
	str sKey;
	ptr pValue;
	size_t iSize;
};



// 创建并填充线程运行时状态
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
	pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
	pThreadData->pCleanupTop = NULL;
	pThreadData->pLocalEntryHead = NULL;
	__xrtInitThreadMemState(pThreadData);
	#ifndef XRT_NO_COROUTINE
		__xrtCoroRuntimeInitThread(pThreadData);
	#endif

	__xrtSeedThreadRand(pThreadData);

	return pThreadData;
}



// 释放线程缓存的错误文本
static void __xrtFreeThreadError(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}

	pThreadData->LastError = xCore.sNull ? xCore.sNull : (str)__xrt_sNullBytes;
	pThreadData->bFreeLastError = FALSE;
}



// 释放线程临时内存池
static void __xrtFreeThreadTempMemory(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return;
	}
	__xrtTempArenaFreeAllThread(pThreadData);
}



// 释放当前线程的全部命名本地数据
static void __xrtFreeThreadLocalEntries(xrtThreadData* pThreadData)
{
	xrtThreadLocalEntry* pEntry;
	if ( pThreadData == NULL ) { return; }
	pEntry = pThreadData->pLocalEntryHead;
	pThreadData->pLocalEntryHead = NULL;
	while ( pEntry ) {
		xrtThreadLocalEntry* pNext = pEntry->pNext;
		xrtFree(pEntry->sKey);
		xrtFree(pEntry->pValue);
		xrtFree(pEntry);
		pEntry = pNext;
	}
}



// 在持锁状态下收尾整个运行时
static void __xrtRuntimeFinalizeLocked()
{
	if ( !xCore.bInit ) {
		return;
	}

	#ifdef XRT_MEM_DEBUG
	#endif

	#ifndef XRT_NO_TEMPLATE
		xte_private_unit();
	#endif

	#if !defined(XRT_NO_LOGGER) && !defined(XRT_NO_TIME)
		__xlogRuntimeUnit();
	#endif

	#if !defined(XRT_NO_THREAD) && !defined(XRT_NO_TIME)
		xrtSignalUnit();
	#endif

	xrtFree(xCore.AppFile);
	xCore.AppFile = xCore.sNull;
	xrtFree(xCore.AppPath);
	xCore.AppPath = xCore.sNull;
	#ifdef XRT_MEM_DEBUG
		xrtMemDebugDumpText("xrt_mem_report_auto.txt");
		xrtMemDebugDumpJson("xrt_mem_report_auto.json");
	#endif
	__xrtMemGlobalUnitPlan(&xCore.MemGlobal);
	#ifdef XRT_MEM_DEBUG
		__xrtMemDebugResetState(&xCore.MemDebug);
	#else
		__xrtMemDebugResetState(NULL);
	#endif

	xCore.bInit = FALSE;
	__xrtRuntimeThreadRefCount = 0;
	__xrtThreadStateUnitStorage();

	#if defined(_WIN32) || defined(_WIN64)
		#if __XRT_RUNTIME_NEED_WSA
			WSACleanup();
		#endif
	#endif
}



// 按栈顺序执行线程清理回调
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



// 获取当前线程的运行时状态
XXAPI xrtThreadData* xrtThreadGetCurrent()
{
	return __xrtThreadStateGet();
}



// 判断当前线程是否已附加到 XRT 运行时
XXAPI bool xrtThreadIsAttached()
{
	return __xrtThreadStateGet() != NULL;
}



// 将当前线程附加到 XRT 运行时
XXAPI xrtThreadData* xrtThreadAttachCurrent()
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();

	if ( pThreadData ) {
		pThreadData->iAttachDepth++;
		return pThreadData;
	}

	__xrtRuntimeLock();

	if ( !xCore.bInit ) {
		__xrtRuntimeUnlock();
		return NULL;
	}

	if ( !__xrtThreadStateInitStorage() ) {
		__xrtRuntimeUnlock();
		return NULL;
	}

	pThreadData = __xrtCreateThreadState(NULL);
	if ( pThreadData == NULL ) {
		__xrtRuntimeUnlock();
		return NULL;
	}

	__xrtRuntimeThreadRefCount++;
	__xrtThreadStateSet(pThreadData);
	__xrtRuntimeUnlock();
	return pThreadData;
}



// 为托管线程附加运行时状态并修正线程标识
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



// 将当前线程从 XRT 运行时分离
XXAPI void xrtThreadDetachCurrent()
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;

	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->iAttachDepth > 1 ) {
		pThreadData->iAttachDepth--;
		return;
	}

	__xrtRunThreadCleanup(pThreadData);
	__xrtFreeThreadLocalEntries(pThreadData);
	__xrtFreeThreadError(pThreadData);
	__xrtFreeThreadTempMemory(pThreadData);
	#ifndef XRT_NO_COROUTINE
		__xrtCoroRuntimeUnitThread(pThreadData);
	#endif
	__xrtUnitThreadMemState(pThreadData);

	__xrtThreadStateSet(NULL);
	procFree(pThreadData);

	__xrtRuntimeLock();
	if ( __xrtRuntimeThreadRefCount > 0 ) {
		__xrtRuntimeThreadRefCount--;
	}
	if ( (xCore.iInitRef == 0) && (__xrtRuntimeThreadRefCount == 0) && xCore.bInit ) {
		__xrtRuntimeFinalizeLocked();
	}
	__xrtRuntimeUnlock();
}



// 获取错误
XXAPI str xrtGetError()
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();

	if ( pThreadData ) {
		return pThreadData->LastError;
	}

	if ( xCore.sNull ) {
		return xCore.sNull;
	}

	return (str)__xrt_sNullBytes;
}



// 查找当前线程的命名本地数据节点
static xrtThreadLocalEntry* __xrtThreadLocalFind(xrtThreadData* pThreadData,
	const char* sKey, xrtThreadLocalEntry*** pLink)
{
	xrtThreadLocalEntry** ppEntry;
	if ( pLink ) { *pLink = NULL; }
	if ( pThreadData == NULL || sKey == NULL || sKey[0] == '\0' ) { return NULL; }
	ppEntry = &pThreadData->pLocalEntryHead;
	while ( *ppEntry ) {
		if ( strcmp((const char*)(*ppEntry)->sKey, sKey) == 0 ) {
			if ( pLink ) { *pLink = ppEntry; }
			return *ppEntry;
		}
		ppEntry = &(*ppEntry)->pNext;
	}
	if ( pLink ) { *pLink = ppEntry; }
	return NULL;
}



// 获取命名线程本地数据
XXAPI ptr xrtThreadLocalGet(const char* sKey, size_t* pSize)
{
	xrtThreadLocalEntry* pEntry = __xrtThreadLocalFind(__xrtThreadStateGet(), sKey, NULL);
	if ( pSize ) { *pSize = pEntry ? pEntry->iSize : 0u; }
	return pEntry ? pEntry->pValue : NULL;
}



// 获取或创建命名线程本地数据
XXAPI ptr xrtThreadLocalGetOrCreate(const char* sKey, size_t iSize)
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();
	xrtThreadLocalEntry* pEntry;
	xrtThreadLocalEntry** ppLink = NULL;
	if ( pThreadData == NULL || sKey == NULL || sKey[0] == '\0' || iSize == 0u ) { return NULL; }
	pEntry = __xrtThreadLocalFind(pThreadData, sKey, &ppLink);
	if ( pEntry ) {
		if ( pEntry->iSize != iSize ) {
			xrtSetError("thread local data size does not match the existing entry.", FALSE);
			return NULL;
		}
		return pEntry->pValue;
	}
	pEntry = (xrtThreadLocalEntry*)xrtCalloc(1u, sizeof(*pEntry));
	if ( pEntry == NULL ) { return NULL; }
	pEntry->sKey = xrtCopyStr((str)sKey, 0u);
	pEntry->pValue = xrtCalloc(1u, iSize);
	if ( pEntry->sKey == NULL || pEntry->pValue == NULL ) {
		xrtFree(pEntry->sKey);
		xrtFree(pEntry->pValue);
		xrtFree(pEntry);
		return NULL;
	}
	pEntry->iSize = iSize;
	*ppLink = pEntry;
	return pEntry->pValue;
}



// 删除命名线程本地数据
XXAPI bool xrtThreadLocalRemove(const char* sKey)
{
	xrtThreadLocalEntry* pEntry;
	xrtThreadLocalEntry** ppLink = NULL;
	pEntry = __xrtThreadLocalFind(__xrtThreadStateGet(), sKey, &ppLink);
	if ( pEntry == NULL || ppLink == NULL ) { return FALSE; }
	*ppLink = pEntry->pNext;
	xrtFree(pEntry->sKey);
	xrtFree(pEntry->pValue);
	xrtFree(pEntry);
	return TRUE;
}

#ifdef XRT_MEM_DEBUG
// 内存调试报告导出辅助函数

// 获取内存调试事件名称
static const char* __xrtMemDebugEventName(uint32 iType)
{
	switch ( iType ) {
		case XRT_MEMDEBUG_EVENT_ALLOC: return "ALLOC";
		case XRT_MEMDEBUG_EVENT_FREE: return "FREE";
		case XRT_MEMDEBUG_EVENT_REALLOC: return "REALLOC";
		case XRT_MEMDEBUG_EVENT_LEAK: return "LEAK";
		case XRT_MEMDEBUG_EVENT_POOL_LEAK: return "POOL_LEAK";
		case XRT_MEMDEBUG_EVENT_OBJECT_LEAK: return "OBJECT_LEAK";
		case XRT_MEMDEBUG_EVENT_DOUBLE_FREE: return "DOUBLE_FREE";
		case XRT_MEMDEBUG_EVENT_INVALID_FREE: return "INVALID_FREE";
		case XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE: return "WRONG_ALLOCATOR_FREE";
		case XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT: return "BUFFER_OVERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT: return "BUFFER_UNDERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT: return "USE_AFTER_FREE_SUSPECT";
		case XRT_MEMDEBUG_EVENT_OBJECT_CREATE: return "OBJECT_CREATE";
		case XRT_MEMDEBUG_EVENT_OBJECT_DESTROY: return "OBJECT_DESTROY";
		case XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY: return "OBJECT_DOUBLE_DESTROY";
		case XRT_MEMDEBUG_EVENT_TEMP_ALLOC: return "TEMP_ALLOC";
		case XRT_MEMDEBUG_EVENT_TEMP_RESET: return "TEMP_RESET";
		default: return "UNKNOWN";
	}
}


// 获取内存调试对象类型名称
static const char* __xrtMemDebugObjectTypeName(uint32 iObjectType)
{
	switch ( iObjectType ) {
		case XRT_MEMDEBUG_OBJECT_ARRAY: return "array";
		case XRT_MEMDEBUG_OBJECT_DICT: return "dict";
		case XRT_MEMDEBUG_OBJECT_LIST: return "list";
		case XRT_MEMDEBUG_OBJECT_AVLTREE: return "avltree";
		case XRT_MEMDEBUG_OBJECT_DYNSTACK: return "dynstack";
		case XRT_MEMDEBUG_OBJECT_MEMPOOL: return "mempool";
		case XRT_MEMDEBUG_OBJECT_FSMEMPOOL: return "fsmempool";
		default: return "unknown";
	}
}


// 获取内存调试对象来源名称
static const char* __xrtMemDebugObjectOriginName(uint32 iOrigin)
{
	switch ( iOrigin ) {
		case XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE: return "create";
		case XRT_MEMDEBUG_OBJECT_ORIGIN_INIT: return "init";
		default: return "unknown";
	}
}


// 判断事件是否使用对象类型字段
static bool __xrtMemDebugEventUsesObjectType(uint32 iType)
{
	return iType == XRT_MEMDEBUG_EVENT_OBJECT_CREATE
		|| iType == XRT_MEMDEBUG_EVENT_OBJECT_DESTROY
		|| iType == XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY
		|| iType == XRT_MEMDEBUG_EVENT_OBJECT_LEAK;
}


// 按 JSON 转义规则写入字符串
static void __xrtMemDebugJsonWriteString(FILE* pFile, const char* sText)
{
	const unsigned char* p = (const unsigned char*)(sText ? sText : "");

	fputc('"', pFile);
	while ( *p ) {
		switch ( *p ) {
			case '\\': fputs("\\\\", pFile); break;
			case '"': fputs("\\\"", pFile); break;
			case '\r': fputs("\\r", pFile); break;
			case '\n': fputs("\\n", pFile); break;
			case '\t': fputs("\\t", pFile); break;
			default:
				if ( *p < 0x20 ) {
					fprintf(pFile, "\\u%04x", (unsigned int)*p);
				} else {
					fputc(*p, pFile);
				}
				break;
		}
		p++;
	}
	fputc('"', pFile);
}


// 将当前内存调试状态导出为文本报告
static bool __xrtMemDebugHasIssues()
{
	return __xrtMemDebugHasLeaks()
		|| xCore.MemDebug.iInvalidFreeCount != 0
		|| xCore.MemDebug.iDoubleFreeCount != 0
		|| xCore.MemDebug.iWrongAllocatorFreeCount != 0
		|| xCore.MemDebug.iObjectDoubleDestroyCount != 0
		|| xCore.MemDebug.iOverflowCount != 0
		|| xCore.MemDebug.iUnderflowCount != 0;
}


// 判断内存调试状态是否正常（无泄漏、无越界等）
static const char* __xrtMemDebugReportStatusName(bool bHasIssues)
{
	return bHasIssues ? "issues_detected" : "clean";
}


// 获取内存调试状态摘要文本
static const char* __xrtMemDebugReportSummary(bool bHasIssues)
{
	return bHasIssues ? "memory issues detected" : "no memory issues detected";
}


// 将内存调试状态导出为文本文件
static bool __xrtMemDebugDumpTextFile(FILE* pFile)
{
	xrtMemBlockHeader* pHeader;
	xrtMemDebugForeignAlloc* pForeign;
	xrtMemDebugObject* pObject;
	xrtMemDebugSiteStat* pSite;
	uint32 iCount;
	bool bHasIssues;

	if ( pFile == NULL ) {
		return FALSE;
	}

	__xrtMemDebugLock();
	bHasIssues = __xrtMemDebugHasIssues();
	fprintf(pFile, "# XRT Memory Debug Report\n\n");
	fprintf(pFile, "- status: %s\n", __xrtMemDebugReportStatusName(bHasIssues));
	fprintf(pFile, "- summary: %s\n", __xrtMemDebugReportSummary(bHasIssues));
	fprintf(pFile, "- live_alloc_count: %llu\n", (unsigned long long)xCore.MemDebug.iLiveAllocCount);
	fprintf(pFile, "- live_alloc_bytes: %llu\n", (unsigned long long)xCore.MemDebug.iLiveAllocBytes);
	fprintf(pFile, "- foreign_live_count: %llu\n", (unsigned long long)xCore.MemDebug.iForeignLiveCount);
	fprintf(pFile, "- foreign_live_bytes: %llu\n", (unsigned long long)xCore.MemDebug.iForeignLiveBytes);
	fprintf(pFile, "- live_object_count: %llu\n", (unsigned long long)xCore.MemDebug.iLiveObjectCount);
	fprintf(pFile, "- temp_current_bytes: %llu\n", (unsigned long long)xCore.MemDebug.iTempCurrentBytes);
	fprintf(pFile, "- temp_peak_bytes: %llu\n", (unsigned long long)xCore.MemDebug.iTempPeakBytes);
	fprintf(pFile, "- temp_reset_count: %llu\n", (unsigned long long)xCore.MemDebug.iTempResetCount);
	fprintf(pFile, "- invalid_free_count: %llu\n", (unsigned long long)xCore.MemDebug.iInvalidFreeCount);
	fprintf(pFile, "- double_free_count: %llu\n", (unsigned long long)xCore.MemDebug.iDoubleFreeCount);
	fprintf(pFile, "- wrong_allocator_free_count: %llu\n", (unsigned long long)xCore.MemDebug.iWrongAllocatorFreeCount);
	fprintf(pFile, "- object_double_destroy_count: %llu\n", (unsigned long long)xCore.MemDebug.iObjectDoubleDestroyCount);
	fprintf(pFile, "- overflow_count: %llu\n", (unsigned long long)xCore.MemDebug.iOverflowCount);
	fprintf(pFile, "- underflow_count: %llu\n\n", (unsigned long long)xCore.MemDebug.iUnderflowCount);

	fprintf(pFile, "## Live Allocations\n");
	for ( pHeader = xCore.MemDebug.pLiveHead; pHeader; pHeader = pHeader->pDebugNext ) {
		fprintf(pFile,
			"- LEAK ptr=%p size=%u alloc=%s:%u thread=%llu time_ms=%llu flags=0x%04x\n",
			__xrtMemGlobalUserFromHeader(pHeader),
			pHeader->iRequestSize,
			pHeader->sAllocFile ? pHeader->sAllocFile : "(unknown)",
			pHeader->iAllocLine,
			(unsigned long long)pHeader->iAllocThreadId,
			(unsigned long long)pHeader->iAllocTimeMs,
			(unsigned int)pHeader->iFlags);
	}

	fprintf(pFile, "\n## Foreign Live Allocations\n");
	for ( pForeign = xCore.MemDebug.pForeignAllocs; pForeign; pForeign = pForeign->pNext ) {
		fprintf(pFile,
			"- POOL_LEAK ptr=%p size=%u allocator=%s alloc=%s:%u thread=%llu time_ms=%llu\n",
			pForeign->pAddress,
			pForeign->iSize,
			__xrtMemDebugAllocatorName(pForeign->iAllocatorKind),
			pForeign->sAllocFile ? pForeign->sAllocFile : "(unknown)",
			pForeign->iAllocLine,
			(unsigned long long)pForeign->iAllocThreadId,
			(unsigned long long)pForeign->iAllocTimeMs);
	}

	fprintf(pFile, "\n## Live Objects\n");
	for ( pObject = xCore.MemDebug.pObjects; pObject; pObject = pObject->pNext ) {
		if ( pObject->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
			continue;
		}
		fprintf(pFile,
			"- OBJECT_LEAK ptr=%p object=%s origin=%s alloc=%s:%u thread=%llu time_ms=%llu\n",
			pObject->pAddress,
			__xrtMemDebugObjectTypeName(pObject->iObjectType),
			__xrtMemDebugObjectOriginName(pObject->iOrigin),
			pObject->sAllocFile ? pObject->sAllocFile : "(unknown)",
			pObject->iAllocLine,
			(unsigned long long)pObject->iAllocThreadId,
			(unsigned long long)pObject->iAllocTimeMs);
	}

	fprintf(pFile, "\n## Site Stats\n");
	for ( pSite = xCore.MemDebug.pSiteStats; pSite; pSite = pSite->pNext ) {
		fprintf(pFile,
			"- site=%s:%u allocator=%s alloc_count=%llu alloc_bytes=%llu free_count=%llu free_bytes=%llu live_count=%llu live_bytes=%llu peak_live_count=%llu peak_live_bytes=%llu\n",
			pSite->sFile ? pSite->sFile : "(unknown)",
			pSite->iLine,
			__xrtMemDebugAllocatorName(pSite->iAllocatorKind),
			(unsigned long long)pSite->iAllocCount,
			(unsigned long long)pSite->iAllocBytes,
			(unsigned long long)pSite->iFreeCount,
			(unsigned long long)pSite->iFreeBytes,
			(unsigned long long)pSite->iLiveCount,
			(unsigned long long)pSite->iLiveBytes,
			(unsigned long long)pSite->iPeakLiveCount,
			(unsigned long long)pSite->iPeakLiveBytes);
	}

	fprintf(pFile, "\n## Recent Events\n");
	iCount = xCore.MemDebug.iEventCount;
	if ( iCount > 0 ) {
		uint32 iStart = (xCore.MemDebug.iEventCursor + XRT_MEMDEBUG_EVENT_CAPACITY - iCount) % XRT_MEMDEBUG_EVENT_CAPACITY;
		for ( uint32 i = 0; i < iCount; i++ ) {
			xrtMemDebugEvent* pEvent = &xCore.MemDebug.arrEvents[(iStart + i) % XRT_MEMDEBUG_EVENT_CAPACITY];
			fprintf(pFile,
				"- %s ptr=%p size=%llu kind=%s file=%s:%u thread=%llu time_ms=%llu\n",
				__xrtMemDebugEventName(pEvent->iType),
				pEvent->pAddress,
				(unsigned long long)pEvent->iSize,
				__xrtMemDebugEventUsesObjectType(pEvent->iType)
					? __xrtMemDebugObjectTypeName(pEvent->iAllocatorKind)
					: __xrtMemDebugAllocatorName(pEvent->iAllocatorKind),
				pEvent->sFile ? pEvent->sFile : "(unknown)",
				pEvent->iLine,
				(unsigned long long)pEvent->iThreadId,
				(unsigned long long)pEvent->iTimeMs);
		}
	}
	__xrtMemDebugUnlock();
	return TRUE;
}


// 将当前内存调试状态导出为 JSON 报告
static bool __xrtMemDebugDumpJsonFile(FILE* pFile)
{
	xrtMemBlockHeader* pHeader;
	xrtMemDebugForeignAlloc* pForeign;
	xrtMemDebugObject* pObject;
	xrtMemDebugSiteStat* pSite;
	uint32 iCount;
	bool bFirst;
	bool bHasIssues;

	if ( pFile == NULL ) {
		return FALSE;
	}

	__xrtMemDebugLock();
	bHasIssues = __xrtMemDebugHasIssues();
	fputs("{\n", pFile);
	fputs("  \"status\": ", pFile);
	__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugReportStatusName(bHasIssues));
	fputs(",\n", pFile);
	fprintf(pFile, "  \"has_issues\": %s,\n", bHasIssues ? "true" : "false");
	fputs("  \"summary\": ", pFile);
	__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugReportSummary(bHasIssues));
	fputs(",\n", pFile);
	fprintf(pFile, "  \"live_alloc_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iLiveAllocCount);
	fprintf(pFile, "  \"live_alloc_bytes\": %llu,\n", (unsigned long long)xCore.MemDebug.iLiveAllocBytes);
	fprintf(pFile, "  \"foreign_live_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iForeignLiveCount);
	fprintf(pFile, "  \"foreign_live_bytes\": %llu,\n", (unsigned long long)xCore.MemDebug.iForeignLiveBytes);
	fprintf(pFile, "  \"live_object_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iLiveObjectCount);
	fprintf(pFile, "  \"temp_current_bytes\": %llu,\n", (unsigned long long)xCore.MemDebug.iTempCurrentBytes);
	fprintf(pFile, "  \"temp_peak_bytes\": %llu,\n", (unsigned long long)xCore.MemDebug.iTempPeakBytes);
	fprintf(pFile, "  \"temp_reset_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iTempResetCount);
	fprintf(pFile, "  \"invalid_free_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iInvalidFreeCount);
	fprintf(pFile, "  \"double_free_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iDoubleFreeCount);
	fprintf(pFile, "  \"wrong_allocator_free_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iWrongAllocatorFreeCount);
	fprintf(pFile, "  \"object_double_destroy_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iObjectDoubleDestroyCount);
	fprintf(pFile, "  \"overflow_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iOverflowCount);
	fprintf(pFile, "  \"underflow_count\": %llu,\n", (unsigned long long)xCore.MemDebug.iUnderflowCount);

	fputs("  \"live_allocations\": [\n", pFile);
	bFirst = TRUE;
	for ( pHeader = xCore.MemDebug.pLiveHead; pHeader; pHeader = pHeader->pDebugNext ) {
		if ( !bFirst ) {
			fputs(",\n", pFile);
		}
		fputs("    {\"type\":\"LEAK\",\"ptr\":\"", pFile);
		fprintf(pFile, "%p", __xrtMemGlobalUserFromHeader(pHeader));
		fputs("\",\"size\":", pFile);
		fprintf(pFile, "%u", pHeader->iRequestSize);
		fputs(",\"file\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, pHeader->sAllocFile ? pHeader->sAllocFile : "(unknown)");
		fprintf(pFile, ",\"line\":%u,\"thread\":%llu,\"time_ms\":%llu}", pHeader->iAllocLine, (unsigned long long)pHeader->iAllocThreadId, (unsigned long long)pHeader->iAllocTimeMs);
		bFirst = FALSE;
	}
	fputs("\n  ],\n", pFile);

	fputs("  \"foreign_live_allocations\": [\n", pFile);
	bFirst = TRUE;
	for ( pForeign = xCore.MemDebug.pForeignAllocs; pForeign; pForeign = pForeign->pNext ) {
		if ( !bFirst ) {
			fputs(",\n", pFile);
		}
		fputs("    {\"type\":\"POOL_LEAK\",\"ptr\":\"", pFile);
		fprintf(pFile, "%p", pForeign->pAddress);
		fputs("\",\"size\":", pFile);
		fprintf(pFile, "%u", pForeign->iSize);
		fputs(",\"allocator\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugAllocatorName(pForeign->iAllocatorKind));
		fputs(",\"file\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, pForeign->sAllocFile ? pForeign->sAllocFile : "(unknown)");
		fprintf(pFile, ",\"line\":%u,\"thread\":%llu,\"time_ms\":%llu}", pForeign->iAllocLine, (unsigned long long)pForeign->iAllocThreadId, (unsigned long long)pForeign->iAllocTimeMs);
		bFirst = FALSE;
	}
	fputs("\n  ],\n", pFile);

	fputs("  \"live_objects\": [\n", pFile);
	bFirst = TRUE;
	for ( pObject = xCore.MemDebug.pObjects; pObject; pObject = pObject->pNext ) {
		if ( pObject->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
			continue;
		}
		if ( !bFirst ) {
			fputs(",\n", pFile);
		}
		fputs("    {\"type\":\"OBJECT_LEAK\",\"ptr\":\"", pFile);
		fprintf(pFile, "%p", pObject->pAddress);
		fputs("\",\"object\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugObjectTypeName(pObject->iObjectType));
		fputs(",\"origin\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugObjectOriginName(pObject->iOrigin));
		fputs(",\"file\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, pObject->sAllocFile ? pObject->sAllocFile : "(unknown)");
		fprintf(pFile, ",\"line\":%u,\"thread\":%llu,\"time_ms\":%llu}",
			pObject->iAllocLine,
			(unsigned long long)pObject->iAllocThreadId,
			(unsigned long long)pObject->iAllocTimeMs);
		bFirst = FALSE;
	}
	fputs("\n  ],\n", pFile);

	fputs("  \"site_stats\": [\n", pFile);
	bFirst = TRUE;
	for ( pSite = xCore.MemDebug.pSiteStats; pSite; pSite = pSite->pNext ) {
		if ( !bFirst ) {
			fputs(",\n", pFile);
		}
		fputs("    {\"file\":", pFile);
		__xrtMemDebugJsonWriteString(pFile, pSite->sFile ? pSite->sFile : "(unknown)");
		fprintf(pFile, ",\"line\":%u,\"allocator\":", pSite->iLine);
		__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugAllocatorName(pSite->iAllocatorKind));
		fprintf(pFile, ",\"alloc_count\":%llu,\"alloc_bytes\":%llu,\"free_count\":%llu,\"free_bytes\":%llu,\"live_count\":%llu,\"live_bytes\":%llu,\"peak_live_count\":%llu,\"peak_live_bytes\":%llu}",
			(unsigned long long)pSite->iAllocCount,
			(unsigned long long)pSite->iAllocBytes,
			(unsigned long long)pSite->iFreeCount,
			(unsigned long long)pSite->iFreeBytes,
			(unsigned long long)pSite->iLiveCount,
			(unsigned long long)pSite->iLiveBytes,
			(unsigned long long)pSite->iPeakLiveCount,
			(unsigned long long)pSite->iPeakLiveBytes);
		bFirst = FALSE;
	}
	fputs("\n  ],\n", pFile);

	fputs("  \"events\": [\n", pFile);
	bFirst = TRUE;
	iCount = xCore.MemDebug.iEventCount;
	if ( iCount > 0 ) {
		uint32 iStart = (xCore.MemDebug.iEventCursor + XRT_MEMDEBUG_EVENT_CAPACITY - iCount) % XRT_MEMDEBUG_EVENT_CAPACITY;
		for ( uint32 i = 0; i < iCount; i++ ) {
			xrtMemDebugEvent* pEvent = &xCore.MemDebug.arrEvents[(iStart + i) % XRT_MEMDEBUG_EVENT_CAPACITY];
			if ( !bFirst ) {
				fputs(",\n", pFile);
			}
			fputs("    {\"type\":", pFile);
			__xrtMemDebugJsonWriteString(pFile, __xrtMemDebugEventName(pEvent->iType));
			fputs(",\"ptr\":\"", pFile);
			fprintf(pFile, "%p", pEvent->pAddress);
			fputs("\",\"size\":", pFile);
			fprintf(pFile, "%llu", (unsigned long long)pEvent->iSize);
			fputs(",\"kind\":", pFile);
			__xrtMemDebugJsonWriteString(pFile,
				__xrtMemDebugEventUsesObjectType(pEvent->iType)
					? __xrtMemDebugObjectTypeName(pEvent->iAllocatorKind)
					: __xrtMemDebugAllocatorName(pEvent->iAllocatorKind));
			fputs(",\"file\":", pFile);
			__xrtMemDebugJsonWriteString(pFile, pEvent->sFile ? pEvent->sFile : "(unknown)");
			fprintf(pFile, ",\"line\":%u,\"thread\":%llu,\"time_ms\":%llu}", pEvent->iLine, (unsigned long long)pEvent->iThreadId, (unsigned long long)pEvent->iTimeMs);
			bFirst = FALSE;
		}
	}
	fputs("\n  ]\n}\n", pFile);
	__xrtMemDebugUnlock();
	return TRUE;
}
#endif



// 启用内存遥测
XXAPI void xrtMemTelemetryEnable(bool bEnable)
{
	xCore.MemTelemetry.bEnabled = bEnable ? 1 : 0;
}



// 判断内存遥测是否启用
XXAPI bool xrtMemTelemetryIsEnabled()
{
	return xCore.MemTelemetry.bEnabled != 0;
}



// 重置内存遥测
XXAPI void xrtMemTelemetryReset()
{
	__xrtRuntimeLock();
	__xrtResetMemTelemetryState(&xCore.MemTelemetry);
	__xrtRuntimeUnlock();
}



// 获取内存遥测快照
XXAPI void xrtMemTelemetryGetSnapshot(xrtMemTelemetrySnapshot* pOut)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	uint32 i;

	if ( pOut == NULL ) {
		return;
	}

	memset(pOut, 0, sizeof(xrtMemTelemetrySnapshot));
	pOut->bEnabled = pState->bEnabled != 0;
	pOut->iClassStep = XRT_MEMPOOL_STEP_SIZE;
	pOut->iClassCutoff = XRT_MEMPOOL_CUTOFF_DEFAULT;
	pOut->iClassCount = XRT_MEMPOOL_CLASS_COUNT_DEFAULT;
	pOut->iMallocCalls = (uint64)pState->iMallocCalls;
	pOut->iMallocBytes = (uint64)pState->iMallocBytes;
	pOut->iCallocCalls = (uint64)pState->iCallocCalls;
	pOut->iCallocBytes = (uint64)pState->iCallocBytes;
	pOut->iReallocCalls = (uint64)pState->iReallocCalls;
	pOut->iReallocBytes = (uint64)pState->iReallocBytes;
	pOut->iFreeCalls = (uint64)pState->iFreeCalls;
	pOut->iTempCalls = (uint64)pState->iTempCalls;
	pOut->iTempBytes = (uint64)pState->iTempBytes;
	pOut->iPooledCandidateCalls = (uint64)pState->iPooledCandidateCalls;
	pOut->iPooledCandidateBytes = (uint64)pState->iPooledCandidateBytes;
	pOut->iFallbackCalls = (uint64)pState->iFallbackCalls;
	pOut->iFallbackBytes = (uint64)pState->iFallbackBytes;
	for ( i = 0; i < XRT_MEMPOOL_CLASS_COUNT_DEFAULT; i++ ) {
		pOut->arrClassCalls[i] = (uint64)pState->arrClassCalls[i];
		pOut->arrClassBytes[i] = (uint64)pState->arrClassBytes[i];
	}
}



#ifdef XRT_MEM_DEBUG
// 启用内存调试
XXAPI void xrtMemDebugEnable(bool bEnable)
{
	xCore.MemDebug.bEnabled = bEnable ? 1 : 0;
}



// 判断内存调试是否启用
XXAPI bool xrtMemDebugIsEnabled()
{
	return xCore.MemDebug.bEnabled != 0;
}



// 重置内存调试
XXAPI void xrtMemDebugReset()
{
	__xrtRuntimeLock();
	__xrtMemDebugResetState(&xCore.MemDebug);
	__xrtRuntimeUnlock();
}



// 导出内存调试文本
XXAPI bool xrtMemDebugDumpText(str sPath)
{
	const char* sOpenPath = sPath ? (const char*)sPath : "xrt_mem_report.txt";
	FILE* pFile = fopen(sOpenPath, "wb");
	bool bRet;
	if ( pFile == NULL ) {
		return FALSE;
	}
	bRet = __xrtMemDebugDumpTextFile(pFile);
	fclose(pFile);
	return bRet;
}



// 导出内存调试 JSON
XXAPI bool xrtMemDebugDumpJson(str sPath)
{
	const char* sOpenPath = sPath ? (const char*)sPath : "xrt_mem_report.json";
	FILE* pFile = fopen(sOpenPath, "wb");
	bool bRet;
	if ( pFile == NULL ) {
		return FALSE;
	}
	bRet = __xrtMemDebugDumpJsonFile(pFile);
	fclose(pFile);
	return bRet;
}
#endif



// 压入线程退出清理回调
XXAPI bool xrtThreadPushCleanup(xrtThreadCleanupProc proc, ptr pArg)
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();
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



// 弹出线程退出清理回调
XXAPI bool xrtThreadPopCleanup(xrtThreadCleanupProc proc, ptr pArg)
{
	xrtThreadData* pThreadData = __xrtThreadStateGet();
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



// 结束托管线程并处理自动销毁
static void __xrtThreadExitManaged(struct xthread_struct* pThread, uint32 iExitCode)
{
	if ( pThread ) {
		#if !defined(_WIN32) && !defined(_WIN64)
			pthread_mutex_lock(&pThread->FinishLock);
		#endif
		pThread->ExitCode = iExitCode;
		pThread->bFinished = TRUE;
		#if !defined(_WIN32) && !defined(_WIN64)
			pthread_cond_broadcast(&pThread->FinishCond);
			pthread_mutex_unlock(&pThread->FinishLock);
		#endif
	}

	xrtThreadDetachCurrent();

	if ( pThread && pThread->bAutoDestroy ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( pThread->Handle ) {
				CloseHandle(pThread->Handle);
				pThread->Handle = NULL;
			}
		#else
			if ( pThread->Handle ) {
				pthread_detach((pthread_t)pThread->Handle);
				pThread->Handle = NULL;
			}
			pthread_cond_destroy(&pThread->FinishCond);
			pthread_mutex_destroy(&pThread->FinishLock);
		#endif
		__xrtThreadObjFree(pThread);
	}
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
		__xrtMemGlobalInitPlan(&xCore.MemGlobal);
		__xrtResetMemTelemetryState(&xCore.MemTelemetry);
		#ifdef XRT_MEM_DEBUG
			__xrtMemDebugResetState(&xCore.MemDebug);
		#endif

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
			ssize_t iSize = -1;
			#if defined(__APPLE__) && defined(__MACH__)
				uint32_t iBufSize = 4096u;
				if ( sTemp && _NSGetExecutablePath((char*)sTemp, &iBufSize) == 0 ) {
					iSize = (ssize_t)strlen((const char*)sTemp);
				}
			#else
				iSize = readlink("/proc/self/exe", sTemp, 4096);
			#endif
			if ( iSize >= 0 ) {
				xCore.AppFile = xrtCopyStr(sTemp, (size_t)iSize);
				xCore.AppPath = xrtPathGetDir(xCore.AppFile, (size_t)iSize);
			} else {
				xCore.AppFile = xCore.sNull;
				xCore.AppPath = xCore.sNull;
			}
			free(sTemp);
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

	if ( (xCore.iInitRef == 0) && (__xrtRuntimeThreadRefCount == 0) && xCore.bInit ) {
		__xrtRuntimeFinalizeLocked();
	}

	__xrtRuntimeUnlock();
}


#ifdef DBUILD_DLL
	
	
	
	#if defined(_WIN32) || defined(_WIN64)
		BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
		{
			if ( fdwReason == DLL_PROCESS_ATTACH ) {
				// 当进程加载 DLL 时调用 DllMain
				xrtInit();
			} else if ( fdwReason == DLL_PROCESS_DETACH ) {
				// 当进程卸载 DLL 时调用 DllMain
				xrtUnit();
			}
			return (TRUE);
		}
	#else
	#endif
	
	
	
#endif

#if defined(XRT_MEM_DEBUG)
	#undef XRT_BUILD_CORE
	#include "lib/memdebug_site_macros_public.h"
#endif
