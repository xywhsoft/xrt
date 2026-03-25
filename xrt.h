


/*
	
	MIT License
	
	Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
	
*/



#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>



// 跨平台头文件
#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/winsock2.h>
		#include <winapi/ws2tcpip.h>
		#include <winapi/mswsock.h>
		#ifndef XRT_THREAD_INIT
			#define XRT_THREAD_INIT
			typedef struct { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
			typedef struct { PVOID Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
			#ifndef SRWLOCK_INIT
				#define SRWLOCK_INIT { 0 }
			#endif
			
			ULONGLONG GetTickCount64();
			
			void WINAPI InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			void WINAPI WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			void WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
			BOOL WINAPI SleepConditionVariableCS(
				PCONDITION_VARIABLE ConditionVariable,
				PCRITICAL_SECTION CriticalSection,
				DWORD dwMilliseconds
			);
			BOOL WINAPI SleepConditionVariableSRW(
				PCONDITION_VARIABLE ConditionVariable,
				PSRWLOCK SRWLock,
				DWORD dwMilliseconds,
				ULONG Flags
			);
			
			void WINAPI InitializeSRWLock(PSRWLOCK SRWLock);
			void WINAPI AcquireSRWLockExclusive(PSRWLOCK SRWLock);
			void WINAPI ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
			void WINAPI AcquireSRWLockShared(PSRWLOCK SRWLock);
			void WINAPI ReleaseSRWLockShared(PSRWLOCK SRWLock);
			BOOL WINAPI TryAcquireSRWLockExclusive(PSRWLOCK SRWLock);
			BOOL WINAPI TryAcquireSRWLockShared(PSRWLOCK SRWLock);
			
			/* IOCP 批量收割支持 */
			typedef struct _OVERLAPPED_ENTRY {
				ULONG_PTR lpCompletionKey;
				LPOVERLAPPED lpOverlapped;
				ULONG_PTR Internal;
				DWORD dwNumberOfBytesTransferred;
			} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;
			
			BOOL WINAPI GetQueuedCompletionStatusEx(
				HANDLE CompletionPort,
				LPOVERLAPPED_ENTRY lpCompletionPortEntries,
				ULONG ulCount,
				PULONG ulNumEntriesRemoved,
				DWORD dwMilliseconds,
				BOOL fAlertable
			);
			
			/* AcceptEx 支持 */
			typedef BOOL (WINAPI *LPFN_ACCEPTEX)(
				SOCKET sListenSocket,
				SOCKET sAcceptSocket,
				PVOID lpOutputBuffer,
				DWORD dwReceiveDataLength,
				DWORD dwLocalAddressLength,
				DWORD dwRemoteAddressLength,
				LPDWORD lpdwBytesReceived,
				LPOVERLAPPED lpOverlapped
			);
			
			typedef void (WINAPI *LPFN_GETACCEPTEXSOCKADDRS)(
				PVOID lpOutputBuffer,
				DWORD dwReceiveDataLength,
				DWORD dwLocalAddressLength,
				DWORD dwRemoteAddressLength,
				struct sockaddr **LocalSockaddr,
				LPINT LocalSockaddrLength,
				struct sockaddr **RemoteSockaddr,
				LPINT RemoteSockaddrLength
			);
			
			#define WSAID_ACCEPTEX \
				{0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
			#define WSAID_GETACCEPTEXSOCKADDRS \
				{0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}
			
			#ifndef SIO_GET_EXTENSION_FUNCTION_POINTER
				#define SIO_GET_EXTENSION_FUNCTION_POINTER 0xC8000006
			#endif
			
			#ifndef SO_UPDATE_ACCEPT_CONTEXT
				#define SO_UPDATE_ACCEPT_CONTEXT 0x700B
			#endif
			
			#ifndef SIO_KEEPALIVE_VALS
				#define SIO_KEEPALIVE_VALS 0x98000004
			#endif
			
			struct tcp_keepalive {
				ULONG onoff;
				ULONG keepalivetime;
				ULONG keepaliveinterval;
			};
			
			const char* WSAAPI inet_ntop(int af, const void* src, char* dst, int size);
			#define InetNtopA inet_ntop
					
			BOOL WINAPI CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);
			BOOL WINAPI IsThreadAFiber(void);
		#endif
	#else
		#include <winsock2.h>
		#include <ws2tcpip.h>
		#include <mswsock.h>
	#endif
	
	#include <windows.h>
#else
	#include <pthread.h>
	#include <semaphore.h>
	#include <signal.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/mman.h>
#endif





// ========================================
// XRT 模块裁剪支持
// ========================================

// 基础功能组
#if defined(XRT_MINIMAL)
	#define XRT_NO_TIME
	#define XRT_NO_FILE
	#define XRT_NO_THREAD
	#define XRT_NO_QUEUE
	#define XRT_NO_COROUTINE
	#define XRT_NO_NETWORK
	#define XRT_NO_CRYPTO
	#define XRT_NO_NETTLS
	#define XRT_NO_XID
	#define XRT_NO_BUFFER
	#define XRT_NO_ARRAY
	#define XRT_NO_BSMN
	#define XRT_NO_MEMUNIT
	#define XRT_NO_MEMPOOL_FS
	#define XRT_NO_STACK
	#define XRT_NO_AVLTREE
	#define XRT_NO_MEMPOOL
	#define XRT_NO_DICT
	#define XRT_NO_LIST
	#define XRT_NO_VALUE
	#define XRT_NO_JNUM
	#define XRT_NO_JSON
	#define XRT_NO_TEMPLATE
	#define XRT_NO_REGEX		// 禁用正则表达式模块
#endif

// 网络根模块裁剪时，同步裁剪全部网络子库
#if defined(XRT_NO_NETWORK)
	#ifndef XRT_NO_XURL
		#define XRT_NO_XURL
	#endif
	#ifndef XRT_NO_HTTP_UTIL
		#define XRT_NO_HTTP_UTIL
	#endif
	#ifndef XRT_NO_XCODEC
		#define XRT_NO_XCODEC
	#endif
	#ifndef XRT_NO_XHTTP
		#define XRT_NO_XHTTP
	#endif
	#ifndef XRT_NO_XHTTPD
		#define XRT_NO_XHTTPD
	#endif
	#ifndef XRT_NO_XWS
		#define XRT_NO_XWS
	#endif
#endif

#if defined(XRT_NO_QUEUE)
	#ifndef XRT_NO_QUEUE_WAIT
		#define XRT_NO_QUEUE_WAIT
	#endif
#endif

// 裁剪依赖警告辅助
#if defined(_MSC_VER)
	#define XRT_CUT_WARN_STR2(x) #x
	#define XRT_CUT_WARN_STR(x) XRT_CUT_WARN_STR2(x)
	#define XRT_CUT_WARN(sText) __pragma(message(__FILE__ "(" XRT_CUT_WARN_STR(__LINE__) "): warning: " sText))
#else
	#define XRT_CUT_WARN(sText)
#endif

#if defined(XRT_NO_XURL) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_XURL ignored because XHTTP/XHTTPD/XWS require XURL."
	#else
		XRT_CUT_WARN("XRT_NO_XURL ignored because XHTTP/XHTTPD/XWS require XURL.")
	#endif
	#undef XRT_NO_XURL
#endif

#if defined(XRT_NO_HTTP_UTIL) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_HTTP_UTIL ignored because XHTTP/XHTTPD/XWS require HTTP_UTIL."
	#else
		XRT_CUT_WARN("XRT_NO_HTTP_UTIL ignored because XHTTP/XHTTPD/XWS require HTTP_UTIL.")
	#endif
	#undef XRT_NO_HTTP_UTIL
#endif

#if defined(XRT_NO_XCODEC) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_XCODEC ignored because XHTTP/XHTTPD/XWS require XCODEC."
	#else
		XRT_CUT_WARN("XRT_NO_XCODEC ignored because XHTTP/XHTTPD/XWS require XCODEC.")
	#endif
	#undef XRT_NO_XCODEC
#endif

#if defined(XRT_NO_CRYPTO) && (!defined(XRT_NO_NETTLS) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_CRYPTO ignored because NETTLS/XWS require CRYPTO."
	#else
		XRT_CUT_WARN("XRT_NO_CRYPTO ignored because NETTLS/XWS require CRYPTO.")
	#endif
	#undef XRT_NO_CRYPTO
#endif

#if defined(XRT_NO_FILE) && (!defined(XRT_NO_JSON) || !defined(XRT_NO_NETTLS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_FILE ignored because JSON/NETTLS require FILE."
	#else
		XRT_CUT_WARN("XRT_NO_FILE ignored because JSON/NETTLS require FILE.")
	#endif
	#undef XRT_NO_FILE
#endif

// 高层模块依赖
#if defined(XRT_NO_VALUE) && (!defined(XRT_NO_JSON) || !defined(XRT_NO_TEMPLATE))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_VALUE ignored because JSON/TEMPLATE require VALUE."
	#else
		XRT_CUT_WARN("XRT_NO_VALUE ignored because JSON/TEMPLATE require VALUE.")
	#endif
	#undef XRT_NO_VALUE
#endif

#if defined(XRT_NO_JNUM) && (!defined(XRT_NO_JSON) || !defined(XRT_NO_TEMPLATE))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_JNUM ignored because JSON/TEMPLATE require JNUM."
	#else
		XRT_CUT_WARN("XRT_NO_JNUM ignored because JSON/TEMPLATE require JNUM.")
	#endif
	#undef XRT_NO_JNUM
#endif

#if defined(XRT_NO_STACK) && !defined(XRT_NO_JSON)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_STACK ignored because JSON requires STACK."
	#else
		XRT_CUT_WARN("XRT_NO_STACK ignored because JSON requires STACK.")
	#endif
	#undef XRT_NO_STACK
#endif

#if defined(XRT_NO_DICT) && (!defined(XRT_NO_VALUE) || !defined(XRT_NO_TEMPLATE))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_DICT ignored because VALUE/TEMPLATE require DICT."
	#else
		XRT_CUT_WARN("XRT_NO_DICT ignored because VALUE/TEMPLATE require DICT.")
	#endif
	#undef XRT_NO_DICT
#endif

// 容器与内存依赖
#if defined(XRT_NO_LIST) && !defined(XRT_NO_VALUE)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_LIST ignored because VALUE requires LIST."
	#else
		XRT_CUT_WARN("XRT_NO_LIST ignored because VALUE requires LIST.")
	#endif
	#undef XRT_NO_LIST
#endif

#if defined(XRT_NO_AVLTREE) && (!defined(XRT_NO_DICT) || !defined(XRT_NO_LIST) || !defined(XRT_NO_VALUE))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_AVLTREE ignored because DICT/LIST/VALUE require AVLTREE."
	#else
		XRT_CUT_WARN("XRT_NO_AVLTREE ignored because DICT/LIST/VALUE require AVLTREE.")
	#endif
	#undef XRT_NO_AVLTREE
#endif

#if defined(XRT_NO_MEMPOOL) && !defined(XRT_NO_DICT)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_MEMPOOL ignored because DICT requires MEMPOOL."
	#else
		XRT_CUT_WARN("XRT_NO_MEMPOOL ignored because DICT requires MEMPOOL.")
	#endif
	#undef XRT_NO_MEMPOOL
#endif

#if defined(XRT_NO_MEMPOOL_FS) && !defined(XRT_NO_AVLTREE)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_MEMPOOL_FS ignored because AVLTREE requires MEMPOOL_FS."
	#else
		XRT_CUT_WARN("XRT_NO_MEMPOOL_FS ignored because AVLTREE requires MEMPOOL_FS.")
	#endif
	#undef XRT_NO_MEMPOOL_FS
#endif

#if defined(XRT_NO_BSMN) && (!defined(XRT_NO_MEMPOOL) || !defined(XRT_NO_MEMPOOL_FS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_BSMN ignored because MEMPOOL/MEMPOOL_FS require BSMN."
	#else
		XRT_CUT_WARN("XRT_NO_BSMN ignored because MEMPOOL/MEMPOOL_FS require BSMN.")
	#endif
	#undef XRT_NO_BSMN
#endif

#if defined(XRT_NO_MEMUNIT) && (!defined(XRT_NO_MEMPOOL) || !defined(XRT_NO_MEMPOOL_FS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_MEMUNIT ignored because MEMPOOL/MEMPOOL_FS require MEMUNIT."
	#else
		XRT_CUT_WARN("XRT_NO_MEMUNIT ignored because MEMPOOL/MEMPOOL_FS require MEMUNIT.")
	#endif
	#undef XRT_NO_MEMUNIT
#endif

#if defined(XRT_NO_ARRAY) && (!defined(XRT_NO_BSMN) || !defined(XRT_NO_STACK) || !defined(XRT_NO_VALUE) || !defined(XRT_NO_TEMPLATE))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_ARRAY ignored because BSMN/STACK/VALUE/TEMPLATE require ARRAY."
	#else
		XRT_CUT_WARN("XRT_NO_ARRAY ignored because BSMN/STACK/VALUE/TEMPLATE require ARRAY.")
	#endif
	#undef XRT_NO_ARRAY
#endif

// 线程与时间依赖
#if defined(XRT_NO_THREAD)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_THREAD ignored because current runtime core requires THREAD support."
	#else
		XRT_CUT_WARN("XRT_NO_THREAD ignored because current runtime core requires THREAD support.")
	#endif
	#undef XRT_NO_THREAD
#endif

#if defined(XRT_NO_QUEUE) && !defined(XRT_NO_NETWORK)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_QUEUE ignored because current XNET runtime requires QUEUE support."
	#else
		XRT_CUT_WARN("XRT_NO_QUEUE ignored because current XNET runtime requires QUEUE support.")
	#endif
	#undef XRT_NO_QUEUE
#endif

#if defined(XRT_NO_TIME) && (!defined(XRT_NO_VALUE) || !defined(XRT_NO_TEMPLATE) || !defined(XRT_NO_XID))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_TIME ignored because VALUE/TEMPLATE/XID require TIME."
	#else
		XRT_CUT_WARN("XRT_NO_TIME ignored because VALUE/TEMPLATE/XID require TIME.")
	#endif
	#undef XRT_NO_TIME
#endif

#undef XRT_CUT_WARN
#if defined(_MSC_VER)
	#undef XRT_CUT_WARN_STR
	#undef XRT_CUT_WARN_STR2
#endif





// ========================================
// XRT 头文件声明
// ========================================

#ifndef XXRTL_CORE
	#define XXRTL_CORE
	
	
	
	// basic type define
	typedef unsigned char* u8str;
	typedef unsigned short* u16str;
	typedef unsigned int* u32str;
	typedef unsigned char* binary;
	typedef u8str str;
	
	typedef char i8;
	typedef char int8;
	typedef unsigned char u8;
	typedef unsigned char uint8;
	typedef short i16;
	typedef short int16;
	typedef unsigned short u16;
	typedef unsigned short uint16;
	typedef unsigned int uint;
	typedef int i32;
	typedef int int32;
	typedef unsigned int u32;
	typedef unsigned int uint32;
	typedef long long i64;
	typedef long long int64;
	typedef unsigned long long u64;
	typedef unsigned long long uint64;
	// long = auto 32 / 64 bit integer
	typedef unsigned long ulong;
	
	typedef float f32;
	typedef float float32;
	typedef double f64;
	typedef double float64;
	
	typedef long long curr;
	
	typedef void* ptr;
	typedef intptr_t intptr;
	typedef uintptr_t uintptr;
	
	typedef int64 xtime;
	
	/*
	#ifndef bool
		typedef int bool;
	#endif
	#ifndef true
		#define true -1
	#endif
	#ifndef false
		#define false 0
	#endif
	*/
	#ifndef TRUE
		#define TRUE 1
	#endif
	#ifndef FALSE
		#define FALSE 0
	#endif
	#ifndef null
		#define null 0
	#endif
	
	#ifdef BUILD_DLL
		#define XXAPI	__declspec(dllexport)
	#else
		#define XXAPI
	#endif
	
	
	
	/* ------------------------------------ 全局数据 ------------------------------------ */
	
	// PCG 随机数状态结构
	typedef struct {
		uint64 state;
		uint64 inc;
	} xrand;

	#define XRT_TEMP_ARENA_BLOCK_SIZE	4096u
	#define XRT_TEMP_ARENA_SPILL_CUTOFF	2048u

	typedef struct xrtThreadData xrtThreadData;
	typedef struct xrtThreadCleanup xrtThreadCleanup;
	typedef void (*xrtThreadCleanupProc)(xrtThreadData* pThreadData, ptr pArg);

	typedef struct {
		uint64 iOwnerThreadId;
		uint32 iFlags;
		uint32 iReserved;
		ptr pLocalAlloc;
		ptr pSharedAlloc;
		ptr pReserved;
	} xrtThreadMemState;

	typedef struct xrtTempArenaBlock {
		struct xrtTempArenaBlock* pNext;
		uint32 iCapacity;
		uint32 iUsed;
		uint32 iFlags;
		uint32 iReserved;
	} xrtTempArenaBlock;

	typedef struct {
		xrtTempArenaBlock* pBlocks;
		xrtTempArenaBlock* pCurrent;
		xrtTempArenaBlock* pSpill;
		uint32 iBlockSize;
		uint32 iSpillCutoff;
		uint64 iCurrentBytes;
		uint64 iPeakBytes;
		uint64 iResetCount;
	} xrtTempArenaState;

	#define XRT_MEMPOOL_STEP_SIZE			16u
	#define XRT_MEMPOOL_CUTOFF_DEFAULT		1024u
	#define XRT_MEMPOOL_CLASS_COUNT_DEFAULT	(XRT_MEMPOOL_CUTOFF_DEFAULT / XRT_MEMPOOL_STEP_SIZE)
	#define XRT_MEMGLOBAL_CACHE_LIMIT		32u
	#define XRT_MEMBLOCK_MAGIC				0x584D454Du
	#define XRT_MEMGLOBAL_SPAN_TARGET_BYTES	4096u
	#define XRT_MEMGLOBAL_SPAN_MIN_BLOCKS	8u
	#define XRT_MEMGLOBAL_SPAN_MAX_BLOCKS	128u

	#define XRT_MEMBLOCK_FLAG_POOLED		0x0001u
	#define XRT_MEMBLOCK_FLAG_BACKING		0x0002u

	typedef struct xrtMemBlockHeader {
		uint32 iMagic;
		uint16 iClassIndex;
		uint16 iFlags;
		uint32 iRequestSize;
		uint32 iReserved;
		#ifdef XRT_MEM_DEBUG
			const char* sAllocFile;
			uint32 iAllocLine;
			const char* sFreeFile;
			uint32 iFreeLine;
			uint64 iAllocThreadId;
			uint64 iAllocSeq;
			uint64 iAllocTimeMs;
			uint64 iFreeTimeMs;
			struct xrtMemBlockHeader* pDebugPrev;
			struct xrtMemBlockHeader* pDebugNext;
			uint32 iFrontCanary;
			uint32 iDebugState;
		#endif
	} xrtMemBlockHeader;

	typedef struct xrtMemFreeNode {
		struct xrtMemFreeNode* pNext;
	} xrtMemFreeNode;

	typedef struct xrtMemGlobalSpan {
		struct xrtMemGlobalSpan* pNext;
		ptr pMemory;
		uint32 iClassIndex;
		uint32 iBlockCount;
	} xrtMemGlobalSpan;

	typedef struct {
		uint16 iBlockSize;
		uint16 iReserved;
		volatile long iLock;
		xrtMemFreeNode* pFreeList;
		uint32 iFreeCount;
		uint32 iSpanCount;
	} xrtMemGlobalClassDesc;

	typedef struct {
		uint16 iClassCount;
		uint16 iClassStep;
		uint32 iCutoff;
		volatile long iSpanLock;
		xrtMemGlobalSpan* pSpanList;
		xrtMemGlobalClassDesc arrClassDesc[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
		uint8 arrSizeClassLut[XRT_MEMPOOL_CUTOFF_DEFAULT + 1];
	} xrtMemGlobalPool;

	typedef struct {
		ptr arrFreeList[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
		uint16 arrFreeCount[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
		uint16 iClassCount;
		uint16 iCacheLimit;
	} xrtMemThreadCache;

	typedef struct {
		volatile long bEnabled;
		volatile long iReserved0;
		volatile int64 iMallocCalls;
		volatile int64 iMallocBytes;
		volatile int64 iCallocCalls;
		volatile int64 iCallocBytes;
		volatile int64 iReallocCalls;
		volatile int64 iReallocBytes;
		volatile int64 iFreeCalls;
		volatile int64 iTempCalls;
		volatile int64 iTempBytes;
		volatile int64 iPooledCandidateCalls;
		volatile int64 iPooledCandidateBytes;
		volatile int64 iFallbackCalls;
		volatile int64 iFallbackBytes;
		volatile int64 arrClassCalls[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
		volatile int64 arrClassBytes[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
	} xrtMemTelemetryState;

	typedef struct {
		bool bEnabled;
		uint32 iClassStep;
		uint32 iClassCutoff;
		uint32 iClassCount;
		uint64 iMallocCalls;
		uint64 iMallocBytes;
		uint64 iCallocCalls;
		uint64 iCallocBytes;
		uint64 iReallocCalls;
		uint64 iReallocBytes;
		uint64 iFreeCalls;
		uint64 iTempCalls;
		uint64 iTempBytes;
		uint64 iPooledCandidateCalls;
		uint64 iPooledCandidateBytes;
		uint64 iFallbackCalls;
		uint64 iFallbackBytes;
		uint64 arrClassCalls[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
		uint64 arrClassBytes[XRT_MEMPOOL_CLASS_COUNT_DEFAULT];
	} xrtMemTelemetrySnapshot;

	#define XRT_MEMDEBUG_CANARY_HEAD				0x584D4448u
	#define XRT_MEMDEBUG_CANARY_TAIL				0x584D4454u
	#define XRT_MEMDEBUG_EVENT_CAPACITY			512u
	#define XRT_MEMDEBUG_QUARANTINE_LIMIT			256u

	#define XRT_MEMDEBUG_ALLOCATOR_GLOBAL			1u
	#define XRT_MEMDEBUG_ALLOCATOR_MEMPOOL			2u
	#define XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL		3u

	#define XRT_MEMDEBUG_STATE_LIVE				1u
	#define XRT_MEMDEBUG_STATE_FREED			2u
	#define XRT_MEMDEBUG_STATE_QUARANTINE			3u

	#define XRT_MEMDEBUG_OBJECT_STATE_LIVE			1u
	#define XRT_MEMDEBUG_OBJECT_STATE_DESTROYED		2u

	#define XRT_MEMDEBUG_OBJECT_ARRAY			1u
	#define XRT_MEMDEBUG_OBJECT_DICT			2u
	#define XRT_MEMDEBUG_OBJECT_LIST			3u
	#define XRT_MEMDEBUG_OBJECT_AVLTREE			4u
	#define XRT_MEMDEBUG_OBJECT_DYNSTACK			5u
	#define XRT_MEMDEBUG_OBJECT_MEMPOOL			6u
	#define XRT_MEMDEBUG_OBJECT_FSMEMPOOL			7u

	#define XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE		1u
	#define XRT_MEMDEBUG_OBJECT_ORIGIN_INIT			2u

	#define XRT_MEMDEBUG_EVENT_ALLOC				1u
	#define XRT_MEMDEBUG_EVENT_FREE				2u
	#define XRT_MEMDEBUG_EVENT_REALLOC			3u
	#define XRT_MEMDEBUG_EVENT_LEAK				4u
	#define XRT_MEMDEBUG_EVENT_POOL_LEAK			5u
	#define XRT_MEMDEBUG_EVENT_OBJECT_LEAK			6u
	#define XRT_MEMDEBUG_EVENT_DOUBLE_FREE			7u
	#define XRT_MEMDEBUG_EVENT_INVALID_FREE			8u
	#define XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE		9u
	#define XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT	10u
	#define XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT	11u
	#define XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT	12u
	#define XRT_MEMDEBUG_EVENT_OBJECT_CREATE			13u
	#define XRT_MEMDEBUG_EVENT_OBJECT_DESTROY			14u
	#define XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY	15u
	#define XRT_MEMDEBUG_EVENT_TEMP_ALLOC			16u
	#define XRT_MEMDEBUG_EVENT_TEMP_RESET			17u

	typedef struct xrtMemDebugEvent {
		uint32 iType;
		uint32 iLine;
		uint32 iAllocatorKind;
		uint32 iReserved;
		uint64 iThreadId;
		uint64 iTimeMs;
		uint64 iSize;
		ptr pAddress;
		const char* sFile;
	} xrtMemDebugEvent;

	typedef struct xrtMemDebugSiteStat {
		struct xrtMemDebugSiteStat* pNext;
		const char* sFile;
		uint32 iLine;
		uint32 iAllocatorKind;
		uint64 iAllocCount;
		uint64 iAllocBytes;
		uint64 iFreeCount;
		uint64 iFreeBytes;
		uint64 iLiveCount;
		uint64 iLiveBytes;
		uint64 iPeakLiveCount;
		uint64 iPeakLiveBytes;
	} xrtMemDebugSiteStat;

	typedef struct xrtMemDebugForeignAlloc {
		struct xrtMemDebugForeignAlloc* pNext;
		ptr pAddress;
		uint32 iSize;
		uint32 iAllocatorKind;
		const char* sAllocFile;
		uint32 iAllocLine;
		uint64 iAllocThreadId;
		uint64 iAllocTimeMs;
	} xrtMemDebugForeignAlloc;

	typedef struct xrtMemDebugObject {
		struct xrtMemDebugObject* pNext;
		ptr pAddress;
		uint32 iObjectType;
		uint32 iOrigin;
		uint32 iState;
		uint32 iReserved;
		const char* sAllocFile;
		uint32 iAllocLine;
		uint64 iAllocThreadId;
		uint64 iAllocTimeMs;
		const char* sFreeFile;
		uint32 iFreeLine;
		uint64 iFreeThreadId;
		uint64 iFreeTimeMs;
	} xrtMemDebugObject;

	typedef struct {
		volatile long iLock;
		volatile long bEnabled;
		volatile long iReserved0;
		volatile long iReserved1;
		uint64 iAllocSeq;
		xrtMemBlockHeader* pLiveHead;
		xrtMemBlockHeader* pLiveTail;
		xrtMemBlockHeader* pQuarantineHead;
		xrtMemBlockHeader* pQuarantineTail;
		uint32 iQuarantineCount;
		uint32 iEventCursor;
		uint64 iQuarantineBytes;
		uint32 iEventCount;
		uint64 iLiveAllocCount;
		uint64 iLiveAllocBytes;
		uint64 iPeakLiveAllocCount;
		uint64 iPeakLiveAllocBytes;
		uint64 iForeignLiveCount;
		uint64 iForeignLiveBytes;
		uint64 iPeakForeignLiveCount;
		uint64 iPeakForeignLiveBytes;
		uint64 iInvalidFreeCount;
		uint64 iDoubleFreeCount;
		uint64 iWrongAllocatorFreeCount;
		uint64 iLiveObjectCount;
		uint64 iPeakLiveObjectCount;
		uint64 iObjectDoubleDestroyCount;
		uint64 iTempCurrentBytes;
		uint64 iTempPeakBytes;
		uint64 iTempResetCount;
		uint64 iOverflowCount;
		uint64 iUnderflowCount;
		xrtMemDebugSiteStat* pSiteStats;
		xrtMemDebugForeignAlloc* pForeignAllocs;
		xrtMemDebugObject* pObjects;
		xrtMemDebugEvent arrEvents[XRT_MEMDEBUG_EVENT_CAPACITY];
	} xrtMemDebugState;

	#define XRT_CO_RUNTIME_FIBER_CONVERTED	0x00000001u
	#define XRT_CO_RUNTIME_FIBER_HOSTED		0x00000002u

	typedef struct {
		uint64 iOwnerThreadId;
		uint32 iFlags;
		uint32 iReserved;
		ptr pCurrent;
		ptr pRoot;
		ptr pDefaultSched;
		ptr pBackendMain;
		ptr pBackendAux;
	} xrtCoroRuntimeState;

	#define XRT_OBJMODE_LOCAL		0
	#define XRT_OBJMODE_SHARED		1
	#define XRT_OBJFLAG_SHARED_PENDING	0x00000001u

	typedef struct {
		xrtThreadData* pOwnerThread;
		uint64 iOwnerThreadId;
		uint32 iMode;
		uint32 iFlags;
		volatile long iSharedLock;
		uint64 iSharedOwnerThreadId;
		uint32 iSharedDepth;
	} xrtOwnerInfo;

	struct xrtThreadCleanup {
		xrtThreadCleanupProc Proc;
		ptr Arg;
		xrtThreadCleanup* pPrev;
	};
	
	// 全局
	typedef struct {
		
		// 是否已经初始化过
		int bInit;

		// 初始化引用计数
		uint32 iInitRef;
		
		// 全局数据 (不可改变)
		str sNull;

		void (*OnError)(str sError);
		
		// 高精度时钟频率单位
		#if defined(_WIN32) || defined(_WIN64)
			uint64 Frequency;
		#endif
		
		// 本机 IP 地址 ( 用于生成 XID )
		uint LocalAddr;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
		// 内存函数
		ptr (*malloc)(size_t iSize);
		ptr (*calloc)(size_t iNum, size_t iSize);
		ptr (*realloc)(ptr pMem, size_t iSize);
		void (*free)(ptr pMem);
		
		// 约等于配置
		int iApproxIntMode;       // 整数模式: 0=差值, 1=百分比
		double fApproxIntTol;     // 整数容差
		int iApproxNumMode;       // 浮点模式: 0=差值, 1=百分比
		double fApproxNumTol;     // 浮点容差
		int64 iApproxTimeTol;     // 时间容差(xtime单位)
		int iApproxStrMode;       // 字符串模式: 0=通配符, 1=相似度
		double fApproxStrTol;     // 字符串相似度阈值(0.0-1.0)
		bool bApproxStrCase;      // 通配符模式大小写开关(TRUE=忽略)
		
		xrtMemGlobalPool MemGlobal;
		xrtMemTelemetryState MemTelemetry;
		#ifdef XRT_MEM_DEBUG
			xrtMemDebugState MemDebug;
		#endif
	} xrtGlobalData;

	// 线程级运行时状态
	struct xrtThreadData {
		xrtGlobalData* pGlobal;
		uint64 iThreadId;
		struct xthread_struct* pSelf;
		uint32 iAttachDepth;
		str LastError;
		bool bFreeLastError;
		xrtTempArenaState tTemp;
		xrand rand32;
		xrand rand64_low;
		xrand rand64_high;
		xrtThreadMemState tMem;
		xrtCoroRuntimeState tCoro;
		xrtThreadCleanup* pCleanupTop;
		ptr pFutureDeferredHead;
		ptr pFutureDeferredTail;
		bool bFutureDeferredCleanupRegistered;
	};
	
	// 全局数据
	XXAPI extern xrtGlobalData xCore;
	
	
	
	// 初始化 xCore
	XXAPI xrtGlobalData* xrtInit();
	
	// 释放 xCore
	XXAPI void xrtUnit();

	// 获取当前线程运行时状态
	XXAPI xrtThreadData* xrtThreadGetCurrent();

	// 当前线程是否已经附加到 XRT 运行时
	XXAPI bool xrtThreadIsAttached();

	// 将当前线程附加到 XRT 运行时
	XXAPI xrtThreadData* xrtThreadAttachCurrent();

	// 将当前线程从 XRT 运行时分离
	XXAPI void xrtThreadDetachCurrent();

	// 获取当前线程错误信息
	XXAPI str xrtGetError();

	// 获取当前线程ID
	XXAPI uint64 xrtThreadGetCurrentId();

	// 注册当前线程退出清理回调
	XXAPI bool xrtThreadPushCleanup(xrtThreadCleanupProc proc, ptr pArg);

	// 弹出当前线程顶部匹配的清理回调
	XXAPI bool xrtThreadPopCleanup(xrtThreadCleanupProc proc, ptr pArg);
	
	
	
	/* ------------------------------------ 基础功能补充 ------------------------------------ */
	
	// 内存查找
	#if defined(_WIN32) || defined(_WIN64)
		XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
	#endif
	
	// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
	XXAPI size_t u16len(u16str sText);
	XXAPI size_t u32len(u32str sText);
	
	
	
	/* ------------------------------------ Base 函数库 ------------------------------------ */
	
	// 申请内存
	XXAPI ptr xrtMalloc(size_t iSize);
	
	// 申请类内存
	XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
	
	// 重新申请内存
	XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
	
	// 释放内存（ 会先判断是否为 null ）
	XXAPI void xrtFree(ptr pmem);
	
	// 申请无需主动释放的临时内存
	XXAPI ptr xrtTempMemory(size_t iSize);
	
	// 释放所有临时内存
	XXAPI void xrtFreeTempMemory();
	XXAPI void xrtMemTelemetryEnable(bool bEnable);
	XXAPI bool xrtMemTelemetryIsEnabled();
	XXAPI void xrtMemTelemetryReset();
	XXAPI void xrtMemTelemetryGetSnapshot(xrtMemTelemetrySnapshot* pOut);
	#ifdef XRT_MEM_DEBUG
		XXAPI void xrtMemDebugEnable(bool bEnable);
		XXAPI bool xrtMemDebugIsEnabled();
		XXAPI void xrtMemDebugReset();
		XXAPI bool xrtMemDebugDumpText(str sPath);
		XXAPI bool xrtMemDebugDumpJson(str sPath);
		XXAPI ptr xrtMallocDbg(size_t iSize, const char* sFile, uint32 iLine);
		XXAPI ptr xrtCallocDbg(size_t iNum, size_t iSize, const char* sFile, uint32 iLine);
		XXAPI ptr xrtReallocDbg(ptr pMem, size_t iSize, const char* sFile, uint32 iLine);
		XXAPI void xrtFreeDbg(ptr pMem, const char* sFile, uint32 iLine);
	#endif

	#if defined(XRT_MEM_DEBUG) && !defined(XRT_BUILD_CORE)
		#define xrtMalloc(iSize) xrtMallocDbg((iSize), __FILE__, __LINE__)
		#define xrtCalloc(iNum, iSize) xrtCallocDbg((iNum), (iSize), __FILE__, __LINE__)
		#define xrtRealloc(pMem, iSize) xrtReallocDbg((pMem), (iSize), __FILE__, __LINE__)
		#define xrtFree(pMem) xrtFreeDbg((pMem), __FILE__, __LINE__)
	#endif
	
	// 设置错误
	XXAPI void xrtSetError(const void* sError, bool bFree);
	XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
	XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree);
	
	// 清除错误
	XXAPI void xrtClearError();

	// 获取当前线程的内存上下文
	static inline xrtThreadMemState* xrtThreadGetMemState()
	{
		xrtThreadData* pThreadData = xrtThreadGetCurrent();
		if ( pThreadData ) {
			return &pThreadData->tMem;
		}
		return NULL;
	}

	static inline long __xrtAtomicCompareExchange32(volatile long* pValue, long iExchange, long iComparand)
	{
		#if defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			long iPrev;
			__asm__ volatile (
				"lock; cmpxchgl %2, %1"
				: "=a"(iPrev), "+m"(*pValue)
				: "r"(iExchange), "0"(iComparand)
				: "memory"
			);
			return iPrev;
		#elif defined(_WIN32) || defined(_WIN64)
			return InterlockedCompareExchange((volatile LONG*)pValue, iExchange, iComparand);
		#else
			return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
		#endif
	}

	static inline long __xrtAtomicExchange32(volatile long* pValue, long iValue)
	{
		#if defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			__asm__ volatile (
				"xchgl %0, %1"
				: "+r"(iValue), "+m"(*pValue)
				:
				: "memory"
			);
			return iValue;
		#elif defined(_WIN32) || defined(_WIN64)
			return InterlockedExchange((volatile LONG*)pValue, iValue);
		#else
			return __sync_lock_test_and_set(pValue, iValue);
		#endif
	}

	/* Keep dedicated 32-bit atomics for uint32 fields. LP64 builds make long-based helpers 64-bit wide. */
	static inline uint32 __xrtAtomicCompareExchangeU32(volatile uint32* pValue, uint32 iExchange, uint32 iComparand)
	{
		#if defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			return (uint32)__xrtAtomicCompareExchange32((volatile long*)pValue, (long)iExchange, (long)iComparand);
		#elif defined(_WIN32) || defined(_WIN64)
			return (uint32)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iExchange, (LONG)iComparand);
		#else
			return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
		#endif
	}

	static inline uint32 __xrtAtomicLoadU32(const volatile uint32* pValue)
	{
		return __xrtAtomicCompareExchangeU32((volatile uint32*)pValue, 0u, 0u);
	}

	static inline void __xrtAtomicStoreU32(volatile uint32* pValue, uint32 iValue)
	{
		#if defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			(void)__xrtAtomicExchange32((volatile long*)pValue, (long)iValue);
		#elif defined(_WIN32) || defined(_WIN64)
			(void)InterlockedExchange((volatile LONG*)pValue, (LONG)iValue);
		#else
			(void)__sync_lock_test_and_set(pValue, iValue);
		#endif
	}

	static inline long __xrtAtomicAddFetch32(volatile long* pValue, long iDelta)
	{
		#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
			long iPrev;
			long iNext;
			do {
				iPrev = InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
				iNext = iPrev + iDelta;
			} while ( InterlockedCompareExchange((volatile LONG*)pValue, iNext, iPrev) != iPrev );
			return iNext;
		#elif defined(_WIN32) || defined(_WIN64)
			return InterlockedExchangeAdd((volatile LONG*)pValue, iDelta) + iDelta;
		#else
			return __sync_add_and_fetch(pValue, iDelta);
		#endif
	}

	static inline int64 __xrtAtomicAddFetch64(volatile int64* pValue, int64 iDelta)
	{
		#if defined(__TINYC__) && defined(_WIN32)
			int64 iPrev;
			int64 iNext;
			do {
				iPrev = (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, 0, 0);
				iNext = iPrev + iDelta;
			} while ( (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iNext, (LONG64)iPrev) != iPrev );
			return iNext;
		#elif defined(_WIN32) || defined(_WIN64)
			return (int64)InterlockedExchangeAdd64((volatile LONG64*)pValue, (LONG64)iDelta) + iDelta;
		#else
			return __sync_add_and_fetch(pValue, iDelta);
		#endif
	}

	static inline int64 __xrtAtomicCompareExchange64(volatile int64* pValue, int64 iExchange, int64 iComparand)
	{
		#if defined(__TINYC__) && defined(_WIN32)
			return (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iExchange, (LONG64)iComparand);
		#elif defined(_WIN32) || defined(_WIN64)
			return (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iExchange, (LONG64)iComparand);
		#else
			return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
		#endif
	}

	static inline int64 __xrtAtomicLoad64(const volatile int64* pValue)
	{
		return __xrtAtomicCompareExchange64((volatile int64*)pValue, 0, 0);
	}

	static inline void __xrtAtomicStore64(volatile int64* pValue, int64 iValue)
	{
		#if defined(__TINYC__) && defined(_WIN32)
			int64 iPrev;
			do {
				iPrev = __xrtAtomicLoad64(pValue);
			} while ( __xrtAtomicCompareExchange64(pValue, iValue, iPrev) != iPrev );
		#elif defined(_WIN32) || defined(_WIN64)
			(void)InterlockedExchange64((volatile LONG64*)pValue, (LONG64)iValue);
		#else
			(void)__sync_lock_test_and_set(pValue, iValue);
		#endif
	}

	static inline long __xrtOwnerAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
	{
		return __xrtAtomicCompareExchange32(pValue, iExchange, iComparand);
	}

	static inline void __xrtOwnerAtomicStore(volatile long* pValue, long iValue)
	{
		__xrtAtomicExchange32(pValue, iValue);
	}

	static inline void __xrtOwnerSpinLock(volatile long* pLock)
	{
		while ( __xrtOwnerAtomicCompareExchange(pLock, 1, 0) != 0 ) {
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(0);
			#else
				usleep(1000);
			#endif
		}
	}

	static inline void __xrtOwnerSpinUnlock(volatile long* pLock)
	{
		__xrtOwnerAtomicStore(pLock, 0);
	}

	static inline uint64 __xrtOwnerGetCurrentThreadId()
	{
		xrtThreadData* pThreadData = xrtThreadGetCurrent();
		if ( pThreadData ) {
			return pThreadData->iThreadId;
		}
		return xrtThreadGetCurrentId();
	}

	// 初始化对象所有权信息（默认本线程私有）
	static inline void xrtOwnerInitMode(xrtOwnerInfo* pOwner, uint32 iMode)
	{
		xrtThreadData* pThreadData = xrtThreadGetCurrent();
		if ( pOwner == NULL ) {
			return;
		}
		pOwner->pOwnerThread = pThreadData;
		pOwner->iOwnerThreadId = pThreadData ? pThreadData->iThreadId : xrtThreadGetCurrentId();
		pOwner->iMode = XRT_OBJMODE_LOCAL;
		pOwner->iFlags = 0;
		pOwner->iSharedLock = 0;
		pOwner->iSharedOwnerThreadId = 0;
		pOwner->iSharedDepth = 0;
		if ( iMode == XRT_OBJMODE_SHARED ) {
			pOwner->iMode = XRT_OBJMODE_SHARED;
			pOwner->iFlags |= XRT_OBJFLAG_SHARED_PENDING;
		}
	}

	// 初始化对象所有权信息（默认本线程私有）
	static inline void xrtOwnerInit(xrtOwnerInfo* pOwner)
	{
		xrtOwnerInitMode(pOwner, XRT_OBJMODE_LOCAL);
	}

	// 获取对象模式
	static inline uint32 xrtOwnerGetMode(const xrtOwnerInfo* pOwner)
	{
		if ( pOwner == NULL ) {
			return XRT_OBJMODE_LOCAL;
		}
		return pOwner->iMode;
	}

	// 允许对象进入共享模式
	static inline void xrtOwnerSetShared(xrtOwnerInfo* pOwner)
	{
		if ( pOwner == NULL ) {
			return;
		}
		pOwner->iMode = XRT_OBJMODE_SHARED;
		pOwner->iFlags |= XRT_OBJFLAG_SHARED_PENDING;
		pOwner->iSharedOwnerThreadId = 0;
		pOwner->iSharedDepth = 0;
	}

	static inline void xrtOwnerActivateShared(xrtOwnerInfo* pOwner)
	{
		if ( pOwner == NULL ) {
			return;
		}
		pOwner->iMode = XRT_OBJMODE_SHARED;
		pOwner->iFlags &= ~XRT_OBJFLAG_SHARED_PENDING;
		pOwner->iSharedOwnerThreadId = 0;
		pOwner->iSharedDepth = 0;
	}

	static inline str __xrtOwnerSharedPendingError(void)
	{
		return (str)"shared object is not published yet; cross-thread access is not allowed before publish.";
	}

	static inline bool xrtOwnerLock(xrtOwnerInfo* pOwner, const void* sError)
	{
		uint64 iThreadId;
		if ( pOwner == NULL ) {
			return FALSE;
		}
		iThreadId = __xrtOwnerGetCurrentThreadId();
		if ( pOwner->iMode == XRT_OBJMODE_SHARED ) {
			if ( pOwner->iFlags & XRT_OBJFLAG_SHARED_PENDING ) {
				if ( pOwner->iOwnerThreadId == 0 || pOwner->iOwnerThreadId == iThreadId ) {
					return TRUE;
				}
				xrtSetError(__xrtOwnerSharedPendingError(), FALSE);
				return FALSE;
			}
			if ( pOwner->iSharedOwnerThreadId == iThreadId && pOwner->iSharedDepth > 0 ) {
				pOwner->iSharedDepth++;
				return TRUE;
			}
			__xrtOwnerSpinLock(&pOwner->iSharedLock);
			pOwner->iSharedOwnerThreadId = iThreadId;
			pOwner->iSharedDepth = 1;
			return TRUE;
		}
		if ( pOwner->iOwnerThreadId == 0 || pOwner->iOwnerThreadId == iThreadId ) {
			return TRUE;
		}
		xrtSetError(sError ? sError : "runtime object belongs to another thread.", FALSE);
		return FALSE;
	}

	static inline void xrtOwnerUnlock(xrtOwnerInfo* pOwner)
	{
		uint64 iThreadId;
		if ( pOwner == NULL ) {
			return;
		}
		if ( pOwner->iMode == XRT_OBJMODE_SHARED && (pOwner->iFlags & XRT_OBJFLAG_SHARED_PENDING) == 0 ) {
			iThreadId = __xrtOwnerGetCurrentThreadId();
			if ( pOwner->iSharedOwnerThreadId != 0 && pOwner->iSharedOwnerThreadId != iThreadId ) {
				return;
			}
			if ( pOwner->iSharedDepth > 1 ) {
				pOwner->iSharedDepth--;
				return;
			}
			pOwner->iSharedDepth = 0;
			pOwner->iSharedOwnerThreadId = 0;
			__xrtOwnerSpinUnlock(&pOwner->iSharedLock);
		}
	}

	static inline bool xrtOwnerBeginMutable(xrtOwnerInfo* pOwner, const void* sError)
	{
		return xrtOwnerLock(pOwner, sError);
	}

	static inline void xrtOwnerEndMutable(xrtOwnerInfo* pOwner)
	{
		xrtOwnerUnlock(pOwner);
	}

	// 检查当前线程是否允许修改对象
	static inline bool xrtOwnerCheckMutable(xrtOwnerInfo* pOwner, const void* sError)
	{
		uint64 iThreadId;
		xrtThreadData* pThreadData;
		if ( pOwner == NULL ) {
			return FALSE;
		}
		pThreadData = xrtThreadGetCurrent();
		iThreadId = pThreadData ? pThreadData->iThreadId : xrtThreadGetCurrentId();
		if ( pOwner->iMode == XRT_OBJMODE_SHARED ) {
			if ( (pOwner->iFlags & XRT_OBJFLAG_SHARED_PENDING) == 0 ) {
				return TRUE;
			}
			if ( pOwner->iOwnerThreadId == 0 || pOwner->iOwnerThreadId == iThreadId ) {
				return TRUE;
			}
			xrtSetError(__xrtOwnerSharedPendingError(), FALSE);
			return FALSE;
		}
		if ( pOwner->iOwnerThreadId == 0 || pOwner->iOwnerThreadId == iThreadId ) {
			return TRUE;
		}
		xrtSetError(sError ? sError : "runtime object belongs to another thread.", FALSE);
		return FALSE;
	}
	
	
	
	/* ------------------------------------ Charset 函数库 ------------------------------------ */
	
	#define XRT_CP_AUTO				-2				// 自动识别字符集（ 可自动识别是否为 utf8，自动识别失败则使用 XRT_CP_BINARY ）
	#define XRT_CP_BINARY			-1				// 二进制文件
	#define XRT_CP_OEM				0				// 本机 OEM 字符集 ( windows为OEM多字节编码，linux固定为utf8 )
	#define XRT_CP_UTF8				65001			// UTF8
	#define XRT_CP_UTF16			1200			// UTF16
	#define XRT_CP_UTF16_BE			1201			// UTF16 big-endian
	#define XRT_CP_UTF32			65005			// UTF32
	#define XRT_CP_UTF32_BE			65006			// UTF32 big-endian
	
	#define XRT_CP_BOM				0x40000000		// with BOM
	#define XRT_MASK_BOM			0xBFFFFFFF		// mask BOM
	
	// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
	XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize, size_t* iRetSize);
	
	// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize, size_t* iRetSize);
	
	// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize, size_t* iRetSize);
	
	// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize, size_t* iRetSize);
	
	// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize, size_t* iRetSize);
	
	// utf-32 转 utf-16（ 需使用 xrtFree 释放 ）
	XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize, size_t* iRetSize);
	
	// utf-16 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise);
	
	// utf-32 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise);
	
	// 任意编码转换 ( 需使用 xrtFree 释放 )
	XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP, size_t* iRetSize);
	
	// 是否为 utf-8 字符串
	XXAPI bool xrtIsUTF8(str sText, size_t iSize);
	
	// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
	XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);
	
	// 获取不同字符集的字符大小
	XXAPI int xrtGetCharSize(int iCP);
	
	
	
	/* ------------------------------------ OS 函数库 ------------------------------------ */
	
	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
	
	
	
	/* ------------------------------------ Math 函数库 ------------------------------------ */
	
	// 约等于模式常量
	#define XRT_APPROX_DIFF     0   // 差值模式
	#define XRT_APPROX_PERCENT  1   // 百分比模式
	
	// 静态初始化随机数生成器的推荐值
	#define XRAND_INITIALIZER  { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
	
	// 初始化随机数生成器
	XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq);
	
	// 生成 32 位随机数 - 线程安全
	XXAPI uint32 xrtRand32Ex(xrand* rng);
	
	// 生成 64 位随机数 - 线程安全
	XXAPI uint64 xrtRand64Ex(xrand* rngLow, xrand* rngHigh);
	
	// 生成范围随机数 - 线程安全
	XXAPI int xrtRandRangeEx(xrand* rng, int min, int max);
	
	// 获取 32 位随机数
	XXAPI uint32 xrtRand32();
	
	// 获取 64 位随机数
	XXAPI uint64 xrtRand64();
	
	// 获取 32 位范围随机数
	XXAPI int xrtRandRange(int min, int max);
	
	// 整数约等于（使用 xCore 配置）
	XXAPI bool xrtIntApprox(int64 a, int64 b);
	
	// 浮点数约等于（使用 xCore 配置）
	XXAPI bool xrtNumApprox(double a, double b);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（ 需使用 xrtFree 释放 ）
	XXAPI str xrtCopyStr(str sText, size_t iSize);
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
	
	// 比较字符串
	XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
	
	// 通配符匹配（ * 匹配任意字符序列，? 匹配单个UTF-8字符，bCase 为 TRUE 时忽略大小写 ）
	XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	
	// 字符串格式化（ 需使用 xrtFree 释放 ）
	XXAPI str xrtFormat(const void* sFormat, ...);
	
	// 字符串替换（ 需使用 xrtFree 释放 ）
	XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize);
	
	// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
	XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize);
	
	// 生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
	
	// HEX 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
	
	// HEX 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtHexDecode(str pText, size_t iSize);
	
	// Base64 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
	
	// Base64 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
	
	// 整数格式化（格式符: , 千分位 | 0N 前导零 | + 正号 | x/X 十六进制 | o 八进制 | b 二进制）（ 需使用 xrtFree 释放 ）
	XXAPI str xrtIntFormat(int64 value, str format);
	
	// 浮点数格式化（格式符: , 千分位 | .N 小数位 | + 正号 | % 百分比）（ 需使用 xrtFree 释放 ）
	XXAPI str xrtNumFormat(double value, str format);
	
	// 字符串相似度（基于 Levenshtein 编辑距离，返回 0.0-1.0）
	XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2);
	
	// 字符串约等于模式常量
	#define XRT_STR_APPROX_LIKE     0   // 通配符模式（s2为模式串）
	#define XRT_STR_APPROX_SIM      1   // 相似度阈值模式
	
	// 字符串约等于（使用 xCore 配置）
	XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2);
	
	// URL 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtUrlEncode(str sSrc, size_t iLen);
	
	// URL 解码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtUrlDecode(str sSrc, size_t iLen);
	
	// 获取 UTF-8 字符的字节数（根据首字节判断）
	static inline int xrtCharLenU8(unsigned char c)
	{
		if ( (c & 0x80) == 0 ) { return 1; }            // 0xxxxxxx - ASCII
		if ( (c & 0xE0) == 0xC0 ) { return 2; }         // 110xxxxx - 2字节
		if ( (c & 0xF0) == 0xE0 ) { return 3; }         // 1110xxxx - 3字节
		if ( (c & 0xF8) == 0xF0 ) { return 4; }         // 11110xxx - 4字节
		if ( (c & 0xFC) == 0xF8 ) { return 5; }         // 111110xx - 5字节
		if ( (c & 0xFE) == 0xFC ) { return 6; }         // 1111110x - 6字节
		return 1; // 异常字符按单字节处理
	}
	
	
	
	/* ------------------------------------ Path 函数库 ------------------------------------ */
	
	// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetNameExt(str sPath, size_t iSize);
	
	// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetName(str sPath, size_t iSize);
	
	// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetExt(str sPath, size_t iSize);
	
	// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetDir(str sPath, size_t iSize);
	
	// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
	XXAPI bool xrtPathIsAbs(str sPath, size_t iSize);
	
	// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen);
	
	// 拼接路径（ 需要使用 xrtFree 释放内存 ）
	XXAPI str xrtPathJoin(uint iCount, ...);
	
	
	
	/* ------------------------------------ Time 函数库 ------------------------------------ */
	
	// 各种固定时间单位的数值
	#define XRT_TIME_MINUTE			60
	#define XRT_TIME_HOUR			3600
	#define XRT_TIME_DAY			86400
	#define XRT_TIME_YEAR			31536000
	#define XRT_TIME_LEAPYEAR		31622400
	#define XRT_TIME_400YEAR		12622780800				// 每隔 400 年有 97 个闰年 + 303 个平年
	#define XRT_TIME_19700101		62167219200
	
	// 时间单位
	#define XRT_TIME_INTERVAL_YEAR			1				// 年
	#define XRT_TIME_INTERVAL_MONTH			2				// 月
	#define XRT_TIME_INTERVAL_DAY			3				// 日
	#define XRT_TIME_INTERVAL_HOUR			4				// 时
	#define XRT_TIME_INTERVAL_MINUTE		5				// 分
	#define XRT_TIME_INTERVAL_SECOND		6				// 秒
	#define XRT_TIME_INTERVAL_WEEKDAY		7				// 星期
	#define XRT_TIME_INTERVAL_QUARTER		8				// 季度
	
	// 转换格式
	#define XRT_TIME_FORMAT_DATETIME		0
	#define XRT_TIME_FORMAT_DATE			1
	#define XRT_TIME_FORMAT_TIME			2
	
	// 获取高精度时钟 Tick ( 返回秒数，便于计算时间间隔 )
	XXAPI double xrtTimer();
	
	// 毫秒级延时
	XXAPI void xrtSleep(uint32 ms);
	
	// 判断是否为闰年
	XXAPI bool xrtIsLeapYear(int iYear);
	
	// 获取某年某月有多少天
	XXAPI int xrtDaysInMonth(int iYear, int iMonth);
	
	// 获取某年有多少天
	XXAPI int xrtDaysInYear(int iYear);
	
	// 构建时间
	XXAPI xtime xrtTimeSerial(int iHour, int iMinute, int iSecond);
	
	// 构建日期
	XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
	
	// 构建日期 + 时间
	XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
	
	// 获取时间中的秒
	XXAPI int xrtSecond(xtime iTime);
	
	// 获取时间中的分钟
	XXAPI int xrtMinute(xtime iTime);
	
	// 获取时间中的小时
	XXAPI int xrtHour(xtime iTime);
	
	// 获取时间中的日期
	XXAPI int xrtDay(xtime iTime);
	
	// 获取时间中的月份
	XXAPI int xrtMonth(xtime iTime);
	
	// 获取时间中的年份
	XXAPI int64 xrtYear(xtime iTime);
	
	// 获取时间中的星期
	XXAPI int xrtWeekday(xtime iTime);
	
	// 获取时间是当年的第几天
	XXAPI int xrtDayOfYear(xtime iTime);
	
	// 解码时间
	XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, int* pHour, int* pMinute, int* pSecond, int* pWeekday, int* pDayOfYear);
	
	// 获取当前日期 + 时间
	XXAPI xtime xrtNow();
	
	// 获取当前日期
	XXAPI xtime xrtDate();
	
	// 获取当前时间
	XXAPI xtime xrtTime();
	
	// 获取字符串格式的当前日期 + 时间（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtNowStr();
	
	// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtDateStr();
	
	// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeStr();
	
	// 转换日期 + 时间为字符串（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
	
	// 字符串转时间（智能解析，支持多种格式）
	// 支持: YYYY-MM-DD HH:MM:SS, YYYY/MM/DD, YYYYMMDD, YYYYMMDDHHMMSS, HH:MM:SS 等
	XXAPI xtime xrtStrToTime(str sTime, size_t iSize);
	
	// 时间单位累加
	XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
	
	// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
	XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
	
	// 获取季度（1-4）
	XXAPI int xrtQuarter(xtime iTime);
	
	// 获取日期部分（去除时间）
	XXAPI xtime xrtDatePart(xtime iTime);
	
	// 获取时间部分（去除日期）
	XXAPI xtime xrtTimePart(xtime iTime);
	
	// 是否同一天
	XXAPI bool xrtIsSameDay(xtime iTime1, xtime iTime2);
	
	// 是否同一月
	XXAPI bool xrtIsSameMonth(xtime iTime1, xtime iTime2);
	
	// 是否同一年
	XXAPI bool xrtIsSameYear(xtime iTime1, xtime iTime2);
	
	// 判断时间是否在区间内
	XXAPI bool xrtTimeInRange(xtime iTime, xtime iStart, xtime iEnd);
	
	// 判断两个时间区间是否重叠
	XXAPI bool xrtTimeRangeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2);
	
	// 与Unix时间戳互转 - xtime转Unix时间戳
	XXAPI int64 xrtToUnixTime(xtime iTime);
	
	// 与Unix时间戳互转 - Unix时间戳转xtime
	XXAPI xtime xrtFromUnixTime(int64 unixTime);
	
	// 获取月份的第一天
	XXAPI xtime xrtFirstDayOfMonth(xtime iTime);
	
	// 获取月份的最后一天
	XXAPI xtime xrtLastDayOfMonth(xtime iTime);
	
	// 获取年份的第一天
	XXAPI xtime xrtFirstDayOfYear(xtime iTime);
	
	// 获取年份的最后一天
	XXAPI xtime xrtLastDayOfYear(xtime iTime);
	
	// 获取周的第一天（iStartDay: 0=周日, 1=周一, ...）
	XXAPI xtime xrtFirstDayOfWeek(xtime iTime, int iStartDay);
	
	// 获取周的最后一天（iStartDay: 0=周日, 1=周一, ...）
	XXAPI xtime xrtLastDayOfWeek(xtime iTime, int iStartDay);
	
	// 获取当年第几周（ISO周数，周一为一周开始）
	XXAPI int xrtWeekOfYear(xtime iTime);
	
	// 获取当月第几周（周一为一周开始）
	XXAPI int xrtWeekOfMonth(xtime iTime);
	
	// 获取UTC时间
	XXAPI xtime xrtNowUTC();
	
	// 获取本地时区偏移（秒）
	XXAPI int xrtTimezoneOffset();
	
	// UTC转本地时间
	XXAPI xtime xrtUTCToLocal(xtime utc);
	
	// 本地时间转UTC
	XXAPI xtime xrtLocalToUTC(xtime local);
	
	// 获取相对时间描述（如"3天前"、"2小时后"）（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtRelativeTime(xtime iTime, xtime iBaseTime);
	
	// 时间格式化为字符串（ 需使用 xrtFree 释放内存 ）
	// 格式占位符: yyyy/yy(年), mm/m(月,h后=分钟), mmm/mmmm(英文月份),
	//             dd/d(日), hh/h(24时), HH/H(12时), nn/n(分钟),
	//             ss/s(秒), ap/AP(am/pm), w/ww/www(星期), q(季度)
	XXAPI str xrtTimeFormat(xtime iTime, const void* sFormat);
	
	// 字符串解析为时间
	// 格式占位符同 xrtTimeFormat，另支持: *(跳过任意非数字), .(至少1个非数字), ?(跳过1字符), 空格(跳过空白)
	// 解析时自动跳过前缀冗余文本
	XXAPI xtime xrtTimeParse(str sTime, str sFormat);
	
	// 时间约等于（使用 xCore.iApproxTimeTol 容差）
	XXAPI bool xrtTimeApprox(xtime a, xtime b);
	
	
	
	/* ------------------------------------ File 函数库 ------------------------------------ */
	/*
		依赖项：
			Time 函数库
	*/
	
	// 文件对象
	typedef struct {
		union {
			ptr obj;			// windows 文件对象
			int idx;			// linux 文件句柄
		};
		int Charset;			// 文件字符集
		uint BOM;				// BOM大小
		int ReadOnly;			// 只读模式
	} xfile_struct, *xfile;
	
	// 游标控制
	#define XRT_SEEK_SET		0
	#define XRT_SEEK_CUR		1
	#define XRT_SEEK_END		2
	
	// 打开文件 ( 需要使用 xrtClose 关闭文件 )
	XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
	
	// 关闭文件
	XXAPI void xrtClose(xfile objFile);
	
	// 设置游标位置
	XXAPI size_t xrtSeek(xfile objFile, int64 iOffset, int iMoveMethod);
	
	// 获取游标位置
	XXAPI size_t xrtTell(xfile objFile);
	
	// 获取文件末尾位置 ( 获取一打开文件的动态大小 )
	XXAPI size_t xrtGetEOF(xfile objFile);
	
	// 是否已经读取到文件末尾
	XXAPI bool xrtEOF(xfile objFile);
	
	// 设置文件末尾
	XXAPI bool xrtSetEOF(xfile objFile);
	
	// 从已打开的文件读取数据 ( iSize 为要读取的字节数，需要使用 xrtFree 释放内存 )
	XXAPI str xrtRead(xfile objFile, size_t iSize, size_t* iRetSize);
	
	// 向已打开的文件写入数据 ( iSize 为要写入的字节数 )
	XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize);
	
	// 从已打开的文件读取二进制数据到特定位置
	XXAPI size_t xrtGetBuffer(xfile objFile, ptr sBuff, size_t iSize);
	
	// 从已打开的文件读取二进制数据 ( 需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtGet(xfile objFile, size_t iSize, size_t* iRetSize);
	
	// 向已打开的文件写入二进制数据
	XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize);
	
	// 向文件追加写入数据
	XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset);
	
	// 写入并覆盖文件内容
	XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
	
	// 读取文件的全部内容 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtFileReadAll(str sPath, int iCharset, size_t* iRetSize);
	
	// 写入并覆盖文件内容 ( 二进制 )
	XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize);
	
	// 读取文件的全部内容 ( 二进制，需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtFileGetAll(str sPath, size_t* iRetSize);
	
	// 判断路径是否存在
	XXAPI bool xrtPathExists(str sPath);
	
	// 判断文件是否存在
	XXAPI bool xrtFileExists(str sPath);
	
	// 判断目录是否存在
	XXAPI bool xrtDirExists(str sPath);
	
	// 获取文件长度
	XXAPI size_t xrtFileGetSize(str sPath);
	
	// 设置文件长度
	XXAPI bool xrtFileSetSize(str sPath, size_t iSize);
	
	// 获取文件属性
	XXAPI int xrtFileGetAttr(str sPath);
	
	// 设置文件属性
	XXAPI bool xrtFileSetAttr(str sPath, int iAttr);
	
	// 获取文件最后一次访问时间
	XXAPI int64 xrtFileGetAccessTime(str sPath);
	
	// 获取文件修改时间
	XXAPI int64 xrtFileGetChangeTime(str sPath);
	
	// 复制文件
	XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite);
	
	// 移动文件（重命名）
	XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite);
	
	// 删除文件
	XXAPI bool xrtFileDelete(str sPath);
	
	// 扫描文件夹 ( 返回文件数量 )
	XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
	
	// 创建文件夹
	XXAPI bool xrtDirCreate(str sPath);
	
	// 创建多级文件夹
	XXAPI bool xrtDirCreateAll(str sPath);
	
	// 复制文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite);
	
	// 移动文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite);
	
	// 删除文件夹 ( 返回操作的文件数量 )
	XXAPI int xrtDirDelete(str sPath);
	
	
	
	/* ------------------------------------ Thread 函数库 ------------------------------------ */
	/*
		依赖项：
			Time 函数库
	*/
	
	// 线程状态
	#define XRT_THREAD_STOPPED		0			// 已停止
	#define XRT_THREAD_RUNNING		1			// 运行中
	#define XRT_THREAD_SUSPENDED	2			// 已挂起
	
	// 等待超时返回值
	#define XRT_WAIT_OK				0			// 等待成功
	#define XRT_WAIT_TIMEOUT		1			// 等待超时
	#define XRT_WAIT_ERROR			-1			// 等待错误
	
	// 线程数据结构
	typedef struct xthread_struct {
		ptr Handle;								// 线程句柄
		uint64 TID;								// 线程ID
		uint32 (*Proc)(ptr param);				// 用户回调函数
		ptr Param;								// 用户参数
	volatile int StopFlag;						// 停止信号标志
	volatile int bFinished;						// 是否已经结束
	volatile int bJoined;						// 是否已经完成等待
	volatile int bAutoDestroy;					// 线程退出后自动释放管理对象
	uint32 ExitCode;							// 线程退出码
	#if !defined(_WIN32) && !defined(_WIN64)
		pthread_mutex_t FinishLock;				// 结束状态锁（POSIX）
			pthread_cond_t FinishCond;			// 结束条件变量（POSIX，monotonic）
		#endif
	} xthread_struct, *xthread;
	
	// 互斥体数据结构
	typedef struct {
		#if defined(_WIN32) || defined(_WIN64)
			SRWLOCK objLock;					// Windows SRWLOCK（最高性能，无递归锁支持）
		#else
			pthread_mutex_t objLock;			// Linux pthread_mutex（非递归模式）
		#endif
	} xmutex_struct, *xmutex;
	
	// 信号量数据结构
	typedef struct {
		#if defined(_WIN32) || defined(_WIN64)
			HANDLE objSem;						// Windows：内核信号量句柄（直接嵌入）
		#else
			pthread_mutex_t objLock;			// POSIX：自维护锁（monotonic wait）
			pthread_cond_t objCond;				// POSIX：自维护条件变量（monotonic wait）
			uint32 iValue;						// 当前计数
			uint32 iMaxValue;					// 最大计数
		#endif
	} xsem_struct, *xsem;
	
	// 条件变量数据结构
	typedef struct {
		#if defined(_WIN32) || defined(_WIN64)
			CONDITION_VARIABLE objCond;			// Windows：条件变量对象（直接嵌入）
		#else
			pthread_cond_t objCond;				// POSIX：条件变量对象（直接嵌入，无需额外 malloc）
		#endif
	} xcond_struct, *xcond;
	
	// 读写锁数据结构
	typedef struct {
		#if defined(_WIN32) || defined(_WIN64)
			SRWLOCK objLock;					// Windows SRWLOCK（最高性能）
		#else
			pthread_rwlock_t objLock;			// Linux pthread_rwlock
		#endif
	} xrwlock_struct, *xrwlock;
	
	/* ---------- 线程管理 ---------- */
	
	// 创建线程
	XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
	
	// 销毁线程对象（不终止线程，仅释放管理结构）
	XXAPI void xrtThreadDestroy(xthread pThread);
	
	// 等待线程结束
	XXAPI void xrtThreadWait(xthread pThread);
	
	// 等待线程结束（带超时，毫秒）
	XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout);
	
	// 发送停止信号（线程需要主动检查 xrtThreadShouldStop）
	XXAPI void xrtThreadStop(xthread pThread);
	
	// 检查是否应该停止（线程内调用）
	XXAPI bool xrtThreadShouldStop(xthread pThread);
	
	// 获取线程状态
	XXAPI int xrtThreadGetState(xthread pThread);
	
	// 获取线程退出码
	XXAPI uint32 xrtThreadGetExitCode(xthread pThread);
	
	// 获取当前线程ID
	XXAPI uint64 xrtThreadGetCurrentId();
	
	// 让出当前线程的时间片
	XXAPI void xrtThreadYield();
	
	/* ---------- 互斥体 ---------- */
	
	// 创建互斥体
	XXAPI xmutex xrtMutexCreate();
	
	// 销毁互斥体
	XXAPI void xrtMutexDestroy(xmutex pMutex);
	
	// 初始化互斥体（对自维护结构体指针使用）
	XXAPI void xrtMutexInit(xmutex pMutex);
	
	// 释放互斥体（对自维护结构体指针使用）
	XXAPI void xrtMutexUnit(xmutex pMutex);
	
	// 锁定互斥体（阻塞）
	XXAPI void xrtMutexLock(xmutex pMutex);
	
	// 尝试锁定互斥体（非阻塞）
	XXAPI bool xrtMutexTryLock(xmutex pMutex);
	
	// 解锁互斥体
	XXAPI void xrtMutexUnlock(xmutex pMutex);
	
	/* ---------- 信号量 ---------- */
	
	// 创建信号量
	XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue);
	
	// 销毁信号量
	XXAPI void xrtSemDestroy(xsem pSem);
	
	// 初始化信号量（对自维护结构体指针使用）
	XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue);
	
	// 释放信号量（对自维护结构体指针使用）
	XXAPI void xrtSemUnit(xsem pSem);
	
	// 等待信号量（阻塞，计数减1）
	XXAPI void xrtSemWait(xsem pSem);
	
	// 尝试等待信号量（非阻塞）
	XXAPI bool xrtSemTryWait(xsem pSem);
	
	// 等待信号量（带超时，毫秒）
	XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout);
	
	// 释放信号量（计数加1）
	XXAPI bool xrtSemPost(xsem pSem);
	
	// 释放信号量（计数加N）
	XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount);
	
	/* ---------- 条件变量 ---------- */
	
	// 创建条件变量
	XXAPI xcond xrtCondCreate();
	
	// 销毁条件变量
	XXAPI void xrtCondDestroy(xcond pCond);
	
	// 初始化条件变量（对自维护结构体指针使用）
	XXAPI void xrtCondInit(xcond pCond);
	
	// 释放条件变量（对自维护结构体指针使用）
	XXAPI void xrtCondUnit(xcond pCond);
	
	// 等待条件变量（需要先锁定互斥体）
	XXAPI void xrtCondWait(xcond pCond, xmutex pMutex);
	
	// 等待条件变量（带超时，毫秒）
	XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout);
	
	// 唤醒一个等待的线程
	XXAPI void xrtCondSignal(xcond pCond);
	
	// 唤醒所有等待的线程
	XXAPI void xrtCondBroadcast(xcond pCond);
	
	/* ---------- 读写锁 ---------- */
	
	// 创建读写锁
	XXAPI xrwlock xrtRWLockCreate();
	
	// 销毁读写锁
	XXAPI void xrtRWLockDestroy(xrwlock pRWLock);
	
	// 初始化读写锁（对自维护结构体指针使用）
	XXAPI void xrtRWLockInit(xrwlock pRWLock);
	
	// 释放读写锁（对自维护结构体指针使用）
	XXAPI void xrtRWLockUnit(xrwlock pRWLock);
	
	// 获取读锁（阻塞）
	XXAPI void xrtRWLockReadLock(xrwlock pRWLock);
	
	// 尝试获取读锁（非阻塞）
	XXAPI bool xrtRWLockTryReadLock(xrwlock pRWLock);
	
	// 释放读锁
	XXAPI void xrtRWLockReadUnlock(xrwlock pRWLock);
	
	// 获取写锁（阻塞）
	XXAPI void xrtRWLockWriteLock(xrwlock pRWLock);
	
	// 尝试获取写锁（非阻塞）
	XXAPI bool xrtRWLockTryWriteLock(xrwlock pRWLock);
	
	// 释放写锁
	XXAPI void xrtRWLockWriteUnlock(xrwlock pRWLock);
	
	// 写锁降级为读锁（保持锁状态）
	XXAPI void xrtRWLockDowngrade(xrwlock pRWLock);
	
	// 读锁升级为写锁（可能失败，需要释放后重新获取）
	XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock);



	/* ------------------------------------ Queue 队列库 ------------------------------------ */

	#ifndef XRT_NO_QUEUE

		typedef enum xqueue_result
		{
			XQUEUE_OK = 0,
			XQUEUE_EMPTY = 1,
			XQUEUE_FULL = 2,
			XQUEUE_CLOSED = 3,
			XQUEUE_TIMEOUT = 4,
			XQUEUE_ERROR = -1
		} xqueue_result;

		typedef enum xqueue_kind
		{
			XQUEUE_KIND_SPSC = 1,
			XQUEUE_KIND_MPSC = 2,
			XQUEUE_KIND_MPMC = 3
		} xqueue_kind;

		typedef struct xqueuebase_struct
		{
			uint32 iKind;
			volatile uint32 bClosed;
		} xqueuebase;

		typedef struct xqueue_config
		{
			uint32 iCapacity;
			uint32 iFlags;
		} xqueue_config;

		typedef void (*xqueue_drain_fn)(ptr pItem, ptr pUserData);

		typedef struct xspscq_struct
		{
			xqueuebase tBase;
			uint32 iCapacity;
			uint32 iMask;
			volatile uint32 iHead;
			uint8 _pad0[64];
			volatile uint32 iTail;
			ptr* arrItems;
		} xspscq_struct, *xspscq;

		typedef struct xmpscq_slot
		{
			volatile uint64 iSeq;
			ptr pItem;
		} xmpscq_slot;

		typedef struct xmpscq_struct
		{
			xqueuebase tBase;
			uint32 iCapacity;
			uint32 iMask;
			uint8 _pad0[64];
			volatile uint64 iHead;
			uint8 _pad1[64];
			volatile uint64 iTail;
			xmpscq_slot* arrSlots;
		} xmpscq_struct, *xmpscq;

		typedef xmpscq_slot xmpmcq_slot;

		typedef struct xmpmcq_struct
		{
			xqueuebase tBase;
			uint32 iCapacity;
			uint32 iMask;
			uint8 _pad0[64];
			volatile uint64 iHead;
			uint8 _pad1[64];
			volatile uint64 iTail;
			xmpmcq_slot* arrSlots;
		} xmpmcq_struct, *xmpmcq;

		XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg);
		XXAPI void xrtSPSCQUnit(xspscq pQueue);
		XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg);
		XXAPI void xrtSPSCQDestroy(xspscq pQueue);
		XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem);
		XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem);
		XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue);
		XXAPI void xrtSPSCQClose(xspscq pQueue);
		XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		XXAPI bool xrtSPSCQReset(xspscq pQueue);

		XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg);
		XXAPI void xrtMPSCQUnit(xmpscq pQueue);
		XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg);
		XXAPI void xrtMPSCQDestroy(xmpscq pQueue);
		XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem);
		XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem);
		XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount);
		XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap);
		XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue);
		XXAPI void xrtMPSCQClose(xmpscq pQueue);
		XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		XXAPI bool xrtMPSCQReset(xmpscq pQueue);

		XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg);
		XXAPI void xrtMPMCQUnit(xmpmcq pQueue);
		XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg);
		XXAPI void xrtMPMCQDestroy(xmpmcq pQueue);
		XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem);
		XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem);
		XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount);
		XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap);
		XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue);
		XXAPI void xrtMPMCQClose(xmpmcq pQueue);
		XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		XXAPI bool xrtMPMCQReset(xmpmcq pQueue);

		XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue);
		XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue);

		#ifndef XRT_NO_QUEUE_WAIT

			typedef struct xmpscqwait_struct
			{
				xmpscq_struct tQueue;
				xsem hItems;
				xmutex hPopLock;
				volatile long iWaiters;
			} xmpscqwait_struct, *xmpscqwait;

			XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg);
			XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue);
			XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg);
			XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue);
			XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem);
			XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem);
			XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem);
			XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs);
			XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue);
			XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue);

		#endif

	#endif
	
	
	
	/* ------------------------------------ Coroutine 协程库 ------------------------------------ */
	/*
		依赖项：
			Thread 函数库
			Time 函数库
	*/
	
	// 协程状态
	#define XRT_CO_READY         0      // 已创建，尚未运行
	#define XRT_CO_RUNNING       1      // 正在运行
	#define XRT_CO_SUSPENDED     2      // 已挂起 (yield)
	#define XRT_CO_DEAD          3      // 已结束

	// 协程终态原因
	#define XRT_CO_TERM_NONE         0
	#define XRT_CO_TERM_RETURNED     1
	#define XRT_CO_TERM_CANCELLED    2

	// 协程 backend 分层/风格：主线只保留 production inline-asm 实现
	#define XRT_CO_BACKEND_TIER_PRODUCTION   2
	#define XRT_CO_BACKEND_STYLE_INLINE_ASM  2
	
	// 默认栈大小
	#define XRT_CO_STACK_DEFAULT (64 * 1024)        // 64 KB
	#define XRT_CO_STACK_MIN     (8 * 1024)         // 8 KB (最小值保护)
	#define XRT_CO_STACK_MAX     (8 * 1024 * 1024)  // 8 MB (最大值保护)
	#define XRT_CO_WAIT_INFINITE UINT32_C(0xffffffff)
	
	// 协程入口函数类型
	typedef void (*xco_entry)(ptr pParam);
	typedef void (*xco_cleanup_proc)(ptr pArg);

	typedef struct xco_cleanup {
		xco_cleanup_proc Proc;
		ptr Arg;
		struct xco_cleanup* pPrev;
	} xco_cleanup;

	// 协程创建参数（标准 runtime API，未来扩展入口）
	#define XRT_CO_CREATE_NONE	0x00000000u

	typedef struct {
		size_t iStackSize;
		ptr pUserData;
		uint32 iFlags;
		uint32 iReserved;
	} xco_create_args;
	
	// 协程上下文（内部使用）
	typedef struct {
		ptr arrReg[40];   // 保存的寄存器/向量状态（各后端按需使用，兼容 Win64 XMM 与 ARM64 q8-q15 非易失状态）
	} __xrt_co_ctx;
	
	// 协程数据结构
	typedef struct xcoro_struct {
		int iState;             // 当前状态 (XRT_CO_*)
		uint32 __iFlags;        // 内部标志
		uint64 __iOwnerThreadId;// 所属线程ID
		xco_entry pfnEntry;     // 入口函数
		ptr pParam;             // 用户传入参数
		ptr pUserData;          // 用户自定义数据
		ptr pResult;            // 协程结果指针
		int64 iExitCode;        // 退出码（预留给 join/close）
		uint32 iTermReason;     // 终态原因 (XRT_CO_TERM_*)
		uint32 __iReserved;
		size_t iStackSize;      // 栈大小
		ptr __pStack;           // 分配的栈内存
		ptr __pStackMem;        // 栈保留区起始地址（含 guard page）
		size_t __iStackAllocSize; // 栈保留区总大小
		size_t __iStackGuardSize; // guard page 大小
		__xrt_co_ctx __tCtx;    // 上下文（汇编/ucontext 后端使用）
		ptr __hFiber;           // Windows Fiber 句柄
		ptr __pSched;           // 所属调度器指针（NULL=无调度器）
		int64 __iWakeTime;      // 唤醒时间戳（调度器用，0=不等待）
		ptr __pWaitObject;      // 当前等待对象（event/future 等）
		uint32 __iWaitKind;     // 当前等待类型（内部使用）
		uint32 __iWaitResult;   // 当前等待结果（内部使用）
		struct xcoro_struct* __pJoinTarget; // 当前等待的目标协程
		struct xcoro_struct* __pJoinWaitHead; // 等待本协程结束的协程队列头
		struct xcoro_struct* __pJoinWaitTail; // 等待本协程结束的协程队列尾
		struct xcoro_struct* __pWaitPrev; // 等待队列 prev
		struct xcoro_struct* __pWaitNext; // 等待队列 next
		uint32 __iJoinRefCount;   // join 引用计数，用于 pin DEAD 协程直到所有等待者返回
		xco_cleanup* __pCleanupTop; // 协程退出清理栈
		struct xcoro_struct* __pReadyPrev;  // ready queue prev
		struct xcoro_struct* __pReadyNext;  // ready queue next
		uint32 __iTimerIndex;   // timer heap index + 1（0=不在堆中）
	} xcoro_struct, *xcoro;
	
	// 调度器（不透明结构）
	typedef struct xrt_co_scheduler xcosched;

	// 协程事件对象（最小 wait source，当前支持单 waiter）
	typedef struct xcoevent_struct {
		xmutex pLock;
		bool bManualReset;
		bool bSignaled;
		struct xcoro_struct* pWaitHead;
		struct xcoro_struct* pWaitTail;
	} xcoevent_struct, *xcoevent;
	
	/* ---------- 协程生命周期 ---------- */

	// 扩展创建协程（支持栈大小、初始 user data 和保留 flags）
	XXAPI xcoro xrtCoCreateEx(xco_entry pfnEntry, ptr pParam, const xco_create_args* pArgs);
	
	// 创建协程
	XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize);
	
	// 销毁协程（协程必须处于 READY 或 DEAD 状态，且不能仍属于调度器）
	XXAPI void xrtCoDestroy(xcoro pCo);

	// 请求取消协程（第一版为协作式取消；READY 协程可直接进入终态）
	XXAPI bool xrtCoCancel(xcoro pCo);

	// 关闭协程句柄或向活协程发出关闭请求
	XXAPI bool xrtCoClose(xcoro pCo);

	// 等待协程结束（主线程可驱动 unscheduled/scheduler-managed 协程，协程内仅支持等待同 scheduler 目标）
	XXAPI bool xrtCoJoin(xcoro pCo);

	// 主动以给定退出码结束当前协程
	XXAPI void xrtCoExit(int64 iExitCode);
	
	/* ---------- 协程切换 ---------- */
	
	// 恢复协程（从调用方切换到协程执行）
	XXAPI bool xrtCoResume(xcoro pCo);
	
	// 挂起当前协程（从协程内部调用）
	XXAPI void xrtCoYield();
	
	/* ---------- 协程状态查询 ---------- */
	
	// 获取协程状态
	XXAPI int xrtCoGetState(xcoro pCo);
	
	// 获取当前正在运行的协程（不在协程中返回 NULL）
	XXAPI xcoro xrtCoGetCurrent();

	// 当前协程是否已收到取消请求
	XXAPI bool xrtCoIsCancelRequested();

	// 协程是否以取消方式结束
	XXAPI bool xrtCoWasCancelled(xcoro pCo);

	// 获取协程退出码
	XXAPI int64 xrtCoGetExitCode(xcoro pCo);

	// 设置当前协程结果指针
	XXAPI void xrtCoSetResult(ptr pResult);

	// 获取协程结果指针
	XXAPI ptr xrtCoGetResult(xcoro pCo);

	// 当前目标使用的协程 backend 名称
	XXAPI str xrtCoGetBackendName();

	// 当前目标使用的协程 backend 分层 (XRT_CO_BACKEND_TIER_*)
	XXAPI int xrtCoGetBackendTier();

	// 当前目标使用的协程 backend 风格 (XRT_CO_BACKEND_STYLE_*)
	XXAPI int xrtCoGetBackendStyle();
	
	/* ---------- 用户数据 ---------- */
	
	// 设置协程的用户自定义数据
	XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData);
	
	// 获取协程的用户自定义数据
	XXAPI ptr xrtCoGetUserData(xcoro pCo);

	// 向当前协程压入一个退出清理回调
	XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg);

	// 弹出当前协程顶部匹配的清理回调，可选择立即执行
	XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute);
	
	/* ---------- 协程调度器 ---------- */
	
	// 创建调度器
	XXAPI xcosched* xrtCoSchedCreate();

	// 获取当前线程的默认调度器（在协程内优先返回当前协程所属调度器）
	XXAPI xcosched* xrtCoSchedCurrent();
	
	// 销毁调度器（会自动销毁所有关联的协程）
	XXAPI void xrtCoSchedDestroy(xcosched* pSched);

	// 向调度器添加一个协程
	XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize);

	// 向调度器投递一个唤醒请求（可跨线程调用）
	XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo);

	// 执行一次调度轮询（可选择等待 post/timer）
	XXAPI bool xrtCoSchedPollOnce(xcosched* pSched, uint32 iTimeout);
	
	// 执行一轮调度（返回 true=还有存活协程）
	XXAPI bool xrtCoSchedStep(xcosched* pSched);
	
	// 持续运行调度器直到所有协程结束
	XXAPI void xrtCoSchedRun(xcosched* pSched);
	
	// 获取调度器中存活的协程数量
	XXAPI int xrtCoSchedGetAlive(xcosched* pSched);

	// 协程睡眠到指定的单调时钟毫秒 deadline
	XXAPI void xrtCoSleepUntil(int64 iDeadlineMs);

	// 等待到指定的单调时钟毫秒 deadline，若等待期间收到取消请求则返回 false
	XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs);

	// 创建协程事件对象（manual reset 或 auto reset）
	XXAPI xcoevent xrtCoEventCreate(bool bManualReset, bool bInitialState);

	// 销毁协程事件对象（有 waiter 时拒绝销毁）
	XXAPI void xrtCoEventDestroy(xcoevent pEvent);

	// 置位协程事件对象，可跨线程唤醒 waiter
	XXAPI void xrtCoEventSet(xcoevent pEvent);

	// 重置协程事件对象
	XXAPI void xrtCoEventReset(xcoevent pEvent);

	// 等待协程事件对象被置位，若等待期间收到取消请求则返回 false
	XXAPI bool xrtCoWaitEvent(xcoevent pEvent);

	// 在超时窗口内等待协程事件对象被置位，若超时或等待期间收到取消请求则返回 false
	XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs);

	// 等待协程事件对象直到指定单调时钟毫秒 deadline，若超时或等待期间收到取消请求则返回 false
	XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs);
	
	// 协程休眠（挂起当前协程，等待指定毫秒后自动恢复，需配合调度器使用）
	XXAPI void xrtCoSleep(uint32 iMs);
	
	
	
	/* ------------------------------------ Hash 函数库 ------------------------------------ */
	
	/*
		Hash32 - nmhash32x [Ver2.0, Update : 2024/10/18 from https://github.com/rurban/smhasher]
			使用协议注意事项：
				BSD 2-Clause 协议
				允许个人使用、商业使用
				复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
	*/
	
	// 默认 seed
	#define HASH32_SEED		0
	
	// 计算 32 位哈希值
	XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed);
	XXAPI uint32 xrtHash32(ptr key, size_t len);
	
	/*
		Hash64 - rapidhash [Ver1.0, Update : 2024/10/18 from https://github.com/Nicoshev/rapidhash]
			使用协议注意事项：
				BSD 2-Clause 协议
				允许个人使用、商业使用
				复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
	*/
	
	// 默认 seed
	#define HASH64_SEED		(0xbdd89aa982704029ull)
	
	// 计算 64 位哈希值
	XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64(ptr key, size_t len);
	XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
	XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
	XXAPI uint64 xrtHash64_Nano(ptr key, size_t len);
	
	
	
	/* ------------------------------------ Network 函数库 ------------------------------------ */
	
	// 获取本机 IP ( 需使用 xrtFree 释放 )
	str xrtGetLocalIP();
	
	// 获取本机 IP ( 返回 uint32 )
	uint32 xrtGetLocalRawIP();
	
	// 获取本机 MAC 地址 ( 需使用 xrtFree 释放 )
	str xrtGetLocalMAC();
	
	// 获取本机名称 ( 需使用 xrtFree 释放 )
	str xrtGetLocalName();
	
	
	
	/* ------------------------------------ Crypto 加密算法库 ------------------------------------ */
	
	/*
		算法来源：
			SHA-256:            Brad Conte (Public Domain)
			SHA-512/384:        原创实现 (FIPS 180-4)
			ChaCha20-Poly1305:  portable8439 (CC0-1.0) + poly1305-donna (Public Domain)
			AES-128/256-GCM:    Steven M. Gibson / GRC.com (Public Domain)
			X25519:             Mike Hamburg / STROBE (MIT License)
			ECDH/ECDSA P-256:   原创实现 (FIPS 186-4 / SEC 2)
			RSA:                axTLS bignum (BSD License)
			HKDF:               原创实现 (RFC 5869)
	*/
	
	// SHA-256 上下文结构
	typedef struct {
		uint32 state[8];
		uint8 buffer[64];
		uint32 len;
		uint64 bits;
	} xsha256_ctx;
	
	// SHA-1 上下文结构 (用于 WebSocket 握手等)
	typedef struct {
		uint32 state[5];
		uint8 buffer[64];
		uint32 len;
		uint64 bits;
	} xsha1_ctx;
	
	// SHA-512 上下文结构 (同时用作 SHA-384 上下文)
	typedef struct {
		uint64 state[8];
		uint8 buffer[128];
		uint32 len;
		uint64 bits;
	} xsha512_ctx;
	
	// SHA-256 哈希
	XXAPI void xrtSHA256(const ptr pData, size_t iLen, uint8 *pOut);
	XXAPI void xrtSHA256Init(xsha256_ctx *pCtx);
	XXAPI void xrtSHA256Update(xsha256_ctx *pCtx, const ptr pData, size_t iLen);
	XXAPI void xrtSHA256Final(xsha256_ctx *pCtx, uint8 *pOut);
	
	// SHA-1 哈希 (用于 WebSocket 握手)
	XXAPI void xrtSHA1(const ptr pData, size_t iLen, uint8 *pOut);
	XXAPI void xrtSHA1Init(xsha1_ctx *pCtx);
	XXAPI void xrtSHA1Update(xsha1_ctx *pCtx, const ptr pData, size_t iLen);
	XXAPI void xrtSHA1Final(xsha1_ctx *pCtx, uint8 *pOut);
	
	// SHA-384 哈希 (基于 SHA-512, 截取 48 字节)
	XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8 *pOut);
	XXAPI void xrtSHA384Init(xsha512_ctx *pCtx);
	XXAPI void xrtSHA384Final(xsha512_ctx *pCtx, uint8 *pOut);
	
	// SHA-512 哈希
	XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8 *pOut);
	XXAPI void xrtSHA512Init(xsha512_ctx *pCtx);
	XXAPI void xrtSHA512Update(xsha512_ctx *pCtx, const ptr pData, size_t iLen);
	XXAPI void xrtSHA512Final(xsha512_ctx *pCtx, uint8 *pOut);
	
	// HMAC-SHA256
	XXAPI void xrtHMAC_SHA256(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
	
	// HMAC-SHA384
	XXAPI void xrtHMAC_SHA384(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
	
	// HMAC-SHA512
	XXAPI void xrtHMAC_SHA512(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
	
	// ChaCha20 流加密 (RFC 8439)
	XXAPI void xrtChaCha20(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, uint32 iCounter, const uint8 *pIn, size_t iLen);
	
	// ChaCha20-Poly1305 AEAD 加密/解密 (RFC 8439)
	// 加密: pOut 需要 iLen + 16 字节空间 (密文 + 16字节tag)
	// 解密: iLen 包含 16 字节 tag，返回 false 表示验证失败
	XXAPI bool xrtChaCha20Poly1305Encrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	XXAPI bool xrtChaCha20Poly1305Decrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// AES-128-GCM AEAD 加密/解密
	// 加密: pOut 需要 iLen + 16 字节空间 (密文 + 16字节tag)
	// 解密: iLen 包含 16 字节 tag，返回 false 表示验证失败
	XXAPI bool xrtAES128GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	XXAPI bool xrtAES128GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// AES-256-GCM AEAD 加密/解密
	// 加密: pOut 需要 iLen + 16 字节空间 (密文 + 16字节tag)
	// 解密: iLen 包含 16 字节 tag，返回 false 表示验证失败
	XXAPI bool xrtAES256GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	XXAPI bool xrtAES256GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// X25519 密钥交换 (RFC 7748)
	XXAPI void xrtX25519Keypair(uint8 *pPrivKey, uint8 *pPubKey);              // 生成密钥对 (各 32 字节)
	XXAPI void xrtX25519SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (32 字节)
	
	// X448 密钥交换 (RFC 7748)
	XXAPI void xrtX448Keypair(uint8 *pPrivKey, uint8 *pPubKey);                // 生成密钥对 (各 56 字节)
	XXAPI void xrtX448SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);    // 计算共享密钥 (56 字节)
	
	// ECDH secp256r1 (P-256) 密钥交换 (TLS 1.2 ECDHE)
	XXAPI void xrtECDHSecp256r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);       // 生成密钥对 (私钥 32 字节, 公钥 65 字节: 0x04||X||Y)
	XXAPI void xrtECDHSecp256r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (32 字节)
	
	// ECDH secp384r1 (P-384) 密钥交换
	XXAPI void xrtECDHSecp384r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);       // 生成密钥对 (私钥 48 字节, 公钥 97 字节: 0x04||X||Y)
	XXAPI void xrtECDHSecp384r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (48 字节)
	
	// Ed25519 签名 (RFC 8032)
	XXAPI void xrtEd25519Keypair(uint8 *pSeed, uint8 *pPubKey);               // 生成种子和公钥 (32 + 32 字节)
	XXAPI void xrtEd25519PublicKey(uint8 *pPubKey, const uint8 *pSeed);       // 从 32 字节 seed 导出公钥
	XXAPI bool xrtEd25519Sign(uint8 *pSig, const uint8 *pMsg, size_t iMsgLen, const uint8 *pSeed); // 生成 64 字节签名

	// ECDSA / Ed25519 签名验证 (用于 TLS 证书验证)
	XXAPI bool xrtEd25519Verify(const uint8 *pMsg, size_t iMsgLen, const uint8 *pSig, const uint8 *pPubKey);
	XXAPI bool xrtECDSAVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pPubKey, size_t iPubKeyLen);
	
	// RSA 模幂运算 + RSA-PSS 签名验证 (axTLS bignum, BSD License)
	XXAPI int  xrtRSAModPow(const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz, const uint8 *pMsg, size_t iMsgSz, uint8 *pOut, size_t iOutSz);
	XXAPI bool xrtRSAPSSVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
	
	// RSA PKCS#1 v1.5 签名验证 (TLS 1.2 证书链)
	XXAPI bool xrtRSAPKCS1Verify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
	
	// HKDF 密钥派生 (RFC 5869, 基于 SHA-256)
	XXAPI void xrtHKDFExtract(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
	XXAPI void xrtHKDFExpand(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen);
	
	// HKDF-SHA384 密钥派生 (RFC 5869, 基于 SHA-384)
	XXAPI void xrtHKDFExtract_SHA384(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
	XXAPI void xrtHKDFExpand_SHA384(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen);
	
	// 加密安全随机数 (Windows: RtlGenRandom, Linux: /dev/urandom)
	XXAPI void xrtRandomBytes(uint8 *pBuf, size_t iLen);

	/* ------------------------------------ Network / TLS 共享状态定义 ------------------------------------ */
	/*
		依赖项：
			Network 函数库
			Crypto 加密算法库
	*/

	/* ---- 共享网络状态码 ---- */
	typedef enum {
	XRT_NET_OK        =  0,
	XRT_NET_ERROR     = -1,
	XRT_NET_AGAIN     = -2,
	XRT_NET_TIMEOUT   = -3,
	XRT_NET_CLOSED    = -4,
	XRT_NET_CANCELLED = -5,
	} xnet_result;

	/* ---- 共享套接字句柄类型 ---- */
	#if defined(_WIN32) || defined(_WIN64)
		typedef SOCKET xsocket;
		#define XSOCKET_INVALID INVALID_SOCKET
	#else
		typedef int xsocket;
		#define XSOCKET_INVALID (-1)
	#endif
	#define XRT_XSOCKET_DEFINED 1

	/* ---- TLS 会话与配置前置声明 ---- */
	typedef struct xrt_tls_session xtlssession;
	typedef struct xrt_tls_resume xtlsresume;
	typedef struct {
		const char* sCertFile;
		const char* sKeyFile;
		const char* sCaFile;
		const char* sHostName;
		bool bVerifyPeer;
		void (*OnSNI)(xtlssession *pSession, const char *sHostName, ptr pUserData);
		ptr pSNIUserData;
		bool bAllowTLS12Ed25519;
		uint16 iMaxVersion;
		const xtlsresume* pResume;
	} xtlsconfig;

	/* ------------------------------------ Regex 正则表达式模块 ------------------------------------ */
	
	#ifndef XRT_NO_REGEX
	// Regex 错误码
	#define XRT_REGEX_ERR_MEM			(-1)	// 内存不足
	#define XRT_REGEX_ERR_PARSE		(-2)	// 解析失败
	#define XRT_REGEX_ERR_LIMIT		(-3)	// 超出限制
	
	// Regex 标志
	typedef uint32 xregexflags;
	#define XRT_REGEX_FLAG_INSENSITIVE	((xregexflags)1u)	// (?i) 不区分大小写
	#define XRT_REGEX_FLAG_MULTILINE	((xregexflags)2u)	// (?m) 多行模式（^$匹配行首尾）
	#define XRT_REGEX_FLAG_DOTNEWLINE	((xregexflags)4u)	// (?s) . 匹配换行符
	#define XRT_REGEX_FLAG_UNGREEDY		((xregexflags)8u)	// (?U) 量词变为非贪婪
	
	// Regex 分配器
	typedef ptr (*xregexallocproc)(ptr pUserData, ptr pMem, size_t iPrevSize, size_t iNextSize);
	
	typedef struct {
		ptr pUserData;				// 用户上下文
		xregexallocproc procAlloc;	// 分配器回调
	} xregexalloc;
	
	// 匹配范围
	typedef struct {
		size_t iBegin;				// 起始位置
		size_t iEnd;				// 结束位置
	} xregexspan;
	
	// 前向声明
	typedef struct xrt_regex xregex;
	typedef struct xrt_regex_builder xregexbuilder;
	typedef struct xrt_regex_set xregexset;
	typedef struct xrt_regex_set_builder xregexsetbuilder;
	
	// 单模式
	XXAPI xregex* xrtRegexCreate(const char* sPatternNt);
	XXAPI int xrtRegexCreateFromBuilder(xregex** ppRegex, const xregexbuilder* pBuilder, const xregexalloc* pAlloc);
	XXAPI void xrtRegexDestroy(xregex* pRegex);
	XXAPI const char* xrtRegexGetErrorMsg(const xregex* pRegex);
	XXAPI size_t xrtRegexGetErrorPos(const xregex* pRegex);
	XXAPI int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);
	XXAPI int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);
	XXAPI int xrtRegexCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32 iCaptureCount);
	XXAPI int xrtRegexWhichCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
	XXAPI int xrtRegexIsMatchAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos);
	XXAPI int xrtRegexFindAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutSpan);
	XXAPI int xrtRegexCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32 iCaptureCount);
	XXAPI int xrtRegexWhichCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
	XXAPI uint32 xrtRegexCaptureCount(const xregex* pRegex);
	XXAPI const char* xrtRegexCaptureName(const xregex* pRegex, uint32 iCaptureIndex, size_t* pOutNameSize);
	
	// Builder
	XXAPI int xrtRegexBuilderCreate(xregexbuilder** ppBuilder, const char* sPattern, size_t iPatternSize, const xregexalloc* pAlloc);
	XXAPI void xrtRegexBuilderDestroy(xregexbuilder* pBuilder);
	XXAPI void xrtRegexBuilderSetFlags(xregexbuilder* pBuilder, xregexflags iFlags);
	
	// 克隆
	XXAPI int xrtRegexClone(xregex** ppOut, const xregex* pRegex, const xregexalloc* pAlloc);
	
	// 多模式
	XXAPI int xrtRegexSetBuilderCreate(xregexsetbuilder** ppBuilder, const xregexalloc* pAlloc);
	XXAPI void xrtRegexSetBuilderDestroy(xregexsetbuilder* pBuilder);
	XXAPI int xrtRegexSetBuilderAdd(xregexsetbuilder* pBuilder, const xregex* pRegex);
	XXAPI xregexset* xrtRegexSetCreate(const char* const* arrPatternsNt, size_t iPatternCount);
	XXAPI int xrtRegexSetCreateFromBuilder(xregexset** ppSet, const xregexsetbuilder* pBuilder, const xregexalloc* pAlloc);
	XXAPI void xrtRegexSetDestroy(xregexset* pSet);
	XXAPI const char* xrtRegexSetGetErrorMsg(const xregexset* pSet);
	XXAPI size_t xrtRegexSetGetErrorPos(const xregexset* pSet);
	XXAPI int xrtRegexSetIsMatch(xregexset* pSet, const char* sText, size_t iTextSize);
	XXAPI int xrtRegexSetMatches(xregexset* pSet, const char* sText, size_t iTextSize, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
	XXAPI int xrtRegexSetIsMatchAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);
	XXAPI int xrtRegexSetMatchesAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
	XXAPI int xrtRegexSetClone(xregexset** ppOut, const xregexset* pSet, const xregexalloc* pAlloc);
	
	#endif // XRT_NO_REGEX

	/* ------------------------------------ XNet V2 ------------------------------------ */
	/*
		依赖项：
			Thread 函数库
			Time 函数库
			Network / TLS 共享状态定义
			Crypto 加密算法库
		子库依赖：
			http util -> Time 函数库
			xcodec -> Crypto 加密算法库
			xhttp -> xurl、http util、xcodec、TLS
			xhttpd -> http util、xcodec、TLS
			xws -> xurl、xcodec、Crypto 加密算法库、TLS
			future / task 协程扩展 -> Coroutine 协程库（仅协程接口）
	*/

	#ifndef XRT_NO_NETWORK
	// 根据子库开关自动裁剪依赖更高的协议层接口
	#if defined(XRT_NO_XURL)
		#ifndef XRT_NO_HTTP_UTIL
			#define XRT_NO_HTTP_UTIL
		#endif
		#ifndef XRT_NO_XHTTP
			#define XRT_NO_XHTTP
		#endif
		#ifndef XRT_NO_XHTTPD
			#define XRT_NO_XHTTPD
		#endif
		#ifndef XRT_NO_XWS
			#define XRT_NO_XWS
		#endif
	#endif
	#if defined(XRT_NO_HTTP_UTIL)
		#ifndef XRT_NO_XHTTP
			#define XRT_NO_XHTTP
		#endif
		#ifndef XRT_NO_XHTTPD
			#define XRT_NO_XHTTPD
		#endif
		#ifndef XRT_NO_XWS
			#define XRT_NO_XWS
		#endif
	#endif
	#if defined(XRT_NO_XCODEC)
		#ifndef XRT_NO_XHTTP
			#define XRT_NO_XHTTP
		#endif
		#ifndef XRT_NO_XHTTPD
			#define XRT_NO_XHTTPD
		#endif
		#ifndef XRT_NO_XWS
			#define XRT_NO_XWS
		#endif
	#endif
	#else
	#ifndef XRT_NO_XURL
		#define XRT_NO_XURL
	#endif
	#ifndef XRT_NO_HTTP_UTIL
		#define XRT_NO_HTTP_UTIL
	#endif
	#ifndef XRT_NO_XCODEC
		#define XRT_NO_XCODEC
	#endif
	#ifndef XRT_NO_XHTTP
		#define XRT_NO_XHTTP
	#endif
	#ifndef XRT_NO_XHTTPD
		#define XRT_NO_XHTTPD
	#endif
	#ifndef XRT_NO_XWS
		#define XRT_NO_XWS
	#endif
	#endif

	#if !defined(XRT_NO_NETWORK)

	/* ============================== xnet base ============================== */

	// XNet 核心对象前向声明
	typedef struct xrt_net_engine   xnetengine;
	typedef struct xrt_net_mem_ctx  xnetmemctx;
	typedef struct xrt_net_worker   xnetworker;
	typedef struct xrt_net_listener xnetlistener;
	typedef struct xrt_net_stream   xnetstream;
	typedef struct xrt_net_dgram    xdgramsock;
	typedef struct xrt_net_chain    xnetchain;
	typedef struct xrt_net_future   xnetfuture;
	typedef struct xrt_net_proxy    xnetproxy;
	typedef struct xrt_promise      xpromise;
	typedef struct xrt_task         xtask;
	typedef struct xrt_task_group   xtaskgroup;
	typedef struct xrt_net_dgram_packet xnetdgrampkt;
	typedef xnetfuture xfuture;

	// 投递到网络引擎线程执行的任务回调
	typedef void (*xnet_task_fn)(xnetworker* pWorker, ptr pArg);

	// 网络地址、数据片段和引用块描述
	typedef struct {
	uint16 iFamily;
	uint16 iPort;
	uint32 iScopeId;
	uint8 aAddr[16];
	} xnetaddr;

	typedef struct {
	const void* pData;
	uint32 iLen;
	} xnetspan;

	typedef struct {
	const void* pData;
	uint32 iLen;
	void (*pfnRelease)(ptr pCtx, const void* pData, size_t iLen);
	ptr pReleaseCtx;
	} xnetbufref;

	// 引擎、监听、连接、UDP 与关闭行为标志
	#define XNET_ENGINE_F_NONE            0x00000000u
	#define XNET_ENGINE_F_AUTO_WORKERS    0x00000001u
	#define XNET_LISTEN_F_NONE            0x00000000u
	#define XNET_LISTEN_F_REUSE_ADDR      0x00000001u
	#define XNET_LISTEN_F_REUSE_PORT      0x00000002u
	#define XNET_LISTEN_F_NO_DELAY        0x00000004u
	#define XNET_LISTEN_F_KEEPALIVE       0x00000008u
	#define XNET_CONNECT_F_NONE           0x00000000u
	#define XNET_CONNECT_F_NO_DELAY       0x00000001u
	#define XNET_CONNECT_F_KEEPALIVE      0x00000002u
	#define XNET_PROXY_NONE               0
	#define XNET_PROXY_SOCKS5             1
	#define XNET_PROXY_HTTP_CONNECT       2
	#define XNET_PROXY_HOST_CAP           256u
	#define XNET_PROXY_USER_CAP           128u
	#define XNET_PROXY_PASS_CAP           128u
	#define XNET_DGRAM_F_NONE             0x00000000u
	#define XNET_DGRAM_F_REUSE_ADDR       0x00000001u
	#define XNET_DGRAM_F_REUSE_PORT       0x00000002u
	#define XNET_CLOSE_F_ABORT            0x00000001u
	#define XNET_CLOSE_F_GRACEFUL         0x00000002u
	#define XNET_CLOSE_F_WAIT_PEER        0x00000004u

	// 引擎、监听、连接和 UDP 套接字配置
	typedef struct {
	uint32 iWorkerCount;
	uint32 iFlags;
	uint32 iSqEntries;
	uint32 iCqEntries;
	uint32 iAcceptBatch;
	uint32 iCmdQueueSize;
	uint32 iTimerTickMs;
	uint32 iTimerWheelSlots;
	uint32 iDefaultHighWater;
	uint32 iDefaultLowWater;
	uint32 iSmallBlockSize;
	uint32 iMediumBlockSize;
	uint32 iLargeBlockSize;
	uint32 iBlockCachePerWorker;
	uint32 iMaxConnsPerWorker;
	} xnetengineconfig;

	typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	} xnetlistenconfig;

	typedef struct {
	int iType;
	char sHost[XNET_PROXY_HOST_CAP];
	uint16 iPort;
	char sUser[XNET_PROXY_USER_CAP];
	char sPass[XNET_PROXY_PASS_CAP];
	} xnetproxyconfig;

	struct xrt_net_proxy {
	volatile long iRefCount;
	xnetproxyconfig tConfig;
	};

	typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iFlags;
	uint32 iConnectTimeoutMs;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	xnetproxy* pProxy;
	} xnetconnectconfig;

	typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iRecvBatch;
	uint32 iSendQueueLimit;
	} xnetdgramconfig;

	/* ============================== xnet mem ============================== */

	// XNet 内存块分类与引用块标记
	#define XNET_MEM_CLASS_SMALL    1u
	#define XNET_MEM_CLASS_MEDIUM   2u
	#define XNET_MEM_CLASS_LARGE    3u
	#define XNET_MEM_CLASS_DYNAMIC  4u
	#define XNET_MEM_CLASS_REF      5u
	#define XNET_BLK_F_REF          0x0001u

	typedef struct __xnet_blk __xnet_blk;

	// XNet 内存池配置、统计和上下文结构
	typedef struct {
	uint32 iSmallBlockSize;
	uint32 iMediumBlockSize;
	uint32 iLargeBlockSize;
	uint32 iSmallCacheLimit;
	uint32 iMediumCacheLimit;
	uint32 iLargeCacheLimit;
	} xnetmemconfig;

	typedef struct {
	uint32 iSmallCached;
	uint32 iMediumCached;
	uint32 iLargeCached;
	uint64 iSmallAllocCount;
	uint64 iMediumAllocCount;
	uint64 iLargeAllocCount;
	uint64 iDynamicAllocCount;
	uint64 iRefAllocCount;
	uint64 iSmallReuseCount;
	uint64 iMediumReuseCount;
	uint64 iLargeReuseCount;
	uint64 iDynamicFreeCount;
	uint64 iRefFreeCount;
	} xnetmemstats;

	struct xrt_net_mem_ctx {
	xnetmemconfig tConfig;
	__xnet_blk* pSmallFree;
	__xnet_blk* pMediumFree;
	__xnet_blk* pLargeFree;
	xnetmemstats tStats;
	};

	struct xrt_net_chain {
	__xnet_blk* pHead;
	__xnet_blk* pTail;
	ptr pMemCtx;
	uint32 iBytes;
	uint32 iBlockCount;
	};

	#ifndef XRT_NO_XURL
	/* ============================== xurl ============================== */

	// URL 解析结果标记与固定缓冲区容量
	#define XRT_URL_F_NONE           0x00000000u
	#define XRT_URL_F_ABSOLUTE       0x00000001u
	#define XRT_URL_F_TARGET_ONLY    0x00000002u
	#define XRT_URL_F_HAS_AUTHORITY  0x00000004u
	#define XRT_URL_F_HAS_USERINFO   0x00000008u
	#define XRT_URL_F_HAS_HOST       0x00000010u
	#define XRT_URL_F_HAS_PORT       0x00000020u
	#define XRT_URL_F_HAS_PATH       0x00000040u
	#define XRT_URL_F_HAS_QUERY      0x00000080u
	#define XRT_URL_F_HAS_FRAGMENT   0x00000100u
	#define XRT_URL_F_SECURE         0x00000200u
	#define XRT_QUERY_F_NONE         0x00000000u
	#define XRT_QUERY_F_HAS_VALUE    0x00000001u
	#define XRT_URL_FIXED_HOST_CAP   256u
	#define XRT_URL_FIXED_PATH_CAP   2048u

	// 字符串视图、URL 视图、查询参数与固定 URL 结构
	typedef struct {
	const char* sPtr;
	size_t iLen;
	} xrtstrview;

	typedef struct {
	uint32 iFlags;
	uint16 iPort;
	xrtstrview tScheme;
	xrtstrview tAuthority;
	xrtstrview tUserInfo;
	xrtstrview tHost;
	xrtstrview tPath;
	xrtstrview tQuery;
	xrtstrview tFragment;
	xrtstrview tTarget;
	} xrturlview;

	typedef struct {
	uint32 iFlags;
	xrtstrview tKey;
	xrtstrview tValue;
	} xrtquerypair;

	typedef struct {
	bool bHttps;
	char sHost[XRT_URL_FIXED_HOST_CAP];
	uint16 iPort;
	char sPath[XRT_URL_FIXED_PATH_CAP];
	} xurl_struct, *xurl;
	#endif /* !XRT_NO_XURL */

	#ifndef XRT_NO_HTTP_UTIL
	/* ============================== http util ============================== */

	// HTTP 头、Cookie 与参数视图结构
	typedef struct {
	xrtstrview tName;
	xrtstrview tValue;
	} xrtheaderpair;

	typedef struct {
	xrtstrview tName;
	xrtstrview tValue;
	} xrtcookiepair;

	#define XRT_HTTP_PARAM_F_NONE       0x00000000u
	#define XRT_HTTP_PARAM_F_HAS_VALUE  0x00000001u

	typedef struct {
	uint32 iFlags;
	xrtstrview tName;
	xrtstrview tValue;
	} xrthttpparam;

	#define XRT_HTTP_MEDIA_TYPE_F_NONE        0x00000000u
	#define XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX  0x00000001u
	#define XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS  0x00000002u

	// Media-Type 与 Content-Disposition 视图
	typedef struct {
	uint32 iFlags;
	xrtstrview tType;
	xrtstrview tSubType;
	xrtstrview tSuffix;
	xrtstrview tParams;
	} xrtmediatypeview;

	#define XRT_HTTP_CONTENT_DISPOSITION_F_NONE              0x00000000u
	#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_PARAMS        0x00000001u
	#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME          0x00000002u
	#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME      0x00000004u
	#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT  0x00000008u

	typedef struct {
	uint32 iFlags;
	xrtstrview tType;
	xrtstrview tParams;
	xrtstrview tName;
	xrtstrview tFileName;
	xrtstrview tFileNameExt;
	xrtstrview tFileNameCharset;
	xrtstrview tFileNameLanguage;
	} xrtcontentdispositionview;

	#define XRT_SET_COOKIE_F_NONE            0x00000000u
	#define XRT_SET_COOKIE_F_HAS_VALUE       0x00000001u
	#define XRT_SET_COOKIE_F_HAS_DOMAIN      0x00000002u
	#define XRT_SET_COOKIE_F_HAS_PATH        0x00000004u
	#define XRT_SET_COOKIE_F_HAS_EXPIRES     0x00000008u
	#define XRT_SET_COOKIE_F_HAS_MAX_AGE     0x00000010u
	#define XRT_SET_COOKIE_F_HAS_SAME_SITE   0x00000020u
	#define XRT_SET_COOKIE_F_SECURE          0x00000040u
	#define XRT_SET_COOKIE_F_HTTP_ONLY       0x00000080u
	#define XRT_SET_COOKIE_F_PARTITIONED     0x00000100u
	#define XRT_SET_COOKIE_F_SAME_PARTY      0x00000200u
	#define XRT_SET_COOKIE_F_HAS_PRIORITY    0x00000400u
	#define XRT_SAME_SITE_UNSPECIFIED        0u
	#define XRT_SAME_SITE_LAX                1u
	#define XRT_SAME_SITE_STRICT             2u
	#define XRT_SAME_SITE_NONE               3u
	#define XRT_COOKIE_PRIORITY_UNSPECIFIED  0u
	#define XRT_COOKIE_PRIORITY_LOW          1u
	#define XRT_COOKIE_PRIORITY_MEDIUM       2u
	#define XRT_COOKIE_PRIORITY_HIGH         3u

	// Set-Cookie 视图
	typedef struct {
	uint32 iFlags;
	int32_t iMaxAge;
	uint8 iSameSite;
	uint8 iPriority;
	xrtstrview tName;
	xrtstrview tValue;
	xrtstrview tDomain;
	xrtstrview tPath;
	xrtstrview tExpires;
	} xrtsetcookieview;

	#define XRT_MULTIPART_F_NONE                   0x00000000u
	#define XRT_MULTIPART_F_HAS_CONTENT_DISP       0x00000001u
	#define XRT_MULTIPART_F_HAS_NAME               0x00000002u
	#define XRT_MULTIPART_F_HAS_FILENAME           0x00000004u
	#define XRT_MULTIPART_F_HAS_CONTENT_TYPE       0x00000008u
	#define XRT_MULTIPART_F_HAS_TRANSFER_ENCODING  0x00000010u
	#define XRT_MULTIPART_F_HAS_FILENAME_EXT       0x00000020u

	// Multipart 视图、流式解析配置和限额配置
	typedef struct {
	uint32 iFlags;
	xrtstrview tHeaders;
	xrtstrview tBody;
	xrtstrview tContentDisposition;
	xrtstrview tName;
	xrtstrview tFileName;
	xrtstrview tFileNameExt;
	xrtstrview tFileNameCharset;
	xrtstrview tFileNameLanguage;
	xrtstrview tContentType;
	xrtstrview tTransferEncoding;
	} xrtmultipartpartview;

	typedef struct {
	size_t iMaxBufferedBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxPartHeaders;
	size_t iTailReserve;
	} xrtmultipartstreamconfig;

	typedef struct {
	size_t iMaxNameBytes;
	size_t iMaxValueBytes;
	size_t iMaxPairs;
	size_t iMaxHeaderLineBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxHeaderCount;
	size_t iMaxTokenBytes;
	size_t iMaxBoundaryBytes;
	size_t iMaxMultipartHeaders;
	size_t iMaxMultipartParts;
	size_t iMaxMultipartBytes;
	} xrthttputillimits;

	typedef enum {
	XRT_MULTIPART_STREAM_RESULT_ERROR      = -1,
	XRT_MULTIPART_STREAM_RESULT_NEED_MORE  = 0,
	XRT_MULTIPART_STREAM_RESULT_PART_BEGIN = 1,
	XRT_MULTIPART_STREAM_RESULT_DATA       = 2,
	XRT_MULTIPART_STREAM_RESULT_PART_END   = 3,
	XRT_MULTIPART_STREAM_RESULT_END        = 4
	} xrtmultipartstreamresult;

	#define XRT_MULTIPART_STREAM_ERR_NONE             0u
	#define XRT_MULTIPART_STREAM_ERR_INVALID_BOUNDARY 1u
	#define XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT     2u
	#define XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT     3u
	#define XRT_MULTIPART_STREAM_ERR_INVALID_HEADER   4u
	#define XRT_MULTIPART_STREAM_ERR_TRUNCATED        5u

	// Multipart 流式事件与解析状态
	typedef struct {
	xrtmultipartstreamresult iResult;
	xrtmultipartpartview tPart;
	xrtstrview tData;
	} xrtmultipartstreamevent;

	typedef struct {
	char* pBuffer;
	size_t iBufferLen;
	size_t iBufferCap;
	size_t iCursor;
	size_t iBoundaryPos;
	size_t iAfterBoundary;
	size_t iBoundaryLen;
	size_t iMaxBufferedBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxPartHeaders;
	size_t iTailReserve;
	uint32 iError;
	uint32 iState;
	bool bFinalBoundary;
	bool bFinishedInput;
	xrtmultipartpartview tCurrentPart;
	char aBoundary[71];
	} xrtmultipartstream;
	#endif

	#ifndef XRT_NO_XCODEC
	/* ============================== codec ============================== */

	// 协议编解码器状态码
	typedef enum {
	XCODEC_STATUS_ERROR = -1,
	XCODEC_STATUS_NEED_MORE = 0,
	XCODEC_STATUS_FRAME = 1
	} xcodecstatus;

	// 编解码器种类与帧标记
	#define XCODEC_KIND_NONE    0u
	#define XCODEC_KIND_LINE    1u
	#define XCODEC_KIND_LENGTH  2u
	#define XCODEC_KIND_HTTP1   3u
	#define XCODEC_KIND_WS      4u
	#define XCODEC_FRAME_F_NONE        0x00000000u
	#define XCODEC_FRAME_F_TEXT        0x00000001u
	#define XCODEC_FRAME_F_BINARY      0x00000002u
	#define XCODEC_FRAME_F_REQUEST     0x00000004u
	#define XCODEC_FRAME_F_RESPONSE    0x00000008u
	#define XCODEC_FRAME_F_FIN         0x00000010u
	#define XCODEC_FRAME_F_MASKED      0x00000020u
	#define XCODEC_FRAME_F_UPGRADE     0x00000040u
	#define XCODEC_FRAME_F_KEEPALIVE   0x00000080u
	#define XCODEC_FRAME_F_CHUNKED     0x00000100u
	#define XCODEC_FRAME_F_CONTROL     0x00000200u

	// 通用帧描述与解析器操作表
	typedef struct {
	uint32 iKind;
	uint32 iFlags;
	size_t iHeaderBytes;
	size_t iPayloadOffset;
	size_t iPayloadBytes;
	size_t iFrameBytes;
	uint64 iMeta0;
	uint64 iMeta1;
	} xcodecframe;

	typedef xcodecstatus (*xcodec_parse_fn)(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
	typedef void (*xcodec_reset_fn)(ptr pCtx);

	typedef struct {
	xcodec_parse_fn Parse;
	xcodec_reset_fn Reset;
	} xcodecparserops;

	typedef struct {
	const xcodecparserops* pOps;
	ptr pCtx;
	} xcodecparser;

	// 行分隔与长度前缀解析器配置
	typedef struct {
	uint8 aDelimiter[4];
	uint32 iDelimiterLen;
	uint32 iMaxLineBytes;
	bool bStripDelimiter;
	} xcodeclinecodec;

	typedef struct {
	uint8 iFieldBytes;
	bool bBigEndian;
	int32_t iLengthAdjust;
	uint32 iMaxPayloadBytes;
	} xcodeclengthcodec;

	#define XCODEC_HTTP1_MAX_HEADERS 32u
	#define XCODEC_HTTP1_TOKEN_CAP   32u
	#define XCODEC_HTTP1_TARGET_CAP  256u
	#define XCODEC_HTTP1_VALUE_CAP   256u
	#define XCODEC_HTTP1_REASON_CAP  128u
	#define XCODEC_HTTP1_F_NONE       0x00000000u
	#define XCODEC_HTTP1_F_REQUEST    0x00000001u
	#define XCODEC_HTTP1_F_RESPONSE   0x00000002u
	#define XCODEC_HTTP1_F_CHUNKED    0x00000004u
	#define XCODEC_HTTP1_F_KEEPALIVE  0x00000008u
	#define XCODEC_HTTP1_F_UPGRADE    0x00000010u

	// HTTP/1 报文与 WebSocket 帧头结构
	typedef struct {
	char sName[XCODEC_HTTP1_TOKEN_CAP];
	char sValue[XCODEC_HTTP1_VALUE_CAP];
	} xcodechttp1header;

	typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iStatusCode;
	int64_t iContentLength;
	size_t iHeadBytes;
	char sMethod[XCODEC_HTTP1_TOKEN_CAP];
	char sTarget[XCODEC_HTTP1_TARGET_CAP];
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xcodechttp1header arrHeaders[XCODEC_HTTP1_MAX_HEADERS];
	} xcodechttp1msg;

	#define XCODEC_WS_OPCODE_CONT   0x0u
	#define XCODEC_WS_OPCODE_TEXT   0x1u
	#define XCODEC_WS_OPCODE_BINARY 0x2u
	#define XCODEC_WS_OPCODE_CLOSE  0x8u
	#define XCODEC_WS_OPCODE_PING   0x9u
	#define XCODEC_WS_OPCODE_PONG   0xAu
	#define XCODEC_WS_F_NONE     0x00000000u
	#define XCODEC_WS_F_FIN      0x00000001u
	#define XCODEC_WS_F_MASKED   0x00000002u
	#define XCODEC_WS_F_CONTROL  0x00000004u

	typedef struct {
	uint32 iFlags;
	uint8 iOpcode;
	uint8 aMask[4];
	uint64 iPayloadLen;
	size_t iHeaderBytes;
	} xcodecwsframeinfo;
	#endif

	/* ============================== xnet stream/dgram/sync ============================== */

	// 监听器、流和 UDP 套接字事件回调
	typedef struct {
	bool (*OnAccept)(ptr pOwner, xnetlistener* pListener, xnetstream* pStream);
	void (*OnError)(ptr pOwner, xnetlistener* pListener, int iSysErr);
	} xnetlistenerevents;

	typedef struct {
	void (*OnOpen)(ptr pOwner, xnetstream* pStream);
	void (*OnRecv)(ptr pOwner, xnetstream* pStream, xnetchain* pChain);
	void (*OnDrain)(ptr pOwner, xnetstream* pStream);
	void (*OnClose)(ptr pOwner, xnetstream* pStream, xnet_result iReason);
	void (*OnError)(ptr pOwner, xnetstream* pStream, int iSysErr);
	void (*OnHighWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
	void (*OnLowWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
	} xnetstreamevents;

	#define XNET_STREAM_WAIT_READABLE 0u
	#define XNET_STREAM_WAIT_WRITABLE 1u
	#define XNET_STREAM_WAIT_DRAIN    2u
	#define XNET_STREAM_WAIT_CLOSE    3u

	typedef struct {
	void (*OnRecv)(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain);
	void (*OnError)(ptr pOwner, xdgramsock* pSock, int iSysErr);
	} xnetdgramevents;

	#define XNET_WAIT_INFINITE UINT32_C(0xffffffff)
	#define XNET_WAITSRC_NONE     0u
	#define XNET_WAITSRC_FUTURE   1u
	#define XNET_WAITSRC_STREAM   2u
	#define XNET_WAITSRC_DGRAM    3u
	#define XNET_WAITSRC_LISTENER 4u

	// Future / Task 回调与状态结构
	typedef xnet_result (*xnet_future_task_fn)(xnetworker* pWorker, ptr pArg, ptr* ppValue);

	typedef enum {
	XFUTURE_PENDING = 0,
	XFUTURE_RESOLVED,
	XFUTURE_REJECTED,
	XFUTURE_CANCELLED,
	XFUTURE_CLOSED
	} xfuture_state;

	typedef enum {
	XTASK_CREATED = 0,
	XTASK_QUEUED,
	XTASK_RUNNING,
	XTASK_DONE,
	XTASK_CANCELLED,
	XTASK_CLOSED
	} xtask_state;

	#define XFUTURE_RESULT_F_NONE        0x00000000u
	#define XFUTURE_RESULT_F_OWN_VALUE   0x00000001u
	#define XFUTURE_RESULT_F_OWN_ERROR   0x00000002u
	#define XFUTURE_RESULT_F_SYS_ERROR   0x00000004u
	#define XFUTURE_RESULT_F_TIMEOUT     0x00000008u
	#define XFUTURE_RESULT_F_CANCELLED   0x00000010u
	#define XFUTURE_RESULT_F_CLOSED      0x00000020u
	#define XFUTURE_RESULT_F_GROUP_ALL   0x00000040u

	typedef struct {
	int32 iStatus;
	ptr pValue;
	str sError;
	uint32 iFlags;
	} xfuture_result;

	typedef struct {
	int iCount;
	ptr* arrValue;
	} xfuture_all_value;

	typedef int32 (*xtask_engine_fn)(xnetworker* pWorker, ptr pArg, xfuture_result* pOut);
	typedef int32 (*xtask_thread_fn)(ptr pArg, xfuture_result* pOut);
	typedef int32 (*xtask_co_fn)(ptr pArg, xfuture_result* pOut);
	typedef int32 (*xfuture_cont_fn)(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut);
	typedef void (*xfuture_finally_fn)(const xfuture_result* pIn, ptr pArg);

	typedef struct {
	uint32 iKind;
	union {
		xnetfuture* pFuture;
		struct {
			xnetstream* pStream;
			uint32 iWaitKind;
		} tStream;
		xdgramsock* pDgram;
		xnetlistener* pListener;
	} u;
	} xnetwaitsrc;

	typedef xnetwaitsrc xwaitsrc;

	#ifndef XRT_NO_XHTTP
	/* ============================== xhttp ============================== */

	#define XHTTP_METHOD_CAP         16u
	#define XHTTP_URL_CAP            1024u
	#define XHTTP_HOST_CAP           256u
	#define XHTTP_PATH_CAP           1024u
	#define XHTTP_HEADER_NAME_CAP    64u
	#define XHTTP_HEADER_VALUE_CAP   256u
	#define XHTTP_MAX_HEADERS        32u
	#define XHTTP_RESP_F_NONE        0x00000000u
	#define XHTTP_RESP_F_CHUNKED     0x00000001u
	#define XHTTP_RESP_F_KEEPALIVE   0x00000002u
	#define XHTTP_RESP_F_UPGRADE     0x00000004u

	typedef struct {
	char sName[XHTTP_HEADER_NAME_CAP];
	char sValue[XHTTP_HEADER_VALUE_CAP];
	} xhttpheader;

	typedef struct {
	bool bHttps;
	uint16 iPort;
	char sHost[XHTTP_HOST_CAP];
	char sPath[XHTTP_PATH_CAP];
	} xhttpurl;

	typedef struct {
	char sMethod[XHTTP_METHOD_CAP];
	char sURL[XHTTP_URL_CAP];
	xhttpurl tURL;
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	uint32 iHeaderCount;
	char* pBody;
	size_t iBodyLen;
	uint32 iTimeoutMs;
	uint32 iIdleTimeoutMs;
	bool bVerifyPeer;
	xnetproxy* pProxy;
	} xhttprequest;

	typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	int64_t iContentLength;
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
	} xhttpresponse;
	#endif

	#ifndef XRT_NO_XHTTPD
	/* ============================== xhttpd ============================== */

	typedef struct xrt_httpd_server xhttpdserver;
	typedef struct xrt_httpd_conn xhttpdconn;

	#define XHTTPD_METHOD_CAP         16u
	#define XHTTPD_TARGET_CAP         256u
	#define XHTTPD_PATH_CAP           256u
	#define XHTTPD_QUERY_CAP          256u
	#define XHTTPD_VERSION_CAP        32u
	#define XHTTPD_REASON_CAP         128u
	#define XHTTPD_HEADER_NAME_CAP    64u
	#define XHTTPD_HEADER_VALUE_CAP   256u
	#define XHTTPD_MAX_HEADERS        32u
	#define XHTTPD_REQ_F_NONE         0x00000000u
	#define XHTTPD_REQ_F_KEEPALIVE    0x00000001u
	#define XHTTPD_REQ_F_CHUNKED      0x00000002u
	#define XHTTPD_REQ_F_UPGRADE      0x00000004u
	#define XHTTPD_RESP_F_NONE        0x00000000u
	#define XHTTPD_RESP_F_CLOSE       0x00000001u

	typedef struct {
	char sName[XHTTPD_HEADER_NAME_CAP];
	char sValue[XHTTPD_HEADER_VALUE_CAP];
	} xhttpdheader;

	typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	int64_t iContentLength;
	char sMethod[XHTTPD_METHOD_CAP];
	char sTarget[XHTTPD_TARGET_CAP];
	char sPath[XHTTPD_PATH_CAP];
	char sQuery[XHTTPD_QUERY_CAP];
	char sVersion[XHTTPD_VERSION_CAP];
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
	} xhttpdrequest;

	typedef struct {
	uint32 iStatusCode;
	uint32 iFlags;
	uint32 iHeaderCount;
	char sReason[XHTTPD_REASON_CAP];
	xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
	char* pBody;
	size_t iBodyLen;
	} xhttpdresponse;

	typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	} xhttpdconfig;

	typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
	} xhttpdevents;
	#endif

	#ifndef XRT_NO_XWS
	/* ============================== xws ============================== */

	typedef struct xrt_ws_client xwsclient;
	typedef struct xrt_ws_server xwsserver;
	typedef struct xrt_ws_conn   xwsconn;

	#define XWS_URL_CAP              1024u
	#define XWS_HOST_CAP             256u
	#define XWS_PATH_CAP             1024u
	#define XWS_ORIGIN_CAP           256u
	#define XWS_PROTOCOL_CAP         128u
	#define XWS_CLOSE_REASON_CAP     123u
	#define XWS_CLOSE_NORMAL         1000u
	#define XWS_CLOSE_GOING_AWAY     1001u
	#define XWS_CLOSE_PROTOCOL       1002u
	#define XWS_CLOSE_UNSUPPORTED    1003u
	#define XWS_CLOSE_TOO_BIG        1009u
	#define XWS_CLOSE_INTERNAL       1011u

	typedef struct {
	char sURL[XWS_URL_CAP];
	char sOrigin[XWS_ORIGIN_CAP];
	char sProtocol[XWS_PROTOCOL_CAP];
	uint32 iConnectTimeoutMs;
	uint32 iRecvLimit;
	bool bVerifyPeer;
	xnetproxy* pProxy;
	} xwsclientconfig;

	typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	char sProtocol[XWS_PROTOCOL_CAP];
	} xwsserverconfig;

	typedef struct {
	void (*OnOpen)(ptr pOwner, xwsclient* pClient);
	void (*OnText)(ptr pOwner, xwsclient* pClient, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsclient* pClient, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsclient* pClient, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsclient* pClient, const void* pData, size_t iLen);
	} xwsclientevents;

	typedef struct {
	void (*OnOpen)(ptr pOwner, xwsserver* pServer, xwsconn* pConn);
	void (*OnText)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const char* pData, size_t iLen);
	void (*OnBinary)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnClose)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, int iSysErr);
	void (*OnPing)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	void (*OnPong)(ptr pOwner, xwsserver* pServer, xwsconn* pConn, const void* pData, size_t iLen);
	} xwsserverevents;
	#endif
	XXAPI void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort);
	XXAPI xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort);
	XXAPI xnet_result xrtNetResolve(const char* sHost, xnetaddr* pAddr);
	XXAPI const char* xrtNetAddrToStr(const xnetaddr* pAddr);
	// 默认配置初始化
	XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg);
	XXAPI void xrtNetListenConfigInit(xnetlistenconfig* pCfg);
	XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg);
	XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg);
	XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy);
	XXAPI void xrtNetProxyRelease(xnetproxy* pProxy);
	XXAPI void xrtNetConnectConfigInit(xnetconnectconfig* pCfg);
	XXAPI void xrtNetDgramConfigInit(xnetdgramconfig* pCfg);

	// XNet 内存上下文与数据链操作
	XXAPI void xrtNetMemConfigInit(xnetmemconfig* pCfg);
	XXAPI void xrtNetMemCtxInit(xnetmemctx* pCtx, const xnetmemconfig* pCfg);
	XXAPI void xrtNetMemCtxTrim(xnetmemctx* pCtx);
	XXAPI void xrtNetMemCtxUnit(xnetmemctx* pCtx);
	XXAPI void xrtNetMemCtxGetStats(const xnetmemctx* pCtx, xnetmemstats* pStats);
	XXAPI void xrtNetChainInitEx(xnetchain* pChain, xnetmemctx* pMemCtx);
	XXAPI void xrtNetChainInit(xnetchain* pChain);
	XXAPI void xrtNetChainClear(xnetchain* pChain);
	XXAPI bool xrtNetChainAppendCopy(xnetchain* pChain, const void* pData, size_t iLen);
	XXAPI bool xrtNetChainAppendRef(xnetchain* pChain, const xnetbufref* pRef);
	XXAPI size_t xrtNetChainBytes(const xnetchain* pChain);
	XXAPI uint32 xrtNetChainSpanCount(const xnetchain* pChain);
	XXAPI uint32 xrtNetChainGetSpans(const xnetchain* pChain, xnetspan* pOut, uint32 iMaxCount);
	XXAPI size_t xrtNetChainPeek(const xnetchain* pChain, ptr pOut, size_t iLen);
	XXAPI size_t xrtNetChainFindByte(const xnetchain* pChain, uint8 ch, size_t iStartOff);
	XXAPI void xrtNetChainConsume(xnetchain* pChain, size_t iLen);

	#ifndef XRT_NO_XURL
	// URL / Query 解析、拷贝、拼装与归一化
	XXAPI xrtstrview xrtStrView(const char* sPtr, size_t iLen);
	XXAPI bool xrtStrViewIsEmpty(xrtstrview tView);
	XXAPI bool xrtStrViewCopyTo(xrtstrview tView, char* sOut, size_t iOutCap);
	XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme);
	XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme);
	XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL);
	XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme);
	XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB);
	XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut);
	XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut);
	XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut);
	XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut);
	XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut);
	XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut);
	XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap);
	XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtUrlResolve(const xrturlview* pBase, const char* sRef, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtQueryNextN(const char* sQuery, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
	XXAPI bool xrtQueryNext(const char* sQuery, size_t* pOffset, xrtquerypair* pOut);
	XXAPI size_t xrtQueryCountN(const char* sQuery, size_t iLen);
	XXAPI size_t xrtQueryCount(const char* sQuery);
	XXAPI bool xrtQueryFindN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, xrtquerypair* pOut);
	XXAPI bool xrtQueryFind(const char* sQuery, const char* sKey, xrtquerypair* pOut);
	XXAPI bool xrtQueryParseToN(const char* sQuery, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtQueryParseTo(const char* sQuery, xrtquerypair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtPercentDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bPlusAsSpace);
	XXAPI bool xrtUrlParseFixedTo(const char* sURL, const char* sSchemeA, const char* sSchemeB, bool* pSchemeB, char* sHost, size_t iHostCap, uint16* pPort, char* sTarget, size_t iTargetCap);
	XXAPI bool xrtUrlParse(const char* sURL, xurl pOut);

	#endif

	#ifndef XRT_NO_HTTP_UTIL
	// HTTP Token、Header、Cookie、参数与 Multipart 工具函数
	XXAPI bool xrtQueryNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
	XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
	XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
	XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
	XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
	XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
	XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut);
	XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits);
	XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpTokenValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpParamValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtQueryValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtCookieValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtFormUrlEncodedValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpHeaderBlockValidate(const char* sBlock, const xrthttputillimits* pLimits);
	XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtSetCookieValidate(const char* sText, const xrthttputillimits* pLimits);
	XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits);
	XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits);
	XXAPI bool xrtHttpIsTokenN(const char* sText, size_t iLen);
	XXAPI bool xrtHttpIsToken(const char* sText);
	XXAPI bool xrtHttpQuotedStringDecodeToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpQuotedStringDecodeTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpQuotedStringBuildToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpQuotedStringBuildTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus);
	XXAPI bool xrtHttpDecodeExtValueTo(const char* sText, size_t iLen, xrtstrview* pCharset, xrtstrview* pLanguage, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpDecodeExtValue(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpBuildExtValueTo(const char* sCharset, const char* sLanguage, const char* sText, size_t iTextLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpBuildExtValue(const char* sCharset, const char* sLanguage, const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderSplitLineN(const char* sLine, size_t iLen, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderSplitLine(const char* sLine, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderBuildLineTo(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderBuildLine(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderBuildCanonicalLineToN(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderBuildCanonicalLineTo(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderBuildBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderBuildCanonicalBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
	XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut);
	XXAPI size_t xrtHttpTokenCountN(const char* sText, size_t iLen);
	XXAPI size_t xrtHttpTokenCount(const char* sText);
	XXAPI bool xrtHttpTokenFindN(const char* sText, size_t iLen, const char* sToken, size_t iTokenLen, xrtstrview* pOut);
	XXAPI bool xrtHttpTokenFind(const char* sText, const char* sToken, xrtstrview* pOut);
	XXAPI bool xrtHttpTokenParseToN(const char* sText, size_t iLen, xrtstrview* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtHttpTokenParseTo(const char* sText, xrtstrview* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtHttpTokenAppendTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken, size_t iTokenLen);
	XXAPI bool xrtHttpTokenAppend(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken);
	XXAPI bool xrtHttpHeaderContainsTokenN(const char* sValue, size_t iValueLen, const char* sToken);
	XXAPI bool xrtHttpHeaderContainsToken(const char* sValue, const char* sToken);
	XXAPI bool xrtHttpHeaderFindN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut);
	XXAPI bool xrtHttpHeaderFind(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut);
	XXAPI size_t xrtHttpHeaderCountN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen);
	XXAPI size_t xrtHttpHeaderCount(const xrtheaderpair* pHeaders, size_t iCount, const char* sName);
	XXAPI bool xrtHttpHeaderFindNthN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pOut);
	XXAPI bool xrtHttpHeaderFindNth(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNth, xrtstrview* pOut);
	XXAPI size_t xrtHttpHeaderFindAllToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, xrtstrview* pOut, size_t iOutCap);
	XXAPI size_t xrtHttpHeaderFindAllTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut, size_t iOutCap);
	XXAPI bool xrtHttpHeaderCanonicalizeNameToN(const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderCanonicalizeNameTo(const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderJoinValuesTo(const xrtstrview* pValues, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderCollectAndJoinToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderCollectAndJoinTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderFindLineN(const char* sBlock, size_t iLen, const char* sName, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderFindLine(const char* sBlock, const char* sName, xrtheaderpair* pOut);
	XXAPI bool xrtHttpHeaderParseBlockToN(const char* sBlock, size_t iLen, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount);
	XXAPI bool xrtHttpHeaderParseBlockTo(const char* sBlock, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount);
	XXAPI bool xrtHttpHeaderAppendPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
	XXAPI bool xrtHttpHeaderAppendPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue);
	XXAPI bool xrtHttpHeaderSetPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
	XXAPI bool xrtHttpHeaderSetPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue);
	XXAPI size_t xrtHttpHeaderRemoveN(xrtheaderpair* pHeaders, size_t* pCount, const char* sName, size_t iNameLen);
	XXAPI size_t xrtHttpHeaderRemove(xrtheaderpair* pHeaders, size_t* pCount, const char* sName);
	XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
	XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut);
	XXAPI bool xrtCookieFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrtcookiepair* pOut);
	XXAPI bool xrtCookieFind(const char* sText, const char* sName, xrtcookiepair* pOut);
	XXAPI bool xrtCookieParseToN(const char* sText, size_t iLen, xrtcookiepair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtCookieParseTo(const char* sText, xrtcookiepair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
	XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut);
	XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut);
	XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut);
	XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
	XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut);
	XXAPI size_t xrtHttpParamCountN(const char* sText, size_t iLen);
	XXAPI size_t xrtHttpParamCount(const char* sText);
	XXAPI bool xrtHttpParamFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrthttpparam* pOut);
	XXAPI bool xrtHttpParamFind(const char* sText, const char* sName, xrthttpparam* pOut);
	XXAPI bool xrtHttpParamAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bQuoteValue);
	XXAPI bool xrtHttpParamAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue, bool bHasValue, bool bQuoteValue);
	XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut);
	XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut);
	XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut);
	XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut);
	XXAPI bool xrtHttpContentDispositionParseN(const char* sText, size_t iLen, xrtcontentdispositionview* pOut);
	XXAPI bool xrtHttpContentDispositionParse(const char* sText, xrtcontentdispositionview* pOut);
	XXAPI bool xrtHttpContentDispositionDecodeFileNameTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpContentDispositionDecodeFileName(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpContentDispositionBuildTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtHttpContentDispositionBuild(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtCookieAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
	XXAPI bool xrtCookieAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue);
	XXAPI bool xrtCookieBuildTo(const xrtcookiepair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtQueryAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, size_t iKeyLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bPlusAsSpace);
	XXAPI bool xrtQueryAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, const char* sValue);
	XXAPI bool xrtQueryBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtFormUrlEncodedNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
	XXAPI bool xrtFormUrlEncodedNext(const char* sText, size_t* pOffset, xrtquerypair* pOut);
	XXAPI bool xrtFormUrlEncodedParseToN(const char* sText, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtFormUrlEncodedParseTo(const char* sText, xrtquerypair* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue);
	XXAPI bool xrtFormUrlEncodedAppendField(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue);
	XXAPI bool xrtFormUrlEncodedBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtSetCookieBuildTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtSetCookieBuildLineTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtSetCookieBuildLine(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtMultipartBoundaryFromContentTypeN(const char* sValue, size_t iLen, xrtstrview* pOut);
	XXAPI bool xrtMultipartBoundaryFromContentType(const char* sValue, xrtstrview* pOut);
	XXAPI bool xrtMultipartBuildContentTypeTo(const char* sBoundary, size_t iBoundaryLen, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtMultipartBuildContentType(const char* sBoundary, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut);
	XXAPI bool xrtMultipartNext(const char* sBody, const char* sBoundary, size_t* pOffset, xrtmultipartpartview* pOut);
	XXAPI bool xrtMultipartParseToN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtMultipartParseTo(const char* sBody, const char* sBoundary, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount);
	XXAPI bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI bool xrtMultipartDecodeFileName(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen);
	XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig);
	XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig);
	XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream);
	XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream);
	XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen);
	XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream);
	XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream);
	XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent);
	XXAPI bool xrtMultipartAppendFieldPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
	XXAPI bool xrtMultipartAppendFieldPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sValue);
	XXAPI bool xrtMultipartAppendRawPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendRawPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendFilePartExtTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sFileNameExt, size_t iFileNameExtLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendFilePartExt(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sFileNameExt, const char* sContentType, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendFilePartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendFilePart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sContentType, const char* pBody, size_t iBodyLen);
	XXAPI bool xrtMultipartAppendFinishTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen);
	XXAPI bool xrtMultipartAppendFinish(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary);

	#endif

	#ifndef XRT_NO_XCODEC
	// 通用编解码器与 HTTP/1 / WebSocket 解析接口
	XXAPI void xrtCodecParserInit(xcodecparser* pParser, const xcodecparserops* pOps, ptr pCtx);
	XXAPI xcodecstatus xrtCodecParserParse(const xcodecparser* pParser, const xnetchain* pInput, xcodecframe* pFrame);
	XXAPI void xrtCodecParserReset(const xcodecparser* pParser);
	XXAPI void xrtCodecFrameInit(xcodecframe* pFrame);
	XXAPI size_t xrtCodecFramePeek(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen);
	XXAPI void xrtCodecFrameConsume(xnetchain* pInput, const xcodecframe* pFrame);
	XXAPI void xrtCodecLineConfigInit(xcodeclinecodec* pCodec);
	XXAPI bool xrtCodecLineSetDelimiter(xcodeclinecodec* pCodec, const void* pDelimiter, uint32 iDelimiterLen);
	XXAPI xcodecstatus xrtCodecLineParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
	XXAPI void xrtCodecLineReset(ptr pCtx);
	XXAPI const xcodecparserops* xrtCodecLineOps(void);
	XXAPI void xrtCodecLengthConfigInit(xcodeclengthcodec* pCodec);
	XXAPI xcodecstatus xrtCodecLengthParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
	XXAPI void xrtCodecLengthReset(ptr pCtx);
	XXAPI const xcodecparserops* xrtCodecLengthOps(void);

	XXAPI const char* xrtCodecHttp1GetHeader(const xcodechttp1msg* pMsg, const char* sName);
	XXAPI void xrtCodecHttp1MessageInit(xcodechttp1msg* pMsg);
	XXAPI size_t xrtCodecHttp1BodyBytes(const xcodecframe* pFrame);
	XXAPI size_t xrtCodecHttp1CopyBody(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen);
	XXAPI xcodecstatus xrtCodecHttp1Parse(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg);

	XXAPI void xrtCodecWsFrameInit(xcodecwsframeinfo* pInfo);
	XXAPI xcodecstatus xrtCodecWsParseFrame(const xnetchain* pInput, xcodecframe* pFrame, xcodecwsframeinfo* pInfo);
	XXAPI void xrtCodecWsUnmask(ptr pData, size_t iLen, const uint8 aMask[4], size_t iStartOffset);

	#endif

	// 网络引擎生命周期与任务投递
	XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);
	XXAPI void xrtNetEngineDestroy(xnetengine* pEngine);
	XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine);
	XXAPI void xrtNetEngineStop(xnetengine* pEngine);
	XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine);
	XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg);
	XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);

	// TLS 会话驱动与恢复
	XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
	XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
	XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
	XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession);
	XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen);
	XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession);
	XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession);
	XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
	XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen);
	XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten);
	XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
	XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession);
	XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession);
	XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume);
	XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession);
	XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession);
	XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile);
	XXAPI void xrtNetTlsSessionSetAllowTLS12Ed25519(xtlssession* pSession, bool bAllow);

	// TCP 流与监听器操作
	XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
	XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
	XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg,
	const xnetlistenerevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pUserData);
	XXAPI void xrtNetListenerDestroy(xnetlistener* pListener);
	XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener);
	XXAPI void xrtNetListenerStop(xnetlistener* pListener);
	XXAPI xnetstream* xrtNetStreamCreate(xnetengine* pEngine, const xnetstreamevents* pEvents, ptr pUserData);
	XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
	XXAPI xnet_result xrtNetStreamConnect(xnetstream* pStream, const xnetconnectconfig* pCfg);
	XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
	XXAPI xnet_result xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iLen);
	XXAPI xnet_result xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount);
	XXAPI xnet_result xrtNetStreamSendRef(xnetstream* pStream, const xnetbufref* pRef);
	XXAPI void xrtNetStreamPauseRead(xnetstream* pStream);
	XXAPI void xrtNetStreamResumeRead(xnetstream* pStream);
	XXAPI size_t xrtNetStreamPendingSend(const xnetstream* pStream);
	XXAPI const xnetaddr* xrtNetStreamLocalAddr(const xnetstream* pStream);
	XXAPI const xnetaddr* xrtNetStreamRemoteAddr(const xnetstream* pStream);
	XXAPI void xrtNetStreamSetUserData(xnetstream* pStream, ptr pData);
	XXAPI ptr xrtNetStreamGetUserData(xnetstream* pStream);

	// UDP 数据报对象与套接字操作
	XXAPI xnetdgrampkt* xrtNetDgramPacketCreate(const xnetaddr* pFrom, const void* pData, size_t iLen);
	XXAPI void xrtNetDgramPacketDestroy(xnetdgrampkt* pPacket);
	XXAPI const xnetaddr* xrtNetDgramPacketFrom(const xnetdgrampkt* pPacket);
	XXAPI size_t xrtNetDgramPacketBytes(const xnetdgrampkt* pPacket);
	XXAPI size_t xrtNetDgramPacketPeek(const xnetdgrampkt* pPacket, ptr pOut, size_t iLen);
	XXAPI xdgramsock* xrtNetDgramCreate(xnetengine* pEngine, const xnetdgramconfig* pCfg, const xnetdgramevents* pEvents, ptr pUserData);
	XXAPI void xrtNetDgramDestroy(xdgramsock* pSock);
	XXAPI xnet_result xrtNetDgramStart(xdgramsock* pSock);
	XXAPI void xrtNetDgramStop(xdgramsock* pSock);
	XXAPI xnet_result xrtNetDgramSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen);
	XXAPI xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount);

	// Future / Promise / Task 基础接口
	XXAPI xnetfuture* xrtNetFutureCreate(void);
	XXAPI xfuture* xFutureCreate(void);
	XXAPI xfuture* xFutureAddRef(xfuture* pFuture);
	XXAPI void xFutureRelease(xfuture* pFuture);
	XXAPI xfuture_state xFutureState(xfuture* pFuture);
	XXAPI int32 xFutureStatus(xfuture* pFuture);
	XXAPI ptr xFutureValue(xfuture* pFuture);
	XXAPI str xFutureError(xfuture* pFuture);
	XXAPI bool xFutureGetResult(xfuture* pFuture, xfuture_result* pOut);
	XXAPI bool xFutureSetDebugName(xfuture* pFuture, str sDebugName);
	XXAPI str xFutureGetDebugName(xfuture* pFuture);
	XXAPI uint64 xFutureGetCreateTimeMs(xfuture* pFuture);
	XXAPI uint64 xFutureGetCompleteTimeMs(xfuture* pFuture);
	XXAPI int xFutureGetPendingContinuationCount(xfuture* pFuture);
	XXAPI int xFutureGetGroupSourceIndex(xfuture* pFuture);
	XXAPI xfuture* xFutureGetGroupSource(xfuture* pFuture);
	XXAPI xfuture* xFuturePeekGroupSource(xfuture* pFuture);
	XXAPI const xfuture_all_value* xFuturePeekAllValue(xfuture* pFuture);
	XXAPI int xFutureGetAllValueCount(xfuture* pFuture);
	XXAPI ptr xFutureGetAllValueItem(xfuture* pFuture, int iIndex);
	XXAPI bool xFutureWait(xfuture* pFuture);
	XXAPI bool xFutureWaitTimeout(xfuture* pFuture, int64 iTimeoutMs);
	XXAPI bool xFutureWaitUntil(xfuture* pFuture, int64 iDeadlineMs);
	XXAPI bool xFutureWaitCo(xfuture* pFuture);
	XXAPI bool xFutureWaitCoTimeout(xfuture* pFuture, int64 iTimeoutMs);
	XXAPI bool xFutureWaitCoUntil(xfuture* pFuture, int64 iDeadlineMs);
	XXAPI ptr xFutureWaitValue(xfuture* pFuture);
	XXAPI ptr xFutureWaitValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
	XXAPI ptr xFutureWaitValueUntil(xfuture* pFuture, int64 iDeadlineMs);
	XXAPI ptr xFutureWaitCoValue(xfuture* pFuture);
	XXAPI ptr xFutureWaitCoValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
	XXAPI ptr xFutureWaitCoValueUntil(xfuture* pFuture, int64 iDeadlineMs);
	XXAPI bool xFutureRequestCancel(xfuture* pFuture);
	XXAPI xpromise* xPromiseCreate(xfuture* pFuture);
	XXAPI void xPromiseDestroy(xpromise* pPromise);
	XXAPI xfuture* xPromiseGetFuture(xpromise* pPromise);
	XXAPI xfuture* xPromisePeekFuture(xpromise* pPromise);
	XXAPI bool xPromiseResolve(xpromise* pPromise, ptr pValue);
	XXAPI bool xPromiseReject(xpromise* pPromise, int32 iStatus, str sError);
	XXAPI bool xPromiseCancel(xpromise* pPromise, str sError);
	XXAPI bool xPromiseClose(xpromise* pPromise, str sError);
	XXAPI xfuture* xTaskRunEngine(xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
	XXAPI xfuture* xTaskRunDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg);
	XXAPI xfuture* xTaskRunThread(xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
	#if !defined(XRT_NO_COROUTINE)
	XXAPI xfuture* xTaskRunCo(xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
	#endif
	XXAPI xfuture* xFutureThenInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureCatchInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureFinallyInline(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureThenCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureCatchCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureFinallyCurrent(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureThenEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureCatchEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg);
	XXAPI xfuture* xFutureFinallyEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_finally_fn pfnCont, ptr pArg);
	#if !defined(XRT_NO_COROUTINE)
	XXAPI xfuture* xFutureThenCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize);
	XXAPI xfuture* xFutureCatchCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize);
	XXAPI xfuture* xFutureFinallyCo(xfuture* pFuture, xcosched* pSched, xfuture_finally_fn pfnCont, ptr pArg, size_t iStackSize);
	#endif
	XXAPI xfuture* xFutureWhenAny(xfuture** arrFuture, int iCount);
	XXAPI xfuture* xFutureWhenAll(xfuture** arrFuture, int iCount);
	XXAPI xfuture* xFutureRace(xfuture** arrFuture, int iCount);
	XXAPI int xFuturePumpCurrentContinuations(int iMaxCount);
	XXAPI xtaskgroup* xTaskGroupCreate(void);
	XXAPI void xTaskGroupDestroy(xtaskgroup* pGroup);
	XXAPI void xTaskGroupClose(xtaskgroup* pGroup);
	XXAPI xtaskgroup* xTaskGroupCreateChild(xtaskgroup* pParent);
	XXAPI bool xTaskGroupBindParent(xtaskgroup* pGroup, xfuture* pParent);
	XXAPI bool xTaskGroupAddFuture(xtaskgroup* pGroup, xfuture* pFuture);
	XXAPI int xTaskGroupCount(xtaskgroup* pGroup);
	XXAPI int xTaskGroupReapCompleted(xtaskgroup* pGroup);
	XXAPI xfuture* xTaskGroupJoinFuture(xtaskgroup* pGroup);
	XXAPI bool xTaskGroupJoin(xtaskgroup* pGroup);
	XXAPI bool xTaskGroupJoinTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);
	XXAPI bool xTaskGroupJoinUntil(xtaskgroup* pGroup, int64 iDeadlineMs);
	XXAPI bool xTaskGroupWait(xtaskgroup* pGroup);
	XXAPI bool xTaskGroupWaitTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);
	XXAPI bool xTaskGroupWaitUntil(xtaskgroup* pGroup, int64 iDeadlineMs);
	XXAPI void xTaskGroupCancel(xtaskgroup* pGroup);
	XXAPI xfuture* xTaskGroupRunEngine(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
	XXAPI xfuture* xTaskGroupRunDelayed(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg);
	XXAPI xfuture* xTaskGroupRunThread(xtaskgroup* pGroup, xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
	#if !defined(XRT_NO_COROUTINE)
	XXAPI xfuture* xTaskGroupRunCo(xtaskgroup* pGroup, xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
	#endif

	// 统一等待源封装
	XXAPI xnetwaitsrc xrtNetWaitSourceNone(void);
	XXAPI xwaitsrc xWaitSourceNone(void);
	XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture);
	XXAPI xwaitsrc xWaitSourceFromFuture(xfuture* pFuture);
	XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xwaitsrc xWaitSourceFromStream(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock);
	XXAPI xwaitsrc xWaitSourceFromDgramRecv(xdgramsock* pSock);
	XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener);
	XXAPI xwaitsrc xWaitSourceFromListenerAccept(xnetlistener* pListener);
	XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture);
	XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetFutureStatus(xnetfuture* pFuture);
	XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture);
	XXAPI xnet_result xrtNetFutureWaitUntil(xnetfuture* pFuture, int64_t iDeadlineMs);
	XXAPI xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture);
	XXAPI xnetengine* xrtNetSyncGetHiddenEngine(void);
	XXAPI void xrtNetSyncShutdownHiddenEngine(void);
	XXAPI xnetfuture* xrtNetEnginePostFuture(xnetengine* pEngine, uint32 iAffinityKey, xnet_future_task_fn pfnTask, ptr pArg);
	XXAPI xnetfuture* xrtNetEnginePostDelayedFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_future_task_fn pfnTask, ptr pArg);
	XXAPI xnetfuture* xrtNetStreamDrainFuture(xnetstream* pStream);
	XXAPI xnetfuture* xrtNetStreamWritableFuture(xnetstream* pStream);
	XXAPI xnetfuture* xrtNetStreamCloseFuture(xnetstream* pStream);
	XXAPI xnetfuture* xrtNetStreamReadableFuture(xnetstream* pStream);
	XXAPI xnetfuture* xrtNetStreamFutureEx(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xnetfuture* xrtNetListenerAcceptFuture(xnetlistener* pListener);
	XXAPI xnetfuture* xrtNetDgramRecvFuture(xdgramsock* pSock);

	// 同步等待接口
	XXAPI xnet_result xrtNetStreamWaitEx(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xnet_result xrtNetStreamWaitTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitUntilEx(xnetstream* pStream, uint32 iWaitKind, int64_t iDeadlineMs);
	XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc);
	XXAPI bool xWaitSourceWait(const xwaitsrc* pSrc);
	XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
	XXAPI bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	XXAPI xnet_result xrtNetWaitSourceWaitUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs);
	XXAPI bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetWaitSourceWaitValue(const xnetwaitsrc* pSrc, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitValue(const xwaitsrc* pSrc);
	XXAPI xnet_result xrtNetWaitSourceWaitValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	XXAPI xnet_result xrtNetWaitSourceWaitValueUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetListenerAccept(xnetlistener* pListener, xnetstream** ppStream);
	XXAPI xnet_result xrtNetListenerAcceptTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream);
	XXAPI xnet_result xrtNetListenerAcceptUntil(xnetlistener* pListener, int64_t iDeadlineMs, xnetstream** ppStream);
	XXAPI xnet_result xrtNetDgramRecv(xdgramsock* pSock, xnetdgrampkt** ppPacket);
	XXAPI xnet_result xrtNetDgramRecvTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket);
	XXAPI xnet_result xrtNetDgramRecvUntil(xdgramsock* pSock, int64_t iDeadlineMs, xnetdgrampkt** ppPacket);

	// 协程等待接口
	XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream);
	XXAPI xnet_result xrtNetStreamWaitDrainCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitDrainCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream);
	XXAPI xnet_result xrtNetStreamWaitWritableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitWritableCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream);
	XXAPI xnet_result xrtNetStreamWaitCloseCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitCloseCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream);
	XXAPI xnet_result xrtNetStreamWaitReadableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitReadableCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
	XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
	XXAPI bool xWaitSourceWaitCo(const xwaitsrc* pSrc);
	XXAPI xnet_result xrtNetWaitSourceWaitCoTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
	XXAPI bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	XXAPI xnet_result xrtNetWaitSourceWaitCoUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs);
	XXAPI bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitCoValue(const xwaitsrc* pSrc);
	XXAPI xnet_result xrtNetWaitSourceWaitCoValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitCoValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	XXAPI xnet_result xrtNetWaitSourceWaitCoValueUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs, ptr* ppValue);
	XXAPI ptr xWaitSourceWaitCoValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	XXAPI xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream);
	XXAPI xnet_result xrtNetListenerAcceptCoTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream);
	XXAPI xnet_result xrtNetListenerAcceptCoUntil(xnetlistener* pListener, int64 iDeadlineMs, xnetstream** ppStream);
	XXAPI xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket);
	XXAPI xnet_result xrtNetDgramRecvCoTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket);
	XXAPI xnet_result xrtNetDgramRecvCoUntil(xdgramsock* pSock, int64 iDeadlineMs, xnetdgrampkt** ppPacket);

	#ifndef XRT_NO_XHTTP
	// XNet 内建 HTTP 客户端
	XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine);
	XXAPI void xrtHttpRequestInit(xhttprequest* pReq);
	XXAPI void xrtHttpRequestUnit(xhttprequest* pReq);
	XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);
	XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);
	XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);
	XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);
	XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
	XXAPI void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
	XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);
	XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp);
	XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);
	XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq);
	XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus);

	#endif

	#ifndef XRT_NO_XHTTPD
	// XNet 内建 HTTP 服务端
	XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
	XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
	XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
	XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
	XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
	XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
	XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
	XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
	XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
	XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
	XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
	XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
	XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
	XXAPI void xrtHttpdStop(xhttpdserver* pServer);
	XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);

	#endif

	#ifndef XRT_NO_XWS
	// XNet 内建 WebSocket 客户端与服务端
	XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg);
	XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg);
	XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData);
	XXAPI xnet_result xrtWsClientStart(xwsclient* pClient);
	XXAPI void xrtWsClientStop(xwsclient* pClient);
	XXAPI void xrtWsClientDestroy(xwsclient* pClient);
	XXAPI bool xrtWsClientIsOpen(const xwsclient* pClient);
	XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen);
	XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen);
	XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen);
	XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason);
	XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData);
	XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer);
	XXAPI xnet_result xrtWsServerStart(xwsserver* pServer);
	XXAPI void xrtWsServerStop(xwsserver* pServer);
	XXAPI void xrtWsServerDestroy(xwsserver* pServer);
	XXAPI bool xrtWsConnIsOpen(const xwsconn* pConn);
	XXAPI xnet_result xrtWsConnSendText(xwsconn* pConn, const char* sText, size_t iLen);
	XXAPI xnet_result xrtWsConnSendBinary(xwsconn* pConn, const void* pData, size_t iLen);
	XXAPI xnet_result xrtWsConnPing(xwsconn* pConn, const void* pData, size_t iLen);
	XXAPI xnet_result xrtWsConnClose(xwsconn* pConn, uint16 iCode, const char* sReason);

	#endif

	#endif /* !XRT_NO_NETWORK && !XRT_BUILD_CORE */
	/* ------------------------------------ TLS ------------------------------------ */
	/*
		依赖项：
			Network / TLS 共享状态定义
			Crypto 加密算法库
			XNet V2
	*/

