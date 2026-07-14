#pragma once
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
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#ifndef UNUSED_ATTR
	#if defined(__GNUC__) || defined(__clang__)
		#define UNUSED_ATTR __attribute__((unused))
	#else
		#define UNUSED_ATTR
	#endif
#endif
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
	#include <io.h>
	#include <direct.h>
	#include <process.h>
	#include <share.h>
	#include <BaseTsd.h>
	#ifndef XRT_MSC_RUNTIME_COMPAT_DEFINED
		#define XRT_MSC_RUNTIME_COMPAT_DEFINED
	#endif
	#ifndef ssize_t
		typedef SSIZE_T ssize_t;
	#endif
	#ifndef mode_t
		typedef int mode_t;
	#endif
	#ifndef pid_t
		typedef int pid_t;
	#endif
	#ifndef strcasecmp
		#define strcasecmp _stricmp
	#endif
	#ifndef strncasecmp
		#define strncasecmp _strnicmp
	#endif
	#ifndef strdup
		#define strdup _strdup
	#endif
	#ifndef getpid
		#define getpid _getpid
	#endif
	#ifndef access
		#define access _access
	#endif
	#ifndef mkdir
		#define mkdir(sPath, iMode) _mkdir(sPath)
	#endif
	#ifndef rmdir
		#define rmdir _rmdir
	#endif
	#ifndef unlink
		#define unlink _unlink
	#endif
	#ifndef fileno
		#define fileno _fileno
	#endif
	#ifndef popen
		#define popen _popen
	#endif
	#ifndef pclose
		#define pclose _pclose
	#endif
	#ifndef __func__
		#define __func__ __FUNCTION__
	#endif
	#ifndef XRT_MSC_TIME_COMPAT_DEFINED
		#define XRT_MSC_TIME_COMPAT_DEFINED
		// 兼容 localtime_r 接口
		static inline struct tm* localtime_r(const time_t* pRawTime, struct tm* pResult)
		{
			return localtime_s(pResult, pRawTime) == 0 ? pResult : NULL;
		}
		// 兼容 gmtime_r 接口
		static inline struct tm* gmtime_r(const time_t* pRawTime, struct tm* pResult)
		{
			return gmtime_s(pResult, pRawTime) == 0 ? pResult : NULL;
		}
		// 兼容 timegm 接口
		static inline time_t timegm(struct tm* pTM)
		{
			return _mkgmtime(pTM);
		}
	#endif
#else
	#include <unistd.h>
	#if defined(_WIN32) || defined(_WIN64)
		#include <share.h>
	#endif
	#if !defined(_WIN32) && !defined(_WIN64)
		#include <utime.h>
	#endif
#endif
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
	#define XRT_NO_FILE_ASYNC
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
	#define XRT_NO_XSON
	#define XRT_NO_TEMPLATE
	#define XRT_NO_REGEX		// 禁用正则表达式模块
	#define XRT_NO_SUBPROCESS
	#define XRT_NO_LOGGER
	#define XRT_NO_TYPED_SPECIAL
#endif
// 网络根模块裁剪时，同步裁剪全部网络子库
#if defined(XRT_NO_NETWORK)
	#ifndef XRT_NO_FILE_ASYNC
		#define XRT_NO_FILE_ASYNC
	#endif
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
	#ifndef XRT_NO_XWEB
		#define XRT_NO_XWEB
	#endif
	#ifndef XRT_NO_XWS
		#define XRT_NO_XWS
	#endif
#endif
#if defined(XRT_NO_XHTTPD) && !defined(XRT_NO_XWEB)
	#define XRT_NO_XWEB
#endif
#if defined(XRT_NO_QUEUE)
	#ifndef XRT_NO_QUEUE_WAIT
		#define XRT_NO_QUEUE_WAIT
	#endif
#endif
#if defined(XRT_NO_STACK) || defined(XRT_NO_AVLTREE) || defined(XRT_NO_QUEUE) || defined(XRT_NO_QUEUE_WAIT) || defined(XRT_NO_VALUE)
	#ifndef XRT_NO_TYPED_SPECIAL
		#define XRT_NO_TYPED_SPECIAL
	#endif
#endif
#if defined(XRT_NO_FILE)
	#ifndef XRT_NO_FILE_ASYNC
		#define XRT_NO_FILE_ASYNC
	#endif
#endif
#if defined(XRT_NO_JSON)
	#ifndef XRT_NO_XSON
		#define XRT_NO_XSON
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
#if defined(XRT_NO_XURL) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWEB) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_XURL ignored because XHTTP/XHTTPD/XWEB/XWS require XURL."
	#else
		XRT_CUT_WARN("XRT_NO_XURL ignored because XHTTP/XHTTPD/XWEB/XWS require XURL.")
	#endif
	#undef XRT_NO_XURL
#endif
#if defined(XRT_NO_HTTP_UTIL) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWEB) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_HTTP_UTIL ignored because XHTTP/XHTTPD/XWEB/XWS require HTTP_UTIL."
	#else
		XRT_CUT_WARN("XRT_NO_HTTP_UTIL ignored because XHTTP/XHTTPD/XWEB/XWS require HTTP_UTIL.")
	#endif
	#undef XRT_NO_HTTP_UTIL
#endif
#if defined(XRT_NO_XCODEC) && (!defined(XRT_NO_XHTTP) || !defined(XRT_NO_XHTTPD) || !defined(XRT_NO_XWEB) || !defined(XRT_NO_XWS))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_XCODEC ignored because XHTTP/XHTTPD/XWEB/XWS require XCODEC."
	#else
		XRT_CUT_WARN("XRT_NO_XCODEC ignored because XHTTP/XHTTPD/XWEB/XWS require XCODEC.")
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
#if defined(XRT_NO_FILE) && (!defined(XRT_NO_JSON) || !defined(XRT_NO_NETTLS) || !defined(XRT_NO_XWEB))
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_FILE ignored because JSON/NETTLS/XWEB require FILE."
	#else
		XRT_CUT_WARN("XRT_NO_FILE ignored because JSON/NETTLS/XWEB require FILE.")
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
#if defined(XRT_NO_TIME) && !defined(XRT_NO_LOGGER)
	#if defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
		#warning "XRT_NO_TIME ignored because LOGGER requires TIME."
	#else
		XRT_CUT_WARN("XRT_NO_TIME ignored because LOGGER requires TIME.")
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
	#define XRT_XTIME_DEFINED
	
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
	#ifndef XRT_RAND_OBJ_DEFINED
	#define XRT_RAND_OBJ_DEFINED
	typedef struct {
		xrand rand32;
		xrand rand64_low;
		xrand rand64_high;
	} xrtRandObj;
	#endif
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
		uint32 iScopeDepth;
		uint32 iReserved;
		uint64 iCurrentBytes;
		uint64 iPeakBytes;
		uint64 iResetCount;
	} xrtTempArenaState;
	typedef struct {
		xrtThreadData* pThreadData;
		xrtTempArenaBlock* pCurrent;
		xrtTempArenaBlock* pSpill;
		uint32 iUsed;
		uint32 iDepth;
		uint64 iCurrentBytes;
		bool bActive;
	} xrtTempScope;
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
	typedef struct {
		xrtThreadData* pThreadData;
		const char* sPrevFile;
		uint32 iPrevLine;
	} xrtMemDebugSiteScope;
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
		xrtTempArenaState tHostTemp;
		bool bTempArenaSwapped;
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
		#ifdef XRT_MEM_DEBUG
			const char* sMemDebugFile;
			uint32 iMemDebugLine;
		#endif
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
		// 内存查找
		XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
	#endif
	
	// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
	XXAPI size_t u16len(u16str sText);
	// 获取 UTF-32 字符串长度
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

	// 原子保留引用，成功时返回递增后的引用计数，非法状态返回 -1
	XXAPI int32 xrtAtomicRefRetain(volatile int32* pCount);

	// 原子释放引用，成功时返回递减后的引用计数，非法状态返回 -1
	XXAPI int32 xrtAtomicRefRelease(volatile int32* pCount);
	
	// 申请无需主动释放的临时内存
	XXAPI ptr xrtTempMemory(size_t iSize);
	
	// 释放所有临时内存
	XXAPI void xrtFreeTempMemory();
	// 建立可嵌套的临时内存作用域
	XXAPI xrtTempScope xrtTempScopeBegin();
	// 结束临时内存作用域，仅回收该作用域内的分配
	XXAPI void xrtTempScopeEnd(xrtTempScope* pScope);
	// 复制字符串到父作用域并结束当前作用域
	XXAPI str xrtTempScopeEndString(xrtTempScope* pScope, str sText, size_t iSize);
	// 将需要 xrtFree 的字符串移入当前临时内存区
	XXAPI str xrtTempStringTake(str sText, size_t iSize);
	// 启用内存遥测
	XXAPI void xrtMemTelemetryEnable(bool bEnable);
	// 判断内存遥测是否启用
	XXAPI bool xrtMemTelemetryIsEnabled();
	// 重置内存遥测
	XXAPI void xrtMemTelemetryReset();
	// 获取内存遥测快照
	XXAPI void xrtMemTelemetryGetSnapshot(xrtMemTelemetrySnapshot* pOut);
	
	#ifdef XRT_MEM_DEBUG
		// 启用内存调试
		XXAPI void xrtMemDebugEnable(bool bEnable);
		// 判断内存调试是否启用
		XXAPI bool xrtMemDebugIsEnabled();
		// 重置内存调试
		XXAPI void xrtMemDebugReset();
		// 导出内存调试文本
		XXAPI bool xrtMemDebugDumpText(str sPath);
		// 导出内存调试 JSON
		XXAPI bool xrtMemDebugDumpJson(str sPath);
		// 分配调试
		XXAPI ptr xrtMallocDbg(size_t iSize, const char* sFile, uint32 iLine);
		// 分配调试
		XXAPI ptr xrtCallocDbg(size_t iNum, size_t iSize, const char* sFile, uint32 iLine);
		// 重新分配调试
		XXAPI ptr xrtReallocDbg(ptr pMem, size_t iSize, const char* sFile, uint32 iLine);
		// 释放调试
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
	// 设置错误 u 16
	XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree);
	// 设置错误 u 32
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
	// 原子操作与对象所有权辅助函数
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		/* Linux TCC x64 lacks __sync_* builtins, so use the underlying x86_64 atomics directly. */
		static inline long __xrtAtomicTccLinuxX64CompareExchangeLong(volatile long* pValue, long iExchange, long iComparand)
		{
			long iPrev;
			__asm__ volatile (
				"lock; cmpxchgq %2, %1"
				: "=a"(iPrev), "+m"(*pValue)
				: "r"(iExchange), "0"(iComparand)
				: "cc", "memory"
			);
			return iPrev;
		}
		// 交换 long 值并返回旧值
		static inline long __xrtAtomicTccLinuxX64ExchangeLong(volatile long* pValue, long iValue)
		{
			__asm__ volatile (
				"xchgq %0, %1"
				: "+r"(iValue), "+m"(*pValue)
				:
				: "memory"
			);
			return iValue;
		}
		// 对 long 值做原子加法并返回新值
		static inline long __xrtAtomicTccLinuxX64AddFetchLong(volatile long* pValue, long iDelta)
		{
			long iPrev = iDelta;
			__asm__ volatile (
				"lock; xaddq %0, %1"
				: "+r"(iPrev), "+m"(*pValue)
				:
				: "cc", "memory"
			);
			return iPrev + iDelta;
		}
		// 比较并交换 uint32 值，返回交换前的旧值
		static inline uint32 __xrtAtomicTccLinuxX64CompareExchangeU32(volatile uint32* pValue, uint32 iExchange, uint32 iComparand)
		{
			uint32 iPrev;
			__asm__ volatile (
				"lock; cmpxchgl %2, %1"
				: "=a"(iPrev), "+m"(*pValue)
				: "r"(iExchange), "0"(iComparand)
				: "cc", "memory"
			);
			return iPrev;
		}
		// 交换 uint32 值并返回旧值
		static inline uint32 __xrtAtomicTccLinuxX64ExchangeU32(volatile uint32* pValue, uint32 iValue)
		{
			__asm__ volatile (
				"xchgl %0, %1"
				: "+r"(iValue), "+m"(*pValue)
				:
				: "memory"
			);
			return iValue;
		}
		// 比较并交换 int64 值，返回交换前的旧值
		static inline int64 __xrtAtomicTccLinuxX64CompareExchange64(volatile int64* pValue, int64 iExchange, int64 iComparand)
		{
			int64 iPrev;
			__asm__ volatile (
				"lock; cmpxchgq %2, %1"
				: "=a"(iPrev), "+m"(*pValue)
				: "r"(iExchange), "0"(iComparand)
				: "cc", "memory"
			);
			return iPrev;
		}
		// 交换 int64 值并返回旧值
		static inline int64 __xrtAtomicTccLinuxX64Exchange64(volatile int64* pValue, int64 iValue)
		{
			__asm__ volatile (
				"xchgq %0, %1"
				: "+r"(iValue), "+m"(*pValue)
				:
				: "memory"
			);
			return iValue;
		}
		// 对 int64 值做原子加法并返回新值
		static inline int64 __xrtAtomicTccLinuxX64AddFetch64(volatile int64* pValue, int64 iDelta)
		{
			int64 iPrev = iDelta;
			__asm__ volatile (
				"lock; xaddq %0, %1"
				: "+r"(iPrev), "+m"(*pValue)
				:
				: "cc", "memory"
			);
			return iPrev + iDelta;
		}
	#endif
	// 比较并交换 32 位整数，返回交换前的旧值
	static inline long __xrtAtomicCompareExchange32(volatile long* pValue, long iExchange, long iComparand)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64CompareExchangeLong(pValue, iExchange, iComparand);
		#elif defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
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
	// 交换 32 位整数并返回旧值
	static inline long __xrtAtomicExchange32(volatile long* pValue, long iValue)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64ExchangeLong(pValue, iValue);
		#elif defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
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
			long iPrev;
			do {
				iPrev = __xrtAtomicCompareExchange32(pValue, 0, 0);
			} while ( __xrtAtomicCompareExchange32(pValue, iValue, iPrev) != iPrev );
			return iPrev;
		#endif
	}
	/* Keep dedicated 32-bit atomics for uint32 fields. LP64 builds make long-based helpers 64-bit wide. */
	static inline uint32 __xrtAtomicCompareExchangeU32(volatile uint32* pValue, uint32 iExchange, uint32 iComparand)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64CompareExchangeU32(pValue, iExchange, iComparand);
		#elif defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			return (uint32)__xrtAtomicCompareExchange32((volatile long*)pValue, (long)iExchange, (long)iComparand);
		#elif defined(_WIN32) || defined(_WIN64)
			return (uint32)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iExchange, (LONG)iComparand);
		#else
			return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
		#endif
	}
	// 原子读取 uint32 值
	static inline uint32 __xrtAtomicLoadU32(const volatile uint32* pValue)
	{
		return __xrtAtomicCompareExchangeU32((volatile uint32*)pValue, 0u, 0u);
	}
	// 原子写入 uint32 值
	static inline void __xrtAtomicStoreU32(volatile uint32* pValue, uint32 iValue)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			(void)__xrtAtomicTccLinuxX64ExchangeU32(pValue, iValue);
		#elif defined(__TINYC__) && defined(_WIN32) && !defined(_WIN64)
			(void)__xrtAtomicExchange32((volatile long*)pValue, (long)iValue);
		#elif defined(_WIN32) || defined(_WIN64)
			(void)InterlockedExchange((volatile LONG*)pValue, (LONG)iValue);
		#else
			uint32 iPrev;
			do {
				iPrev = __xrtAtomicLoadU32(pValue);
			} while ( __xrtAtomicCompareExchangeU32(pValue, iValue, iPrev) != iPrev );
		#endif
	}
	// 对 32 位整数做原子加法并返回新值
	static inline long __xrtAtomicAddFetch32(volatile long* pValue, long iDelta)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64AddFetchLong(pValue, iDelta);
		#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
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
	// 对 int64 值做原子加法并返回新值
	static inline int64 __xrtAtomicAddFetch64(volatile int64* pValue, int64 iDelta)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64AddFetch64(pValue, iDelta);
		#elif defined(__TINYC__) && defined(_WIN32)
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
	// 比较并交换 int64 值，返回交换前的旧值
	static inline int64 __xrtAtomicCompareExchange64(volatile int64* pValue, int64 iExchange, int64 iComparand)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			return __xrtAtomicTccLinuxX64CompareExchange64(pValue, iExchange, iComparand);
		#elif defined(__TINYC__) && defined(_WIN32)
			return (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iExchange, (LONG64)iComparand);
		#elif defined(_WIN32) || defined(_WIN64)
			return (int64)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iExchange, (LONG64)iComparand);
		#else
			return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
		#endif
	}
	// 原子读取 int64 值
	static inline int64 __xrtAtomicLoad64(const volatile int64* pValue)
	{
		return __xrtAtomicCompareExchange64((volatile int64*)pValue, 0, 0);
	}
	// 原子写入 int64 值
	static inline void __xrtAtomicStore64(volatile int64* pValue, int64 iValue)
	{
		#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
			(void)__xrtAtomicTccLinuxX64Exchange64(pValue, iValue);
		#elif defined(__TINYC__) && defined(_WIN32)
			int64 iPrev;
			do {
				iPrev = __xrtAtomicLoad64(pValue);
			} while ( __xrtAtomicCompareExchange64(pValue, iValue, iPrev) != iPrev );
		#elif defined(_WIN32) || defined(_WIN64)
			(void)InterlockedExchange64((volatile LONG64*)pValue, (LONG64)iValue);
		#else
			int64 iPrev;
			do {
				iPrev = __xrtAtomicLoad64(pValue);
			} while ( __xrtAtomicCompareExchange64(pValue, iValue, iPrev) != iPrev );
		#endif
	}
	// 所有权锁使用的原子比较交换
	static inline long __xrtOwnerAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
	{
		return __xrtAtomicCompareExchange32(pValue, iExchange, iComparand);
	}
	// 所有权锁使用的原子写入
	static inline void __xrtOwnerAtomicStore(volatile long* pValue, long iValue)
	{
		__xrtAtomicExchange32(pValue, iValue);
	}
	// 抢占共享所有权自旋锁
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
	// 释放共享所有权自旋锁
	static inline void __xrtOwnerSpinUnlock(volatile long* pLock)
	{
		__xrtOwnerAtomicStore(pLock, 0);
	}
	// 获取所有权系统使用的当前线程 id
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
	// 激活共享所有权状态
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
	// 返回共享对象尚未发布时的统一错误文本
	static inline str __xrtOwnerSharedPendingError(void)
	{
		return (str)"shared object is not published yet; cross-thread access is not allowed before publish.";
	}
	// 锁定所有权
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
	// 解锁所有权
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
	// 开始可变访问
	static inline bool xrtOwnerBeginMutable(xrtOwnerInfo* pOwner, const void* sError)
	{
		return xrtOwnerLock(pOwner, sError);
	}
	// 结束可变访问
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
	
	// 读取环境变量并返回需要由 xrtFree 释放的 UTF-8 副本
	XXAPI str xrtEnvGet(str sName);

	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
#ifndef XRT_NO_SUBPROCESS
	#define XPROC_STATE_FAILED		-1
	#define XPROC_STATE_INIT		0
	#define XPROC_STATE_RUNNING		1
	#define XPROC_STATE_EXITED		2
	#define XPROC_STATE_CLOSED		3
	#define XPROC_TARGET_EXEC		1
	#define XPROC_TARGET_SHELL		2
	#define XPROC_STDIO_INHERIT		0
	#define XPROC_STDIO_PIPE		1
	#define XPROC_STDIO_NULL		2
	#define XPROC_STREAM_NONE		0
	#define XPROC_STREAM_STDOUT		1
	#define XPROC_STREAM_STDERR		2
	#define XPROC_STREAM_TERMINAL	3
	#define XPROC_EXIT_NONE			0
	#define XPROC_EXIT_NORMAL		1
	#define XPROC_EXIT_SIGNAL		2
	#define XPROC_EXIT_SPAWN_FAILED	3
	#define XPROC_EXIT_WAIT_FAILED	4
	#define XPROC_STAGE_NONE		0
	#define XPROC_STAGE_SPAWN		1
	#define XPROC_STAGE_WORKDIR		2
	#define XPROC_STAGE_ENV			3
	#define XPROC_STAGE_STDIN		4
	#define XPROC_STAGE_STDOUT		5
	#define XPROC_STAGE_STDERR		6
	#define XPROC_STAGE_EXEC		7
	#define XPROC_STAGE_WAIT		8
	#define XPROC_STOP_NONE			0
	#define XPROC_STOP_INTERRUPT	1
	#define XPROC_STOP_TERMINATE	2
	#define XPROC_STOP_KILL			3
	#define XPROC_STOP_KILL_TREE	4
	#define XPROC_EVENT_NONE		0
	#define XPROC_EVENT_START		1
	#define XPROC_EVENT_OUTPUT		2
	#define XPROC_EVENT_EXIT		3
	typedef struct xprocess_struct xprocess;
	typedef struct {
		int iMode;
		bool bCapture;
	} xprocessstdio;
	typedef struct {
		int iKind;
		int iExitCode;
		int iSignal;
		int iStage;
		int iOsError;
		int iStopReason;
		bool bTimedOut;
		bool bCancelled;
	} xprocessexitinfo;
	typedef struct {
		uint64 iBaseOffset;
		uint64 iNextOffset;
		uint64 iStreamEndOffset;
		bool bDone;
	} xprocessreadinfo;
	typedef struct {
		uint64 iSeq;
		int iKind;
		int iStream;
		uint64 iOffset;
		uint64 iSize;
		uint64 tTimeMs;
		xprocessexitinfo ExitInfo;
	} xprocessevent;
	typedef struct {
		uint64 iBaseSeq;
		uint64 iNextSeq;
		uint64 iEventEndSeq;
		bool bDone;
	} xprocesseventreadinfo;
	typedef struct {
		void (*OnStart)(xprocess* pProcess, ptr pUserData);
		void (*OnStdout)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
		void (*OnStderr)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
		void (*OnExit)(xprocess* pProcess, const xprocessexitinfo* pExitInfo, ptr pUserData);
	} xprocessevents;
	typedef struct {
		int iTargetKind;
		str sProgram;
		str* arrArgs;
		uint32 iArgCount;
		str sCommand;
		str sWorkDir;
		str* arrEnv;
		uint32 iEnvCount;
		bool bInheritEnv;
		bool bUseTerminal;
		bool bNewConsole;
		bool bMergeStderr;
		bool bCreateProcessGroup;
		bool bHideWindow;
		uint32 iTerminalCols;
		uint32 iTerminalRows;
		uint32 iReadChunkSize;
		size_t iMaxCaptureBytes;
		uint32 iMaxEventCount;
		xprocessstdio Stdin;
		xprocessstdio Stdout;
		xprocessstdio Stderr;
		const xprocessevents* pEvents;
		ptr pUserData;
	} xprocessconfig;
	typedef struct {
		xprocessexitinfo ExitInfo;
		int iExitCode;
		ptr pStdout;
		size_t iStdoutSize;
		uint64 iStdoutBaseOffset;
		ptr pStderr;
		size_t iStderrSize;
		uint64 iStderrBaseOffset;
		bool bStdoutTruncated;
		bool bStderrTruncated;
		uint64 iDurationMs;
	} xprocessresult;
	typedef struct {
		xprocessresult* arrResults;
		uint32 iResultCount;
		uint32 iCompletedCount;
		int iExitCode;
		int iFailedIndex;
		ptr pStdout;
		size_t iStdoutSize;
		ptr pStderr;
		size_t iStderrSize;
		bool bSuccess;
		bool bTimedOut;
		bool bCancelled;
		bool bStdoutTruncated;
		bool bStderrTruncated;
		uint64 iDurationMs;
	} xprocesspipelineresult;
	// 初始化进程配置
	XXAPI void xrtProcessConfigInit(xprocessconfig* pConfig);
	// 启动进程
	XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig);
	// 销毁进程
	XXAPI void xrtProcessDestroy(xprocess* pProcess);
	// 获取进程状态
	XXAPI int xrtProcessState(xprocess* pProcess);
	// 判断进程是否仍在运行
	XXAPI bool xrtProcessIsRunning(xprocess* pProcess);
	// 获取进程退出码
	XXAPI int xrtProcessExitCode(xprocess* pProcess);
	// 获取结构化退出信息
	XXAPI bool xrtProcessGetExitInfo(xprocess* pProcess, xprocessexitinfo* pInfo);
	// 当前平台是否支持 terminal 模式
	XXAPI bool xrtProcessTerminalSupported(void);
	// 写入进程标准输入
	XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize);
	// 写入进程标准输入文本
	XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize);
	// 关闭进程标准输入
	XXAPI bool xrtProcessCloseStdin(xprocess* pProcess);
	// 等待进程
	XXAPI bool xrtProcessWait(xprocess* pProcess);
	// 限时等待进程结束
	XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs);
	// 向进程发送中断信号
	XXAPI bool xrtProcessInterrupt(xprocess* pProcess);
	// 尽量温和地要求进程退出
	XXAPI bool xrtProcessTerminate(xprocess* pProcess);
	// 强制结束当前进程
	XXAPI bool xrtProcessKill(xprocess* pProcess);
	// 强制结束整个进程树
	XXAPI bool xrtProcessKillTree(xprocess* pProcess);
	// 调整 terminal 尺寸
	XXAPI bool xrtProcessResizeTerminal(xprocess* pProcess, uint32 iCols, uint32 iRows);
	// 获取进程标准输出快照（返回新分配的内存，调用方负责 xrtFree）
	XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize);
	// 获取进程标准错误快照（返回新分配的内存，调用方负责 xrtFree）
	XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize);
	// 按偏移读取标准输出增量（返回新分配的内存，调用方负责 xrtFree）
	XXAPI ptr xrtProcessReadStdoutSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo);
	// 按偏移读取标准错误增量（返回新分配的内存，调用方负责 xrtFree）
	XXAPI ptr xrtProcessReadStderrSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo);
	// 按序号读取事件增量（返回新分配的事件数组，调用方负责 xrtFree）
	XXAPI xprocessevent* xrtProcessReadEventsSince(xprocess* pProcess, uint64 iSeq, uint32 iMaxCount, uint32* piCount, xprocesseventreadinfo* pInfo);
	// 执行进程并捕获输出
	XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs);
	// 释放进程结果
	XXAPI void xrtProcessResultUnit(xprocessresult* pResult);
	XXAPI bool xrtProcessResultCopy(const xprocessresult* pSrc, xprocessresult* pDst);
	XXAPI bool xrtProcessResultSuccess(const xprocessresult* pResult);
	XXAPI bool xrtProcessRun(const xprocessconfig* pConfig, const void* pInput, size_t iInputSize, xprocessresult* pResult, uint32 iTimeoutMs);
	XXAPI bool xrtProcessPipelineRun(const xprocessconfig* arrConfigs, uint32 iConfigCount, xprocesspipelineresult* pResult, uint32 iTimeoutMs);
	XXAPI void xrtProcessPipelineResultUnit(xprocesspipelineresult* pResult);
#endif
	
	
	
	/* ------------------------------------ Math 函数库 ------------------------------------ */
	
	// 约等于模式常量
	#define XRT_APPROX_DIFF     0   // 差值模式
	#define XRT_APPROX_PERCENT  1   // 百分比模式
	
	// 静态初始化随机数生成器的推荐值
	#define XRAND_INITIALIZER  { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL }
	
	// 初始化随机数生成器
	XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq);
	
	// 初始化当前线程默认随机数生成器
	XXAPI void xrtRandSeedThread(uint64 seed, uint64 seq);
	
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
	
	// 创建独立随机数对象（需使用 xrtRandDestroy 释放）
	XXAPI ptr xrtRandCreate(uint64 seed, uint64 seq);
	
	// 释放独立随机数对象
	XXAPI int64 xrtRandDestroy(ptr pRand);
	
	// 重置独立随机数对象种子
	XXAPI void xrtRandSeedObj(ptr pRand, uint64 seed, uint64 seq);
	
	// 使用独立随机数对象生成 32 位随机数
	XXAPI uint32 xrtRand32Obj(ptr pRand);
	
	// 使用独立随机数对象生成 64 位随机数
	XXAPI uint64 xrtRand64Obj(ptr pRand);
	
	// 使用独立随机数对象生成范围随机数
	XXAPI int xrtRandRangeObj(ptr pRand, int min, int max);
	
	// 整数约等于（使用 xCore 配置）
	XXAPI bool xrtIntApprox(int64 a, int64 b);
	
	// 浮点数约等于（使用 xCore 配置）
	XXAPI bool xrtNumApprox(double a, double b);
	
	#ifndef XRT_MATH_PI
		#define XRT_MATH_PI 3.14159265358979323846264338327950288
	#endif
	#ifndef XRT_MATH_TAU
		#define XRT_MATH_TAU (2.0 * XRT_MATH_PI)
	#endif
	#ifndef XRT_MATH_E
		#define XRT_MATH_E 2.71828182845904523536028747135266250
	#endif
	#ifndef XRT_MATH_INF
		#define XRT_MATH_INF HUGE_VAL
	#endif
	#ifndef XRT_MATH_NAN
		#ifdef NAN
			#define XRT_MATH_NAN NAN
		#else
			#define XRT_MATH_NAN (HUGE_VAL - HUGE_VAL)
		#endif
	#endif
	
	XXAPI double xrtMathMin(double a, double b);
	XXAPI double xrtMathMax(double a, double b);
	XXAPI double xrtMathClamp(double value, double minValue, double maxValue);
	XXAPI int xrtMathSign(double value);
	XXAPI double xrtMathTrunc(double value);
	XXAPI double xrtMathFract(double value);
	XXAPI double xrtMathMod(double value, double divisor);
	XXAPI double xrtMathRad(double degrees);
	XXAPI double xrtMathDeg(double radians);
	XXAPI bool xrtMathIsNaN(double value);
	XXAPI bool xrtMathIsInf(double value);
	XXAPI bool xrtMathIsFinite(double value);
	XXAPI double xrtMathLog2(double value);
	XXAPI double xrtMathExp2(double value);
	XXAPI double xrtMathLog1p(double value);
	XXAPI double xrtMathExpm1(double value);
	XXAPI double xrtMathCbrt(double value);
	XXAPI double xrtMathHypot(double x, double y);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（ 需使用 xrtFree 释放 ）
	XXAPI str xrtCopyStr(str sText, size_t iSize);
	// 连接两个字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtStrConcat(str sLeft, str sRight);
	// 连接多个字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtStrJoin(uint32 iCount, ...);
	// 复制字符串 u 16
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	// 复制字符串 u 32
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	// 复制内存
	XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
	
	// 比较字符串
	XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
	XXAPI size_t xrtStrByteLen(str sText, size_t iSize);
	XXAPI size_t xrtStrUtf8CharSize(str sText, size_t iSize, size_t iPos);
	XXAPI size_t xrtStrCharLen(str sText, size_t iSize);
	XXAPI size_t xrtStrCharToBytePos(str sText, size_t iSize, size_t iCharIndex);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	XXAPI int64 xrtStrIndexOf(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	XXAPI int64 xrtStrLastIndexOf(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	XXAPI int64 xrtStrCount(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	// 查找子串首次出现位置（ 未找到返回 0 ）
	XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
	
	// 通配符匹配（ * 匹配任意字符序列，? 匹配单个UTF-8字符，bCase 为 TRUE 时忽略大小写 ）
	XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	// 从右侧裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	// 裁剪
	XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
	
	// 字符串格式化（ 需使用 xrtFree 释放 ）
	XXAPI str xrtFormat(const void* sFormat, ...);
	
	// 字符串替换（ 需使用 xrtFree 释放 ）
	XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize);
	XXAPI str xrtStrSlice(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize);
	XXAPI str xrtStrLeft(str sText, size_t iSize, int64 iCount, size_t* iRetSize);
	XXAPI str xrtStrRight(str sText, size_t iSize, int64 iCount, size_t* iRetSize);
	XXAPI str xrtStrMid(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize);
	XXAPI str xrtStrCharAt(str sText, size_t iSize, int64 iIndex, size_t* iRetSize);
	XXAPI int xrtStrByteAt(str sText, size_t iSize, int64 iIndex);
	XXAPI str xrtStrRepeat(str sText, size_t iSize, int64 iCount, size_t* iRetSize);
	XXAPI bool xrtStrIsEmpty(str sText, size_t iSize);
	XXAPI bool xrtStrIsBlank(str sText, size_t iSize);
	XXAPI str xrtStrRemove(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase, size_t* iRetSize);
	XXAPI str xrtStrRemoveAt(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize);
	XXAPI str xrtStrInsert(str sText, size_t iSize, int64 iStart, str sPartText, size_t iPartSize, size_t* iRetSize);
	XXAPI str xrtStrPadLeft(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize);
	XXAPI str xrtStrPadRight(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize);
	XXAPI str xrtStrPadCenter(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize);
	XXAPI str xrtStrReverse(str sText, size_t iSize, size_t* iRetSize);
	XXAPI str* xrtStrSplitLines(str sText, size_t iSize, size_t* iRetSize);
	
	// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
	XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize);
	
	// 生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
	
	// 使用独立随机数对象生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStrObj(ptr pRand, str sTemplate, size_t iSize, size_t iLen);
	
	// HEX 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
	// HEX 编码原始字节（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncodeBytes(ptr pMem, size_t iSize);
	
	// HEX 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtHexDecode(str pText, size_t iSize);
	
	// Base64 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
	// Base64 编码原始字节（ 需使用 xrtFree 释放 ）
	XXAPI str xrtBase64EncodeBytes(ptr pMem, size_t iSize, str sTable);
	
	// Base64 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
	
	// 整数格式化（格式符: , 千分位 | 0N 前导零 | + 正号 | x/X 十六进制 | o 八进制 | b 二进制）（ 需使用 xrtFree 释放 ）
	XXAPI str xrtIntFormat(int64 value, str format);
	
	// 无符号整数格式化（格式符同 xrtIntFormat）（ 需使用 xrtFree 释放 ）
	XXAPI str xrtUIntFormat(uint64 value, str format);
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
	
	// URL 解码（ 需使用 xrtFree 释放 ）
	
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
	XXAPI str xrtPathSep();
	XXAPI str xrtPathNormalize(str sPath, size_t iSize);
	XXAPI str xrtPathAbs(str sPath, size_t iSize);
	XXAPI str xrtPathRel(str sBase, str sTarget);
	XXAPI str xrtPathCwd();
	XXAPI bool xrtPathSetCwd(str sPath);
	XXAPI str xrtPathHome();
	XXAPI str xrtPathTemp();
	XXAPI str xrtPathAppDir();
	XXAPI str xrtPathWithExt(str sPath, str sExt);
	XXAPI str xrtPathWithName(str sPath, str sNameExt);
	
	
	
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
	
	// Try to parse a string into xtime. Returns TRUE only when parsing succeeds.
	XXAPI bool xrtTryStrToTime(str sTime, size_t iSize, xtime* pOut);
	
	// 时间单位累加
	XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
	
	// 单位时间差计算
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
	#ifndef XRT_NO_LOGGER
	/* ---------- 日志系统 ---------- */
	typedef enum {
		XLOG_TRACE = 0,
		XLOG_DEBUG = 1,
		XLOG_INFO = 2,
		XLOG_WARN = 3,
		XLOG_ERROR = 4,
		XLOG_FATAL = 5,
		XLOG_OFF = 6
	} xloglevel;
	typedef enum {
		XLOG_FORMAT_TEXT = 0,
		XLOG_FORMAT_SIMPLE = 1,
		XLOG_FORMAT_JSON = 2
	} xlogformat;
	typedef struct xlogger xlogger;
	typedef struct xlogappender xlogappender;
	typedef struct xlogevent {
		xtime iTime;
		xloglevel iLevel;
		const char* sLogger;
		const char* sFile;
		uint32 iLine;
		const char* sFunc;
		uint64 iThreadId;
		const char* sMessage;
	} xlogevent;
	typedef void (*xlogcustomproc)(const xlogevent* pEvent, ptr pUserData);
	XXAPI xlogger* xlogCreate(str sName);
	XXAPI void xlogDestroy(xlogger* pLogger);
	XXAPI xlogger* xlogDefault();
	XXAPI void xlogSetDefault(xlogger* pLogger);
	XXAPI void xlogSetLevel(xlogger* pLogger, xloglevel iLevel);
	XXAPI xloglevel xlogGetLevel(xlogger* pLogger);
	XXAPI xlogappender* xlogAddConsole(xlogger* pLogger, xloglevel iMinLevel, bool bColor);
	XXAPI xlogappender* xlogAddFile(xlogger* pLogger, str sPath, xloglevel iMinLevel);
	XXAPI xlogappender* xlogAddRollingFile(xlogger* pLogger, str sPath, uint64 iMaxSize, uint32 iMaxBackup, xloglevel iMinLevel);
	XXAPI xlogappender* xlogAddCustom(xlogger* pLogger, str sName, xloglevel iMinLevel, xlogcustomproc Proc, ptr pUserData);
	XXAPI void xlogAppenderSetLevel(xlogappender* pAppender, xloglevel iMinLevel);
	XXAPI void xlogAppenderSetFormat(xlogappender* pAppender, xlogformat iFormat);
	XXAPI void xlogAppenderSetColor(xlogappender* pAppender, bool bColor);
	XXAPI void xlogWrite(xlogger* pLogger, xloglevel iLevel, const char* sFile, uint32 iLine, const char* sFunc, const char* sFmt, ...);
	XXAPI void xlogWriteV(xlogger* pLogger, xloglevel iLevel, const char* sFile, uint32 iLine, const char* sFunc, const char* sFmt, va_list args);
	XXAPI void xlogFlush(xlogger* pLogger);
	XXAPI str xlogLevelName(xloglevel iLevel);
	#define xloggerTrace(pLogger, ...)	xlogWrite((pLogger), XLOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xloggerDebug(pLogger, ...)	xlogWrite((pLogger), XLOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xloggerInfo(pLogger, ...)	xlogWrite((pLogger), XLOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xloggerWarn(pLogger, ...)	xlogWrite((pLogger), XLOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xloggerError(pLogger, ...)	xlogWrite((pLogger), XLOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xloggerFatal(pLogger, ...)	xlogWrite((pLogger), XLOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogTrace(...)				xlogWrite(xlogDefault(), XLOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogDebug(...)				xlogWrite(xlogDefault(), XLOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogInfo(...)				xlogWrite(xlogDefault(), XLOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogWarn(...)				xlogWrite(xlogDefault(), XLOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogError(...)				xlogWrite(xlogDefault(), XLOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#define xlogFatal(...)				xlogWrite(xlogDefault(), XLOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)
	#endif
	
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
	
	// Try to parse a formatted string into xtime.
	XXAPI bool xrtTryTimeParse(str sTime, str sFormat, xtime* pOut);
	
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
	XXAPI bool xrtFileRename(str sSrc, str sDst, bool bReWrite);
	XXAPI bool xrtFileReplace(str sSrc, str sDst);
	XXAPI bool xrtFileTouch(str sPath);
	XXAPI bool xrtFileWriteAllAtomic(str sPath, str sText, size_t iSize, int iCharset);
	
	// 删除文件
	XXAPI bool xrtFileDelete(str sPath);
	
	// 扫描文件夹 ( 返回文件数量 )
	typedef int (*xrtDirScanProc)(str sPath, size_t iSize, int bDir, ptr pData, ptr Param);
	typedef int (*xrtDirScanExProc)(str sDir, size_t iDirSize, str sName, size_t iNameSize, str sPath, size_t iPathSize, int bDir, ptr pData, ptr Param);
	XXAPI int xrtDirScanEx(str sPath, int bRecu, xrtDirScanExProc pProc, ptr Param);
	XXAPI int xrtDirScan(str sPath, int bRecu, xrtDirScanProc pProc, ptr Param);
	XXAPI int64 xrtDirCount(str sPath, bool bRecu, int iMode);
	XXAPI int64 xrtDirSize(str sPath, bool bRecu);
	XXAPI bool xrtDirIsEmpty(str sPath);
	
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
	XXAPI bool xrtDirIsRoot(str sPath);
	XXAPI bool xrtDirClean(str sPath);
	XXAPI bool xrtDirEnsureEmpty(str sPath);
	#if !defined(XRT_NO_NETWORK)
		#define XAFILE_F_READ			0x0001u
		#define XAFILE_F_WRITE			0x0002u
		#define XAFILE_F_CREATE		0x0004u
		#define XAFILE_F_TRUNCATE		0x0008u
		#define XAFILE_SHARE_READ		0x0001u
		#define XAFILE_SHARE_WRITE		0x0002u
		#define XAFILE_SHARE_DELETE	0x0004u
		typedef struct xasyncfile_struct xasyncfile;
		typedef struct {
			uint32 iFlags;
			uint32 iShareFlags;
			str sPath;
		} xasyncfileconfig;
		typedef struct {
			ptr pData;
			size_t iSize;
			uint64 iOffset;
			bool bEOF;
		} xasyncfilebuf;
		typedef struct {
			uint64 iValue;
			uint64 iOffset;
		} xasyncfileio;
		// 异步初始化文件配置
		XXAPI void xrtAsyncFileConfigInit(xasyncfileconfig* pConfig);
		// 异步打开文件
		XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig);
		// 异步关闭文件
		XXAPI void xrtAsyncFileClose(xasyncfile* pFile);
		// 异步销毁文件缓冲区
		XXAPI void xrtAsyncFileBufDestroy(xasyncfilebuf* pBuf);
		// 分离异步文件缓冲区数据并销毁外层对象，返回数据由调用方使用 xrtFree 释放
		XXAPI ptr xrtAsyncFileBufDetach(xasyncfilebuf* pBuf, size_t* pSize, uint64* pOffset, bool* pEOF);
		// 异步销毁文件 io
		XXAPI void xrtAsyncFileIoDestroy(xasyncfileio* pInfo);
		
	#endif
	
	
	
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
			DWORD iOwnerThreadId;				// 当前持有锁的线程ID（用于保持非递归 mutex 语义）
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
			CRITICAL_SECTION objStateLock;		// Windows：内部状态锁
			CONDITION_VARIABLE objReadCond;		// Windows：读者等待队列
			CONDITION_VARIABLE objWriteCond;	// Windows：写者等待队列
		#else
			pthread_mutex_t objStateLock;		// POSIX：内部状态锁
			pthread_cond_t objReadCond;			// POSIX：读者等待队列
			pthread_cond_t objWriteCond;		// POSIX：写者等待队列
		#endif
		uint32 iReaderCount;					// 当前持有的读锁数量
		uint32 iWaitingReaderCount;			// 当前等待的读者数量
		uint32 iWaitingWriterCount;			// 当前等待的写者数量（含升级者）
		bool bWriterLocked;					// 是否已有写者持锁
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
	
	// 写锁降级为读锁（原子保留锁状态）
	XXAPI void xrtRWLockDowngrade(xrwlock pRWLock);
	
	// 读锁升级为写锁（原子转换，返回 FALSE 表示锁状态无效）
	XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock);
	/* ------------------------------------ Queue 队列库 ------------------------------------ */
	#ifndef XRT_NO_QUEUE
		// 无锁队列基础类型与配置
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
		// 初始化单生产者单消费者队列
		XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg);
		// 释放单生产者单消费者队列内部资源
		XXAPI void xrtSPSCQUnit(xspscq pQueue);
		// 创建单生产者单消费者队列
		XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg);
		// 销毁单生产者单消费者队列
		XXAPI void xrtSPSCQDestroy(xspscq pQueue);
		// 尝试向单生产者单消费者队列压入元素
		XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem);
		// 尝试从单生产者单消费者队列弹出元素
		XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem);
		// 获取单生产者单消费者队列的近似元素数量
		XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue);
		// 关闭单生产者单消费者队列写入端
		XXAPI void xrtSPSCQClose(xspscq pQueue);
		// 排空单生产者单消费者队列中的剩余元素
		XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		// 重置单生产者单消费者队列状态
		XXAPI bool xrtSPSCQReset(xspscq pQueue);
		// 初始化多生产者单消费者队列
		XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg);
		// 释放多生产者单消费者队列内部资源
		XXAPI void xrtMPSCQUnit(xmpscq pQueue);
		// 创建多生产者单消费者队列
		XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg);
		// 销毁多生产者单消费者队列
		XXAPI void xrtMPSCQDestroy(xmpscq pQueue);
		// 尝试向多生产者单消费者队列压入元素
		XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem);
		// 尝试从多生产者单消费者队列弹出元素
		XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem);
		// 批量向多生产者单消费者队列压入元素
		XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount);
		// 批量从多生产者单消费者队列弹出元素
		XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap);
		// 获取多生产者单消费者队列的近似元素数量
		XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue);
		// 关闭多生产者单消费者队列写入端
		XXAPI void xrtMPSCQClose(xmpscq pQueue);
		// 排空多生产者单消费者队列中的剩余元素
		XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		// 重置多生产者单消费者队列状态
		XXAPI bool xrtMPSCQReset(xmpscq pQueue);
		// 初始化多生产者多消费者队列
		XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg);
		// 释放多生产者多消费者队列内部资源
		XXAPI void xrtMPMCQUnit(xmpmcq pQueue);
		// 创建多生产者多消费者队列
		XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg);
		// 销毁多生产者多消费者队列
		XXAPI void xrtMPMCQDestroy(xmpmcq pQueue);
		// 尝试向多生产者多消费者队列压入元素
		XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem);
		// 尝试从多生产者多消费者队列弹出元素
		XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem);
		// 批量向多生产者多消费者队列压入元素
		XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount);
		// 批量从多生产者多消费者队列弹出元素
		XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap);
		// 获取多生产者多消费者队列的近似元素数量
		XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue);
		// 关闭多生产者多消费者队列写入端
		XXAPI void xrtMPMCQClose(xmpmcq pQueue);
		// 排空多生产者多消费者队列中的剩余元素
		XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData);
		// 重置多生产者多消费者队列状态
		XXAPI bool xrtMPMCQReset(xmpmcq pQueue);
		// 判断队列是否已关闭
		XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue);
		// 判断队列是否已经排空
		XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue);
		#ifndef XRT_NO_QUEUE_WAIT
			typedef struct xmpscqwait_struct
			{
				xmpscq_struct tQueue;
				xsem hItems;
				xmutex hPopLock;
				volatile long iWaiters;
			} xmpscqwait_struct, *xmpscqwait;
			// 初始化可等待的多生产者单消费者队列
			XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg);
			// 释放可等待的多生产者单消费者队列内部资源
			XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue);
			// 创建可等待的多生产者单消费者队列
			XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg);
			// 销毁可等待的多生产者单消费者队列
			XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue);
			// 尝试向可等待的多生产者单消费者队列压入元素
			XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem);
			// 尝试从可等待的多生产者单消费者队列弹出元素
			XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem);
			// 阻塞等待并弹出可等待队列中的元素
			XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem);
			// 限时等待并弹出可等待队列中的元素
			XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs);
			// 获取可等待队列的近似元素数量
			XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue);
			// 关闭可等待队列写入端并唤醒等待者
			XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue);
		#endif
		#ifndef XRT_NO_QUEUE_WAIT
			typedef struct xchannel_struct
			{
				xmpscqwait_struct tQueue;
			} xchannel_struct, *xchannel;
			// 初始化 channel
			XXAPI bool xrtChannelInit(xchannel pChannel, uint32 iCapacity);
			// 释放 channel 内部资源
			XXAPI void xrtChannelUnit(xchannel pChannel);
			// 创建 channel
			XXAPI xchannel xrtChannelCreate(uint32 iCapacity);
			// 销毁 channel
			XXAPI void xrtChannelDestroy(xchannel pChannel);
			// 向 channel 发送一个指针值
			XXAPI xqueue_result xrtChannelSend(xchannel pChannel, ptr pItem);
			// 从 channel 尝试接收一个指针值
			XXAPI xqueue_result xrtChannelTryRecv(xchannel pChannel, ptr* ppItem);
			// 从 channel 阻塞接收一个指针值
			XXAPI xqueue_result xrtChannelRecv(xchannel pChannel, ptr* ppItem);
			// 从 channel 限时接收一个指针值
			XXAPI xqueue_result xrtChannelRecvTimeout(xchannel pChannel, ptr* ppItem, uint32 iTimeoutMs);
			// 关闭 channel
			XXAPI void xrtChannelClose(xchannel pChannel);
			// 获取 channel 中近似元素数量
			XXAPI uint32 xrtChannelCount(xchannel pChannel);
			// 判断 channel 是否已经关闭
			XXAPI bool xrtChannelIsClosed(xchannel pChannel);
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
	// 协程 backend 分层/风格：production backend 允许 inline-asm / Windows Fiber 实现
	#define XRT_CO_BACKEND_TIER_PRODUCTION   2
	#define XRT_CO_BACKEND_STYLE_INLINE_ASM  2
	#define XRT_CO_BACKEND_STYLE_FIBER       3
	
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
		ptr __pStack;           // 自管栈内存（仅 inline asm backend 使用）
		ptr __pStackMem;        // 自管栈保留区起始地址（含 guard page）
		size_t __iStackAllocSize; // 自管栈保留区总大小
		size_t __iStackGuardSize; // 自管栈 guard page 大小
		__xrt_co_ctx __tCtx;    // 上下文（汇编/ucontext 后端使用）
		xrtTempArenaState __tTemp; // 协程独立临时内存区，随上下文切换
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
	// 使用默认 seed 计算 32 位哈希值
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
	// 使用默认 seed 计算 64 位哈希值
	XXAPI uint64 xrtHash64(ptr key, size_t len);
	// 使用 Micro 变体计算 64 位哈希值
	XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
	// 使用默认 seed 计算 Micro 变体 64 位哈希值
	XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
	// 使用 Nano 变体计算 64 位哈希值
	XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
	// 使用默认 seed 计算 Nano 变体 64 位哈希值
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
	// 初始化 SHA256
	XXAPI void xrtSHA256Init(xsha256_ctx *pCtx);
	// 更新 SHA256
	XXAPI void xrtSHA256Update(xsha256_ctx *pCtx, const ptr pData, size_t iLen);
	// 结束 SHA256 计算并输出摘要
	XXAPI void xrtSHA256Final(xsha256_ctx *pCtx, uint8 *pOut);
	
	// SHA-1 哈希 (用于 WebSocket 握手)
	XXAPI void xrtSHA1(const ptr pData, size_t iLen, uint8 *pOut);
	// 初始化 SHA1
	XXAPI void xrtSHA1Init(xsha1_ctx *pCtx);
	// 更新 SHA1
	XXAPI void xrtSHA1Update(xsha1_ctx *pCtx, const ptr pData, size_t iLen);
	// 结束 SHA1 计算并输出摘要
	XXAPI void xrtSHA1Final(xsha1_ctx *pCtx, uint8 *pOut);
	
	// SHA-384 哈希 (基于 SHA-512, 截取 48 字节)
	XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8 *pOut);
	// 初始化 SHA384 上下文
	XXAPI void xrtSHA384Init(xsha512_ctx *pCtx);
	// 结束 SHA384 计算并输出摘要
	XXAPI void xrtSHA384Final(xsha512_ctx *pCtx, uint8 *pOut);
	
	// SHA-512 哈希
	XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8 *pOut);
	// 初始化 SHA512
	XXAPI void xrtSHA512Init(xsha512_ctx *pCtx);
	// 更新 SHA512
	XXAPI void xrtSHA512Update(xsha512_ctx *pCtx, const ptr pData, size_t iLen);
	// 结束 SHA512 计算并输出摘要
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
	// 解密并校验 ChaCha20-Poly1305 数据
	XXAPI bool xrtChaCha20Poly1305Decrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// AES-128-GCM AEAD 加密/解密
	// 加密: pOut 需要 iLen + 16 字节空间 (密文 + 16字节tag)
	// 解密: iLen 包含 16 字节 tag，返回 false 表示验证失败
	XXAPI bool xrtAES128GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	// 解密并校验 AES-128-GCM 数据
	XXAPI bool xrtAES128GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// AES-256-GCM AEAD 加密/解密
	// 加密: pOut 需要 iLen + 16 字节空间 (密文 + 16字节tag)
	// 解密: iLen 包含 16 字节 tag，返回 false 表示验证失败
	XXAPI bool xrtAES256GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	// 解密并校验 AES-256-GCM 数据
	XXAPI bool xrtAES256GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
	
	// X25519 密钥交换 (RFC 7748)
	XXAPI void xrtX25519Keypair(uint8 *pPrivKey, uint8 *pPubKey);              // 生成密钥对 (各 32 字节)
	// 计算 X25519 共享密钥
	XXAPI void xrtX25519SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (32 字节)
	
	// X448 密钥交换 (RFC 7748)
	XXAPI void xrtX448Keypair(uint8 *pPrivKey, uint8 *pPubKey);                // 生成密钥对 (各 56 字节)
	// 计算 X448 共享密钥
	XXAPI void xrtX448SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);    // 计算共享密钥 (56 字节)
	
	// ECDH secp256r1 (P-256) 密钥交换 (TLS 1.2 ECDHE)
	XXAPI void xrtECDHSecp256r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);       // 生成密钥对 (私钥 32 字节, 公钥 65 字节: 0x04||X||Y)
	// 计算 secp256r1 共享密钥
	XXAPI void xrtECDHSecp256r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (32 字节)
	
	// ECDH secp384r1 (P-384) 密钥交换
	XXAPI void xrtECDHSecp384r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);       // 生成密钥对 (私钥 48 字节, 公钥 97 字节: 0x04||X||Y)
	// 计算 secp384r1 共享密钥
	XXAPI void xrtECDHSecp384r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);  // 计算共享密钥 (48 字节)
	
	// Ed25519 签名 (RFC 8032)
	XXAPI void xrtEd25519Keypair(uint8 *pSeed, uint8 *pPubKey);               // 生成种子和公钥 (32 + 32 字节)
	// 从 Ed25519 种子导出公钥
	XXAPI void xrtEd25519PublicKey(uint8 *pPubKey, const uint8 *pSeed);       // 从 32 字节 seed 导出公钥
	// 生成 Ed25519 签名
	XXAPI bool xrtEd25519Sign(uint8 *pSig, const uint8 *pMsg, size_t iMsgLen, const uint8 *pSeed); // 生成 64 字节签名
	// ECDSA / Ed25519 签名验证 (用于 TLS 证书验证)
	XXAPI bool xrtEd25519Verify(const uint8 *pMsg, size_t iMsgLen, const uint8 *pSig, const uint8 *pPubKey);
	// 校验 ECDSA 签名
	XXAPI bool xrtECDSAVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pPubKey, size_t iPubKeyLen);
	
	// RSA 模幂运算 + RSA-PSS 签名验证 (axTLS bignum, BSD License)
	XXAPI int  xrtRSAModPow(const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz, const uint8 *pMsg, size_t iMsgSz, uint8 *pOut, size_t iOutSz);
	// 校验 RSA-PSS 签名
	XXAPI bool xrtRSAPSSVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
	
	// RSA PKCS#1 v1.5 签名验证 (TLS 1.2 证书链)
	XXAPI bool xrtRSAPKCS1Verify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
	
	// HKDF 密钥派生 (RFC 5869, 基于 SHA-256)
	XXAPI void xrtHKDFExtract(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
	// 使用 SHA-256 扩展 HKDF 输出密钥材料
	XXAPI void xrtHKDFExpand(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen);
	
	// HKDF-SHA384 密钥派生 (RFC 5869, 基于 SHA-384)
	XXAPI void xrtHKDFExtract_SHA384(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
	// 使用 SHA-384 扩展 HKDF 输出密钥材料
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
		const char* sCrlFile;
		const void* pCertData;
		size_t iCertDataLen;
		const void* pKeyData;
		size_t iKeyDataLen;
		const void* pCaData;
		size_t iCaDataLen;
		const void* pCrlData;
		size_t iCrlDataLen;
		const char* sHostName;
		bool bVerifyPeer;
		void (*OnSNI)(xtlssession *pSession, const char *sHostName, ptr pUserData);
		ptr pSNIUserData;
		bool bAllowTLS12Ed25519;
		uint16 iMaxVersion;
		const xtlsresume* pResume;
		volatile long iDataLock;
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
		typedef struct xrt_regex_match xregexmatch;
		typedef struct xrt_regex_builder xregexbuilder;
		typedef struct xrt_regex_set xregexset;
		typedef struct xrt_regex_set_builder xregexsetbuilder;
		
		// 单模式
		XXAPI xregex* xrtRegexCreate(const char* sPatternNt);
		XXAPI xregex* xrtRegexCreateEx(const char* sPattern, size_t iPatternSize, xregexflags iFlags);
		// 根据 Builder 创建正则对象
		XXAPI int xrtRegexCreateFromBuilder(xregex** ppRegex, const xregexbuilder* pBuilder, const xregexalloc* pAlloc);
		// 销毁正则
		XXAPI void xrtRegexDestroy(xregex* pRegex);
		// 获取正则错误 msg
		XXAPI const char* xrtRegexGetErrorMsg(const xregex* pRegex);
		// 获取正则错误 pos
		XXAPI size_t xrtRegexGetErrorPos(const xregex* pRegex);
		// 判断文本是否匹配正则
		XXAPI int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);
		// 查找正则
		XXAPI int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);
		// 获取正则捕获结果
		XXAPI int xrtRegexCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32 iCaptureCount);
		// 获取正则捕获结果及命中标记
		XXAPI int xrtRegexWhichCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
		// 从指定位置判断文本是否匹配正则
		XXAPI int xrtRegexIsMatchAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos);
		// 查找正则
		XXAPI int xrtRegexFindAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutSpan);
		// 从指定位置获取正则捕获结果
		XXAPI int xrtRegexCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32 iCaptureCount);
		// 从指定位置获取正则捕获结果及命中标记
		XXAPI int xrtRegexWhichCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
		// 获取正则捕获组数量
		XXAPI uint32 xrtRegexCaptureCount(const xregex* pRegex);
		// 获取正则 capture 名称
		XXAPI const char* xrtRegexCaptureName(const xregex* pRegex, uint32 iCaptureIndex, size_t* pOutNameSize);
		XXAPI int xrtRegexFullMatch(xregex* pRegex, const char* sText, size_t iTextSize);
		XXAPI xregexmatch* xrtRegexFindMatch(xregex* pRegex, const char* sText, size_t iTextSize);
		XXAPI xregexmatch* xrtRegexFindMatchAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos);
		XXAPI void xrtRegexMatchDestroy(xregexmatch* pMatch);
		XXAPI int xrtRegexMatchOk(const xregexmatch* pMatch);
		XXAPI int64 xrtRegexMatchByteStart(const xregexmatch* pMatch);
		XXAPI int64 xrtRegexMatchByteEnd(const xregexmatch* pMatch);
		XXAPI int64 xrtRegexMatchStart(xregexmatch* pMatch);
		XXAPI int64 xrtRegexMatchEnd(xregexmatch* pMatch);
		XXAPI const char* xrtRegexMatchText(xregexmatch* pMatch);
		XXAPI str xrtRegexMatchTextCopy(xregexmatch* pMatch);
		
		// Builder
		XXAPI int xrtRegexBuilderCreate(xregexbuilder** ppBuilder, const char* sPattern, size_t iPatternSize, const xregexalloc* pAlloc);
		// 销毁正则 Builder
		XXAPI void xrtRegexBuilderDestroy(xregexbuilder* pBuilder);
		// 设置正则 Builder 标志
		XXAPI void xrtRegexBuilderSetFlags(xregexbuilder* pBuilder, xregexflags iFlags);
		
		// 克隆
		XXAPI int xrtRegexClone(xregex** ppOut, const xregex* pRegex, const xregexalloc* pAlloc);
		
		// 多模式
		XXAPI int xrtRegexSetBuilderCreate(xregexsetbuilder** ppBuilder, const xregexalloc* pAlloc);
		// 销毁正则集合 Builder
		XXAPI void xrtRegexSetBuilderDestroy(xregexsetbuilder* pBuilder);
		// 向正则集合 Builder 添加一个模式
		XXAPI int xrtRegexSetBuilderAdd(xregexsetbuilder* pBuilder, const xregex* pRegex);
		// 直接根据模式列表创建正则集合
		XXAPI xregexset* xrtRegexSetCreate(const char* const* arrPatternsNt, size_t iPatternCount);
		// 直接根据模式列表和标志创建正则集合
		XXAPI xregexset* xrtRegexSetCreateEx(const char* const* arrPatternsNt, size_t iPatternCount, xregexflags iFlags);
		// 根据 Builder 创建正则集合
		XXAPI int xrtRegexSetCreateFromBuilder(xregexset** ppSet, const xregexsetbuilder* pBuilder, const xregexalloc* pAlloc);
		// 销毁正则集合
		XXAPI void xrtRegexSetDestroy(xregexset* pSet);
		// 获取正则集合错误信息
		XXAPI const char* xrtRegexSetGetErrorMsg(const xregexset* pSet);
		// 获取正则集合错误位置
		XXAPI size_t xrtRegexSetGetErrorPos(const xregexset* pSet);
		// 判断文本是否命中任意正则集合项
		XXAPI int xrtRegexSetIsMatch(xregexset* pSet, const char* sText, size_t iTextSize);
		// 获取命中的正则集合项索引
		XXAPI int xrtRegexSetMatches(xregexset* pSet, const char* sText, size_t iTextSize, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
		// 从指定位置判断文本是否命中任意正则集合项
		XXAPI int xrtRegexSetIsMatchAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);
		// 获取第一个命中的正则集合项索引，未命中返回 -1
		XXAPI int xrtRegexSetFirst(xregexset* pSet, const char* sText, size_t iTextSize);
		// 从指定位置获取第一个命中的正则集合项索引，未命中返回 -1
		XXAPI int xrtRegexSetFirstAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);
		// 从指定位置获取命中的正则集合项索引
		XXAPI int xrtRegexSetMatchesAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
		// 克隆正则集合
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
		uint32 iRecvQueueLimit;
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
		#ifdef XRT_MEM_DEBUG
			uint64 iDebugOwnerThreadId;
		#endif
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
		uint32 iHeaderCap;
		uint32 iStatusCode;
		int64_t iContentLength;
		size_t iHeadBytes;
		char sMethod[XCODEC_HTTP1_TOKEN_CAP];
		char sTarget[XCODEC_HTTP1_TARGET_CAP];
		char sVersion[XCODEC_HTTP1_TOKEN_CAP];
		char sReason[XCODEC_HTTP1_REASON_CAP];
		xcodechttp1header arrHeaders[XCODEC_HTTP1_MAX_HEADERS];
		xcodechttp1header* pHeaders;
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
		void (*OnTimer)(ptr pOwner, xnetstream* pStream);
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
		void (*pfnFreeValue)(ptr pValue);
		void (*pfnFreeError)(ptr pError);
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
		typedef struct xrt_http_client xhttpclient;
		typedef struct xrt_http_header {
			char sName[XHTTP_HEADER_NAME_CAP];
			char sValue[XHTTP_HEADER_VALUE_CAP];
		} xhttpheader;
		typedef struct xrt_http_url {
			bool bHttps;
			uint16 iPort;
			char sHost[XHTTP_HOST_CAP];
			char sPath[XHTTP_PATH_CAP];
		} xhttpurl;
		typedef enum xrt_http_phase {
			XHTTP_PHASE_NONE = 0,
			XHTTP_PHASE_PREPARE,
			XHTTP_PHASE_CONNECT,
			XHTTP_PHASE_WRITE,
			XHTTP_PHASE_RESPONSE_HEADERS,
			XHTTP_PHASE_RESPONSE_BODY,
			XHTTP_PHASE_COMPLETE
		} xhttp_phase;
		typedef enum xrt_http_error_code {
			XHTTP_ERROR_NONE = 0,
			XHTTP_ERROR_INVALID_ARGUMENT,
			XHTTP_ERROR_OUT_OF_MEMORY,
			XHTTP_ERROR_INVALID_URL,
			XHTTP_ERROR_CONNECT,
			XHTTP_ERROR_WRITE,
			XHTTP_ERROR_PROTOCOL,
			XHTTP_ERROR_CALLBACK_ABORT,
			XHTTP_ERROR_TIMEOUT_TOTAL,
			XHTTP_ERROR_TIMEOUT_IDLE,
			XHTTP_ERROR_CANCELLED,
			XHTTP_ERROR_CONNECTION_CLOSED,
			XHTTP_ERROR_TRANSPORT
		} xhttp_error_code;
		typedef struct xrt_http_diagnostics {
			xhttp_error_code eError;
			xhttp_phase ePhase;
			xnet_result eTransportStatus;
			int iSystemError;
			bool bReusedConnection;
			uint64 iStartedMs;
			uint64 iConnectedMs;
			uint64 iRequestSentMs;
			uint64 iFirstByteMs;
			uint64 iHeadersMs;
			uint64 iCompletedMs;
			uint64 iConnectDurationMs;
			uint64 iTimeToFirstByteMs;
			uint64 iTransferDurationMs;
			uint64 iTotalDurationMs;
			uint64 iRequestBytes;
			uint64 iResponseBodyBytes;
		} xhttpdiagnostics;
		typedef struct xrt_http_request {
			char sMethod[XHTTP_METHOD_CAP];
			char sURL[XHTTP_URL_CAP];
			xhttpurl tURL;
			xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
			xhttpheader* pHeaders;
			uint32 iHeaderCount;
			uint32 iHeaderCap;
			char* pBody;
			size_t iBodyLen;
			uint32 iTimeoutMs;
			uint32 iIdleTimeoutMs;
			bool bVerifyPeer;
			xnetproxy* pProxy;
			xhttpdiagnostics* pDiagnostics;
		} xhttprequest;
		typedef struct xrt_http_response {
			uint32 iStatusCode;
			uint32 iFlags;
			uint32 iHeaderCount;
			uint32 iHeaderCap;
			int64_t iContentLength;
			char sVersion[XCODEC_HTTP1_TOKEN_CAP];
			char sReason[XCODEC_HTTP1_REASON_CAP];
			xhttpheader arrHeaders[XHTTP_MAX_HEADERS];
			xhttpheader* pHeaders;
			char* pBody;
			size_t iBodyLen;
			xhttpdiagnostics tDiagnostics;
		} xhttpresponse;
		typedef bool (*xhttpstreamheadersfn)(ptr pUserData, const xhttpresponse* pResponse);
		typedef bool (*xhttpstreambodyfn)(ptr pUserData, const void* pData, size_t iLen);
		typedef struct xrt_http_stream_callbacks {
			ptr pUserData;
			xhttpstreamheadersfn OnHeaders;
			xhttpstreambodyfn OnBody;
		} xhttpstreamcallbacks;
		
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
		#define XHTTPD_FILE_CHUNK_SIZE    65536u
		#define XHTTPD_REQ_F_NONE         0x00000000u
		#define XHTTPD_REQ_F_KEEPALIVE    0x00000001u
		#define XHTTPD_REQ_F_CHUNKED      0x00000002u
		#define XHTTPD_REQ_F_UPGRADE      0x00000004u
		#define XHTTPD_REQ_F_STREAMING    0x00000008u
		#define XHTTPD_RESP_F_NONE        0x00000000u
		#define XHTTPD_RESP_F_CLOSE       0x00000001u
		#define XHTTPD_CONN_TIMER_NONE    0u
		#define XHTTPD_CONN_TIMER_HEADER  1u
		#define XHTTPD_CONN_TIMER_BODY    2u
		#define XHTTPD_CONN_TIMER_IDLE    3u
		#define XHTTPD_CONN_TIMER_WRITE   4u
		typedef enum {
			XHTTPD_METHOD_UNKNOWN = 0,
			XHTTPD_METHOD_GET = 1,
			XHTTPD_METHOD_HEAD = 2,
			XHTTPD_METHOD_POST = 3,
			XHTTPD_METHOD_PUT = 4,
			XHTTPD_METHOD_DELETE = 5,
			XHTTPD_METHOD_PATCH = 6,
			XHTTPD_METHOD_OPTIONS = 7
		} xhttpdmethod;
		typedef struct {
			char sName[XHTTPD_HEADER_NAME_CAP];
			char sValue[XHTTPD_HEADER_VALUE_CAP];
		} xhttpdheader;
		typedef struct {
			uint32 iFlags;
			uint32 iHeaderCount;
			uint32 iHeaderCap;
			uint32 iMethod;
			int64_t iContentLength;
			char sMethod[XHTTPD_METHOD_CAP];
			char sTarget[XHTTPD_TARGET_CAP];
			char sPath[XHTTPD_PATH_CAP];
			char sQuery[XHTTPD_QUERY_CAP];
			char sVersion[XHTTPD_VERSION_CAP];
			xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
			xhttpdheader* pHeaders;
			char* pBody;
			size_t iBodyLen;
			size_t iBodyCap;
		} xhttpdrequest;
		typedef struct {
			uint32 iStatusCode;
			uint32 iFlags;
			uint32 iHeaderCount;
			uint32 iHeaderCap;
			char sReason[XHTTPD_REASON_CAP];
			xhttpdheader arrHeaders[XHTTPD_MAX_HEADERS];
			xhttpdheader* pHeaders;
			char* pBody;
			size_t iBodyLen;
			size_t iBodyCap;
		} xhttpdresponse;
		typedef struct {
			xnetaddr tBindAddr;
			uint32 iFlags;
			uint32 iBacklog;
			uint32 iRecvLimit;
			uint32 iBodyLimit;
			uint32 iHeaderLimit;
			uint32 iHeaderBytesLimit;
			uint32 iHeaderTimeoutMs;
			uint32 iBodyTimeoutMs;
			uint32 iIdleTimeoutMs;
			uint32 iWriteTimeoutMs;
			const xtlsconfig* pTlsConfig;
		} xhttpdconfig;
		typedef struct {
			void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
			bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
			xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
			bool (*OnRequestBodyBegin)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
			bool (*OnRequestBody)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, const void* pData, size_t iLen);
			bool (*OnRequestBodyEnd)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
			xfuture* (*OnRequestBodyEndAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
			void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
			void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
		} xhttpdevents;
		
	#endif
	#ifndef XRT_NO_XWEB
		
		/* ============================== xweb ============================== */
		typedef struct xrt_web_server xwebserver;
		typedef struct xrt_web_app xwebapp;
		typedef struct xrt_web_request xwebrequest;
		typedef struct xrt_web_response xwebresponse;
		typedef int xwebaction;
		#define XWEB_NEXT 0
		#define XWEB_DONE 1
		#define XWEB_ASYNC 2
		#define XWEB_ERROR (-1)
		typedef xwebaction (*xwebhandler)(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData);
		typedef bool (*xwebbodybegin)(xwebrequest* pReq, ptr pUserData);
		typedef bool (*xwebbodychunk)(xwebrequest* pReq, const void* pData, size_t iLen, ptr pUserData);
		typedef xwebaction (*xwebbodyend)(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData);
		typedef xfuture* (*xwebbodyendasync)(xwebrequest* pReq, ptr pUserData);
		typedef xwebaction (*xweberrorhandler)(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sMessage, ptr pUserData);
		typedef void (*xwebfree)(ptr pUserData);
		typedef struct {
			xnetaddr tBindAddr;
			uint32 iFlags;
			uint32 iBacklog;
			uint32 iRecvLimit;
			uint32 iBodyLimit;
			uint32 iHeaderLimit;
			uint32 iHeaderBytesLimit;
			uint32 iHeaderTimeoutMs;
			uint32 iBodyTimeoutMs;
			uint32 iIdleTimeoutMs;
			uint32 iWriteTimeoutMs;
			uint32 iMultipartPartLimit;
			uint32 iMultipartPartSizeLimit;
			uint32 iWorkerCount;
			const xtlsconfig* pTlsConfig;
		} xwebconfig;
		typedef struct {
			uint32 iFlags;
			const char* sIndexNames;
			const char* sCacheControl;
			size_t iChunkSize;
		} xwebstaticconfig;
		#define XWEB_METHOD_ANY        0xFFFFFFFFu
		#define XWEB_METHOD_GET        (1u << XHTTPD_METHOD_GET)
		#define XWEB_METHOD_HEAD       (1u << XHTTPD_METHOD_HEAD)
		#define XWEB_METHOD_POST       (1u << XHTTPD_METHOD_POST)
		#define XWEB_METHOD_PUT        (1u << XHTTPD_METHOD_PUT)
		#define XWEB_METHOD_DELETE     (1u << XHTTPD_METHOD_DELETE)
		#define XWEB_METHOD_PATCH      (1u << XHTTPD_METHOD_PATCH)
		#define XWEB_METHOD_OPTIONS    (1u << XHTTPD_METHOD_OPTIONS)
		#define XWEB_STATIC_F_NONE             0x00000000u
		#define XWEB_STATIC_F_ALLOW_DOTFILES   0x00000001u
		#define XWEB_STATIC_F_ALLOW_BACKSLASH  0x00000002u
		
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
	// XNet 地址、配置与数据链基础接口
	
	// 初始化网络 addr 任意
	XXAPI void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort);
	// 解析 IP 与端口到网络地址结构
	XXAPI xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort);
	// 解析网络
	XXAPI xnet_result xrtNetResolve(const char* sHost, xnetaddr* pAddr);
	// 将网络地址转换为可读字符串
	XXAPI const char* xrtNetAddrToStr(const xnetaddr* pAddr);
	
	// 默认配置初始化
	XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg);
	// 初始化网络 listen 配置
	XXAPI void xrtNetListenConfigInit(xnetlistenconfig* pCfg);
	// 初始化网络代理配置
	XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg);
	// 创建网络代理
	XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg);
	// 增加网络代理 ref
	XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy);
	// 释放网络代理
	XXAPI void xrtNetProxyRelease(xnetproxy* pProxy);
	// 初始化网络 connect 配置
	XXAPI void xrtNetConnectConfigInit(xnetconnectconfig* pCfg);
	// 初始化网络数据报配置
	XXAPI void xrtNetDgramConfigInit(xnetdgramconfig* pCfg);
	// XNet 内存上下文与数据链操作
	XXAPI void xrtNetMemConfigInit(xnetmemconfig* pCfg);
	// 初始化网络内存 ctx（ctx 设计为线程归属对象，不应在活跃阶段跨线程共享）
	XXAPI void xrtNetMemCtxInit(xnetmemctx* pCtx, const xnetmemconfig* pCfg);
	// 裁剪网络内存 ctx（仅适合在 ctx 已经静止时调用）
	XXAPI void xrtNetMemCtxTrim(xnetmemctx* pCtx);
	// 释放网络内存 ctx
	XXAPI void xrtNetMemCtxUnit(xnetmemctx* pCtx);
	// 获取网络内存上下文统计信息
	XXAPI void xrtNetMemCtxGetStats(const xnetmemctx* pCtx, xnetmemstats* pStats);
	// 初始化网络 chain 扩展（ctx 需与后续访问线程保持一致）
	XXAPI void xrtNetChainInitEx(xnetchain* pChain, xnetmemctx* pMemCtx);
	// 使用默认内存上下文初始化网络数据链
	XXAPI void xrtNetChainInit(xnetchain* pChain);
	// 清空网络数据链中的全部片段
	XXAPI void xrtNetChainClear(xnetchain* pChain);
	// 复制一段数据并追加到网络数据链末尾
	XXAPI bool xrtNetChainAppendCopy(xnetchain* pChain, const void* pData, size_t iLen);
	// 以引用方式追加一段网络缓冲区
	XXAPI bool xrtNetChainAppendRef(xnetchain* pChain, const xnetbufref* pRef);
	// 统计网络数据链中的总字节数
	XXAPI size_t xrtNetChainBytes(const xnetchain* pChain);
	// 统计网络数据链包含的 span 数量
	XXAPI uint32 xrtNetChainSpanCount(const xnetchain* pChain);
	// 导出网络数据链中的 span 描述
	XXAPI uint32 xrtNetChainGetSpans(const xnetchain* pChain, xnetspan* pOut, uint32 iMaxCount);
	// 预览网络数据链前部内容
	XXAPI size_t xrtNetChainPeek(const xnetchain* pChain, ptr pOut, size_t iLen);
	// 按偏移预览网络数据链内容
	XXAPI size_t xrtNetChainPeekAt(const xnetchain* pChain, size_t iOffset, ptr pOut, size_t iLen);
	// 在网络数据链中查找指定字节
	XXAPI size_t xrtNetChainFindByte(const xnetchain* pChain, uint8 ch, size_t iStartOff);
	// 消费网络数据链前部字节
	XXAPI void xrtNetChainConsume(xnetchain* pChain, size_t iLen);
	#ifndef XRT_NO_XURL
		
		// URL / Query 解析、拷贝、拼装与归一化
		XXAPI xrtstrview xrtStrView(const char* sPtr, size_t iLen);
		// 判断字符串视图是否为空
		XXAPI bool xrtStrViewIsEmpty(xrtstrview tView);
		// 复制字符串视图
		XXAPI bool xrtStrViewCopyTo(xrtstrview tView, char* sOut, size_t iOutCap);
		// 获取 URL 默认端口
		XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme);
		// 判断是否为 URL 安全协议
		XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme);
		// 判断是否为 URL 默认端口
		XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL);
		// 判断是否为 URL 视图协议
		XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme);
		// 判断 URL 视图是否匹配两个协议之一
		XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB);
		// 复制 URL 视图协议
		XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 复制 URL 视图授权段
		XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 复制 URL 视图路径
		XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 复制 URL 视图查询
		XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 复制 URL 视图片段
		XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 解析 URL 授权段
		XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut);
		// 解析 URL 授权段
		XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut);
		// 解析 URL Target 部分
		XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut);
		// 解析 URL Target 部分
		XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut);
		// 解析 URL 视图
		XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut);
		// 解析 URL 视图
		XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut);
		// 复制 URL 视图主机
		XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 复制 URL 视图 Target 部分
		XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 构建 URL 主机头部
		XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap);
		// 构建 URL 主机头部固定长度
		XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap);
		// 规范化 URL 路径
		XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 URL Target 部分
		XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 URL 授权段
		XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 URL
		XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 将相对 URL 解析到输出缓冲区
		XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 将相对 URL 解析为完整 URL
		XXAPI bool xrtUrlResolve(const xrturlview* pBase, const char* sRef, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 获取下一个查询
		XXAPI bool xrtQueryNextN(const char* sQuery, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
		// 获取下一个查询
		XXAPI bool xrtQueryNext(const char* sQuery, size_t* pOffset, xrtquerypair* pOut);
		// 统计查询
		XXAPI size_t xrtQueryCountN(const char* sQuery, size_t iLen);
		// 统计查询
		XXAPI size_t xrtQueryCount(const char* sQuery);
		// 查找查询
		XXAPI bool xrtQueryFindN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, xrtquerypair* pOut);
		// 查找查询
		XXAPI bool xrtQueryFind(const char* sQuery, const char* sKey, xrtquerypair* pOut);
		// 查找并解码查询值到固定缓冲区
		XXAPI bool xrtQueryFindValueToN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 查找并解码查询值到固定缓冲区
		XXAPI bool xrtQueryFindValueTo(const char* sQuery, const char* sKey, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 解析查询
		XXAPI bool xrtQueryParseToN(const char* sQuery, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount);
		// 解析查询
		XXAPI bool xrtQueryParseTo(const char* sQuery, xrtquerypair* pOut, size_t iCap, size_t* pCount);
		// 执行百分号解码并写入输出缓冲区
		XXAPI bool xrtPercentDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bPlusAsSpace);
		// 解析 URL 固定长度
		XXAPI bool xrtUrlParseFixedTo(const char* sURL, const char* sSchemeA, const char* sSchemeB, bool* pSchemeB, char* sHost, size_t iHostCap, uint16* pPort, char* sTarget, size_t iTargetCap);
		// 解析 URL
		XXAPI bool xrtUrlParse(const char* sURL, xurl pOut);
	#endif
	#ifndef XRT_NO_HTTP_UTIL
		
		// HTTP Token、Header、Cookie、参数与 Multipart 工具函数
		XXAPI bool xrtQueryNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
		// 获取下一个 HTTP Token
		XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
		// 获取下一个 HTTP 头部行
		XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
		// 获取下一个 Cookie
		XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
		// 解析 Set-Cookie 头部视图
		XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
		// 获取下一个 HTTP 参数
		XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
		// 获取下一个 Multipart
		XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut);
		// 初始化 HTTP 工具限制配置
		XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits);
		// 应用 Multipart 流配置 limits
		XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits);
		// 校验 HTTP Token
		XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验 HTTP Token
		XXAPI bool xrtHttpTokenValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验 HTTP 参数
		XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验 HTTP 参数
		XXAPI bool xrtHttpParamValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验查询
		XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验查询
		XXAPI bool xrtQueryValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验 Cookie
		XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验 Cookie
		XXAPI bool xrtCookieValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验表单 URL 编码
		XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验表单 URL 编码
		XXAPI bool xrtFormUrlEncodedValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验 HTTP 头部块
		XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits);
		// 校验 HTTP 头部块
		XXAPI bool xrtHttpHeaderBlockValidate(const char* sBlock, const xrthttputillimits* pLimits);
		// 校验 Set-Cookie 文本是否合法
		XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
		// 校验 Set-Cookie 文本是否合法
		XXAPI bool xrtSetCookieValidate(const char* sText, const xrthttputillimits* pLimits);
		// 校验 Multipart
		XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits);
		// 校验 Multipart
		XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits);
		// 判断是否为 HTTP Token
		XXAPI bool xrtHttpIsTokenN(const char* sText, size_t iLen);
		// 判断是否为 HTTP Token
		XXAPI bool xrtHttpIsToken(const char* sText);
		// 解码 HTTP 引用字符串
		XXAPI bool xrtHttpQuotedStringDecodeToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 解码 HTTP 引用字符串
		XXAPI bool xrtHttpQuotedStringDecodeTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 引用字符串
		XXAPI bool xrtHttpQuotedStringBuildToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 引用字符串
		XXAPI bool xrtHttpQuotedStringBuildTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 执行百分号编码并写入输出缓冲区
		XXAPI bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus);
		// 执行百分号编码（需使用 xrtFree 释放）
		XXAPI str xrtPercentEncode(const char* sText, size_t iLen, bool bSpaceAsPlus, size_t* pOutLen);
		// 执行百分号解码（需使用 xrtFree 释放）
		XXAPI str xrtPercentDecode(const char* sText, size_t iLen, bool bPlusAsSpace, size_t* pOutLen);
		// 解码 HTTP 扩展值
		XXAPI bool xrtHttpDecodeExtValueTo(const char* sText, size_t iLen, xrtstrview* pCharset, xrtstrview* pLanguage, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 解码 HTTP 扩展值
		XXAPI bool xrtHttpDecodeExtValue(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 扩展值
		XXAPI bool xrtHttpBuildExtValueTo(const char* sCharset, const char* sLanguage, const char* sText, size_t iTextLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 扩展值
		XXAPI bool xrtHttpBuildExtValue(const char* sCharset, const char* sLanguage, const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 拆分单行 HTTP 头部
		XXAPI bool xrtHttpHeaderSplitLineN(const char* sLine, size_t iLen, xrtheaderpair* pOut);
		// 拆分单行 HTTP 头部
		XXAPI bool xrtHttpHeaderSplitLine(const char* sLine, xrtheaderpair* pOut);
		// 构建 HTTP 头部行
		XXAPI bool xrtHttpHeaderBuildLineTo(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 头部行
		XXAPI bool xrtHttpHeaderBuildLine(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 头部规范化行
		XXAPI bool xrtHttpHeaderBuildCanonicalLineToN(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 头部规范化行
		XXAPI bool xrtHttpHeaderBuildCanonicalLineTo(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 头部块
		XXAPI bool xrtHttpHeaderBuildBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 HTTP 头部规范化块
		XXAPI bool xrtHttpHeaderBuildCanonicalBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 获取下一个 HTTP Token
		XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
		// 获取下一个 HTTP Token
		XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut);
		// 统计 HTTP Token
		XXAPI size_t xrtHttpTokenCountN(const char* sText, size_t iLen);
		// 统计 HTTP Token
		XXAPI size_t xrtHttpTokenCount(const char* sText);
		// 查找 HTTP Token
		XXAPI bool xrtHttpTokenFindN(const char* sText, size_t iLen, const char* sToken, size_t iTokenLen, xrtstrview* pOut);
		// 查找 HTTP Token
		XXAPI bool xrtHttpTokenFind(const char* sText, const char* sToken, xrtstrview* pOut);
		// 解析 HTTP Token
		XXAPI bool xrtHttpTokenParseToN(const char* sText, size_t iLen, xrtstrview* pOut, size_t iCap, size_t* pCount);
		// 解析 HTTP Token
		XXAPI bool xrtHttpTokenParseTo(const char* sText, xrtstrview* pOut, size_t iCap, size_t* pCount);
		// 追加 HTTP Token
		XXAPI bool xrtHttpTokenAppendTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken, size_t iTokenLen);
		// 追加 HTTP Token
		XXAPI bool xrtHttpTokenAppend(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken);
		// 判断是否包含 HTTP 头部 Token
		XXAPI bool xrtHttpHeaderContainsTokenN(const char* sValue, size_t iValueLen, const char* sToken);
		// 判断是否包含 HTTP 头部 Token
		XXAPI bool xrtHttpHeaderContainsToken(const char* sValue, const char* sToken);
		// 查找 HTTP 头部
		XXAPI bool xrtHttpHeaderFindN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut);
		// 查找 HTTP 头部
		XXAPI bool xrtHttpHeaderFind(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut);
		// 统计 HTTP 头部
		XXAPI size_t xrtHttpHeaderCountN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen);
		// 统计 HTTP 头部
		XXAPI size_t xrtHttpHeaderCount(const xrtheaderpair* pHeaders, size_t iCount, const char* sName);
		// 查找 HTTP 头部 nth
		XXAPI bool xrtHttpHeaderFindNthN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pOut);
		// 查找 HTTP 头部 nth
		XXAPI bool xrtHttpHeaderFindNth(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNth, xrtstrview* pOut);
		// 查找 HTTP 头部全部
		XXAPI size_t xrtHttpHeaderFindAllToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, xrtstrview* pOut, size_t iOutCap);
		// 查找 HTTP 头部全部
		XXAPI size_t xrtHttpHeaderFindAllTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut, size_t iOutCap);
		// 规范化 HTTP 头部名称
		XXAPI bool xrtHttpHeaderCanonicalizeNameToN(const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 规范化 HTTP 头部名称
		XXAPI bool xrtHttpHeaderCanonicalizeNameTo(const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 加入 HTTP 头部 values
		XXAPI bool xrtHttpHeaderJoinValuesTo(const xrtstrview* pValues, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 收集同名 HTTP 头部并拼接结果
		XXAPI bool xrtHttpHeaderCollectAndJoinToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 收集同名 HTTP 头部并拼接结果
		XXAPI bool xrtHttpHeaderCollectAndJoinTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 获取下一个 HTTP 头部行
		XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
		// 获取下一个 HTTP 头部行
		XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut);
		// 查找 HTTP 头部行
		XXAPI bool xrtHttpHeaderFindLineN(const char* sBlock, size_t iLen, const char* sName, xrtheaderpair* pOut);
		// 查找 HTTP 头部行
		XXAPI bool xrtHttpHeaderFindLine(const char* sBlock, const char* sName, xrtheaderpair* pOut);
		// 解析 HTTP 头部块
		XXAPI bool xrtHttpHeaderParseBlockToN(const char* sBlock, size_t iLen, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount);
		// 解析 HTTP 头部块
		XXAPI bool xrtHttpHeaderParseBlockTo(const char* sBlock, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount);
		// 追加 HTTP 头部键值对
		XXAPI bool xrtHttpHeaderAppendPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
		// 追加 HTTP 头部键值对
		XXAPI bool xrtHttpHeaderAppendPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue);
		// 设置 HTTP 头部键值对
		XXAPI bool xrtHttpHeaderSetPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
		// 设置 HTTP 头部键值对
		XXAPI bool xrtHttpHeaderSetPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue);
		// 删除 HTTP 头部
		XXAPI size_t xrtHttpHeaderRemoveN(xrtheaderpair* pHeaders, size_t* pCount, const char* sName, size_t iNameLen);
		// 删除 HTTP 头部
		XXAPI size_t xrtHttpHeaderRemove(xrtheaderpair* pHeaders, size_t* pCount, const char* sName);
		// 获取下一个 Cookie
		XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
		// 获取下一个 Cookie
		XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut);
		// 查找 Cookie
		XXAPI bool xrtCookieFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrtcookiepair* pOut);
		// 查找 Cookie
		XXAPI bool xrtCookieFind(const char* sText, const char* sName, xrtcookiepair* pOut);
		// 解析 Cookie
		XXAPI bool xrtCookieParseToN(const char* sText, size_t iLen, xrtcookiepair* pOut, size_t iCap, size_t* pCount);
		// 解析 Cookie
		XXAPI bool xrtCookieParseTo(const char* sText, xrtcookiepair* pOut, size_t iCap, size_t* pCount);
		// 解析 Set-Cookie 视图
		XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
		// 解析 Set-Cookie 视图
		XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut);
		// 解析 set Cookie 行
		XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut);
		// 解析 set Cookie 行
		XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut);
		// 获取下一个 HTTP 参数
		XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
		// 获取下一个 HTTP 参数
		XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut);
		// 统计 HTTP 参数
		XXAPI size_t xrtHttpParamCountN(const char* sText, size_t iLen);
		// 统计 HTTP 参数
		XXAPI size_t xrtHttpParamCount(const char* sText);
		// 查找 HTTP 参数
		XXAPI bool xrtHttpParamFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrthttpparam* pOut);
		// 查找 HTTP 参数
		XXAPI bool xrtHttpParamFind(const char* sText, const char* sName, xrthttpparam* pOut);
		// 追加 HTTP 参数键值对
		XXAPI bool xrtHttpParamAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bQuoteValue);
		// 追加 HTTP 参数键值对
		XXAPI bool xrtHttpParamAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue, bool bHasValue, bool bQuoteValue);
		// 解析 Media Type 视图
		XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut);
		// 解析 Media Type 视图
		XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut);
		// 构建 Media Type 文本
		XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 Media Type 文本
		XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 查找 Media Type 参数
		XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut);
		// 查找 Media Type 参数
		XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut);
		// 解析 Content-Disposition 视图
		XXAPI bool xrtHttpContentDispositionParseN(const char* sText, size_t iLen, xrtcontentdispositionview* pOut);
		// 解析 Content-Disposition 视图
		XXAPI bool xrtHttpContentDispositionParse(const char* sText, xrtcontentdispositionview* pOut);
		// 解码 HTTP content disposition 文件名称
		XXAPI bool xrtHttpContentDispositionDecodeFileNameTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 解码 HTTP content disposition 文件名称
		XXAPI bool xrtHttpContentDispositionDecodeFileName(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 Content-Disposition 文本
		XXAPI bool xrtHttpContentDispositionBuildTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 Content-Disposition 文本
		XXAPI bool xrtHttpContentDispositionBuild(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 追加 Cookie 键值对
		XXAPI bool xrtCookieAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
		// 追加 Cookie 键值对
		XXAPI bool xrtCookieAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue);
		// 构建 Cookie
		XXAPI bool xrtCookieBuildTo(const xrtcookiepair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 追加查询键值对
		XXAPI bool xrtQueryAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, size_t iKeyLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bPlusAsSpace);
		// 追加查询键值对
		XXAPI bool xrtQueryAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, const char* sValue);
		// 构建查询
		XXAPI bool xrtQueryBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 获取下一个表单 URL 编码
		XXAPI bool xrtFormUrlEncodedNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
		// 获取下一个表单 URL 编码
		XXAPI bool xrtFormUrlEncodedNext(const char* sText, size_t* pOffset, xrtquerypair* pOut);
		// 解析表单 URL 编码
		XXAPI bool xrtFormUrlEncodedParseToN(const char* sText, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount);
		// 解析表单 URL 编码
		XXAPI bool xrtFormUrlEncodedParseTo(const char* sText, xrtquerypair* pOut, size_t iCap, size_t* pCount);
		// 解码表单 URL 编码
		XXAPI bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 表单 URL 编码（需使用 xrtFree 释放）
		XXAPI str xrtFormUrlEncode(const char* sText, size_t iLen, size_t* pOutLen);
		// 表单 URL 解码（需使用 xrtFree 释放）
		XXAPI str xrtFormUrlDecode(const char* sText, size_t iLen, size_t* pOutLen);
		// 追加表单 URL 编码 field
		XXAPI bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue);
		// 追加表单 URL 编码 field
		XXAPI bool xrtFormUrlEncodedAppendField(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue);
		// 构建表单 URL 编码
		XXAPI bool xrtFormUrlEncodedBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 Set-Cookie 文本
		XXAPI bool xrtSetCookieBuildTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 set Cookie 行
		XXAPI bool xrtSetCookieBuildLineTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 构建 set Cookie 行
		XXAPI bool xrtSetCookieBuildLine(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 从 Content-Type 中提取 Multipart boundary
		XXAPI bool xrtMultipartBoundaryFromContentTypeN(const char* sValue, size_t iLen, xrtstrview* pOut);
		// 从 Content-Type 中提取 Multipart boundary
		XXAPI bool xrtMultipartBoundaryFromContentType(const char* sValue, xrtstrview* pOut);
		// 根据 boundary 构建 Multipart Content-Type
		XXAPI bool xrtMultipartBuildContentTypeTo(const char* sBoundary, size_t iBoundaryLen, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 根据 boundary 构建 Multipart Content-Type
		XXAPI bool xrtMultipartBuildContentType(const char* sBoundary, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 获取下一个 Multipart
		XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut);
		// 获取下一个 Multipart
		XXAPI bool xrtMultipartNext(const char* sBody, const char* sBoundary, size_t* pOffset, xrtmultipartpartview* pOut);
		// 解析 Multipart
		XXAPI bool xrtMultipartParseToN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount);
		// 解析 Multipart
		XXAPI bool xrtMultipartParseTo(const char* sBody, const char* sBoundary, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount);
		// 解码 Multipart 文件名称
		XXAPI bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 解码 Multipart 文件名称
		XXAPI bool xrtMultipartDecodeFileName(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen);
		// 初始化 Multipart 流配置
		XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig);
		// 初始化 Multipart 流
		XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig);
		// 释放 Multipart 流
		XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream);
		// 重置 Multipart 流
		XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream);
		// 向 Multipart 流解析器喂入数据
		XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen);
		// 完成 Multipart 流
		XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream);
		// 获取 Multipart 流错误
		XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream);
		// 获取下一个 Multipart 流
		XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent);
		// 追加 Multipart 表单字段 part
		XXAPI bool xrtMultipartAppendFieldPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen);
		// 追加 Multipart 表单字段 part
		XXAPI bool xrtMultipartAppendFieldPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sValue);
		// 追加 Multipart 原始数据 part
		XXAPI bool xrtMultipartAppendRawPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 原始数据 part
		XXAPI bool xrtMultipartAppendRawPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 文件 part 扩展
		XXAPI bool xrtMultipartAppendFilePartExtTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sFileNameExt, size_t iFileNameExtLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 文件 part 扩展
		XXAPI bool xrtMultipartAppendFilePartExt(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sFileNameExt, const char* sContentType, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 文件 part
		XXAPI bool xrtMultipartAppendFilePartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 文件 part
		XXAPI bool xrtMultipartAppendFilePart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sContentType, const char* pBody, size_t iBodyLen);
		// 追加 Multipart 结束边界
		XXAPI bool xrtMultipartAppendFinishTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen);
		// 追加 Multipart 结束边界
		XXAPI bool xrtMultipartAppendFinish(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary);
	#endif
	#ifndef XRT_NO_XCODEC
		
		// 通用编解码器与 HTTP/1 / WebSocket 解析接口
		XXAPI void xrtCodecParserInit(xcodecparser* pParser, const xcodecparserops* pOps, ptr pCtx);
		// 按当前编解码策略解析输入数据
		XXAPI xcodecstatus xrtCodecParserParse(const xcodecparser* pParser, const xnetchain* pInput, xcodecframe* pFrame);
		// 重置通用编解码器解析状态
		XXAPI void xrtCodecParserReset(const xcodecparser* pParser);
		// 初始化编解码帧描述
		XXAPI void xrtCodecFrameInit(xcodecframe* pFrame);
		// 从输入链中预览当前帧内容
		XXAPI size_t xrtCodecFramePeek(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen);
		// 从输入链中消费当前帧内容
		XXAPI void xrtCodecFrameConsume(xnetchain* pInput, const xcodecframe* pFrame);
		// 初始化编解码器行配置
		XXAPI void xrtCodecLineConfigInit(xcodeclinecodec* pCodec);
		// 设置编解码器行 delimiter
		XXAPI bool xrtCodecLineSetDelimiter(xcodeclinecodec* pCodec, const void* pDelimiter, uint32 iDelimiterLen);
		// 解析编解码器行
		XXAPI xcodecstatus xrtCodecLineParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
		// 重置编解码器行
		XXAPI void xrtCodecLineReset(ptr pCtx);
		// 获取行协议编解码器操作表
		XXAPI const xcodecparserops* xrtCodecLineOps(void);
		// 初始化编解码器 length 配置
		XXAPI void xrtCodecLengthConfigInit(xcodeclengthcodec* pCodec);
		// 解析长度前缀帧
		XXAPI xcodecstatus xrtCodecLengthParse(ptr pCtx, const xnetchain* pInput, xcodecframe* pFrame);
		// 重置长度前缀帧解析状态
		XXAPI void xrtCodecLengthReset(ptr pCtx);
		// 获取长度前缀编解码器操作表
		XXAPI const xcodecparserops* xrtCodecLengthOps(void);
		// 获取编解码器 HTTP/1 头部
		XXAPI const char* xrtCodecHttp1GetHeader(const xcodechttp1msg* pMsg, const char* sName);
		// 初始化编解码器 HTTP/1 消息
		XXAPI void xrtCodecHttp1MessageInit(xcodechttp1msg* pMsg);
		XXAPI void xrtCodecHttp1MessageUnit(xcodechttp1msg* pMsg);
		// 获取 HTTP/1 帧正文部分的字节数
		XXAPI size_t xrtCodecHttp1BodyBytes(const xcodecframe* pFrame);
		// 复制编解码器 HTTP/1 正文
		XXAPI size_t xrtCodecHttp1CopyBody(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen);
		// 只解析编解码器 HTTP/1 头部
		XXAPI xcodecstatus xrtCodecHttp1ParseHead(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg);
		// 解析编解码器 HTTP/1
		XXAPI xcodecstatus xrtCodecHttp1Parse(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg);
		// 初始化编解码器 WebSocket frame
		XXAPI void xrtCodecWsFrameInit(xcodecwsframeinfo* pInfo);
		// 解析编解码器 WebSocket frame
		XXAPI xcodecstatus xrtCodecWsParseFrame(const xnetchain* pInput, xcodecframe* pFrame, xcodecwsframeinfo* pInfo);
		// 对 WebSocket 载荷执行掩码还原
		XXAPI void xrtCodecWsUnmask(ptr pData, size_t iLen, const uint8 aMask[4], size_t iStartOffset);
	#endif
	// 网络引擎生命周期与任务投递
	XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);
	// 销毁网络引擎
	XXAPI void xrtNetEngineDestroy(xnetengine* pEngine);
	// 启动网络引擎
	XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine);
	// 停止网络引擎
	XXAPI void xrtNetEngineStop(xnetengine* pEngine);
	// 获取网络引擎工作线程数量
	XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine);
	// 向网络引擎投递立即执行的任务
	XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg);
	// 向网络引擎投递延迟执行的任务
	XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);
	// TLS 会话驱动与恢复
	XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
	// 销毁网络 TLS session
	XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
	// 判断 TLS 会话是否已可收发明文
	XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
	// 推进 TLS 握手状态机
	XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession);
	// 喂入收到的 TLS 密文数据
	XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen);
	// 获取待发送的 TLS 密文字节数
	XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession);
	// 获取待读取的 TLS 明文字节数
	XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession);
	// 预览待发送的 TLS 密文数据
	XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
	// 消费已经发送出去的 TLS 密文数据
	XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen);
	// 写入待加密发送的明文数据
	XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten);
	// 读取已经解密完成的明文数据
	XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
	// 关闭网络 TLS session 队列
	XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession);
	// 导出 TLS 会话恢复信息
	XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession);
	// 销毁网络 TLS resume
	XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume);
	// 判断 TLS 会话是否通过恢复建立
	XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession);
	// 获取 TLS 会话中的 SNI 主机名
	XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession);
	// 设置网络 TLS session 证书
	XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile);
	// 设置网络 TLS session 内存证书
	XXAPI xnet_result xrtNetTlsSessionSetCertData(xtlssession* pSession, const void* pCertData, size_t iCertLen, const void* pKeyData, size_t iKeyLen);
	// 设置 TLS 1.2 是否允许使用 Ed25519
	XXAPI void xrtNetTlsSessionSetAllowTLS12Ed25519(xtlssession* pSession, bool bAllow);
	// TCP 流与监听器操作
	XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
	// 关闭网络流
	XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
	// 创建网络监听器
	XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg,
	const xnetlistenerevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pUserData);
	// 销毁网络监听器
	XXAPI void xrtNetListenerDestroy(xnetlistener* pListener);
	// 启动网络监听器
	XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener);
	// 停止网络监听器
	XXAPI void xrtNetListenerStop(xnetlistener* pListener);
	// 创建网络流
	XXAPI xnetstream* xrtNetStreamCreate(xnetengine* pEngine, const xnetstreamevents* pEvents, ptr pUserData);
	// 销毁网络流
	XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
	// 连接网络流
	XXAPI xnet_result xrtNetStreamConnect(xnetstream* pStream, const xnetconnectconfig* pCfg);
	// 关闭网络流
	XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
	// 发送网络流
	XXAPI xnet_result xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iLen);
	// 发送网络流 vec
	XXAPI xnet_result xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount);
	// 发送网络流 ref
	XXAPI xnet_result xrtNetStreamSendRef(xnetstream* pStream, const xnetbufref* pRef);
	// 读取网络流 pause
	XXAPI void xrtNetStreamPauseRead(xnetstream* pStream);
	// 读取网络流 resume
	XXAPI void xrtNetStreamResumeRead(xnetstream* pStream);
	// 发送网络流 pending
	XXAPI size_t xrtNetStreamPendingSend(const xnetstream* pStream);
	// 获取网络流本地地址
	XXAPI const xnetaddr* xrtNetStreamLocalAddr(const xnetstream* pStream);
	// 获取网络流远端地址
	XXAPI const xnetaddr* xrtNetStreamRemoteAddr(const xnetstream* pStream);
	// 设置网络流 user 数据
	XXAPI void xrtNetStreamSetUserData(xnetstream* pStream, ptr pData);
	// 获取网络流 user 数据
	XXAPI ptr xrtNetStreamGetUserData(xnetstream* pStream);
	// UDP 数据报对象与套接字操作
	XXAPI xnetdgrampkt* xrtNetDgramPacketCreate(const xnetaddr* pFrom, const void* pData, size_t iLen);
	// 销毁网络数据报 packet
	XXAPI void xrtNetDgramPacketDestroy(xnetdgrampkt* pPacket);
	// 获取数据报来源地址
	XXAPI const xnetaddr* xrtNetDgramPacketFrom(const xnetdgrampkt* pPacket);
	// 获取数据报正文长度
	XXAPI size_t xrtNetDgramPacketBytes(const xnetdgrampkt* pPacket);
	// 查看网络数据报 packet
	XXAPI size_t xrtNetDgramPacketPeek(const xnetdgrampkt* pPacket, ptr pOut, size_t iLen);
	// 创建网络数据报
	XXAPI xdgramsock* xrtNetDgramCreate(xnetengine* pEngine, const xnetdgramconfig* pCfg, const xnetdgramevents* pEvents, ptr pUserData);
	// 销毁网络数据报
	XXAPI void xrtNetDgramDestroy(xdgramsock* pSock);
	// 启动网络数据报
	XXAPI xnet_result xrtNetDgramStart(xdgramsock* pSock);
	// 停止网络数据报
	XXAPI void xrtNetDgramStop(xdgramsock* pSock);
	// 发送网络数据报
	XXAPI xnet_result xrtNetDgramSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen);
	// 发送网络数据报 vec
	XXAPI xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount);
	// TCP/UDP 便捷同步接口