#ifndef XRT_NO_NETTLS
#endif

	/* ------------------------------------ XID 函数库 ------------------------------------ */
	/*
		依赖项：
			Time 函数库
			Network 函数库
	*/
	
	// XID 数据结构 ( 192 bit )
	typedef struct {
		xtime Time;				// 当前时间戳
		int32 Addr;				// 本机 IP 地址
		int32 Tick;				// CPU 时钟 ( 低 32 位 )
		int64 Rand;				// 随机数
	} xid_struct, *xid;
	
	// XID 转 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtEncodeXID(xid pXID);
	
	// 字符串 转 XID ( 需要使用 xrtFree 释放内存 )
	XXAPI xid xrtDecodeXID(str sXID);
	
	// 获取 XID ( 需要使用 xrtFree 释放内存 )
	XXAPI xid xrtMakeXID();
	
	// 获取 XID 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtMakeXIDS();
	
	// 比较两个 XID 是否相同
	XXAPI bool xrtCompXID(xid pXID1, xid pXID2);
	
	
	
	/* ------------------------------------ Buffer 函数库 ------------------------------------ */
	
	// 内容类型
	#define XBUF_BINARY 0						// 二进制
	#define XBUF_ANSI 1							// ANSI 字符串
	#define XBUF_UTF8 1							// UTF8 字符串
	#define XBUF_UTF16 2						// UTF16 字符串
	#define XBUF_UTF32 4						// UTF32 字符串
	
	// 默认增量长度
	#define XBUFFER_ALLOC_STEP 0x10000
	
	// 内存缓冲区管理单元数据结构
	typedef struct {
		char* Buffer;							// 内存缓冲区
		uint32 Length;							// 内存长度
		uint32 AllocLength;						// 已申请内存长度
		uint32 AllocStep;						// 预分配内存步长
	} xbuffer_struct, *xbuffer;
	
	// 创建内存缓冲区管理器
	XXAPI xbuffer xrtBufferCreate(uint32 iStep);
	
	// 销毁内存缓冲区管理器
	XXAPI void xrtBufferDestroy(xbuffer pBuf);
	
	// 初始化缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep);
	
	// 释放缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferUnit(xbuffer pBuf);
	
	// 分配内存
	XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount);
	
	// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
	XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode);
	
	// 末尾添加数据
	XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode);
	
	// 清空管理单元
	#define xrtBufferClear xrtBufferUnit
	
	
	
	/* ------------------------------------ Point Array 函数库 ------------------------------------ */
	
	/*
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 默认步长
	#define XPARRAY_PREASSIGNSTEP	256
	
	// 指针数组内存管理器数据结构
	typedef struct {
		ptr* Memory;							// 管理器内存指针
		uint32 Count;							// 管理器中存在多少成员
		uint32 AllocCount;						// 已经申请的结构数量
		uint32 AllocStep;						// 预分配内存步长
		xrtOwnerInfo Owner;						// 所有权信息
	} xparray_struct, *xparray;
	
	// 创建指针内存管理器
	XXAPI xparray xrtPtrArrayCreate(uint32 iMode);
	
	// 销毁指针内存管理器
	XXAPI void xrtPtrArrayDestroy(xparray pObject);
	
	// 初始化内存管理器（对自维护结构体指针使用）
	XXAPI void xrtPtrArrayInit(xparray pObject, uint32 iMode);
	
	// 释放内存管理器（对自维护结构体指针使用）
	XXAPI void xrtPtrArrayUnit(xparray pObject);

	// 显式锁定管理器（shared 模式下可用于稳定访问内部指针/遍历）
	XXAPI bool xrtPtrArrayLock(xparray pObject);
	XXAPI void xrtPtrArrayUnlock(xparray pObject);
	
	// 分配内存
	XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount);
	
	// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
	XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal);
	
	// 末尾添加成员
	XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal);
	
	// 添加成员，自动查找空隙（替换为 NULL 的值）
	XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal);
	
	// 交换成员
	XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB);
	
	// 删除成员
	XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount);
	
	// 删除所有成员
	#define xrtPtrArrayRemoveAll xrtPtrArrayUnit
	
	// 清空管理器
	#define xrtPtrArrayClear xrtPtrArrayUnit
	
	// 获取成员指针
	XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos);
	XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos);
	static inline ptr xrtPtrArrayGet_Inline(xparray pObject, uint32 iPos)
	{
		return pObject->Memory[iPos - 1];
	}
	
	// 设置成员指针
	XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
	XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal);
	static inline void xrtPtrArraySet_Inline(xparray pObject, uint32 iPos, ptr pVal)
	{
		if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
			return;
		}
		pObject->Memory[iPos - 1] = pVal;
		xrtOwnerEndMutable(&pObject->Owner);
	}
	
	// 成员排序
	XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar);
	
	
	
	/* ------------------------------------ Array 函数库 ------------------------------------ */
	
	/*
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 默认步长
	#define XARRAY_PREASSIGNSTEP	256
	
	// 结构体数组内存管理器数据结构
	typedef struct {
		str Memory;						// 管理器内存指针
		uint32 ItemLength;				// 成员占用内存长度
		uint32 Count;					// 管理器中存在多少成员
		uint32 AllocCount;				// 已经申请的结构数量
		uint32 AllocStep;				// 预分配内存步长
		xrtOwnerInfo Owner;				// 所有权信息
	} xarray_struct, *xarray;
	
	// 创建数组
	XXAPI xarray xrtArrayCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁数组
	XXAPI void xrtArrayDestroy(xarray pArr);
	
	// 初始化数组的数据结构 ( 用于内嵌数组的对象使用 )
	XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength, uint32 iMode);
	
	// 释放数组的数据结构 ( 但不会释放数组结构体本身的内存，用于内嵌数组的对象使用 )
	XXAPI void xrtArrayUnit(xarray pArr);

	// 显式锁定数组（shared 模式下可用于稳定访问内部指针/遍历）
	XXAPI bool xrtArrayLock(xarray pArr);
	XXAPI void xrtArrayUnlock(xarray pArr);
	
	// 分配内存
	XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount);
	
	// 中间插入成员
	XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
	
	// 末尾添加成员
	XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
	
	// 交换成员
	XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB);
	
	// 删除成员
	XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
	
	// 删除所有成员
	#define xrtArrayRemoveAll xrtArrayUnit
	
	// 清空管理器
	#define xrtArrayClear xrtArrayUnit
	
	// 获取成员数据指针
	XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos);
	XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos);
	static inline ptr xrtArrayGet_Inline(xarray pArr, uint32 iPos)
	{
		return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
	}
	
	// 成员排序
	XXAPI bool xrtArraySort(xarray pArr, ptr procCompar);
	
	
	
	/* ------------------------------------ BSMM 函数库 ------------------------------------ */
	/*
		依赖项：
			Point Array 函数库
	*/
	
	/*
		Blocks Struct Memory Management [块结构内存管理器]
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 内存指针单向链表数据结构
	typedef struct MemPtr_LLNode {
		ptr Ptr;
		struct MemPtr_LLNode* Next;
	} MemPtr_LLNode;
	
	// 数据块结构内存管理器数据结构
	typedef struct {
		xrtOwnerInfo Owner;			// 所有权信息
		uint32 ItemLength;			// 成员占用内存长度
		uint32 Count;					// 管理器中存在多少成员
		xparray_struct PageMMU;				// 内存页管理器
		MemPtr_LLNode* LL_Free;				// 已释放的内存块链表
	} xbsmm_struct, *xbsmm;
	
	// 创建数据块结构内存管理器
	XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁数据块结构内存管理器
	XXAPI void xrtBsmmDestroy(xbsmm objBSMM);
	
	// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
	XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength, uint32 iMode);
	
	// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
	XXAPI void xrtBsmmUnit(xbsmm objBSMM);
	
	// 申请结构体内存
	XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM);
	
	// 释放结构体内存
	XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p);
	
	// 获取成员指针（非特殊需求不建议使用）
	static inline ptr xrtBsmmGetPtr_Inline(xbsmm objBSMM, uint32 iIdx)
	{
		uint32 iBlock = iIdx >> 8;
		uint32 iPos = iIdx & 0xFF;
		str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
		if ( pBlock ) {
			return &pBlock[iPos * objBSMM->ItemLength];
		} else {
			return NULL;
		}
	}
	
	
	
	/* ------------------------------------ Memory Unit 函数库 ------------------------------------ */
	
	// 内存固定的前置数据（用于识别内存是哪个管理器分配的）
	typedef struct {
		uint32 ItemFlag;
	} MMU_Value, *MMU_ValuePtr;
	
	// MMU有效ID区间掩码
	#define MMU_FLAG_MASK				0x3FFFFFFF
	
	// 结构体使用状态标记
	#define MMU_FLAG_USE				0x80000000
	
	// GC回收标记位
	#define MMU_FLAG_GC					0x40000000
	
	// 非内存管理器管理的内存
	#define MMU_FLAG_EXT				0xBFFFFFFF
	
	// GC标记
	#define xrtMemUnitGC_Mark(p) (((MMU_ValuePtr)((void*)p - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC)
	
	// 数据管理单元数据结构
	typedef struct {
		xrtOwnerInfo Owner;						// 所有权信息
		uint8 FreeList[256];						// 已释放成员列表
		uint32 ItemLength;							// 成员占用内存长度
		uint16 Count;								// 成员数量
		uint8 FreeCount;							// 已释放成员数量
		uint8 FreeOffset;							// 首个已释放成员在列表中的偏移位置
		uint32 Flag;								// 值的 Flag 前缀（由上级管理器下发，0-255 区间会被 idx 覆盖）
		char Memory[];								// 数据
	} xmemunit_struct, *xmemunit;
	
	// 创建内存管理单元（iItemLength会自动增加4个字节用于确定内存位置和所属的管理器单元编号）
	XXAPI xmemunit xrtMemUnitCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁内存管理单元
	#define xrtMemUnitDestroy xrtFree
	
	// 从内存管理单元中申请一个元素
	static inline ptr xrtMemUnitAlloc_Inline(xmemunit objUnit)
	{
		if ( objUnit == NULL ) {
			return NULL;
		}
		if ( !xrtOwnerBeginMutable(&objUnit->Owner, "memory unit belongs to another thread.") ) {
			return NULL;
		}
		if ( objUnit->Count > 255 ) {
			xrtOwnerEndMutable(&objUnit->Owner);
			return NULL;
		}
		uint8 idx = objUnit->Count;
		// 优先复用已释放的数据
		if ( objUnit->FreeCount > 0 ) {
			idx = objUnit->FreeList[objUnit->FreeOffset];
			objUnit->FreeOffset++;
			objUnit->FreeCount--;
		}
		objUnit->Count++;
		MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
		v->ItemFlag = MMU_FLAG_USE | objUnit->Flag | idx;
		xrtOwnerEndMutable(&objUnit->Owner);
		return (ptr)&v[1];
	}
	XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
	
	// 释放内存管理单元中某个元素（FreeIdx不会清空 ItemFlag，建议由调用方负责清空）
	static inline void xrtMemUnitFreeIdx_Inline(xmemunit objUnit, uint8 idx)
	{
		if ( objUnit == NULL ) {
			return;
		}
		if ( !xrtOwnerBeginMutable(&objUnit->Owner, "memory unit belongs to another thread.") ) {
			return;
		}
		objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
		objUnit->Count--;
		if ( objUnit->Count ) {
			objUnit->FreeCount++;
		} else {
			objUnit->FreeCount = 0;
			objUnit->FreeOffset = 0;
		}
		xrtOwnerEndMutable(&objUnit->Owner);
	}
	XXAPI bool xrtMemUnitFreeIdx(xmemunit objUnit, uint8 idx);
	static inline void xrtMemUnitFree_Inline(xmemunit objUnit, ptr obj)
	{
		if ( objUnit == NULL || obj == NULL ) {
			return;
		}
		if ( !xrtOwnerBeginMutable(&objUnit->Owner, "memory unit belongs to another thread.") ) {
			return;
		}
		MMU_ValuePtr v = obj - 4;
		unsigned char idx = v->ItemFlag & 0xFF;
		v->ItemFlag = 0;
		objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
		objUnit->Count--;
		if ( objUnit->Count ) {
			objUnit->FreeCount++;
		} else {
			objUnit->FreeCount = 0;
			objUnit->FreeOffset = 0;
		}
		xrtOwnerEndMutable(&objUnit->Owner);
	}
	XXAPI bool xrtMemUnitFree(xmemunit objUnit, ptr obj);
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI int xrtMemUnitGC(xmemunit objUnit, bool bFreeMark);
	
	
	
	/* ------------------------------------ Fixed-Size Memory Pool 函数库 ------------------------------------ */
	/*
		依赖项：
			BSMM 函数库
			Memory Unit 函数库
	*/
	
	// 内存管理器链表结构
	typedef struct MMU_LLNode {
		uint32 Flag;
		xmemunit objMMU;
		struct MMU_LLNode* Prev;
		struct MMU_LLNode* Next;
	} MMU_LLNode;
	
	// 256步进内存管理器数据结构
	typedef struct {
		xrtOwnerInfo Owner;					// 所有权信息
		uint32 ItemLength;					// 成员占用内存长度
		xbsmm_struct arrMMU;						// MMU 阵列
		MMU_LLNode* LL_Idle;						// 有空闲的内存管理单元链表首元素 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的内存管理单元链表首元素 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 缓存的全空内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的内存管理单元 Flag 链表首元素 (申请新单元优先从这里找)
	} xfsmempool_struct, *xfsmempool;
	
	// 创建内存管理器
	XXAPI xfsmempool xrtFSMemPoolCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁内存管理器
	XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM);
	
	// 初始化内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolInit(xfsmempool objMM, uint32 iItemLength, uint32 iMode);
	
	// 释放内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolUnit(xfsmempool objMM);
	
	// 从内存管理器中申请一块内存
	XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
	
	// 将内存管理器申请的内存释放掉
	XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p);
	
	// 将一块内存标记为使用中
	#define xrtFSMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark);
	
	
	
	/* ------------------------------------ Stack 函数库 ------------------------------------ */
	
	// 结构体静态栈数据结构
	typedef struct {
		union {
			char* Memory;					// 栈数据内存 - 结构体
			ptr* PtrMem;					// 栈数据内存 - 指针
		};
		uint32 ItemLength;			// 栈成员占用内存长度
		uint32 MaxCount;				// 栈最大可以容纳多少成员（栈深度）
		uint32 Count;					// 栈中存在多少成员（栈顶位置）
	} xstack_struct, *xstack;
	
	// 创建结构体静态栈
	XXAPI xstack xrtStackCreate(uint32 iMaxCount, uint32 iItemLength);
	
	// 销毁结构体静态栈
	#define xrtStackDestroy xrtFree
	
	// 压栈
	XXAPI ptr xrtStackPush(xstack objSTK);
	XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData);
	XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtStackPop(xstack objSTK);
	XXAPI ptr xrtStackPopPtr(xstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtStackTop(xstack objSTK);
	XXAPI ptr xrtStackTopPtr(xstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos);
	XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint32 iPos);
	
	
	
	/* ------------------------------------ Dynamic Stack 函数库 ------------------------------------ */
	/*
		依赖项：
			Point Array 函数库
	*/
	
	/*
		结构体动态栈，结构体内存256个递增，栈最大深度不固定
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 结构体动态栈数据结构
	typedef struct {
		uint32 ItemLength;					// 栈成员占用内存长度
		uint32 Count;							// 栈中存在多少成员（栈顶位置）
		xparray_struct MMU;							// MMU 管理器
	} xdynstack_struct, *xdynstack;
	
	// 创建结构体动态栈
	XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength);
	
	// 销毁结构体动态栈
	XXAPI void xrtDynStackDestroy(xdynstack objSTK);
	
	// 初始化结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackInit(xdynstack objSTK, uint32 iItemLength);
	
	// 释放结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackUnit(xdynstack objSTK);
	
	// 压栈
	XXAPI ptr xrtDynStackPush(xdynstack objSTK);
	XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData);
	XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtDynStackPop(xdynstack objSTK);
	XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtDynStackTop(xdynstack objSTK);
	XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos);
	XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, uint32 iPos);
	
	
	
	/* ------------------------------------ AVLTree Base 函数库 ------------------------------------ */
	
	// AVL树最大高度
	#define AVLTree_MAX_HEIGHT  46
	
	// AVL树节点基础定义
	typedef struct xavltnode_struct {
		struct xavltnode_struct* left;
		struct xavltnode_struct* right;
		int height;
	} xavltnode_struct, *xavltnode;
	
	// AVL树迭代器结构（按需分配，无需存储树对象）
	#define XRT_AVLITER_FLAG_HOLD_ROOT_LOCK	0x00000001u
	typedef struct xavltree_iterator_struct {
		xavltnode Path[AVLTree_MAX_HEIGHT];		// 遍历路径栈
		int32 Depth;							// 当前栈深度（-1表示未激活）
		ptr Current;							// 当前节点数据
		uint32 Flags;							// 迭代器附加状态
	} xavltree_iterator_struct, *xavltree_iterator;
	
	// AVL树对象数据结构
	typedef struct {
		xavltnode RootNode;
		uint32 Count;
		xavltree_iterator Iterator;		// 当前激活的迭代器对象
	} xavltbase_struct, *xavltbase;
	
	// 比较回调函数
	typedef int (*AVLTree_CompProc)(ptr pNode, ptr pKey);
	
	// 遍历回调函数
	typedef bool (*AVLTree_EachProc)(ptr pNode, ptr pArg);
	
	// 获取 xavltnode 对象
	#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((ptr)p - sizeof(xavltnode_struct)))
	
	// 获取 xavltnode 对应的数据段
	#define xrtAVLTreeGetNodeData(p) ((ptr)(&p[1]))
	
	// 获取根节点数据段
	#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
	
	// 初始化 AVLTree
	#define xrtAVLTB_Init(o) do { \
		(o)->RootNode = NULL; \
		(o)->Count = 0; \
		(o)->Iterator = NULL; \
	} while(0)
	
	// 释放 AVLTree
	#define xrtAVLTB_Unit xrtAVLTB_Init
	
	// 向 AVLTree 中插入节点
	XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey, xavltnode pNewNode);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, ptr pKey);
	
	// 删除所有成员
	#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
	
	// 清空管理器
	#define xrtAVLTB_Clear xrtAVLTB_Unit
	
	// 遍历 AVLTree 所有节点
	XXAPI bool xrtAVLTB_WalkRecuProc(xavltnode root, AVLTree_EachProc procEach, ptr pArg);
	XXAPI bool xrtAVLTB_WalkExRecuProc(xavltnode root, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg);
	#define xrtAVLTB_Walk(obj, p, a) xrtAVLTB_WalkRecuProc(obj->RootNode, (ptr)p, (ptr)a)
	#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) xrtAVLTB_WalkExRecuProc(obj->RootNode, (ptr)p1, (ptr)p2, (ptr)p3, (ptr)a)
	
	// 启动迭代器（按需创建迭代器对象）
	XXAPI void xrtAVLTB_IterBegin(xavltbase objAVLT);
	
	// 获取下一个节点，返回 NULL 表示迭代结束
	XXAPI ptr xrtAVLTB_IterNext(xavltbase objAVLT);
	
	// 手动结束迭代器（提前释放迭代器对象）
	XXAPI void xrtAVLTB_IterEnd(xavltbase objAVLT);
	
	// 基础遍历宏
	#define AVLTBASE_FOREACH(tree, var) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		for ( ptr var = xrtAVLTB_IterNext((xavltbase)tree); var != NULL; var = xrtAVLTB_IterNext((xavltbase)tree) )
	
	// 带类型转换的遍历宏
	#define AVLTBASE_FOREACH_TYPE(tree, var, type) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		for ( type var = xrtAVLTB_IterNext((xavltbase)tree); var != NULL; var = xrtAVLTB_IterNext((xavltbase)tree) )
	
	// 跳出迭代器
	#define AVLTBASE_BREAK(tree) xrtAVLTB_IterEnd((xavltbase)tree); break;
	
	
	
	/* ------------------------------------ AVLTree 函数库 ------------------------------------ */
	/*
		依赖项：
			AVLTree Base 函数库
			Fixed-Size Memory Pool 函数库
	*/
	
	// 键释放回调函数 ( 如果 key 中有额外需要释放的值时使用 )
	typedef void (*AVLTree_FreeProc)(ptr objTree, ptr pNode);
	
	// AVL树对象数据结构
	typedef struct xavltree_struct {
		xavltnode RootNode;
		uint32 Count;
		xavltree_iterator Iterator;		// 当前激活的迭代器对象
		xrtOwnerInfo Owner;			// 所有权信息
		struct xavltree_struct* Parent;
		AVLTree_CompProc CompProc;
		AVLTree_FreeProc FreeProc;
		xfsmempool_struct objMM;
		xavltnode NodeCache;
	} xavltree_struct, *xavltree;
	
	// 创建 AVLTree
	XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp, uint32 iMode);
	
	// 销毁 AVLTree
	XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
	
	// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
	XXAPI void xrtAVLTreeInit(xavltree objAVLT, uint32 iItemLength, AVLTree_CompProc procComp, uint32 iMode);
	
	// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
	XXAPI void xrtAVLTreeUnit(xavltree objAVLT);

	// 显式锁定 AVLTree（shared 模式下可用于稳定访问内部指针/遍历）
	XXAPI bool xrtAVLTreeLock(xavltree objAVLT);
	XXAPI void xrtAVLTreeUnlock(xavltree objAVLT);
	
	// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
	XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey);
	
	// 删除所有成员
	#define xrtAVLTreeRemoveAll xrtAVLTreeUnit
	
	// 清空管理器
	#define xrtAVLTreeClear xrtAVLTreeUnit
	
	// 遍历 AVLTree 所有节点
	XXAPI bool xrtAVLTreeWalk(xavltree objAVLT, AVLTree_EachProc procEach, ptr pArg);
	XXAPI bool xrtAVLTreeWalkEx(xavltree objAVLT, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg);
	
	// 迭代器操作
	XXAPI void xrtAVLTreeIterBegin(xavltree objAVLT);
	XXAPI ptr xrtAVLTreeIterNext(xavltree objAVLT);
	XXAPI void xrtAVLTreeIterEnd(xavltree objAVLT);
	#define AVLTREE_FOREACH(tree, var) \
		xrtAVLTreeIterBegin(tree); \
		for ( ptr var = xrtAVLTreeIterNext(tree); var != NULL; var = xrtAVLTreeIterNext(tree) )
	#define AVLTREE_FOREACH_TYPE(tree, var, type) \
		xrtAVLTreeIterBegin(tree); \
		for ( type var = xrtAVLTreeIterNext(tree); var != NULL; var = xrtAVLTreeIterNext(tree) )
	#define AVLTREE_BREAK(tree) xrtAVLTreeIterEnd(tree); break;
	
	
	
	/* ------------------------------------ Memory Pool 函数库 ------------------------------------ */
	/*
		依赖项：
			BSMM 函数库
			Fixed-Size Memory Pool 函数库
	*/
	
	// MP256 or MP64K 大内存结构体前置结构
	typedef struct {
		uint32 Index;							// BigMM 的块索引
		uint32 Flag;							// 符合 MM256 标准的 Flag
	} MP_MemHead;
	
	// MP256 or MP64K 大内存信息链表结构体（实际返回的内存地址相当于 Ptr + sizeof(MP_MemHead)）
	typedef struct MP_BigInfoLL {
		uint32 Index;							// BigMM 槽位索引
		uint32 Size;							// 申请内存的大小，可有可无（可开发辅助功能，如泄漏检测）
		ptr Ptr;									// 指针地址，使用 mmu_malloc 返回的地址
		struct MP_BigInfoLL* Next;					// 下一个链表节点（用于释放链表）
	} MP_BigInfoLL;
	
	// 单个长度区间的内存管理器结构
	typedef struct FSB_Item {
		uint32 MinLength;						// 支持最小的内存长度
		uint32 MaxLength;						// 支持最大的内存长度
		MMU_LLNode* LL_Idle;						// 空闲的 MMU 内存管理单元链表 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的 MMU 内存管理单元链表 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 全空的 MMU 内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的 MMU 内存管理单元链表 (申请新单元优先从这里找)
		struct FSB_Item* left;						// legacy tree pointer (unused in LUT mode)
		struct FSB_Item* right;						// legacy tree pointer (unused in LUT mode)
	} FSB_Item;
	
	// 通用内存池数据结构（LUT 分桶）
	typedef struct {
		xrtOwnerInfo Owner;							// 所有权信息
		FSB_Item* FSB_Memory;						// fixed-size-blocks bucket array
		FSB_Item* FSB_RootNode;						// legacy compatibility alias, points to the first bucket when active
		uint32* FSB_Lut;							// size -> bucket lookup table (0..iFallbackCutoff)
		uint32 iBucketStep;							// bucket step size in bytes
		uint32 iBucketCount;						// number of buckets in FSB_Memory
		uint32 iFallbackCutoff;						// max size still served by pooled buckets
		xbsmm_struct arrMMU;						// MMU 阵列
		xbsmm_struct BigMM;							// 大内存数组
		MP_BigInfoLL* LL_BigFree;					// 大内存已释放的内存块链表
	} xmempool_struct, *xmempool;
	
	// 创建内存池
	XXAPI xmempool xrtMemPoolCreate(int iCustom, uint32 iMode);
	
	// 销毁内存池
	XXAPI void xrtMemPoolDestroy(xmempool objMP);
	
	// 初始化内存池（对自维护结构体指针使用，和 MP256_Create 功能类似）
	XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom, uint32 iMode);
	
	// 释放内存池（对自维护结构体指针使用，和 MP256_Destroy 功能类似）
	XXAPI void xrtMemPoolUnit(xmempool objMP);
	
	// 从内存池中申请一块内存
	XXAPI ptr xrtMemPoolAlloc(xmempool objMP, uint32 iSize);
	
	// 将内存池申请的内存释放掉
	XXAPI void xrtMemPoolFree(xmempool objMP, ptr ptr);
	
	// 将一块内存标记为使用中
	#define xrtMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark);
	
	
	
	/* ------------------------------------ Dict 函数库 ------------------------------------ */
	/*
		依赖项：
			AVLTree 函数库
			Memory Pool 函数库
	*/
	
	// 字典 Key 数据结构
	typedef struct {
		ptr Key;
		uint32 KeyLen;
		uint32 Hash;
	} Dict_Key;
	
	// 字典对象数据结构
	typedef struct {
		xavltree_struct AVLT;
		xmempool MP;
		xrtOwnerInfo Owner;			// 所有权信息
	} xdict_struct, *xdict;
	
	// 字典遍历回调函数
	typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);
	
	// 创建哈希表
	XXAPI xdict xrtDictCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁哈希表
	XXAPI void xrtDictDestroy(xdict objHT);
	
	// 初始化哈希表（对自维护结构体指针使用，和 AVLHT32_Create 功能类似）
	XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength, uint32 iMode);
	
	// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
	XXAPI void xrtDictUnit(xdict objHT);

	// 显式锁定哈希表（shared 模式下可用于稳定访问内部指针/遍历）
	XXAPI bool xrtDictLock(xdict objHT);
	XXAPI void xrtDictUnlock(xdict objHT);
	
	// 设置值
	static inline ptr xrtDictSetWithKey(xdict objHT, Dict_Key* objKey, bool* bNewRet)
	{
		ptr pRet = NULL;
		if ( !xrtOwnerBeginMutable(&objHT->Owner, (str)"dict belongs to another thread.") ) {
			return NULL;
		}
		bool bNew;
		Dict_Key* pNode = xrtAVLTreeInsert(&objHT->AVLT, objKey, &bNew);
		if ( pNode ) {
			if ( bNewRet ) {
				*bNewRet = bNew;
			}
			if ( bNew ) {
				uint32 iKeyLen = objKey->KeyLen;
				if ( objHT->MP ) {
					pNode->Key = xrtMemPoolAlloc(objHT->MP, iKeyLen + 1);
				} else {
					pNode->Key = xrtMalloc(iKeyLen + 1);
				}
				pNode->KeyLen = iKeyLen;
				pNode->Hash = objKey->Hash;
				memcpy(pNode->Key, objKey->Key, iKeyLen);
				((char*)pNode->Key)[iKeyLen] = 0;
			}
			pRet = &pNode[1];
		}
		xrtOwnerEndMutable(&objHT->Owner);
		return pRet;
	}
	XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet);
	
	// 设置值 - 当值为 ptr 时直接修改指针内容
	XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal);
	
	// 获取值
	static inline ptr xrtDictGetWithKey(xdict objHT, Dict_Key* objKey)
	{
		ptr pRet = NULL;
		if ( !xrtOwnerBeginMutable(&objHT->Owner, (str)"dict belongs to another thread.") ) {
			return NULL;
		}
		Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, objKey);
		if ( pNode ) {
			pRet = &pNode[1];
		}
		xrtOwnerEndMutable(&objHT->Owner);
		return pRet;
	}
	XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 获取值 - 当值为 ptr 时直接获取指针内容
	XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除值
	XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除值，当值为 ptr 时返回 ptr
	XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 判断值是否存在
	XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen);
	
	// 删除所有成员
	#define xrtDictRemoveAll xrtDictUnit
	
	// 清空管理器
	#define xrtDictClear xrtDictUnit
	
	// 获取表内元素数量
	XXAPI uint32 xrtDictCount(xdict objHT);
	
	// 遍历表元素
	XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);
	
	// 迭代器操作
	#define xrtDictIterBegin(obj) xrtAVLTB_IterBegin((xavltbase)obj)
	#define xrtDictIterNext(obj)  xrtAVLTB_IterNext((xavltbase)obj)
	#define xrtDictIterEnd(obj)   xrtAVLTB_IterEnd((xavltbase)obj)
	#define DICT_BREAK AVLTBASE_BREAK
	
	// 迭代器辅助宏，强制展开 __LINE__
	#define __XRT_CONCAT(a, b) a##b
	#define __XRT_CONCATLINE(a, b) __XRT_CONCAT(a, b)
	
	// 基础遍历宏
	#define DICT_FOREACH(tree, key, val) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		bool __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1; \
		for ( Dict_Key* key = xrtAVLTB_IterNext((xavltbase)tree); key != NULL; key = xrtAVLTB_IterNext((xavltbase)tree), __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1 ) \
			for ( ptr val = (ptr)(&key[1]); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 0 )
	
	// 带类型转换的遍历宏
	#define DICT_FOREACH_TYPE(tree, key, val, type) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		bool __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1; \
		for ( Dict_Key* key = xrtAVLTB_IterNext((xavltbase)tree); key != NULL; key = xrtAVLTB_IterNext((xavltbase)tree), __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1 ) \
			for ( type val = (type)(&key[1]); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 0 )
	
	
	
	/* ------------------------------------ List 函数库 ------------------------------------ */
	/*
		依赖项：
			AVLTree 函数库
	*/
	
	// 列表对象数据结构
	typedef struct {
		xavltree_struct AVLT;
		xrtOwnerInfo Owner;			// 所有权信息
	} xlist_struct, *xlist;
	
	// 列表遍历回调函数
	typedef bool (*List_EachProc)(int64 pKey, ptr pVal, ptr pArg);
	
	// 创建列表
	XXAPI xlist xrtListCreate(uint32 iItemLength, uint32 iMode);
	
	// 销毁列表
	XXAPI void xrtListDestroy(xlist objList);
	
	// 初始化列表（对自维护结构体指针使用）
	XXAPI void xrtListInit(xlist objList, uint32 iItemLength, uint32 iMode);
	
	// 释放列表（对自维护结构体指针使用）
	XXAPI void xrtListUnit(xlist objList);

	// 显式锁定列表（shared 模式下可用于稳定访问内部指针/遍历）
	XXAPI bool xrtListLock(xlist objList);
	XXAPI void xrtListUnlock(xlist objList);
	
	// 设置值
	XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet);
	
	// 设置值 - 当值为 ptr 时直接修改指针内容
	XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
	
	// 获取值
	XXAPI ptr xrtListGet(xlist objList, int64 iKey);
	
	// 获取值 - 当值为 ptr 时直接获取指针内容
	XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
	
	// 删除值
	XXAPI bool xrtListRemove(xlist objList, int64 iKey);
	
	// 删除值，当值为 ptr 时返回 ptr
	XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey);
	
	// 判断值是否存在
	XXAPI bool xrtListExists(xlist objList, int64 iKey);
	
	// 删除所有成员
	#define xrtListRemoveAll xrtListUnit
	
	// 清空管理器
	#define xrtListClear xrtListUnit
	
	// 获取表内元素数量
	XXAPI uint32 xrtListCount(xlist objList);
	
	// 遍历表元素
	XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
	
	// 迭代器操作
	#define xrtListIterBegin(obj) xrtAVLTB_IterBegin((xavltbase)obj)
	#define xrtListIterNext(obj)  xrtAVLTB_IterNext((xavltbase)obj)
	#define xrtListIterEnd(obj)   xrtAVLTB_IterEnd((xavltbase)obj)
	#define LIST_BREAK AVLTBASE_BREAK
	
	// 基础遍历宏
	#define LIST_FOREACH(tree, idx, val) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		bool __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1; \
		int64* __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree); \
		for ( int64 idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) != NULL; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree), idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0, __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1 ) \
			for ( ptr val = (ptr)(&__XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[1]); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 0 )
	
	// 带类型转换的遍历宏
	#define LIST_FOREACH_TYPE(tree, idx, val, type) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		bool __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1; \
		int64* __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree); \
		for ( int64 idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) != NULL; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree), idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0, __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1 ) \
			for ( type val = (type)(&__XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[1]); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 0 )

	
	
	/* ------------------------------------ JNUM 函数库 ------------------------------------ */
	
	// 数值类型
	typedef enum {
		JNUM_NULL,
		JNUM_BOOL,
		JNUM_INT,
		JNUM_HEX,
		JNUM_LINT,
		JNUM_LHEX,
		JNUM_DOUBLE,
	} jnum_type_t;
	
	// 数值
	typedef union {
		bool vbool;
		int32_t vint;
		uint32_t vhex;
		int64_t vlint;
		uint64_t vlhex;
		double vdbl;
	} jnum_value_t;
	
	// 数字转字符串
	XXAPI int xrtI32ToStr(int32_t num, char* buffer);
	XXAPI int xrtI64ToStr(int64_t num, char* buffer);
	XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
	XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
	XXAPI int xrtNumToStr(double num, char* buffer);
	
	// 解析数字字符串
	XXAPI int xrtParseNum(const char *str, jnum_type_t *type, jnum_value_t *value);
	
	// 解析字符串（自动跳过前导空白字符）
	static inline int xrtParseNumSkipSpace(const void *str, jnum_type_t *type, jnum_value_t *value)
	{
		const char *pBase = (const char*)str;
		const char *s = pBase;
		while (1) {
			switch (*s) {
			case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': case ' ': ++s; break;
			default: goto next;
			}
		}
	next:
		return (int)(xrtParseNum(s, type, value) + (s - pBase));
	}
	
	// 字符串转数字
	XXAPI int32_t xrtStrToI32(const void* pStr);
	XXAPI int64_t xrtStrToI64(const void* pStr);
	XXAPI uint32_t xrtStrToU32(const void* pStr);
	XXAPI uint64_t xrtStrToU64(const void* pStr);
	XXAPI double xrtStrToNum(const void* pStr);
	
	
	
	/* ------------------------------------ Value 函数库 ------------------------------------ */
	/*
		依赖项：
			Point Array 函数库
			List 函数库
			AVLTree 函数库
			Dict 函数库
			Time 函数库
	*/
	
	// 数据类型 - 主类型
	#define XVO_DT_NULL				0				// null
	#define XVO_DT_BOOL				1				// bool : true | false
	#define XVO_DT_INT				2				// 整数（int64）
	#define XVO_DT_FLOAT			3				// 浮点数（double）
	#define XVO_DT_TEXT				4				// 字符串
	#define XVO_DT_TIME				5				// 时间
	#define XVO_DT_POINT			6				// 指针
	#define XVO_DT_FUNC				7				// 函数
	#define XVO_DT_ARRAY			8				// 数组
	#define XVO_DT_LIST				9				// 列表
	#define XVO_DT_COLL				10				// 集合
	#define XVO_DT_TABLE			11				// 表
	#define XVO_DT_CLASS			12				// 结构体
	#define XVO_DT_CUSTOM			15				// 自定义

	// Value 头部位定义（共享、静态和引用计数都编码在 Header 中）
	#define XVO_HEADER_SHARED_MASK		0x00000010u
	#define XVO_HEADER_STATIC_MASK		0x00000020u
	#define XVO_HEADER_REFCOUNT_SHIFT	6u
	#define XVO_HEADER_REFCOUNT_ONE		(1u << XVO_HEADER_REFCOUNT_SHIFT)
	#define XVO_HEADER_REFCOUNT_MAX		0x03FFFFFFu
	#define XVO_HEADER_REFCOUNT_MASK	(XVO_HEADER_REFCOUNT_MAX << XVO_HEADER_REFCOUNT_SHIFT)
	#define XVO_HEADER_INIT(iType, bStatic, bShared, iRefCount) \
		((((uint32)(iType)) & 0x0Fu) | ((bShared) ? XVO_HEADER_SHARED_MASK : 0u) | ((bStatic) ? XVO_HEADER_STATIC_MASK : 0u) | ((((uint32)(iRefCount)) & XVO_HEADER_REFCOUNT_MAX) << XVO_HEADER_REFCOUNT_SHIFT))
	
	// Value 标准数据类 [ 16 Byte ]
	typedef struct xvalue_struct {
		union {
			struct {
				uint32 Type:4;
				uint32 Reserve:1;
				uint32 IsStatic:1;
				uint32 RefCount:26;
			};
			volatile uint32 Header;
		};
		uint32 Size;
		union {
			bool vBool;
			int64 vInt;
			double vFloat;
			str vText;
			xtime vTime;
			ptr vPoint;
			struct xvalue_struct* (*vFunc)(struct xvalue_struct* varENV, struct xvalue_struct* varParam);
			xparray vArray;
			xlist vList;
			xavltree vColl;
			xdict vTable;
			ptr vStruct;
			ptr vCustom;
		};
	} xvalue_struct, *xvalue;
	
	// 函数指针类型定义
	typedef xvalue (*xfunction)(xvalue pENV, xvalue arrParam);
	
	// Custom 类虚表（用于自定义对象接入 Value 生命周期）
	struct xcustom_struct {
		void (*construct)(xvalue var);
		void (*destruct)(xvalue var);
		int (*set)(xvalue var, str key, xvalue val);
		xvalue (*get)(xvalue var, str key);
		xvalue (*call)(xvalue var, str key, xvalue param);
		ptr value;
	};

	// Value 头部与共享状态的内联辅助函数
	static inline uint32 __xvoAtomicCompareExchange32(volatile uint32* pValue, uint32 iExchange, uint32 iComparand)
	{
		return __xrtAtomicCompareExchangeU32(pValue, iExchange, iComparand);
	}

	static inline void xvoInitHeader(xvalue pVal, uint32 iType, bool bStatic, bool bShared, uint32 iRefCount)
	{
		if ( pVal == NULL ) {
			return;
		}
		pVal->Header = XVO_HEADER_INIT(iType, bStatic, bShared, iRefCount);
	}

	static inline void xvoInitOwnedHeader_Inline(xvalue pVal, uint32 iType, uint32 iMode)
	{
		xvoInitHeader(pVal, iType, FALSE, iMode == XRT_OBJMODE_SHARED, 1);
	}

	static inline bool xvoIsShared_Inline(xvalue pVal)
	{
		if ( pVal == NULL ) {
			return FALSE;
		}
		return (pVal->Header & XVO_HEADER_SHARED_MASK) != 0;
	}

	static inline void xvoSetShared_Inline(xvalue pVal)
	{
		if ( pVal == NULL || pVal->IsStatic ) {
			return;
		}
		pVal->Header |= XVO_HEADER_SHARED_MASK;
	}

	static inline bool xrtOwnerIsRealShared(const xrtOwnerInfo* pOwner)
	{
		if ( pOwner == NULL ) {
			return FALSE;
		}
		return pOwner->iMode == XRT_OBJMODE_SHARED && (pOwner->iFlags & XRT_OBJFLAG_SHARED_PENDING) == 0;
	}

	static inline bool xvoMakeShared_Inline(xvalue pVal)
	{
		if ( pVal == NULL || pVal->IsStatic ) {
			return TRUE;
		}
		switch ( pVal->Type ) {
			case XVO_DT_ARRAY:
				if ( !xrtOwnerIsRealShared(&pVal->vArray->Owner) ) {
					xrtSetError((str)"array value requires a real shared array root.", FALSE);
					return FALSE;
				}
				break;
			case XVO_DT_LIST:
				if ( !xrtOwnerIsRealShared(&pVal->vList->Owner) ) {
					xrtSetError((str)"list value requires a real shared list root.", FALSE);
					return FALSE;
				}
				break;
			case XVO_DT_TABLE:
				if ( !xrtOwnerIsRealShared(&pVal->vTable->Owner) ) {
					xrtSetError((str)"table value requires a real shared table root.", FALSE);
					return FALSE;
				}
				break;
			case XVO_DT_COLL:
				if ( !xrtOwnerIsRealShared(&pVal->vColl->Owner) ) {
					xrtSetError((str)"coll value requires a real shared coll root.", FALSE);
					return FALSE;
				}
				break;
			default:
				break;
		}
		xvoSetShared_Inline(pVal);
		return TRUE;
	}

	static inline bool xvoPrepareStoreWithOwner_Inline(const xrtOwnerInfo* pOwner, xvalue pVal)
	{
		if ( pVal == NULL ) {
			return FALSE;
		}
		if ( !xrtOwnerIsRealShared(pOwner) ) {
			return TRUE;
		}
		return xvoMakeShared_Inline(pVal);
	}
	
	// 引用计数操作
	XXAPI void xvoAddRef(xvalue pVal);
	XXAPI void xvoUnref(xvalue pVal);
	static inline void xvoAddRef_Inline(xvalue pVal)
	{
		uint32 iOldHeader;
		uint32 iNewHeader;
		uint32 iRefCount;
		if ( pVal == NULL ) {
			return;
		}
		if ( pVal->IsStatic ) {
			return;
		}
		if ( !xvoIsShared_Inline(pVal) ) {
			if ( pVal->RefCount >= XVO_HEADER_REFCOUNT_MAX ) {
				// 引用计数太多，就转为静态值
				pVal->IsStatic = 1;
			} else {
				pVal->RefCount++;
			}
			return;
		}
		while ( TRUE ) {
			iOldHeader = pVal->Header;
			if ( iOldHeader & XVO_HEADER_STATIC_MASK ) {
				return;
			}
			iRefCount = (iOldHeader & XVO_HEADER_REFCOUNT_MASK) >> XVO_HEADER_REFCOUNT_SHIFT;
			if ( iRefCount >= XVO_HEADER_REFCOUNT_MAX ) {
				iNewHeader = iOldHeader | XVO_HEADER_STATIC_MASK;
			} else {
				iNewHeader = iOldHeader + XVO_HEADER_REFCOUNT_ONE;
			}
			if ( __xvoAtomicCompareExchange32(&pVal->Header, iNewHeader, iOldHeader) == iOldHeader ) {
				return;
			}
		}
	}
	
	// 创建值
	XXAPI xvalue xvoCreateNull();
	XXAPI xvalue xvoCreateBool(bool bVal);
	XXAPI xvalue xvoCreateInt(int64 iVal);
	XXAPI xvalue xvoCreateFloat(double fVal);
	XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
	XXAPI xvalue xvoCreateTime(xtime tVal);
	XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
	XXAPI xvalue xvoCreatePoint(ptr point);
	XXAPI xvalue xvoCreateFunc(xfunction pFunc);
	XXAPI xvalue xvoCreateArray();
	XXAPI xvalue xvoCreateArrayEx(uint32 iMode);
	XXAPI xvalue xvoCreateList();
	XXAPI xvalue xvoCreateListEx(uint32 iMode);
	XXAPI xvalue xvoCreateColl();
	XXAPI xvalue xvoCreateCollEx(uint32 iMode);
	XXAPI xvalue xvoCreateTable();
	XXAPI xvalue xvoCreateTableEx(uint32 iMode);
	XXAPI xvalue xvoCreateClass(uint32 iSize);
	XXAPI xvalue xvoCreateCustom(ptr pObj);
	
	// 读取值
	XXAPI bool xvoGetBool(xvalue pVal);
	XXAPI int64 xvoGetInt(xvalue pVal);
	XXAPI double xvoGetFloat(xvalue pVal);
	XXAPI str xvoGetText(xvalue pVal);
	XXAPI xtime xvoGetTime(xvalue pVal);
	XXAPI ptr xvoGetPoint(xvalue pVal);
	XXAPI xfunction xvoGetFunc(xvalue pVal);
	XXAPI xparray xvoGetArray(xvalue pVal);
	XXAPI xlist xvoGetList(xvalue pVal);
	XXAPI xavltree xvoGetColl(xvalue pVal);
	XXAPI xdict xvoGetTable(xvalue pVal);
	XXAPI ptr xvoGetClass(xvalue pVal);
	XXAPI ptr xvoGetCustom(xvalue pVal);
	
	// Array 读数据
	XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index);
	#define xvoArrayGetBool(pArr, index)														xvoGetBool(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetInt(pArr, index)															xvoGetInt(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetFloat(pArr, index)														xvoGetFloat(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetText(pArr, index)														xvoGetText(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetTime(pArr, index)														xvoGetTime(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetPoint(pArr, index)														xvoGetPoint(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetFunc(pArr, index)														xvoGetFunc(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetArray(pArr, index)														xvoGetArray(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetList(pArr, index)														xvoGetList(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetColl(pArr, index)														xvoGetColl(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetTable(pArr, index)														xvoGetTable(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetClass(pArr, index)														xvoGetClass(xvoArrayGetValue(pArr, index))
	#define xvoArrayGetCustom(pArr, index)														xvoGetCustom(xvoArrayGetValue(pArr, index))
	
	// Array 追加数据
	XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
	#define xvoArrayAppendNull(pArr)															xvoArrayAppendValue(pArr, xvoCreateNull(), TRUE)
	#define xvoArrayAppendBool(pArr, bVal)														xvoArrayAppendValue(pArr, xvoCreateBool(bVal), TRUE)
	#define xvoArrayAppendInt(pArr, iVal)														xvoArrayAppendValue(pArr, xvoCreateInt(iVal), TRUE)
	#define xvoArrayAppendFloat(pArr, fVal)														xvoArrayAppendValue(pArr, xvoCreateFloat(fVal), TRUE)
	#define xvoArrayAppendText(pArr, sVal, iSize, bColloc)										xvoArrayAppendValue(pArr, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArrayAppendTime(pArr, tVal)														xvoArrayAppendValue(pArr, xvoCreateTime(tVal), TRUE)
	#define xvoArrayAppendTimeSerial(pArr, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoArrayAppendValue(pArr, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArrayAppendPoint(pArr, pVal)														xvoArrayAppendValue(pArr, xvoCreatePoint(pVal), TRUE)
	#define xvoArrayAppendFunc(pArr, func)														xvoArrayAppendValue(pArr, xvoCreateFunc(func), TRUE)
	#define xvoArrayAppendArray(pArr)															xvoArrayAppendValue(pArr, xvoCreateArray(), TRUE)
	#define xvoArrayAppendList(pArr)															xvoArrayAppendValue(pArr, xvoCreateList(), TRUE)
	#define xvoArrayAppendColl(pArr)															xvoArrayAppendValue(pArr, xvoCreateColl(), TRUE)
	#define xvoArrayAppendTable(pArr)															xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
	#define xvoArrayAppendClass(pArr, size)														xvoArrayAppendValue(pArr, xvoCreateClass(size), TRUE)
	#define xvoArrayAppendCustom(pArr, point)													xvoArrayAppendValue(pArr, xvoCreateCustom(point), TRUE)
	
	// Array 插入操作
	XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
	#define xvoArrayInsertNull(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateNull(), TRUE)
	#define xvoArrayInsertBool(pArr, idx, bVal)													xvoArrayInsertValue(pArr, idx, xvoCreateBool(bVal), TRUE)
	#define xvoArrayInsertInt(pArr, idx, iVal)													xvoArrayInsertValue(pArr, idx, xvoCreateInt(iVal), TRUE)
	#define xvoArrayInsertFloat(pArr, idx, fVal)												xvoArrayInsertValue(pArr, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoArrayInsertText(pArr, idx, sVal, iSize, bColloc)									xvoArrayInsertValue(pArr, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArrayInsertTime(pArr, idx, tVal)													xvoArrayInsertValue(pArr, idx, xvoCreateTime(tVal), TRUE)
	#define xvoArrayInsertTimeSerial(pArr, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)	xvoArrayInsertValue(pArr, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArrayInsertPoint(pArr, idx, pVal)												xvoArrayInsertValue(pArr, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoArrayInsertFunc(pArr, idx, func)													xvoArrayInsertValue(pArr, idx, xvoCreateFunc(func), TRUE)
	#define xvoArrayInsertArray(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateArray(), TRUE)
	#define xvoArrayInsertList(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateList(), TRUE)
	#define xvoArrayInsertColl(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateColl(), TRUE)
	#define xvoArrayInsertTable(pArr, idx)														xvoArrayInsertValue(pArr, idx, xvoCreateTable(), TRUE)
	#define xvoArrayInsertClass(pArr, idx, size)												xvoArrayInsertValue(pArr, idx, xvoCreateClass(size), TRUE)
	#define xvoArrayInsertCustom(pArr, idx, point)												xvoArrayInsertValue(pArr, idx, xvoCreateCustom(point), TRUE)
	
	// Array 修改操作
	XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc);
	#define xvoArraySetNull(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateNull(), TRUE)
	#define xvoArraySetBool(pArr, idx, bVal)													xvoArraySetValue(pArr, idx, xvoCreateBool(bVal), TRUE)
	#define xvoArraySetInt(pArr, idx, iVal)														xvoArraySetValue(pArr, idx, xvoCreateInt(iVal), TRUE)
	#define xvoArraySetFloat(pArr, idx, fVal)													xvoArraySetValue(pArr, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoArraySetText(pArr, idx, sVal, iSize, bColloc)									xvoArraySetValue(pArr, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoArraySetTime(pArr, idx, tVal)													xvoArraySetValue(pArr, idx, xvoCreateTime(tVal), TRUE)
	#define xvoArraySetTimeSerial(pArr, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoArraySetValue(pArr, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoArraySetPoint(pArr, idx, pVal)													xvoArraySetValue(pArr, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoArraySetFunc(pArr, idx, func)													xvoArraySetValue(pArr, idx, xvoCreateFunc(func), TRUE)
	#define xvoArraySetArray(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateArray(), TRUE)
	#define xvoArraySetList(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateList(), TRUE)
	#define xvoArraySetColl(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateColl(), TRUE)
	#define xvoArraySetTable(pArr, idx)															xvoArraySetValue(pArr, idx, xvoCreateTable(), TRUE)
	#define xvoArraySetClass(pArr, idx, size)													xvoArraySetValue(pArr, idx, xvoCreateClass(size), TRUE)
	#define xvoArraySetCustom(pArr, idx, point)													xvoArraySetValue(pArr, idx, xvoCreateCustom(point), TRUE)
	
	// Array 合并
	XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2);
	
	// Array 操作
	XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2);
	XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);
	XXAPI uint32 xvoArrayItemCount(xvalue pArr);
	XXAPI bool xvoArrayClear(xvalue pArr);
	XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count);
	XXAPI bool xvoArraySort(xvalue pArr, ptr proc);
	
	// List 读数据
	XXAPI xvalue xvoListGetValue(xvalue pList, int64 index);
	#define xvoListGetBool(pList, index)														xvoGetBool(xvoListGetValue(pList, index))
	#define xvoListGetInt(pList, index)															xvoGetInt(xvoListGetValue(pList, index))
	#define xvoListGetFloat(pList, index)														xvoGetFloat(xvoListGetValue(pList, index))
	#define xvoListGetText(pList, index)														xvoGetText(xvoListGetValue(pList, index))
	#define xvoListGetTime(pList, index)														xvoGetTime(xvoListGetValue(pList, index))
	#define xvoListGetPoint(pList, index)														xvoGetPoint(xvoListGetValue(pList, index))
	#define xvoListGetFunc(pList, index)														xvoGetFunc(xvoListGetValue(pList, index))
	#define xvoListGetArray(pList, index)														xvoGetArray(xvoListGetValue(pList, index))
	#define xvoListGetList(pList, index)														xvoGetList(xvoListGetValue(pList, index))
	#define xvoListGetColl(pList, index)														xvoGetColl(xvoListGetValue(pList, index))
	#define xvoListGetTable(pList, index)														xvoGetTable(xvoListGetValue(pList, index))
	#define xvoListGetClass(pList, index)														xvoGetClass(xvoListGetValue(pList, index))
	#define xvoListGetCustom(pList, index)														xvoGetCustom(xvoListGetValue(pList, index))
	
	// List 写数据
	XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc);
	#define xvoListSetNull(pList, idx)															xvoListSetValue(pList, idx, xvoCreateNull(), TRUE)
	#define xvoListSetBool(pList, idx, bVal)													xvoListSetValue(pList, idx, xvoCreateBool(bVal), TRUE)
	#define xvoListSetInt(pList, idx, iVal)														xvoListSetValue(pList, idx, xvoCreateInt(iVal), TRUE)
	#define xvoListSetFloat(pList, idx, fVal)													xvoListSetValue(pList, idx, xvoCreateFloat(fVal), TRUE)
	#define xvoListSetText(pList, idx, sVal, iSize, bColloc)									xvoListSetValue(pList, idx, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoListSetTime(pList, idx, tVal)													xvoListSetValue(pList, idx, xvoCreateTime(tVal), TRUE)
	#define xvoListSetTimeSerial(pList, idx, iYear, iMonth, iDay, iHour, iMinute, iSecond)		xvoListSetValue(pList, idx, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoListSetPoint(pList, idx, pVal)													xvoListSetValue(pList, idx, xvoCreatePoint(pVal), TRUE)
	#define xvoListSetFunc(pList, idx, func)													xvoListSetValue(pList, idx, xvoCreateFunc(func), TRUE)
	#define xvoListSetArray(pList, idx)															xvoListSetValue(pList, idx, xvoCreateArray(), TRUE)
	#define xvoListSetList(pList, idx)															xvoListSetValue(pList, idx, xvoCreateList(), TRUE)
	#define xvoListSetColl(pList, idx)															xvoListSetValue(pList, idx, xvoCreateColl(), TRUE)
	#define xvoListSetTable(pList, idx)															xvoListSetValue(pList, idx, xvoCreateTable(), TRUE)
	#define xvoListSetClass(pList, idx, size)													xvoListSetValue(pList, idx, xvoCreateClass(size), TRUE)
	#define xvoListSetCustom(pList, idx, point)													xvoListSetValue(pList, idx, xvoCreateCustom(point), TRUE)
	
	// List 合并
	XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite);
	
	// List 操作
	XXAPI bool xvoListExists(xvalue pList, int64 index);
	XXAPI bool xvoListRemove(xvalue pList, int64 index);
	XXAPI uint32 xvoListItemCount(xvalue pList);
	XXAPI bool xvoListClear(xvalue pList);
	XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList);
	
	// Coll Key 数据结构
	typedef struct {
		uint64 Hash;
		xvalue Value;
	} Coll_Key;
	
	// 构建 Coll_Key（文本值额外编码长度与哈希，其他值复用类型位和数值位）
	#if defined(__x86_64__) || defined(_M_X64)
		// 64 bit
		#define MAKE_COLL_KEY(k, v) if ( v->Type == XVO_DT_TEXT ) { uint64 iHash = xrtHash64(v->vText, v->Size); k.Hash = ((uint64)v->Type << 60) | ((uint64)v->Size << 28) | (iHash & 0xFFFFFFF); } else if ( v->Type == XVO_DT_BOOL ) { k.Hash = ((uint64)v->Type << 60) | v->vBool; } else if ( v->Type == XVO_DT_NULL ) { k.Hash = (uint64)v->Type << 60; } else { k.Hash = ((uint64)v->Type << 60) | (v->vInt & 0xFFFFFFFFFFFFFFF); } k.Value = v;
	#elif defined(__i386__) || defined(_M_IX86)
		// 32 bit
		#define MAKE_COLL_KEY(k, v) if ( v->Type == XVO_DT_TEXT ) { uint32 iHash = xrtHash32(v->vText, v->Size); k.Hash = ((uint64)v->Type << 60) | ((uint64)v->Size << 28) | (iHash & 0xFFFFFFF); } else if ( v->Type == XVO_DT_BOOL ) { k.Hash = ((uint64)v->Type << 60) | v->vBool; } else if ( v->Type == XVO_DT_NULL ) { k.Hash = (uint64)v->Type << 60; } else { k.Hash = ((uint64)v->Type << 60) | (v->vInt & 0xFFFFFFFFFFFFFFF); } k.Value = v;
	#endif
	
	// Coll 内联写数据
	static inline bool xvoCollSetValueWithKey(xavltree pColl, Coll_Key* objKey, bool bColloc)
	{
		bool bNew;
		if ( pColl == NULL || objKey == NULL || objKey->Value == NULL ) {
			return FALSE;
		}
		if ( !xvoPrepareStoreWithOwner_Inline(&pColl->Owner, objKey->Value) ) {
			return FALSE;
		}
		Coll_Key* pNode = xrtAVLTreeInsert(pColl, objKey, &bNew);
		if ( pNode ) {
			if ( bNew ) {
				pNode->Hash = objKey->Hash;
				pNode->Value = objKey->Value;
				if ( bColloc == FALSE ) {
					xvoAddRef_Inline(objKey->Value);
				}
			} else {
				if ( bColloc ) {
					xvoUnref(objKey->Value);
				}
			}
			return TRUE;
		}
		return FALSE;
	}
	
	// Coll 写数据
	XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc);
	#define xvoCollSetNull(pList)																xvoCollSetValue(pList, xvoCreateNull(), TRUE)
	#define xvoCollSetBool(pList, bVal)															xvoCollSetValue(pList, xvoCreateBool(bVal), TRUE)
	#define xvoCollSetInt(pList, iVal)															xvoCollSetValue(pList, xvoCreateInt(iVal), TRUE)
	#define xvoCollSetFloat(pList, fVal)														xvoCollSetValue(pList, xvoCreateFloat(fVal), TRUE)
	#define xvoCollSetText(pList, sVal, iSize, bColloc)											xvoCollSetValue(pList, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoCollSetTime(pList, tVal)															xvoCollSetValue(pList, xvoCreateTime(tVal), TRUE)
	#define xvoCollSetTimeSerial(pList, iYear, iMonth, iDay, iHour, iMinute, iSecond)			xvoCollSetValue(pList, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoCollSetPoint(pList, pVal)														xvoCollSetValue(pList, xvoCreatePoint(pVal), TRUE)
	#define xvoCollSetFunc(pList, func)															xvoCollSetValue(pList, xvoCreateFunc(func), TRUE)
	#define xvoCollSetCustom(pList, point)														xvoCollSetValue(pList, xvoCreateCustom(point), TRUE)
	
	// Coll 获取差集 [ pSelf 集合相对 pColl 集合不存在的元素 ]
	XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl);
	
	// Coll 获取对称差集 [ 两个集合中不重复的元素 ]
	XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl);
	
	// Coll 获取交集 [ pSelf 集合相对 pColl 集合存在的元素 ]
	XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl);
	
	// Coll 获取并集 [ 合并两个集合，返回和一个新的集合 ]
	XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl);
	
	// Coll 合并集合 [ 将 pColl 中的元素并入 pSelf ]
	XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl);
	
	// Coll 操作
	XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal);
	XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal);
	XXAPI uint32 xvoCollItemCount(xvalue pColl);
	XXAPI bool xvoCollClear(xvalue pColl);
	XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl);
	
	// Table 读数据
	XXAPI xvalue xvoTableGetValue(xvalue pTbl, const void* key, uint32 kl);
	#define xvoTableGetBool(pTbl, key, kl)														xvoGetBool(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetInt(pTbl, key, kl)														xvoGetInt(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetFloat(pTbl, key, kl)														xvoGetFloat(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetText(pTbl, key, kl)														xvoGetText(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetTime(pTbl, key, kl)														xvoGetTime(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetPoint(pTbl, key, kl)														xvoGetPoint(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetFunc(pTbl, key, kl)														xvoGetFunc(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetArray(pTbl, key, kl)														xvoGetArray(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetList(pTbl, key, kl)														xvoGetList(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetColl(pTbl, key, kl)														xvoGetColl(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetTable(pTbl, key, kl)														xvoGetTable(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetClass(pTbl, key, kl)														xvoGetClass(xvoTableGetValue(pTbl, key, kl))
	#define xvoTableGetCustom(pTbl, key, kl)													xvoGetCustom(xvoTableGetValue(pTbl, key, kl))
	
	// Table 写数据
	XXAPI bool xvoTableSetValue(xvalue pTbl, const void* key, uint32 kl, xvalue pVal, bool bColloc);
	#define xvoTableSetNull(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateNull(), TRUE)
	#define xvoTableSetBool(pTbl, key, kl, bVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateBool(bVal), TRUE)
	#define xvoTableSetInt(pTbl, key, kl, iVal)													xvoTableSetValue(pTbl, key, kl, xvoCreateInt(iVal), TRUE)
	#define xvoTableSetFloat(pTbl, key, kl, fVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateFloat(fVal), TRUE)
	#define xvoTableSetText(pTbl, key, kl, sVal, iSize, bColloc)								xvoTableSetValue(pTbl, key, kl, xvoCreateText(sVal, iSize, bColloc), TRUE)
	#define xvoTableSetTime(pTbl, key, kl, tVal)												xvoTableSetValue(pTbl, key, kl, xvoCreateTime(tVal), TRUE)
	#define xvoTableSetTimeSerial(pTbl, key, kl, iYear, iMonth, iDay, iHour, iMinute, iSecond)	xvoTableSetValue(pTbl, key, kl, xvoCreateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond), TRUE)
	#define xvoTableSetPoint(pTbl, key, kl, pVal)												xvoTableSetValue(pTbl, key, kl, xvoCreatePoint(pVal), TRUE)
	#define xvoTableSetFunc(pTbl, key, kl, func)												xvoTableSetValue(pTbl, key, kl, xvoCreateFunc(func), TRUE)
	#define xvoTableSetArray(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateArray(), TRUE)
	#define xvoTableSetList(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateList(), TRUE)
	#define xvoTableSetColl(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateColl(), TRUE)
	#define xvoTableSetTable(pTbl, key, kl)														xvoTableSetValue(pTbl, key, kl, xvoCreateTable(), TRUE)
	#define xvoTableSetClass(pTbl, key, kl, size)												xvoTableSetValue(pTbl, key, kl, xvoCreateClass(size), TRUE)
	#define xvoTableSetCustom(pTbl, key, kl, point)												xvoTableSetValue(pTbl, key, kl, xvoCreateCustom(point), TRUE)
	
	// Table 合并
	XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite);
	
	// Table 操作
	XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl);
	XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);
	XXAPI uint32 xvoTableItemCount(xvalue pTbl);
	XXAPI bool xvoTableClear(xvalue pTbl);
	XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
	
	// 类型操作
	XXAPI bool xvoIsNull(xvalue pVal);
	XXAPI int xvoType(xvalue pVal);
	#define xvoArrayItemType(pArr, index)														xvoType(xvoArrayGetValue(pArr, index))
	#define xvoListItemType(pList, index)														xvoType(xvoListGetValue(pList, index))
	#define xvoTableItemType(pTbl, key, kl)														xvoType(xvoTableGetValue(pTbl, key, kl))
	
	// 获取数据长度
	XXAPI uint32 xvoGetSize(xvalue pVal);
	#define xvoArrayItemSize(pArr, index)														xvoGetSize(xvoArrayGetValue(pArr, index))
	#define xvoListItemSize(pList, index)														xvoGetSize(xvoListGetValue(pList, index))
	#define xvoTableItemSize(pTbl, key, kl)														xvoGetSize(xvoTableGetValue(pTbl, key, kl))
	
	// 浅拷贝
	XXAPI xvalue xvoCopy(xvalue pVal);
	
	// 深拷贝
	XXAPI xvalue xvoDeepCopy(xvalue pVal);
	
	// 输出 xte Value 的结构和值
	XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey);
	
	
	
	/* ------------------------------------ JSON 函数库 ------------------------------------ */
	/*
		依赖项：
			File 函数库
			Value 函数库
			JNUM 函数库
	*/
	
	// json对象的类型
	typedef enum {
		JSON_NULL = 0,              /* It doesn't has value variable: null */
		JSON_BOOL,                  /* Its value variable is vbool: true, false */
		JSON_INT,                   /* Its value variable is vint */
		JSON_HEX,                   /* Its value variable is vhex */
		JSON_LINT,                  /* Its value variable is vlint */
		JSON_LHEX,                  /* Its value variable is vlhex */
		JSON_DOUBLE,                /* Its value variable is vdbl */
		JSON_STRING,                /* Its value variable is vstr */
		JSON_ARRAY,                 /* Collection type: array */
		JSON_OBJECT                 /* Collection type: object */
	} json_type_t;
	
	// json对象的键或字符串类型的值的信息（LJSON使用此结构就知道了字符串长度，可以加快数据处理）
	typedef struct {
		uint32_t type:4;			// json对象的类型，只在作为json对象的键时才有效
		uint32_t escaped:1;			// 是否含转义字符
		uint32_t alloced:1;			// 是否是在堆中分配，只在SAX接口中有效
		uint32_t reserved:2;		// 保留位
		uint32_t len:24;			// 字符串长度
	} json_strinfo_t;
	
	// 带信息的字符串（LJSON使用此结构就知道了字符串长度，可以加快数据处理）
	typedef struct {
		char *str;					// 字符串数据
		json_strinfo_t info;		// 字符串信息
	} json_string_t;
	
	// json数字类型的值（LJSON支持长整数和十六进制数）
	typedef union {
		bool vbool;
		int32_t vint;
		uint32_t vhex;
		int64_t vlint;
		uint64_t vlhex;
		double vdbl;
	} json_number_t;
	
	// SAX APIs中指示JSON_ARRAY或JSON_OBJECT的开始和结束（集合类型是括号包起来的，JSON_SAX_START表示左边括号, JSON_SAX_FINISH指示右边括号）
	typedef enum {
		JSON_SAX_START = 0,
		JSON_SAX_FINISH
	} json_sax_cmd_t;
	
	// json对象的值（LJSON使用union管理对象的值从而节省内存空间）
	typedef union {
		json_number_t vnum;			// 数字类型的值
		json_string_t vstr;			// 字符串类型的值
		json_sax_cmd_t vcmd;		// SAX APIs指示集合对象的开始和结束
	} json_sax_value_t;
	
	// SAX解析器传递给回调函数的描述（LJSON SAX解析将维护用于状态机的深度信息。）
	typedef struct {
		int total;					// 数组深度的大小
		int index;					// JSON类型和键的当前索引
		json_string_t *array;		// 存储JSON对象类型和键的JSON深度信息
		json_sax_value_t value;		// json对象值
		void *userdata;				// 用户数据指针
	} json_sax_parser_t;
	
	// SAX的回调函数（返回 `JSON_SAX_PARSE_CONTINUE` 表示继续解析；返回 `JSON_SAX_PARSE_STOP` 表示停止解析）
	typedef enum {
		JSON_SAX_PARSE_CONTINUE = 0,
		JSON_SAX_PARSE_STOP
	} json_sax_ret_t;

	// SAX 回调函数类型
	typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);
	
	// 获取json_string_t的信息（str含有 `"  \  \b  \f  \n  \r  \t  \v` 字符时会设置escaped）
	XXAPI json_strinfo_t xrtJsonGetStringInfo(const char *str, const json_strinfo_t *orig);
	
	// 更新json_string_t的信息（jstr->info.len是0时数据才会更新）
	static inline void xrtJsonUpdateStringInfo(json_string_t *jstr)
	{
		if (!jstr->info.len)
			jstr->info = xrtJsonGetStringInfo(jstr->str, &jstr->info);
	}
	
	// 从字符串进行SAX解析（失败返回-1；成功返回0）
	XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb);
	
	// 打印缓冲
	typedef struct {
		size_t size;
		char *p;
	} json_print_ptr_t;
	
	// 打印参数设置
	typedef struct {
		size_t plus_size;
		size_t item_size;
		int item_total;
		bool format_flag;
		json_print_ptr_t *ptr;
	} json_print_choice_t;
	
	// SAX打印句柄（实际就是 `json_sax_print_t` 的指针）
	typedef ptr json_sax_print_hd;
	
	// 启动SAX打印器；失败返回NULL，成功返回指针（SAX打印器的句柄）
	XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice);
	
	// 启动格式化的SAX打印器，输出到字符串
	static inline json_sax_print_hd json_sax_print_format_start(int item_total, json_print_ptr_t *ptr)
	{
		json_print_choice_t choice;

		memset(&choice, 0, sizeof(choice));
		choice.format_flag = true;
		choice.item_total = item_total;
		choice.ptr = ptr;
		return xrtJsonPrintStart(&choice);
	}
	
	// 启动压缩的SAX打印器，输出到字符串
	static inline json_sax_print_hd json_sax_print_unformat_start(int item_total, json_print_ptr_t *ptr)
	{
		json_print_choice_t choice;

		memset(&choice, 0, sizeof(choice));
		choice.format_flag = false;
		choice.item_total = item_total;
		choice.ptr = ptr;
		return xrtJsonPrintStart(&choice);
	}
	
	// SAX打印json对象
	XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
	// SAX 打印快捷包装
	static inline int xrtJsonPrintNull(json_sax_print_hd handle, json_string_t *jkey) { return xrtJsonPrintValue(handle, JSON_NULL, jkey, NULL); }
	static inline int xrtJsonPrintBool(json_sax_print_hd handle, json_string_t *jkey, bool value) { return xrtJsonPrintValue(handle, JSON_BOOL, jkey, &value); }
	static inline int xrtJsonPrintInt(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return xrtJsonPrintValue(handle, JSON_INT, jkey, &value); }
	static inline int xrtJsonPrintHex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return xrtJsonPrintValue(handle, JSON_HEX, jkey, &value); }
	static inline int xrtJsonPrintInt64(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return xrtJsonPrintValue(handle, JSON_LINT, jkey, &value); }
	static inline int xrtJsonPrintHex64(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return xrtJsonPrintValue(handle, JSON_LHEX, jkey, &value); }
	static inline int xrtJsonPrintDouble(json_sax_print_hd handle, json_string_t *jkey, double value) { return xrtJsonPrintValue(handle, JSON_DOUBLE, jkey, &value); }
	static inline int xrtJsonPrintString(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return xrtJsonPrintValue(handle, JSON_STRING, jkey, value); }
	static inline int xrtJsonPrintArray(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return xrtJsonPrintValue(handle, JSON_ARRAY, jkey, &value); }
	static inline int xrtJsonPrintObject(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return xrtJsonPrintValue(handle, JSON_OBJECT, jkey, &value); }
	
	// SAX 打印集合快捷宏
	#define xrtJsonPrintArrayNull(handle)         xrtJsonPrintNull  (handle, NULL, NULL)
	#define xrtJsonPrintArrayStart(handle, jkey)  xrtJsonPrintArray (handle, jkey, JSON_SAX_START)
	#define xrtJsonPrintArrayFinish(handle)       xrtJsonPrintArray (handle, NULL, JSON_SAX_FINISH)
	
	#define xrtJsonPrintObjectNull(handle, jkey)  xrtJsonPrintNull  (handle, jkey, NULL)
	#define xrtJsonPrintObjectStart(handle, jkey) xrtJsonPrintObject(handle, jkey, JSON_SAX_START)
	#define xrtJsonPrintObjectFinish(handle)      xrtJsonPrintObject(handle, NULL, JSON_SAX_FINISH)
	
	// 结束SAX打印器
	XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);
	
	// 解析 JSON
	XXAPI xvalue xrtParseJSON(str sText, size_t iSize);
	XXAPI xvalue xrtParseJSON_File(str sFile);
	
	// 将 xvalue 转换为 JSON
	XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);
	XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);
	
	
	
	/* ------------------------------------ XSON 函数库 ------------------------------------ */
	/*
		依赖项：
			File 函数库
			Value 函数库
			Time 函数库
			JNUM 函数库
			JSON 函数库
	*/
	
	#define XSON_F_IGNORE_UNSUPPORTED_ENCODE	0x0001u
	#define XSON_F_IGNORE_UNSUPPORTED_DECODE	0x0002u
	
	// 解析 XSON（保持对 JSON 的兼容）
	XXAPI xvalue xrtParseXSON(str sText, size_t iSize);
	XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags);
	XXAPI xvalue xrtParseXSON_File(str sFile);
	XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags);
	
	// 将 xvalue 转换为 XSON
	XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize);
	XXAPI int xrtStringifyXSON_File(str sFile, xvalue varVal, int bFormat, uint32 iFlags);
	
	
	
	/* ------------------------------------ Template 函数库 ------------------------------------ */
	/*
		依赖项：
			File 函数库
			Array 函数库
			Point Array 函数库
			Dict 函数库
			List 函数库
			Value 函数库
			JNUM 函数库
	*/
	
	typedef struct XTE_Engine_Struct* xteengine;
	typedef struct XTE_Template_Struct* xtetemplate;
	typedef struct XTE_RenderCtx_Struct XTE_RenderCtx;
	typedef struct XTE_StmtParseCtx_Struct XTE_StmtParseCtx;
	typedef struct XTE_StmtRenderCtx_Struct XTE_StmtRenderCtx;
	typedef struct XTE_FuncCtx_Struct XTE_FuncCtx;

	#define XTE_NODE_TEXT					1
	#define XTE_NODE_OUTPUT					2
	#define XTE_NODE_INLINE_BOOL			3
	#define XTE_NODE_STATEMENT				4

	#define XTE_EXPR_PATH					1
	#define XTE_EXPR_TEXT					2
	#define XTE_EXPR_INT					3
	#define XTE_EXPR_BOOL					4
	#define XTE_EXPR_BOOL_EXPR				5

	#define XTE_OUTPUT_TEXT					1
	#define XTE_OUTPUT_NUM					2
	#define XTE_OUTPUT_TIME					3
	#define XTE_OUTPUT_FUNC					4

	#define XTE_STMT_INLINE					0x0001
	#define XTE_STMT_BLOCK					0x0002
	#define XTE_STMT_HYBRID					(XTE_STMT_INLINE | XTE_STMT_BLOCK)
	#define XTE_STMT_RAW_BODY				0x0004
	#define XTE_STMT_ALLOW_NAMED_ARGS		0x0008

	typedef struct {
		int iCode;
		const char* sDesc;
		uint32 iLine;
		uint32 iColumn;
		uint32 iPos;
		uint32 iRefLine;
		uint32 iRefColumn;
		uint32 iRefPos;
	} XTE_Error;

	typedef struct {
		int (*procWrite)(void* pUserData, const char* sText, size_t iSize);
		void* pUserData;
		size_t iWritten;
	} XTE_Writer;

	typedef struct {
		const char* sBracket;
		uint32 iFlags;
	} XTE_ParseOptions;

	typedef struct {
		xvalue pRoot;
		xvalue pCurrent;
		xvalue pGlobal;
		xdict pIncludeMap;
		XTE_Writer* pWriter;
		uint32 iFlags;
	} XTE_RenderOptions;

	typedef struct {
		uint32 iStart;
		uint32 iCount;
	} XTE_NodeSpan;

	typedef struct {
		uint32 iType;
		uint32 iFlags;
		uint32 iTextOff;
		uint32 iTextSize;
		int64 iIntValue;
		int iBoolValue;
	} XTE_ExprNode;

	typedef struct {
		uint32 iNameOff;
		uint32 iNameSize;
		uint32 iRawOff;
		uint32 iRawSize;
		uint32 iFlags;
		uint32 iExprIndex;
	} XTE_ArgItem;

	typedef struct {
		xtetemplate hTemplate;
		const XTE_ArgItem* pItems;
		uint32 iCount;
	} XTE_ArgList;

	typedef struct {
		uint32 iType;
		uint32 iFlags;
		uint32 iPos;
		uint32 iSize;
		union {
			struct {
				uint32 iTextOff;
				uint32 iTextSize;
			} Text;
			struct {
				uint32 iOutputType;
				uint32 iExprIndex;
				uint32 iFormatOff;
				uint32 iFormatSize;
				uint32 iNameOff;
				uint32 iNameSize;
				uint32 iArgStart;
				uint32 iArgCount;
			} Output;
			struct {
				uint32 iExprIndex;
				uint32 iTrueOff;
				uint32 iTrueSize;
				uint32 iFalseOff;
				uint32 iFalseSize;
			} InlineBool;
			struct {
				uint32 iStmtNameOff;
				uint32 iStmtNameSize;
				uint32 iArgStart;
				uint32 iArgCount;
				XTE_NodeSpan tBody;
				uint32 iRawBodyOff;
				uint32 iRawBodySize;
				ptr pData;
			} Statement;
		} Data;
	} XTE_Node;

	typedef enum {
		XTE_FLOW_OK = 0,
		XTE_FLOW_BREAK = 1,
		XTE_FLOW_CONTINUE = 2,
		XTE_FLOW_ERROR = -1
	} XTE_Flow;

	typedef struct XTE_StatementDef_Struct {
		const char* sName;
		uint32 iFlags;
		uint16 iMinArgs;
		uint16 iMaxArgs;
		void* pUserData;

		int (*procParse)(XTE_StmtParseCtx* pCtx, void** ppData);
		XTE_Flow (*procRender)(XTE_StmtRenderCtx* pCtx);
		void (*procFreeData)(void* pData);
	} XTE_StatementDef;

	typedef struct XTE_FunctionDef_Struct {
		const char* sName;
		uint16 iMinArgs;
		uint16 iMaxArgs;
		void* pUserData;
		int (*procCall)(XTE_FuncCtx* pCtx, xvalue* ppRet);
	} XTE_FunctionDef;

	struct XTE_StmtParseCtx_Struct {
		xteengine hEngine;
		xtetemplate hTemplate;
		const XTE_StatementDef* pDef;
		const XTE_ArgList* pArgs;
		const XTE_NodeSpan* pBody;
		const char* sRawBody;
		size_t iRawBodySize;
		XTE_Error* pError;
		void* pUserData;
	};

	struct XTE_StmtRenderCtx_Struct {
		XTE_RenderCtx* pRender;
		const XTE_StatementDef* pDef;
		const XTE_ArgList* pArgs;
		const XTE_NodeSpan* pBody;
		const char* sRawBody;
		size_t iRawBodySize;
		void* pData;
		void* pUserData;
	};

	struct XTE_FuncCtx_Struct {
		XTE_RenderCtx* pRender;
		const XTE_FunctionDef* pDef;
		const XTE_ArgList* pArgs;
		void* pUserData;
	};

	XXAPI xteengine xteCreateEngine(void);
	XXAPI void xteDestroyEngine(xteengine hEngine);
	XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
	XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef);
	XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef);

	XXAPI xtetemplate xteParseEx(xteengine hEngine, const char* sText, size_t iSize, const XTE_ParseOptions* pOptions, XTE_Error* pError);
	XXAPI xtetemplate xteParse(const char* sText, size_t iSize, const char* sBracket);
	XXAPI void xteDestroyTemplate(xtetemplate hTemplate);
	XXAPI void xteParseFree(xtetemplate hTemplate);

	XXAPI int xteRenderEx(xtetemplate hTemplate, const XTE_RenderOptions* pOptions, XTE_Error* pError);
	XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize);

	XXAPI xvalue xteResolvePath(const char* sPath, size_t iPathSize, xvalue pCurrent, xvalue pRoot, xvalue pLocal, xvalue pGlobal);

	XXAPI uint32 xteTemplateGetNodeCount(xtetemplate hTemplate);
	XXAPI uint32 xteTemplateGetExprCount(xtetemplate hTemplate);
	XXAPI uint32 xteTemplateGetArgCount(xtetemplate hTemplate);
	XXAPI uint32 xteTemplateGetStringPoolSize(xtetemplate hTemplate);
	XXAPI XTE_NodeSpan xteTemplateGetRootSpan(xtetemplate hTemplate);
	XXAPI const XTE_Node* xteTemplateGetNode(xtetemplate hTemplate, uint32 iIndex);
	XXAPI const XTE_ExprNode* xteTemplateGetExpr(xtetemplate hTemplate, uint32 iIndex);
	XXAPI const XTE_ArgItem* xteTemplateGetArg(xtetemplate hTemplate, uint32 iIndex);
	XXAPI const char* xteTemplateGetString(xtetemplate hTemplate, uint32 iOff);

	XXAPI uint32 xteArgCount(const XTE_ArgList* pArgs);
	XXAPI const XTE_ArgItem* xteArgAt(const XTE_ArgList* pArgs, uint32 iIndex);
	XXAPI const XTE_ArgItem* xteFindNamedArg(const XTE_ArgList* pArgs, const char* sName, size_t iNameSize);
	XXAPI const char* xteArgNameText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	XXAPI const char* xteArgRawText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	XXAPI uint32 xteArgExprType(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	XXAPI xvalue xteEvalArgValue(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
	XXAPI int xteEvalArgBool(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
	XXAPI int xteEvalArgInt(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
	XXAPI int xteEvalArgFloat(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
	XXAPI char* xteEvalArgText(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
	XXAPI int xteEvalArgBoolStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
	XXAPI int xteEvalArgIntStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
	XXAPI int xteEvalArgFloatStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
	XXAPI char* xteEvalArgTextStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);

	XXAPI const XTE_ArgItem* xteStmtParseRequireArg(XTE_StmtParseCtx* pCtx, uint32 iIndex, const char* sDesc);
	XXAPI const XTE_ArgItem* xteStmtParseRequireNamedArg(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	XXAPI const XTE_ArgItem* xteStmtParseRequireExprType(XTE_StmtParseCtx* pCtx, uint32 iIndex, uint32 iExprType, const char* sDesc);
	XXAPI const XTE_ArgItem* xteStmtParseRequireNamedExprType(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, uint32 iExprType, const char* sDesc);
	XXAPI const XTE_ArgItem* xteStmtRequireArg(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
	XXAPI const XTE_ArgItem* xteStmtRequireNamedArg(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	XXAPI int xteStmtRequireBoolStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
	XXAPI int xteStmtRequireNamedBoolStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
	XXAPI int xteStmtRequireIntStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
	XXAPI int xteStmtRequireNamedIntStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
	XXAPI int xteStmtRequireFloatStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
	XXAPI int xteStmtRequireNamedFloatStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
	XXAPI char* xteStmtRequireTextStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
	XXAPI char* xteStmtRequireNamedTextStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	XXAPI const XTE_ArgItem* xteFuncRequireArg(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
	XXAPI const XTE_ArgItem* xteFuncRequireNamedArg(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	XXAPI int xteFuncRequireBoolStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
	XXAPI int xteFuncRequireNamedBoolStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
	XXAPI int xteFuncRequireIntStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
	XXAPI int xteFuncRequireNamedIntStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
	XXAPI int xteFuncRequireFloatStrict(XTE_FuncCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
	XXAPI int xteFuncRequireNamedFloatStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
	XXAPI char* xteFuncRequireTextStrict(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
	XXAPI char* xteFuncRequireNamedTextStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	XXAPI int xteStmtParseSetError(XTE_StmtParseCtx* pCtx, int iCode, const char* sDesc);
	XXAPI XTE_Flow xteStmtSetError(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc);
	XXAPI int xteFuncSetError(XTE_FuncCtx* pCtx, int iCode, const char* sDesc);
	XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize);
	XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx);
	XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent);

	#ifdef XTE_ENABLE_FILE
	XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
	XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
	#endif

	#ifdef XTE_DEBUGMODE
	XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
	XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
	#endif
	
	
	
	/* ------------------------------------ HTTP Client ------------------------------------ */
	/*
		依赖项：
			Buffer 函数库
			Dict 函数库
			File 函数库
			Network 函数库
			Crypto 加密算法库
			TLS
	*/
	
	/* ---- HTTP 方法 ---- */
	typedef enum {
		XHTTP_GET = 0, XHTTP_POST, XHTTP_PUT, XHTTP_DELETE, XHTTP_PATCH, XHTTP_HEAD
	} xhttp_method;
	
	/* ---- HTTP 回调函数 ---- */
	// pBuf: 自增缓冲区，包含截至目前已接收的全部数据
	// iTotal: Content-Length (已知时为正数，未知时为 0)
	// iReceived: 已接收字节数
	// 返回值: true 继续, false 中止传输
	typedef bool (*xhttp_proc)(xbuffer pBuf, size_t iTotal, size_t iReceived);
	
	/* ---- HTTP 响应对象 ---- */
	typedef struct {
		int iStatusCode;              // HTTP 状态码 (200, 404, ...)
		xbuffer_struct tBody;         // 响应正文 (xbuffer 自增缓冲区)
		xbuffer_struct tRawHeaders;   // 原始响应头 (完整文本)
		char sVersion[16];            // "HTTP/1.1"
		char sStatusText[64];         // "OK", "Not Found" 等
		char sContentType[128];       // Content-Type 值
		size_t iContentLength;        // Content-Length (-1 表示未知)
	} xhttpresp_struct, *xhttpresp;
	
	/* ---- HTTP 请求对象 ---- */
	typedef struct {
		xhttp_method iMethod;           // 请求方法
		char sURL[2048];                // 完整 URL
		xbuffer_struct tHeaders;        // 自定义请求头 (Key: Value\r\n 格式追加)
		xbuffer_struct tBody;           // 请求正文
		int iMaxRedirects;              // 最大重定向次数 (默认 5)
		int iTimeoutSec;                // 超时秒数 (默认 10)
		bool bVerifySSL;                // SSL 证书验证 (默认 true)
		xhttp_proc procOnData;          // 流式数据回调
		ptr pUserData;                  // 用户自定义数据
		bool bIsMultipart;              // 是否为 multipart 请求
		xbuffer_struct tMultipart;      // multipart body 构建缓冲区
		char sBoundary[64];             // multipart boundary
		xdict pCookies;                 // Cookie 字典 (始终用于 cookies 管理)
		bool bCookiePersist;            // 持久化标志 (TRUE=jar 模式, 自动保存 Set-Cookie)
	} xhttpreq_struct, *xhttpreq;
	
	/* ---- 极简 API ---- */
	
	// GET 请求 - 返回响应对象，sHeaders 可为 NULL，pProc 可为 NULL
	XXAPI xhttpresp xrtHttpGet(str sURL, str sHeaders, xhttp_proc pProc);
	
	// POST 请求 - sBody 为请求正文 (默认 application/x-www-form-urlencoded)
	XXAPI xhttpresp xrtHttpPost(str sURL, str sBody, str sHeaders, xhttp_proc pProc);
	
	// GET 下载文件 - 数据写入 sFilePath，sHeaders 可为 NULL，pProc 用于进度回调
	XXAPI bool xrtHttpGetFile(str sURL, str sFilePath, str sHeaders, xhttp_proc pProc);
	
	// POST 下载文件 - sHeaders 可为 NULL
	XXAPI bool xrtHttpPostFile(str sURL, str sBody, str sFilePath, str sHeaders, xhttp_proc pProc);
	
	// 释放响应对象
	XXAPI void xrtHttpRespFree(xhttpresp pResp);
	
	/* ---- 完整 API ---- */
	
	// 创建/销毁请求对象
	XXAPI xhttpreq xrtHttpReqCreate(xhttp_method iMethod, str sURL);
	XXAPI void xrtHttpReqFree(xhttpreq pReq);
	
	// 设置请求头 (可多次调用追加不同 Header)
	XXAPI void xrtHttpReqSetHeader(xhttpreq pReq, str sName, str sValue);
	
	// 设置请求正文 (原始 body)
	XXAPI void xrtHttpReqSetBody(xhttpreq pReq, str pData, size_t iLen, str sContentType);
	
	// 添加表单字段 (application/x-www-form-urlencoded)
	XXAPI void xrtHttpReqAddField(xhttpreq pReq, str sName, str sValue);
	
	// 添加 Multipart 表单字段
	XXAPI void xrtHttpReqAddFormField(xhttpreq pReq, str sName, str sValue);
	
	// 添加 Multipart 文件
	XXAPI void xrtHttpReqAddFormFile(xhttpreq pReq, str sFieldName, str sFilePath, str sMimeType);
	
	// 添加 Multipart 内存数据 (作为文件上传)
	XXAPI void xrtHttpReqAddFormData(xhttpreq pReq, str sFieldName, str sFileName, str pData, size_t iLen, str sMimeType);
	
	// 配置选项
	XXAPI void xrtHttpReqSetTimeout(xhttpreq pReq, int iTimeoutSec);
	XXAPI void xrtHttpReqSetRedirect(xhttpreq pReq, int iMaxRedirects);
	XXAPI void xrtHttpReqSetVerifySSL(xhttpreq pReq, bool bVerify);
	XXAPI void xrtHttpReqSetCallback(xhttpreq pReq, xhttp_proc pProc);
	XXAPI void xrtHttpReqSetUserData(xhttpreq pReq, ptr pData);
	
	// Cookie 管理
	XXAPI void xrtHttpReqEnableCookies(xhttpreq pReq, bool bEnable);
	XXAPI void xrtHttpReqSetCookie(xhttpreq pReq, str sName, str sValue);
	XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName);
	
	// 执行请求 (阻塞, 返回响应对象)
	XXAPI xhttpresp xrtHttpReqExecute(xhttpreq pReq);
	
	// 响应对象读取辅助
	XXAPI int xrtHttpRespCode(xhttpresp pResp);
	XXAPI str xrtHttpRespBody(xhttpresp pResp);
	XXAPI size_t xrtHttpRespBodyLen(xhttpresp pResp);
	XXAPI str xrtHttpRespHeader(xhttpresp pResp, str sName);
	XXAPI str xrtHttpRespCookie(xhttpresp pResp, str sName);
	XXAPI str xrtHttpRespContentType(xhttpresp pResp);
	
	
	
	/* ------------------------------------ Memory Debug Helper 调试包装 ------------------------------------ */
	/*
		依赖项：
			Array 函数库
			Dict 函数库
			List 函数库
			AVLTree 函数库
			Dynamic Stack 函数库
			Memory Pool 函数库
			Fixed-Size Memory Pool 函数库
	*/

	#ifdef XRT_MEM_DEBUG
		// 调试包装创建函数（为容器记录来源文件与行号）
	XXAPI xarray xrtArrayCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtArrayInitDbg(xarray pArr, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtArrayDestroyDbg(xarray pArr, const char* sFile, uint32 iLine);
	XXAPI void xrtArrayUnitDbg(xarray pArr, const char* sFile, uint32 iLine);

	XXAPI xdict xrtDictCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtDictInitDbg(xdict objHT, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtDictDestroyDbg(xdict objHT, const char* sFile, uint32 iLine);
	XXAPI void xrtDictUnitDbg(xdict objHT, const char* sFile, uint32 iLine);

	XXAPI xlist xrtListCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtListInitDbg(xlist objList, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtListDestroyDbg(xlist objList, const char* sFile, uint32 iLine);
	XXAPI void xrtListUnitDbg(xlist objList, const char* sFile, uint32 iLine);

	XXAPI xavltree xrtAVLTreeCreateDbg(unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtAVLTreeInitDbg(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtAVLTreeDestroyDbg(xavltree objAVLT, const char* sFile, uint32 iLine);
	XXAPI void xrtAVLTreeUnitDbg(xavltree objAVLT, const char* sFile, uint32 iLine);

	XXAPI xdynstack xrtDynStackCreateDbg(uint32 iItemLength, const char* sFile, uint32 iLine);
	XXAPI void xrtDynStackInitDbg(xdynstack objSTK, uint32 iItemLength, const char* sFile, uint32 iLine);
	XXAPI void xrtDynStackDestroyDbg(xdynstack objSTK, const char* sFile, uint32 iLine);
	XXAPI void xrtDynStackUnitDbg(xdynstack objSTK, const char* sFile, uint32 iLine);

	XXAPI xmempool xrtMemPoolCreateDbg(int iCustom, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtMemPoolInitDbg(xmempool objMP, int iCustom, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtMemPoolDestroyDbg(xmempool objMP, const char* sFile, uint32 iLine);
	XXAPI void xrtMemPoolUnitDbg(xmempool objMP, const char* sFile, uint32 iLine);

	XXAPI xfsmempool xrtFSMemPoolCreateDbg(unsigned int iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtFSMemPoolInitDbg(xfsmempool objMM, unsigned int iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	XXAPI void xrtFSMemPoolDestroyDbg(xfsmempool objMM, const char* sFile, uint32 iLine);
	XXAPI void xrtFSMemPoolUnitDbg(xfsmempool objMM, const char* sFile, uint32 iLine);
	#endif

	#if defined(XRT_MEM_DEBUG) && !defined(XRT_BUILD_CORE)
		// 调试包装宏（自动注入调用位置）
	#define xrtArrayCreate(iItemLength, iMode) xrtArrayCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtArrayInit(pArr, iItemLength, iMode) xrtArrayInitDbg((pArr), (iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtArrayDestroy(pArr) xrtArrayDestroyDbg((pArr), __FILE__, __LINE__)
	#define xrtArrayUnit(pArr) xrtArrayUnitDbg((pArr), __FILE__, __LINE__)

	#define xrtDictCreate(iItemLength, iMode) xrtDictCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtDictInit(objHT, iItemLength, iMode) xrtDictInitDbg((objHT), (iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtDictDestroy(objHT) xrtDictDestroyDbg((objHT), __FILE__, __LINE__)
	#define xrtDictUnit(objHT) xrtDictUnitDbg((objHT), __FILE__, __LINE__)

	#define xrtListCreate(iItemLength, iMode) xrtListCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtListInit(objList, iItemLength, iMode) xrtListInitDbg((objList), (iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtListDestroy(objList) xrtListDestroyDbg((objList), __FILE__, __LINE__)
	#define xrtListUnit(objList) xrtListUnitDbg((objList), __FILE__, __LINE__)

	#define xrtAVLTreeCreate(iItemLength, procComp, iMode) xrtAVLTreeCreateDbg((iItemLength), (procComp), (iMode), __FILE__, __LINE__)
	#define xrtAVLTreeInit(objAVLT, iItemLength, procComp, iMode) xrtAVLTreeInitDbg((objAVLT), (iItemLength), (procComp), (iMode), __FILE__, __LINE__)
	#define xrtAVLTreeDestroy(objAVLT) xrtAVLTreeDestroyDbg((objAVLT), __FILE__, __LINE__)
	#define xrtAVLTreeUnit(objAVLT) xrtAVLTreeUnitDbg((objAVLT), __FILE__, __LINE__)

	#define xrtDynStackCreate(iItemLength) xrtDynStackCreateDbg((iItemLength), __FILE__, __LINE__)
	#define xrtDynStackInit(objSTK, iItemLength) xrtDynStackInitDbg((objSTK), (iItemLength), __FILE__, __LINE__)
	#define xrtDynStackDestroy(objSTK) xrtDynStackDestroyDbg((objSTK), __FILE__, __LINE__)
	#define xrtDynStackUnit(objSTK) xrtDynStackUnitDbg((objSTK), __FILE__, __LINE__)

	#define xrtMemPoolCreate(iCustom, iMode) xrtMemPoolCreateDbg((iCustom), (iMode), __FILE__, __LINE__)
	#define xrtMemPoolInit(objMP, iCustom, iMode) xrtMemPoolInitDbg((objMP), (iCustom), (iMode), __FILE__, __LINE__)
	#define xrtMemPoolDestroy(objMP) xrtMemPoolDestroyDbg((objMP), __FILE__, __LINE__)
	#define xrtMemPoolUnit(objMP) xrtMemPoolUnitDbg((objMP), __FILE__, __LINE__)

	#define xrtFSMemPoolCreate(iItemLength, iMode) xrtFSMemPoolCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtFSMemPoolInit(objMM, iItemLength, iMode) xrtFSMemPoolInitDbg((objMM), (iItemLength), (iMode), __FILE__, __LINE__)
	#define xrtFSMemPoolDestroy(objMM) xrtFSMemPoolDestroyDbg((objMM), __FILE__, __LINE__)
	#define xrtFSMemPoolUnit(objMM) xrtFSMemPoolUnitDbg((objMM), __FILE__, __LINE__)
	#endif

#endif