XXAPI xnetstream* xrtNetTcpConnect(const char* sHost, uint16 iPort, int64 iTimeoutMs);
XXAPI xnetstream* xrtNetTcpConnectEx(const xnetconnectconfig* pCfg);
XXAPI xnetlistener* xrtNetTcpListen(const char* sHost, uint16 iPort, uint32 iBacklog);
XXAPI xnetlistener* xrtNetTcpListenEx(const xnetlistenconfig* pCfg);
	XXAPI void xrtNetTcpListenerDestroy(xnetlistener* pListener);
	XXAPI xnetstream* xrtNetTcpAccept(xnetlistener* pListener, int64 iTimeoutMs);
	XXAPI uint16 xrtNetTcpListenerPort(const xnetlistener* pListener);
	XXAPI void xrtNetTcpStreamDestroy(xnetstream* pStream);
	XXAPI void xrtNetTcpStreamClose(xnetstream* pStream);
XXAPI size_t xrtNetStreamAvailable(const xnetstream* pStream);
XXAPI size_t xrtNetStreamRead(xnetstream* pStream, void* pOut, size_t iMaxBytes);
XXAPI ptr xrtNetStreamReadBytes(xnetstream* pStream, size_t iMaxBytes, size_t* pOutLen);
XXAPI ptr xrtNetStreamRecvBytes(xnetstream* pStream, size_t iMaxBytes, int64 iTimeoutMs, size_t* pOutLen);
XXAPI str xrtNetStreamReadText(xnetstream* pStream, size_t iMaxBytes);
XXAPI str xrtNetStreamRecvText(xnetstream* pStream, size_t iMaxBytes, int64 iTimeoutMs);
XXAPI bool xrtNetStreamSendBytes(xnetstream* pStream, const void* pData, size_t iLen);
XXAPI bool xrtNetStreamSendText(xnetstream* pStream, const char* sText);
	XXAPI bool xrtNetStreamDrain(xnetstream* pStream, int64 iTimeoutMs);
	XXAPI str xrtNetStreamLocalAddrText(const xnetstream* pStream);
	XXAPI str xrtNetStreamRemoteAddrText(const xnetstream* pStream);
	XXAPI int xrtNetStreamLocalPort(const xnetstream* pStream);
	XXAPI int xrtNetStreamRemotePort(const xnetstream* pStream);
XXAPI xdgramsock* xrtNetUdpBind(const char* sHost, uint16 iPort);
XXAPI xdgramsock* xrtNetUdpBindEx(const xnetdgramconfig* pCfg);
XXAPI void xrtNetUdpDestroy(xdgramsock* pSock);
XXAPI int xrtNetUdpLocalPort(const xdgramsock* pSock);
XXAPI str xrtNetUdpLocalAddrText(const xdgramsock* pSock);
XXAPI bool xrtNetUdpSendBytes(xdgramsock* pSock, const char* sHost, uint16 iPort, const void* pData, size_t iLen);
XXAPI bool xrtNetUdpSendText(xdgramsock* pSock, const char* sHost, uint16 iPort, const char* sText);
XXAPI xnetdgrampkt* xrtNetUdpRecv(xdgramsock* pSock, int64 iTimeoutMs);
XXAPI ptr xrtNetDgramPacketBytesCopy(const xnetdgrampkt* pPacket, size_t* pOutLen);
XXAPI str xrtNetDgramPacketText(const xnetdgrampkt* pPacket);
	XXAPI str xrtNetDgramPacketFromText(const xnetdgrampkt* pPacket);
	XXAPI int xrtNetDgramPacketFromPort(const xnetdgrampkt* pPacket);
	XXAPI xfuture* xrtNetTcpConnectAsync(const char* sHost, uint16 iPort, int64 iTimeoutMs);
	XXAPI xfuture* xrtNetTcpAcceptAsync(xnetlistener* pListener, int64 iTimeoutMs);
	XXAPI xfuture* xrtNetStreamRecvTextAsync(xnetstream* pStream, size_t iMaxBytes, int64 iTimeoutMs);
	XXAPI xfuture* xrtNetStreamSendTextAsync(xnetstream* pStream, const char* sText);
	XXAPI xfuture* xrtNetStreamDrainAsync(xnetstream* pStream, int64 iTimeoutMs);
	XXAPI xfuture* xrtNetUdpRecvAsync(xdgramsock* pSock, int64 iTimeoutMs);
	XXAPI xfuture* xrtNetUdpSendTextAsync(xdgramsock* pSock, const char* sHost, uint16 iPort, const char* sText);
	// Future / Promise / Task 基础接口
	XXAPI xnetfuture* xrtNetFutureCreate(void);
	// 创建 Future
	XXAPI xfuture* xFutureCreate(void);
	// 增加 Future 引用计数
	XXAPI xfuture* xFutureAddRef(xfuture* pFuture);
	// 释放 Future
	XXAPI void xFutureRelease(xfuture* pFuture);
	// 获取 Future 状态
	XXAPI xfuture_state xFutureState(xfuture* pFuture);
	// 获取 Future 状态
	XXAPI int32 xFutureStatus(xfuture* pFuture);
	// 获取 Future 值
	XXAPI ptr xFutureValue(xfuture* pFuture);
	// 获取 Future 错误
	XXAPI str xFutureError(xfuture* pFuture);
	// 获取 Future 结果
	XXAPI bool xFutureGetResult(xfuture* pFuture, xfuture_result* pOut);
	// 判断 Future 结果值是否由 Future 拥有
	XXAPI bool xFutureValueIsOwned(xfuture* pFuture);
	// 设置 Future 调试名称
	XXAPI bool xFutureSetDebugName(xfuture* pFuture, str sDebugName);
	// 获取 Future 调试名称
	XXAPI str xFutureGetDebugName(xfuture* pFuture);
	// 获取 Future 创建时间
	XXAPI uint64 xFutureGetCreateTimeMs(xfuture* pFuture);
	// 获取 Future 完成时间
	XXAPI uint64 xFutureGetCompleteTimeMs(xfuture* pFuture);
	// 获取 Future 尚未执行的 continuation 数量
	XXAPI int xFutureGetPendingContinuationCount(xfuture* pFuture);
	// 获取 Future 组源 index
	XXAPI int xFutureGetGroupSourceIndex(xfuture* pFuture);
	// 获取 Future 组源
	XXAPI xfuture* xFutureGetGroupSource(xfuture* pFuture);
	// 查看 Future 组源
	XXAPI xfuture* xFuturePeekGroupSource(xfuture* pFuture);
	// 查看 Future 全部值
	XXAPI const xfuture_all_value* xFuturePeekAllValue(xfuture* pFuture);
	// 统计 Future get 全部值
	XXAPI int xFutureGetAllValueCount(xfuture* pFuture);
	// 获取 Future 全部值 item
	XXAPI ptr xFutureGetAllValueItem(xfuture* pFuture, int iIndex);
	// 等待 Future
	XXAPI bool xFutureWait(xfuture* pFuture);
	// 等待 Future 超时
	XXAPI bool xFutureWaitTimeout(xfuture* pFuture, int64 iTimeoutMs);
	// 等待 Future 直到指定时刻
	XXAPI bool xFutureWaitUntil(xfuture* pFuture, int64 iDeadlineMs);
	// 等待 Future 协程
	XXAPI bool xFutureWaitCo(xfuture* pFuture);
	// 等待 Future 协程超时
	XXAPI bool xFutureWaitCoTimeout(xfuture* pFuture, int64 iTimeoutMs);
	// 等待 Future 协程直到指定时刻
	XXAPI bool xFutureWaitCoUntil(xfuture* pFuture, int64 iDeadlineMs);
	// 等待 Future 值
	XXAPI ptr xFutureWaitValue(xfuture* pFuture);
	// 等待 Future 值超时
	XXAPI ptr xFutureWaitValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
	// 等待 Future 值直到指定时刻
	XXAPI ptr xFutureWaitValueUntil(xfuture* pFuture, int64 iDeadlineMs);
	// 等待 Future 协程值
	XXAPI ptr xFutureWaitCoValue(xfuture* pFuture);
	// 等待 Future 协程值超时
	XXAPI ptr xFutureWaitCoValueTimeout(xfuture* pFuture, int64 iTimeoutMs);
	// 等待 Future 协程值直到指定时刻
	XXAPI ptr xFutureWaitCoValueUntil(xfuture* pFuture, int64 iDeadlineMs);
	// 请求取消 Future
	XXAPI bool xFutureRequestCancel(xfuture* pFuture);
	// 将 Future 的取消请求转发到另一个 Future
	XXAPI bool xFutureForwardCancelTo(xfuture* pFuture, xfuture* pTarget);
	// 创建 Promise
	XXAPI xpromise* xPromiseCreate(xfuture* pFuture);
	// 销毁 Promise
	XXAPI void xPromiseDestroy(xpromise* pPromise);
	// 获取 Promise Future
	XXAPI xfuture* xPromiseGetFuture(xpromise* pPromise);
	// 查看 Promise Future
	XXAPI xfuture* xPromisePeekFuture(xpromise* pPromise);
	// 解析 Promise
	XXAPI bool xPromiseResolve(xpromise* pPromise, ptr pValue);
	XXAPI bool xPromiseResolveOwned(xpromise* pPromise, ptr pValue);
	// 以错误状态拒绝 Promise
	XXAPI bool xPromiseReject(xpromise* pPromise, int32 iStatus, str sError);
	// 取消 Promise
	XXAPI bool xPromiseCancel(xpromise* pPromise, str sError);
	// 关闭 Promise
	XXAPI bool xPromiseClose(xpromise* pPromise, str sError);
	// 在网络引擎线程中运行任务
	XXAPI xfuture* xTaskRunEngine(xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
	// 在网络引擎线程中延迟运行任务
	XXAPI xfuture* xTaskRunDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg);
	// 在独立线程中运行任务
	XXAPI xfuture* xTaskRunThread(xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
	#if !defined(XRT_NO_FILE_ASYNC)
		
		// 异步读取文件
		XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize);
		// 异步写入文件
		XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize);
		// 异步刷新文件
		XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile);
		// 异步获取文件大小
		XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile);
		// 异步设置文件大小
		XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize);
		// 异步追加文件
		XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset);
		// 异步读取文件全部
		XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset);
		// 异步写入文件全部
		XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset);
		// 异步获取文件全部
		XXAPI xfuture* xrtFileGetAllAsync(str sPath);
		// 异步写入原始字节到文件
		XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize);
		// 异步复制文件
		XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite);
		// 异步移动文件
		XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite);
		// 异步删除文件
		XXAPI xfuture* xrtFileDeleteAsync(str sPath);
		// 异步创建目录
		XXAPI xfuture* xrtDirCreateAsync(str sPath);
		// 异步创建目录全部
		XXAPI xfuture* xrtDirCreateAllAsync(str sPath);
		// 异步复制目录
		XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite);
		// 异步移动目录
		XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite);
		// 异步删除目录
		XXAPI xfuture* xrtDirDeleteAsync(str sPath);
	#endif
	#ifndef XRT_NO_SUBPROCESS
		
		// 等待进程 Future
		XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess);
		
	#endif
	#if !defined(XRT_NO_COROUTINE)
		
		// 在协程调度器中运行任务
		XXAPI xfuture* xTaskRunCo(xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
		
	#endif
	// Future 延续回调绑定接口
	// 在当前执行路径中追加 then 回调
	XXAPI xfuture* xFutureThenInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在当前执行路径中追加 catch 回调
	XXAPI xfuture* xFutureCatchInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在当前执行路径中追加 finally 回调
	XXAPI xfuture* xFutureFinallyInline(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg);
	// 在源 Future 完成它的线程中追加 then 回调
	XXAPI xfuture* xFutureThenSource(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在源 Future 完成它的线程中追加 catch 回调
	XXAPI xfuture* xFutureCatchSource(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在源 Future 完成它的线程中追加 finally 回调
	XXAPI xfuture* xFutureFinallySource(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg);
	// 在当前线程上下文中追加 then 回调
	XXAPI xfuture* xFutureThenCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在当前线程上下文中追加 catch 回调
	XXAPI xfuture* xFutureCatchCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg);
	// 在当前线程上下文中追加 finally 回调
	XXAPI xfuture* xFutureFinallyCurrent(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg);
	// 在网络引擎线程中追加 then 回调
	XXAPI xfuture* xFutureThenEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg);
	// 在网络引擎线程中追加 catch 回调
	XXAPI xfuture* xFutureCatchEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg);
	// 在网络引擎线程中追加 finally 回调
	XXAPI xfuture* xFutureFinallyEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_finally_fn pfnCont, ptr pArg);
	#if !defined(XRT_NO_COROUTINE)
		// 在协程调度器中追加 then 回调
		XXAPI xfuture* xFutureThenCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize);
		// 在协程调度器中追加 catch 回调
		XXAPI xfuture* xFutureCatchCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize);
		// 在协程调度器中追加 finally 回调
		XXAPI xfuture* xFutureFinallyCo(xfuture* pFuture, xcosched* pSched, xfuture_finally_fn pfnCont, ptr pArg, size_t iStackSize);
		
	#endif
	
	// 创建任意 Future 完成聚合
	XXAPI xfuture* xFutureWhenAny(xfuture** arrFuture, int iCount);
	// 创建全部 Future 完成聚合
	XXAPI xfuture* xFutureWhenAll(xfuture** arrFuture, int iCount);
	// 创建 Future 竞速聚合
	XXAPI xfuture* xFutureRace(xfuture** arrFuture, int iCount);
	// 推进 Future 当前延续回调
	XXAPI int xFuturePumpCurrentContinuations(int iMaxCount);
	// 创建任务组
	XXAPI xtaskgroup* xTaskGroupCreate(void);
	// 销毁任务组
	XXAPI void xTaskGroupDestroy(xtaskgroup* pGroup);
	// 关闭任务组
	XXAPI void xTaskGroupClose(xtaskgroup* pGroup);
	// 创建任务组子节点
	XXAPI xtaskgroup* xTaskGroupCreateChild(xtaskgroup* pParent);
	// 绑定任务组父 Future
	XXAPI bool xTaskGroupBindParent(xtaskgroup* pGroup, xfuture* pParent);
	// 增加任务组 Future
	XXAPI bool xTaskGroupAddFuture(xtaskgroup* pGroup, xfuture* pFuture);
	// 统计任务组
	XXAPI int xTaskGroupCount(xtaskgroup* pGroup);
	// 回收任务组中已完成的子任务
	XXAPI int xTaskGroupReapCompleted(xtaskgroup* pGroup);
	// 加入任务组 Future
	XXAPI xfuture* xTaskGroupJoinFuture(xtaskgroup* pGroup);
	// 等待任务组结束
	XXAPI bool xTaskGroupJoin(xtaskgroup* pGroup);
	// 限时等待任务组结束
	XXAPI bool xTaskGroupJoinTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);
	// 等待任务组结束直到指定时刻
	XXAPI bool xTaskGroupJoinUntil(xtaskgroup* pGroup, int64 iDeadlineMs);
	// 等待任务组
	XXAPI bool xTaskGroupWait(xtaskgroup* pGroup);
	// 等待任务组超时
	XXAPI bool xTaskGroupWaitTimeout(xtaskgroup* pGroup, int64 iTimeoutMs);
	// 等待任务组直到指定时刻
	XXAPI bool xTaskGroupWaitUntil(xtaskgroup* pGroup, int64 iDeadlineMs);
	// 取消任务组
	XXAPI void xTaskGroupCancel(xtaskgroup* pGroup);
	// 在网络引擎线程中启动任务组任务
	XXAPI xfuture* xTaskGroupRunEngine(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg);
	// 在网络引擎线程中延迟启动任务组任务
	XXAPI xfuture* xTaskGroupRunDelayed(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg);
	// 在线程中启动任务组任务
	XXAPI xfuture* xTaskGroupRunThread(xtaskgroup* pGroup, xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize);
	#if !defined(XRT_NO_COROUTINE)
		// 在协程调度器中启动任务组任务
		XXAPI xfuture* xTaskGroupRunCo(xtaskgroup* pGroup, xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize);
		
	#endif
	// 统一等待源封装
	XXAPI xnetwaitsrc xrtNetWaitSourceNone(void);
	// 创建空等待源
	XXAPI xwaitsrc xWaitSourceNone(void);
	// 创建 Future 等待源
	XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture);
	// 从 Future 构造等待源
	XXAPI xwaitsrc xWaitSourceFromFuture(xfuture* pFuture);
	// 创建流等待源
	XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind);
	// 从流对象构造等待源
	XXAPI xwaitsrc xWaitSourceFromStream(xnetstream* pStream, uint32 iWaitKind);
	// 创建数据报接收等待源
	XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock);
	// 从数据报接收构造等待源
	XXAPI xwaitsrc xWaitSourceFromDgramRecv(xdgramsock* pSock);
	// 创建监听器接受等待源
	XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener);
	// 从监听器接受构造等待源
	XXAPI xwaitsrc xWaitSourceFromListenerAccept(xnetlistener* pListener);
	// 销毁网络 Future
	XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture);
	// 等待网络 Future
	XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs);
	// 获取网络 Future 状态
	XXAPI xnet_result xrtNetFutureStatus(xnetfuture* pFuture);
	// 获取网络 Future 值
	XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture);
	// 等待网络 Future 直到指定时刻
	XXAPI xnet_result xrtNetFutureWaitUntil(xnetfuture* pFuture, int64_t iDeadlineMs);
	// 等待网络 Future 协程直到指定时刻
	XXAPI xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs);
	// 等待网络 Future 协程超时
	XXAPI xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs);
	// 等待网络 Future 协程
	XXAPI xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture);
	// 获取网络同步 hidden 引擎
	XXAPI xnetengine* xrtNetSyncGetHiddenEngine(void);
	// 关闭网络同步模式使用的隐藏引擎
	XXAPI void xrtNetSyncShutdownHiddenEngine(void);
	// 向网络引擎投递返回 Future 的任务
	XXAPI xnetfuture* xrtNetEnginePostFuture(xnetengine* pEngine, uint32 iAffinityKey, xnet_future_task_fn pfnTask, ptr pArg);
	// 向网络引擎投递延迟返回 Future 的任务
	XXAPI xnetfuture* xrtNetEnginePostDelayedFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_future_task_fn pfnTask, ptr pArg);
	// 获取网络流排空事件对应的 Future
	XXAPI xnetfuture* xrtNetStreamDrainFuture(xnetstream* pStream);
	// 获取网络流可写事件对应的 Future
	XXAPI xnetfuture* xrtNetStreamWritableFuture(xnetstream* pStream);
	// 关闭网络流 Future
	XXAPI xnetfuture* xrtNetStreamCloseFuture(xnetstream* pStream);
	// 获取网络流可读事件对应的 Future
	XXAPI xnetfuture* xrtNetStreamReadableFuture(xnetstream* pStream);
	// 按等待类型获取网络流事件 Future
	XXAPI xnetfuture* xrtNetStreamFutureEx(xnetstream* pStream, uint32 iWaitKind);
	// 接受网络监听器 Future
	XXAPI xnetfuture* xrtNetListenerAcceptFuture(xnetlistener* pListener);
	// 接收网络数据报 Future
	XXAPI xnetfuture* xrtNetDgramRecvFuture(xdgramsock* pSock);
	
	// 同步等待接口
	XXAPI xnet_result xrtNetStreamWaitEx(xnetstream* pStream, uint32 iWaitKind);
	// 等待网络流超时扩展
	XXAPI xnet_result xrtNetStreamWaitTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	// 等待网络流直到指定时刻扩展
	XXAPI xnet_result xrtNetStreamWaitUntilEx(xnetstream* pStream, uint32 iWaitKind, int64_t iDeadlineMs);
	// 创建 wait等待源
	XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc);
	// 等待 wait 源完成
	XXAPI bool xWaitSourceWait(const xwaitsrc* pSrc);
	// 创建 wait 超时等待源
	XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
	// 限时等待 wait 源完成
	XXAPI bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	// 创建 wait 直到指定时刻等待源
	XXAPI xnet_result xrtNetWaitSourceWaitUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs);
	// 等待 wait 源直到指定时刻
	XXAPI bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	// 创建 wait 值等待源
	XXAPI xnet_result xrtNetWaitSourceWaitValue(const xnetwaitsrc* pSrc, ptr* ppValue);
	// 等待 wait 源并返回其值
	XXAPI ptr xWaitSourceWaitValue(const xwaitsrc* pSrc);
	// 创建 wait 值超时等待源
	XXAPI xnet_result xrtNetWaitSourceWaitValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue);
	// 等待 x wait 源值超时
	XXAPI ptr xWaitSourceWaitValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	// 创建 wait 值直到指定时刻等待源
	XXAPI xnet_result xrtNetWaitSourceWaitValueUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs, ptr* ppValue);
	// 等待 x wait 源值直到指定时刻
	XXAPI ptr xWaitSourceWaitValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	// 接受网络监听器
	XXAPI xnet_result xrtNetListenerAccept(xnetlistener* pListener, xnetstream** ppStream);
	// 接受网络监听器超时
	XXAPI xnet_result xrtNetListenerAcceptTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream);
	// 接受网络监听器直到指定时刻
	XXAPI xnet_result xrtNetListenerAcceptUntil(xnetlistener* pListener, int64_t iDeadlineMs, xnetstream** ppStream);
	// 接收网络数据报
	XXAPI xnet_result xrtNetDgramRecv(xdgramsock* pSock, xnetdgrampkt** ppPacket);
	// 接收网络数据报超时
	XXAPI xnet_result xrtNetDgramRecvTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket);
	// 接收网络数据报直到指定时刻
	XXAPI xnet_result xrtNetDgramRecvUntil(xdgramsock* pSock, int64_t iDeadlineMs, xnetdgrampkt** ppPacket);
	// 协程等待接口
	XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
	// 等待网络流协程超时扩展
	XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	// 等待网络流协程直到指定时刻扩展
	XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);
	// 等待网络流 drain 协程
	XXAPI xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream);
	// 等待网络流 drain 协程超时
	XXAPI xnet_result xrtNetStreamWaitDrainCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	// 等待网络流 drain 协程直到指定时刻
	XXAPI xnet_result xrtNetStreamWaitDrainCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	// 等待网络流 writable 协程
	XXAPI xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream);
	// 等待网络流 writable 协程超时
	XXAPI xnet_result xrtNetStreamWaitWritableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	// 等待网络流 writable 协程直到指定时刻
	XXAPI xnet_result xrtNetStreamWaitWritableCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	// 关闭网络流 wait 协程
	XXAPI xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream);
	// 关闭网络流 wait 协程超时
	XXAPI xnet_result xrtNetStreamWaitCloseCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	// 关闭网络流 wait 协程直到指定时刻
	XXAPI xnet_result xrtNetStreamWaitCloseCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	// 等待网络流 readable 协程
	XXAPI xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream);
	// 等待网络流 readable 协程超时
	XXAPI xnet_result xrtNetStreamWaitReadableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs);
	// 等待网络流 readable 协程直到指定时刻
	XXAPI xnet_result xrtNetStreamWaitReadableCoUntil(xnetstream* pStream, int64 iDeadlineMs);
	// 等待网络流协程扩展
	XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
	// 等待网络流协程超时扩展
	XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
	// 等待网络流协程直到指定时刻扩展
	XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);
	// 创建 wait 协程等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
	// 在协程中等待 wait 源完成
	XXAPI bool xWaitSourceWaitCo(const xwaitsrc* pSrc);
	// 创建 wait 协程超时等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCoTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
	// 等待 x wait 源协程超时
	XXAPI bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	// 创建 wait 协程直到指定时刻等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCoUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs);
	// 等待 x wait 源协程直到指定时刻
	XXAPI bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	// 创建 wait 协程值等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue);
	// 等待 x wait 源协程值
	XXAPI ptr xWaitSourceWaitCoValue(const xwaitsrc* pSrc);
	// 创建 wait 协程值超时等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCoValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue);
	// 等待 x wait 源协程值超时
	XXAPI ptr xWaitSourceWaitCoValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
	// 创建 wait 协程值直到指定时刻等待源
	XXAPI xnet_result xrtNetWaitSourceWaitCoValueUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs, ptr* ppValue);
	// 等待 x wait 源协程值直到指定时刻
	XXAPI ptr xWaitSourceWaitCoValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
	// 接受网络监听器协程
	XXAPI xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream);
	// 接受网络监听器协程超时
	XXAPI xnet_result xrtNetListenerAcceptCoTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream);
	// 接受网络监听器协程直到指定时刻
	XXAPI xnet_result xrtNetListenerAcceptCoUntil(xnetlistener* pListener, int64 iDeadlineMs, xnetstream** ppStream);
	// 接收网络数据报协程
	XXAPI xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket);
	// 接收网络数据报协程超时
	XXAPI xnet_result xrtNetDgramRecvCoTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket);
	// 接收网络数据报协程直到指定时刻
	XXAPI xnet_result xrtNetDgramRecvCoUntil(xdgramsock* pSock, int64 iDeadlineMs, xnetdgrampkt** ppPacket);
	#ifndef XRT_NO_XHTTP
		
		// XNet 内建 HTTP 客户端
		XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine);
		XXAPI xhttpclient* xrtHttpClientCreate(xnetengine* pEngine);
		XXAPI void xrtHttpClientDestroy(xhttpclient* pClient);
		XXAPI void xrtHttpClientCloseIdle(xhttpclient* pClient);
		XXAPI xnetfuture* xrtHttpClientExecuteAsync(xhttpclient* pClient, const xhttprequest* pReq);
		XXAPI xnetfuture* xrtHttpClientExecuteStreamAsync(xhttpclient* pClient, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks);
		XXAPI xhttpresponse* xrtHttpClientExecuteSync(xhttpclient* pClient, const xhttprequest* pReq, xnet_result* pStatus);
		// 初始化 HTTP 请求对象
		XXAPI void xrtHttpRequestInit(xhttprequest* pReq);
		XXAPI xhttprequest* xrtHttpRequestCreate(void);
		// 释放 HTTP 请求对象内部资源
		XXAPI void xrtHttpRequestUnit(xhttprequest* pReq);
		XXAPI void xrtHttpRequestDestroy(xhttprequest* pReq);
		// 设置 HTTP 请求方法
		XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);
		// 设置 HTTP request URL
		XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);
		// 设置 HTTP request 头部
		XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);
		XXAPI bool xrtHttpRequestAddHeader(xhttprequest* pReq, const char* sName, const char* sValue);
		XXAPI uint32 xrtHttpRequestRemoveHeader(xhttprequest* pReq, const char* sName);
		// 复制请求正文并设置 Content-Type
		XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);
		// 设置 HTTP request 超时
		XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
		// 设置 HTTP 请求空闲超时
		XXAPI void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
		XXAPI void xrtHttpDiagnosticsInit(xhttpdiagnostics* pDiagnostics);
		XXAPI const char* xrtHttpErrorCodeName(xhttp_error_code eError);
		XXAPI const char* xrtHttpPhaseName(xhttp_phase ePhase);
		XXAPI void xrtHttpRequestSetDiagnostics(xhttprequest* pReq, xhttpdiagnostics* pDiagnostics);
		// 设置 HTTPS 是否校验证书
		XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);
		// 销毁 HTTP 响应对象
		XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp);
		// 获取 HTTP response 头部
		XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);
		XXAPI uint32 xrtHttpResponseHeaderCount(const xhttpresponse* pResp);
		XXAPI const char* xrtHttpResponseHeaderNameAt(const xhttpresponse* pResp, uint32 iIndex);
		XXAPI const char* xrtHttpResponseHeaderValueAt(const xhttpresponse* pResp, uint32 iIndex);
		XXAPI uint32 xrtHttpResponseStatusCode(const xhttpresponse* pResp);
		XXAPI const char* xrtHttpResponseReason(const xhttpresponse* pResp);
		XXAPI int64_t xrtHttpResponseContentLength(const xhttpresponse* pResp);
		XXAPI const void* xrtHttpResponseBody(const xhttpresponse* pResp, size_t* pLen);
		XXAPI char* xrtHttpResponseBodyTextCopy(const xhttpresponse* pResp);
		XXAPI const xhttpdiagnostics* xrtHttpResponseDiagnostics(const xhttpresponse* pResp);
		XXAPI void xrtHttpStreamCallbacksInit(xhttpstreamcallbacks* pCallbacks);
		// 异步执行 HTTP 请求
		XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq);
		XXAPI xnetfuture* xrtHttpExecuteStreamAsync(xnetengine* pEngine, const xhttprequest* pReq, const xhttpstreamcallbacks* pCallbacks);
		XXAPI bool xrtHttpCancel(xnetfuture* pFuture);
		// 同步执行 HTTP 请求
		XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus);
		XXAPI xhttpresponse* xrtHttpExecuteSyncDefault(const xhttprequest* pReq);
		
	#endif
	#ifndef XRT_NO_XHTTPD
		
		// XNet 内建 HTTP 服务端
		XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
		XXAPI uint32 xrtHttpdRequestHeaderCount(const xhttpdrequest* pReq);
		XXAPI const char* xrtHttpdRequestHeaderNameAt(const xhttpdrequest* pReq, uint32 iIndex);
		XXAPI const char* xrtHttpdRequestHeaderValueAt(const xhttpdrequest* pReq, uint32 iIndex);
		// 获取 HTTP 服务端 request 方法 ID
		XXAPI uint32 xrtHttpdRequestMethod(const xhttpdrequest* pReq);
		// 获取 HTTP 服务端 response 头部
		XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
		// 初始化 HTTP 服务端配置
		XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
		// 初始化 HTTP 服务端请求对象
		XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
		// 释放 HTTP 服务端请求对象内部资源
		XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
		// 预留 HTTP 服务端 request body 缓冲容量
		XXAPI bool xrtHttpdRequestReserveBody(xhttpdrequest* pReq, size_t iCap);
		// 复制设置 HTTP 服务端 request body
		XXAPI bool xrtHttpdRequestSetBodyCopy(xhttpdrequest* pReq, const void* pData, size_t iLen);
		// 追加 HTTP 服务端 request body
		XXAPI bool xrtHttpdRequestAppendBodyCopy(xhttpdrequest* pReq, const void* pData, size_t iLen);
		// 初始化 HTTP 服务端响应对象
		XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
		// 释放 HTTP 服务端响应对象内部资源
		XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
		// 预留 HTTP 服务端 response body 缓冲容量
		XXAPI bool xrtHttpdResponseReserveBody(xhttpdresponse* pResp, size_t iCap);
		// 创建 HTTP 服务端响应对象
		XXAPI xhttpdresponse* xrtHttpdResponseCreate(void);
		// 销毁 HTTP 服务端响应对象
		XXAPI void xrtHttpdResponseDestroy(xhttpdresponse* pResp);
		// 设置 HTTP 服务端 response 状态
		XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
		// 设置 HTTP 服务端 response 头部
		XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
		XXAPI bool xrtHttpdResponseAddHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
		// 获取 HTTP 服务端默认状态文本
		XXAPI const char* xrtHttpdStatusText(uint32 iStatusCode);
		// 复制服务端响应正文并设置 Content-Type
		XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
		// 追加 HTTP 服务端 response body
		XXAPI bool xrtHttpdResponseAppendBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen);
		// 一次性填充 HTTP 服务端响应对象
		XXAPI bool xrtHttpdResponseReply(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason, const char* sHeaders, const void* pBody, size_t iBodyLen);
		// 判断 HTTP 服务端连接是否仍然打开
		XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn);
		// 向 HTTP 服务端连接发送响应
		XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp);
		// 向 HTTP 服务端连接一次性发送轻量响应
		XXAPI xnet_result xrtHttpdConnReply(xhttpdconn* pConn, uint32 iStatusCode, const char* sReason, const char* sHeaders, const void* pBody, size_t iBodyLen);
		// 开始 HTTP 服务端连接流式响应
		XXAPI xnet_result xrtHttpdConnStart(xhttpdconn* pConn, const xhttpdresponse* pResp);
		// 向 HTTP 服务端连接流式响应发送数据
		XXAPI xnet_result xrtHttpdConnSend(xhttpdconn* pConn, const void* pData, size_t iLen);
		// 结束 HTTP 服务端连接流式响应
		XXAPI xnet_result xrtHttpdConnEnd(xhttpdconn* pConn);
		// 向 HTTP 服务端连接分块发送文件响应
		XXAPI xnet_result xrtHttpdConnSendFile(xhttpdconn* pConn, const xhttpdresponse* pResp, const char* sFilePath, size_t iChunkSize);
		// 主动关闭 HTTP 服务端连接
		XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags);
		// 创建 HTTP 服务端
		XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
		// 获取 HTTP 服务端 bound 端口
		XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
		// 获取 HTTP 服务端当前连接数量
		XXAPI uint32 xrtHttpdConnectionCount(xhttpdserver* pServer);
		// 启动 HTTP 服务端
		XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
		XXAPI bool xrtHttpdDrain(xhttpdserver* pServer, uint32 iTimeoutMs);
		// 停止 HTTP 服务端
		XXAPI void xrtHttpdStop(xhttpdserver* pServer);
		// 销毁 HTTP 服务端
		XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
	#endif
	#ifndef XRT_NO_XWEB
		
		// XRT 高层 Web 服务基座
		XXAPI void xrtWebConfigInit(xwebconfig* pCfg);
		XXAPI void xrtWebStaticConfigInit(xwebstaticconfig* pCfg);
		XXAPI xwebapp* xrtWebAppCreate(void);
		XXAPI void xrtWebAppRetain(xwebapp* pApp);
		XXAPI void xrtWebAppRelease(xwebapp* pApp);
		XXAPI void xrtWebAppDestroy(xwebapp* pApp);
		XXAPI uint32 xrtWebMethodMask(const char* sMethods);
		XXAPI bool xrtWebAppError(xwebapp* pApp, xweberrorhandler pHandler, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebAppRouteEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebAppStreamRouteEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebAppStreamRoute(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData);
		XXAPI bool xrtWebAppStreamRouteAsyncEx(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebAppStreamRouteAsync(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData);
		XXAPI bool xrtWebAppRoute(xwebapp* pApp, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebAppGet(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebAppPost(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebAppAny(xwebapp* pApp, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebAppStatic(xwebapp* pApp, const char* sMount, const char* sRoot, const xwebstaticconfig* pCfg);
		XXAPI bool xrtWebAppStaticDefault(xwebapp* pApp, const char* sMount, const char* sRoot);
		XXAPI bool xrtWebAppStaticEx(xwebapp* pApp, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags);
		XXAPI xwebserver* xrtWebServerCreate(xnetengine* pEngine, const xwebconfig* pCfg);
		XXAPI xwebserver* xrtWebServerCreateHostEx(xnetengine* pEngine, const char* sHost, uint16 iPort, uint32 iBacklog, uint32 iRecvLimit, uint32 iBodyLimit, uint32 iWorkerCount);
		XXAPI xwebserver* xrtWebServerCreateHostTlsEx(xnetengine* pEngine, const char* sCertFile, const char* sKeyFile, const char* sHost, uint16 iPort, uint32 iBacklog, uint32 iRecvLimit, uint32 iBodyLimit, uint32 iWorkerCount);
		XXAPI void xrtWebServerDestroy(xwebserver* pServer);
		XXAPI xnet_result xrtWebServerStart(xwebserver* pServer);
		XXAPI bool xrtWebServerStartOk(xwebserver* pServer);
		XXAPI void xrtWebServerStop(xwebserver* pServer);
		XXAPI bool xrtWebServerDrain(xwebserver* pServer, uint32 iTimeoutMs);
		XXAPI uint16 xrtWebServerPort(const xwebserver* pServer);
		XXAPI uint32 xrtWebServerRequestCount(const xwebserver* pServer);
		XXAPI uint32 xrtWebServerConnectionCount(xwebserver* pServer);
		XXAPI bool xrtWebServerSetApp(xwebserver* pServer, xwebapp* pApp);
		XXAPI bool xrtWebServerReloadApp(xwebserver* pServer, xwebapp* pApp);
		XXAPI bool xrtWebServerHost(xwebserver* pServer, const char* sHost, xwebapp* pApp);
		XXAPI bool xrtWebServerRemoveHost(xwebserver* pServer, const char* sHost);
		XXAPI bool xrtWebServerTlsHost(xwebserver* pServer, const char* sHost, const char* sCertFile, const char* sKeyFile);
		XXAPI bool xrtWebServerRemoveTlsHost(xwebserver* pServer, const char* sHost);
		XXAPI bool xrtWebServerError(xwebserver* pServer, xweberrorhandler pHandler, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebServerRouteEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebServerRoute(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebServerStreamRouteEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebServerStreamRoute(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyend pEnd, ptr pUserData);
		XXAPI bool xrtWebServerStreamRouteAsyncEx(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData, xwebfree pFreeUserData);
		XXAPI bool xrtWebServerStreamRouteAsync(xwebserver* pServer, uint32 iMethodMask, const char* sPattern, xwebbodybegin pBegin, xwebbodychunk pBody, xwebbodyendasync pEndAsync, ptr pUserData);
		XXAPI bool xrtWebServerGet(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebServerPost(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebServerAny(xwebserver* pServer, const char* sPattern, xwebhandler pHandler, ptr pUserData);
		XXAPI bool xrtWebServerStatic(xwebserver* pServer, const char* sMount, const char* sRoot, const xwebstaticconfig* pCfg);
		XXAPI bool xrtWebServerStaticDefault(xwebserver* pServer, const char* sMount, const char* sRoot);
		XXAPI bool xrtWebServerStaticEx(xwebserver* pServer, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32 iFlags);
		XXAPI const char* xrtWebRequestMethod(const xwebrequest* pReq);
		XXAPI str xrtWebRequestMethodCopy(const xwebrequest* pReq);
		XXAPI uint32 xrtWebRequestMethodId(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestTarget(const xwebrequest* pReq);
		XXAPI str xrtWebRequestTargetCopy(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestPath(const xwebrequest* pReq);
		XXAPI str xrtWebRequestPathCopy(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestQuery(const xwebrequest* pReq);
		XXAPI str xrtWebRequestQueryCopy(const xwebrequest* pReq);
		XXAPI size_t xrtWebRequestQueryCount(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestQueryPairAt(const xwebrequest* pReq, size_t iIndex, xrtquerypair* pOut);
		XXAPI bool xrtWebRequestQueryHasValueAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestQueryNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestQueryValueCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI const char* xrtWebRequestHeader(const xwebrequest* pReq, const char* sName);
		XXAPI str xrtWebRequestHeaderCopy(const xwebrequest* pReq, const char* sName);
		XXAPI size_t xrtWebRequestHeaderCount(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestHeaderNameAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI const char* xrtWebRequestHeaderValueAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestHeaderNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestHeaderValueCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI const void* xrtWebRequestBody(const xwebrequest* pReq, size_t* pLen);
		XXAPI str xrtWebRequestBodyTextCopy(const xwebrequest* pReq);
		XXAPI int64 xrtWebRequestContentLength(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestContentType(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestIsFormUrlEncoded(const xwebrequest* pReq);
		XXAPI size_t xrtWebRequestFormCount(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestFormPairAt(const xwebrequest* pReq, size_t iIndex, xrtquerypair* pOut);
		XXAPI bool xrtWebRequestFormHasValueAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestFormNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestFormValueCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestFormValueCopy(const xwebrequest* pReq, const char* sName);
		XXAPI bool xrtWebRequestIsMultipartForm(const xwebrequest* pReq);
		XXAPI size_t xrtWebRequestMultipartCount(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestMultipartPartAt(const xwebrequest* pReq, size_t iIndex, xrtmultipartpartview* pOut);
		XXAPI bool xrtWebRequestMultipartIsFileAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestMultipartNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestMultipartFileNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestMultipartContentTypeCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI bool xrtWebRequestMultipartBodyViewAt(const xwebrequest* pReq, size_t iIndex, const void** ppData, size_t* pLen);
		XXAPI size_t xrtWebRequestMultipartBodySizeAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestMultipartBodyTextCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestMultipartValueCopy(const xwebrequest* pReq, const char* sName);
		XXAPI str xrtWebRequestLocalAddrCopy(const xwebrequest* pReq);
		XXAPI str xrtWebRequestRemoteAddrCopy(const xwebrequest* pReq);
		XXAPI str xrtWebRequestTlsSNICopy(const xwebrequest* pReq);
		XXAPI int xrtWebRequestLocalPort(const xwebrequest* pReq);
		XXAPI int xrtWebRequestRemotePort(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestParam(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
		XXAPI str xrtWebRequestParamCopy(const xwebrequest* pReq, const char* sName);
		XXAPI size_t xrtWebRequestParamCount(const xwebrequest* pReq);
		XXAPI const char* xrtWebRequestParamNameAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI bool xrtWebRequestParamValueViewAt(const xwebrequest* pReq, size_t iIndex, const char** ppValue, size_t* pLen);
		XXAPI str xrtWebRequestParamNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestParamValueCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI bool xrtWebRequestQueryValue(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
		XXAPI str xrtWebRequestQueryValueCopy(const xwebrequest* pReq, const char* sName);
		XXAPI bool xrtWebRequestCookie(const xwebrequest* pReq, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen);
		XXAPI str xrtWebRequestCookieCopy(const xwebrequest* pReq, const char* sName);
		XXAPI size_t xrtWebRequestCookieCount(const xwebrequest* pReq);
		XXAPI bool xrtWebRequestCookiePairAt(const xwebrequest* pReq, size_t iIndex, xrtcookiepair* pOut);
		XXAPI str xrtWebRequestCookieNameCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI str xrtWebRequestCookieValueCopyAt(const xwebrequest* pReq, size_t iIndex);
		XXAPI void xrtWebResponseStatus(xwebresponse* pResp, uint32 iStatusCode, const char* sReason);
		XXAPI bool xrtWebResponseSetHeader(xwebresponse* pResp, const char* sName, const char* sValue);
		XXAPI bool xrtWebResponseAddHeader(xwebresponse* pResp, const char* sName, const char* sValue);
		XXAPI bool xrtWebResponseCookie(xwebresponse* pResp, const char* sName, const char* sValue, const char* sPath, int32 iMaxAge, uint32 iSameSite, uint32 iFlags);
		XXAPI bool xrtWebResponseDeleteCookie(xwebresponse* pResp, const char* sName, const char* sPath);
		XXAPI bool xrtWebResponseText(xwebresponse* pResp, const char* sText, const char* sContentType);
		XXAPI bool xrtWebResponseJsonText(xwebresponse* pResp, const char* sText);
		XXAPI bool xrtWebResponseBody(xwebresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
		XXAPI bool xrtWebResponseReserveBody(xwebresponse* pResp, size_t iCap);
		XXAPI bool xrtWebResponseAppendBody(xwebresponse* pResp, const void* pData, size_t iLen);
		XXAPI bool xrtWebResponseAppendText(xwebresponse* pResp, const char* sText);
		XXAPI bool xrtWebResponseStart(xwebresponse* pResp);
		XXAPI bool xrtWebResponseSend(xwebresponse* pResp, const void* pData, size_t iLen);
		XXAPI bool xrtWebResponseWriteText(xwebresponse* pResp, const char* sText);
		XXAPI bool xrtWebResponseEnd(xwebresponse* pResp);
		XXAPI bool xrtWebResponseFile(xwebresponse* pResp, const char* sFilePath, size_t iChunkSize);
		XXAPI bool xrtWebResponseRedirect(xwebresponse* pResp, const char* sURL, uint32 iStatusCode);
		XXAPI const char* xrtWebMimeByPath(const char* sPath);
	#endif
	#ifndef XRT_NO_XWS
		
		// XNet 内建 WebSocket 客户端与服务端
		XXAPI void xrtWsClientConfigInit(xwsclientconfig* pCfg);
		// 初始化 WebSocket server 配置
		XXAPI void xrtWsServerConfigInit(xwsserverconfig* pCfg);
		// 创建 WebSocket 客户端
		XXAPI xwsclient* xrtWsClientCreate(xnetengine* pEngine, const xwsclientconfig* pCfg, const xwsclientevents* pEvents, ptr pUserData);
		// 启动 WebSocket 客户端
		XXAPI xnet_result xrtWsClientStart(xwsclient* pClient);
		// 停止 WebSocket 客户端
		XXAPI void xrtWsClientStop(xwsclient* pClient);
		// 销毁 WebSocket 客户端
		XXAPI void xrtWsClientDestroy(xwsclient* pClient);
		// 判断 WebSocket 客户端是否已连接
		XXAPI bool xrtWsClientIsOpen(const xwsclient* pClient);
		// 发送 WebSocket client 文本
		XXAPI xnet_result xrtWsClientSendText(xwsclient* pClient, const char* sText, size_t iLen);
		// 发送 WebSocket 客户端二进制消息
		XXAPI xnet_result xrtWsClientSendBinary(xwsclient* pClient, const void* pData, size_t iLen);
		// 发送 WebSocket 客户端 Ping
		XXAPI xnet_result xrtWsClientPing(xwsclient* pClient, const void* pData, size_t iLen);
		// 主动关闭 WebSocket 客户端
		XXAPI xnet_result xrtWsClientClose(xwsclient* pClient, uint16 iCode, const char* sReason);
		// 创建 WebSocket 服务端
		XXAPI xwsserver* xrtWsServerCreate(xnetengine* pEngine, const xwsserverconfig* pCfg, const xwsserverevents* pEvents, ptr pUserData);
		// 获取 WebSocket 服务端绑定端口
		XXAPI uint16 xrtWsServerBoundPort(const xwsserver* pServer);
		// 启动 WebSocket 服务端
		XXAPI xnet_result xrtWsServerStart(xwsserver* pServer);
		// 停止 WebSocket 服务端
		XXAPI void xrtWsServerStop(xwsserver* pServer);
		// 销毁 WebSocket 服务端
		XXAPI void xrtWsServerDestroy(xwsserver* pServer);
		// 判断 WebSocket 连接是否仍然打开
		XXAPI bool xrtWsConnIsOpen(const xwsconn* pConn);
		// 发送 WebSocket conn 文本
		XXAPI xnet_result xrtWsConnSendText(xwsconn* pConn, const char* sText, size_t iLen);
		// 发送 WebSocket 连接二进制消息
		XXAPI xnet_result xrtWsConnSendBinary(xwsconn* pConn, const void* pData, size_t iLen);
		// 发送 WebSocket 连接 Ping
		XXAPI xnet_result xrtWsConnPing(xwsconn* pConn, const void* pData, size_t iLen);
		// 主动关闭 WebSocket 连接
		XXAPI xnet_result xrtWsConnClose(xwsconn* pConn, uint16 iCode, const char* sReason);
	#endif
	#endif /* !XRT_NO_NETWORK && !XRT_BUILD_CORE */
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
	// 通过原始内存创建 Buffer
	XXAPI xbuffer xrtBufferFrom(ptr pData, size_t iSize);
	// 接管由 xrtMalloc 分配的原始内存，成功后由 Buffer 负责释放
	XXAPI xbuffer xrtBufferAdopt(ptr pData, size_t iSize);
	// 将 HEX 文本解码为 Buffer
	XXAPI xbuffer xrtBufferFromHex(str sText, size_t iSize);
	// 将 Base64 文本解码为 Buffer
	XXAPI xbuffer xrtBufferFromBase64(str sText, size_t iSize, str sTable);
	
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
	// 解锁指针数组
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
	// 获取成员指针（不安全接口）
	XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos);
	// 获取指针数组 inline
	static inline ptr xrtPtrArrayGet_Inline(xparray pObject, uint32 iPos)
	{
		return pObject->Memory[iPos - 1];
	}
	
	// 设置成员指针
	XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal);
	// 设置成员指针（不安全接口）
	XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal);
	// 设置指针数组 inline
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
	// 解锁数组
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
	// 不做边界检查，直接获取数组成员指针
	XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos);
	// 内联计算数组成员指针
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
	#define xrtMemUnitGC_Mark(p) (((MMU_ValuePtr)((uint8*)(p) - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC)
	
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
	// 从内存管理单元中申请一个元素
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
	// 按元素编号释放内存管理单元中的元素
	XXAPI bool xrtMemUnitFreeIdx(xmemunit objUnit, uint8 idx);
	// 内联释放内存管理单元中的元素
	static inline void xrtMemUnitFree_Inline(xmemunit objUnit, ptr obj)
	{
		if ( objUnit == NULL || obj == NULL ) {
			return;
		}
		if ( !xrtOwnerBeginMutable(&objUnit->Owner, "memory unit belongs to another thread.") ) {
			return;
		}
		MMU_ValuePtr v = (MMU_ValuePtr)((uint8*)obj - sizeof(MMU_Value));
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
	// 释放内存管理单元中的元素
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
	// 压入栈数据
	XXAPI uint32 xrtStackPushData(xstack objSTK, ptr pData);
	// 压入栈指针
	XXAPI uint32 xrtStackPushPtr(xstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtStackPop(xstack objSTK);
	// 弹出栈指针
	XXAPI ptr xrtStackPopPtr(xstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtStackTop(xstack objSTK);
	// 获取顶部栈指针
	XXAPI ptr xrtStackTopPtr(xstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtStackGetPos(xstack objSTK, uint32 iPos);
	// 不做边界检查，直接获取指定位置的栈对象
	XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint32 iPos);
	// 获取栈 pos 指针
	XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint32 iPos);
	// 不做边界检查，直接获取指定位置的栈指针成员
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
	// 压入 dyn 栈数据
	XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData);
	// 压入 dyn 栈指针
	XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtDynStackPop(xdynstack objSTK);
	// 弹出 dyn 栈指针
	XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtDynStackTop(xdynstack objSTK);
	// 获取顶部 dyn 栈指针
	XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos);
	// 不做边界检查，直接获取指定位置的动态栈对象
	XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos);
	// 获取指定位置的动态栈指针成员
	XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos);
	// 不做边界检查，直接获取指定位置的动态栈指针成员
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
	#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((uint8*)(p) - sizeof(xavltnode_struct)))
	
	// 获取 xavltnode 对应的数据段
	#define xrtAVLTreeGetNodeData(p) ((ptr)(&p[1]))
	
	// 获取根节点数据段
	#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
	
	// 初始化 AVLTree
	#define xrtAVLTB_Init(o) do { \
		(o)->RootNode = NULL; \
		(o)->Count = 0; \
		(o)->Iterator = NULL; \
	} while ( 0 )
	
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
	// 以前序 / 中序 / 后序回调遍历 AVLTree
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
	// 解锁 AVL 树
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
	// 遍历 AVL 树扩展
	XXAPI bool xrtAVLTreeWalkEx(xavltree objAVLT, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg);
	
	// 迭代器操作
	XXAPI void xrtAVLTreeIterBegin(xavltree objAVLT);
	// 获取下一个 AVL 树 iter
	XXAPI ptr xrtAVLTreeIterNext(xavltree objAVLT);
	// 结束 AVL 树 iter
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
	// 解锁字典
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
	// 设置字典
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
	// 获取字典
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
	// 解锁列表
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
	#define XRT_LIST_FOREACH(tree, idx, val) \
		xrtAVLTB_IterBegin((xavltbase)tree); \
		bool __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1; \
		int64* __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree); \
		for ( int64 idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) != NULL; __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) = xrtAVLTB_IterNext((xavltbase)tree), idx = __XRT_CONCATLINE(__xrt_iter_data_, __LINE__) ? __XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[0] : 0, __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 1 ) \
			for ( ptr val = (ptr)(&__XRT_CONCATLINE(__xrt_iter_data_, __LINE__)[1]); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__); __XRT_CONCATLINE(__xrt_iter_break_, __LINE__) = 0 )
	
	// 带类型转换的遍历宏
	#define XRT_LIST_FOREACH_TYPE(tree, idx, val, type) \
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
	// 将 int64 转为字符串
	XXAPI int xrtI64ToStr(int64_t num, char* buffer);
	// 将 uint32 转为字符串
	XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
	// 将 uint64 转为字符串
	XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
	// 将浮点数转为字符串
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
	// 将字符串转为 int64
	XXAPI int64_t xrtStrToI64(const void* pStr);
	// 将字符串转为 uint32
	XXAPI uint32_t xrtStrToU32(const void* pStr);
	// 将字符串转为 uint64
	XXAPI uint64_t xrtStrToU64(const void* pStr);
	// 将字符串转为浮点数
	XXAPI double xrtStrToNum(const void* pStr);
	
	
	
	/* ------------------------------------ Runtime Type 函数库 ------------------------------------ */
	/*
		说明：
			Runtime Type 是 XRT 自身的运行期类型描述系统。
			它不依赖 xlang 语法，可直接被 C 程序使用。
			xlang 的编译期类型系统只映射到这里，不反向污染 XRT。
	*/
	typedef struct xvalue_struct xvalue_struct;
	typedef xvalue_struct* xvalue;
	typedef struct xrt_type_desc xrt_type_desc;
	typedef struct xrt_type_ops xrt_type_ops;
	typedef struct xrt_generic_type xrt_generic_type;
	typedef struct xrt_method_desc xrt_method_desc;
	typedef struct xrt_method_table xrt_method_table;
	typedef struct xrt_func_param xrt_func_param;
	typedef struct xrt_func_sig xrt_func_sig;
	typedef struct xrt_call_frame xrt_call_frame;
	typedef struct xrt_call_result xrt_call_result;
	typedef struct xrt_callable xrt_callable;
	typedef struct xrt_record_value xrt_record_value;
	typedef struct xrt_handle_value xrt_handle_value;
	typedef struct xset_struct xset_struct;
	typedef xset_struct* xset;
	typedef struct xrt_typed_array_struct xrt_typed_array_struct;
	typedef struct xrt_typed_list_struct xrt_typed_list_struct;
	typedef struct xrt_typed_set_struct xrt_typed_set_struct;
	typedef struct xrt_typed_dict_struct xrt_typed_dict_struct;
	typedef struct xrt_typed_stack_struct xrt_typed_stack_struct;
	typedef struct xrt_typed_avl_tree_struct xrt_typed_avl_tree_struct;
	typedef struct xrt_typed_queue_struct xrt_typed_queue_struct;
	typedef xrt_typed_array_struct* xtarray;
	typedef xrt_typed_list_struct* xtlist;
	typedef xrt_typed_set_struct* xtset;
	typedef xrt_typed_dict_struct* xtdict;
	typedef xrt_typed_stack_struct* xtstack;
	typedef xrt_typed_avl_tree_struct* xtavltree;
	typedef xrt_typed_queue_struct* xtqueue;
	typedef bool (*xrt_xcall_proc)(xrt_call_frame* pFrame, xrt_call_result* pResult);
	typedef bool (*xset_each_proc)(const ptr pItem, ptr pArg);
	#define XRT_TYPE_KIND_INVALID		0u
	#define XRT_TYPE_KIND_NULL			1u
	#define XRT_TYPE_KIND_BOOL			2u
	#define XRT_TYPE_KIND_INT			3u
	#define XRT_TYPE_KIND_FLOAT			4u
	#define XRT_TYPE_KIND_STRING		5u
	#define XRT_TYPE_KIND_TIME			6u
	#define XRT_TYPE_KIND_POINT			7u
	#define XRT_TYPE_KIND_FUNCTION		8u
	#define XRT_TYPE_KIND_ARRAY			9u
	#define XRT_TYPE_KIND_LIST			10u
	#define XRT_TYPE_KIND_SET			11u
	#define XRT_TYPE_KIND_DICT			12u
	#define XRT_TYPE_KIND_RECORD		13u
	#define XRT_TYPE_KIND_HANDLE		14u
	#define XRT_TYPE_KIND_TYPE			15u
	#define XRT_TYPE_KIND_FUTURE		16u
	#define XRT_TYPE_KIND_CLASS		17u
	#define XRT_NATIVE_ABI_XCALL		0u
	#define XRT_NATIVE_ABI_CDECL		1u
	#define XRT_NATIVE_ABI_STDCALL		2u
	#define XRT_NATIVE_ABI_FASTCALL		3u
	#define XRT_FUNC_FLAG_VARARGS		0x00000001u
	#define XRT_FUNC_FLAG_KWARGS		0x00000002u
	#define XRT_FUNC_PARAM_OPTIONAL		0x00000001u
	#define XRT_CALL_RESULT_INLINE_COUNT	4u
	#define XRT_HANDLE_FLAG_OWNED		0x00000001u
	#define XRT_HANDLE_FLAG_BORROWED		0x00000002u
	#define XRT_HANDLE_FLAG_NULLABLE		0x00000004u
	#define XRT_TYPE_CONVERT_EXACT		0x00000001u
	#define XRT_TYPE_CONVERT_SAFE_WIDEN	0x00000002u
	#define XRT_TYPE_CONVERT_EXPLICIT	0x00000004u
	#ifndef XL6_XRT_CALL_TYPEDEF
	struct xrt_type_ops {
		void (*init)(ptr pObj);
		void (*copy)(ptr pDst, const ptr pSrc);
		void (*move)(ptr pDst, ptr pSrc);
		void (*drop)(ptr pObj);
		int (*compare)(const ptr pA, const ptr pB);
		uint64 (*hash)(const ptr pObj);
		str (*to_string)(const ptr pObj, uint32* pSize);
		xvalue (*box)(const ptr pObj, const xrt_type_desc* pType);
		bool (*unbox)(xvalue pVal, ptr pOut, const xrt_type_desc* pType);
		void (*clone)(ptr pDst, const ptr pSrc);
	};
	struct xrt_type_desc {
		uint32 TypeId;
		uint32 Kind;
		const char* Name;
		uint32 NameSize;
		const char* AbiName;
		uint32 AbiNameSize;
		size_t Size;
		size_t Align;
		const xrt_type_ops* Ops;
		const xrt_method_table* Methods;
		const void* Extra;
		const xrt_type_desc* BaseType;
	};
	struct xrt_generic_type {
		uint32 ArgCount;
		const xrt_type_desc* const* Args;
	};
	#endif
	struct xrt_method_desc {
		const char* Name;
		uint32 NameSize;
		const xrt_func_sig* Sig;
		ptr NativeEntry;
		uint32 NativeAbi;
		xrt_xcall_proc XCallEntry;
	};
	struct xrt_method_table {
		uint32 Count;
		const xrt_method_desc* Methods;
	};
	#if !defined(XL6_XRT_FUNC_SIG_TYPEDEF) && !defined(XL6_XRT_CALL_TYPEDEF)
	struct xrt_func_param {
		const char* Name;
		uint32 NameSize;
		const xrt_type_desc* Type;
		xvalue DefaultValue;
		uint32 Flags;
	};
	struct xrt_func_sig {
		uint64 TypeId;
		const char* Name;
		uint32 NameSize;
		uint32 ParamCount;
		const xrt_func_param* Params;
		uint32 ReturnCount;
		const xrt_type_desc* const* ReturnTypes;
		uint32 Flags;
	};
	#endif
	#ifndef XL6_XRT_CALL_TYPEDEF
	struct xrt_call_frame {
		xvalue Self;
		ptr Env;
		uint32 Argc;
		xvalue* Argv;
		xvalue Kwargs;
		xvalue Varargs;
		const xrt_func_sig* Sig;
	};
	struct xrt_call_result {
		uint32 Count;
		xvalue Values[XRT_CALL_RESULT_INLINE_COUNT];
		xvalue Overflow;
	};
	#endif
	struct xrt_callable {
		uint32 RefCount;
		const xrt_func_sig* Sig;
		ptr NativeEntry;
		uint32 NativeAbi;
		xrt_xcall_proc XCallEntry;
		ptr Env;
		void (*DropEnv)(ptr pEnv);
	};
	struct xrt_record_value {
		const xrt_type_desc* Type;
		xvalue Owner;
		uint32 Flags;
		uint32 Size;
		uint8 Data[1];
	};
	struct xrt_handle_value {
		const xrt_type_desc* Type;
		ptr Handle;
		uint32 Flags;
	};
	struct xrt_typed_array_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		xarray_struct Array;
	};
	struct xrt_typed_list_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		xlist_struct List;
	};
	struct xrt_typed_dict_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		xdict_struct Dict;
	};
	struct xset_struct {
		xrtOwnerInfo Owner;
		const xrt_type_desc* Type;
		uint32 ItemLength;
		uint32 Count;
		uint32 Capacity;
		uint8* State;
		uint64* Hash;
		uint8* Data;
	};
	struct xrt_typed_set_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		xset_struct Set;
	};
	#ifndef XRT_NO_TYPED_SPECIAL
	#define XRT_TYPED_QUEUE_SPSC 1u
	#define XRT_TYPED_QUEUE_MPSC 2u
	#define XRT_TYPED_QUEUE_MPMC 3u
	#define XRT_TYPED_QUEUE_WAIT 4u
	struct xrt_typed_stack_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		xdynstack_struct Stack;
	};
	struct xrt_typed_avl_tree_struct {
		volatile long RefCount;
		bool HeapOwned;
		const xrt_type_desc* KeyType;
		const xrt_type_desc* ValueType;
		uint32 KeyOffset;
		uint32 ValueOffset;
		uint32 EntrySize;
		uint32 Mode;
		xmutex Lock;
		xavltree_struct Tree;
	};
	struct xrt_typed_queue_struct {
		volatile long RefCount;
		bool HeapOwned;
		uint32 Kind;
		const xrt_type_desc* ItemType;
		uint32 ItemLength;
		ptr Queue;
	};
	#endif
	XXAPI const xrt_type_desc* xrtTypeNull();
	XXAPI const xrt_type_desc* xrtTypeBool();
	XXAPI const xrt_type_desc* xrtTypeBool32();
	XXAPI const xrt_type_desc* xrtTypeInt();
	XXAPI const xrt_type_desc* xrtTypeInt8();
	XXAPI const xrt_type_desc* xrtTypeUInt8();
	XXAPI const xrt_type_desc* xrtTypeInt16();
	XXAPI const xrt_type_desc* xrtTypeUInt16();
	XXAPI const xrt_type_desc* xrtTypeInt32();
	XXAPI const xrt_type_desc* xrtTypeUInt32();
	XXAPI const xrt_type_desc* xrtTypeInt64();
	XXAPI const xrt_type_desc* xrtTypeUInt64();
	XXAPI const xrt_type_desc* xrtTypeFloat();
	XXAPI const xrt_type_desc* xrtTypeFloat32();
	XXAPI const xrt_type_desc* xrtTypeFloat64();
	XXAPI const xrt_type_desc* xrtTypeString();
	XXAPI const xrt_type_desc* xrtTypeTime();
	XXAPI const xrt_type_desc* xrtTypePoint();
	XXAPI const xrt_type_desc* xrtTypeFunction();
	XXAPI const xrt_type_desc* xrtTypeFuture();
	XXAPI const xrt_type_desc* xrtTypeArray();
	XXAPI const xrt_type_desc* xrtTypeList();
	XXAPI const xrt_type_desc* xrtTypeSet();
	XXAPI const xrt_type_desc* xrtTypeDict();
	XXAPI const xrt_type_desc* xrtTypeRecord();
	XXAPI const xrt_type_desc* xrtTypeHandle();
	XXAPI const xrt_type_desc* xrtTypeType();
	XXAPI const xrt_type_desc* xrtTypeValue();
	XXAPI bool xrtTypeSame(const xrt_type_desc* pA, const xrt_type_desc* pB);
	XXAPI bool xrtTypeIsA(const xrt_type_desc* pType, const xrt_type_desc* pTarget);
	XXAPI const char* xrtTypeName(const xrt_type_desc* pType);
	XXAPI uint32 xrtTypeGenericArgCount(const xrt_type_desc* pType);
	XXAPI const xrt_type_desc* xrtTypeGenericArg(const xrt_type_desc* pType, uint32 iIndex);
	XXAPI const xrt_method_table* xrtTypeMethods(const xrt_type_desc* pType);
	XXAPI const xrt_method_desc* xrtTypeFindMethod(const xrt_type_desc* pType, const char* sName, uint32 iNameSize);
	XXAPI str xrtTypeToStringValue(const xrt_type_desc* pType, const ptr pObj, uint32* pSize);
	XXAPI bool xrtTypeCanWiden(const xrt_type_desc* pSrcType, const xrt_type_desc* pDstType);
	XXAPI bool xrtTypeCanConvert(const xrt_type_desc* pSrcType, const xrt_type_desc* pDstType, uint32 iFlags);
	XXAPI bool xrtTypeConvertValue(const xrt_type_desc* pDstType, ptr pDst, const xrt_type_desc* pSrcType, const ptr pSrc, uint32 iFlags);
	XXAPI bool xrtValueConvertTo(xvalue pVal, const xrt_type_desc* pDstType, ptr pDst, uint32 iFlags);
	XXAPI xvalue xrtTypeBoxValue(const xrt_type_desc* pType, const ptr pObj);
	XXAPI bool xrtTypeUnboxValue(const xrt_type_desc* pType, xvalue pVal, ptr pOut);
	XXAPI void xrtTypeDropValue(const xrt_type_desc* pType, ptr pObj);
	XXAPI uint64 xrtFuncSigTypeId(const xrt_func_sig* pSig);
	XXAPI void xrtCallResultInit(xrt_call_result* pResult);
	XXAPI void xrtCallResultUnit(xrt_call_result* pResult);
	XXAPI bool xrtCallResultSetValue(xrt_call_result* pResult, uint32 iIndex, xvalue pVal, bool bColloc);
	XXAPI xrt_callable* xrtCallableCreate(const xrt_func_sig* pSig, ptr pNativeEntry, uint32 iNativeAbi, xrt_xcall_proc pXCallEntry, ptr pEnv, void (*pDropEnv)(ptr pEnv));
	XXAPI const xrt_func_sig* xrtCallableSig(const xrt_callable* pCallable);
	XXAPI uint64 xrtCallableTypeId(const xrt_callable* pCallable);
	XXAPI void xrtCallableAddRef(xrt_callable* pCallable);
	XXAPI void xrtCallableUnref(xrt_callable* pCallable);
	XXAPI bool xrtCallableInvoke(xrt_callable* pCallable, xrt_call_frame* pFrame, xrt_call_result* pResult);
	/* ------------------------------------ Typed Container 函数库 ------------------------------------ */
	/*
		说明：
			Typed Container 是 XRT 对静态元素类型容器的统一薄封装。
			元素生命周期由 type desc / type ops 驱动，C 和 xlang 都可以直接使用。
	*/
	XXAPI xtarray xrtTypedArrayCreate(const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xtarray xrtTypedArrayRetain(xtarray pArray);
	XXAPI void xrtTypedArrayRelease(xtarray pArray);
	XXAPI xtarray xrtTypedArrayFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xvalue xrtTypedArrayToValue(xtarray pArray);
	XXAPI void xrtTypedArrayDestroy(xtarray pArray);
	XXAPI void xrtTypedArrayInit(xtarray pArray, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI void xrtTypedArrayUnit(xtarray pArray);
	XXAPI ptr xrtTypedArrayAppend(xtarray pArray, const ptr pItem);
	XXAPI bool xrtTypedArrayAppendValue(xtarray pArray, xvalue pVal);
	XXAPI ptr xrtTypedArrayInsert(xtarray pArray, uint32 iPos, const ptr pItem);
	XXAPI bool xrtTypedArrayInsertValue(xtarray pArray, uint32 iPos, xvalue pVal);
	XXAPI bool xrtTypedArraySet(xtarray pArray, uint32 iPos, const ptr pItem);
	XXAPI bool xrtTypedArraySetValue(xtarray pArray, uint32 iPos, xvalue pVal);
	XXAPI ptr xrtTypedArrayGet(xtarray pArray, uint32 iPos);
	XXAPI bool xrtTypedArrayRemove(xtarray pArray, uint32 iPos, uint32 iCount);
	XXAPI bool xrtTypedArrayTake(xtarray pArray, uint32 iPos, ptr pOut);
	XXAPI xvalue xrtTypedArrayTakeValue(xtarray pArray, uint32 iPos);
	XXAPI bool xrtTypedArrayPop(xtarray pArray, ptr pOut);
	XXAPI xvalue xrtTypedArrayPopValue(xtarray pArray);
	XXAPI bool xrtTypedArrayClear(xtarray pArray);
	XXAPI bool xrtTypedArrayReverse(xtarray pArray);
	XXAPI int64 xrtTypedArrayIndexOf(xtarray pArray, const ptr pItem);
	XXAPI int64 xrtTypedArrayIndexOfValue(xtarray pArray, xvalue pVal);
	XXAPI bool xrtTypedArrayContains(xtarray pArray, const ptr pItem);
	XXAPI bool xrtTypedArrayContainsValue(xtarray pArray, xvalue pVal);
	XXAPI bool xrtTypedArrayExtend(xtarray pTarget, xtarray pSource);
	XXAPI xtarray xrtTypedArrayCloneEx(xtarray pArray, uint32 iMode);
	XXAPI xtarray xrtTypedArrayMoveToShared(xtarray pArray);
	XXAPI xtarray xrtTypedArrayClone(xtarray pArray);
	XXAPI xtarray xrtTypedArrayConcat(xtarray pLeft, xtarray pRight);
	XXAPI bool xrtTypedArrayEquals(xtarray pLeft, xtarray pRight);
	XXAPI uint32 xrtTypedArrayCount(xtarray pArray);
	XXAPI const xrt_type_desc* xrtTypedArrayItemType(xtarray pArray);
	XXAPI xtlist xrtTypedListCreate(const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xtlist xrtTypedListRetain(xtlist pList);
	XXAPI void xrtTypedListRelease(xtlist pList);
	XXAPI xtlist xrtTypedListFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xvalue xrtTypedListToValue(xtlist pList);
	XXAPI void xrtTypedListDestroy(xtlist pList);
	XXAPI void xrtTypedListInit(xtlist pList, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI void xrtTypedListUnit(xtlist pList);
	XXAPI ptr xrtTypedListSet(xtlist pList, int64 iKey, const ptr pItem);
	XXAPI bool xrtTypedListSetValue(xtlist pList, int64 iKey, xvalue pVal);
	XXAPI ptr xrtTypedListGet(xtlist pList, int64 iKey);
	XXAPI ptr xrtTypedListItemAt(xtlist pList, uint32 iIndex, int64* pKey);
	XXAPI bool xrtTypedListRemove(xtlist pList, int64 iKey);
	XXAPI bool xrtTypedListTake(xtlist pList, int64 iKey, ptr pOut);
	XXAPI xvalue xrtTypedListTakeValue(xtlist pList, int64 iKey);
	XXAPI bool xrtTypedListClear(xtlist pList);
	XXAPI bool xrtTypedListContains(xtlist pList, const ptr pItem);
	XXAPI bool xrtTypedListContainsValue(xtlist pList, xvalue pVal);
	XXAPI bool xrtTypedListMerge(xtlist pTarget, xtlist pSource);
	XXAPI xtlist xrtTypedListCloneEx(xtlist pList, uint32 iMode);
	XXAPI xtlist xrtTypedListMoveToShared(xtlist pList);
	XXAPI xtlist xrtTypedListClone(xtlist pList);
	XXAPI bool xrtTypedListEquals(xtlist pLeft, xtlist pRight);
	XXAPI uint32 xrtTypedListCount(xtlist pList);
	XXAPI const xrt_type_desc* xrtTypedListItemType(xtlist pList);
	XXAPI xtset xrtTypedSetCreate(const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xtset xrtTypedSetRetain(xtset pSet);
	XXAPI void xrtTypedSetRelease(xtset pSet);
	XXAPI xtset xrtTypedSetFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xvalue xrtTypedSetToValue(xtset pSet);
	XXAPI void xrtTypedSetDestroy(xtset pSet);
	XXAPI void xrtTypedSetInit(xtset pSet, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI void xrtTypedSetUnit(xtset pSet);
	XXAPI bool xrtTypedSetAdd(xtset pSet, const ptr pItem);
	XXAPI bool xrtTypedSetAddValue(xtset pSet, xvalue pVal);
	XXAPI ptr xrtTypedSetGet(xtset pSet, const ptr pItem);
	XXAPI bool xrtTypedSetExists(xtset pSet, const ptr pItem);
	XXAPI bool xrtTypedSetRemove(xtset pSet, const ptr pItem);
	XXAPI bool xrtTypedSetExistsValue(xtset pSet, xvalue pVal);
	XXAPI bool xrtTypedSetRemoveValue(xtset pSet, xvalue pVal);
	XXAPI ptr xrtTypedSetItemAt(xtset pSet, uint32 iIndex);
	XXAPI bool xrtTypedSetClear(xtset pSet);
	XXAPI bool xrtTypedSetMerge(xtset pTarget, xtset pSource);
	XXAPI xtset xrtTypedSetCloneEx(xtset pSet, uint32 iMode);
	XXAPI xtset xrtTypedSetMoveToShared(xtset pSet);
	XXAPI xtset xrtTypedSetClone(xtset pSet);
	XXAPI xtset xrtTypedSetUnion(xtset pLeft, xtset pRight);
	XXAPI xtset xrtTypedSetIntersection(xtset pLeft, xtset pRight);
	XXAPI xtset xrtTypedSetDifference(xtset pLeft, xtset pRight);
	XXAPI xtset xrtTypedSetSymmetricDifference(xtset pLeft, xtset pRight);
	XXAPI bool xrtTypedSetIsSubset(xtset pLeft, xtset pRight, bool bProper);
	XXAPI bool xrtTypedSetIsSuperset(xtset pLeft, xtset pRight, bool bProper);
	XXAPI bool xrtTypedSetEquals(xtset pLeft, xtset pRight);
	XXAPI uint32 xrtTypedSetCount(xtset pSet);
	XXAPI const xrt_type_desc* xrtTypedSetItemType(xtset pSet);
	XXAPI xtdict xrtTypedDictCreate(const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xtdict xrtTypedDictRetain(xtdict pDict);
	XXAPI void xrtTypedDictRelease(xtdict pDict);
	XXAPI xtdict xrtTypedDictFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI xvalue xrtTypedDictToValue(xtdict pDict);
	XXAPI void xrtTypedDictDestroy(xtdict pDict);
	XXAPI void xrtTypedDictInit(xtdict pDict, const xrt_type_desc* pItemType, uint32 iMode);
	XXAPI void xrtTypedDictUnit(xtdict pDict);
	XXAPI ptr xrtTypedDictSet(xtdict pDict, const ptr sKey, uint32 iKeyLen, const ptr pItem);
	XXAPI bool xrtTypedDictSetValue(xtdict pDict, const ptr sKey, uint32 iKeyLen, xvalue pVal);
	XXAPI ptr xrtTypedDictGet(xtdict pDict, const ptr sKey, uint32 iKeyLen);
	XXAPI ptr xrtTypedDictItemAt(xtdict pDict, uint32 iIndex, const char** psKey, uint32* piKeyLen);
	XXAPI bool xrtTypedDictExists(xtdict pDict, const ptr sKey, uint32 iKeyLen);
	XXAPI bool xrtTypedDictRemove(xtdict pDict, const ptr sKey, uint32 iKeyLen);
	XXAPI bool xrtTypedDictTake(xtdict pDict, const ptr sKey, uint32 iKeyLen, ptr pOut);
	XXAPI xvalue xrtTypedDictTakeValue(xtdict pDict, const ptr sKey, uint32 iKeyLen);
	XXAPI bool xrtTypedDictClear(xtdict pDict);
	XXAPI bool xrtTypedDictMerge(xtdict pTarget, xtdict pSource);
	XXAPI xtdict xrtTypedDictCloneEx(xtdict pDict, uint32 iMode);
	XXAPI xtdict xrtTypedDictMoveToShared(xtdict pDict);
	XXAPI xtdict xrtTypedDictClone(xtdict pDict);
	XXAPI bool xrtTypedDictEquals(xtdict pLeft, xtdict pRight);
	XXAPI uint32 xrtTypedDictCount(xtdict pDict);
	XXAPI const xrt_type_desc* xrtTypedDictItemType(xtdict pDict);
	#ifndef XRT_NO_TYPED_SPECIAL
	XXAPI xtstack xrtTypedStackCreate(const xrt_type_desc* pItemType);
	XXAPI xtstack xrtTypedStackRetain(xtstack pStack);
	XXAPI void xrtTypedStackRelease(xtstack pStack);
	XXAPI bool xrtTypedStackPush(xtstack pStack, const ptr pItem);
	XXAPI bool xrtTypedStackPop(xtstack pStack, ptr pOut);
	XXAPI bool xrtTypedStackPeek(xtstack pStack, ptr pOut);
	XXAPI void xrtTypedStackClear(xtstack pStack);
	XXAPI uint32 xrtTypedStackCount(xtstack pStack);
	XXAPI const xrt_type_desc* xrtTypedStackItemType(xtstack pStack);
	XXAPI xtavltree xrtTypedAvlTreeCreate(const xrt_type_desc* pKeyType, const xrt_type_desc* pValueType, uint32 iMode);
	XXAPI xtavltree xrtTypedAvlTreeRetain(xtavltree pTree);
	XXAPI void xrtTypedAvlTreeRelease(xtavltree pTree);
	XXAPI bool xrtTypedAvlTreeSet(xtavltree pTree, const ptr pKey, const ptr pValue);
	XXAPI bool xrtTypedAvlTreeGet(xtavltree pTree, const ptr pKey, ptr pOut);
	XXAPI bool xrtTypedAvlTreeContains(xtavltree pTree, const ptr pKey);
	XXAPI bool xrtTypedAvlTreeRemove(xtavltree pTree, const ptr pKey);
	XXAPI void xrtTypedAvlTreeClear(xtavltree pTree);
	XXAPI uint32 xrtTypedAvlTreeCount(xtavltree pTree);
	XXAPI xtarray xrtTypedAvlTreeKeys(xtavltree pTree, uint32 iMode);
	XXAPI xtarray xrtTypedAvlTreeValues(xtavltree pTree, uint32 iMode);
	XXAPI const xrt_type_desc* xrtTypedAvlTreeKeyType(xtavltree pTree);
	XXAPI const xrt_type_desc* xrtTypedAvlTreeValueType(xtavltree pTree);
	XXAPI xtqueue xrtTypedQueueCreate(uint32 iKind, const xrt_type_desc* pItemType, uint32 iCapacity);
	XXAPI xtqueue xrtTypedQueueRetain(xtqueue pQueue);
	XXAPI void xrtTypedQueueRelease(xtqueue pQueue);
	XXAPI xqueue_result xrtTypedQueueTryPush(xtqueue pQueue, const ptr pItem);
	XXAPI xqueue_result xrtTypedQueueTryPop(xtqueue pQueue, ptr pOut);
	XXAPI xqueue_result xrtTypedQueuePop(xtqueue pQueue, ptr pOut);
	XXAPI xqueue_result xrtTypedQueuePopTimeout(xtqueue pQueue, ptr pOut, uint32 iTimeoutMs);
	XXAPI uint32 xrtTypedQueueApproxCount(xtqueue pQueue);
	XXAPI void xrtTypedQueueClose(xtqueue pQueue);
	XXAPI bool xrtTypedQueueIsClosed(xtqueue pQueue);
	XXAPI bool xrtTypedQueueIsDrained(xtqueue pQueue);
	XXAPI const xrt_type_desc* xrtTypedQueueItemType(xtqueue pQueue);
	#endif
	/* ------------------------------------ Set 函数库 ------------------------------------ */
	/*
		说明：
			Set 是 XRT 的独立集合容器。
			它依赖 type desc / type ops 表达元素类型、哈希、比较和生命周期。
			调用方不需要知道底层使用哈希表、树或其他结构。
	*/
	XXAPI xset xrtSetCreate(const xrt_type_desc* pType, uint32 iMode);
	XXAPI void xrtSetDestroy(xset pSet);
	XXAPI void xrtSetInit(xset pSet, const xrt_type_desc* pType, uint32 iMode);
	XXAPI void xrtSetUnit(xset pSet);
	XXAPI bool xrtSetAdd(xset pSet, const ptr pItem);
	XXAPI bool xrtSetExists(xset pSet, const ptr pItem);
	XXAPI bool xrtSetRemove(xset pSet, const ptr pItem);
	XXAPI ptr xrtSetGet(xset pSet, const ptr pItem);
	XXAPI ptr xrtSetItemAt(xset pSet, uint32 iIndex);
	XXAPI uint32 xrtSetCount(xset pSet);
	XXAPI void xrtSetWalk(xset pSet, xset_each_proc procEach, ptr pArg);
	XXAPI xset xrtSetClone(xset pSet);
	XXAPI bool xrtSetMerge(xset pTarget, xset pSource);
	XXAPI xset xrtSetUnion(xset pLeft, xset pRight);
	XXAPI xset xrtSetIntersection(xset pLeft, xset pRight);
	XXAPI xset xrtSetDifference(xset pLeft, xset pRight);
	XXAPI xset xrtSetSymmetricDifference(xset pLeft, xset pRight);
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
	
	// Value 标准数据类
	struct xvalue_struct {
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
			xrt_callable* vCallable;
			xparray vArray;
			xlist vList;
			xavltree vColl;
			xset vSet;
			xdict vTable;
			ptr vStruct;
			ptr vCustom;
		};
		union {
			struct xvalue_struct* vFuncEnv;
			ptr vExtra;
			const xrt_type_desc* vTypeDesc;
		};
	};
	
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
	// 初始化头部
	static inline void xvoInitHeader(xvalue pVal, uint32 iType, bool bStatic, bool bShared, uint32 iRefCount)
	{
		if ( pVal == NULL ) {
			return;
		}
		pVal->Header = XVO_HEADER_INIT(iType, bStatic, bShared, iRefCount);
	}
	// 初始化带所有权模式的值头部
	static inline void xvoInitOwnedHeader_Inline(xvalue pVal, uint32 iType, uint32 iMode)
	{
		xvoInitHeader(pVal, iType, FALSE, iMode == XRT_OBJMODE_SHARED, 1);
	}
	// 判断值头部是否标记为共享
	static inline bool xvoIsShared_Inline(xvalue pVal)
	{
		if ( pVal == NULL ) {
			return FALSE;
		}
		return (pVal->Header & XVO_HEADER_SHARED_MASK) != 0;
	}
	// 将值头部切换为共享模式
	static inline void xvoSetShared_Inline(xvalue pVal)
	{
		if ( pVal == NULL || pVal->IsStatic ) {
			return;
		}
		pVal->Header |= XVO_HEADER_SHARED_MASK;
	}
	// 判断所有权是否已经真正进入共享状态
	static inline bool xrtOwnerIsRealShared(const xrtOwnerInfo* pOwner)
	{
		if ( pOwner == NULL ) {
			return FALSE;
		}
		return pOwner->iMode == XRT_OBJMODE_SHARED && (pOwner->iFlags & XRT_OBJFLAG_SHARED_PENDING) == 0;
	}
	// 将值及其根对象校验后标记为共享
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
				if ( pVal->Size == sizeof(xset_struct) ) {
					if ( !xrtOwnerIsRealShared(&pVal->vSet->Owner) ) {
						xrtSetError((str)"set value requires a real shared set root.", FALSE);
						return FALSE;
					}
				} else {
					if ( !xrtOwnerIsRealShared(&pVal->vColl->Owner) ) {
						xrtSetError((str)"coll value requires a real shared coll root.", FALSE);
						return FALSE;
					}
				}
				break;
			default:
				break;
		}
		xvoSetShared_Inline(pVal);
		return TRUE;
	}
	// 在共享容器写入前准备值的共享状态
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
	// 创建独立外壳并共享容器 backing；标量沿用普通引用语义
	XXAPI xvalue xvoShare(xvalue pVal);
	// 确保容器 backing 独占；共享时在本次写入前完成分离
	XXAPI bool xvoEnsureUnique(xvalue pVal);
	// 判断动态值是否已经进入共享所有权模式
	XXAPI bool xvoIsShared(xvalue pVal);
	// 减少值的引用计数
	XXAPI void xvoUnref(xvalue pVal);
	// 内联增加值的引用计数
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
	// 创建布尔值
	XXAPI xvalue xvoCreateBool(bool bVal);
	// 创建整数
	XXAPI xvalue xvoCreateInt(int64 iVal);
	// 创建浮点数
	XXAPI xvalue xvoCreateFloat(double fVal);
	// 创建文本
	XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);
	// 创建时间值
	XXAPI xvalue xvoCreateTime(xtime tVal);
	// 根据日期时间字段创建时间值
	XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
	// 创建指针值
	XXAPI xvalue xvoCreatePoint(ptr point);
	// 创建函数值
	XXAPI xvalue xvoCreateFunc(xfunction pFunc);
	// 创建带环境的函数值
	XXAPI xvalue xvoCreateFuncEx(xfunction pFunc, xvalue pEnv);
	// 创建 callable 函数值
	XXAPI xvalue xvoCreateCallable(xrt_callable* pCallable, bool bColloc);
	// 创建 record/class 动态值
	XXAPI xvalue xvoCreateRecord(const xrt_type_desc* pType, const ptr pData);
	XXAPI xvalue xvoCreateRecordMove(const xrt_type_desc* pType, ptr pData);
	// 创建具有对象身份的 class；payload 由类型描述初始化并由引用计数管理
	XXAPI xvalue xvoCreateObject(const xrt_type_desc* pType);
	// 创建 handle/custom 动态值
	XXAPI xvalue xvoCreateHandle(const xrt_type_desc* pType, ptr pHandle, uint32 iFlags);
	// 创建数组
	XXAPI xvalue xvoCreateArray();
	// 创建数组扩展
	XXAPI xvalue xvoCreateArrayEx(uint32 iMode);
	// 创建列表
	XXAPI xvalue xvoCreateList();
	// 创建列表扩展
	XXAPI xvalue xvoCreateListEx(uint32 iMode);
	// 创建集合值
	XXAPI xvalue xvoCreateColl();
	// 创建基于 XRT set 的集合值
	XXAPI xvalue xvoCreateSet();
	// 创建带所有权模式的集合值
	XXAPI xvalue xvoCreateCollEx(uint32 iMode);
	// 创建基于 XRT set 且带所有权模式的集合值
	XXAPI xvalue xvoCreateSetEx(uint32 iMode);
	// 创建表值
	XXAPI xvalue xvoCreateTable();
	// 创建带所有权模式的表值
	XXAPI xvalue xvoCreateTableEx(uint32 iMode);
	// 创建分类
	XXAPI xvalue xvoCreateClass(uint32 iSize);
	// 创建自定义对象值
	XXAPI xvalue xvoCreateCustom(ptr pObj);
	// 使用可变参数创建数组字面量
	XXAPI xvalue xvoCreateArrayArgs(uint32 iCount, ...);
	// 使用可变参数创建列表字面量，参数为 index/value 成对出现
	XXAPI xvalue xvoCreateListArgs(uint32 iCount, ...);
	// 使用可变参数创建集合字面量
	XXAPI xvalue xvoCreateSetArgs(uint32 iCount, ...);
	// 使用可变参数创建表字面量，参数为 key/value 成对出现
	XXAPI xvalue xvoCreateTableArgs(uint32 iCount, ...);
	
	// 读取值
	XXAPI bool xvoGetBool(xvalue pVal);
	// 获取整数
	XXAPI int64 xvoGetInt(xvalue pVal);
	// 获取浮点数
	XXAPI double xvoGetFloat(xvalue pVal);
	// 获取文本
	XXAPI str xvoGetText(xvalue pVal);
	// 获取可释放的字符串副本
	XXAPI str xvoToString(xvalue pVal, uint32* pSize);
	// 获取时间值
	XXAPI xtime xvoGetTime(xvalue pVal);
	// 获取指针值
	XXAPI ptr xvoGetPoint(xvalue pVal);
	// 获取函数值
	XXAPI xfunction xvoGetFunc(xvalue pVal);
	// 获取 callable 函数对象
	XXAPI xrt_callable* xvoGetCallable(xvalue pVal);
	// 获取 callable 函数签名
	XXAPI const xrt_func_sig* xvoCallableSig(xvalue pVal);
	// 获取函数值环境
	XXAPI xvalue xvoGetFuncEnv(xvalue pVal);
	// 获取动态值的精确运行时类型
	XXAPI const xrt_type_desc* xvoTypeDesc(xvalue pVal);
	// 设置动态值的精确运行时类型
	XXAPI bool xvoSetTypeDesc(xvalue pVal, const xrt_type_desc* pType);
	// 调用 function/callable 动态值
	XXAPI bool xvoInvoke(xvalue pFunc, xrt_call_frame* pFrame, xrt_call_result* pResult);
	// 获取数组
	XXAPI xparray xvoGetArray(xvalue pVal);
	XXAPI xparray xvoGetMutableArray(xvalue pVal);
	// 获取列表
	XXAPI xlist xvoGetList(xvalue pVal);
	XXAPI xlist xvoGetMutableList(xvalue pVal);
	// 获取集合值
	XXAPI xavltree xvoGetColl(xvalue pVal);
	XXAPI xavltree xvoGetMutableColl(xvalue pVal);
	// 获取基于 XRT set 的集合对象
	XXAPI xset xvoGetSet(xvalue pVal);
	XXAPI xset xvoGetMutableSet(xvalue pVal);
	// 获取表值
	XXAPI xdict xvoGetTable(xvalue pVal);
	XXAPI xdict xvoGetMutableTable(xvalue pVal);
	// 获取分类
	XXAPI ptr xvoGetClass(xvalue pVal);
	// 获取 record/class 数据指针
	XXAPI ptr xvoGetRecordData(xvalue pVal);
	// 根据 record/class 数据指针获取非持有的 xvalue 外壳
	XXAPI xvalue xvoGetRecordOwner(const ptr pData);
	// 获取自定义对象值
	XXAPI ptr xvoGetCustom(xvalue pVal);
	// 获取 handle/custom 句柄
	XXAPI ptr xvoGetHandleData(xvalue pVal);
	// 获取 typed handle 的可写句柄槽；裸 custom 或其他类型返回 NULL
	XXAPI ptr* xvoGetHandleSlot(xvalue pVal);
	
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
	// 删除数组
	XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);
	// Take one array item without unref; ownership of the stored value reference is transferred to caller.
	XXAPI xvalue xvoArrayTakeValue(xvalue pArr, uint32 index);
	// Pop the last array item without unref; ownership of the stored value reference is transferred to caller.
	XXAPI xvalue xvoArrayPopValue(xvalue pArr);
	// 获取数组成员数量
	XXAPI uint32 xvoArrayItemCount(xvalue pArr);
	// 清除数组
	XXAPI bool xvoArrayClear(xvalue pArr);
	// 分配数组
	XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count);
	// 对数组成员排序
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
	// 删除列表
	XXAPI bool xvoListRemove(xvalue pList, int64 index);
	// Take one list item without unref; ownership of the stored value reference is transferred to caller.
	XXAPI xvalue xvoListTakeValue(xvalue pList, int64 index);
	// 获取列表成员数量
	XXAPI uint32 xvoListItemCount(xvalue pList);
	// 清除列表
	XXAPI bool xvoListClear(xvalue pList);
	// 设置列表父节点
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
	// 从集合中删除一个值
	XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal);
	// 获取集合成员数量
	XXAPI uint32 xvoCollItemCount(xvalue pColl);
	// 清空集合
	XXAPI bool xvoCollClear(xvalue pColl);
	// 基于 XRT set 的集合操作
	XXAPI bool xvoSetAddValue(xvalue pSet, xvalue pVal, bool bColloc);
	XXAPI bool xvoSetExistsValue(xvalue pSet, xvalue pVal);
	XXAPI bool xvoSetRemoveValue(xvalue pSet, xvalue pVal);
	XXAPI xvalue xvoSetGetValueAt(xvalue pSet, uint32 iIndex);
	XXAPI uint32 xvoSetItemCount(xvalue pSet);
	XXAPI bool xvoSetClear(xvalue pSet);
	// 设置集合父节点
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
	// 从表中删除一个键
	XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl);
	// Take one table value without unref; ownership of the stored value reference is transferred to caller.
	XXAPI xvalue xvoTableTakeValue(xvalue pTbl, str key, uint32 kl);
	// 获取表成员数量
	XXAPI uint32 xvoTableItemCount(xvalue pTbl);
	// 清空表
	XXAPI bool xvoTableClear(xvalue pTbl);
	// 按插入顺序遍历表；没有顺序信息时退回底层字典遍历
	XXAPI bool xvoTableWalk(xvalue pTbl, Dict_EachProc procEach, ptr pArg);
	// 设置表父节点
	XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
	
	// 类型操作
	XXAPI bool xvoIsNull(xvalue pVal);
	// 获取值类型
	XXAPI int xvoType(xvalue pVal);
	XXAPI str xvoTypeName(int iType);
	XXAPI bool xvoIsBool(xvalue pVal);
	XXAPI bool xvoIsInt(xvalue pVal);
	XXAPI bool xvoIsFloat(xvalue pVal);
	XXAPI bool xvoIsText(xvalue pVal);
	XXAPI bool xvoIsTime(xvalue pVal);
	XXAPI bool xvoIsPoint(xvalue pVal);
	XXAPI bool xvoIsFunc(xvalue pVal);
	XXAPI bool xvoIsArray(xvalue pVal);
	XXAPI bool xvoIsList(xvalue pVal);
	XXAPI bool xvoIsColl(xvalue pVal);
	XXAPI bool xvoIsTable(xvalue pVal);
	XXAPI bool xvoIsClass(xvalue pVal);
	XXAPI bool xvoIsCustom(xvalue pVal);
	XXAPI bool xvoIsNumber(xvalue pVal);
	XXAPI bool xvoIsBasic(xvalue pVal);
	XXAPI bool xvoIsContainer(xvalue pVal);
	XXAPI bool xvoCanCompareBasic(xvalue pLeft, xvalue pRight);
	XXAPI int xvoBasicCompare(xvalue pLeft, xvalue pRight);
	XXAPI bool xvoBasicEqual(xvalue pLeft, xvalue pRight);
	// 判断 xvalue 的语言级真假值
	XXAPI bool xvoValueTruthy(xvalue pVal);
	// 安全按整数下标读取数组或列表
	XXAPI xvalue xvoIndexGetI64(xvalue pVal, int64 iIndex);
	XXAPI xvalue xvoIndexGetMutableI64(xvalue pVal, int64 iIndex);
	// 按整数下标写入数组或列表；数组负下标从尾部计数，列表保留稀疏 int64 键
	XXAPI bool xvoIndexSetI64(xvalue pVal, int64 iIndex, xvalue pItem, bool bColloc);
	// 安全按字符串键读取表
	XXAPI xvalue xvoIndexGetKey(xvalue pVal, str sKey, uint32 iKeySize);
	XXAPI xvalue xvoIndexGetMutableKey(xvalue pVal, str sKey, uint32 iKeySize);
	// 安全按动态键读取容器
	XXAPI xvalue xvoIndexGetValue(xvalue pVal, xvalue pKey);
	XXAPI xvalue xvoIndexGetMutableValue(xvalue pVal, xvalue pKey);
	// 判断容器或字符串是否包含指定值
	XXAPI bool xvoValueContains(xvalue pValues, xvalue pItem);
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
	// 按对象模式浅拷贝；共享模式会递归复制复合值，保证整张值图可安全发布
	XXAPI xvalue xvoCopyEx(xvalue pVal, uint32 iMode);
	
	// 深拷贝
	XXAPI xvalue xvoDeepCopy(xvalue pVal);
	// 按对象模式深拷贝
	XXAPI xvalue xvoDeepCopyEx(xvalue pVal, uint32 iMode);
	// 将值的所有权发布到共享环境；成功后调用方不再持有原本地值
	XXAPI xvalue xvoMoveToShared(xvalue pVal);
	
	// 输出 xte Value 的结构和值
	XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey);
	// 按 xlang print 语义输出 xvalue
	XXAPI void xvoPrint(xvalue pVal, bool bNewLine);
	
	
	
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
	// 输出 JSON 布尔值
	static inline int xrtJsonPrintBool(json_sax_print_hd handle, json_string_t *jkey, bool value) { return xrtJsonPrintValue(handle, JSON_BOOL, jkey, &value); }
	// 输出 JSON 整数
	static inline int xrtJsonPrintInt(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return xrtJsonPrintValue(handle, JSON_INT, jkey, &value); }
	// 输出 JSON 十六进制整数
	static inline int xrtJsonPrintHex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return xrtJsonPrintValue(handle, JSON_HEX, jkey, &value); }
	// 输出 JSON 整数 64
	static inline int xrtJsonPrintInt64(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return xrtJsonPrintValue(handle, JSON_LINT, jkey, &value); }
	// 输出 JSON hex 64
	static inline int xrtJsonPrintHex64(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return xrtJsonPrintValue(handle, JSON_LHEX, jkey, &value); }
	// 输出 JSON 浮点数
	static inline int xrtJsonPrintDouble(json_sax_print_hd handle, json_string_t *jkey, double value) { return xrtJsonPrintValue(handle, JSON_DOUBLE, jkey, &value); }
	// 输出 JSON 字符串
	static inline int xrtJsonPrintString(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return xrtJsonPrintValue(handle, JSON_STRING, jkey, value); }
	// 输出 JSON 数组
	static inline int xrtJsonPrintArray(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return xrtJsonPrintValue(handle, JSON_ARRAY, jkey, &value); }
	// 输出 JSON 对象开始 / 结束标记
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
	// 验证 JSON
	XXAPI bool xrtJsonValid(str sText, size_t iSize);
	// 解析 JSON 文件
	XXAPI xvalue xrtParseJSON_File(str sFile);
	
	// 将 xvalue 转换为 JSON
	XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);
	// 将 xvalue 格式化为 JSON 并写入文件
	XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);
	
	
	
	#if !defined(XRT_NO_XSON)
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
	// 解析 XSON 扩展
	XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags);
	// 验证 XSON 扩展
	XXAPI bool xrtXsonValid(str sText, size_t iSize, uint32 iFlags);
	// 解析 XSON 文件
	XXAPI xvalue xrtParseXSON_File(str sFile);
	// 解析 XSON 文件扩展
	XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags);
	
	// 将 xvalue 转换为 XSON
	XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize);
	// 将 xvalue 格式化为 XSON 并写入文件
	XXAPI int xrtStringifyXSON_File(str sFile, xvalue varVal, int bFormat, uint32 iFlags);
	#endif
	
	
	
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
	// 创建引擎
	XXAPI xteengine xteCreateEngine(void);
	// 销毁引擎
	XXAPI void xteDestroyEngine(xteengine hEngine);
	// 注册模板引擎内建语句
	XXAPI int xteRegisterBuiltinStatements(xteengine hEngine);
	// 注册自定义模板语句
	XXAPI int xteRegisterStatement(xteengine hEngine, const XTE_StatementDef* pDef);
	// 注册自定义模板函数
	XXAPI int xteRegisterFunction(xteengine hEngine, const XTE_FunctionDef* pDef);
	// 解析扩展
	XXAPI xtetemplate xteParseEx(xteengine hEngine, const char* sText, size_t iSize, const XTE_ParseOptions* pOptions, XTE_Error* pError);
	// 解析
	XXAPI xtetemplate xteParse(const char* sText, size_t iSize, const char* sBracket);
	// 销毁模板
	XXAPI void xteDestroyTemplate(xtetemplate hTemplate);
	// 释放模板解析阶段分配的资源
	XXAPI void xteParseFree(xtetemplate hTemplate);
	// 按指定选项渲染模板
	XXAPI int xteRenderEx(xtetemplate hTemplate, const XTE_RenderOptions* pOptions, XTE_Error* pError);
	// 构建
	XXAPI char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, xdict pIncludeMap, size_t* pRetSize);
	// 解析路径
	XXAPI xvalue xteResolvePath(const char* sPath, size_t iPathSize, xvalue pCurrent, xvalue pRoot, xvalue pLocal, xvalue pGlobal);
	// 统计模板 get 节点
	XXAPI uint32 xteTemplateGetNodeCount(xtetemplate hTemplate);
	// 获取模板表达式数量
	XXAPI uint32 xteTemplateGetExprCount(xtetemplate hTemplate);
	// 获取模板参数项数量
	XXAPI uint32 xteTemplateGetArgCount(xtetemplate hTemplate);
	// 获取模板字符串内存池大小
	XXAPI uint32 xteTemplateGetStringPoolSize(xtetemplate hTemplate);
	// 获取模板根节点范围
	XXAPI XTE_NodeSpan xteTemplateGetRootSpan(xtetemplate hTemplate);
	// 获取模板节点
	XXAPI const XTE_Node* xteTemplateGetNode(xtetemplate hTemplate, uint32 iIndex);
	// 获取模板表达式节点
	XXAPI const XTE_ExprNode* xteTemplateGetExpr(xtetemplate hTemplate, uint32 iIndex);
	// 获取模板参数项
	XXAPI const XTE_ArgItem* xteTemplateGetArg(xtetemplate hTemplate, uint32 iIndex);
	// 获取模板字符串
	XXAPI const char* xteTemplateGetString(xtetemplate hTemplate, uint32 iOff);
	// 获取参数列表中的参数数量
	XXAPI uint32 xteArgCount(const XTE_ArgList* pArgs);
	// 获取参数列表中指定位置的参数
	XXAPI const XTE_ArgItem* xteArgAt(const XTE_ArgList* pArgs, uint32 iIndex);
	// 按名称查找参数列表中的参数
	XXAPI const XTE_ArgItem* xteFindNamedArg(const XTE_ArgList* pArgs, const char* sName, size_t iNameSize);
	// 获取参数名称文本
	XXAPI const char* xteArgNameText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	// 获取参数原始文本
	XXAPI const char* xteArgRawText(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	// 获取参数表达式类型
	XXAPI uint32 xteArgExprType(const XTE_ArgList* pArgs, const XTE_ArgItem* pArg);
	// 求值参数并返回 xvalue
	XXAPI xvalue xteEvalArgValue(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
	// 求值参数并按布尔值读取
	XXAPI int xteEvalArgBool(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
	// 求值参数并按整数读取
	XXAPI int xteEvalArgInt(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
	// 求值参数并按浮点数读取
	XXAPI int xteEvalArgFloat(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
	// 求值参数并按文本读取
	XXAPI char* xteEvalArgText(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
	// 严格按布尔类型求值参数
	XXAPI int xteEvalArgBoolStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int* pOut);
	// 严格按整数类型求值参数
	XXAPI int xteEvalArgIntStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, int64* pOut);
	// 严格按浮点类型求值参数
	XXAPI int xteEvalArgFloatStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg, double* pOut);
	// 严格按文本类型求值参数
	XXAPI char* xteEvalArgTextStrict(XTE_RenderCtx* pCtx, const XTE_ArgItem* pArg);
	// 在语句解析阶段要求指定位置参数存在
	XXAPI const XTE_ArgItem* xteStmtParseRequireArg(XTE_StmtParseCtx* pCtx, uint32 iIndex, const char* sDesc);
	// 在语句解析阶段要求指定名称参数存在
	XXAPI const XTE_ArgItem* xteStmtParseRequireNamedArg(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	// 在语句解析阶段要求参数表达式类型匹配
	XXAPI const XTE_ArgItem* xteStmtParseRequireExprType(XTE_StmtParseCtx* pCtx, uint32 iIndex, uint32 iExprType, const char* sDesc);
	// 在语句解析阶段要求命名参数表达式类型匹配
	XXAPI const XTE_ArgItem* xteStmtParseRequireNamedExprType(XTE_StmtParseCtx* pCtx, const char* sName, size_t iNameSize, uint32 iExprType, const char* sDesc);
	// 在语句渲染阶段要求指定位置参数存在
	XXAPI const XTE_ArgItem* xteStmtRequireArg(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
	// 在语句渲染阶段要求指定名称参数存在
	XXAPI const XTE_ArgItem* xteStmtRequireNamedArg(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	// 在语句渲染阶段严格读取布尔参数
	XXAPI int xteStmtRequireBoolStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取命名布尔参数
	XXAPI int xteStmtRequireNamedBoolStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取整数参数
	XXAPI int xteStmtRequireIntStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取命名整数参数
	XXAPI int xteStmtRequireNamedIntStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取浮点参数
	XXAPI int xteStmtRequireFloatStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取命名浮点参数
	XXAPI int xteStmtRequireNamedFloatStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
	// 在语句渲染阶段严格读取文本参数
	XXAPI char* xteStmtRequireTextStrict(XTE_StmtRenderCtx* pCtx, uint32 iIndex, const char* sDesc);
	// 在语句渲染阶段严格读取命名文本参数
	XXAPI char* xteStmtRequireNamedTextStrict(XTE_StmtRenderCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	// 在函数调用阶段要求指定位置参数存在
	XXAPI const XTE_ArgItem* xteFuncRequireArg(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
	// 在函数调用阶段要求指定名称参数存在
	XXAPI const XTE_ArgItem* xteFuncRequireNamedArg(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	// 在函数调用阶段严格读取布尔参数
	XXAPI int xteFuncRequireBoolStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int* pOut, const char* sDesc);
	// 在函数调用阶段严格读取命名布尔参数
	XXAPI int xteFuncRequireNamedBoolStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int* pOut, const char* sDesc);
	// 在函数调用阶段严格读取整数参数
	XXAPI int xteFuncRequireIntStrict(XTE_FuncCtx* pCtx, uint32 iIndex, int64* pOut, const char* sDesc);
	// 在函数调用阶段严格读取命名整数参数
	XXAPI int xteFuncRequireNamedIntStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, int64* pOut, const char* sDesc);
	// 在函数调用阶段严格读取浮点参数
	XXAPI int xteFuncRequireFloatStrict(XTE_FuncCtx* pCtx, uint32 iIndex, double* pOut, const char* sDesc);
	// 在函数调用阶段严格读取命名浮点参数
	XXAPI int xteFuncRequireNamedFloatStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, double* pOut, const char* sDesc);
	// 在函数调用阶段严格读取文本参数
	XXAPI char* xteFuncRequireTextStrict(XTE_FuncCtx* pCtx, uint32 iIndex, const char* sDesc);
	// 在函数调用阶段严格读取命名文本参数
	XXAPI char* xteFuncRequireNamedTextStrict(XTE_FuncCtx* pCtx, const char* sName, size_t iNameSize, const char* sDesc);
	// 在语句解析阶段设置错误信息
	XXAPI int xteStmtParseSetError(XTE_StmtParseCtx* pCtx, int iCode, const char* sDesc);
	// 在语句渲染阶段设置错误信息
	XXAPI XTE_Flow xteStmtSetError(XTE_StmtRenderCtx* pCtx, int iCode, const char* sDesc);
	// 在函数调用阶段设置错误信息
	XXAPI int xteFuncSetError(XTE_FuncCtx* pCtx, int iCode, const char* sDesc);
	// 向模板输出流写入文本
	XXAPI int xteStmtWrite(XTE_StmtRenderCtx* pCtx, const char* sText, size_t iSize);
	// 渲染语句主体
	XXAPI int xteStmtRenderBody(XTE_StmtRenderCtx* pCtx);
	// 使用指定作用域渲染语句主体
	XXAPI int xteStmtRenderBodyWithScope(XTE_StmtRenderCtx* pCtx, xvalue pLocal, xvalue pCurrent);
	#ifdef XTE_ENABLE_FILE
		
		// 将模板调试结果保存到文件
		XXAPI int xteTemplateSaveFile(xtetemplate hTemplate, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
		// 加载模板文件
		XXAPI xtetemplate xteTemplateLoadFile(xteengine hEngine, const char* sFilePath, uint32 iFlags, XTE_Error* pError);
		
	#endif
	#ifdef XTE_DEBUGMODE
		
		// 导出模板
		XXAPI int xteTemplateDump(xtetemplate hTemplate, XTE_Writer* pWriter, uint32 iFlags);
		// 将模板调试信息输出到控制台
		XXAPI int xteTemplateDumpConsole(xtetemplate hTemplate, uint32 iFlags);
		
	#endif
	
	
	
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
		// 初始化数组调试
		XXAPI void xrtArrayInitDbg(xarray pArr, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁数组调试
		XXAPI void xrtArrayDestroyDbg(xarray pArr, const char* sFile, uint32 iLine);
		// 释放数组调试
		XXAPI void xrtArrayUnitDbg(xarray pArr, const char* sFile, uint32 iLine);
		// 创建字典调试
		XXAPI xdict xrtDictCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 初始化字典调试
		XXAPI void xrtDictInitDbg(xdict objHT, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁字典调试
		XXAPI void xrtDictDestroyDbg(xdict objHT, const char* sFile, uint32 iLine);
		// 释放字典调试
		XXAPI void xrtDictUnitDbg(xdict objHT, const char* sFile, uint32 iLine);
		XXAPI ptr xrtDictSetDbg(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet, const char* sFile, uint32 iLine);
		XXAPI bool xrtDictSetPtrDbg(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal, const char* sFile, uint32 iLine);
		XXAPI bool xrtDictRemoveDbg(xdict objHT, ptr sKey, uint32 iKeyLen, const char* sFile, uint32 iLine);
		XXAPI ptr xrtDictRemovePtrDbg(xdict objHT, ptr sKey, uint32 iKeyLen, const char* sFile, uint32 iLine);
		// 创建列表调试
		XXAPI xlist xrtListCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 初始化列表调试
		XXAPI void xrtListInitDbg(xlist objList, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁列表调试
		XXAPI void xrtListDestroyDbg(xlist objList, const char* sFile, uint32 iLine);
		// 释放列表调试
		XXAPI void xrtListUnitDbg(xlist objList, const char* sFile, uint32 iLine);
		XXAPI ptr xrtListSetDbg(xlist objList, int64 iKey, bool* bNewRet, const char* sFile, uint32 iLine);
		XXAPI bool xrtListSetPtrDbg(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal, const char* sFile, uint32 iLine);
		XXAPI bool xrtListRemoveDbg(xlist objList, int64 iKey, const char* sFile, uint32 iLine);
		XXAPI ptr xrtListRemovePtrDbg(xlist objList, int64 iKey, const char* sFile, uint32 iLine);
		// 创建 AVL 树调试
		XXAPI xavltree xrtAVLTreeCreateDbg(unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine);
		// 初始化 AVL 树调试
		XXAPI void xrtAVLTreeInitDbg(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁 AVL 树调试
		XXAPI void xrtAVLTreeDestroyDbg(xavltree objAVLT, const char* sFile, uint32 iLine);
		// 释放 AVL 树调试
		XXAPI void xrtAVLTreeUnitDbg(xavltree objAVLT, const char* sFile, uint32 iLine);
		// 创建 dyn 栈调试
		XXAPI xdynstack xrtDynStackCreateDbg(uint32 iItemLength, const char* sFile, uint32 iLine);
		// 初始化 dyn 栈调试
		XXAPI void xrtDynStackInitDbg(xdynstack objSTK, uint32 iItemLength, const char* sFile, uint32 iLine);
		// 销毁 dyn 栈调试
		XXAPI void xrtDynStackDestroyDbg(xdynstack objSTK, const char* sFile, uint32 iLine);
		// 释放 dyn 栈调试
		XXAPI void xrtDynStackUnitDbg(xdynstack objSTK, const char* sFile, uint32 iLine);
		// 创建内存内存池调试
		XXAPI xmempool xrtMemPoolCreateDbg(int iCustom, uint32 iMode, const char* sFile, uint32 iLine);
		// 初始化内存内存池调试
		XXAPI void xrtMemPoolInitDbg(xmempool objMP, int iCustom, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁内存内存池调试
		XXAPI void xrtMemPoolDestroyDbg(xmempool objMP, const char* sFile, uint32 iLine);
		// 释放内存内存池调试
		XXAPI void xrtMemPoolUnitDbg(xmempool objMP, const char* sFile, uint32 iLine);
		// 创建 fs 内存内存池调试
		XXAPI xfsmempool xrtFSMemPoolCreateDbg(unsigned int iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 初始化 fs 内存内存池调试
		XXAPI void xrtFSMemPoolInitDbg(xfsmempool objMM, unsigned int iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
		// 销毁 fs 内存内存池调试
		XXAPI void xrtFSMemPoolDestroyDbg(xfsmempool objMM, const char* sFile, uint32 iLine);
		// 释放 fs 内存内存池调试
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
		#define xrtDictSet(objHT, sKey, iKeyLen, bNewRet) xrtDictSetDbg((objHT), (sKey), (iKeyLen), (bNewRet), __FILE__, __LINE__)
		#define xrtDictSetPtr(objHT, sKey, iKeyLen, pVal, ppOldVal) xrtDictSetPtrDbg((objHT), (sKey), (iKeyLen), (pVal), (ppOldVal), __FILE__, __LINE__)
		#define xrtDictRemove(objHT, sKey, iKeyLen) xrtDictRemoveDbg((objHT), (sKey), (iKeyLen), __FILE__, __LINE__)
		#define xrtDictRemovePtr(objHT, sKey, iKeyLen) xrtDictRemovePtrDbg((objHT), (sKey), (iKeyLen), __FILE__, __LINE__)
		#define xrtListCreate(iItemLength, iMode) xrtListCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
		#define xrtListInit(objList, iItemLength, iMode) xrtListInitDbg((objList), (iItemLength), (iMode), __FILE__, __LINE__)
		#define xrtListDestroy(objList) xrtListDestroyDbg((objList), __FILE__, __LINE__)
		#define xrtListUnit(objList) xrtListUnitDbg((objList), __FILE__, __LINE__)
		#define xrtListSet(objList, iKey, bNewRet) xrtListSetDbg((objList), (iKey), (bNewRet), __FILE__, __LINE__)
		#define xrtListSetPtr(objList, iKey, pVal, ppOldVal) xrtListSetPtrDbg((objList), (iKey), (pVal), (ppOldVal), __FILE__, __LINE__)
		#define xrtListRemove(objList, iKey) xrtListRemoveDbg((objList), (iKey), __FILE__, __LINE__)
		#define xrtListRemovePtr(objList, iKey) xrtListRemovePtrDbg((objList), (iKey), __FILE__, __LINE__)
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
