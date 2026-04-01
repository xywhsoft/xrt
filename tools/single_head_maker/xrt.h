/*

    XRT Single Header File
    Generated: 2026-04-01 14:31:30

    MIT License

    Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in Software without restriction, including without limitation the rights
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

// ========================================
// XRT Single Header File
// ========================================
//
// Usage:
//   // In exactly ONE source file:
//   #define XRT_IMPLEMENTATION
//   #include "xrt.h"
//
//   // In all other files:
//   #include "xrt.h"
//
// Note:
//   - Define XRT_IMPLEMENTATION in exactly one source file
//   - Include this header in all other files without XRT_IMPLEMENTATION
//   - No need to link against xrt library
//
// ========================================

#ifndef XRT_SINGLE_HEADER
#define XRT_SINGLE_HEADER


// ========================================
// File: D:/git/xrt/xrt.h
// ========================================

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
#define _GNU_SOURCE
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
	
	// 申请无需主动释放的临时内存
	XXAPI ptr xrtTempMemory(size_t iSize);
	
	// 释放所有临时内存
	XXAPI void xrtFreeTempMemory();
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
	
	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
	#define XPROC_STATE_FAILED		-1
	#define XPROC_STATE_INIT		0
	#define XPROC_STATE_RUNNING		1
	#define XPROC_STATE_EXITED		2
	#define XPROC_STATE_CLOSED		3
	#define XPROC_F_USE_SHELL		0x0001u
	#define XPROC_F_PIPE_STDIN		0x0002u
	#define XPROC_F_PIPE_STDOUT		0x0004u
	#define XPROC_F_PIPE_STDERR		0x0008u
	#define XPROC_F_MERGE_STDERR	0x0010u
	#define XPROC_F_HIDE_WINDOW		0x0020u
	#define XPROC_F_NO_CAPTURE		0x0040u
	#define XPROC_F_KILL_TREE		0x0080u
	typedef struct xprocess_struct xprocess;
	typedef struct {
		void (*OnStart)(xprocess* pProcess, ptr pUserData);
		void (*OnStdout)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
		void (*OnStderr)(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData);
		void (*OnExit)(xprocess* pProcess, int iExitCode, ptr pUserData);
	} xprocessevents;
	typedef struct {
		uint32 iFlags;
		str sProgram;
		str* arrArgs;
		uint32 iArgCount;
		str sCommandLine;
		str sWorkDir;
		uint32 iReadChunkSize;
		size_t iMaxCaptureBytes;
		const xprocessevents* pEvents;
		ptr pUserData;
	} xprocessconfig;
	typedef struct {
		int iExitCode;
		ptr pStdout;
		size_t iStdoutSize;
		ptr pStderr;
		size_t iStderrSize;
		bool bStdoutTruncated;
		bool bStderrTruncated;
	} xprocessresult;
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
	// 终止进程
	XXAPI bool xrtProcessTerminate(xprocess* pProcess);
	// 终止进程树
	XXAPI bool xrtProcessKillTree(xprocess* pProcess);
	// 获取进程标准输出
	XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize);
	// 获取进程标准错误
	XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize);
	// 执行进程并捕获输出
	XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs);
	// 释放进程结果
	XXAPI void xrtProcessResultUnit(xprocessresult* pResult);
	
	
	
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
	// 复制字符串 u 16
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	// 复制字符串 u 32
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	// 复制内存
	XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
	
	// 比较字符串
	XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
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
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
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
	// 获取 HTTP/1 帧正文部分的字节数
	XXAPI size_t xrtCodecHttp1BodyBytes(const xcodecframe* pFrame);
	// 复制编解码器 HTTP/1 正文
	XXAPI size_t xrtCodecHttp1CopyBody(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen);
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
	// 等待进程 Future
	XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess);
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
	// 初始化 HTTP 请求对象
	XXAPI void xrtHttpRequestInit(xhttprequest* pReq);
	// 释放 HTTP 请求对象内部资源
	XXAPI void xrtHttpRequestUnit(xhttprequest* pReq);
	// 设置 HTTP 请求方法
	XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);
	// 设置 HTTP request URL
	XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);
	// 设置 HTTP request 头部
	XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);
	// 复制请求正文并设置 Content-Type
	XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);
	// 设置 HTTP request 超时
	XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
	// 设置 HTTP 请求空闲超时
	XXAPI void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
	// 设置 HTTPS 是否校验证书
	XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);
	// 销毁 HTTP 响应对象
	XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp);
	// 获取 HTTP response 头部
	XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);
	// 异步执行 HTTP 请求
	XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq);
	// 同步执行 HTTP 请求
	XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus);
	#endif
	#ifndef XRT_NO_XHTTPD
	// XNet 内建 HTTP 服务端
	XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
	// 获取 HTTP 服务端 response 头部
	XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
	// 初始化 HTTP 服务端配置
	XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
	// 初始化 HTTP 服务端请求对象
	XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
	// 释放 HTTP 服务端请求对象内部资源
	XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
	// 初始化 HTTP 服务端响应对象
	XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
	// 释放 HTTP 服务端响应对象内部资源
	XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
	// 创建 HTTP 服务端响应对象
	XXAPI xhttpdresponse* xrtHttpdResponseCreate(void);
	// 销毁 HTTP 服务端响应对象
	XXAPI void xrtHttpdResponseDestroy(xhttpdresponse* pResp);
	// 设置 HTTP 服务端 response 状态
	XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
	// 设置 HTTP 服务端 response 头部
	XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
	// 复制服务端响应正文并设置 Content-Type
	XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
	// 判断 HTTP 服务端连接是否仍然打开
	XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn);
	// 向 HTTP 服务端连接发送响应
	XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp);
	// 主动关闭 HTTP 服务端连接
	XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags);
	// 创建 HTTP 服务端
	XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
	// 获取 HTTP 服务端 bound 端口
	XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
	// 启动 HTTP 服务端
	XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
	// 停止 HTTP 服务端
	XXAPI void xrtHttpdStop(xhttpdserver* pServer);
	// 销毁 HTTP 服务端
	XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
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
	// 创建带所有权模式的集合值
	XXAPI xvalue xvoCreateCollEx(uint32 iMode);
	// 创建表值
	XXAPI xvalue xvoCreateTable();
	// 创建带所有权模式的表值
	XXAPI xvalue xvoCreateTableEx(uint32 iMode);
	// 创建分类
	XXAPI xvalue xvoCreateClass(uint32 iSize);
	// 创建自定义对象值
	XXAPI xvalue xvoCreateCustom(ptr pObj);
	
	// 读取值
	XXAPI bool xvoGetBool(xvalue pVal);
	// 获取整数
	XXAPI int64 xvoGetInt(xvalue pVal);
	// 获取浮点数
	XXAPI double xvoGetFloat(xvalue pVal);
	// 获取文本
	XXAPI str xvoGetText(xvalue pVal);
	// 获取时间值
	XXAPI xtime xvoGetTime(xvalue pVal);
	// 获取指针值
	XXAPI ptr xvoGetPoint(xvalue pVal);
	// 获取函数值
	XXAPI xfunction xvoGetFunc(xvalue pVal);
	// 获取数组
	XXAPI xparray xvoGetArray(xvalue pVal);
	// 获取列表
	XXAPI xlist xvoGetList(xvalue pVal);
	// 获取集合值
	XXAPI xavltree xvoGetColl(xvalue pVal);
	// 获取表值
	XXAPI xdict xvoGetTable(xvalue pVal);
	// 获取分类
	XXAPI ptr xvoGetClass(xvalue pVal);
	// 获取自定义对象值
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
	// 删除数组
	XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count);
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
	// 获取表成员数量
	XXAPI uint32 xvoTableItemCount(xvalue pTbl);
	// 清空表
	XXAPI bool xvoTableClear(xvalue pTbl);
	// 设置表父节点
	XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable);
	
	// 类型操作
	XXAPI bool xvoIsNull(xvalue pVal);
	// 获取值类型
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
	// 解析 JSON 文件
	XXAPI xvalue xrtParseJSON_File(str sFile);
	
	// 将 xvalue 转换为 JSON
	XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);
	// 将 xvalue 格式化为 JSON 并写入文件
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
	// 解析 XSON 扩展
	XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags);
	// 解析 XSON 文件
	XXAPI xvalue xrtParseXSON_File(str sFile);
	// 解析 XSON 文件扩展
	XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags);
	
	// 将 xvalue 转换为 XSON
	XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize);
	// 将 xvalue 格式化为 XSON 并写入文件
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
	// 释放请求对象
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
	// 设置最大重定向次数
	XXAPI void xrtHttpReqSetRedirect(xhttpreq pReq, int iMaxRedirects);
	// 设置是否校验 SSL 证书
	XXAPI void xrtHttpReqSetVerifySSL(xhttpreq pReq, bool bVerify);
	// 设置请求进度回调
	XXAPI void xrtHttpReqSetCallback(xhttpreq pReq, xhttp_proc pProc);
	// 设置请求用户上下文
	XXAPI void xrtHttpReqSetUserData(xhttpreq pReq, ptr pData);
	
	// Cookie 管理
	XXAPI void xrtHttpReqEnableCookies(xhttpreq pReq, bool bEnable);
	// 设置 HTTP req Cookie
	XXAPI void xrtHttpReqSetCookie(xhttpreq pReq, str sName, str sValue);
	// 删除 HTTP req Cookie
	XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName);
	
	// 执行请求 (阻塞, 返回响应对象)
	XXAPI xhttpresp xrtHttpReqExecute(xhttpreq pReq);
	
	// 响应对象读取辅助
	XXAPI int xrtHttpRespCode(xhttpresp pResp);
	// 获取 HTTP resp 正文
	XXAPI str xrtHttpRespBody(xhttpresp pResp);
	// 获取响应正文长度
	XXAPI size_t xrtHttpRespBodyLen(xhttpresp pResp);
	// 获取 HTTP resp 头部
	XXAPI str xrtHttpRespHeader(xhttpresp pResp, str sName);
	// 获取响应 Cookie
	XXAPI str xrtHttpRespCookie(xhttpresp pResp, str sName);
	// 获取响应 Content-Type
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
	// 创建列表调试
	XXAPI xlist xrtListCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	// 初始化列表调试
	XXAPI void xrtListInitDbg(xlist objList, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine);
	// 销毁列表调试
	XXAPI void xrtListDestroyDbg(xlist objList, const char* sFile, uint32 iLine);
	// 释放列表调试
	XXAPI void xrtListUnitDbg(xlist objList, const char* sFile, uint32 iLine);
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

// ========================================
// Implementation
// ========================================

#ifdef XRT_IMPLEMENTATION


// ========================================
// File: D:\git\xrt/xrt.c
// ========================================


#define XRT_BUILD_CORE
// (skipped include: #include "xrt.h")
#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
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
	#include <poll.h>
	#include <sys/syscall.h>
	#include <sys/eventfd.h>
	#include <sys/uio.h>
	#include <sched.h>
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
static volatile long __xrtMemForeignAllocLock = 0;
static xrtMemDebugForeignAlloc* __xrtMemForeignAllocList = NULL;
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

// ========================================
// File: D:/git/xrt/lib/suplib.h
// ========================================


// 内存查找
#if defined(_WIN32) || defined(_WIN64)
	// memmem 相关处理
	XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
	{
		size_t arrShift[256];
		if ( (iMemSize == 0) || (iSubSize == 0) ) {
			return NULL;
		}
		if ( iMemSize  < iSubSize ) {
			return NULL;
		}
		unsigned char* pMemC = (unsigned char*)pMem;
		unsigned char* pSubC = (unsigned char*)pSub;
		if ( iSubSize == 1 ) {
			return memchr(pMem, pSubC[0], iMemSize);
		}
		for ( size_t i = 0; i < 256; i++ ) {
			arrShift[i] = iSubSize;
		}
		for ( size_t i = 0; i + 1 < iSubSize; i++ ) {
			arrShift[pSubC[i]] = iSubSize - i - 1;
		}
		for ( size_t i = 0; i <= (iMemSize - iSubSize); ) {
			unsigned char iLast = pMemC[i + iSubSize - 1];
			if ( iLast == pSubC[iSubSize - 1] &&
				memcmp(&pMemC[i], pSubC, iSubSize - 1) == 0 ) {
				return &pMemC[i];
			}
			i += arrShift[iLast];
		}
		return NULL;
	}
#endif
// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
XXAPI size_t u16len(u16str sText)
{
	size_t iSize = 0;
	if ( sText == NULL ) { return 0; }
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}
// u32len 相关处理
XXAPI size_t u32len(u32str sText)
{
	size_t iSize = 0;
	if ( sText == NULL ) { return 0; }
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}
// 引入子库 - 按依赖关系和裁剪支持重新组织

// ========================================
// File: D:/git/xrt/lib/memglobal.h
// ========================================


// 内部函数：获取内存全局 align 大小
static inline size_t __xrtMemGlobalAlignSize(size_t iSize)
{
	size_t iAlign = sizeof(ptr);
	return (iSize + (iAlign - 1)) & ~(iAlign - 1);
}
// 内部函数：获取内存全局头部大小
static inline size_t __xrtMemGlobalHeaderSize()
{
	return __xrtMemGlobalAlignSize(sizeof(xrtMemBlockHeader));
}
// 指针相关处理
static inline ptr (*__xrtMemGlobalProcMalloc())(size_t)
{
	return xCore.malloc ? xCore.malloc : malloc;
}
// 指针相关处理
static inline ptr (*__xrtMemGlobalProcCalloc())(size_t, size_t)
{
	return xCore.calloc ? xCore.calloc : calloc;
}
// 指针相关处理
static inline ptr (*__xrtMemGlobalProcRealloc())(ptr, size_t)
{
	return xCore.realloc ? xCore.realloc : realloc;
}
// void 相关处理
static inline void (*__xrtMemGlobalProcFree())(ptr)
{
	return xCore.free ? xCore.free : free;
}
// 内部函数：锁定内存全局
static inline void __xrtMemGlobalLock(volatile long* pLock)
{
	uint32 iSpin = 0;
	while ( __xrtAtomicCompareExchange32(pLock, 1, 0) != 0 ) {
		iSpin++;
		if ( (iSpin & 0x3FFu) == 0u ) {
			#if defined(_WIN32) || defined(_WIN64)
				SwitchToThread();
			#else
				sched_yield();
			#endif
		}
	}
}
// 内部函数：解锁内存全局
static inline void __xrtMemGlobalUnlock(volatile long* pLock)
{
	__xrtAtomicExchange32(pLock, 0);
}
// 内部函数：初始化内存全局计划
static inline void __xrtMemGlobalInitPlan(xrtMemGlobalPool* pPool)
{
	uint32 i;
	if ( pPool == NULL ) {
		return;
	}
	memset(pPool, 0, sizeof(xrtMemGlobalPool));
	pPool->iClassCount = XRT_MEMPOOL_CLASS_COUNT_DEFAULT;
	pPool->iClassStep = XRT_MEMPOOL_STEP_SIZE;
	pPool->iCutoff = XRT_MEMPOOL_CUTOFF_DEFAULT;
	for ( i = 0; i < XRT_MEMPOOL_CLASS_COUNT_DEFAULT; i++ ) {
		pPool->arrClassDesc[i].iBlockSize = (uint16)((i + 1) * XRT_MEMPOOL_STEP_SIZE);
	}
	for ( i = 0; i <= XRT_MEMPOOL_CUTOFF_DEFAULT; i++ ) {
		if ( i == 0 ) {
			pPool->arrSizeClassLut[i] = 0;
		} else {
			pPool->arrSizeClassLut[i] = (uint8)(((i + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE) - 1);
		}
	}
}
// 内部函数：确保内存全局计划
static inline void __xrtMemGlobalEnsurePlan()
{
	if ( xCore.MemGlobal.iClassCount == 0 ) {
		__xrtMemGlobalInitPlan(&xCore.MemGlobal);
	}
}
// 内部函数：获取内存全局分类 index
static inline uint32 __xrtMemGlobalClassIndex(const xrtMemGlobalPool* pPool, size_t iSize)
{
	if ( pPool == NULL || iSize == 0 || iSize > pPool->iCutoff ) {
		return (uint32)-1;
	}
	return pPool->arrSizeClassLut[iSize];
}
// 内部函数：获取内存全局分类块大小
static inline uint32 __xrtMemGlobalClassBlockSize(const xrtMemGlobalPool* pPool, uint32 iClass)
{
	if ( pPool == NULL || iClass >= pPool->iClassCount ) {
		return 0;
	}
	return pPool->arrClassDesc[iClass].iBlockSize;
}
// 内部函数：获取内存全局调试 tail 大小
static inline size_t __xrtMemGlobalDebugTailSize()
{
	#ifdef XRT_MEM_DEBUG
		return sizeof(uint32);
	#else
		return 0;
	#endif
}
// 内部函数：分配内存全局 payload 大小
static inline size_t __xrtMemGlobalAllocPayloadSize(size_t iRequestSize)
{
	size_t iPayload = iRequestSize ? iRequestSize : 1;
	return iPayload + __xrtMemGlobalDebugTailSize();
}
// 内部函数：__xrtMemGlobalClassIndexForRequest
static inline uint32 __xrtMemGlobalClassIndexForRequest(const xrtMemGlobalPool* pPool, size_t iRequestSize)
{
	return __xrtMemGlobalClassIndex(pPool, __xrtMemGlobalAllocPayloadSize(iRequestSize));
}
// 内部函数：内存调试 now 毫秒相关处理
static inline uint64 __xrtMemDebugNowMs()
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		return ((uint64)timer.tv_sec * 1000ULL) + (uint64)(timer.tv_nsec / 1000000ULL);
	#endif
}
// 内部函数：__xrtMemGlobalHeaderFromUser
static inline xrtMemBlockHeader* __xrtMemGlobalHeaderFromUser(ptr pUser)
{
	if ( pUser == NULL ) {
		return NULL;
	}
	return (xrtMemBlockHeader*)((char*)pUser - __xrtMemGlobalHeaderSize());
}
// 内部函数：获取内存全局 user from 头部
static inline ptr __xrtMemGlobalUserFromHeader(xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return NULL;
	}
	return (ptr)((char*)pHeader + __xrtMemGlobalHeaderSize());
}
// 内部函数：判断全局内存头部是否有效
static inline bool __xrtMemGlobalHeaderValid(const xrtMemBlockHeader* pHeader)
{
	return pHeader != NULL && pHeader->iMagic == XRT_MEMBLOCK_MAGIC;
}
// 内部函数：写入内存全局头部
static inline void __xrtMemGlobalWriteHeader(xrtMemBlockHeader* pHeader, uint32 iClassIndex, uint16 iFlags, uint32 iRequestSize)
{
	pHeader->iMagic = XRT_MEMBLOCK_MAGIC;
	pHeader->iClassIndex = (uint16)iClassIndex;
	pHeader->iFlags = iFlags;
	pHeader->iRequestSize = iRequestSize;
	pHeader->iReserved = 0;
	#ifdef XRT_MEM_DEBUG
		pHeader->sAllocFile = NULL;
		pHeader->iAllocLine = 0;
		pHeader->sFreeFile = NULL;
		pHeader->iFreeLine = 0;
		pHeader->iAllocThreadId = 0;
		pHeader->iAllocSeq = 0;
		pHeader->iAllocTimeMs = 0;
		pHeader->iFreeTimeMs = 0;
		pHeader->pDebugPrev = NULL;
		pHeader->pDebugNext = NULL;
		pHeader->iFrontCanary = XRT_MEMDEBUG_CANARY_HEAD ^ (uint32)(uintptr_t)pHeader;
		pHeader->iDebugState = 0;
	#endif
}
#ifdef XRT_MEM_DEBUG
// 内部函数：获取内存全局 tail canary 值
static inline uint32 __xrtMemGlobalTailCanaryValue(const xrtMemBlockHeader* pHeader)
{
	return XRT_MEMDEBUG_CANARY_TAIL ^ (uint32)pHeader->iRequestSize ^ 0xA5A5A5A5u;
}
// 内部函数：__xrtMemGlobalTailCanaryPtr
static inline uint32* __xrtMemGlobalTailCanaryPtr(xrtMemBlockHeader* pHeader)
{
	size_t iPayload = pHeader->iRequestSize ? pHeader->iRequestSize : 1;
	return (uint32*)((char*)__xrtMemGlobalUserFromHeader(pHeader) + iPayload);
}
// 内部函数：__xrtMemGlobalWriteTailCanary
static inline void __xrtMemGlobalWriteTailCanary(xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return;
	}
	*__xrtMemGlobalTailCanaryPtr(pHeader) = __xrtMemGlobalTailCanaryValue(pHeader);
}
// 内部函数：__xrtMemGlobalCheckFrontCanary
static inline bool __xrtMemGlobalCheckFrontCanary(const xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return FALSE;
	}
	return pHeader->iFrontCanary == (XRT_MEMDEBUG_CANARY_HEAD ^ (uint32)(uintptr_t)pHeader);
}
// 内部函数：__xrtMemGlobalCheckTailCanary
static inline bool __xrtMemGlobalCheckTailCanary(const xrtMemBlockHeader* pHeader)
{
	uint32* pTail;
	if ( pHeader == NULL ) {
		return FALSE;
	}
	pTail = __xrtMemGlobalTailCanaryPtr((xrtMemBlockHeader*)pHeader);
	return *pTail == __xrtMemGlobalTailCanaryValue(pHeader);
}
// 内部函数：__xrtMemDebugEnabled
static inline bool __xrtMemDebugEnabled()
{
	return xCore.MemDebug.bEnabled != 0;
}
static inline const char* __xrtMemDebugAllocatorName(uint32 iAllocatorKind);
// 内部函数：__xrtMemDebugImmediateObjectTypeName
static inline const char* __xrtMemDebugImmediateObjectTypeName(uint32 iObjectType)
{
	switch ( iObjectType ) {
		case XRT_MEMDEBUG_OBJECT_ARRAY:
			return "array";
		case XRT_MEMDEBUG_OBJECT_DICT:
			return "dict";
		case XRT_MEMDEBUG_OBJECT_LIST:
			return "list";
		case XRT_MEMDEBUG_OBJECT_AVLTREE:
			return "avltree";
		case XRT_MEMDEBUG_OBJECT_DYNSTACK:
			return "dynstack";
		case XRT_MEMDEBUG_OBJECT_MEMPOOL:
			return "mempool";
		case XRT_MEMDEBUG_OBJECT_FSMEMPOOL:
			return "fsmempool";
		default:
			return "unknown";
	}
}
// 内部函数：获取内存调试 immediate 事件名称
static inline const char* __xrtMemDebugImmediateEventName(uint32 iType)
{
	switch ( iType ) {
		case XRT_MEMDEBUG_EVENT_DOUBLE_FREE:
			return "DOUBLE_FREE";
		case XRT_MEMDEBUG_EVENT_INVALID_FREE:
			return "INVALID_FREE";
		case XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE:
			return "WRONG_ALLOCATOR_FREE";
		case XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT:
			return "BUFFER_OVERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT:
			return "BUFFER_UNDERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT:
			return "USE_AFTER_FREE_SUSPECT";
		case XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY:
			return "OBJECT_DOUBLE_DESTROY";
		default:
			return "UNKNOWN";
	}
}
// 内部函数：__xrtMemDebugShouldEmitImmediate
static inline bool __xrtMemDebugShouldEmitImmediate(uint32 iType)
{
	return iType == XRT_MEMDEBUG_EVENT_DOUBLE_FREE
		|| iType == XRT_MEMDEBUG_EVENT_INVALID_FREE
		|| iType == XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE
		|| iType == XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY;
}
// 内部函数：__xrtMemDebugEmitConsoleLine
static inline void __xrtMemDebugEmitConsoleLine(const char* sText, size_t iLen)
{
	if ( sText == NULL || iLen == 0 ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
			DWORD iWritten = 0;
			if ( hErr && hErr != INVALID_HANDLE_VALUE ) {
				WriteFile(hErr, sText, (DWORD)iLen, &iWritten, NULL);
			}
			OutputDebugStringA(sText);
		}
	#else
		(void)write(2, sText, iLen);
	#endif
}
// 内部函数：__xrtMemDebugEmitImmediateNoLock
static inline void __xrtMemDebugEmitImmediateNoLock(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	char sText[640];
	const char* sKind;
	int iWritten;
	if ( !__xrtMemDebugShouldEmitImmediate(iType) ) {
		return;
	}
	if ( iType == XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY ) {
		sKind = __xrtMemDebugImmediateObjectTypeName(iAllocatorKind);
	} else {
		sKind = __xrtMemDebugAllocatorName(iAllocatorKind);
	}
	iWritten = snprintf(
		sText,
		sizeof(sText),
		"[XRT_MEM_DEBUG][%s] time_ms=%llu ptr=%p size=%llu kind=%s site=%s:%u thread=%llu\n",
		__xrtMemDebugImmediateEventName(iType),
		(unsigned long long)__xrtMemDebugNowMs(),
		pAddress,
		(unsigned long long)iSize,
		sKind ? sKind : "unknown",
		sFile ? sFile : "(unknown)",
		(unsigned int)iLine,
		(unsigned long long)xrtThreadGetCurrentId()
	);
	if ( iWritten <= 0 ) {
		return;
	}
	if ( (size_t)iWritten >= sizeof(sText) ) {
		iWritten = (int)(sizeof(sText) - 1);
	}
	__xrtMemDebugEmitConsoleLine(sText, (size_t)iWritten);
}
// 内部函数：锁定内存调试
static inline void __xrtMemDebugLock()
{
	__xrtMemGlobalLock(&xCore.MemDebug.iLock);
}
// 内部函数：解锁内存调试
static inline void __xrtMemDebugUnlock()
{
	__xrtMemGlobalUnlock(&xCore.MemDebug.iLock);
}
// 内部函数：获取内存调试 allocator 名称
static inline const char* __xrtMemDebugAllocatorName(uint32 iAllocatorKind)
{
	switch ( iAllocatorKind ) {
		case XRT_MEMDEBUG_ALLOCATOR_GLOBAL:
			return "global";
		case XRT_MEMDEBUG_ALLOCATOR_MEMPOOL:
			return "mempool";
		case XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL:
			return "fsmempool";
		default:
			return "unknown";
	}
}
// 内部函数：__xrtMemDebugFindSiteStatNoLock
static inline xrtMemDebugSiteStat* __xrtMemDebugFindSiteStatNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind)
{
	xrtMemDebugSiteStat* pNode = xCore.MemDebug.pSiteStats;
	while ( pNode ) {
		if ( pNode->sFile == sFile && pNode->iLine == iLine && pNode->iAllocatorKind == iAllocatorKind ) {
			return pNode;
		}
		pNode = pNode->pNext;
	}
	return NULL;
}
// 内部函数：__xrtMemDebugEnsureSiteStatNoLock
static inline xrtMemDebugSiteStat* __xrtMemDebugEnsureSiteStatNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind)
{
	xrtMemDebugSiteStat* pNode = __xrtMemDebugFindSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pNode ) {
		return pNode;
	}
	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugSiteStat));
	if ( pNode == NULL ) {
		return NULL;
	}
	pNode->sFile = sFile;
	pNode->iLine = iLine;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->pNext = xCore.MemDebug.pSiteStats;
	xCore.MemDebug.pSiteStats = pNode;
	return pNode;
}
// 内部函数：__xrtMemDebugSiteOnAllocNoLock
static inline void __xrtMemDebugSiteOnAllocNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind, size_t iSize)
{
	xrtMemDebugSiteStat* pSite = __xrtMemDebugEnsureSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pSite == NULL ) {
		return;
	}
	pSite->iAllocCount++;
	pSite->iAllocBytes += iSize;
	pSite->iLiveCount++;
	pSite->iLiveBytes += iSize;
	if ( pSite->iLiveCount > pSite->iPeakLiveCount ) {
		pSite->iPeakLiveCount = pSite->iLiveCount;
	}
	if ( pSite->iLiveBytes > pSite->iPeakLiveBytes ) {
		pSite->iPeakLiveBytes = pSite->iLiveBytes;
	}
}
// 内部函数：__xrtMemDebugSiteOnFreeNoLock
static inline void __xrtMemDebugSiteOnFreeNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind, size_t iSize)
{
	xrtMemDebugSiteStat* pSite = __xrtMemDebugEnsureSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pSite == NULL ) {
		return;
	}
	pSite->iFreeCount++;
	pSite->iFreeBytes += iSize;
	if ( pSite->iLiveCount > 0 ) {
		pSite->iLiveCount--;
	}
	if ( pSite->iLiveBytes >= iSize ) {
		pSite->iLiveBytes -= iSize;
	} else {
		pSite->iLiveBytes = 0;
	}
}
// 内部函数：锁定内存调试记录事件 no
static inline void __xrtMemDebugRecordEventNoLock(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugEvent* pEvent = &xCore.MemDebug.arrEvents[xCore.MemDebug.iEventCursor];
	memset(pEvent, 0, sizeof(xrtMemDebugEvent));
	pEvent->iType = iType;
	pEvent->iLine = iLine;
	pEvent->iAllocatorKind = iAllocatorKind;
	pEvent->iThreadId = xrtThreadGetCurrentId();
	pEvent->iTimeMs = __xrtMemDebugNowMs();
	pEvent->iSize = (uint64)iSize;
	pEvent->pAddress = pAddress;
	pEvent->sFile = sFile;
	xCore.MemDebug.iEventCursor = (xCore.MemDebug.iEventCursor + 1) % XRT_MEMDEBUG_EVENT_CAPACITY;
	if ( xCore.MemDebug.iEventCount < XRT_MEMDEBUG_EVENT_CAPACITY ) {
		xCore.MemDebug.iEventCount++;
	}
}
// 内部函数：__xrtMemDebugAttachLiveNoLock
static inline void __xrtMemDebugAttachLiveNoLock(xrtMemBlockHeader* pHeader)
{
	pHeader->pDebugPrev = xCore.MemDebug.pLiveTail;
	pHeader->pDebugNext = NULL;
	if ( xCore.MemDebug.pLiveTail ) {
		xCore.MemDebug.pLiveTail->pDebugNext = pHeader;
	} else {
		xCore.MemDebug.pLiveHead = pHeader;
	}
	xCore.MemDebug.pLiveTail = pHeader;
	xCore.MemDebug.iLiveAllocCount++;
	xCore.MemDebug.iLiveAllocBytes += pHeader->iRequestSize;
	if ( xCore.MemDebug.iLiveAllocCount > xCore.MemDebug.iPeakLiveAllocCount ) {
		xCore.MemDebug.iPeakLiveAllocCount = xCore.MemDebug.iLiveAllocCount;
	}
	if ( xCore.MemDebug.iLiveAllocBytes > xCore.MemDebug.iPeakLiveAllocBytes ) {
		xCore.MemDebug.iPeakLiveAllocBytes = xCore.MemDebug.iLiveAllocBytes;
	}
}
// 内部函数：__xrtMemDebugDetachLiveNoLock
static inline void __xrtMemDebugDetachLiveNoLock(xrtMemBlockHeader* pHeader)
{
	if ( pHeader->pDebugPrev ) {
		pHeader->pDebugPrev->pDebugNext = pHeader->pDebugNext;
	} else {
		xCore.MemDebug.pLiveHead = pHeader->pDebugNext;
	}
	if ( pHeader->pDebugNext ) {
		pHeader->pDebugNext->pDebugPrev = pHeader->pDebugPrev;
	} else {
		xCore.MemDebug.pLiveTail = pHeader->pDebugPrev;
	}
	pHeader->pDebugPrev = NULL;
	pHeader->pDebugNext = NULL;
	if ( xCore.MemDebug.iLiveAllocCount > 0 ) {
		xCore.MemDebug.iLiveAllocCount--;
	}
	if ( xCore.MemDebug.iLiveAllocBytes >= pHeader->iRequestSize ) {
		xCore.MemDebug.iLiveAllocBytes -= pHeader->iRequestSize;
	} else {
		xCore.MemDebug.iLiveAllocBytes = 0;
	}
}
// 内部函数：__xrtMemDebugFindForeignNoLock
static inline xrtMemDebugForeignAlloc* __xrtMemDebugFindForeignNoLock(ptr pAddress, xrtMemDebugForeignAlloc** ppPrev)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode = xCore.MemDebug.pForeignAllocs;
	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			if ( ppPrev ) {
				*ppPrev = pPrev;
			}
			return pNode;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}
	if ( ppPrev ) {
		*ppPrev = NULL;
	}
	return NULL;
}
// 内部函数：__xrtMemDebugRegisterForeignAlloc
static inline void __xrtMemDebugRegisterForeignAlloc(ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	if ( __xrtMemDebugFindForeignNoLock(pAddress, NULL) != NULL ) {
		__xrtMemDebugUnlock();
		return;
	}
	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugForeignAlloc));
	if ( pNode == NULL ) {
		__xrtMemDebugUnlock();
		return;
	}
	pNode->pAddress = pAddress;
	pNode->iSize = (uint32)iSize;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->pNext = xCore.MemDebug.pForeignAllocs;
	xCore.MemDebug.pForeignAllocs = pNode;
	xCore.MemDebug.iForeignLiveCount++;
	xCore.MemDebug.iForeignLiveBytes += iSize;
	if ( xCore.MemDebug.iForeignLiveCount > xCore.MemDebug.iPeakForeignLiveCount ) {
		xCore.MemDebug.iPeakForeignLiveCount = xCore.MemDebug.iForeignLiveCount;
	}
	if ( xCore.MemDebug.iForeignLiveBytes > xCore.MemDebug.iPeakForeignLiveBytes ) {
		xCore.MemDebug.iPeakForeignLiveBytes = xCore.MemDebug.iForeignLiveBytes;
	}
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, iAllocatorKind, iSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_ALLOC, pAddress, iSize, iAllocatorKind, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：__xrtMemDebugUnregisterForeignAlloc
static inline bool __xrtMemDebugUnregisterForeignAlloc(ptr pAddress, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return FALSE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindForeignNoLock(pAddress, &pPrev);
	if ( pNode == NULL ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pAddress, 0, iAllocatorKind, sFile, iLine);
		xCore.MemDebug.iDoubleFreeCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pAddress, 0, iAllocatorKind, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	if ( pPrev ) {
		pPrev->pNext = pNode->pNext;
	} else {
		xCore.MemDebug.pForeignAllocs = pNode->pNext;
	}
	if ( xCore.MemDebug.iForeignLiveCount > 0 ) {
		xCore.MemDebug.iForeignLiveCount--;
	}
	if ( xCore.MemDebug.iForeignLiveBytes >= pNode->iSize ) {
		xCore.MemDebug.iForeignLiveBytes -= pNode->iSize;
	} else {
		xCore.MemDebug.iForeignLiveBytes = 0;
	}
	__xrtMemDebugSiteOnFreeNoLock(pNode->sAllocFile, pNode->iAllocLine, pNode->iAllocatorKind, pNode->iSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_FREE, pAddress, pNode->iSize, pNode->iAllocatorKind, sFile, iLine);
	__xrtMemGlobalProcFree()(pNode);
	__xrtMemDebugUnlock();
	return TRUE;
}
// 内部函数：__xrtMemDebugLookupForeignAlloc
static inline bool __xrtMemDebugLookupForeignAlloc(ptr pAddress, uint32* pAllocatorKind, size_t* pSize, const char** psFile, uint32* pLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return FALSE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindForeignNoLock(pAddress, NULL);
	if ( pNode == NULL ) {
		__xrtMemDebugUnlock();
		return FALSE;
	}
	if ( pAllocatorKind ) {
		*pAllocatorKind = pNode->iAllocatorKind;
	}
	if ( pSize ) {
		*pSize = pNode->iSize;
	}
	if ( psFile ) {
		*psFile = pNode->sAllocFile;
	}
	if ( pLine ) {
		*pLine = pNode->iAllocLine;
	}
	__xrtMemDebugUnlock();
	return TRUE;
}
// 内部函数：__xrtMemDebugFindObjectNoLock
static inline xrtMemDebugObject* __xrtMemDebugFindObjectNoLock(ptr pAddress)
{
	xrtMemDebugObject* pNode = xCore.MemDebug.pObjects;
	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			return pNode;
		}
		pNode = pNode->pNext;
	}
	return NULL;
}
// 内部函数：注册内存调试 object
static inline void __xrtMemDebugRegisterObject(ptr pAddress, uint32 iObjectType, uint32 iOrigin, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode == NULL ) {
		pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugObject));
		if ( pNode == NULL ) {
			__xrtMemDebugUnlock();
			return;
		}
		pNode->pAddress = pAddress;
		pNode->pNext = xCore.MemDebug.pObjects;
		xCore.MemDebug.pObjects = pNode;
	}
	if ( pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		xCore.MemDebug.iLiveObjectCount++;
		if ( xCore.MemDebug.iLiveObjectCount > xCore.MemDebug.iPeakLiveObjectCount ) {
			xCore.MemDebug.iPeakLiveObjectCount = xCore.MemDebug.iLiveObjectCount;
		}
	}
	pNode->iObjectType = iObjectType;
	pNode->iOrigin = iOrigin;
	pNode->iState = XRT_MEMDEBUG_OBJECT_STATE_LIVE;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->sFreeFile = NULL;
	pNode->iFreeLine = 0;
	pNode->iFreeThreadId = 0;
	pNode->iFreeTimeMs = 0;
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_CREATE, pAddress, 0, iObjectType, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：__xrtMemDebugObjectGuardDestroy
static inline bool __xrtMemDebugObjectGuardDestroy(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return TRUE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode && pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
		xCore.MemDebug.iObjectDoubleDestroyCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	__xrtMemDebugUnlock();
	return TRUE;
}
// 内部函数：注销内存调试 object
static inline bool __xrtMemDebugUnregisterObject(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return TRUE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode == NULL || pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, iObjectType, sFile, iLine);
		xCore.MemDebug.iObjectDoubleDestroyCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, iObjectType, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	pNode->iState = XRT_MEMDEBUG_OBJECT_STATE_DESTROYED;
	pNode->sFreeFile = sFile;
	pNode->iFreeLine = iLine;
	pNode->iFreeThreadId = xrtThreadGetCurrentId();
	pNode->iFreeTimeMs = __xrtMemDebugNowMs();
	if ( xCore.MemDebug.iLiveObjectCount > 0 ) {
		xCore.MemDebug.iLiveObjectCount--;
	}
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
	__xrtMemDebugUnlock();
	return TRUE;
}
// 内部函数：分配内存调试 track
static inline void __xrtMemDebugTrackAlloc(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	pHeader->sAllocFile = sFile;
	pHeader->iAllocLine = iLine;
	pHeader->sFreeFile = NULL;
	pHeader->iFreeLine = 0;
	pHeader->iAllocThreadId = xrtThreadGetCurrentId();
	pHeader->iAllocTimeMs = __xrtMemDebugNowMs();
	pHeader->iAllocSeq = ++xCore.MemDebug.iAllocSeq;
	pHeader->iFreeTimeMs = 0;
	pHeader->iDebugState = XRT_MEMDEBUG_STATE_LIVE;
	__xrtMemDebugAttachLiveNoLock(pHeader);
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_ALLOC, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：释放内存调试 track
static inline void __xrtMemDebugTrackFree(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	if ( pHeader->iDebugState == XRT_MEMDEBUG_STATE_LIVE ) {
		__xrtMemDebugDetachLiveNoLock(pHeader);
		__xrtMemDebugSiteOnFreeNoLock(pHeader->sAllocFile, pHeader->iAllocLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	}
	pHeader->sFreeFile = sFile;
	pHeader->iFreeLine = iLine;
	pHeader->iFreeTimeMs = __xrtMemDebugNowMs();
	pHeader->iDebugState = XRT_MEMDEBUG_STATE_FREED;
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_FREE, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：__xrtMemDebugTrackReallocInPlace
static inline void __xrtMemDebugTrackReallocInPlace(xrtMemBlockHeader* pHeader, size_t iOldSize, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	__xrtMemDebugSiteOnFreeNoLock(pHeader->sAllocFile, pHeader->iAllocLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, iOldSize);
	pHeader->sAllocFile = sFile;
	pHeader->iAllocLine = iLine;
	pHeader->iAllocThreadId = xrtThreadGetCurrentId();
	pHeader->iAllocTimeMs = __xrtMemDebugNowMs();
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_REALLOC, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：内存调试记录 simple 事件相关处理
static inline void __xrtMemDebugRecordSimpleEvent(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	if ( !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	__xrtMemDebugRecordEventNoLock(iType, pAddress, iSize, iAllocatorKind, sFile, iLine);
	if ( iType == XRT_MEMDEBUG_EVENT_DOUBLE_FREE ) {
		xCore.MemDebug.iDoubleFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_INVALID_FREE ) {
		xCore.MemDebug.iInvalidFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE ) {
		xCore.MemDebug.iWrongAllocatorFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT ) {
		xCore.MemDebug.iOverflowCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT ) {
		xCore.MemDebug.iUnderflowCount++;
	}
	__xrtMemDebugEmitImmediateNoLock(iType, pAddress, iSize, iAllocatorKind, sFile, iLine);
	__xrtMemDebugUnlock();
}
// 内部函数：重置内存调试状态
static inline void __xrtMemDebugResetState(xrtMemDebugState* pState)
{
	xrtMemBlockHeader* pQuarantineHead;
	xrtMemDebugSiteStat* pSiteHead;
	xrtMemDebugForeignAlloc* pForeignHead;
	xrtMemDebugObject* pObjectHead;
	xrtMemDebugSiteStat* pSite;
	xrtMemDebugForeignAlloc* pForeign;
	xrtMemDebugObject* pObject;
	xrtMemBlockHeader* pQuarantine;
	if ( pState == NULL ) {
		return;
	}
	__xrtMemGlobalLock(&pState->iLock);
	pQuarantineHead = pState->pQuarantineHead;
	pSiteHead = pState->pSiteStats;
	pForeignHead = pState->pForeignAllocs;
	pObjectHead = pState->pObjects;
	memset(pState, 0, sizeof(xrtMemDebugState));
	pState->iLock = 1;
	pState->bEnabled = 1;
	__xrtMemGlobalUnlock(&pState->iLock);
	pQuarantine = pQuarantineHead;
	while ( pQuarantine ) {
		xrtMemBlockHeader* pNext = pQuarantine->pDebugNext;
		__xrtMemGlobalProcFree()(pQuarantine);
		pQuarantine = pNext;
	}
	pSite = pSiteHead;
	while ( pSite ) {
		xrtMemDebugSiteStat* pNext = pSite->pNext;
		__xrtMemGlobalProcFree()(pSite);
		pSite = pNext;
	}
	pForeign = pForeignHead;
	while ( pForeign ) {
		xrtMemDebugForeignAlloc* pNext = pForeign->pNext;
		__xrtMemGlobalProcFree()(pForeign);
		pForeign = pNext;
	}
	pObject = pObjectHead;
	while ( pObject ) {
		xrtMemDebugObject* pNext = pObject->pNext;
		__xrtMemGlobalProcFree()(pObject);
		pObject = pNext;
	}
}
// 内部函数：判断是否存在内存调试 leaks
static inline bool __xrtMemDebugHasLeaks()
{
	return xCore.MemDebug.pLiveHead != NULL || xCore.MemDebug.pForeignAllocs != NULL || xCore.MemDebug.iLiveObjectCount != 0;
}
#else
// 内部函数：__xrtMemGlobalWriteTailCanary
static inline void __xrtMemGlobalWriteTailCanary(xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
}
// 内部函数：__xrtMemGlobalCheckFrontCanary
static inline bool __xrtMemGlobalCheckFrontCanary(const xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
	return TRUE;
}
// 内部函数：__xrtMemGlobalCheckTailCanary
static inline bool __xrtMemGlobalCheckTailCanary(const xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
	return TRUE;
}
// 内部函数：__xrtMemDebugFindForeignReleaseNoLock
static inline xrtMemDebugForeignAlloc* __xrtMemDebugFindForeignReleaseNoLock(ptr pAddress, xrtMemDebugForeignAlloc** ppPrev)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode = __xrtMemForeignAllocList;
	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			if ( ppPrev ) {
				*ppPrev = pPrev;
			}
			return pNode;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}
	if ( ppPrev ) {
		*ppPrev = NULL;
	}
	return NULL;
}
// 内部函数：__xrtMemDebugRegisterForeignAlloc
static inline void __xrtMemDebugRegisterForeignAlloc(ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL ) {
		return;
	}
	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	if ( __xrtMemDebugFindForeignReleaseNoLock(pAddress, NULL) != NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return;
	}
	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugForeignAlloc));
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return;
	}
	pNode->pAddress = pAddress;
	pNode->iSize = (uint32)iSize;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->pNext = __xrtMemForeignAllocList;
	__xrtMemForeignAllocList = pNode;
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
}
// 内部函数：__xrtMemDebugUnregisterForeignAlloc
static inline bool __xrtMemDebugUnregisterForeignAlloc(ptr pAddress, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode;
	(void)iAllocatorKind;
	(void)sFile;
	(void)iLine;
	if ( pAddress == NULL ) {
		return FALSE;
	}
	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pNode = __xrtMemDebugFindForeignReleaseNoLock(pAddress, &pPrev);
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return FALSE;
	}
	if ( pPrev ) {
		pPrev->pNext = pNode->pNext;
	} else {
		__xrtMemForeignAllocList = pNode->pNext;
	}
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	__xrtMemGlobalProcFree()(pNode);
	return TRUE;
}
// 内部函数：__xrtMemDebugLookupForeignAlloc
static inline bool __xrtMemDebugLookupForeignAlloc(ptr pAddress, uint32* pAllocatorKind, size_t* pSize, const char** psFile, uint32* pLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL ) {
		return FALSE;
	}
	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pNode = __xrtMemDebugFindForeignReleaseNoLock(pAddress, NULL);
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return FALSE;
	}
	if ( pAllocatorKind ) {
		*pAllocatorKind = pNode->iAllocatorKind;
	}
	if ( pSize ) {
		*pSize = pNode->iSize;
	}
	if ( psFile ) {
		*psFile = pNode->sAllocFile;
	}
	if ( pLine ) {
		*pLine = pNode->iAllocLine;
	}
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	return TRUE;
}
// 内部函数：注册内存调试 object
static inline void __xrtMemDebugRegisterObject(ptr pAddress, uint32 iObjectType, uint32 iOrigin, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)iOrigin;
	(void)sFile;
	(void)iLine;
}
// 内部函数：__xrtMemDebugObjectGuardDestroy
static inline bool __xrtMemDebugObjectGuardDestroy(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)sFile;
	(void)iLine;
	return TRUE;
}
// 内部函数：注销内存调试 object
static inline bool __xrtMemDebugUnregisterObject(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)sFile;
	(void)iLine;
	return TRUE;
}
// 内部函数：分配内存调试 track
static inline void __xrtMemDebugTrackAlloc(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)sFile;
	(void)iLine;
}
// 内部函数：释放内存调试 track
static inline void __xrtMemDebugTrackFree(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)sFile;
	(void)iLine;
}
// 内部函数：__xrtMemDebugTrackReallocInPlace
static inline void __xrtMemDebugTrackReallocInPlace(xrtMemBlockHeader* pHeader, size_t iOldSize, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)iOldSize;
	(void)sFile;
	(void)iLine;
}
// 内部函数：内存调试记录 simple 事件相关处理
static inline void __xrtMemDebugRecordSimpleEvent(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	(void)iType;
	(void)pAddress;
	(void)iSize;
	(void)iAllocatorKind;
	(void)sFile;
	(void)iLine;
}
// 内部函数：重置内存调试状态
static inline void __xrtMemDebugResetState(ptr pState)
{
	xrtMemDebugForeignAlloc* pHead;
	(void)pState;
	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pHead = __xrtMemForeignAllocList;
	__xrtMemForeignAllocList = NULL;
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	while ( pHead ) {
		xrtMemDebugForeignAlloc* pNext = pHead->pNext;
		__xrtMemGlobalProcFree()(pHead);
		pHead = pNext;
	}
}
// 内部函数：判断是否存在内存调试 leaks
static inline bool __xrtMemDebugHasLeaks()
{
	return FALSE;
}
#endif
// 内部函数：获取内存全局线程 cache
static inline xrtMemThreadCache* __xrtMemGlobalGetThreadCache()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		return NULL;
	}
	return (xrtMemThreadCache*)pThreadData->tMem.pLocalAlloc;
}
// 内部函数：初始化内存全局线程 cache
static inline bool __xrtMemGlobalInitThreadCache(xrtThreadData* pThreadData)
{
	ptr (*procCalloc)(size_t, size_t) = __xrtMemGlobalProcCalloc();
	xrtMemThreadCache* pCache;
	if ( pThreadData == NULL ) {
		return FALSE;
	}
	if ( pThreadData->tMem.pLocalAlloc ) {
		return TRUE;
	}
	__xrtMemGlobalEnsurePlan();
	pCache = procCalloc(1, sizeof(xrtMemThreadCache));
	if ( pCache == NULL ) {
		return FALSE;
	}
	pCache->iClassCount = xCore.MemGlobal.iClassCount;
	pCache->iCacheLimit = XRT_MEMGLOBAL_CACHE_LIMIT;
	pThreadData->tMem.pLocalAlloc = pCache;
	return TRUE;
}
// 内部函数：压入内存全局 central
static inline void __xrtMemGlobalPushCentral(uint32 iClass, xrtMemFreeNode* pHead, xrtMemFreeNode* pTail, uint32 iCount)
{
	xrtMemGlobalClassDesc* pClass = &xCore.MemGlobal.arrClassDesc[iClass];
	if ( pHead == NULL || pTail == NULL || iCount == 0 ) {
		return;
	}
	__xrtMemGlobalLock(&pClass->iLock);
	pTail->pNext = pClass->pFreeList;
	pClass->pFreeList = pHead;
	pClass->iFreeCount += iCount;
	__xrtMemGlobalUnlock(&pClass->iLock);
}
// 内部函数：弹出内存全局 central
static inline xrtMemFreeNode* __xrtMemGlobalPopCentral(uint32 iClass, uint32 iMaxCount, uint32* pOutCount)
{
	xrtMemGlobalClassDesc* pClass = &xCore.MemGlobal.arrClassDesc[iClass];
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 iCount = 0;
	__xrtMemGlobalLock(&pClass->iLock);
	while ( pClass->pFreeList && iCount < iMaxCount ) {
		xrtMemFreeNode* pNode = pClass->pFreeList;
		pClass->pFreeList = pNode->pNext;
		pNode->pNext = NULL;
		if ( pHead == NULL ) {
			pHead = pNode;
		} else {
			pTail->pNext = pNode;
		}
		pTail = pNode;
		iCount++;
	}
	if ( pClass->iFreeCount >= iCount ) {
		pClass->iFreeCount -= iCount;
	} else {
		pClass->iFreeCount = 0;
	}
	__xrtMemGlobalUnlock(&pClass->iLock);
	if ( pOutCount ) {
		*pOutCount = iCount;
	}
	return pHead;
}
// 内部函数：分配内存全局 span
static inline bool __xrtMemGlobalAllocSpan(uint32 iClass)
{
	ptr (*procMalloc)(size_t) = __xrtMemGlobalProcMalloc();
	size_t iHeaderSize = __xrtMemGlobalHeaderSize();
	size_t iPayloadSize = __xrtMemGlobalClassBlockSize(&xCore.MemGlobal, iClass);
	size_t iStride = __xrtMemGlobalAlignSize(iHeaderSize + iPayloadSize);
	uint32 iBlockCount;
	size_t iBytes;
	xrtMemGlobalSpan* pSpan;
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 i;
	if ( iPayloadSize == 0 ) {
		return FALSE;
	}
	iBlockCount = (uint32)(XRT_MEMGLOBAL_SPAN_TARGET_BYTES / iStride);
	if ( iBlockCount < XRT_MEMGLOBAL_SPAN_MIN_BLOCKS ) {
		iBlockCount = XRT_MEMGLOBAL_SPAN_MIN_BLOCKS;
	}
	if ( iBlockCount > XRT_MEMGLOBAL_SPAN_MAX_BLOCKS ) {
		iBlockCount = XRT_MEMGLOBAL_SPAN_MAX_BLOCKS;
	}
	iBytes = __xrtMemGlobalAlignSize(sizeof(xrtMemGlobalSpan)) + (iStride * iBlockCount);
	pSpan = (xrtMemGlobalSpan*)procMalloc(iBytes);
	if ( pSpan == NULL ) {
		return FALSE;
	}
	pSpan->pMemory = pSpan;
	pSpan->iClassIndex = iClass;
	pSpan->iBlockCount = iBlockCount;
	{
		char* pCursor = (char*)pSpan + __xrtMemGlobalAlignSize(sizeof(xrtMemGlobalSpan));
		for ( i = 0; i < iBlockCount; i++ ) {
			xrtMemBlockHeader* pHeader = (xrtMemBlockHeader*)pCursor;
			xrtMemFreeNode* pNode;
			__xrtMemGlobalWriteHeader(pHeader, iClass, XRT_MEMBLOCK_FLAG_POOLED, (uint32)iPayloadSize);
			pNode = (xrtMemFreeNode*)__xrtMemGlobalUserFromHeader(pHeader);
			pNode->pNext = NULL;
			if ( pHead == NULL ) {
				pHead = pNode;
			} else {
				pTail->pNext = pNode;
			}
			pTail = pNode;
			pCursor += iStride;
		}
	}
	__xrtMemGlobalLock(&xCore.MemGlobal.iSpanLock);
	pSpan->pNext = xCore.MemGlobal.pSpanList;
	xCore.MemGlobal.pSpanList = pSpan;
	__xrtMemGlobalUnlock(&xCore.MemGlobal.iSpanLock);
	xCore.MemGlobal.arrClassDesc[iClass].iSpanCount++;
	__xrtMemGlobalPushCentral(iClass, pHead, pTail, iBlockCount);
	return TRUE;
}
// 内部函数：__xrtMemGlobalRefillThreadCache
static inline bool __xrtMemGlobalRefillThreadCache(xrtMemThreadCache* pCache, uint32 iClass)
{
	uint32 iDesired = pCache ? (uint32)(pCache->iCacheLimit / 2) : 0;
	uint32 iCount = 0;
	xrtMemFreeNode* pHead;
	xrtMemFreeNode* pNode;
	if ( pCache == NULL || iClass >= pCache->iClassCount ) {
		return FALSE;
	}
	if ( iDesired == 0 ) {
		iDesired = 1;
	}
	pHead = __xrtMemGlobalPopCentral(iClass, iDesired, &iCount);
	if ( pHead == NULL ) {
		if ( !__xrtMemGlobalAllocSpan(iClass) ) {
			return FALSE;
		}
		pHead = __xrtMemGlobalPopCentral(iClass, iDesired, &iCount);
		if ( pHead == NULL ) {
			return FALSE;
		}
	}
	pNode = pHead;
	while ( pNode ) {
		xrtMemFreeNode* pNext = pNode->pNext;
		pNode->pNext = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode;
		pCache->arrFreeCount[iClass]++;
		pNode = pNext;
	}
	return pCache->arrFreeCount[iClass] > 0;
}
// 内部函数：__xrtMemGlobalDrainThreadCache
static inline void __xrtMemGlobalDrainThreadCache(xrtMemThreadCache* pCache, uint32 iClass, uint32 iKeepCount)
{
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 iCount = 0;
	if ( pCache == NULL || iClass >= pCache->iClassCount ) {
		return;
	}
	while ( pCache->arrFreeCount[iClass] > iKeepCount ) {
		xrtMemFreeNode* pNode = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode->pNext;
		pCache->arrFreeCount[iClass]--;
		pNode->pNext = NULL;
		if ( pHead == NULL ) {
			pHead = pNode;
		} else {
			pTail->pNext = pNode;
		}
		pTail = pNode;
		iCount++;
	}
	if ( iCount > 0 ) {
		__xrtMemGlobalPushCentral(iClass, pHead, pTail, iCount);
	}
}
// 内部函数：释放内存全局线程 cache
static inline void __xrtMemGlobalUnitThreadCache(xrtThreadData* pThreadData)
{
	void (*procFree)(ptr) = __xrtMemGlobalProcFree();
	xrtMemThreadCache* pCache;
	uint32 i;
	if ( pThreadData == NULL || pThreadData->tMem.pLocalAlloc == NULL ) {
		return;
	}
	pCache = (xrtMemThreadCache*)pThreadData->tMem.pLocalAlloc;
	for ( i = 0; i < pCache->iClassCount; i++ ) {
		__xrtMemGlobalDrainThreadCache(pCache, i, 0);
	}
	procFree(pCache);
	pThreadData->tMem.pLocalAlloc = NULL;
}
// 内部函数：释放内存全局计划
static inline void __xrtMemGlobalUnitPlan(xrtMemGlobalPool* pPool)
{
	void (*procFree)(ptr) = __xrtMemGlobalProcFree();
	xrtMemGlobalSpan* pSpan;
	if ( pPool == NULL ) {
		return;
	}
	pSpan = pPool->pSpanList;
	while ( pSpan ) {
		xrtMemGlobalSpan* pNext = pSpan->pNext;
		procFree(pSpan->pMemory);
		pSpan = pNext;
	}
	memset(pPool, 0, sizeof(xrtMemGlobalPool));
}
// 内部函数：分配内存全局 pooled
static inline ptr __xrtMemGlobalAllocPooled(uint32 iClass, size_t iRequestSize, bool bZero)
{
	xrtMemThreadCache* pCache = __xrtMemGlobalGetThreadCache();
	xrtMemFreeNode* pNode = NULL;
	xrtMemBlockHeader* pHeader;
	uint32 iBlockSize;
	if ( pCache && iClass < pCache->iClassCount ) {
		if ( pCache->arrFreeList[iClass] == NULL ) {
			if ( !__xrtMemGlobalRefillThreadCache(pCache, iClass) ) {
				return NULL;
			}
		}
		pNode = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode->pNext;
		pCache->arrFreeCount[iClass]--;
	} else {
		uint32 iCount = 0;
		pNode = __xrtMemGlobalPopCentral(iClass, 1, &iCount);
		if ( pNode == NULL ) {
			if ( !__xrtMemGlobalAllocSpan(iClass) ) {
				return NULL;
			}
			pNode = __xrtMemGlobalPopCentral(iClass, 1, &iCount);
			if ( pNode == NULL ) {
				return NULL;
			}
		}
	}
	pHeader = __xrtMemGlobalHeaderFromUser(pNode);
	iBlockSize = __xrtMemGlobalClassBlockSize(&xCore.MemGlobal, iClass);
	__xrtMemGlobalWriteHeader(pHeader, iClass, XRT_MEMBLOCK_FLAG_POOLED, (uint32)iRequestSize);
	if ( bZero ) {
		memset(pNode, 0, iBlockSize);
	#ifdef XRT_MEM_DEBUG
	} else {
		memset(pNode, 0xCD, iBlockSize);
	#endif
	}
	__xrtMemGlobalWriteTailCanary(pHeader);
	return pNode;
}
// 内部函数：分配内存全局 backing
static inline ptr __xrtMemGlobalAllocBacking(size_t iRequestSize, bool bZero)
{
	ptr pRaw;
	xrtMemBlockHeader* pHeader;
	size_t iAllocPayload = __xrtMemGlobalAllocPayloadSize(iRequestSize);
	size_t iAllocSize = __xrtMemGlobalHeaderSize() + iAllocPayload;
	if ( bZero ) {
		pRaw = __xrtMemGlobalProcCalloc()(1, iAllocSize);
	} else {
		pRaw = __xrtMemGlobalProcMalloc()(iAllocSize);
	}
	if ( pRaw == NULL ) {
		return NULL;
	}
	pHeader = (xrtMemBlockHeader*)pRaw;
	__xrtMemGlobalWriteHeader(pHeader, 0xFFFFu, XRT_MEMBLOCK_FLAG_BACKING, (uint32)iRequestSize);
	#ifdef XRT_MEM_DEBUG
		if ( !bZero ) {
			memset(__xrtMemGlobalUserFromHeader(pHeader), 0xCD, iAllocPayload);
		}
	#endif
	__xrtMemGlobalWriteTailCanary(pHeader);
	return __xrtMemGlobalUserFromHeader(pHeader);
}
// 内部函数：分配内存全局位置
static inline ptr __xrtMemGlobalAllocSite(size_t iSize, bool bZero, const char* sFile, uint32 iLine)
{
	uint32 iClass;
	ptr pMem;
	__xrtMemGlobalEnsurePlan();
	iClass = __xrtMemGlobalClassIndexForRequest(&xCore.MemGlobal, iSize);
	if ( iClass != (uint32)-1 ) {
		pMem = __xrtMemGlobalAllocPooled(iClass, iSize, bZero);
	} else {
		pMem = __xrtMemGlobalAllocBacking(iSize, bZero);
	}
	if ( pMem ) {
		__xrtMemDebugTrackAlloc(__xrtMemGlobalHeaderFromUser(pMem), sFile, iLine);
	}
	return pMem;
}
// 内部函数：分配内存全局
static inline ptr __xrtMemGlobalAlloc(size_t iSize, bool bZero)
{
	return __xrtMemGlobalAllocSite(iSize, bZero, NULL, 0);
}
// 内部函数：释放内存全局 free
static inline void __xrtMemGlobalFreeRelease(ptr pMem)
{
	xrtMemBlockHeader* pHeader;
	if ( pMem == NULL ) {
		return;
	}
	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		__xrtMemGlobalProcFree()(pMem);
		return;
	}
	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED ) {
		uint32 iClass = pHeader->iClassIndex;
		xrtMemThreadCache* pCache = __xrtMemGlobalGetThreadCache();
		xrtMemFreeNode* pNode = (xrtMemFreeNode*)pMem;
		if ( pCache && iClass < pCache->iClassCount ) {
			if ( pCache->arrFreeCount[iClass] >= pCache->iCacheLimit ) {
				__xrtMemGlobalDrainThreadCache(pCache, iClass, pCache->iCacheLimit / 2);
			}
			pNode->pNext = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
			pCache->arrFreeList[iClass] = pNode;
			pCache->arrFreeCount[iClass]++;
		} else {
			pNode->pNext = NULL;
			__xrtMemGlobalPushCentral(iClass, pNode, pNode, 1);
		}
		return;
	}
	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
		__xrtMemGlobalProcFree()(pHeader);
		return;
	}
	__xrtMemGlobalProcFree()(pHeader);
}
// 内部函数：释放内存全局位置
static inline void __xrtMemGlobalFreeSite(ptr pMem, const char* sFile, uint32 iLine)
{
	xrtMemBlockHeader* pHeader;
	if ( pMem == NULL ) {
		return;
	}
	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		uint32 iAllocatorKind = 0;
		size_t iForeignSize = 0;
		const char* sForeignFile = NULL;
		uint32 iForeignLine = 0;
		if ( __xrtMemDebugLookupForeignAlloc(pMem, &iAllocatorKind, &iForeignSize, &sForeignFile, &iForeignLine) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE, pMem, iForeignSize, iAllocatorKind, sFile, iLine);
			xrtSetError("memory belongs to an explicit pool allocator.", FALSE);
			return;
		}
		#ifdef XRT_MEM_DEBUG
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_INVALID_FREE, pMem, 0, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("invalid free detected.", FALSE);
			return;
		#else
			__xrtMemGlobalProcFree()(pMem);
			return;
		#endif
	}
	#ifdef XRT_MEM_DEBUG
		if ( pHeader->iDebugState != XRT_MEMDEBUG_STATE_LIVE ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("double free detected.", FALSE);
			return;
		}
		if ( !__xrtMemGlobalCheckFrontCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		if ( !__xrtMemGlobalCheckTailCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		__xrtMemDebugTrackFree(pHeader, sFile, iLine);
		memset(pMem, 0xDD, __xrtMemGlobalAllocPayloadSize(pHeader->iRequestSize));
		if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
			__xrtMemDebugLock();
			pHeader->iDebugState = XRT_MEMDEBUG_STATE_QUARANTINE;
			pHeader->pDebugPrev = xCore.MemDebug.pQuarantineTail;
			pHeader->pDebugNext = NULL;
			if ( xCore.MemDebug.pQuarantineTail ) {
				xCore.MemDebug.pQuarantineTail->pDebugNext = pHeader;
			} else {
				xCore.MemDebug.pQuarantineHead = pHeader;
			}
			xCore.MemDebug.pQuarantineTail = pHeader;
			xCore.MemDebug.iQuarantineCount++;
			xCore.MemDebug.iQuarantineBytes += pHeader->iRequestSize;
			while ( xCore.MemDebug.iQuarantineCount > XRT_MEMDEBUG_QUARANTINE_LIMIT && xCore.MemDebug.pQuarantineHead ) {
				xrtMemBlockHeader* pOld = xCore.MemDebug.pQuarantineHead;
				xCore.MemDebug.pQuarantineHead = pOld->pDebugNext;
				if ( xCore.MemDebug.pQuarantineHead ) {
					xCore.MemDebug.pQuarantineHead->pDebugPrev = NULL;
				} else {
					xCore.MemDebug.pQuarantineTail = NULL;
				}
				pOld->pDebugPrev = NULL;
				pOld->pDebugNext = NULL;
				if ( xCore.MemDebug.iQuarantineCount > 0 ) {
					xCore.MemDebug.iQuarantineCount--;
				}
				if ( xCore.MemDebug.iQuarantineBytes >= pOld->iRequestSize ) {
					xCore.MemDebug.iQuarantineBytes -= pOld->iRequestSize;
				} else {
					xCore.MemDebug.iQuarantineBytes = 0;
				}
				__xrtMemGlobalProcFree()(pOld);
			}
			__xrtMemDebugUnlock();
			return;
		}
	#endif
	__xrtMemGlobalFreeRelease(pMem);
}
// 内部函数：释放内存全局
static inline void __xrtMemGlobalFree(ptr pMem)
{
	__xrtMemGlobalFreeSite(pMem, NULL, 0);
}
// 内部函数：重新分配内存全局位置
static inline ptr __xrtMemGlobalReallocSite(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	xrtMemBlockHeader* pHeader;
	size_t iOldSize;
	if ( pMem == NULL ) {
		return __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
	}
	if ( iSize == 0 ) {
		__xrtMemGlobalFreeSite(pMem, sFile, iLine);
		return NULL;
	}
	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		uint32 iAllocatorKind = 0;
		size_t iForeignSize = 0;
		if ( __xrtMemDebugLookupForeignAlloc(pMem, &iAllocatorKind, &iForeignSize, NULL, NULL) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE, pMem, iForeignSize, iAllocatorKind, sFile, iLine);
			xrtSetError("memory belongs to an explicit pool allocator.", FALSE);
			return NULL;
		}
		return __xrtMemGlobalProcRealloc()(pMem, iSize);
	}
	#ifdef XRT_MEM_DEBUG
		if ( pHeader->iDebugState != XRT_MEMDEBUG_STATE_LIVE ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("realloc on freed memory detected.", FALSE);
			return NULL;
		}
		if ( !__xrtMemGlobalCheckFrontCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		if ( !__xrtMemGlobalCheckTailCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
	#endif
	iOldSize = pHeader->iRequestSize;
	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED ) {
		uint32 iOldClass = pHeader->iClassIndex;
		uint32 iNewClass = __xrtMemGlobalClassIndexForRequest(&xCore.MemGlobal, iSize);
		if ( iNewClass == iOldClass ) {
			pHeader->iRequestSize = (uint32)iSize;
			__xrtMemGlobalWriteTailCanary(pHeader);
			__xrtMemDebugTrackReallocInPlace(pHeader, iOldSize, sFile, iLine);
			return pMem;
		}
	} else if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
		#ifndef XRT_MEM_DEBUG
		if ( iSize > xCore.MemGlobal.iCutoff ) {
			size_t iAllocPayload = __xrtMemGlobalAllocPayloadSize(iSize);
			size_t iAllocSize = __xrtMemGlobalHeaderSize() + iAllocPayload;
			xrtMemBlockHeader* pNewHeader = __xrtMemGlobalProcRealloc()(pHeader, iAllocSize);
			if ( pNewHeader == NULL ) {
				return NULL;
			}
			__xrtMemGlobalWriteHeader(pNewHeader, 0xFFFFu, XRT_MEMBLOCK_FLAG_BACKING, (uint32)iSize);
			__xrtMemGlobalWriteTailCanary(pNewHeader);
			return __xrtMemGlobalUserFromHeader(pNewHeader);
		}
		#endif
	}
	{
		ptr pNewMem = __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
		if ( pNewMem == NULL ) {
			return NULL;
		}
		memcpy(pNewMem, pMem, iOldSize < iSize ? iOldSize : iSize);
		__xrtMemGlobalFreeSite(pMem, sFile, iLine);
		return pNewMem;
	}
}
// 内部函数：重新分配内存全局
static inline ptr __xrtMemGlobalRealloc(ptr pMem, size_t iSize)
{
	return __xrtMemGlobalReallocSite(pMem, iSize, NULL, 0);
}

// ========================================
// File: D:/git/xrt/lib/base.h
// ========================================


// 申请内存
#define __XRT_MEMTELEMETRY_OP_MALLOC 1
#define __XRT_MEMTELEMETRY_OP_CALLOC 2
#define __XRT_MEMTELEMETRY_OP_REALLOC 3
static inline size_t __xrtMemTelemetryMulClamp(size_t iNum, size_t iSize);
static inline uint32 __xrtMemTelemetryClassIndex(size_t iSize);
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize);
static inline void __xrtMemTelemetryRecordFree();
static inline void __xrtMemTelemetryRecordTemp(size_t iSize);
// 内部函数：分配位置
static inline ptr __xrtMallocSite(size_t iSize, const char* sFile, uint32 iLine)
{
	ptr mem = __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
	if ( mem == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}
	__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_MALLOC, iSize);
	return mem;
}
// 内部函数：分配位置
static inline ptr __xrtCallocSite(size_t iNum, size_t iSize, const char* sFile, uint32 iLine)
{
	size_t iTotal = __xrtMemTelemetryMulClamp(iNum, iSize);
	ptr mem = __xrtMemGlobalAllocSite(iTotal, TRUE, sFile, iLine);
	if ( mem == NULL ) {
		xrtSetError("class memory allocate failed.", FALSE);
	} else {
		__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_CALLOC, iTotal);
	}
	return mem;
}
// 内部函数：重新分配位置
static inline ptr __xrtReallocSite(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	ptr mem;
	if ( pMem == xCore.sNull ) {
		pMem = NULL;
	}
	mem = __xrtMemGlobalReallocSite(pMem, iSize, sFile, iLine);
	if ( mem == NULL ) {
		str sError = xrtGetError();
		if ( iSize != 0 && (sError == NULL || sError == xCore.sNull || sError[0] == '\0') ) {
			xrtSetError("memory reallocate failed.", FALSE);
		}
	} else {
		__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_REALLOC, iSize);
	}
	return mem;
}
// 内部函数：释放位置
static inline void __xrtFreeSite(ptr pmem, const char* sFile, uint32 iLine)
{
	if ( pmem && (pmem != xCore.sNull) ) {
		__xrtMemTelemetryRecordFree();
		__xrtMemGlobalFreeSite(pmem, sFile, iLine);
	}
}
#ifdef XRT_MEM_DEBUG
// 分配调试
XXAPI ptr xrtMallocDbg(size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtMallocSite(iSize, sFile, iLine);
}
#endif
// 分配
XXAPI ptr xrtMalloc(size_t iSize)
{
	return __xrtMallocSite(iSize, NULL, 0);
}
// 申请类内存
#ifdef XRT_MEM_DEBUG
// 分配调试
XXAPI ptr xrtCallocDbg(size_t iNum, size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtCallocSite(iNum, iSize, sFile, iLine);
}
#endif
// 分配
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	return __xrtCallocSite(iNum, iSize, NULL, 0);
}
// 重新申请内存
#ifdef XRT_MEM_DEBUG
// 重新分配调试
XXAPI ptr xrtReallocDbg(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtReallocSite(pMem, iSize, sFile, iLine);
}
#endif
// 重新分配
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	return __xrtReallocSite(pMem, iSize, NULL, 0);
}
// 释放内存（ 会先判断是否为 null ）
#ifdef XRT_MEM_DEBUG
// 释放调试
XXAPI void xrtFreeDbg(ptr pmem, const char* sFile, uint32 iLine)
{
	__xrtFreeSite(pmem, sFile, iLine);
}
#endif
// 释放
XXAPI void xrtFree(ptr pmem)
{
	__xrtFreeSite(pmem, NULL, 0);
}
// 内部函数：获取临时内存区块头部大小
static inline size_t __xrtTempArenaBlockHeaderSize()
{
	return __xrtMemGlobalAlignSize(sizeof(xrtTempArenaBlock));
}
// 内部函数：获取临时内存区块用户区指针
static inline ptr __xrtTempArenaBlockUser(xrtTempArenaBlock* pBlock)
{
	return (ptr)((char*)pBlock + __xrtTempArenaBlockHeaderSize());
}
// 内部函数：分配临时内存区块
static inline xrtTempArenaBlock* __xrtTempArenaAllocBlock(size_t iCapacity)
{
	size_t iTotal;
	size_t iHeaderSize;
	xrtTempArenaBlock* pBlock;
	if ( iCapacity == 0 ) {
		iCapacity = 1;
	}
	iHeaderSize = __xrtTempArenaBlockHeaderSize();
	if ( iCapacity > UINT_MAX || iHeaderSize > (SIZE_MAX - iCapacity) ) {
		xrtSetError("temporary memory block size overflow.", FALSE);
		return NULL;
	}
	iTotal = iHeaderSize + iCapacity;
	pBlock = (xrtTempArenaBlock*)__xrtMemGlobalProcMalloc()(iTotal);
	if ( pBlock == NULL ) {
		return NULL;
	}
	memset(pBlock, 0, iHeaderSize);
	pBlock->iCapacity = (uint32)iCapacity;
	return pBlock;
}
// 内部函数：记录临时内存区分配调试信息
static inline void __xrtTempArenaDebugOnAlloc(xrtThreadData* pThreadData, size_t iSize, bool bSpill)
{
	#ifdef XRT_MEM_DEBUG
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugLock();
		pThreadData->tTemp.iCurrentBytes += iSize;
		if ( pThreadData->tTemp.iCurrentBytes > pThreadData->tTemp.iPeakBytes ) {
			pThreadData->tTemp.iPeakBytes = pThreadData->tTemp.iCurrentBytes;
		}
		xCore.MemDebug.iTempCurrentBytes += iSize;
		if ( xCore.MemDebug.iTempCurrentBytes > xCore.MemDebug.iTempPeakBytes ) {
			xCore.MemDebug.iTempPeakBytes = xCore.MemDebug.iTempCurrentBytes;
		}
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_ALLOC, NULL, iSize, bSpill ? XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL : XRT_MEMDEBUG_ALLOCATOR_GLOBAL, NULL, 0);
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
		(void)iSize;
		(void)bSpill;
	#endif
}
// 内部函数：记录临时内存区重置调试信息
static inline void __xrtTempArenaDebugOnReset(xrtThreadData* pThreadData)
{
	#ifdef XRT_MEM_DEBUG
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugLock();
		if ( xCore.MemDebug.iTempCurrentBytes >= pThreadData->tTemp.iCurrentBytes ) {
			xCore.MemDebug.iTempCurrentBytes -= pThreadData->tTemp.iCurrentBytes;
		} else {
			xCore.MemDebug.iTempCurrentBytes = 0;
		}
		xCore.MemDebug.iTempResetCount++;
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_RESET, NULL, pThreadData->tTemp.iCurrentBytes, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, NULL, 0);
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
	#endif
}
// 内部函数：确保临时内存区当前
static inline bool __xrtTempArenaEnsureCurrent(xrtThreadData* pThreadData, size_t iNeed)
{
	xrtTempArenaBlock* pBlock;
	size_t iCapacity;
	if ( pThreadData == NULL ) {
		return FALSE;
	}
	if ( pThreadData->tTemp.pCurrent && ((size_t)pThreadData->tTemp.pCurrent->iUsed + iNeed) <= pThreadData->tTemp.pCurrent->iCapacity ) {
		return TRUE;
	}
	if ( pThreadData->tTemp.pCurrent && pThreadData->tTemp.pCurrent->pNext ) {
		xrtTempArenaBlock* pNext = pThreadData->tTemp.pCurrent->pNext;
		while ( pNext ) {
			if ( ((size_t)pNext->iUsed + iNeed) <= pNext->iCapacity ) {
				pThreadData->tTemp.pCurrent = pNext;
				return TRUE;
			}
			pNext = pNext->pNext;
		}
	}
	if ( pThreadData->tTemp.pCurrent == NULL && pThreadData->tTemp.pBlocks ) {
		xrtTempArenaBlock* pNext = pThreadData->tTemp.pBlocks;
		while ( pNext ) {
			if ( ((size_t)pNext->iUsed + iNeed) <= pNext->iCapacity ) {
				pThreadData->tTemp.pCurrent = pNext;
				return TRUE;
			}
			pNext = pNext->pNext;
		}
	}
	iCapacity = pThreadData->tTemp.iBlockSize ? pThreadData->tTemp.iBlockSize : XRT_TEMP_ARENA_BLOCK_SIZE;
	if ( iNeed > iCapacity ) {
		iCapacity = iNeed;
	}
	pBlock = __xrtTempArenaAllocBlock(iCapacity);
	if ( pBlock == NULL ) {
		return FALSE;
	}
	if ( pThreadData->tTemp.pBlocks == NULL ) {
		pThreadData->tTemp.pBlocks = pBlock;
	} else if ( pThreadData->tTemp.pCurrent ) {
		pThreadData->tTemp.pCurrent->pNext = pBlock;
	} else {
		xrtTempArenaBlock* pTail = pThreadData->tTemp.pBlocks;
		while ( pTail->pNext ) {
			pTail = pTail->pNext;
		}
		pTail->pNext = pBlock;
	}
	pThreadData->tTemp.pCurrent = pBlock;
	return TRUE;
}
// 内部函数：重置临时内存区线程
static inline void __xrtTempArenaResetThread(xrtThreadData* pThreadData)
{
	xrtTempArenaBlock* pBlock;
	xrtTempArenaBlock* pSpill;
	if ( pThreadData == NULL ) {
		return;
	}
	__xrtTempArenaDebugOnReset(pThreadData);
	for ( pBlock = pThreadData->tTemp.pBlocks; pBlock; pBlock = pBlock->pNext ) {
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pBlock), 0xE7, pBlock->iCapacity);
		#endif
		pBlock->iUsed = 0;
	}
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pSpill), 0xE7, pSpill->iCapacity);
		#endif
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	pThreadData->tTemp.pCurrent = pThreadData->tTemp.pBlocks;
	pThreadData->tTemp.pSpill = NULL;
	pThreadData->tTemp.iCurrentBytes = 0;
	pThreadData->tTemp.iResetCount++;
}
// 内部函数：释放临时内存区全部线程
static inline void __xrtTempArenaFreeAllThread(xrtThreadData* pThreadData)
{
	xrtTempArenaBlock* pBlock;
	xrtTempArenaBlock* pSpill;
	if ( pThreadData == NULL ) {
		return;
	}
	__xrtTempArenaDebugOnReset(pThreadData);
	pBlock = pThreadData->tTemp.pBlocks;
	while ( pBlock ) {
		xrtTempArenaBlock* pNext = pBlock->pNext;
		__xrtMemGlobalProcFree()(pBlock);
		pBlock = pNext;
	}
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	memset(&pThreadData->tTemp, 0, sizeof(xrtTempArenaState));
	pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
}
// 申请无需主动释放的临时内存（线程级）
XXAPI ptr xrtTempMemory(size_t iSize)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	size_t iNeed;
	bool bSpill;
	xrtTempArenaBlock* pBlock;
	ptr pRet;
	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return NULL;
	}
	// 申请内存
	if ( pThreadData->tTemp.iBlockSize == 0 ) {
		pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	}
	if ( pThreadData->tTemp.iSpillCutoff == 0 ) {
		pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
	}
	iNeed = __xrtMemGlobalAlignSize(iSize ? iSize : 1);
	bSpill = iNeed > pThreadData->tTemp.iSpillCutoff;
	if ( bSpill ) {
		pBlock = __xrtTempArenaAllocBlock(iNeed);
		if ( pBlock == NULL ) {
			xrtSetError("temporary memory allocate failed.", FALSE);
			return NULL;
		}
		pBlock->iUsed = (uint32)iNeed;
		pBlock->iFlags = 1u;
		pBlock->pNext = pThreadData->tTemp.pSpill;
		pThreadData->tTemp.pSpill = pBlock;
		pRet = __xrtTempArenaBlockUser(pBlock);
	} else {
		if ( !__xrtTempArenaEnsureCurrent(pThreadData, iNeed) ) {
			xrtSetError("temporary memory allocate failed.", FALSE);
			return NULL;
		}
		pBlock = pThreadData->tTemp.pCurrent;
		pRet = (ptr)((char*)__xrtTempArenaBlockUser(pBlock) + pBlock->iUsed);
		pBlock->iUsed += (uint32)iNeed;
	}
	__xrtMemTelemetryRecordTemp(iSize);
	__xrtTempArenaDebugOnAlloc(pThreadData, iNeed, bSpill);
	return pRet;
}
// 释放当前线程的所有临时内存
XXAPI void xrtFreeTempMemory()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		return;
	}
	__xrtTempArenaResetThread(pThreadData);
}
// 设置错误（线程级）
XXAPI void xrtSetError(const void* sError, bool bFree)
{
	str sErrorText = (str)sError;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	// 回调通知
	if ( xCore.OnError ) {
		xCore.OnError(sErrorText);
	}
	if ( pThreadData == NULL ) {
		return;
	}
	// 释放旧的错误信息
	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = sErrorText;
	pThreadData->bFreeLastError = bFree;
}
// 设置错误 u 16
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF16to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}
// 设置错误 u 32
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF32to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}
// 清除当前线程错误
XXAPI void xrtClearError()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		return;
	}
	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = xCore.sNull;
	pThreadData->bFreeLastError = FALSE;
}
// 内部函数：内存遥测乘法钳制相关处理
static inline size_t __xrtMemTelemetryMulClamp(size_t iNum, size_t iSize)
{
	if ( iNum == 0 || iSize == 0 ) {
		return 0;
	}
	if ( iNum > (SIZE_MAX / iSize) ) {
		return SIZE_MAX;
	}
	return iNum * iSize;
}
// 内部函数：获取内存遥测分类 index
static inline uint32 __xrtMemTelemetryClassIndex(size_t iSize)
{
	if ( xCore.MemGlobal.iClassCount == XRT_MEMPOOL_CLASS_COUNT_DEFAULT && iSize <= xCore.MemGlobal.iCutoff ) {
		if ( iSize == 0 ) {
			return 0;
		}
		return xCore.MemGlobal.arrSizeClassLut[iSize];
	}
	if ( iSize <= 1 ) {
		return 0;
	}
	if ( iSize >= XRT_MEMPOOL_CUTOFF_DEFAULT ) {
		return XRT_MEMPOOL_CLASS_COUNT_DEFAULT - 1;
	}
	return (uint32)(((iSize + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE) - 1);
}
// 内部函数：__xrtMemTelemetryRecordSizedOp
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	uint32 iIdx;
	if ( pState->bEnabled == 0 ) {
		return;
	}
	switch ( iOp ) {
		case __XRT_MEMTELEMETRY_OP_MALLOC:
			__xrtAtomicAddFetch64(&pState->iMallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iMallocBytes, (int64)iSize);
			break;
		case __XRT_MEMTELEMETRY_OP_CALLOC:
			__xrtAtomicAddFetch64(&pState->iCallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iCallocBytes, (int64)iSize);
			break;
		case __XRT_MEMTELEMETRY_OP_REALLOC:
			__xrtAtomicAddFetch64(&pState->iReallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iReallocBytes, (int64)iSize);
			break;
		default:
			return;
	}
	if ( iSize <= XRT_MEMPOOL_CUTOFF_DEFAULT ) {
		iIdx = __xrtMemTelemetryClassIndex(iSize);
		__xrtAtomicAddFetch64(&pState->iPooledCandidateCalls, 1);
		__xrtAtomicAddFetch64(&pState->iPooledCandidateBytes, (int64)iSize);
		__xrtAtomicAddFetch64(&pState->arrClassCalls[iIdx], 1);
		__xrtAtomicAddFetch64(&pState->arrClassBytes[iIdx], (int64)iSize);
	} else {
		__xrtAtomicAddFetch64(&pState->iFallbackCalls, 1);
		__xrtAtomicAddFetch64(&pState->iFallbackBytes, (int64)iSize);
	}
}
// 内部函数：释放内存遥测记录
static inline void __xrtMemTelemetryRecordFree()
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	if ( pState->bEnabled == 0 ) {
		return;
	}
	__xrtAtomicAddFetch64(&pState->iFreeCalls, 1);
}
// 内部函数：内存遥测记录临时相关处理
static inline void __xrtMemTelemetryRecordTemp(size_t iSize)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	if ( pState->bEnabled == 0 ) {
		return;
	}
	// temp arena 单独统计，不折算进 malloc/free/pooled 候选通道。
	__xrtAtomicAddFetch64(&pState->iTempCalls, 1);
	__xrtAtomicAddFetch64(&pState->iTempBytes, (int64)iSize);
}

// ========================================
// File: D:/git/xrt/lib/string.h
// ========================================


#define __xrt_cstr(sText) ((const char*)(sText))
#define __xrt_str(sText) ((char*)(sText))
// 内部函数：HEX 字符转数值
static bool __xrtHexDecodeNibble(uint8 c, uint8* pNibble)
{
	if ( pNibble == NULL ) {
		return FALSE;
	}
	if ( c >= '0' && c <= '9' ) {
		*pNibble = (uint8)(c - '0');
		return TRUE;
	}
	if ( c >= 'A' && c <= 'F' ) {
		*pNibble = (uint8)(c - 'A' + 10u);
		return TRUE;
	}
	if ( c >= 'a' && c <= 'f' ) {
		*pNibble = (uint8)(c - 'a' + 10u);
		return TRUE;
	}
	return FALSE;
}
// 内部函数：安全获取 UTF-8 字符长度
static size_t __xrtUtf8CharLenSafe(str sText, size_t iSize, size_t iPos)
{
	size_t iCharLen;
	if ( !sText || iPos >= iSize ) { return 0; }
	iCharLen = (size_t)xrtCharLenU8((unsigned char)sText[iPos]);
	if ( (iCharLen == 0) || (iCharLen > (iSize - iPos)) ) {
		return 1;
	}
	return iCharLen;
}
// 内部函数：检查字节序列是否在集合中
static bool __xrtStrHasToken(str sText, size_t iSize, const char* sToken, size_t iTokenSize)
{
	if ( !sText || !sToken || (iTokenSize == 0) || (iSize < iTokenSize) ) { return FALSE; }
	for ( size_t i = 0; (i + iTokenSize) <= iSize; i++ ) {
		if ( memcmp(&sText[i], sToken, iTokenSize) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}
// 创建字符串副本（ 需使用 xrtFree 释放 ）(线程安全)
XXAPI str xrtCopyStr(str sText, size_t iSize)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	return sRet;
}
// 复制字符串 u 16
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { return (u16str)xCore.sNull; }
	if ( iSize > ((SIZE_MAX / sizeof(unsigned short)) - 1u) ) { return (u16str)xCore.sNull; }
	u16str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) { return (u16str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned short));
	sRet[iSize] = 0;
	return sRet;
}
// 复制字符串 u 32
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { return (u32str)xCore.sNull; }
	if ( iSize > ((SIZE_MAX / sizeof(unsigned int)) - 1u) ) { return (u32str)xCore.sNull; }
	u32str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) { return (u32str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned int));
	sRet[iSize] = 0;
	return sRet;
}
// 复制内存
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { return xCore.sNull; }
	ptr pRet = xrtMalloc(iSize);
	if ( pRet == NULL ) { return xCore.sNull; }
	memcpy(pRet, pMem, iSize);
	return pRet;
}
// 比较字符串
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase)
{
	if ( s1 == NULL || s2 == NULL ) {
		if ( s1 == s2 ) { return 0; }
		return s1 ? 1 : -1;
	}
	if ( iSize > 0 ) {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return strnicmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
			#else
				// 其他平台方案
				return strncasecmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
			#endif
		} else {
			return strncmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
		}
	} else {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return stricmp(__xrt_cstr(s1), __xrt_cstr(s2));
			#else
				// 其他平台方案
				return strcasecmp(__xrt_cstr(s1), __xrt_cstr(s2));
			#endif
		} else {
			return strcmp(__xrt_cstr(s1), __xrt_cstr(s2));
		}
	}
}
// 字符串转为小写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = tolower(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
	}
	return sRet;
}
// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise != FALSE ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = toupper(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
	}
	return sRet;
}
// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）(线程安全)
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	if ( sText == NULL ) { return NULL; }
	if ( sSubText == NULL ) { return NULL; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return NULL; }
	str sSub;
	if ( bCase ) {
		str sText1 = xrtLCase(sText, 0, FALSE);
		str sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			sSub = &sText[sSub - sText1];
		}
		xrtFree(sText1);
		xrtFree(sText2);
	} else {
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	return sSub;
}
// xrtInStr 相关处理
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	if ( sText == NULL ) { return 0; }
	if ( sSubText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return 0; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return 0; }
	str sSub;
	if ( bCase ) {
		str sText1 = xrtLCase(sText, 0, FALSE);
		str sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			sSub = &sText[sSub - sText1];
		}
		xrtFree(sText1);
		xrtFree(sText2);
	} else {
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	if ( sSub ) {
		return (sSub - sText) + 1;
	} else {
		return 0;
	}
}
// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize)
{
	if ( (sText == NULL) || (sSubText == NULL) ) { return NULL; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return NULL; }
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( __xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			return &sText[i];
		}
		i += iCharLen;
	}
	return NULL;
}
// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	size_t iCount = 0;
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			break;
		}
		iCount += iCharLen;
		i += iCharLen;
	}
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCount], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCount], iSize - iCount);
	}
}
// xrtRTrim 相关处理
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( size_t j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b10000000 ) {
			// UTF-8 续字节，向前查找首字节
			int iEnd = i;
			while ( (i > 0) && ((sText[i] & 0b11000000) == 0b10000000) ) {
				i--;
			}
			// 现在 i 指向首字节
			int iCharLen = iEnd - i + 1;
			if ( (size_t)iCharLen <= iSubSize ) {
				size_t iLen = iSubSize - iCharLen + 1;
				for ( size_t j = 0; j < iLen; j++ ) {
					bool bMatch = TRUE;
					for ( int k = 0; k < iCharLen; k++ ) {
						if ( sSubText[j + k] != sText[i + k] ) {
							bMatch = FALSE;
							break;
						}
					}
					if ( bMatch ) {
						iCount += iCharLen;
						bBreak = FALSE;
						break;
					}
				}
			}
			// i 已经在首字节，循环会 i-- 移到前一个字符末尾
		} else {
			// 孤立的首字节或异常字符（FE、FF），跳过
		}
		if ( bBreak ) {
			break;
		}
	}
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(sText, iSize - iCount);
	}
}
// 裁剪
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	size_t iCountL = 0;
	size_t iCountR = 0;
	// 裁剪左侧
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			break;
		}
		iCountL += iCharLen;
		i += iCharLen;
	}
	// 全部裁剪需要特殊处理
	if ( iCountL >= iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 裁剪右侧
	for ( size_t i = iSize; i > iCountL; ) {
		size_t iStart = i - 1;
		size_t iCharLen = 1;
		while ( (iStart > iCountL) && (((unsigned char)sText[iStart] & 0b11000000u) == 0b10000000u) ) {
			iStart--;
		}
		iCharLen = i - iStart;
		if ( __xrtUtf8CharLenSafe(sText, iSize, iStart) != iCharLen ) {
			iStart = i - 1;
			iCharLen = 1;
		}
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[iStart], iCharLen) ) {
			break;
		}
		iCountR += iCharLen;
		i = iStart;
	}
	size_t iCount = iCountL + iCountR;
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCountL], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCountL], iSize - iCount);
	}
}
// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
	// 不改动源数据时，直接创建副本
	if ( bSrcRevise == FALSE ) {
		sText = xrtCopyStr(sText, iSize);
	}
	size_t iCount = 0;
	size_t iWrite = 0;
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( __xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			iCount += iCharLen;
		} else {
			if ( iWrite != i ) {
				memmove(&sText[iWrite], &sText[i], iCharLen);
			}
			iWrite += iCharLen;
		}
		i += iCharLen;
	}
	sText[iWrite] = 0;
	if ( iRetSize ) { *iRetSize = iWrite; }
	return sText;
}
// 字符串格式化（ 需使用 xrtFree 释放 ）
XXAPI str xrtFormat(const void* sFormat, ...)
{
	const char* sFormatText = __xrt_cstr(sFormat);
	if ( sFormatText == NULL ) { return xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormatText, ip);
	va_end(ip);
	if ( iSize > 0 ) {
		str sRet = xrtMalloc(iSize + 1);
		if ( sRet == NULL ) { return xCore.sNull; }
		va_start(ip, sFormat);
		iSize = vsnprintf(__xrt_str(sRet), iSize + 1, sFormatText, ip);
		va_end(ip);
		sRet[iSize] = 0;
		return sRet;
	} else {
		return xCore.sNull;
	}
}
// 字符串替换（ 需使用 xrtFree 释放 ）
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( sRepText == NULL ) { iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = strlen(__xrt_cstr(sRepText)); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	str sTextPtr;
	str sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (size_t)(sTextPtr - sText), sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRet = iSize + iFindCount * (iRepSize - iSubSize);
	str sRet = (str)xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (str)xCore.sNull; }
	// 复制原始字符串, 替换需要改变的部分
	str sRetPtr = sRet;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (size_t)(sTextPtr - sText), sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		size_t iSkipSize = sSubPos - sTextPtr;
		// 复制前面的部分，直到出现要查找的字符串
		strncpy(__xrt_str(sRetPtr), __xrt_cstr(sTextPtr), iSkipSize);
		sRetPtr += iSkipSize;
		// 复制要替换的字符串
		strncpy(__xrt_str(sRetPtr), __xrt_cstr(sRepText), iRepSize);
		sRetPtr += iRepSize;
	}
	// 复制最后一段剩下的字符串
	if ( &sText[iSize] > sTextPtr ) {
		memcpy(sRetPtr, sTextPtr, &sText[iSize] - sTextPtr);
	}
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}
// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { goto return_nullstr; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { goto return_nullstr; }
	if ( sSepText == NULL ) { goto return_nullsep; }
	if ( iSepSize == 0 ) { iSepSize = strlen(__xrt_cstr(sSepText)); }
	if ( iSepSize == 0 ) { goto return_nullsep; }
	// 统计分隔符出现的次数
	int iCount = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( size_t j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			iCount++;
			i += iSepSize - 1;
		}
	}
	// 如果字符串没有被分割，按照分隔符为空处理
	if ( iCount == 0 ) {
		goto return_nullsep;
	}
	// 准备返回的数据 [分割指针 + NULL + 字符串表 + \0]
	str* sRet;
	str pData;
	if ( bSrcRevise ) {
		sRet = xrtMalloc( (iCount + 2) * sizeof(ptr) );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = sText;
	} else {
		sRet = xrtMalloc( ((iCount + 2) * sizeof(ptr)) + (iSize - ((iSepSize - 1) * iCount)) + 1 );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = (str)&sRet[iCount + 2];
	}
	// 开始分割数据
	iCount = 0;
	int iPos = 0;
	str pAddr = pData;
	for ( size_t i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( size_t j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			// 找到分隔符
			sRet[iCount] = pAddr;
			iCount++;
			if ( bSrcRevise ) {
				pData[i] = 0;
				pAddr = pData + i + iSepSize;
			} else {
				pData[iPos] = 0;
				iPos++;
				pAddr = &pData[iPos];
			}
			i += iSepSize - 1;
		} else {
			// 没找到分隔符（不修改源数据时负责数据拷贝）
			if ( bSrcRevise == FALSE ) {
				pData[iPos] = sText[i];
				iPos++;
			}
		}
	}
	if ( bSrcRevise == FALSE ) {
		pData[iPos] = 0;
	}
	sRet[iCount] = pAddr;
	iCount++;
	sRet[iCount] = NULL;
	if ( iRetSize ) { *iRetSize = iCount; }
	return sRet;
	
// 处理内容为 空字符串 或 NULL 的情况（只返回包含一个空元素的数组）
return_nullstr:
	sRet = xrtMalloc(2 * sizeof(void*));
	if ( sRet == NULL ) {
		goto return_error;
	}
	sRet[0] = xCore.sNull;
	sRet[1] = NULL;
	if ( iRetSize ) { *iRetSize = 1; }
	return sRet;
	
// 处理分隔符为 空字符串 或 NULL 的情况（只返回包含一个内容的数组）
return_nullsep:
	if ( bSrcRevise ) {
		sRet = xrtMalloc(2 * sizeof(void*));
		if ( sRet == NULL ) {
			goto return_error;
		}
		sRet[0] = sText;
	} else {
		sRet = xrtMalloc((2 * sizeof(void*)) + iSize + 1);
		if ( sRet == NULL ) {
			goto return_error;
		}
		str sTextRef = (str)&sRet[2];
		memcpy(sTextRef, sText, iSize);
		sTextRef[iSize] = 0;
		sRet[0] = sTextRef;
	}
	sRet[1] = NULL;
	if ( iRetSize ) { *iRetSize = 1; }
	return sRet;
	
// 内存申请异常返回
return_error:
	if ( iRetSize ) { *iRetSize = 0; }
	return (str*)xCore.sNull;
}
// 生成随机字符串（ 需使用 xrtFree 释放 ）
static const str RandStringDefaultTemplate = (str)"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
// xrtRandStr 相关处理
XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen)
{
	if ( sTemplate == NULL ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sTemplate)); }
	if ( iSize == 0 ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iLen == 0 ) { return xCore.sNull; }
	iSize--;
	str sRet = xrtMalloc(iLen + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		int idx = xrtRandRange(0, iSize);
		sRet[i] = sTemplate[idx];
	}
	sRet[iLen] = 0;
	return sRet;
}
// HEX 编码（ 需使用 xrtFree 释放 ）
#define dec2hex(c) (c > 9 ? c + 55 : c + '0')
// xrtHexEncode 相关处理
XXAPI str xrtHexEncode(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc((iSize * 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	uint8* pStr = pMem;
	int iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		int i1 = (pStr[i] & 0xF0) >> 4;
		int i2 = pStr[i] & 0x0F;
		sRet[iPos++] = dec2hex(i1);
		sRet[iPos++] = dec2hex(i2);
	}
	sRet[iPos] = 0;
	return sRet;
}
// xrtHexDecode 相关处理
XXAPI ptr xrtHexDecode(str sText, size_t iSize)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	if ( (iSize & 1u) != 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc((iSize / 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	int iPos = 0;
	for ( size_t i = 0; i < iSize; i += 2 ) {
		uint8 c0 = (uint8)sText[i];
		uint8 c1 = (uint8)sText[i + 1];
		uint8 iHigh;
		uint8 iLow;
		if ( !__xrtHexDecodeNibble(c0, &iHigh) || !__xrtHexDecodeNibble(c1, &iLow) ) {
			xrtFree(sRet);
			return xCore.sNull;
		}
		sRet[iPos++] = (char)((iHigh << 4) | iLow);
	}
	sRet[iPos] = 0;
	return sRet;
}
// Base64 编码（ 需使用 xrtFree 释放 ）
static const str Base64EncodeTable = (str)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// xrtBase64Encode 相关处理
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	// 申请返回值内存
	size_t iRet= 4 * ((iSize + 2) / 3);
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	// 开始编码
	uint8* pStr = pMem;
	for ( size_t i = 0, j = 0; i < iSize; ) {
		uint32_t octet_a = i < iSize ? pStr[i++] : 0;
		uint32_t octet_b = i < iSize ? pStr[i++] : 0;
		uint32_t octet_c = i < iSize ? pStr[i++] : 0;
		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
		sRet[j++] = sTable[(triple >> 3 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 2 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 1 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 0 * 6) & 0x3F];
	}
	// 添加填充字符 '='
	for ( size_t i = 0; i < (3 - iSize % 3) % 3; i++ ) {
		sRet[iRet - 1 - i] = '=';
	}
	// 返回编码后的数据
	sRet[iRet] = 0;
	return sRet;
}
// Base64 解码（ 需使用 xrtFree 释放 ）
static const str sErrorBase64_mul4 = (str)"Base64 input length must be multiple of 4 !";
static const str sErrorBase64_char = (str)"Base64 input contains invalid characters !";
// xrtBase64Decode 相关处理
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	int8_t Base64DecodeTable[256];
	memset(Base64DecodeTable, -1, sizeof(Base64DecodeTable));
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	for ( int i = 0; i < 64; i++ ) {
		Base64DecodeTable[(unsigned char)sTable[i]] = i;
	}
	// 计算输出缓冲区大小
	if ( iSize % 4 != 0 ) {
		xrtSetError(sErrorBase64_mul4, FALSE);
		return xCore.sNull;
	}
	// 计算返回长度
	int iRet = iSize / 4 * 3;
	if ( sText[iSize - 1] == '=' ) { iRet--; }
	if ( sText[iSize - 2] == '=' ) { iRet--; }
	// 申请返回值内存
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	// 开始解码
	for ( size_t i = 0, j = 0; i < iSize; ) {
		unsigned char cA = (unsigned char)sText[i];
		int8_t sextet_a = cA == '=' ? 0 : Base64DecodeTable[cA];
		i++;
		unsigned char cB = (unsigned char)sText[i];
		int8_t sextet_b = cB == '=' ? 0 : Base64DecodeTable[cB];
		i++;
		unsigned char cC = (unsigned char)sText[i];
		int8_t sextet_c = cC == '=' ? 0 : Base64DecodeTable[cC];
		i++;
		unsigned char cD = (unsigned char)sText[i];
		int8_t sextet_d = cD == '=' ? 0 : Base64DecodeTable[cD];
		i++;
		// 发现非法字符
		if (sextet_a == -1 || sextet_b == -1 || sextet_c == -1 || sextet_d == -1) {
			xrtSetError(sErrorBase64_char, FALSE);
			xrtFree(sRet);
			return xCore.sNull;
		}
		// 组合 4 个 6 位值为 3 个 8 位字节
		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 2 * 8) & 0xFF; }
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 1 * 8) & 0xFF; }
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 0 * 8) & 0xFF; }
	}
	sRet[iRet] = '\0';
	return sRet;
}
// 通配符匹配（ * 匹配任意字符序列，? 匹配单个UTF-8字符，bCase 为 TRUE 时忽略大小写 ）
// 使用贪婪匹配算法：O(n*m) 最坏时间复杂度，O(1) 空间复杂度
XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase)
{
	// 参数检查
	if ( sPattern == NULL ) { return FALSE; }
	if ( iPatSize == 0 ) { iPatSize = strlen(__xrt_cstr(sPattern)); }
	
	// 空模式只匹配空字符串
	if ( iPatSize == 0 ) {
		if ( sText == NULL ) { return TRUE; }
		if ( iTextSize == 0 ) { iTextSize = strlen(__xrt_cstr(sText)); }
		return iTextSize == 0;
	}
	
	// 处理空文本
	if ( sText == NULL ) {
		// 空文本只能匹配全是 * 的模式
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	if ( iTextSize == 0 ) { iTextSize = strlen(__xrt_cstr(sText)); }
	if ( iTextSize == 0 ) {
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	
	// 贪婪匹配算法
	size_t t = 0;           // 文本位置
	size_t p = 0;           // 模式位置
	size_t starP = (size_t)-1;   // 最近的 * 在模式中的位置
	size_t starT = 0;       // 遇到 * 时文本的位置
	
	while ( t < iTextSize ) {
		if ( p < iPatSize && sPattern[p] == '*' ) {
			// 记录 * 的位置，先假定它匹配 0 个字符
			starP = p;
			starT = t;
			p++;
		} else if ( p < iPatSize && sPattern[p] == '?' ) {
			// ? 匹配一个完整的 UTF-8 字符
			int charLen = xrtCharLenU8((unsigned char)sText[t]);
			// 检查剩余长度是否足够
			if ( t + charLen > iTextSize ) {
				// 字符不完整，尝试回溯
				if ( starP == (size_t)-1 ) { return FALSE; }
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				if ( starT > iTextSize ) { return FALSE; }
			} else {
				t += charLen;
				p++;
			}
		} else {
			// 普通字符匹配（内联字符比较）
			unsigned char c1 = (unsigned char)sText[t];
			unsigned char c2;
			if ( p >= iPatSize ) {
				if ( starP == (size_t)-1 ) { return FALSE; }
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				if ( starT > iTextSize ) { return FALSE; }
				continue;
			}
			c2 = (unsigned char)sPattern[p];
			bool bMatch = (c1 == c2);
			if ( !bMatch && bCase ) {
				// 大小写不敏感：只对 ASCII 字母转换
				if ( c1 >= 'A' && c1 <= 'Z' ) { c1 += 32; }
				if ( c2 >= 'A' && c2 <= 'Z' ) { c2 += 32; }
				bMatch = (c1 == c2);
			}
			if ( p < iPatSize && bMatch ) {
				t++;
				p++;
			} else {
				// 匹配失败，回溯到上一个 *
				if ( starP == (size_t)-1 ) { return FALSE; }
				// 让 * 多匹配一个 UTF-8 字符
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				// 如果 starT 已超出文本范围，则匹配失败
				if ( starT > iTextSize ) { return FALSE; }
			}
		}
	}
	
	// 文本已匹配完，检查模式剩余部分是否全是 *
	while ( p < iPatSize && sPattern[p] == '*' ) {
		p++;
	}
	
	return p == iPatSize;
}
// ============================================================================
// 数值格式化函数
// ============================================================================
// 查表法: 十六进制字符表
static const char xrt_digit_table[] = "0123456789abcdef0123456789ABCDEF";
// 内部函数: 解析格式字符串
typedef struct {
	bool showSign;      // 显示正号
	bool thousands;     // 千分位
	bool percent;       // 百分比
	bool uppercase;     // 大写十六进制
	int base;           // 进制 (10, 16, 8, 2)
	int width;          // 前导零宽度
	int precision;      // 小数位数 (-1 表示未指定)
} XrtNumFmtOpts;
// xrt_parse_format 相关处理
static inline void xrt_parse_format(str format, XrtNumFmtOpts* opts)
{
	opts->showSign = FALSE;
	opts->thousands = FALSE;
	opts->percent = FALSE;
	opts->uppercase = FALSE;
	opts->base = 10;
	opts->width = 0;
	opts->precision = -1;
	
	if ( format == NULL || *format == '\0' ) { return; }
	
	const char* p = __xrt_cstr(format);
	while ( *p ) {
		char c = *p++;
		switch ( c ) {
			case '+': opts->showSign = TRUE; break;
			case ',': opts->thousands = TRUE; break;
			case '%': opts->percent = TRUE; break;
			case 'x': opts->base = 16; opts->uppercase = FALSE; break;
			case 'X': opts->base = 16; opts->uppercase = TRUE; break;
			case 'o': opts->base = 8; break;
			case 'b': case 'B': opts->base = 2; break;
			case '.':
				// 解析小数位数
				opts->precision = 0;
				while ( *p >= '0' && *p <= '9' ) {
					opts->precision = opts->precision * 10 + (*p++ - '0');
				}
				break;
			case '0':
				// 解析前导零宽度
				while ( *p >= '0' && *p <= '9' ) {
					opts->width = opts->width * 10 + (*p++ - '0');
				}
				break;
			default:
				if ( c >= '1' && c <= '9' ) {
					// 数字开头也解析为宽度
					opts->width = c - '0';
					while ( *p >= '0' && *p <= '9' ) {
						opts->width = opts->width * 10 + (*p++ - '0');
					}
				}
				break;
		}
	}
}
// 内部函数: uint64 转非十进制字符串（从 buffer 末尾往前写）
static inline char* xrt_u64_to_base(char* bufEnd, uint64 value, int base, bool upper)
{
	char* p = bufEnd;
	const char* digits = upper ? &xrt_digit_table[16] : xrt_digit_table;
	
	if ( base == 16 ) {
		do {
			*--p = digits[value & 0xF];
			value >>= 4;
		} while ( value );
	} else if ( base == 8 ) {
		do {
			*--p = '0' + (char)(value & 0x7);
			value >>= 3;
		} while ( value );
	} else {
		// 二进制
		do {
			*--p = '0' + (char)(value & 0x1);
			value >>= 1;
		} while ( value );
	}
	
	return p;
}
// 内部函数: 添加千分位分隔符
static inline int xrt_add_thousands(char* dst, const char* src, int srcLen)
{
	int commas = (srcLen - 1) / 3;  // 需要插入的逗号数量
	int totalLen = srcLen + commas;
	int pos = totalLen;
	int cnt = 0;
	
	for ( int i = srcLen - 1; i >= 0; i-- ) {
		dst[--pos] = src[i];
		if ( ++cnt == 3 && i > 0 ) {
			dst[--pos] = ',';
			cnt = 0;
		}
	}
	
	return totalLen;
}
// 整数格式化
XXAPI str xrtIntFormat(int64 value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理符号
	bool negative = (value < 0);
	uint64 absVal = negative ? (uint64)(-(value + 1)) + 1 : (uint64)value;
	
	// 转换为字符串
	char tmpBuf[96];
	char* numStart;
	int numLen;
	
	if ( opts.base == 10 ) {
		// 十进制: 使用 xrtI64ToStr
		numLen = xrtI64ToStr(negative ? value : (int64)absVal, tmpBuf);
		numStart = tmpBuf;
		if ( negative ) {
			numStart++;  // 跳过负号
			numLen--;
		}
	} else {
		// 非十进制
		char* tmpEnd = tmpBuf + sizeof(tmpBuf);
		numStart = xrt_u64_to_base(tmpEnd, absVal, opts.base, opts.uppercase);
		numLen = (int)(tmpEnd - numStart);
		opts.showSign = FALSE;
		opts.thousands = FALSE;
		negative = FALSE;
	}
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int digitLen = numLen;
	if ( opts.thousands && digitLen > 3 ) {
		digitLen += (digitLen - 1) / 3;
	}
	int padLen = (opts.width > digitLen) ? (opts.width - digitLen) : 0;
	int totalLen = signLen + padLen + digitLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = __xrt_str(buffer);
	
	// 写入符号
	if ( signLen ) {
		*out++ = negative ? '-' : '+';
	}
	
	// 写入前导零
	while ( padLen-- > 0 ) { *out++ = '0'; }
	
	// 写入数字
	if ( opts.thousands && numLen > 3 ) {
		xrt_add_thousands(out, numStart, numLen);
		out += digitLen;
	} else {
		memcpy(out, numStart, numLen);
		out += numLen;
	}
	
	*out = '\0';
	return buffer;
}
// 浮点数格式化
XXAPI str xrtNumFormat(double value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理百分比
	if ( opts.percent ) {
		value *= 100.0;
	}
	
	// 处理特殊值
	if ( value != value ) {  // NaN
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "NaN", 4);
		return ret;
	}
	if ( isinf(value) && value > 0.0 ) {
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "Inf", 4);
		return ret;
	}
	if ( isinf(value) && value < 0.0 ) {
		str ret = xrtMalloc(5);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "-Inf", 5);
		return ret;
	}
	
	// 处理符号
	bool negative = (value < 0);
	if ( negative ) { value = -value; }
	
	// 确定小数位数
	int precision = (opts.precision >= 0) ? opts.precision : 6;
	if ( precision > 15 ) { precision = 15; }
	
	// 四舍五入
	static const double roundTable[] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005,
		0.000000005, 0.0000000005, 0.00000000005, 0.000000000005,
		0.0000000000005, 0.00000000000005, 0.000000000000005, 0.0000000000000005
	};
	value += roundTable[precision];
	
	// 分离整数部分和小数部分
	uint64 intPart = (uint64)value;
	double fracPart = value - (double)intPart;
	
	// 转换整数部分: 使用 xrtI64ToStr
	char tmpBuf[32];
	int intLen = xrtI64ToStr((int64)intPart, tmpBuf);
	char* intStart = tmpBuf;
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int intDigitLen = intLen;
	if ( opts.thousands && intDigitLen > 3 ) {
		intDigitLen += (intDigitLen - 1) / 3;
	}
	int fracLen = (precision > 0) ? (1 + precision) : 0;
	int percentLen = opts.percent ? 1 : 0;
	int totalLen = signLen + intDigitLen + fracLen + percentLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = __xrt_str(buffer);
	
	// 写入符号
	if ( negative ) {
		*out++ = '-';
	} else if ( opts.showSign ) {
		*out++ = '+';
	}
	
	// 写入整数部分
	if ( opts.thousands && intLen > 3 ) {
		xrt_add_thousands(out, intStart, intLen);
		out += intDigitLen;
	} else {
		memcpy(out, intStart, intLen);
		out += intLen;
	}
	
	// 写入小数部分
	if ( precision > 0 ) {
		*out++ = '.';
		for ( int i = 0; i < precision; i++ ) {
			fracPart *= 10.0;
			int digit = (int)fracPart;
			*out++ = '0' + digit;
			fracPart -= digit;
		}
	}
	
	// 写入百分号
	if ( opts.percent ) {
		*out++ = '%';
	}
	
	*out = '\0';
	return buffer;
}
// 字符串相似度（基于 Levenshtein 编辑距离，返回 0.0-1.0）
// 高性能优化：一维数组 O(min(m,n)) 空间，内联 min 计算
XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2)
{
	// 空指针检查
	if ( s1 == NULL ) { s1 = (str)""; len1 = 0; }
	if ( s2 == NULL ) { s2 = (str)""; len2 = 0; }
	
	// 自动计算长度
	if ( len1 == 0 ) { len1 = strlen(__xrt_cstr(s1)); }
	if ( len2 == 0 ) { len2 = strlen(__xrt_cstr(s2)); }
	
	// 快速路径：空字符串
	if ( len1 == 0 && len2 == 0 ) { return 1.0; }
	if ( len1 == 0 ) { return 0.0; }
	if ( len2 == 0 ) { return 0.0; }
	
	// 快速路径：完全相同
	if ( len1 == len2 && memcmp(s1, s2, len1) == 0 ) { return 1.0; }
	
	// 确保 s1 是较长的字符串（优化内存访问）
	if ( len1 < len2 ) {
		str ts = s1; s1 = s2; s2 = ts;
		size_t tl = len1; len1 = len2; len2 = tl;
	}
	
	// 分配一维 DP 数组（只需要较短字符串的长度+1）
	size_t dpSize = len2 + 1;
	int* dp = (int*)xrtMalloc(dpSize * sizeof(int));
	if ( dp == NULL ) { return 0.0; }
	
	// 初始化第一行：dp[j] = j
	for ( size_t j = 0; j <= len2; j++ ) {
		dp[j] = (int)j;
	}
	
	// DP 计算（行优先遍历，对缓存友好）
	for ( size_t i = 1; i <= len1; i++ ) {
		int prev = dp[0];  // 保存 dp[i-1][j-1]
		dp[0] = (int)i;    // dp[i][0] = i
		
		unsigned char c1 = (unsigned char)s1[i - 1];
		
		for ( size_t j = 1; j <= len2; j++ ) {
			int temp = dp[j];  // 保存当前值，作为下一次迭代的 prev
			
			if ( c1 == (unsigned char)s2[j - 1] ) {
				// 字符相同，无需操作
				dp[j] = prev;
			} else {
				// 字符不同，取 min(删除, 插入, 替换) + 1
				int del = dp[j];      // dp[i-1][j] + 1
				int ins = dp[j - 1];  // dp[i][j-1] + 1
				int rep = prev;       // dp[i-1][j-1] + 1
				
				// 内联 min3 计算
				int minVal = del;
				if ( ins < minVal ) { minVal = ins; }
				if ( rep < minVal ) { minVal = rep; }
				
				dp[j] = minVal + 1;
			}
			
			prev = temp;
		}
	}
	
	// 获取编辑距离
	int distance = dp[len2];
	xrtFree(dp);
	
	// 计算相似度：1 - distance / maxLen
	size_t maxLen = len1;  // len1 >= len2
	return 1.0 - (double)distance / (double)maxLen;
}
// 字符串约等于（使用 xCore 配置）
// iApproxStrMode=0: 通配符模式（使用 xrtStrLike，s2 为模式串）
// iApproxStrMode=1: 相似度模式（使用 xrtStrSim 和 fApproxStrTol 阈值）
XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2)
{
	if ( xCore.iApproxStrMode == XRT_STR_APPROX_LIKE ) {
		// 通配符模式
		return xrtStrLike(s1, len1, s2, len2, xCore.bApproxStrCase);
	} else {
		// 相似度模式
		double threshold = xCore.fApproxStrTol;
		if ( threshold <= 0.0 || threshold > 1.0 ) {
			threshold = 0.95;  // 无效阈值使用默认值
		}
		double sim = xrtStrSim(s1, len1, s2, len2);
		return sim >= threshold;
	}
}

// ========================================
// File: D:/git/xrt/lib/os.h
// ========================================


// 运行程序
XXAPI ptr xrtRun(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return NULL;
		}
		BOOL bOK = CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		if ( bOK == FALSE ) {
			return NULL;
		}
		if ( pi.hThread ) {
			CloseHandle(pi.hThread);
		}
		return (ptr)pi.hProcess;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}
// 打开文件（ Windows 系统使用 ShellExecute，Linux 系统使用 xdg-open ）
XXAPI ptr xrtStart(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return NULL;
		}
		ptr hRet = (ptr)ShellExecuteW(0, NULL, sPathW, NULL, NULL, SW_SHOW);
		xrtFree(sPathW);
		if ( (intptr_t)hRet <= 32 ) {
			xrtSetError("start failed.", FALSE);
			return NULL;
		}
		return hRet;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execlp("xdg-open", "xdg-open", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}
// 运行程序并等待程序运行结束
XXAPI int xrtChain(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		if ( sPathW == NULL ) {
			return -1;
		}
		BOOL bOK = CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		if ( bOK == FALSE ) {
			return -1;
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
		if ( pi.hProcess ) {
			GetExitCodeProcess(pi.hProcess, &iRet);
			CloseHandle(pi.hProcess);
		}
		if ( pi.hThread ) {
			CloseHandle(pi.hThread);
		}
		return (int)iRet;
	#else
		(void)iSize;
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			_exit(127);
		} else if ( pid > 0 ) {
			int status;
			waitpid(pid, &status, 0);
			if ( WIFEXITED(status) ) {
				return WEXITSTATUS(status);
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	#endif
}

// ========================================
// File: D:/git/xrt/lib/hash.h
// ========================================


/*
	Hash32 - nmhash32x [Ver2.0, Update : 2024/10/18 from https://github.com/rurban/smhasher]
		修改：
			删除函数：NMHASH32_0to8（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32_9to255（仅NMHASH32依赖，只保留NMHASH32X）
			删除定义：NMHASH32_9to32（仅NMHASH32依赖，只保留NMHASH32X）
			删除定义：NMHASH32_33to255（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32_avalanche32（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32（只保留NMHASH32X）
			添加部分条件编译分支：判断 __TINYC__ 以区分是否使用 TCC 编译
			删除头文件引入：<stdint.h> 和 <string.h>（最上面已经引入）
		使用协议注意事项：
			BSD 2-Clause 协议
			允许个人使用、商业使用
			复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/
/*
	BSD 2-Clause License
	Copyright (c) 2021, James Z.M. Gao 
	All rights reserved.
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice, this
	   list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define NMH_VERSION 2
#ifdef _MSC_VER
#  pragma warning(push, 3)
#endif
#if defined(__cplusplus) && __cplusplus < 201103L
#  define __STDC_CONSTANT_MACROS 1
#endif
#if defined(__GNUC__)
#  if defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  endif
#elif defined(_MSC_VER)
#  include <intrin.h>
#endif
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#if (defined(__GNUC__) && (__GNUC__ >= 3))  \
  || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) \
  || defined(__clang__)
#    define NMH_likely(x) __builtin_expect(x, 1)
#else
#    define NMH_likely(x) (x)
#endif
#if defined(__has_builtin)
#  if __has_builtin(__builtin_rotateleft32)
#    define NMH_rotl32 __builtin_rotateleft32 /* clang */
#  endif
#endif
#if !defined(NMH_rotl32)
#  if defined(_MSC_VER)
	 /* Note: although _rotl exists for minGW (GCC under windows), performance seems poor */
#    define NMH_rotl32(x,r) _rotl(x,r)
#  else
#    define NMH_rotl32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))
#  endif
#endif
#ifdef __TINYC__
#  define NMH_RESTRICT   restrict
#  define NMH_VECTOR NMH_SCALAR
#elif ((defined(sun) || defined(__sun)) && __cplusplus) /* Solaris includes __STDC_VERSION__ with C++. Tested with GCC 5.5 */
#  define NMH_RESTRICT   /* disable */
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* >= C99 */
#  define NMH_RESTRICT   restrict
#elif defined(__cplusplus) && (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER))
#  define NMH_RESTRICT __restrict__
#elif defined(__cplusplus) && defined(_MSC_VER)
#  define NMH_RESTRICT __restrict
#else
#  define NMH_RESTRICT   /* disable */
#endif
/* endian macros */
#ifndef NMHASH_LITTLE_ENDIAN
#  if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || defined(__x86_64__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || defined(__SDCC)
#    define NMHASH_LITTLE_ENDIAN 1
#  elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define NMHASH_LITTLE_ENDIAN 0
#  else
#    warning could not determine endianness! Falling back to little endian.
#    define NMHASH_LITTLE_ENDIAN 1
#  endif
#endif
/* vector macros */
#define NMH_SCALAR 0
#define NMH_SSE2   1
#define NMH_AVX2   2
#define NMH_AVX512 3
#ifndef NMH_VECTOR    /* can be defined on command line */
#  if defined(__AVX512BW__)
#    define NMH_VECTOR NMH_AVX512 /* _mm512_mullo_epi16 requires AVX512BW */
#  elif defined(__AVX2__)
#    define NMH_VECTOR NMH_AVX2  /* add '-mno-avx256-split-unaligned-load' and '-mn-oavx256-split-unaligned-store' for gcc */
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
#    define NMH_VECTOR NMH_SSE2
#  else
#    define NMH_VECTOR NMH_SCALAR
#  endif
#endif
/* align macros */
#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)   /* C11+ */
#  include <stdalign.h>
#  define NMH_ALIGN(n)      alignas(n)
#elif defined(__GNUC__)
#  define NMH_ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  define NMH_ALIGN(n)      __declspec(align(n))
#else
#  define NMH_ALIGN(n)   /* disabled */
#endif
#if NMH_VECTOR > 0
#  define NMH_ACC_ALIGN 64
#elif defined(__BIGGEST_ALIGNMENT__)
#  define NMH_ACC_ALIGN __BIGGEST_ALIGNMENT__
#elif defined(__SDCC)
#  define NMH_ACC_ALIGN 1
#else
#  define NMH_ACC_ALIGN 16
#endif
/* constants */
/* primes from xxh */
#define NMH_PRIME32_1  UINT32_C(0x9E3779B1)
#define NMH_PRIME32_2  UINT32_C(0x85EBCA77)
#define NMH_PRIME32_3  UINT32_C(0xC2B2AE3D)
#define NMH_PRIME32_4  UINT32_C(0x27D4EB2F)
/*! Pseudorandom secret taken directly from FARSH. */
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t NMH_ACC_INIT[32] = {
	UINT32_C(0xB8FE6C39), UINT32_C(0x23A44BBE), UINT32_C(0x7C01812C), UINT32_C(0xF721AD1C),
	UINT32_C(0xDED46DE9), UINT32_C(0x839097DB), UINT32_C(0x7240A4A4), UINT32_C(0xB7B3671F),
	UINT32_C(0xCB79E64E), UINT32_C(0xCCC0E578), UINT32_C(0x825AD07D), UINT32_C(0xCCFF7221),
	UINT32_C(0xB8084674), UINT32_C(0xF743248E), UINT32_C(0xE03590E6), UINT32_C(0x813A264C),
	UINT32_C(0x3C2852BB), UINT32_C(0x91C300CB), UINT32_C(0x88D0658B), UINT32_C(0x1B532EA3),
	UINT32_C(0x71644897), UINT32_C(0xA20DF94E), UINT32_C(0x3819EF46), UINT32_C(0xA9DEACD8),
	UINT32_C(0xA8FA763F), UINT32_C(0xE39C343F), UINT32_C(0xF9DCBBC7), UINT32_C(0xC70B4F1D),
	UINT32_C(0x8A51E04B), UINT32_C(0xCDB45931), UINT32_C(0xC89F7EC9), UINT32_C(0xD9787364),
};
#if defined(_MSC_VER) && _MSC_VER >= 1914
#  pragma warning(push)
#  pragma warning(disable: 5045)
#endif
#ifdef __SDCC
#  define const
#  pragma save
#  pragma disable_warning 110
#  pragma disable_warning 126
#endif
/* read functions */
static inline
uint32_t
NMH_readLE32(const void *const p)
{
	uint32_t v;
	memcpy(&v, p, 4);
#	if (NMHASH_LITTLE_ENDIAN)
	return v;
#	elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
	return __builtin_bswap32(v);
#	elif defined(_MSC_VER)
	return _byteswap_ulong(v);
#	else
	return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
#	endif
}
static inline
uint16_t
NMH_readLE16(const void *const p)
{
	uint16_t v;
	memcpy(&v, p, 2);
#	if (NMHASH_LITTLE_ENDIAN)
	return v;
#	else
	return (uint16_t)((v << 8) | (v >> 8));
#	endif
}
#define __NMH_M1 UINT32_C(0xF0D9649B)
#define __NMH_M2 UINT32_C(0x29A7935D)
#define __NMH_M3 UINT32_C(0x55D35831)
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M1_V[32] = {
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
};
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M2_V[32] = {
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
};
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M3_V[32] = {
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
};
#undef __NMH_M1
#undef __NMH_M2
#undef __NMH_M3
#if NMH_VECTOR == NMH_SCALAR
#define NMHASH32_long_round NMHASH32_long_round_scalar
static inline
void
NMHASH32_long_round_scalar(uint32_t *const NMH_RESTRICT accX, uint32_t *const NMH_RESTRICT accY, const uint8_t* const NMH_RESTRICT p)
{
	/* breadth first calculation will hint some compiler to auto vectorize the code
	 * on gcc, the performance becomes 10x than the depth first, and about 80% of the manually vectorized code
	 */
	const size_t nbGroups = sizeof(NMH_ACC_INIT) / sizeof(*NMH_ACC_INIT);
	size_t i;
	
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= NMH_readLE32(p + i * 4);
	}
	for (i = 0; i < nbGroups; ++i) {
		accY[i] ^= NMH_readLE32(p + i * 4 + sizeof(NMH_ACC_INIT));
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] += accY[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accY[i] ^= accX[i] >> 1;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M1_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] << 5 ^ accX[i] >> 13;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M2_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accY[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] << 11 ^ accX[i] >> 9;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M3_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] >> 10 ^ accX[i] >> 20;
	}
}
#endif
#if NMH_VECTOR == NMH_SSE2
#  define _NMH_MM_(F) _mm_ ## F
#  define _NMH_MMW_(F) _mm_ ## F ## 128
#  define _NMH_MM_T __m128i
#elif NMH_VECTOR == NMH_AVX2
#  define _NMH_MM_(F) _mm256_ ## F
#  define _NMH_MMW_(F) _mm256_ ## F ## 256
#  define _NMH_MM_T __m256i
#elif NMH_VECTOR == NMH_AVX512
#  define _NMH_MM_(F) _mm512_ ## F
#  define _NMH_MMW_(F) _mm512_ ## F ## 512
#  define _NMH_MM_T __m512i
#endif
#if NMH_VECTOR == NMH_SSE2 || NMH_VECTOR == NMH_AVX2 || NMH_VECTOR == NMH_AVX512
#  define NMHASH32_long_round NMHASH32_long_round_sse
#  define NMH_VECTOR_NB_GROUP (sizeof(NMH_ACC_INIT) / sizeof(*NMH_ACC_INIT) / (sizeof(_NMH_MM_T) / sizeof(*NMH_ACC_INIT)))
static inline
void
NMHASH32_long_round_sse(uint32_t* const NMH_RESTRICT accX, uint32_t* const NMH_RESTRICT accY, const uint8_t* const NMH_RESTRICT p)
{
	const _NMH_MM_T *const NMH_RESTRICT m1    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M1_V;
	const _NMH_MM_T *const NMH_RESTRICT m2    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M2_V;
	const _NMH_MM_T *const NMH_RESTRICT m3    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M3_V;
		  _NMH_MM_T *const              xaccX = (      _NMH_MM_T *             )accX;
		  _NMH_MM_T *const              xaccY = (      _NMH_MM_T *             )accY;
		  _NMH_MM_T *const              xp    = (      _NMH_MM_T *             )p;
	size_t i;
	
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(xaccX[i], _NMH_MMW_(loadu_si)(xp + i));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccY[i] = _NMH_MMW_(xor_si)(xaccY[i], _NMH_MMW_(loadu_si)(xp + i + NMH_VECTOR_NB_GROUP));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(add_epi32)(xaccX[i], xaccY[i]);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccY[i] = _NMH_MMW_(xor_si)(xaccY[i], _NMH_MM_(srli_epi32)(xaccX[i], 1));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m1);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(slli_epi32)(xaccX[i], 5)), _NMH_MM_(srli_epi32)(xaccX[i], 13));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m2);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(xaccX[i], xaccY[i]);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(slli_epi32)(xaccX[i], 11)), _NMH_MM_(srli_epi32)(xaccX[i], 9));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m3);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(srli_epi32)(xaccX[i], 10)), _NMH_MM_(srli_epi32)(xaccX[i], 20));
	}
}
#  undef _NMH_MM_
#  undef _NMH_MMW_
#  undef _NMH_MM_T
#  undef NMH_VECTOR_NB_GROUP
#endif
static
uint32_t
NMHASH32_long(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	NMH_ALIGN(NMH_ACC_ALIGN) uint32_t accX[sizeof(NMH_ACC_INIT)/sizeof(*NMH_ACC_INIT)];
	NMH_ALIGN(NMH_ACC_ALIGN) uint32_t accY[sizeof(accX)/sizeof(*accX)];
	size_t const nbRounds = (len - 1) / (sizeof(accX) + sizeof(accY));
	size_t i;
	uint32_t sum = 0;
	
	/* init */
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) accX[i] = NMH_ACC_INIT[i];
	for (i = 0; i < sizeof(accY)/sizeof(*accY); ++i) accY[i] = seed;
	
	for (i = 0; i < nbRounds; ++i) {
		NMHASH32_long_round(accX, accY, p + i * (sizeof(accX) + sizeof(accY)));
	}
	NMHASH32_long_round(accX, accY, p + len - (sizeof(accX) + sizeof(accY)));
	
	/* merge acc */
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) accX[i] ^= NMH_ACC_INIT[i];
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) sum += accX[i];
	
#	if SIZE_MAX > UINT32_C(-1)
	sum += (uint32_t)(len >> 32);
#	endif
	return sum ^ (uint32_t)len;
}
static inline
uint32_t
NMHASH32X_0to4(uint32_t x, uint32_t const seed)
{
	/* [bdab1ea9 18 a7896a1b 12 83796a2d 16] = 0.092922873297662509 */
	x ^= seed;
	x *= UINT32_C(0xBDAB1EA9);
	x += NMH_rotl32(seed, 31);
	x ^= x >> 18;
	x *= UINT32_C(0xA7896A1B);
	x ^= x >> 12;
	x *= UINT32_C(0x83796A2D);
	x ^= x >> 16;
	return x;
}
static inline
uint32_t
NMHASH32X_5to8(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	/* - 5 to 9 bytes
	 * - mixer: [11049a7d 23 bcccdc7b 12 065e9dad 12] = 0.16577596555667246 */
	
	uint32_t       x = NMH_readLE32(p) ^ NMH_PRIME32_3;
	uint32_t const y = NMH_readLE32(p + len - 4) ^ seed;
	x += y;
	x ^= x >> len;
	x *= UINT32_C(0x11049A7D);
	x ^= x >> 23;
	x *= UINT32_C(0xBCCCDC7B);
	x ^= NMH_rotl32(y, 3);
	x ^= x >> 12;
	x *= UINT32_C(0x065E9DAD);
	x ^= x >> 12;
	return x;
}
static inline
uint32_t
NMHASH32X_9to255(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	/* - at least 9 bytes
	 * - base mixer: [11049a7d 23 bcccdc7b 12 065e9dad 12] = 0.16577596555667246
	 * - tail mixer: [16 a52fb2cd 15 551e4d49 16] = 0.17162579707098322
	 */
	
	uint32_t x = NMH_PRIME32_3;
	uint32_t y = seed;
	uint32_t a = NMH_PRIME32_4;
	uint32_t b = seed;
	size_t i, r = (len - 1) / 16;
	
	for (i = 0; i < r; ++i) {
		x ^= NMH_readLE32(p + i * 16 + 0);
		y ^= NMH_readLE32(p + i * 16 + 4);
		x ^= y;
		x *= UINT32_C(0x11049A7D);
		x ^= x >> 23;
		x *= UINT32_C(0xBCCCDC7B);
		y  = NMH_rotl32(y, 4);
		x ^= y;
		x ^= x >> 12;
		x *= UINT32_C(0x065E9DAD);
		x ^= x >> 12;
		a ^= NMH_readLE32(p + i * 16 + 8);
		b ^= NMH_readLE32(p + i * 16 + 12);
		a ^= b;
		a *= UINT32_C(0x11049A7D);
		a ^= a >> 23;
		a *= UINT32_C(0xBCCCDC7B);
		b  = NMH_rotl32(b, 3);
		a ^= b;
		a ^= a >> 12;
		a *= UINT32_C(0x065E9DAD);
		a ^= a >> 12;
	}
	
	if (NMH_likely(((uint8_t)len-1) & 8)) {
		if (NMH_likely(((uint8_t)len-1) & 4)) {
			a ^= NMH_readLE32(p + r * 16 + 0);
			b ^= NMH_readLE32(p + r * 16 + 4);
			a ^= b;
			a *= UINT32_C(0x11049A7D);
			a ^= a >> 23;
			a *= UINT32_C(0xBCCCDC7B);
			a ^= NMH_rotl32(b, 4);
			a ^= a >> 12;
			a *= UINT32_C(0x065E9DAD);
		} else {
			a ^= NMH_readLE32(p + r * 16) + b;
			a ^= a >> 16;
			a *= UINT32_C(0xA52FB2CD);
			a ^= a >> 15;
			a *= UINT32_C(0x551E4D49);
		}
		
		x ^= NMH_readLE32(p + len - 8);
		y ^= NMH_readLE32(p + len - 4);
		x ^= y;
		x *= UINT32_C(0x11049A7D);
		x ^= x >> 23;
		x *= UINT32_C(0xBCCCDC7B);
		x ^= NMH_rotl32(y, 3);
		x ^= x >> 12;
		x *= UINT32_C(0x065E9DAD);
	} else {
		if (NMH_likely(((uint8_t)len-1) & 4)) {
			a ^= NMH_readLE32(p + r * 16) + b;
			a ^= a >> 16;
			a *= UINT32_C(0xA52FB2CD);
			a ^= a >> 15;
			a *= UINT32_C(0x551E4D49);
		}
		x ^= NMH_readLE32(p + len - 4) + y;
		x ^= x >> 16;
		x *= UINT32_C(0xA52FB2CD);
		x ^= x >> 15;
		x *= UINT32_C(0x551E4D49);
	}
	
	x ^= (uint32_t)len;
	x ^= NMH_rotl32(a, 27); /* rotate one lane to pass Diff test */
	x ^= x >> 14;
	x *= UINT32_C(0x141CC535);
	
	return x;
}
static inline
uint32_t
NMHASH32X_avalanche32(uint32_t x)
{
	/* mixer with 2 mul from skeeto/hash-prospector:
	 * [15 d168aaad 15 af723597 15] = 0.15983776156606694
	 */
	x ^= x >> 15;
	x *= UINT32_C(0xD168AAAD);
	x ^= x >> 15;
	x *= UINT32_C(0xAF723597);
	x ^= x >> 15;
	return x;
}
/* use 32*32->32 multiplication for short hash */
static inline
uint32_t
NMHASH32X(const void* const NMH_RESTRICT input, size_t const len, uint32_t seed)
{
	const uint8_t *const p = (const uint8_t *)input;
	if (NMH_likely(len <= 8)) {
		if (NMH_likely(len > 4)) {
			return NMHASH32X_5to8(p, len, seed);
		} else {
			/* 0-4 bytes */
			union { uint32_t u32; uint16_t u16[2]; uint8_t u8[4]; } data;
			switch (len) {
				case 0: seed += NMH_PRIME32_2;
					data.u32 = 0;
					break;
				case 1: seed += NMH_PRIME32_2 + (UINT32_C(1) << 24) + (1 << 1);
					data.u32 = p[0];
					break;
				case 2: seed += NMH_PRIME32_2 + (UINT32_C(2) << 24) + (2 << 1);
					data.u32 = NMH_readLE16(p);
					break;
				case 3: seed += NMH_PRIME32_2 + (UINT32_C(3) << 24) + (3 << 1);
					data.u16[1] = p[2];
					data.u16[0] = NMH_readLE16(p);
					break;
				case 4: seed += NMH_PRIME32_1;
					data.u32 = NMH_readLE32(p);
					break;
				default: return 0;
			}
			return NMHASH32X_0to4(data.u32, seed);
		}
	}
	if (NMH_likely(len < 256)) {
		return NMHASH32X_9to255(p, len, seed);
	}
	return NMHASH32X_avalanche32(NMHASH32_long(p, len, seed));
}
#if defined(_MSC_VER) && _MSC_VER >= 1914
#  pragma warning(pop)
#endif
#ifdef __SDCC
#  pragma restore
#  undef const
#endif
// 计算 32 位哈希值
XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed)
{
	return NMHASH32X(key, len, seed);
}
// xrtHash32 相关处理
XXAPI uint32 xrtHash32(ptr key, size_t len)
{
	return xrtHash32_WithSeed(key, len, HASH32_SEED);
}
/*
Hash64 - rapidhash [Ver3.0, Update : 2025/08/14 from https://github.com/Nicoshev/rapidhash]
	修改：
		删除头文件引入：<stdint.h> 和 <string.h>（最上面已经引入）
	使用协议注意事项：
		BSD 2-Clause 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/
/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */
/*
 *  Includes.
 */
 #if defined(_MSC_VER)
 # include <intrin.h>
 # if defined(_M_X64) && !defined(_M_ARM64EC)
 #   pragma intrinsic(_umul128)
 # endif
 #endif
 
 /*
  *  C/C++ macros.
  */
 
 #ifdef _MSC_VER
 # define RAPIDHASH_ALWAYS_INLINE __forceinline
 #elif defined(__GNUC__)
 # define RAPIDHASH_ALWAYS_INLINE inline __attribute__((__always_inline__))
 #else
 # define RAPIDHASH_ALWAYS_INLINE inline
 #endif
 
 #ifdef __cplusplus
 # define RAPIDHASH_NOEXCEPT noexcept
 # define RAPIDHASH_CONSTEXPR constexpr
 # ifndef RAPIDHASH_INLINE
 #   define RAPIDHASH_INLINE RAPIDHASH_ALWAYS_INLINE
 # endif
 # if __cplusplus >= 201402L && !defined(_MSC_VER)
 #   define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_ALWAYS_INLINE constexpr
 # else
 #   define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_ALWAYS_INLINE
 # endif
 #else
 # define RAPIDHASH_NOEXCEPT
 # define RAPIDHASH_CONSTEXPR static const
 # ifndef RAPIDHASH_INLINE
 #   define RAPIDHASH_INLINE static RAPIDHASH_ALWAYS_INLINE
 # endif
 # define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_INLINE
 #endif
 /*
  *  Unrolled macro.
  *  Improves large input speed, but increases code size and worsens small input speed.
  *
  *  RAPIDHASH_COMPACT: Normal behavior.
  *  RAPIDHASH_UNROLLED: 
  *
  */
  #ifndef RAPIDHASH_UNROLLED
  # define RAPIDHASH_COMPACT
  #elif defined(RAPIDHASH_COMPACT)
  # error "cannot define RAPIDHASH_COMPACT and RAPIDHASH_UNROLLED simultaneously."
  #endif
 
 /*
  *  Protection macro, alters behaviour of rapid_mum multiplication function.
  *
  *  RAPIDHASH_FAST: Normal behavior, max speed.
  *  RAPIDHASH_PROTECTED: Extra protection against entropy loss.
  */
 #ifndef RAPIDHASH_PROTECTED
 # define RAPIDHASH_FAST
 #elif defined(RAPIDHASH_FAST)
 # error "cannot define RAPIDHASH_PROTECTED and RAPIDHASH_FAST simultaneously."
 #endif
 
 /*
  *  Likely and unlikely macros.
  */
 #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
 # define _likely_(x)  __builtin_expect(x,1)
 # define _unlikely_(x)  __builtin_expect(x,0)
 #else
 # define _likely_(x) (x)
 # define _unlikely_(x) (x)
 #endif
 
 /*
  *  Endianness macros.
  */
 #ifndef RAPIDHASH_LITTLE_ENDIAN
 # if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
 #   define RAPIDHASH_LITTLE_ENDIAN
 # elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
 #   define RAPIDHASH_BIG_ENDIAN
 # else
 #   warning "could not determine endianness! Falling back to little endian."
 #   define RAPIDHASH_LITTLE_ENDIAN
 # endif
 #endif
 
 /*
  *  Default secret parameters.
  */
   RAPIDHASH_CONSTEXPR uint64_t rapid_secret[8] = {
     0x2d358dccaa6c78a5ull,
     0x8bb84b93962eacc9ull,
     0x4b33a62ed433d4a3ull,
     0x4d5a2da51de1aa47ull,
     0xa0761d6478bd642full,
     0xe7037ed1a0b428dbull,
     0x90ed1765281c388cull,
     0xaaaaaaaaaaaaaaaaull};
 
 /*
  *  64*64 -> 128bit multiply function.
  *
  *  @param A  Address of 64-bit number.
  *  @param B  Address of 64-bit number.
  *
  *  Calculates 128-bit C = *A * *B.
  *
  *  When RAPIDHASH_FAST is defined:
  *  Overwrites A contents with C's low 64 bits.
  *  Overwrites B contents with C's high 64 bits.
  *
  *  When RAPIDHASH_PROTECTED is defined:
  *  Xors and overwrites A contents with C's low 64 bits.
  *  Xors and overwrites B contents with C's high 64 bits.
  */
 RAPIDHASH_INLINE_CONSTEXPR void rapid_mum(uint64_t *A, uint64_t *B) RAPIDHASH_NOEXCEPT {
 #if defined(__SIZEOF_INT128__)
   __uint128_t r=*A; r*=*B;
   #ifdef RAPIDHASH_PROTECTED
   *A^=(uint64_t)r; *B^=(uint64_t)(r>>64);
   #else
   *A=(uint64_t)r; *B=(uint64_t)(r>>64);
   #endif
 #elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
   #if defined(_M_X64)
     #ifdef RAPIDHASH_PROTECTED
     uint64_t a, b;
     a=_umul128(*A,*B,&b);
     *A^=a;  *B^=b;
     #else
     *A=_umul128(*A,*B,B);
     #endif
   #else
     #ifdef RAPIDHASH_PROTECTED
     uint64_t a, b;
     b = __umulh(*A, *B);
     a = *A * *B;
     *A^=a;  *B^=b;
     #else
     uint64_t c = __umulh(*A, *B);
     *A = *A * *B;
     *B = c;
     #endif
   #endif
 #else
   uint64_t ha=*A>>32, hb=*B>>32, la=(uint32_t)*A, lb=(uint32_t)*B;
   uint64_t rh=ha*hb, rm0=ha*lb, rm1=hb*la, rl=la*lb, t=rl+(rm0<<32), c=t<rl;
   uint64_t lo=t+(rm1<<32); 
   c+=lo<t; 
   uint64_t hi=rh+(rm0>>32)+(rm1>>32)+c;
   #ifdef RAPIDHASH_PROTECTED
   *A^=lo;  *B^=hi;
   #else
   *A=lo;  *B=hi;
   #endif
 #endif
 }
 
 /*
  *  Multiply and xor mix function.
  *
  *  @param A  64-bit number.
  *  @param B  64-bit number.
  *
  *  Calculates 128-bit C = A * B.
  *  Returns 64-bit xor between high and low 64 bits of C.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapid_mix(uint64_t A, uint64_t B) RAPIDHASH_NOEXCEPT { rapid_mum(&A,&B); return A^B; }
 
 /*
  *  Read functions.
  */
 #ifdef RAPIDHASH_LITTLE_ENDIAN
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return v;}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return v;}
 #elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return __builtin_bswap64(v);}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return __builtin_bswap32(v);}
 #elif defined(_MSC_VER)
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return _byteswap_uint64(v);}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return _byteswap_ulong(v);}
 #else
 // rapid_read64 相关处理
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT {
   uint64_t v; memcpy(&v, p, 8);
   return (((v >> 56) & 0xff)| ((v >> 40) & 0xff00)| ((v >> 24) & 0xff0000)| ((v >>  8) & 0xff000000)| ((v <<  8) & 0xff00000000)| ((v << 24) & 0xff0000000000)| ((v << 40) & 0xff000000000000)| ((v << 56) & 0xff00000000000000));
 }
 // rapid_read32 相关处理
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT {
   uint32_t v; memcpy(&v, p, 4);
   return (((v >> 24) & 0xff)| ((v >>  8) & 0xff00)| ((v <<  8) & 0xff0000)| ((v << 24) & 0xff000000));
 }
 #endif
 
 /*
  *  rapidhash main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
  const uint8_t *p=(const uint8_t *)key;
  seed ^= rapid_mix(seed ^ secret[2], secret[1]);
  uint64_t a=0, b=0;
  size_t i = len;
  if (_likely_(len <= 16)) {
    if (len >= 4) {
      seed ^= len;
      if (len >= 8) {
        const uint8_t* plast = p + len - 8;
        a = rapid_read64(p);
        b = rapid_read64(plast);
      } else {
        const uint8_t* plast = p + len - 4;
        a = rapid_read32(p);
        b = rapid_read32(plast);
      }
    } else if (len > 0) {
      a = (((uint64_t)p[0])<<45)|p[len-1];
      b = p[len>>1];
    } else
      a = b = 0;
  } else {
    uint64_t see1 = seed, see2 = seed;
    uint64_t see3 = seed, see4 = seed;
    uint64_t see5 = seed, see6 = seed;
#ifdef RAPIDHASH_COMPACT
    if (i > 112) {
      do {
        seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
        see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
        see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
        see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
        see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
        see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
        see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
        p += 112;
        i -= 112;
      } while(i > 112);
      seed ^= see1;
      see2 ^= see3;
      see4 ^= see5;
      seed ^= see6;
      see2 ^= see4;
      seed ^= see2;
    }
#else 
    if (i > 224) {
      do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
          see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
          see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
          seed = rapid_mix(rapid_read64(p + 112) ^ secret[0], rapid_read64(p + 120) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 128) ^ secret[1], rapid_read64(p + 136) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 144) ^ secret[2], rapid_read64(p + 152) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 160) ^ secret[3], rapid_read64(p + 168) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 176) ^ secret[4], rapid_read64(p + 184) ^ see4);
          see5 = rapid_mix(rapid_read64(p + 192) ^ secret[5], rapid_read64(p + 200) ^ see5);
          see6 = rapid_mix(rapid_read64(p + 208) ^ secret[6], rapid_read64(p + 216) ^ see6);
          p += 224;
          i -= 224;
      } while (i > 224);
    }
    if (i > 112) {
      seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
      see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
      see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
      see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
      see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
      see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
      see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
      p += 112;
      i -= 112;
    }
    seed ^= see1;
    see2 ^= see3;
    see4 ^= see5;
    seed ^= see6;
    see2 ^= see4;
    seed ^= see2;
#endif
    if (i > 16) {
      seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
      if (i > 32) {
          seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
          if (i > 48) {
              seed = rapid_mix(rapid_read64(p + 32) ^ secret[1], rapid_read64(p + 40) ^ seed);
              if (i > 64) {
                  seed = rapid_mix(rapid_read64(p + 48) ^ secret[1], rapid_read64(p + 56) ^ seed);
                  if (i > 80) {
                      seed = rapid_mix(rapid_read64(p + 64) ^ secret[2], rapid_read64(p + 72) ^ seed);
                      if (i > 96) {
                          seed = rapid_mix(rapid_read64(p + 80) ^ secret[1], rapid_read64(p + 88) ^ seed);
                      }
                  }
              }
          }
      }
    }
    a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
  }
  a ^= secret[1];
  b ^= seed;
  rapid_mum(&a, &b);
  return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
}
 /*
  *  rapidhashMicro main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
    const uint8_t *p=(const uint8_t *)key;
    seed ^= rapid_mix(seed ^ secret[2], secret[1]);
    uint64_t a=0, b=0;
    size_t i = len;
    if (_likely_(len <= 16)) {
      if (len >= 4) {
        seed ^= len;
        if (len >= 8) {
          const uint8_t* plast = p + len - 8;
          a = rapid_read64(p);
          b = rapid_read64(plast);
        } else {
          const uint8_t* plast = p + len - 4;
          a = rapid_read32(p);
          b = rapid_read32(plast);
        }
      } else if (len > 0) {
        a = (((uint64_t)p[0])<<45)|p[len-1];
        b = p[len>>1];
      } else
        a = b = 0;
    } else {
      if (i > 80) {
        uint64_t see1 = seed, see2 = seed;
        uint64_t see3 = seed, see4 = seed;
        do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
          p += 80;
          i -= 80;
        } while(i > 80);
        seed ^= see1;
        see2 ^= see3;
        seed ^= see4;
        seed ^= see2;
      }
      if (i > 16) {
        seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
        if (i > 32) {
            seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
            if (i > 48) {
                seed = rapid_mix(rapid_read64(p + 32) ^ secret[1], rapid_read64(p + 40) ^ seed);
                if (i > 64) {
                    seed = rapid_mix(rapid_read64(p + 48) ^ secret[1], rapid_read64(p + 56) ^ seed);
                }
            }
        }
      }
      a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
    }
    a ^= secret[1];
    b ^= seed;
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
  }
  /*
  *  rapidhashNano main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
    const uint8_t *p=(const uint8_t *)key;
    seed ^= rapid_mix(seed ^ secret[2], secret[1]);
    uint64_t a=0, b=0;
    size_t i = len;
    if (_likely_(len <= 16)) {
      if (len >= 4) {
        seed ^= len;
        if (len >= 8) {
          const uint8_t* plast = p + len - 8;
          a = rapid_read64(p);
          b = rapid_read64(plast);
        } else {
          const uint8_t* plast = p + len - 4;
          a = rapid_read32(p);
          b = rapid_read32(plast);
        }
      } else if (len > 0) {
        a = (((uint64_t)p[0])<<45)|p[len-1];
        b = p[len>>1];
      } else
        a = b = 0;
    } else {
      if (i > 48) {
        uint64_t see1 = seed, see2 = seed;
        do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          p += 48;
          i -= 48;
        } while(i > 48);
        seed ^= see1;
        seed ^= see2;
      }
      if (i > 16) {
        seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
        if (i > 32) {
            seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
        }
      }
      a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
    }
    a ^= secret[1];
    b ^= seed;
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
  }
 
/*
 *  rapidhash seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhash_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhash general purpose hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhash_withSeed(key, len, 0);
}
/*
 *  rapidhashMicro seeded hash function.
 *
 *  Designed for HPC and server applications, where cache misses make a noticeable performance detriment.
 *  Clang-18+ compiles it to ~140 instructions without stack usage, both on x86-64 and aarch64.
 *  Faster for sizes up to 512 bytes, just 15%-20% slower for inputs above 1kb.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
 RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhashMicro_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhashMicro hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhashMicro_withSeed(key, len, 0);
}
/*
 *  rapidhashNano seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
 RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhashNano_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhashNano hash function.
 *
 *  Designed for Mobile and embedded applications, where keeping a small code size is a top priority.
 *  Clang-18+ compiles it to less than 100 instructions without stack usage, both on x86-64 and aarch64.
 *  The fastest for sizes up to 48 bytes, but may be considerably slower for larger inputs.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhashNano_withSeed(key, len, 0);
}
// 计算 64 位哈希值
XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhash_internal(key, len, seed, rapid_secret);
}
// xrtHash64 相关处理
XXAPI uint64 xrtHash64(ptr key, size_t len)
{
	return xrtHash64_WithSeed(key, len, 0);
}
// xrtHash64_Micro_WithSeed 相关处理
XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhashMicro_internal(key, len, seed, rapid_secret);
}
// xrtHash64_Micro 相关处理
XXAPI uint64 xrtHash64_Micro(ptr key, size_t len)
{
	return xrtHash64_Micro_WithSeed(key, len, 0);
}
// xrtHash64_Nano_WithSeed 相关处理
XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhashNano_internal(key, len, seed, rapid_secret);
}
// xrtHash64_Nano 相关处理
XXAPI uint64 xrtHash64_Nano(ptr key, size_t len)
{
	return xrtHash64_Nano_WithSeed(key, len, 0);
}

// ========================================
// File: D:/git/xrt/lib/charset.h
// ========================================


// utf8 字符长度码表
static char BytesExtraTableUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};
// 内部函数：获取 UTF-8 额外字节数
static unsigned char __xrtBytesExtraUTF8(unsigned char c)
{
	return (unsigned char)BytesExtraTableUTF8[c];
}
// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			char iExtraBytes = (char)__xrtBytesExtraUTF8((unsigned char)sText[iSize]);
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			iSize += iExtraBytes + 1;
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			size_t iExtraBytes = (size_t)__xrtBytesExtraUTF8((unsigned char)sText[i]);
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			i += iExtraBytes;
		}
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		size_t iExtraBytes = (size_t)__xrtBytesExtraUTF8((unsigned char)sText[i]);
		if ( (i + iExtraBytes) >= iSize ) {
			sRet[iPos++] = 0xFFFD;
			break;
		}
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			size_t iNext = i + 1;
			 sRet[iPos++] = (unsigned short)((((uint32)sText[i]) & 0b00011111u) << 6) | (((uint32)sText[iNext]) & 0b00111111u);
			 i = iNext;
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			size_t iNext1 = i + 1;
			 size_t iNext2 = i + 2;
			 sRet[iPos++] = (unsigned short)(((((uint32)sText[i]) & 0b00001111u) << 12) | ((((uint32)sText[iNext1]) & 0b00111111u) << 6) | (((uint32)sText[iNext2]) & 0b00111111u));
			 i = iNext2;
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			if ( sText[i] & 0b00000100 ) {
				// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
				i += iExtraBytes;
			} else {
				size_t iNext1 = i + 1;
				 size_t iNext2 = i + 2;
				 size_t iNext3 = i + 3;
				 uint32 c = ((((uint32)sText[i]) & 0b00000011u) << 18) | ((((uint32)sText[iNext1]) & 0b00111111u) << 12) | ((((uint32)sText[iNext2]) & 0b00111111u) << 6) | (((uint32)sText[iNext3]) & 0b00111111u);
				if ( c < 0x10000 ) {
					// 原则上不会进入这个分支，除非遇到错误的编码
					sRet[iPos++] = c;
				} else {
					c -= 0x10000;
					sRet[iPos++] = 0b1101100000000000 | ((c & 0b11111111110000000000) >> 10);
					sRet[iPos++] = 0b1101110000000000 | (c & 0b00000000001111111111);
				}
				 i = iNext3;
			}
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		} else if ( iExtraBytes == 5 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			iPos++;
			iSize += __xrtBytesExtraUTF8((unsigned char)sText[iSize]) + 1;
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			iPos++;
			i += __xrtBytesExtraUTF8((unsigned char)sText[i]);
		}
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	// 申请所需内存
	u32str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		size_t iExtraBytes = (size_t)__xrtBytesExtraUTF8((unsigned char)sText[i]);
		if ( (i + iExtraBytes) >= iSize ) {
			sRet[iPos++] = 0xFFFD;
			break;
		}
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			size_t iNext = i + 1;
			 sRet[iPos++] = ((((uint32)sText[i]) & 0b00011111u) << 6) | (((uint32)sText[iNext]) & 0x3Fu);
			 i = iNext;
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			size_t iNext1 = i + 1;
			 size_t iNext2 = i + 2;
			 sRet[iPos++] = ((((uint32)sText[i]) & 0b00001111u) << 12) | ((((uint32)sText[iNext1]) & 0x3Fu) << 6) | (((uint32)sText[iNext2]) & 0x3Fu);
			 i = iNext2;
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			size_t iNext1 = i + 1;
			 size_t iNext2 = i + 2;
			 size_t iNext3 = i + 3;
			 sRet[iPos++] = ((((uint32)sText[i]) & 0b00000111u) << 18) | ((((uint32)sText[iNext1]) & 0x3Fu) << 12) | ((((uint32)sText[iNext2]) & 0x3Fu) << 6) | (((uint32)sText[iNext3]) & 0x3Fu);
			 i = iNext3;
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符
			size_t iNext1 = i + 1;
			 size_t iNext2 = i + 2;
			 size_t iNext3 = i + 3;
			 size_t iNext4 = i + 4;
			 sRet[iPos++] = ((((uint32)sText[i]) & 0b00000011u) << 24) | ((((uint32)sText[iNext1]) & 0x3Fu) << 18) | ((((uint32)sText[iNext2]) & 0x3Fu) << 12) | ((((uint32)sText[iNext3]) & 0x3Fu) << 6) | (((uint32)sText[iNext4]) & 0x3Fu);
			 i = iNext4;
		} else if ( iExtraBytes == 5 ) {
			// 六字节字符
			size_t iNext1 = i + 1;
			 size_t iNext2 = i + 2;
			 size_t iNext3 = i + 3;
			 size_t iNext4 = i + 4;
			 size_t iNext5 = i + 5;
			 sRet[iPos++] = ((((uint32)sText[i]) & 0b00000001u) << 30) | ((((uint32)sText[iNext1]) & 0x3Fu) << 24) | ((((uint32)sText[iNext2]) & 0x3Fu) << 18) | ((((uint32)sText[iNext3]) & 0x3Fu) << 12) | ((((uint32)sText[iNext4]) & 0x3Fu) << 6) | (((uint32)sText[iNext5]) & 0x3Fu);
			 i = iNext5;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) {
		iSize = u16len(sText);
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	for ( size_t i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			size_t iNext = i + 1;
			if ( (iNext < iSize) && ((sText[iNext] & 0b1111110000000000) == 0b1101110000000000) ) {
				iPos += 4;
			} else {
				// 错误的代理对，使用替换字符 EFBFBD 代替
				iPos += 3;
			}
			if ( iNext < iSize ) {
				i = iNext;
			}
		} else if ( iChar <= 0x7F ) {
			iPos++;
		} else if ( iChar <= 0x7FF ) {
			iPos += 2;
		} else {
			iPos += 3;
		}
	}
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			size_t iNextIndex = i + 1;
			if ( (iNextIndex < iSize) && ((sText[iNextIndex] & 0b1111110000000000) == 0b1101110000000000) ) {
				uint16 iNext = sText[iNextIndex];
				uint32 cp = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
				sRet[iPos++] = 0xF0 | ((cp >> 18) & 0x7);
				sRet[iPos++] = 0x80 | ((cp >> 12) & 0x3F);
				sRet[iPos++] = 0x80 | ((cp >> 6) & 0x3F);
				sRet[iPos++] = 0x80 | (cp & 0x3F);
			} else {
				// 错误的代理对，使用替换字符 EFBFBD 代替
				sRet[iPos++] = 0xEF;
				sRet[iPos++] = 0xBF;
				sRet[iPos++] = 0xBD;
			}
			if ( iNextIndex < iSize ) {
				i = iNextIndex;
			}
		} else if ( iChar <= 0x7F ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			sRet[iPos++] = 0xC0 | ((iChar & 0x7C0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else {
			sRet[iPos++] = 0xE0 | ((iChar & 0xF000) >> 12);
			sRet[iPos++] = 0x80 | ((iChar & 0xFC0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( (sText[iSize] & 0b1111110000000000) == 0b1101100000000000 ) {
				iSize += 2;
			} else {
				iSize++;
			}
			iPos++;
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				i++;
			}
			iPos++;
		}
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	// 申请所需内存
	u32str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u32str)xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			size_t iNextIndex = i + 1;
			if ( (iNextIndex < iSize) && ((sText[iNextIndex] & 0b1111110000000000) == 0b1101110000000000) ) {
				uint16 iNext = sText[iNextIndex];
				sRet[iPos++] = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
			} else {
				// 错误的代理对，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
			}
			if ( iNextIndex < iSize ) {
				i = iNextIndex;
			}
		} else {
			sRet[iPos++] = iChar;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint32 iChar = sText[iSize++];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0x7F ) {
			// ASCII 兼容字符
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			// 双字节字符
			sRet[iPos++] = 0xC0 | ((iChar >> 6) & 0x1F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0xFFFF ) {
			// 三字节字符
			sRet[iPos++] = 0xE0 | ((iChar >> 12) & 0xF);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x1FFFFF ) {
			// 四字节字符
			sRet[iPos++] = 0xF0 | ((iChar >> 18) & 0x7);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x3FFFFFF ) {
			// 五字节字符
			sRet[iPos++] = 0xF8 | ((iChar >> 24) & 0x3);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x7FFFFFFF ) {
			// 六字节字符
			sRet[iPos++] = 0xFC | ((iChar >> 30) & 0x1);
			sRet[iPos++] = 0x80 | ((iChar >> 24) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-32 转 utf-16 ( 需使用 xrtFree 释放 )
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( sText[iSize] <= 0xFFFF ) {
				iPos++;
			} else if ( sText[iSize] <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
			iSize++;
		}
	} else {
		for ( size_t i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0xFFFF ) {
				iPos++;
			} else if ( iChar <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
		}
	}
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (u16str)xCore.sNull; }
	// 开始转换编码
	iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0xFFFF ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x10FFFF ) {
			iChar -= 0x10000;
			sRet[iPos++] = 0b1101100000000000 | ((iChar & 0b11111111110000000000) >> 10);
			sRet[iPos++] = 0b1101110000000000 | (iChar & 0b00000000001111111111);
		} else {
			// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
			sRet[iPos++] = 0xFFFD;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	if ( iRetSize ) { *iRetSize = iPos; }
	return sRet;
}
// utf-16 大端序和小端序转换 ( 需使用 xrtFree 释放 )
XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { return (u16str)xCore.sNull; }
	u16str sRet;
	if ( bSrcRevise ) {
		sRet = sText;
	} else {
		sRet = xrtCopyStrU16(sText, iSize);
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		sRet[i] = ((sRet[i] & 0xFF) << 8) | ((sRet[i] >> 8) & 0xFF);
	}
	return sRet;
}
// utf-32 大端序和小端序转换 ( 需使用 xrtFree 释放 )
XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { return (u32str)xCore.sNull; }
	u32str sRet;
	if ( bSrcRevise ) {
		sRet = sText;
	} else {
		sRet = xrtCopyStrU32(sText, iSize);
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		sRet[i] = ((sRet[i] >> 24) & 0xFF) | ((sRet[i] >> 8) & 0xFF00) | ((sRet[i] << 8) & 0xFF0000) | ((sRet[i] << 24) & 0xFF000000);
	}
	return sRet;
}
// 任意编码转换 ( 需使用 xrtFree 释放 )
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 非 windows 平台 ( 仅支持 utf-8、utf-16、utf-32 三种编码互相转换 [ 含 LE、BE 字节序 ]，oem 编码固定为 utf-8 )
	#if defined(_WIN32) || defined(_WIN64)
	#else
		if ( iInCP == XRT_CP_OEM ) { iInCP = XRT_CP_UTF8; }
		if ( iOutCP == XRT_CP_OEM ) { iOutCP = XRT_CP_UTF8; }
	#endif
	// 如果编码相同，返回副本
	if ( iInCP == iOutCP ) {
		if ( (iInCP == XRT_CP_UTF16) || (iInCP == XRT_CP_UTF16_BE) ) {
			return xrtCopyStrU16(sText, iSize);
		} else if ( (iInCP == XRT_CP_UTF32) || (iInCP == XRT_CP_UTF32_BE) ) {
			return xrtCopyStrU32(sText, iSize);
		} else {
			return xrtCopyStr(sText, iSize);
		}
	}
	// 需要转换编码 - 排列组合 20 种情况 ( 内置支持的编码转换组合 )
	if ( iInCP == XRT_CP_UTF8 ) {
		if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF8to16(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF8to32(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			size_t iRet = 0;
			u16str sRet = xrtUTF8to16(sText, iSize, &iRet);
			xrtUTF16LEtoBE(sRet, iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			size_t iRet = 0;
			u32str sRet = xrtUTF8to32(sText, iSize, &iRet);
			xrtUTF32LEtoBE(sRet, iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF16 ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			return xrtUTF16to8(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF16to32(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			return xrtUTF16LEtoBE(sText, iSize, FALSE);
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			size_t iRet = 0;
			u32str sRet = xrtUTF16to32(sText, iSize, &iRet);
			xrtUTF32LEtoBE(sRet, iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF32 ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			return xrtUTF32to8(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF32to16(sText, iSize, NULL);
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			size_t iRet = 0;
			u16str sRet = xrtUTF32to16(sText, iSize, &iRet);
			xrtUTF16LEtoBE(sRet, iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			return xrtUTF32LEtoBE(sText, iSize, FALSE);
		}
	} else if ( iInCP == XRT_CP_UTF16_BE ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			str sRet = xrtUTF16to8(sTemp, iSize, NULL);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			u32str sRet = xrtUTF16to32(sTemp, iSize, NULL);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			return xrtUTF16LEtoBE(sText, iSize, FALSE);
		} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			size_t iRet = 0;
			u32str sRet = xrtUTF16to32(sTemp, iSize, &iRet);
			xrtFree(sTemp);
			xrtUTF32LEtoBE(sRet, iRet, TRUE);
			return sRet;
		}
	} else if ( iInCP == XRT_CP_UTF32_BE ) {
		if ( iOutCP == XRT_CP_UTF8 ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			str sRet = xrtUTF32to8(sTemp, iSize, NULL);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			u16str sRet = xrtUTF32to16(sTemp, iSize, NULL);
			xrtFree(sTemp);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			u32str sTemp = xrtUTF32LEtoBE(sText, iSize, FALSE);
			size_t iRet = 0;
			u16str sRet = xrtUTF32to16(sTemp, iSize, &iRet);
			xrtFree(sTemp);
			xrtUTF16LEtoBE(sRet, iRet, TRUE);
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF32 ) {
			return xrtUTF32LEtoBE(sText, iSize, FALSE);
		}
	}
	// 内置方案无法满足时的扩展方案
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案 - 调用 Win32SDK 转换 - 五种排列组合优化
		if ( iInCP == XRT_CP_UTF16 ) {
			// UTF16 转 多字节
			if ( iSize == 0 ) { iSize = u16len(sText); }
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sText, iSize, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sText, iSize, (LPSTR)sRet, iRet, NULL, NULL);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iInCP == XRT_CP_UTF16_BE ) {
			// UTF16 BE 转 多字节
			if ( iSize == 0 ) { iSize = u16len(sText); }
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			u16str sTemp = xrtUTF16LEtoBE(sText, iSize, FALSE);
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sTemp, iSize, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sTemp, iSize, (LPSTR)sRet, iRet, NULL, NULL);
			xrtFree(sTemp);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16 ) {
			// 多字节 转 UTF16
			if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			size_t iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRet == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			u16str sRet = xrtMalloc((iRet + 1) * sizeof(unsigned short));
			if ( sRet == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, sRet, iRet);
			sRet[iRet] = 0;
			return sRet;
		} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			// 多字节 转 UTF16 BE
			if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			size_t iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRet == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			u16str sRet = xrtMalloc((iRet + 1) * sizeof(unsigned short));
			if ( sRet == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			iRet = MultiByteToWideChar(iInCP, 0, sText, iSize, sRet, iRet);
			sRet[iRet] = 0;
			xrtUTF16LEtoBE(sRet, iRet, TRUE);
			return sRet;
		} else {
			// 多字节 转 多字节
			if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			// 先转换为 utf16
			size_t iRetW = MultiByteToWideChar(iInCP, 0, sText, iSize, NULL, 0);
			if ( iRetW == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			u16str sRetW = xrtMalloc((iRetW + 1) * sizeof(unsigned short));
			if ( sRetW == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			iRetW = MultiByteToWideChar(iInCP, 0, sText, iSize, sRetW, iRetW);
			sRetW[iRetW] = 0;
			// 再转换为最终编码
			size_t iRet = WideCharToMultiByte(iOutCP, 0, sRetW, iRetW, NULL, 0, NULL, NULL);
			if ( iRet == 0 ) {
				if ( iRetSize ) { *iRetSize = 0; }
				xrtFree(sRetW);
				return xCore.sNull;
			}
			str sRet = xrtMalloc(iRet + 1);
			if ( sRet == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				xrtFree(sRetW);
				return xCore.sNull;
			}
			iRet = WideCharToMultiByte(iOutCP, 0, sRetW, iRetW, (LPSTR)sRet, iRet, NULL, NULL);
			xrtFree(sRetW);
			sRet[iRet] = 0;
			return sRet;
		}
	#else
		// 其他平台方案 - 暂无 ( 可使用 libiconv 等库，但是太大了 )
	#endif
	// 无法处理的编码转换组合
	xrtSetError("Unsupported charset !", FALSE);
	if ( iRetSize ) { *iRetSize = 0; }
	return xCore.sNull;
}
// 是否为 utf-8 字符串
XXAPI bool xrtIsUTF8(str sText, size_t iSize)
{
	// NULL 返回 FALSE，空字符串返回 TRUE
	if ( sText == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
	if ( iSize == 0 ) { return TRUE; }
	// 检测是否符合标准
	for ( size_t i = 0; i < iSize; i++ ) {
		// 遇到 \0、FE、FF 直接返回 FALSE
		if ( (sText[i] == 0) || (sText[i] == 0xFE) || (sText[i] == 0xFF) ) {
			return FALSE;
		}
		// 检查多字节字符是否已 0b10 开头
		size_t iExtraBytes = (size_t)__xrtBytesExtraUTF8((unsigned char)sText[i]);
		if ( iExtraBytes ) {
			for ( size_t j = 0; (j < iExtraBytes) && ((i + 1) < iSize); j++ ) {
				if ( (sText[++i] & 0b11000000) != 0b10000000 ) {
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}
// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM)
{
	if ( sText == NULL ) { return XRT_CP_BINARY; }
	if ( iSize == 0 ) { return XRT_CP_BINARY; }
	u8str sPtr = sText;
	int bNoUTF8 = FALSE;		// 是否不符合 UTF8 标准
	int bNoUTF16LE = FALSE;		// 是否不符合 UTF16 标准
	int bNoUTF16BE = FALSE;		// 是否不符合 UTF16 BE 标准
	int bNoUTF32LE = FALSE;		// 是否不符合 UTF32 标准
	int bNoUTF32BE = FALSE;		// 是否不符合 UTF32 BE 标准
	int iNullBE = 0;			// UTF16 大端序方向 \0 的数量
	int iNullLE = 0;			// UTF16 小端序方向 \0 的数量
	int iNullSize = 0;			// 遇到连续 \0 的次数
	int iMaxNull = 0;			// 最多连续 \0 的数量
	// 通过 BOM 判断字符串编码
	if ( bBOM ) {
		if ( iSize >= 3 ) {
			if ( (sPtr[0] == 0xEF) && (sPtr[1] == 0xBB) && (sPtr[2] == 0xBF) ) {
				return XRT_CP_UTF8 | XRT_CP_BOM;
			}
		}
		if ( iSize >= 4 ) {
			if ( (sPtr[0] == 0xFF) && (sPtr[1] == 0xFE) && (sPtr[2] == 0x00) && (sPtr[3] == 0x00) ) {
				return XRT_CP_UTF32 | XRT_CP_BOM;
			}
			if ( (sPtr[0] == 0x00) && (sPtr[1] == 0x00) && (sPtr[2] == 0xFE) && (sPtr[3] == 0xFF) ) {
				return XRT_CP_UTF32_BE | XRT_CP_BOM;
			}
		}
		if ( iSize >= 2 ) {
			if ( (sPtr[0] == 0xFF) && (sPtr[1] == 0xFE) ) {
				return XRT_CP_UTF16 | XRT_CP_BOM;
			}
			if ( (sPtr[0] == 0xFE) && (sPtr[1] == 0xFF) ) {
				return XRT_CP_UTF16_BE | XRT_CP_BOM;
			}
		}
	}
	// 开始推测字符串编码
	for ( size_t i = 0; i < iSize; i++ ) {
		// 检测 utf-8 不可能出现的字符
		if ( (sPtr[i] == 0xFE) || (sPtr[i] == 0xFF) ) {
			bNoUTF8 = TRUE;
		}
		// 检测 utf-8 多字符编码是否正确
		if ( bNoUTF8 == FALSE ) {
			size_t iExtraBytes = (size_t)__xrtBytesExtraUTF8((unsigned char)sPtr[i]);
			for ( size_t j = 1; (j <= iExtraBytes) && ((i + j) < iSize); j++ ) {
				if ( (sPtr[i + j] & 0b11000000) != 0b10000000 ) {
					bNoUTF8 = TRUE;
					break;
				}
			}
		}
		// 检测 utf-16 代理区是否合规
		if ( (i & 1) == 0 ) {
			if ( (i + 2u) < iSize ) {
				if ( bNoUTF16BE == FALSE ) {
					if ( (sPtr[i] & 0b11111100) == 0b11011000 ) {
						if ( (sPtr[i + 2] & 0b11111100) != 0b11011100 ) {
							bNoUTF16BE = TRUE;
						}
					}
				}
				if ( bNoUTF16LE == FALSE ) {
					if ( (sPtr[i] & 0b11111100) == 0b11011100 ) {
						if ( (sPtr[i + 2] & 0b11111100) != 0b11011000 ) {
							bNoUTF16LE = TRUE;
						}
					}
				}
			}
		}
		// 检测是否符合 utf-32 范围 ( 0x10FFFF 以内‌ )
		if ( (i & 3) == 0 ) {
			if ( (i + 3u) < iSize ) {
				if ( bNoUTF32BE == FALSE ) {
					uint32 c = ((uint32)(uint8)sPtr[i] << 24) |
						((uint32)(uint8)sPtr[i + 1] << 16) |
						((uint32)(uint8)sPtr[i + 2] << 8) |
						(uint32)(uint8)sPtr[i + 3];
					if ( c > 0x10FFFF ) {
						bNoUTF32BE = TRUE;
					}
				}
				if ( bNoUTF32LE == FALSE ) {
					uint32 c = ((uint32)(uint8)sPtr[i + 3] << 24) |
						((uint32)(uint8)sPtr[i + 2] << 16) |
						((uint32)(uint8)sPtr[i + 1] << 8) |
						(uint32)(uint8)sPtr[i];
					if ( c > 0x10FFFF ) {
						bNoUTF32LE = TRUE;
					}
				}
			}
		}
		// 统计连续 \0 长度
		if ( sPtr[i] == 0 ) {
			iNullSize++;
			if ( i & 1 ) {
				iNullLE++;		// LE 英文字符特征 : X0
			} else {
				iNullBE++;		// BE 英文字符特征 : 0X
			}
		} else {
			if ( iNullSize > iMaxNull ) {
				iMaxNull = iNullSize;
			}
			iNullSize = 0;
		}
	}
	// 根据条件推测编码
	if ( iMaxNull > 3 ) {
		return XRT_CP_BINARY;
	} else if ( iMaxNull > 1 ) {
		if ( bNoUTF32LE == FALSE ) {
			return XRT_CP_UTF32;
		} else if ( bNoUTF32BE == FALSE ) {
			return XRT_CP_UTF32_BE;
		} else {
			return XRT_CP_BINARY;
		}
	} else if ( iMaxNull == 1 ) {
		if ( bNoUTF16LE == FALSE ) {
			if ( bNoUTF16BE == FALSE ) {
				// 当无法区分到底是 utf16 BE 还是 LE 时，根据 \0 出现较多的方向判断
				if ( iNullBE > iNullLE ) {
					return XRT_CP_UTF16_BE;
				} else {
					return XRT_CP_UTF16;
				}
			}
			return XRT_CP_UTF16;
		} else if ( bNoUTF16BE == FALSE ) {
			return XRT_CP_UTF16_BE;
		} else if ( bNoUTF32LE == FALSE ) {
			return XRT_CP_UTF32;
		} else if ( bNoUTF32BE == FALSE ) {
			return XRT_CP_UTF32_BE;
		} else {
			return XRT_CP_BINARY;
		}
	} else {
		if ( bNoUTF8 == FALSE ) {
			return XRT_CP_UTF8;
		} else {
			#if defined(_WIN32) || defined(_WIN64)
				return XRT_CP_OEM;
			#else
				return XRT_CP_BINARY;
			#endif
		}
	}
}
// 获取不同字符集的字符大小
XXAPI int xrtGetCharSize(int iCP)
{
	iCP &= XRT_MASK_BOM;
	if ( (iCP == XRT_CP_UTF16) || (iCP == XRT_CP_UTF16_BE) ) {
		return 2;
	} else if ( (iCP == XRT_CP_UTF32) || (iCP == XRT_CP_UTF32_BE) ) {
		return 4;
	} else {
		return 1;
	}
}

// ========================================
// File: D:/git/xrt/lib/math.h
// ========================================


/*
	PCG Random Number Generation for C. [ Update : 2025/08/19 from https://github.com/imneme/pcg-c-basic ]
	修改：
		整合到 xrt 库
		提供 Ex 版本 API（调用者管理状态，线程安全）
		普通版本 API 使用线程状态（线程隔离，性能优先）
	使用协议注意事项：
		Apache License, Version 2.0 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/
/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */
// 初始化随机数生成器
XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq)
{
	rng->state = 0U;
	rng->inc = (seq << 1u) | 1u;
	// 运算两次
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	rng->state += seed;
	oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
}
// 生成 32 位随机数 - 线程安全
XXAPI uint32 xrtRand32Ex(xrand* rng)
{
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	uint32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32 rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}
// 生成 64 位随机数 - 线程安全
XXAPI uint64 xrtRand64Ex(xrand* rngLow, xrand* rngHigh)
{
	uint32 iLow = xrtRand32Ex(rngLow);
	uint32 iHigh = xrtRand32Ex(rngHigh);
	return ((uint64)iHigh << 32) | (uint64)iLow;
}
// 生成范围随机数 - 线程安全
XXAPI int xrtRandRangeEx(xrand* rng, int min, int max)
{
	if ( min <= max ) {
		uint32 iRange = (uint32)(((int64)max - (int64)min) + 1);
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + min;
		}
	} else {
		uint32 iRange = (uint32)(((int64)min - (int64)max) + 1);
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + max;
		}
	}
	return 0;
}
// 获取 32 位随机数
XXAPI uint32 xrtRand32()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}
	return xrtRand32Ex(&pThreadData->rand32);
}
// 获取 64 位随机数
XXAPI uint64 xrtRand64()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}
	return xrtRand64Ex(&pThreadData->rand64_low, &pThreadData->rand64_high);
}
// 获取 32 位范围随机数
XXAPI int xrtRandRange(int min, int max)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}
	return xrtRandRangeEx(&pThreadData->rand32, min, max);
}
// 整数约等于
XXAPI bool xrtIntApprox(int64 a, int64 b)
{
	if ( a == b ) { return TRUE; }
	
	int64 diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxIntMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxIntTol <= 0.0 ) { return FALSE; }
		int64 maxAbs = (a >= 0 ? a : -a);
		int64 bAbs = (b >= 0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0 ) { return TRUE; }
		double ratio = (double)diff / (double)maxAbs;
		return (ratio <= xCore.fApproxIntTol);
	} else {
		// 差值模式
		return (diff <= (int64)xCore.fApproxIntTol);
	}
}
// 浮点数约等于
XXAPI bool xrtNumApprox(double a, double b)
{
	if ( a == b ) { return TRUE; }
	
	double diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxNumMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxNumTol <= 0.0 ) { return FALSE; }
		double maxAbs = (a >= 0.0 ? a : -a);
		double bAbs = (b >= 0.0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0.0 ) { return TRUE; }
		double ratio = diff / maxAbs;
		return (ratio <= xCore.fApproxNumTol);
	} else {
		// 差值模式
		return (diff <= xCore.fApproxNumTol);
	}
}

// ========================================
// File: D:/git/xrt/lib/path.h
// ========================================


// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetNameExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}
// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetName(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	size_t iPointPos = 0;
	for ( size_t i = iSize; i-- > 0; ) {
		if ( sPath[i] == L'.' ) {
			iPointPos = iSize - i;
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - iPointPos - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}
// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( sPath[i] == L'.' ) {
			return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			return xCore.sNull;
		}
	}
	return xCore.sNull;
}
// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetDir(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xrtCopyStr(sPath, iSize - 1);
			} else {
				return xrtCopyStr(sPath, i);
			}
		}
	}
	return xCore.sNull;
}
// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
XXAPI bool xrtPathIsAbs(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return FALSE; }
	if ( sPath[0] == '/' ) {
		return TRUE;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( sPath[i] == ':' ) {
			return TRUE;
		}
	}
	return FALSE;
}
// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen)
{
	if ( sHead ) {
		if ( iHeadSize == 0 ) { iHeadSize = strlen((const char*)sHead); }
		if ( iHeadSize == 0 ) { sHead = NULL; }
	} else {
		iHeadSize = 0;
	}
	if ( sFoot ) {
		if ( iFootSize == 0 ) { iFootSize = strlen((const char*)sFoot); }
		if ( iFootSize == 0 ) { sFoot = NULL; }
	} else {
		iFootSize = 0;
	}
	int iSize = iHeadSize + iFootSize + iLen;
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	if ( sHead ) {
		memcpy(sRet, sHead, iHeadSize);
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		int idx = xrtRandRange(0, 61);		// 这里写 61，可以忽略 - 和 _ 字符
		sRet[iHeadSize + i] = RandStringDefaultTemplate[idx];
	}
	if ( sFoot ) {
		memcpy(&sRet[iHeadSize + iLen], sFoot, iFootSize);
	}
	sRet[iSize] = 0;
	return sRet;
}
// 拼接路径（ 需要使用 xrtFree 释放内存 ）
XXAPI str xrtPathJoin(uint iCount, ...)
{
	if ( iCount == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(4096);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	va_list args;
	va_start(args, iCount);
	size_t iPos = 0;
	for ( uint i = 0; i < iCount; i++ ) {
		str sPath = va_arg(args, str);
		if ( sPath == NULL ) { continue; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { continue; }
		if ( (iPos + iSize) > 4094 ) { xrtFree(sRet); return xCore.sNull; }
		memcpy(&sRet[iPos], sPath, iSize);
		iPos += iSize;
		if ( i < (iCount - 1) ) {
			if ( (sRet[iPos - 1] != '\\') && (sRet[iPos - 1] != '/') ) {
				#if defined(_WIN32) || defined(_WIN64)
					sRet[iPos] = '\\';
				#else
					sRet[iPos] = '/';
				#endif
				iPos++;
			}
		}
	}
	va_end(args);
	if ( iPos > 4000 ) {
		sRet[iPos] = 0;
		return sRet;
	} else {
		str sRetTrim = xrtMalloc(iPos + 1);
		if ( sRetTrim == NULL ) {
			sRet[iPos] = 0;
			return sRet;
		} else {
			memcpy(sRetTrim, sRet, iPos);
			xrtFree(sRet);
			sRetTrim[iPos] = 0;
			return sRetTrim;
		}
	}
}
#ifndef XRT_NO_TIME

// ========================================
// File: D:/git/xrt/lib/time.h
// ========================================


// 获取高精度时钟 Tick ( 返回秒数，便于计算时间间隔 )
XXAPI double xrtTimer()
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( xCore.Frequency == 0.0 ) {
			return (double)GetTickCount64() * 0.001;
		} else {
			LARGE_INTEGER QPC;
			QueryPerformanceCounter(&QPC);
			return (double)QPC.QuadPart / (double)xCore.Frequency;
		}
	#else
		// 其他平台方案
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		return timer.tv_sec + ((double)timer.tv_nsec * 0.000000001);
	#endif
}
// 毫秒级延时
XXAPI void xrtSleep(uint32 ms)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		Sleep(ms);
	#else
		// 其他平台方案
		struct timespec req;
		req.tv_sec = ms / 1000;
		req.tv_nsec = (long)((ms % 1000) * 1000000L);
		while ( nanosleep(&req, &req) == -1 && errno == EINTR ) {
		}
	#endif
}
// 判断是否为闰年
XXAPI bool xrtIsLeapYear(int iYear)
{
	if ( (iYear % 400) == 0 ) {
		return TRUE;
	} else if ( (iYear % 100) == 0 ) {
		return FALSE;
	} else {
		if ( (iYear & 3) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
}
// 获取某年某月有多少天
XXAPI int xrtDaysInMonth(int iYear, int iMonth)
{
	static const int arrDays[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if ( (iMonth >= 1) && (iMonth <= 12) ) {
		if ( iMonth == 2 ) {
			if ( xrtIsLeapYear(iYear) ) {
				return 29;
			} else {
				return 28;
			}
		} else {
			return arrDays[iMonth - 1];
		}
	} else {
		return 0;
	}
}
// 获取某年有多少天
XXAPI int xrtDaysInYear(int iYear)
{
	if ( xrtIsLeapYear(iYear) ) {
		return 366;
	} else {
		return 365;
	}
}
// 构建时间
XXAPI xtime xrtTimeSerial(int iHour, int iMinute, int iSecond)
{
	return (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
}
// 构建日期
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay)
{
	int iDaysInMonth;
	if ( (iMonth < 1) || (iMonth > 12) ) {
		xrtSetError("Month range error !", FALSE);
		return 0;
	}
	iDaysInMonth = xrtDaysInMonth((int)iYear, iMonth);
	if ( (iDay < 1) || (iDay > iDaysInMonth) ) {
		xrtSetError("Day range error !", FALSE);
		return 0;
	}
	xtime iDate = (iDay - 1) * XRT_TIME_DAY;
	for ( int i = 1; i < iMonth; i++ ) {
		iDate += xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
	}
	if ( iYear < 0 ) {
		// 公元前
		iYear = llabs(iYear);
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( uint64 i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear((int)i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
		iDate = iDate * -1;
	} else {
		// 公元后
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( uint64 i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear((int)i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
	}
	return iDate;
}
// 构建日期 + 时间
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xtime iTime = (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
	xtime iDate = xrtDateSerial(iYear, iMonth, iDay);
	return iDate + iTime;
}
// 获取时间中的秒
XXAPI int xrtSecond(xtime iTime)
{
	return iTime % XRT_TIME_MINUTE;
}
// 获取时间中的分钟
XXAPI int xrtMinute(xtime iTime)
{
	return (iTime % XRT_TIME_HOUR) / 60;
}
// 获取时间中的小时
XXAPI int xrtHour(xtime iTime)
{
	return (iTime % XRT_TIME_DAY) / XRT_TIME_HOUR;
}
// 获取时间中的日期
XXAPI int xrtDay(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			break;
		}
	}
	int iDay = 1;
	for ( int i = 1; i <= 31; i++ ) {
		if ( iYearMod >= XRT_TIME_DAY ) {
			iYearMod -= XRT_TIME_DAY;
		} else {
			iDay = i;
			break;
		}
	}
	return iDay;
}
// 获取时间中的月份
XXAPI int xrtMonth(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	return iMonth;
}
// 获取时间中的年份
XXAPI int64 xrtYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( iTime < 0 ) {
		return -iYear;
	} else {
		return iYear;
	}
}
// 获取时间中的星期
XXAPI int xrtWeekday(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iDay = iTimeAbs / XRT_TIME_DAY;
	return iDay % 7;
}
// 获取时间是当年的第几天
XXAPI int xrtDayOfYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	return 1 + (iYearMod / XRT_TIME_DAY);
}
// 解码时间
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, int* pHour, int* pMinute, int* pSecond, int* pWeekday, int* pDayOfYear)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( pYear ) {
		if ( iTime < 0 ) {
			*pYear = -iYear;
		} else {
			*pYear = iYear;
		}
	}
	if ( pDayOfYear ) {
		*pDayOfYear = 1 + (iYearMod / XRT_TIME_DAY);
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	if ( pMonth ) {
		*pMonth = iMonth;
	}
	if ( pDay ) {
		int iDay = 1;
		for ( int i = 1; i <= 31; i++ ) {
			if ( iYearMod >= XRT_TIME_DAY ) {
				iYearMod -= XRT_TIME_DAY;
			} else {
				iDay = i;
				break;
			}
		}
		*pDay = iDay;
	}
	if ( pHour ) {
		*pHour = (iTimeAbs % XRT_TIME_DAY) / XRT_TIME_HOUR;
	}
	if ( pMinute ) {
		*pMinute = (iTimeAbs % XRT_TIME_HOUR) / 60;
	}
	if ( pSecond ) {
		*pSecond = iTimeAbs % XRT_TIME_MINUTE;
	}
	if ( pWeekday ) {
		uint64 iDay = iTimeAbs / XRT_TIME_DAY;
		*pWeekday = iDay % 7;
	}
}
// 获取当前日期 + 时间 (线程安全)
XXAPI xtime xrtNow()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtDateTimeSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}
// 获取当前日期 (线程安全)
XXAPI xtime xrtDate()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtDateSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday);
}
// 获取当前时间 (线程安全)
XXAPI xtime xrtTime()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtTimeSerial(stm.tm_hour, stm.tm_min, stm.tm_sec);
}
// 获取字符串格式的当前日期 + 时间（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtNowStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%d-%02d-%02d %02d:%02d:%02d", 1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}
// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtDateStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%d-%02d-%02d", 1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday);
}
// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtTimeStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%02d:%02d:%02d", stm.tm_hour, stm.tm_min, stm.tm_sec);
}
// 转换日期 + 时间为字符串（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtTimeToStr(xtime iTime, int iFormat)
{
	if ( iFormat == XRT_TIME_FORMAT_DATETIME ) {
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtFormat("%d-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
	} else if ( iFormat == XRT_TIME_FORMAT_DATE ) {
		int64 iYear;
		int iMonth, iDay;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, NULL, NULL, NULL, NULL, NULL);
		return xrtFormat("%d-%02d-%02d", iYear, iMonth, iDay);
	} else if ( iFormat == XRT_TIME_FORMAT_TIME ) {
		int iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtFormat("%02d:%02d:%02d", iHour, iMinute, iSecond);
	} else {
		return xCore.sNull;
	}
}
// 时间单位累加
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime)
{
	if ( interval == XRT_TIME_INTERVAL_YEAR ) {
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtDateTimeSerial(iYear + iValue, iMonth, iDay, iHour, iMinute, iSecond);
	} else if ( interval == XRT_TIME_INTERVAL_MONTH ) {
		xtime iValueAbs = llabs(iValue);
		uint64 iAddYear = iValueAbs / 12;
		uint64 iAddMonth = iValueAbs % 12;
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		if ( iValue < 0 ) {
			if ( iMonth - iAddMonth < 1 ) {
				iYear = iYear - iAddYear - 1;
				iMonth = 12 - (iAddMonth - iMonth);
			} else {
				iYear = iYear - iAddYear;
				iMonth -= iAddMonth;
			}
		} else {
			if ( iMonth + iAddMonth > 12 ) {
				iYear = iYear + iAddYear + 1;
				iMonth = (iMonth + iAddMonth) % 12;
			} else {
				iYear = iYear + iAddYear;
				iMonth += iAddMonth;
			}
		}
		xtime tRet = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return tRet;
	} else if ( interval == XRT_TIME_INTERVAL_DAY ) {
		return iTime + (iValue * XRT_TIME_DAY);
	} else if ( interval == XRT_TIME_INTERVAL_HOUR ) {
		return iTime + (iValue * XRT_TIME_HOUR);
	} else if ( interval == XRT_TIME_INTERVAL_MINUTE ) {
		return iTime + (iValue * XRT_TIME_MINUTE);
	} else if ( interval == XRT_TIME_INTERVAL_SECOND ) {
		return iTime + iValue;
	} else if ( interval == XRT_TIME_INTERVAL_WEEKDAY ) {
		return iTime + (iValue * XRT_TIME_DAY * 7);
	} else if ( interval == XRT_TIME_INTERVAL_QUARTER ) {
		return xrtDateAdd(XRT_TIME_INTERVAL_MONTH, iValue * 3, iTime);
	} else {
		return iTime;
	}
}
// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2)
{
	if ( interval == XRT_TIME_INTERVAL_YEAR ) {
		int64 iYear1 = xrtYear(iTime1);
		int64 iYear2 = xrtYear(iTime2);
		return iYear2 - iYear1;
	} else if ( interval == XRT_TIME_INTERVAL_MONTH ) {
		int64 iYear1, iYear2;
		int iMonth1, iMonth2;
		xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
		xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
		return ((iYear2 - iYear1) * 12) + (iMonth2 - iMonth1);
	} else if ( interval == XRT_TIME_INTERVAL_DAY ) {
		return (iTime2 - iTime1) / XRT_TIME_DAY;
	} else if ( interval == XRT_TIME_INTERVAL_HOUR ) {
		return (iTime2 - iTime1) / XRT_TIME_HOUR;
	} else if ( interval == XRT_TIME_INTERVAL_MINUTE ) {
		return (iTime2 - iTime1) / XRT_TIME_MINUTE;
	} else if ( interval == XRT_TIME_INTERVAL_SECOND ) {
		return iTime2 - iTime1;
	} else if ( interval == XRT_TIME_INTERVAL_QUARTER ) {
		int64 iYear1, iYear2;
		int iMonth1, iMonth2;
		xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
		xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
		return (((iYear2 - iYear1) * 12) + (iMonth2 - iMonth1)) / 3;
	} else {
		return 0;
	}
}
// 获取季度（1-4）
XXAPI int xrtQuarter(xtime iTime)
{
	int iMonth = xrtMonth(iTime);
	return ((iMonth - 1) / 3) + 1;
}
// 获取日期部分（去除时间）
XXAPI xtime xrtDatePart(xtime iTime)
{
	xtime iMod = iTime % XRT_TIME_DAY;
	if ( iMod < 0 ) {
		iMod += XRT_TIME_DAY;
	}
	return iTime - iMod;
}
// 获取时间部分（去除日期）
XXAPI xtime xrtTimePart(xtime iTime)
{
	xtime iMod = iTime % XRT_TIME_DAY;
	if ( iMod < 0 ) {
		iMod += XRT_TIME_DAY;
	}
	return iMod;
}
// 是否同一天
XXAPI bool xrtIsSameDay(xtime iTime1, xtime iTime2)
{
	return xrtDatePart(iTime1) == xrtDatePart(iTime2);
}
// 是否同一月
XXAPI bool xrtIsSameMonth(xtime iTime1, xtime iTime2)
{
	int64 iYear1, iYear2;
	int iMonth1, iMonth2;
	xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
	xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
	return (iYear1 == iYear2) && (iMonth1 == iMonth2);
}
// 是否同一年
XXAPI bool xrtIsSameYear(xtime iTime1, xtime iTime2)
{
	return xrtYear(iTime1) == xrtYear(iTime2);
}
// 判断时间是否在区间内
XXAPI bool xrtTimeInRange(xtime iTime, xtime iStart, xtime iEnd)
{
	return (iTime >= iStart) && (iTime <= iEnd);
}
// 判断两个时间区间是否重叠
XXAPI bool xrtTimeRangeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2)
{
	return (iStart1 <= iEnd2) && (iEnd1 >= iStart2);
}
// 与Unix时间戳互转 - xtime转Unix时间戳
XXAPI int64 xrtToUnixTime(xtime iTime)
{
	return iTime - XRT_TIME_19700101;
}
// 与Unix时间戳互转 - Unix时间戳转xtime
XXAPI xtime xrtFromUnixTime(int64 unixTime)
{
	return XRT_TIME_19700101 + unixTime;
}
// 获取月份的第一天
XXAPI xtime xrtFirstDayOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth;
	xrtDecodeSerial(iTime, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	return xrtDateSerial(iYear, iMonth, 1);
}
// 获取月份的最后一天
XXAPI xtime xrtLastDayOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth;
	xrtDecodeSerial(iTime, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	int iDays = xrtDaysInMonth((int)iYear, iMonth);
	return xrtDateSerial(iYear, iMonth, iDays);
}
// 获取年份的第一天
XXAPI xtime xrtFirstDayOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	return xrtDateSerial(iYear, 1, 1);
}
// 获取年份的最后一天
XXAPI xtime xrtLastDayOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	return xrtDateSerial(iYear, 12, 31);
}
// 获取周的第一天（iStartDay: 0=周日, 1=周一, ...）
XXAPI xtime xrtFirstDayOfWeek(xtime iTime, int iStartDay)
{
	int iWeekday = xrtWeekday(iTime);
	int iDiff = iWeekday - iStartDay;
	if ( iDiff < 0 ) {
		iDiff += 7;
	}
	return xrtDatePart(iTime) - (iDiff * XRT_TIME_DAY);
}
// 获取周的最后一天（iStartDay: 0=周日, 1=周一, ...）
XXAPI xtime xrtLastDayOfWeek(xtime iTime, int iStartDay)
{
	return xrtFirstDayOfWeek(iTime, iStartDay) + (6 * XRT_TIME_DAY);
}
// 获取当年第几周（ISO周数，周一为一周开始）
XXAPI int xrtWeekOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	xtime iFirstDayOfYear = xrtDateSerial(iYear, 1, 1);
	int iFirstWeekday = xrtWeekday(iFirstDayOfYear);
	int iFirstDayOffset = (iFirstWeekday == 0) ? 6 : (iFirstWeekday - 1);
	int iDayOfYear = xrtDayOfYear(iTime);
	return ((iDayOfYear - 1 + iFirstDayOffset) / 7) + 1;
}
// 获取当月第几周（周一为一周开始）
XXAPI int xrtWeekOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth, iDay;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, NULL, NULL, NULL, NULL, NULL);
	xtime iFirstDayOfMonth = xrtDateSerial(iYear, iMonth, 1);
	int iFirstWeekday = xrtWeekday(iFirstDayOfMonth);
	int iFirstDayOffset = (iFirstWeekday == 0) ? 6 : (iFirstWeekday - 1);
	return ((iDay - 1 + iFirstDayOffset) / 7) + 1;
}
// 获取UTC时间
XXAPI xtime xrtNowUTC()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		#ifdef __TINYC__
			// TCC 不支持 gmtime_s，使用 gmtime
			struct tm* pstm = gmtime(&rawtime);
			if ( pstm ) {
				stm = *pstm;
			} else {
				return 0;
			}
		#else
			if ( gmtime_s(&stm, &rawtime) != 0 ) {
				return 0;
			}
		#endif
	#else
		gmtime_r(&rawtime, &stm);
	#endif
	return xrtDateTimeSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}
// 获取本地时区偏移（秒）
XXAPI int xrtTimezoneOffset()
{
	time_t rawtime = time(NULL);
	struct tm stmLocal, stmUTC;
	#if defined(_WIN32) || defined(_WIN64)
		#ifdef __TINYC__
			// TCC 不支持 localtime_s/gmtime_s，使用普通版本
			struct tm* pLocal = localtime(&rawtime);
			if ( pLocal ) { stmLocal = *pLocal; } else { return 0; }
			struct tm* pUTC = gmtime(&rawtime);
			if ( pUTC ) { stmUTC = *pUTC; } else { return 0; }
		#else
			localtime_s(&stmLocal, &rawtime);
			if ( gmtime_s(&stmUTC, &rawtime) != 0 ) {
				return 0;
			}
		#endif
	#else
		localtime_r(&rawtime, &stmLocal);
		gmtime_r(&rawtime, &stmUTC);
	#endif
	xtime iLocal = xrtDateTimeSerial(1900 + stmLocal.tm_year, stmLocal.tm_mon + 1, stmLocal.tm_mday, stmLocal.tm_hour, stmLocal.tm_min, stmLocal.tm_sec);
	xtime iUTC = xrtDateTimeSerial(1900 + stmUTC.tm_year, stmUTC.tm_mon + 1, stmUTC.tm_mday, stmUTC.tm_hour, stmUTC.tm_min, stmUTC.tm_sec);
	return (int)(iLocal - iUTC);
}
// UTC转本地时间
XXAPI xtime xrtUTCToLocal(xtime utc)
{
	return utc + xrtTimezoneOffset();
}
// 本地时间转UTC
XXAPI xtime xrtLocalToUTC(xtime local)
{
	return local - xrtTimezoneOffset();
}
// 获取相对时间描述（如"3天前"、"2小时后"）
XXAPI str xrtRelativeTime(xtime iTime, xtime iBaseTime)
{
	int64 iDiff = iTime - iBaseTime;
	bool bFuture = (iDiff >= 0);
	int64 iAbsDiff = llabs(iDiff);
	const char* sSuffix = bFuture ? "后" : "前";
	if ( iAbsDiff < 60 ) {
		return xrtFormat("%lld秒%s", iAbsDiff, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_HOUR ) {
		return xrtFormat("%lld分钟%s", iAbsDiff / 60, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY ) {
		return xrtFormat("%lld小时%s", iAbsDiff / XRT_TIME_HOUR, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY * 30 ) {
		return xrtFormat("%lld天%s", iAbsDiff / XRT_TIME_DAY, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY * 365 ) {
		return xrtFormat("%lld个月%s", iAbsDiff / (XRT_TIME_DAY * 30), sSuffix);
	} else {
		return xrtFormat("%lld年%s", iAbsDiff / (XRT_TIME_DAY * 365), sSuffix);
	}
}
// 字符串转时间（智能解析，支持多种格式）
XXAPI xtime xrtStrToTime(str sTime, size_t iSize)
{
	if ( !sTime ) return 0;
	if ( iSize == 0 ) iSize = strlen((const char*)sTime);
	if ( iSize == 0 ) return 0;
	
	int year = 0, month = 1, day = 1, hour = 0, minute = 0, second = 0;
	const char* p = (const char*)sTime;
	const char* end = (const char*)sTime + iSize;
	
	// 跳过前导非数字
	while ( p < end && (*p < '0' || *p > '9') ) p++;
	if ( p >= end ) return 0;
	
	// 解析第一个数字序列
	int64 num1 = 0;
	int len1 = 0;
	while ( p < end && *p >= '0' && *p <= '9' ) {
		num1 = num1 * 10 + (*p - '0');
		len1++; p++;
	}
	
	if ( len1 == 8 ) {
		// YYYYMMDD
		year = (int)(num1 / 10000);
		month = (int)((num1 / 100) % 100);
		day = (int)(num1 % 100);
	} else if ( len1 == 14 ) {
		// YYYYMMDDHHMMSS
		year = (int)(num1 / 10000000000LL);
		month = (int)((num1 / 100000000LL) % 100);
		day = (int)((num1 / 1000000LL) % 100);
		hour = (int)((num1 / 10000LL) % 100);
		minute = (int)((num1 / 100LL) % 100);
		second = (int)(num1 % 100);
	} else if ( len1 == 4 ) {
		year = num1;
		// 跳过分隔符
		while ( p < end && (*p < '0' || *p > '9') ) p++;
		if ( p < end ) {
			int num2 = 0;
			while ( p < end && *p >= '0' && *p <= '9' ) {
				num2 = num2 * 10 + (*p - '0'); p++;
			}
			month = num2;
			while ( p < end && (*p < '0' || *p > '9') ) p++;
			if ( p < end ) {
				int num3 = 0;
				while ( p < end && *p >= '0' && *p <= '9' ) {
					num3 = num3 * 10 + (*p - '0'); p++;
				}
				day = num3;
				// 继续解析时间
				while ( p < end && (*p < '0' || *p > '9') ) p++;
				if ( p < end ) {
					int num4 = 0;
					while ( p < end && *p >= '0' && *p <= '9' ) {
						num4 = num4 * 10 + (*p - '0'); p++;
					}
					hour = num4;
					while ( p < end && (*p < '0' || *p > '9') ) p++;
					if ( p < end ) {
						int num5 = 0;
						while ( p < end && *p >= '0' && *p <= '9' ) {
							num5 = num5 * 10 + (*p - '0'); p++;
						}
						minute = num5;
						while ( p < end && (*p < '0' || *p > '9') ) p++;
						if ( p < end ) {
							int num6 = 0;
							while ( p < end && *p >= '0' && *p <= '9' ) {
								num6 = num6 * 10 + (*p - '0'); p++;
							}
							second = num6;
						}
					}
				}
			}
		}
	} else if ( len1 <= 2 ) {
		// 可能是 HH:MM:SS 或 MM-DD
		char sep = (p < end) ? *p : 0;
		if ( sep == ':' ) {
			hour = num1;
			p++;
			if ( p < end ) {
				int num2 = 0;
				while ( p < end && *p >= '0' && *p <= '9' ) {
					num2 = num2 * 10 + (*p - '0'); p++;
				}
				minute = num2;
				if ( p < end && *p == ':' ) {
					p++;
					int num3 = 0;
					while ( p < end && *p >= '0' && *p <= '9' ) {
						num3 = num3 * 10 + (*p - '0'); p++;
					}
					second = num3;
				}
			}
			year = 0; month = 1; day = 1;
		}
	}
	
	return xrtDateTimeSerial(year, month, day, hour, minute, second);
}
// ============================================================================
// 时间格式化与解析 (xrtTimeFormat / xrtTimeParse)
// ============================================================================
// 英文月份表
static const char* _xrtMonthShort[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* _xrtMonthFull[]  = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
// 英文星期表
static const char* _xrtWeekShort[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* _xrtWeekFull[]  = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
// 格式单元类型
enum _XRT_FMT_TYPE {
	_XRT_FMT_LITERAL = 0,   // 固定文本
	_XRT_FMT_YEAR4,         // yyyy
	_XRT_FMT_YEAR2,         // yy
	_XRT_FMT_MONTH2,        // mm (月份，补0)
	_XRT_FMT_MONTH1,        // m  (月份，不补0)
	_XRT_FMT_MONTH_SHORT,   // mmm (Jan)
	_XRT_FMT_MONTH_FULL,    // mmmm (January)
	_XRT_FMT_DAY2,          // dd
	_XRT_FMT_DAY1,          // d
	_XRT_FMT_HOUR24_2,      // hh (24小时制)
	_XRT_FMT_HOUR24_1,      // h
	_XRT_FMT_HOUR12_2,      // HH (12小时制)
	_XRT_FMT_HOUR12_1,      // H
	_XRT_FMT_MINUTE2,       // nn 或 h后的mm
	_XRT_FMT_MINUTE1,       // n
	_XRT_FMT_SECOND2,       // ss
	_XRT_FMT_SECOND1,       // s
	_XRT_FMT_AMPM_LOWER,    // ap
	_XRT_FMT_AMPM_UPPER,    // AP
	_XRT_FMT_WEEKDAY_NUM,   // w
	_XRT_FMT_WEEKDAY_SHORT, // ww
	_XRT_FMT_WEEKDAY_FULL,  // www
	_XRT_FMT_QUARTER,       // q
	_XRT_FMT_SKIP_ANY,      // * (跳过任意非数字)
	_XRT_FMT_SKIP_ONE_PLUS, // . (跳过至少1个非数字)
	_XRT_FMT_SKIP_CHAR,     // ? (跳过1个字符)
	_XRT_FMT_SKIP_SPACE     // 空格 (跳过空白)
};
// 格式单元结构
typedef struct {
	int type;             // 单元类型
	char text[64];        // 固定文本内容
	int textLen;          // 文本长度
} _XrtFmtCell;
// 格式解析结果
typedef struct {
	_XrtFmtCell cells[64]; // 最多64个单元
	int count;             // 单元数量
} _XrtFmtResult;
// 辅助：比较字符（不区分大小写）
static inline int _xrtCharLower(int c) {
	return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}
// 辅助：不区分大小写比较字符串
static inline int _xrtStrNCmpI(const char* s1, const char* s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		int c1 = _xrtCharLower((unsigned char)s1[i]);
		int c2 = _xrtCharLower((unsigned char)s2[i]);
		if (c1 != c2) return c1 - c2;
		if (c1 == 0) return 0;
	}
	return 0;
}
// 辅助：写入2位数字（补前导0）
static inline void _xrtWrite2Digit(char* buf, int val) {
	buf[0] = '0' + (val / 10);
	buf[1] = '0' + (val % 10);
}
// 辅助：解析数字（返回解析位数，val存储结果）
static inline int _xrtParseDigits(const char* s, const char* end, int minLen, int maxLen, int* val) {
	if (!s || s >= end) return 0;
	int len = 0;
	int v = 0;
	while (s + len < end && len < maxLen && s[len] >= '0' && s[len] <= '9') {
		v = v * 10 + (s[len] - '0');
		len++;
	}
	if (len < minLen) return 0;
	if (len == maxLen && s + len < end && s[len] >= '0' && s[len] <= '9') {
		return 0;
	}
	*val = v;
	return len;
}
// 辅助：匹配英文月份（返回月份1-12，0表示失败）
static inline int _xrtMatchMonth(const char* s, const char* end, int* consumed) {
	if (!s || s >= end) return 0;
	size_t remain = end - s;
	for (int i = 0; i < 12; i++) {
		size_t len = strlen(_xrtMonthFull[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtMonthFull[i], len) == 0) {
			*consumed = (int)len;
			return i + 1;
		}
	}
	for (int i = 0; i < 12; i++) {
		size_t len = strlen(_xrtMonthShort[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtMonthShort[i], len) == 0) {
			*consumed = (int)len;
			return i + 1;
		}
	}
	return 0;
}
// 辅助：匹配英文星期（返回星期0-6，-1表示失败）
static inline int _xrtMatchWeekday(const char* s, const char* end, int* consumed) {
	if (!s || s >= end) return -1;
	size_t remain = end - s;
	for (int i = 0; i < 7; i++) {
		size_t len = strlen(_xrtWeekFull[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtWeekFull[i], len) == 0) {
			*consumed = (int)len;
			return i;
		}
	}
	for (int i = 0; i < 7; i++) {
		size_t len = strlen(_xrtWeekShort[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtWeekShort[i], len) == 0) {
			*consumed = (int)len;
			return i;
		}
	}
	return -1;
}
// 解析格式字符串
static void _xrtParseFormat(const char* fmt, _XrtFmtResult* result, int forParse) {
	result->count = 0;
	if (!fmt) return;
	if (forParse) {
		result->cells[result->count].type = _XRT_FMT_SKIP_ANY;
		result->cells[result->count].textLen = 0;
		result->count++;
	}
	const char* p = fmt;
	int afterHour = 0;
	while (*p && result->count < 63) {
		_XrtFmtCell* cell = &result->cells[result->count];
		cell->textLen = 0;
		if (p[0] == 'y') {
			int cnt = 0;
			while (p[cnt] == 'y' && cnt < 4) cnt++;
			if (cnt >= 4) { cell->type = _XRT_FMT_YEAR4; p += 4; }
			else { cell->type = _XRT_FMT_YEAR2; p += cnt; }
			afterHour = 0;
			result->count++;
		} else if (p[0] == 'm') {
			int cnt = 0;
			while (p[cnt] == 'm' && cnt < 4) cnt++;
			if (cnt >= 4) { cell->type = _XRT_FMT_MONTH_FULL; p += 4; afterHour = 0; }
			else if (cnt == 3) { cell->type = _XRT_FMT_MONTH_SHORT; p += 3; afterHour = 0; }
			else if (cnt == 2) { cell->type = afterHour ? _XRT_FMT_MINUTE2 : _XRT_FMT_MONTH2; p += 2; }
			else { cell->type = _XRT_FMT_MONTH1; p += 1; afterHour = 0; }
			result->count++;
		} else if (p[0] == 'd') {
			int cnt = 0;
			while (p[cnt] == 'd' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_DAY2 : _XRT_FMT_DAY1;
			p += cnt; afterHour = 0;
			result->count++;
		} else if (p[0] == 'h') {
			int cnt = 0;
			while (p[cnt] == 'h' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_HOUR24_2 : _XRT_FMT_HOUR24_1;
			p += cnt; afterHour = 1;
			result->count++;
		} else if (p[0] == 'H') {
			int cnt = 0;
			while (p[cnt] == 'H' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_HOUR12_2 : _XRT_FMT_HOUR12_1;
			p += cnt; afterHour = 1;
			result->count++;
		} else if (p[0] == 'n') {
			int cnt = 0;
			while (p[cnt] == 'n' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_MINUTE2 : _XRT_FMT_MINUTE1;
			p += cnt;
			result->count++;
		} else if (p[0] == 's') {
			int cnt = 0;
			while (p[cnt] == 's' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_SECOND2 : _XRT_FMT_SECOND1;
			p += cnt;
			result->count++;
		} else if (p[0] == 'a' && p[1] == 'p') {
			cell->type = _XRT_FMT_AMPM_LOWER; p += 2;
			result->count++;
		} else if (p[0] == 'A' && p[1] == 'P') {
			cell->type = _XRT_FMT_AMPM_UPPER; p += 2;
			result->count++;
		} else if (p[0] == 'w') {
			int cnt = 0;
			while (p[cnt] == 'w' && cnt < 3) cnt++;
			if (cnt >= 3) { cell->type = _XRT_FMT_WEEKDAY_FULL; p += 3; }
			else if (cnt == 2) { cell->type = _XRT_FMT_WEEKDAY_SHORT; p += 2; }
			else { cell->type = _XRT_FMT_WEEKDAY_NUM; p += 1; }
			result->count++;
		} else if (p[0] == 'q') {
			cell->type = _XRT_FMT_QUARTER; p += 1;
			result->count++;
		} else if (forParse && p[0] == '*') {
			cell->type = _XRT_FMT_SKIP_ANY; p += 1;
			result->count++;
		} else if (forParse && p[0] == '.') {
			cell->type = _XRT_FMT_SKIP_ONE_PLUS; p += 1;
			result->count++;
		} else if (forParse && p[0] == '?') {
			cell->type = _XRT_FMT_SKIP_CHAR; p += 1;
			result->count++;
		} else if (forParse && (p[0] == ' ' || p[0] == '\t')) {
			cell->type = _XRT_FMT_SKIP_SPACE;
			while (*p == ' ' || *p == '\t') p++;
			result->count++;
		} else {
			cell->type = _XRT_FMT_LITERAL;
			int len = 0;
			while (p[len] && len < 63) {
				char c = p[len];
				if (c == 'y' || c == 'm' || c == 'd' || c == 'h' || c == 'H' ||
				    c == 'n' || c == 's' || c == 'w' || c == 'q' ||
				    (c == 'a' && p[len+1] == 'p') || (c == 'A' && p[len+1] == 'P')) break;
				if (forParse && (c == '*' || c == '.' || c == '?' || c == ' ' || c == '\t')) break;
				cell->text[len] = c; len++;
			}
			if (len > 0) {
				cell->text[len] = '\0'; cell->textLen = len; p += len;
				result->count++;
			} else { p++; }
		}
	}
}
// 时间格式化为字符串
XXAPI str xrtTimeFormat(xtime iTime, const void* sFormat)
{
	if (!sFormat) return NULL;
	_XrtFmtResult fmt;
	_xrtParseFormat(sFormat, &fmt, 0);
	if (fmt.count == 0) return NULL;
	int64 iYear; int iMonth, iDay, iHour, iMinute, iSecond, iWeekday;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, NULL);
	int iQuarter = ((iMonth - 1) / 3) + 1;
	int iHour12 = iHour % 12; if (iHour12 == 0) iHour12 = 12;
	int isPM = (iHour >= 12);
	size_t bufSize = 256;
	char* buf = (char*)xrtMalloc(bufSize);
	if (!buf) return NULL;
	size_t pos = 0;
	for (int i = 0; i < fmt.count && pos < bufSize - 32; i++) {
		_XrtFmtCell* cell = &fmt.cells[i];
		switch (cell->type) {
			case _XRT_FMT_LITERAL:
				if (pos + cell->textLen < bufSize) { memcpy(buf + pos, cell->text, cell->textLen); pos += cell->textLen; }
				break;
			case _XRT_FMT_YEAR4: pos += snprintf(buf + pos, bufSize - pos, "%04lld", iYear); break;
			case _XRT_FMT_YEAR2: pos += snprintf(buf + pos, bufSize - pos, "%02d", (int)(iYear % 100)); break;
			case _XRT_FMT_MONTH2: _xrtWrite2Digit(buf + pos, iMonth); pos += 2; break;
			case _XRT_FMT_MONTH1: pos += snprintf(buf + pos, bufSize - pos, "%d", iMonth); break;
			case _XRT_FMT_MONTH_SHORT:
				if (iMonth >= 1 && iMonth <= 12) { size_t len = strlen((const char*)_xrtMonthShort[iMonth - 1]); memcpy(buf + pos, _xrtMonthShort[iMonth - 1], len); pos += len; }
				break;
			case _XRT_FMT_MONTH_FULL:
				if (iMonth >= 1 && iMonth <= 12) { size_t len = strlen((const char*)_xrtMonthFull[iMonth - 1]); memcpy(buf + pos, _xrtMonthFull[iMonth - 1], len); pos += len; }
				break;
			case _XRT_FMT_DAY2: _xrtWrite2Digit(buf + pos, iDay); pos += 2; break;
			case _XRT_FMT_DAY1: pos += snprintf(buf + pos, bufSize - pos, "%d", iDay); break;
			case _XRT_FMT_HOUR24_2: _xrtWrite2Digit(buf + pos, iHour); pos += 2; break;
			case _XRT_FMT_HOUR24_1: pos += snprintf(buf + pos, bufSize - pos, "%d", iHour); break;
			case _XRT_FMT_HOUR12_2: _xrtWrite2Digit(buf + pos, iHour12); pos += 2; break;
			case _XRT_FMT_HOUR12_1: pos += snprintf(buf + pos, bufSize - pos, "%d", iHour12); break;
			case _XRT_FMT_MINUTE2: _xrtWrite2Digit(buf + pos, iMinute); pos += 2; break;
			case _XRT_FMT_MINUTE1: pos += snprintf(buf + pos, bufSize - pos, "%d", iMinute); break;
			case _XRT_FMT_SECOND2: _xrtWrite2Digit(buf + pos, iSecond); pos += 2; break;
			case _XRT_FMT_SECOND1: pos += snprintf(buf + pos, bufSize - pos, "%d", iSecond); break;
			case _XRT_FMT_AMPM_LOWER: buf[pos++] = isPM ? 'p' : 'a'; buf[pos++] = 'm'; break;
			case _XRT_FMT_AMPM_UPPER: buf[pos++] = isPM ? 'P' : 'A'; buf[pos++] = 'M'; break;
			case _XRT_FMT_WEEKDAY_NUM: buf[pos++] = '0' + iWeekday; break;
			case _XRT_FMT_WEEKDAY_SHORT:
				if (iWeekday >= 0 && iWeekday <= 6) { size_t len = strlen((const char*)_xrtWeekShort[iWeekday]); memcpy(buf + pos, _xrtWeekShort[iWeekday], len); pos += len; }
				break;
			case _XRT_FMT_WEEKDAY_FULL:
				if (iWeekday >= 0 && iWeekday <= 6) { size_t len = strlen((const char*)_xrtWeekFull[iWeekday]); memcpy(buf + pos, _xrtWeekFull[iWeekday], len); pos += len; }
				break;
			case _XRT_FMT_QUARTER: buf[pos++] = '0' + iQuarter; break;
			default: break;
		}
	}
	buf[pos] = '\0';
	return (str)buf;
}
// 字符串解析为时间
XXAPI xtime xrtTimeParse(str sTime, str sFormat)
{
	if (!sTime || !sFormat) return 0;
	_XrtFmtResult fmt;
	_xrtParseFormat((const char*)sFormat, &fmt, 1);
	if (fmt.count == 0) return 0;
	const char* s = (const char*)sTime;
	const char* end = (const char*)sTime + strlen((const char*)sTime);
	int year = 0, month = 1, day = 1, hour = 0, minute = 0, second = 0;
	int isPM = -1, is12Hour = 0;
	for (int i = 0; i < fmt.count && s < end; i++) {
		_XrtFmtCell* cell = &fmt.cells[i];
		int val, consumed;
		switch (cell->type) {
			case _XRT_FMT_LITERAL:
				if ((size_t)(end - s) >= (size_t)cell->textLen && memcmp(s, cell->text, cell->textLen) == 0) s += cell->textLen;
				else return 0;
				break;
			case _XRT_FMT_YEAR4:
				consumed = _xrtParseDigits(s, end, 4, 4, &val);
				if (consumed > 0) { year = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_YEAR2:
				consumed = _xrtParseDigits(s, end, 2, 2, &val);
				if (consumed > 0) { year = (val < 70) ? (2000 + val) : (1900 + val); s += consumed; } else return 0;
				break;
			case _XRT_FMT_MONTH2: case _XRT_FMT_MONTH1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 12) { month = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_MONTH_SHORT: case _XRT_FMT_MONTH_FULL:
				val = _xrtMatchMonth(s, end, &consumed);
				if (val > 0) { month = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_DAY2: case _XRT_FMT_DAY1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 31) { day = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_HOUR24_2: case _XRT_FMT_HOUR24_1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 23) { hour = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_HOUR12_2: case _XRT_FMT_HOUR12_1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 12) { hour = val; is12Hour = 1; s += consumed; } else return 0;
				break;
			case _XRT_FMT_MINUTE2: case _XRT_FMT_MINUTE1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 59) { minute = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_SECOND2: case _XRT_FMT_SECOND1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 59) { second = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_AMPM_LOWER: case _XRT_FMT_AMPM_UPPER:
				if (end - s >= 2) {
					char c0 = _xrtCharLower(s[0]), c1 = _xrtCharLower(s[1]);
					if (c0 == 'a' && c1 == 'm') { isPM = 0; s += 2; }
					else if (c0 == 'p' && c1 == 'm') { isPM = 1; s += 2; }
					else return 0;
				} else return 0;
				break;
			case _XRT_FMT_WEEKDAY_NUM:
				consumed = _xrtParseDigits(s, end, 1, 1, &val);
				if (consumed > 0 && val >= 0 && val <= 6) s += consumed; else return 0;
				break;
			case _XRT_FMT_WEEKDAY_SHORT: case _XRT_FMT_WEEKDAY_FULL:
				val = _xrtMatchWeekday(s, end, &consumed);
				if (val >= 0) s += consumed; else return 0;
				break;
			case _XRT_FMT_QUARTER:
				consumed = _xrtParseDigits(s, end, 1, 1, &val);
				if (consumed > 0 && val >= 1 && val <= 4) s += consumed; else return 0;
				break;
			case _XRT_FMT_SKIP_ANY:
				// 智能跳过：根据下一个单元类型决定如何跳过
				if (i + 1 < fmt.count) {
					_XrtFmtCell* next = &fmt.cells[i + 1];
					if (next->type == _XRT_FMT_LITERAL) {
						// 下一个是字面量，搜索字面量位置
						const char* found = NULL;
						for (const char* p = s; p + next->textLen <= end; p++) {
							if (memcmp(p, next->text, next->textLen) == 0) {
								found = p; break;
							}
						}
						if (found) s = found;
					} else if (next->type == _XRT_FMT_MONTH_SHORT || next->type == _XRT_FMT_MONTH_FULL) {
						// 下一个是英文月份，搜索月份位置
						while (s < end) {
							if (_xrtMatchMonth(s, end, &consumed) > 0) break;
							s++;
						}
					} else if (next->type == _XRT_FMT_WEEKDAY_SHORT || next->type == _XRT_FMT_WEEKDAY_FULL) {
						// 下一个是英文星期，搜索星期位置
						while (s < end) {
							if (_xrtMatchWeekday(s, end, &consumed) >= 0) break;
							s++;
						}
					} else if (next->type == _XRT_FMT_AMPM_LOWER || next->type == _XRT_FMT_AMPM_UPPER) {
						// 下一个是AM/PM，搜索位置
						while (s + 1 < end) {
							char c0 = _xrtCharLower(s[0]), c1 = _xrtCharLower(s[1]);
							if ((c0 == 'a' || c0 == 'p') && c1 == 'm') break;
							s++;
						}
					} else {
						// 默认：跳过非数字
						while (s < end && (*s < '0' || *s > '9')) s++;
					}
				} else {
					while (s < end && (*s < '0' || *s > '9')) s++;
				}
				break;
			case _XRT_FMT_SKIP_ONE_PLUS:
				if (s < end && (*s < '0' || *s > '9')) { s++; while (s < end && (*s < '0' || *s > '9')) s++; }
				else return 0;
				break;
			case _XRT_FMT_SKIP_CHAR:
				if (s < end) { if ((unsigned char)*s >= 0x80) s += (s + 1 < end) ? 2 : 1; else s++; }
				else return 0;
				break;
			case _XRT_FMT_SKIP_SPACE:
				while (s < end && (*s == ' ' || *s == '\t')) s++;
				break;
			default: break;
		}
	}
	if (is12Hour && isPM >= 0) {
		if (isPM == 1 && hour != 12) hour += 12;
		else if (isPM == 0 && hour == 12) hour = 0;
	}
	if (year == 0) { xtime now = xrtNow(); year = (int)xrtYear(now); }
	return xrtDateTimeSerial(year, month, day, hour, minute, second);
}
// 时间约等于
XXAPI bool xrtTimeApprox(xtime a, xtime b)
{
	if ( a == b ) { return TRUE; }
	
	xtime diff = (a > b) ? (a - b) : (b - a);
	return (diff <= xCore.iApproxTimeTol);
}
#endif
#ifndef XRT_NO_FILE

// ========================================
// File: D:/git/xrt/lib/file.h
// ========================================


// 错误描述定义
static const str sErrorFile_Open = (str)"Failed to open file !";
static const str sErrorFile_OpenDir = (str)"Failed to open dir !";
static const str sErrorFile_Handle = (str)"Incorrect file handle !";
static const str sErrorFile_BOM = (str)"Incorrect BOM data !";
static const str sErrorFile_Seek = (str)"Incorrect seek method !";
static const str sErrorFile_Read = (str)"File read failure !";
static const str sErrorFile_Write = (str)"File write failure !";
// 打开文件
XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, bReadOnly ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
		xfile objFile = xrtMalloc(sizeof(xfile_struct));
		if ( objFile == NULL ) {
			CloseHandle(hFile);
			return NULL;
		}
		DWORD iRetSize;
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		objFile->obj = hFile;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		objFile->ReadOnly = bReadOnly;
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		if ( iCharset == XRT_CP_AUTO ) {
			// 自动识别文件编码
			if ( stuSize.QuadPart == 0 ) {
				// 空文件默认使用 utf8 编码 ( 无 BOM )
				objFile->Charset = XRT_CP_UTF8;
			} else {
				// 最多读取 64KB 数据做编码推测（数据越多推测准确性越高，数据越少推测速度越快）
				size_t iReadSize = stuSize.QuadPart > 65536 ? 65536 : stuSize.QuadPart;
				str sText = xrtMalloc(iReadSize);
				if ( sText == NULL ) {
					CloseHandle(hFile);
					xrtFree(objFile);
					return NULL;
				}
				ReadFile(hFile, (ptr)sText, iReadSize, &iRetSize, NULL);
				objFile->Charset = xrtDetectCharset(sText, iReadSize, TRUE);
				xrtFree(sText);
				// 重置文件指针到开头
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			}
		} else {
			// 手动指定编码时，检查 BOM 是否正确或提前写入 BOM
			if ( iCharset == XRT_CP_BINARY ) {
				// BINARY 模式不处理 BOM，确保文件指针在开头
				objFile->BOM = 0;
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			} else if ( (iCharset > 0) && ((iCharset & XRT_CP_BOM) == XRT_CP_BOM) ) {
				int bErrorBOM = FALSE;
				if ( stuSize.QuadPart == 0 ) {
					// 0 字节文件自动添加 BOM ( 如果是只读模式直接报错 )
					if ( bReadOnly ) {
						bErrorBOM = TRUE;
					} else {
						if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
							uint8 sBOM[3] = { 0xEF, 0xBB, 0xBF };
							WriteFile(hFile, (ptr)sBOM, 3, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
							unsigned short iBOM = 0xFEFF;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
							unsigned short iBOM = 0xFFFE;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
							unsigned int iBOM = 0x0000FEFF;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
							unsigned int iBOM = 0xFFFE0000;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						}
					}
				} else {
					// 固定编码的文件检测并跳过 BOM ( BOM 与检测结果不符直接报错 )
					uint8 sBOM[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
					ReadFile(hFile, (ptr)sBOM, 4, &iRetSize, NULL);
					if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
						if ( (sBOM[0] != 0xEF) || (sBOM[1] != 0xBB) || (sBOM[2] != 0xBF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
						if ( (sBOM[0] != 0xFE) || (sBOM[1] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) || (sBOM[2] != 0x00) || (sBOM[3] != 0x00) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
						if ( (sBOM[0] != 0x00) || (sBOM[1] != 0x00) || (sBOM[2] != 0xFE) || (sBOM[3] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					}
				}
				if ( bErrorBOM ) {
					xrtSetError(sErrorFile_BOM, FALSE);
					CloseHandle(hFile);
					xrtFree(objFile);
					return NULL;
				}
			}
		}
		// 计算 BOM 长度 ( 处理到这个步骤可以确保 BOM 信息是正确的 )
		if ( (objFile->Charset > 0) && ((objFile->Charset & XRT_CP_BOM) == XRT_CP_BOM) ) {
			if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
				objFile->BOM = 3;
				objFile->Charset = XRT_CP_UTF8;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16_BE;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32_BE;
			}
		}
		// 游标跳过 BOM (BINARY 模式已经在前面重置过文件指针)
		if (objFile->Charset != XRT_CP_BINARY) {
			SetFilePointer(hFile, objFile->BOM, NULL, FILE_BEGIN);
		}
		return objFile;
	#else
		// 其他平台方案
		int fd = open(sPath, bReadOnly ? O_RDONLY : (O_RDWR | O_CREAT), 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
		xfile objFile = xrtMalloc(sizeof(xfile_struct));
		if ( objFile == NULL ) {
			close(fd);
			return NULL;
		}
		size_t iRetSize;
		struct stat fileStat;
		fstat(fd, &fileStat);
		objFile->idx = fd;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		objFile->ReadOnly = bReadOnly;
		lseek(fd, 0, SEEK_SET);
		if ( iCharset == XRT_CP_AUTO ) {
			// 自动识别文件编码
			if ( fileStat.st_size == 0 ) {
				// 空文件默认使用 utf8 编码 ( 无 BOM )
				objFile->Charset = XRT_CP_UTF8;
			} else {
				// 最多读取 64KB 数据做编码推测（数据越多推测准确性越高，数据越少推测速度越快）
				size_t iReadSize = fileStat.st_size > 65536 ? 65536 : fileStat.st_size;
				str sText = xrtMalloc(iReadSize);
				if ( sText == NULL ) {
					close(fd);
					xrtFree(objFile);
					return NULL;
				}
				read(fd, sText, iReadSize);
				objFile->Charset = xrtDetectCharset(sText, iReadSize, TRUE);
				xrtFree(sText);
				// 重置文件指针到开头
				lseek(fd, 0, SEEK_SET);
			}
		} else {
			// 手动指定编码时，检查 BOM 是否正确或提前写入 BOM
			if ( iCharset == XRT_CP_BINARY ) {
				// BINARY 模式不处理 BOM，确保文件指针在开头
				objFile->BOM = 0;
				lseek(fd, 0, SEEK_SET);
			} else if ( (iCharset > 0) && ((iCharset & XRT_CP_BOM) == XRT_CP_BOM) ) {
				int bErrorBOM = FALSE;
				if ( fileStat.st_size == 0 ) {
					// 0 字节文件自动添加 BOM ( 如果是只读模式直接报错 )
					if ( bReadOnly ) {
						bErrorBOM = TRUE;
					} else {
						if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
							uint8 sBOM[3] = { 0xEF, 0xBB, 0xBF };
							write(fd, (ptr)sBOM, 3);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
							unsigned short iBOM = 0xFEFF;
							write(fd, (ptr)&iBOM, 2);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
							unsigned short iBOM = 0xFFFE;
							write(fd, (ptr)&iBOM, 2);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
							unsigned int iBOM = 0x0000FEFF;
							write(fd, (ptr)&iBOM, 4);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
							unsigned int iBOM = 0xFFFE0000;
							write(fd, (ptr)&iBOM, 4);
						}
					}
				} else {
					// 固定编码的文件检测并跳过 BOM ( BOM 与检测结果不符直接报错 )
					uint8 sBOM[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
					read(fd, (ptr)sBOM, 4);
					if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
						if ( (sBOM[0] != 0xEF) || (sBOM[1] != 0xBB) || (sBOM[2] != 0xBF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
						if ( (sBOM[0] != 0xFE) || (sBOM[1] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) || (sBOM[2] != 0x00) || (sBOM[3] != 0x00) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
						if ( (sBOM[0] != 0x00) || (sBOM[1] != 0x00) || (sBOM[2] != 0xFE) || (sBOM[3] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					}
				}
				if ( bErrorBOM ) {
					xrtSetError(sErrorFile_BOM, FALSE);
					close(fd);
					xrtFree(objFile);
					return NULL;
				}
			}
		}
		// 计算 BOM 长度 ( 处理到这个步骤可以确保 BOM 信息是正确的 )
		if ( (objFile->Charset > 0) && ((objFile->Charset & XRT_CP_BOM) == XRT_CP_BOM) ) {
			if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
				objFile->BOM = 3;
				objFile->Charset = XRT_CP_UTF8;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16_BE;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32_BE;
			}
		}
		// 游标跳过 BOM (BINARY 模式已经在前面重置过文件指针)
		if (objFile->Charset != XRT_CP_BINARY) {
			lseek(fd, objFile->BOM, SEEK_SET);
		}
		return objFile;
	#endif
}
// 关闭文件
XXAPI void xrtClose(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile ) {
			if ( objFile->obj != INVALID_HANDLE_VALUE ) {
				if ( !objFile->ReadOnly ) {
					FlushFileBuffers(objFile->obj);
				}
				CloseHandle(objFile->obj);
				objFile->obj = NULL;
			}
			xrtFree(objFile);
		}
	#else
		// 其他平台方案
		if ( objFile ) {
			if ( objFile->idx != -1 ) {
				if ( !objFile->ReadOnly ) {
					fsync(objFile->idx);
				}
				close(objFile->idx);
				objFile->idx = 0;
			}
			xrtFree(objFile);
		}
	#endif
}
// 设置游标位置
XXAPI size_t xrtSeek(xfile objFile, int64 iOffset, int iMoveMethod)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER iOffsetStu;
			LARGE_INTEGER iPosStu;
			iOffsetStu.QuadPart = iOffset;
			if ( iMoveMethod == XRT_SEEK_SET ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_BEGIN) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else if ( iMoveMethod == XRT_SEEK_CUR ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_CURRENT) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else if ( iMoveMethod == XRT_SEEK_END ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_END) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else {
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iMoveMethod == XRT_SEEK_SET ) {
				return lseek(objFile->idx, iOffset, SEEK_SET);
			} else if ( iMoveMethod == XRT_SEEK_CUR ) {
				return lseek(objFile->idx, iOffset, SEEK_CUR);
			} else if ( iMoveMethod == XRT_SEEK_END ) {
				return lseek(objFile->idx, iOffset, SEEK_END);
			} else {
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 获取游标位置
XXAPI size_t xrtTell(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER iPos_stu;
			iPos_stu.QuadPart = 0;
			SetFilePointerEx(objFile->obj, iPos_stu, &iPos_stu, FILE_CURRENT);
			return iPos_stu.QuadPart;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			return lseek(objFile->idx, 0, SEEK_CUR);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 获取文件末尾位置 ( 获取一打开文件的动态大小 )
XXAPI size_t xrtGetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER stuSize;
			GetFileSizeEx(objFile->obj, &stuSize);
			return stuSize.QuadPart;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			struct stat fileStat;
			fstat(objFile->idx, &fileStat);
			return fileStat.st_size;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 是否已经读取到文件末尾
XXAPI bool xrtEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			size_t iPos = xrtTell(objFile);
			size_t iEnd = xrtGetEOF(objFile);
			return (iPos >= iEnd);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return TRUE;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			size_t iPos = xrtTell(objFile);
			size_t iEnd = xrtGetEOF(objFile);
			return (iPos >= iEnd);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return TRUE;
		}
	#endif
}
// 设置文件末尾
XXAPI bool xrtSetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			SetEndOfFile(objFile->obj);
			return TRUE;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return FALSE;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			size_t iPos = xrtTell(objFile);
			int iRet = ftruncate(objFile->idx, iPos);
			if ( iRet == 0 ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return FALSE;
		}
	#endif
}
// 从已打开的文件读取数据 ( iSize 为要读取的字节数，需要使用 xrtFree 释放内存 )
XXAPI str xrtRead(xfile objFile, size_t iSize, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRet] = 0;
				sBuff[iRet + 1] = 0;
				sBuff[iRet + 2] = 0;
				sBuff[iRet + 3] = 0;
				// 转换编码 (将读取到的数据转换为 utf-8 编码)
				if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
					int iCharSize = xrtGetCharSize(objFile->Charset);
					size_t iTextSize = 0;
					str sRet = xrtConvCharset(sBuff, iRet / iCharSize, objFile->Charset, XRT_CP_UTF8, &iTextSize);
					xrtFree(sBuff);
					if ( iRetSize ) { *iRetSize = iTextSize; }
					return sRet;
				} else {
					if ( iRetSize ) { *iRetSize = iRet; }
					return sBuff;
				}
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			size_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet > 0 ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRet] = 0;
				sBuff[iRet + 1] = 0;
				sBuff[iRet + 2] = 0;
				sBuff[iRet + 3] = 0;
				// 转换编码 (将读取到的数据转换为 utf-8 编码)
				if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
					int iCharSize = xrtGetCharSize(objFile->Charset);
					size_t iTextSize = 0;
					str sRet = xrtConvCharset(sBuff, iRet / iCharSize, objFile->Charset, XRT_CP_UTF8, &iTextSize);
					xrtFree(sBuff);
					if ( iRetSize ) { *iRetSize = iTextSize; }
					return sRet;
				} else {
					if ( iRetSize ) { *iRetSize = iRet; }
					return sBuff;
				}
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#endif
}
// 向已打开的文件写入数据 ( iSize 为要写入的字节数 )
XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( sText == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
			if ( iSize == 0 ) { return 0; }
			DWORD iRetSize;
			if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
				// 需要转换为目标文件的编码再写入
				size_t iTextSize = 0;
				str sBuff = xrtConvCharset(sText, iSize, XRT_CP_UTF8, objFile->Charset, &iTextSize);
				int iCharSize = xrtGetCharSize(objFile->Charset);
				int iRet = WriteFile(objFile->obj, sBuff, iTextSize * iCharSize, &iRetSize, NULL);
				xrtFree(sBuff);
				if ( iRet ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			} else {
				// 二进制或 utf-8 编码可以直接写入
				if ( WriteFile(objFile->obj, sText, iSize, &iRetSize, NULL) ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( sText == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
			if ( iSize == 0 ) { return 0; }
			if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
				// 需要转换为目标文件的编码再写入
				size_t iConvSize = 0;
				str sBuff = xrtConvCharset(sText, iSize, XRT_CP_UTF8, objFile->Charset, &iConvSize);
				int iCharSize = xrtGetCharSize(objFile->Charset);
				size_t iRetSize = write(objFile->idx, sBuff, iConvSize * iCharSize);
				xrtFree(sBuff);
				if ( iRetSize > 0 ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			} else {
				// 二进制或 utf-8 编码可以直接写入
				size_t iRetSize = write(objFile->idx, sText, iSize);
				if ( iRetSize > 0 ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 从已打开的文件读取二进制数据到特定位置
XXAPI size_t xrtGetBuffer(xfile objFile, ptr sBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { return 0; }
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				return iRet;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { return 0; }
			// 读取数据
			ssize_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet >= 0 ) {
				return (size_t)iRet;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 从已打开的文件读取二进制数据 ( 需要使用 xrtFree 释放内存 )
XXAPI ptr xrtGet(xfile objFile, size_t iSize, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				if ( iRetSize ) { *iRetSize = iRet; }
				return sBuff;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			size_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet > 0 ) {
				if ( iRetSize ) { *iRetSize = iRet; }
				return sBuff;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#endif
}
// 向已打开的文件写入二进制数据
XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( pBuff == NULL ) { return 0; }
			if ( iSize == 0 ) { return 0; }
			DWORD iRetSize;
			if ( WriteFile(objFile->obj, pBuff, iSize, &iRetSize, NULL) ) {
				return iRetSize;
			} else {
				xrtSetError(sErrorFile_Write, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( pBuff == NULL ) { return 0; }
			if ( iSize == 0 ) { return 0; }
			size_t iRetSize = write(objFile->idx, pBuff, iSize);
			if ( iRetSize > 0 ) {
				return iRetSize;
			} else {
				xrtSetError(sErrorFile_Write, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}
// 向文件追加写入数据
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset)
{
	if ( sText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
	if ( iSize == 0 ) { return 0; }
	xfile objFile = xrtOpen(sPath, FALSE, iCharset);
	if ( objFile ) {
		xrtSeek(objFile, 0, XRT_SEEK_END);
		ulong iRet = xrtWrite(objFile, sText, iSize);
		xrtClose(objFile);
		return iRet;
	}
	return 0;
}
// 写入并覆盖文件内容
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset)
{
	if ( sText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
	if ( iSize == 0 ) { return 0; }
	xfile objFile = xrtOpen(sPath, FALSE, iCharset);
	if ( objFile ) {
		ulong iRet = xrtWrite(objFile, sText, iSize);
		xrtSetEOF(objFile);
		xrtClose(objFile);
		return iRet;
	}
	return 0;
}
// 读取文件的全部内容 ( 需要使用 xrtFree 释放内存 )
XXAPI str xrtFileReadAll(str sPath, int iCharset, size_t* iRetSize)
{
	xfile objFile = xrtOpen(sPath, TRUE, iCharset);
	if ( objFile ) {
		uint64 iSize = xrtGetEOF(objFile) - objFile->BOM;
		if ( iSize > 0 ) {
			size_t iFileSize = 0;
			str sRet = xrtRead(objFile, iSize, &iFileSize);
			if ( iRetSize ) { *iRetSize = iFileSize; }
			xrtClose(objFile);
			return sRet;
		} else {
			xrtClose(objFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	}
	if ( iRetSize ) { *iRetSize = 0; }
	return xCore.sNull;
}
// 写入并覆盖文件内容 ( 二进制 )
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( iSize == 0 ) { return 0; }
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		DWORD iRetSize;
		if ( WriteFile(hFile, pBuff, iSize, &iRetSize, NULL) ) {
			SetEndOfFile(hFile);
			CloseHandle(hFile);
			return iRetSize;
		} else {
			xrtSetError(sErrorFile_Write, FALSE);
			CloseHandle(hFile);
			return 0;
		}
	#else
		// 其他平台方案
		if ( iSize == 0 ) { return 0; }
		int fd = open(sPath, O_RDWR | O_CREAT, 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		lseek(fd, 0, SEEK_SET);
		size_t iRetSize = write(fd, pBuff, iSize);
		if ( iRetSize > 0 ) {
			size_t iPos = lseek(fd, 0, SEEK_CUR);
			ftruncate(fd, iPos);
			close(fd);
			return iRetSize;
		} else {
			xrtSetError(sErrorFile_Write, FALSE);
			close(fd);
			return 0;
		}
	#endif
}
// 读取文件的全部内容 ( 二进制，需要使用 xrtFree 释放内存 )
XXAPI ptr xrtFileGetAll(str sPath, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		if ( stuSize.QuadPart == 0 ) {
			CloseHandle(hFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		str pBuff = xrtMalloc(stuSize.QuadPart + 1);
		if ( pBuff == NULL ) {
			CloseHandle(hFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		DWORD iRet = 0;
		ReadFile(hFile, pBuff, stuSize.QuadPart, &iRet, NULL);
		if ( iRet == 0 ) {
			xrtSetError(sErrorFile_Read, FALSE);
			CloseHandle(hFile);
			xrtFree(pBuff);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		pBuff[iRet] = 0;
		CloseHandle(hFile);
		if ( iRetSize ) { *iRetSize = iRet; }
		return pBuff;
	#else
		// 其他平台方案
		int fd = open(sPath, O_RDONLY);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		lseek(fd, 0, SEEK_SET);
		struct stat fileStat;
		fstat(fd, &fileStat);
		if ( fileStat.st_size == 0 ) {
			close(fd);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		str pBuff = xrtMalloc(fileStat.st_size + 1);
		if ( pBuff == NULL ) {
			close(fd);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		size_t iRet = read(fd, pBuff, fileStat.st_size);
		if ( iRet == 0 ) {
			xrtSetError(sErrorFile_Read, FALSE);
			close(fd);
			xrtFree(pBuff);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		pBuff[iRet] = 0;
		close(fd);
		if ( iRetSize ) { *iRetSize = iRet; }
		return pBuff;
	#endif
}
// 判断路径是否存在
XXAPI bool xrtPathExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		bool bRet = GetFileAttributesW(sPathW) != INVALID_FILE_ATTRIBUTES;
		xrtFree(sPathW);
		return bRet;
	#else
		// 其他平台方案
		if ( access(sPath, F_OK) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}
// 判断文件是否存在
XXAPI bool xrtFileExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			if ( fileStat.st_mode & S_IFDIR ) {
				return FALSE;
			} else if ( fileStat.st_mode & S_IFREG ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	#endif
}
// 判断目录是否存在
XXAPI bool xrtDirExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			if ( fileStat.st_mode & S_IFDIR ) {
				return TRUE;
			} else if ( fileStat.st_mode & S_IFREG ) {
				return FALSE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	#endif
}
// 获取文件长度
XXAPI size_t xrtFileGetSize(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		LARGE_INTEGER iSize;
		GetFileSizeEx(hFile, &iSize);
		CloseHandle(hFile);
		return iSize.QuadPart;
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_size;
		} else {
			return 0;
		}
	#endif
}
// 设置文件长度
XXAPI bool xrtFileSetSize(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		LARGE_INTEGER iPos_stu;
		iPos_stu.QuadPart = iSize;
		SetFilePointerEx(hFile, iPos_stu, NULL, FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		return TRUE;
	#else
		// 其他平台方案
		int fd = open(sPath, O_RDWR | O_CREAT, 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		ftruncate(fd, iSize);
		close(fd);
		return TRUE;
	#endif
}
// 获取文件属性
XXAPI int xrtFileGetAttr(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = GetFileAttributesW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_mode;
		} else {
			return 0;
		}
	#endif
}
// 设置文件属性
XXAPI bool xrtFileSetAttr(str sPath, int iAttr)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = SetFileAttributesW(sPathW, iAttr);
		xrtFree(sPathW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int iRet = chmod(sPath, iAttr);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}
// 获取文件最后一次访问时间
XXAPI int64 xrtFileGetAccessTime(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		struct _stat64 fileStat;
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet;
		if ( sPathW == NULL ) {
			return 0;
		}
		iRet = _wstat64((const wchar_t*)sPathW, &fileStat);
		xrtFree(sPathW);
		if ( iRet == 0 ) {
			return fileStat.st_atime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_atime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#endif
}
// 获取文件修改时间
XXAPI int64 xrtFileGetChangeTime(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		struct _stat64 fileStat;
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet;
		if ( sPathW == NULL ) {
			return 0;
		}
		iRet = _wstat64((const wchar_t*)sPathW, &fileStat);
		xrtFree(sPathW);
		if ( iRet == 0 ) {
			return fileStat.st_mtime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_mtime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#endif
}
// 复制文件
XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		u16str sDstW = xrtUTF8to16(sDst, 0, NULL);
		int iRet = CopyFileW(sSrcW, sDstW, bReWrite ? FALSE : TRUE);
		xrtFree(sSrcW);
		xrtFree(sDstW);
		return iRet;
	#else
		// 其他平台方案
		if ( bReWrite == FALSE ) {
			if ( access(sDst, F_OK) == 0 ) {
				return FALSE;		// 文件已经存在
			}
		}
		// 打开文件
		int fsrc = open(sSrc, O_RDONLY);
		if ( fsrc == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		int fdst = open(sDst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if ( fdst == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			close(fsrc);
			return FALSE;
		}
		// 申请内存
		struct stat fileStat;
		fstat(fsrc, &fileStat);
		if ( fileStat.st_size == 0 ) {
			close(fsrc);
			close(fdst);
			return TRUE;		// 0 字节大小的文件不需要复制
		}
		str sText = xrtMalloc(fileStat.st_size);
		if ( sText == NULL ) {
			close(fsrc);
			close(fdst);
			return FALSE;
		}
		// 读取原始文件内容
		ssize_t iRetSize = read(fsrc, sText, fileStat.st_size);
		if ( iRetSize < 0 || iRetSize != fileStat.st_size ) {
			xrtSetError(sErrorFile_Read, FALSE);
			close(fsrc);
			close(fdst);
			xrtFree(sText);
			return FALSE;
		}
		// 写入目标文件
		iRetSize = write(fdst, sText, fileStat.st_size);
		if ( iRetSize < 0 || iRetSize != fileStat.st_size ) {
			xrtSetError(sErrorFile_Write, FALSE);
			close(fsrc);
			close(fdst);
			xrtFree(sText);
			return FALSE;
		}
		close(fsrc);
		close(fdst);
		xrtFree(sText);
		return TRUE;
	#endif
}
// 移动文件（重命名）
XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		u16str sDstW = xrtUTF8to16(sDst, 0, NULL);
		// 如果源文件和目标文件都存在，并且 bReWrite 为 TRUE，将目标文件删除
		int bExistSrc = xrtFileExists(sSrc);
		int bExistDst = xrtFileExists(sDst);
		if ( bExistSrc && bExistDst && bReWrite ) {
			DeleteFileW(sDstW);
		}
		// 移动文件
		int iRet = MoveFileExW(sSrcW, sDstW, MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH | (bReWrite ? MOVEFILE_REPLACE_EXISTING : 0));
		xrtFree(sSrcW);
		xrtFree(sDstW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int bExistSrc = xrtFileExists(sSrc);
		int bExistDst = xrtFileExists(sDst);
		if ( bExistSrc && bExistDst && bReWrite ) {
			remove(sDst);
		}
		int iRet = rename(sSrc, sDst);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			// 无法移动文件时，使用复制 + 删除来实现文件移动
			iRet = xrtFileCopy(sSrc, sDst, bReWrite);
			if ( iRet ) {
				remove(sSrc);
				return TRUE;
			} else {
				return FALSE;
			}
		}
	#endif
}
// 删除文件
XXAPI bool xrtFileDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = DeleteFileW(sPathW);
		xrtFree(sPathW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int iRet = remove(sPath);
		if ( iRet ) {
			return FALSE;
		} else {
			return TRUE;
		}
	#endif
}
// 扫描文件夹 ( 返回文件数量 )
#if defined(_WIN32) || defined(_WIN64)
	// windows 方案
	int __pri__DirScan_Proc(str sPath, size_t iSize, int bRecu, ptr pProc, ptr Param)
	{
		(void)iSize;
		int (*pCallBack)(ptr sPath, size_t iSize, int bDir, ptr pData, ptr Param) = pProc;
		int iFileCount = 0;
		WIN32_FIND_DATAW objFindData;
		str sFindPath = xrtPathJoin(2, sPath, "*");
		u16str sFindPathW = xrtUTF8to16(sFindPath, 0, NULL);
		HANDLE hFind = FindFirstFileW(sFindPathW, &objFindData);
		xrtFree(sFindPath);
		xrtFree(sFindPathW);
		if ( hFind == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_OpenDir, FALSE);
			return 0;
		}
		int bNext = TRUE;
		while ( bNext ) {
			int bExit = FALSE;
			if ( objFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
				// 过滤 . 和 .. 目录
				if ( (objFindData.cFileName[0] == L'.') && ((objFindData.cFileName[1] == 0) || ((objFindData.cFileName[1] == L'.') && (objFindData.cFileName[2] == 0))) ) {
				} else {
					str sFileName = xrtUTF16to8(objFindData.cFileName, 0, NULL);
					str sDir = xrtPathJoin(2, sPath, sFileName);
					size_t iDirSize = strlen((const char*)sDir);
					xrtFree(sFileName);
					// 处理文件夹 - 进入
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 1, &objFindData, Param);
					}
					// 递归子目录
					if ( bRecu ) {
						iFileCount += __pri__DirScan_Proc(sDir, iDirSize, bRecu, pProc, Param);
					}
					// 处理文件夹 - 离开
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 2, &objFindData, Param);
					}
					xrtFree(sDir);
				}
			} else {
				// 处理文件
				str sFileName = xrtUTF16to8(objFindData.cFileName, 0, NULL);
				str sFile = xrtPathJoin(2, sPath, sFileName);
				size_t iFileSize = strlen((const char*)sFile);
				xrtFree(sFileName);
				if ( pProc ) {
					bExit = pCallBack(sFile, iFileSize, 0, &objFindData, Param);
				}
				xrtFree(sFile);
				iFileCount++;
			}
			// 中途停止扫描
			if ( bExit ) {
				break;
			}
			bNext = FindNextFileW(hFind, &objFindData);
		}
		FindClose(hFind);
		return iFileCount;
	}
#else
	// 其他平台方案
	int __pri__DirScan_Proc(str sPath, size_t iSize, int bRecu, ptr pProc, ptr Param)
	{
		int (*pCallBack)(ptr sPath, size_t iSize, int bDir, ptr pData, ptr Param) = pProc;
		int iFileCount = 0;
		DIR* dir = opendir(sPath);
		if ( dir == NULL ) {
			xrtSetError(sErrorFile_OpenDir, FALSE);
			return 0;
		}
		struct dirent* entry;
		while ( (entry = readdir(dir)) != NULL ) {
			int bExit = FALSE;
			unsigned char iType = entry->d_type;
			if ( iType == DT_UNKNOWN ) {
				str sEntry = xrtPathJoin(2, sPath, entry->d_name);
				if ( sEntry ) {
					struct stat entryStat;
					if ( stat(sEntry, &entryStat) == 0 ) {
						if ( S_ISDIR(entryStat.st_mode) ) {
							iType = DT_DIR;
						} else if ( S_ISREG(entryStat.st_mode) ) {
							iType = DT_REG;
						}
					}
					xrtFree(sEntry);
				}
			}
			if ( iType == DT_DIR ) {
				// 过滤 . 和 .. 目录
				if ( (entry->d_name[0] == '.') && ((entry->d_name[1] == 0) || ((entry->d_name[1] == '.') && (entry->d_name[2] == 0))) ) {
				} else {
					str sDir = xrtPathJoin(2, sPath, entry->d_name);
					size_t iDirSize = strlen(sDir);
					// 处理文件夹 - 进入
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 1, entry, Param);
					}
					// 递归子目录
					if ( bRecu ) {
						iFileCount += __pri__DirScan_Proc(sDir, iDirSize, bRecu, pProc, Param);
					}
					// 处理文件夹 - 离开
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 2, entry, Param);
					}
					xrtFree(sDir);
				}
			} else if ( iType == DT_REG ) {
				// 处理文件
				str sFile = xrtPathJoin(2, sPath, entry->d_name);
				size_t iFileSize = strlen(sFile);
				if ( pProc ) {
					bExit = pCallBack(sFile, iFileSize, 0, entry, Param);
				}
				xrtFree(sFile);
				iFileCount++;
			}
			// 中途停止扫描
			if ( bExit ) {
				break;
			}
		}
		closedir(dir);
		return iFileCount;
	}
#endif
// xrtDirScan 相关处理
XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iFileCount = __pri__DirScan_Proc(sPath, iSize, bRecu, pProc, Param);
		return iFileCount;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iFileCount = __pri__DirScan_Proc(sPath, iSize, bRecu, pProc, Param);
		return iFileCount;
	#endif
}
// 创建文件夹
XXAPI bool xrtDirCreate(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = CreateDirectoryW(sPathW, NULL);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		int iRet = mkdir(sPath, 0755);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}
// 创建多级文件夹
XXAPI bool xrtDirCreateAll(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return FALSE; }
		size_t iSize = 0;
		u16str sPathW = xrtUTF8to16(sPath, 0, &iSize);
		if ( iSize == 0 ) { return FALSE; }
		u16str sCurPath = xrtMalloc((iSize + 1) * sizeof(wchar_t));
		if ( sCurPath == NULL ) {
			xrtFree(sPathW);
			return FALSE;
		}
		size_t iCurPos = 0;
		for ( size_t i = 0; i < iSize; i++ ) {
			if ( (sPathW[i] == L'/') || (sPathW[i] == L'\\') ) {
				sCurPath[iCurPos] = 0;
				CreateDirectoryW(sCurPath, NULL);
				sCurPath[iCurPos++] = L'\\';
			} else {
				sCurPath[iCurPos++] = sPathW[i];
			}
		}
		sCurPath[iSize] = 0;
		CreateDirectoryW(sCurPath, NULL);
		xrtFree(sCurPath);
		xrtFree(sPathW);
		return TRUE;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return FALSE; }
		size_t iSize = strlen(sPath);
		if ( iSize == 0 ) { return FALSE; }
		str sCurPath = xrtMalloc(iSize + 1);
		if ( sCurPath == NULL ) {
			return FALSE;
		}
		size_t iCurPos = 0;
		for ( int i = 0; i < iSize; i++ ) {
			if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
				sCurPath[iCurPos] = 0;
				mkdir(sCurPath, 0755);
				sCurPath[iCurPos++] = '/';
			} else {
				sCurPath[iCurPos++] = sPath[i];
			}
		}
		sCurPath[iSize] = 0;
		mkdir(sCurPath, 0755);
		xrtFree(sCurPath);
		return TRUE;
	#endif
}
// 复制文件夹 ( 返回操作的文件数量 )
#if defined(_WIN32) || defined(_WIN64)
	typedef struct {
		str DstPath;
		size_t DstSize;
		size_t SrcSize;
		int ReWrite;
		int MoveMode;			// 移动模式
	} xrtCopyFolder_Info;
	// 内部函数：复制 pri 目录进程
	int __pri__DirCopyProc(str sPath, size_t iSize, int bDir, ptr pData, xrtCopyFolder_Info* pInfo)
	{
		(void)iSize;
		(void)pData;
		if ( bDir == 0 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcopy file   : %S -> %S\n", sPath, sDstPath);
			if ( pInfo->MoveMode ) {
				xrtFileMove(sPath, sDstPath, pInfo->ReWrite);
			} else {
				xrtFileCopy(sPath, sDstPath, pInfo->ReWrite);
			}
			xrtFree(sDstPath);
		} else if ( bDir == 1 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcreate dir  : %S\n", sDstPath);
			xrtDirCreate(sDstPath);
			xrtFree(sDstPath);
		} else if ( bDir == 2 ) {
			if ( pInfo->MoveMode ) {
				// 移动模式离开文件夹时删除文件夹
				//printf("\tremove dir  : %S\n", sPath);
				u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
				RemoveDirectoryW(sPathW);
				xrtFree(sPathW);
			}
		}
		return FALSE;
	}
#else
	typedef struct {
		str DstPath;
		size_t DstSize;
		size_t SrcSize;
		int ReWrite;
		int MoveMode;			// 移动模式
	} xrtCopyFolder_Info;
	// 内部函数：复制 pri 目录进程
	int __pri__DirCopyProc(str sPath, size_t iSize, int bDir, ptr pData, xrtCopyFolder_Info* pInfo)
	{
		if ( bDir == 0 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcopy file   : %s -> %s\n", sPath, sDstPath);
			if ( pInfo->MoveMode ) {
				xrtFileMove(sPath, sDstPath, pInfo->ReWrite);
			} else {
				xrtFileCopy(sPath, sDstPath, pInfo->ReWrite);
			}
			xrtFree(sDstPath);
		} else if ( bDir == 1 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcreate dir  : %s\n", sDstPath);
			xrtDirCreate(sDstPath);
			xrtFree(sDstPath);
		} else if ( bDir == 2 ) {
			if ( pInfo->MoveMode ) {
				// 移动模式离开文件夹时删除文件夹
				//printf("\tremove dir  : %s\n", sPath);
				rmdir(sPath);
			}
		}
		return FALSE;
	}
#endif
// 复制目录
XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen((const char*)sSrc);
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen((const char*)sDst);
		if ( iDstSize == 0 ) { return 0; }
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = FALSE;
		xrtDirCreateAll(sDst);
		return xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
	#else
		// 其他平台方案
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen((const char*)sSrc);
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen((const char*)sDst);
		if ( iDstSize == 0 ) { return 0; }
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = FALSE;
		xrtDirCreateAll(sDst);
		return xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
	#endif
}
// 移动文件夹 ( 返回操作的文件数量 )
XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen(__xrt_cstr(sSrc));
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen(__xrt_cstr(sDst));
		if ( iDstSize == 0 ) { return 0; }
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = TRUE;
		xrtDirCreateAll(sDst);
		int iRet = xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		RemoveDirectoryW(sSrcW);
		xrtFree(sSrcW);
		return iRet;
	#else
		// 其他平台方案
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen(__xrt_cstr(sSrc));
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen(__xrt_cstr(sDst));
		if ( iDstSize == 0 ) { return 0; }
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = TRUE;
		xrtDirCreateAll(sDst);
		int iRet = xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
		rmdir(sSrc);
		return iRet;
	#endif
}
// 删除文件夹 ( 返回操作的文件数量 )
#if defined(_WIN32) || defined(_WIN64)
	// 内部函数：删除 pri 目录进程
	int __pri__DirDeleteProc(str sPath, size_t iSize, int bDir, ptr pData, xrtCopyFolder_Info* pInfo)
	{
		(void)pData;
		(void)pInfo;
		if ( bDir == 0 ) {
			//printf("\tremove file : %S\n", sPath);
			xrtFileDelete(sPath);
		} else if ( bDir == 2 ) {
			//printf("\tremove dir  : %S\n", sPath);
			u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
			RemoveDirectoryW(sPathW);
			xrtFree(sPathW);
		}
		return FALSE;
	}
#else
	// 内部函数：删除 pri 目录进程
	int __pri__DirDeleteProc(str sPath, size_t iSize, int bDir, ptr pData, xrtCopyFolder_Info* pInfo)
	{
		if ( bDir == 0 ) {
			//printf("\tremove file : %s\n", sPath);
			xrtFileDelete(sPath);
		} else if ( bDir == 2 ) {
			//printf("\tremove dir  : %s\n", sPath);
			rmdir(sPath);
		}
		return FALSE;
	}
#endif
// 删除目录
XXAPI int xrtDirDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iRet = xrtDirScan(sPath, TRUE, __pri__DirDeleteProc, NULL);
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		RemoveDirectoryW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen(sPath);
		if ( iSize == 0 ) { return 0; }
		int iRet = xrtDirScan(sPath, TRUE, __pri__DirDeleteProc, NULL);
		rmdir(sPath);
		return iRet;
	#endif
	return 0;
}

// ========================================
// File: D:/git/xrt/lib/file_async.h
// ========================================

#if !defined(XRT_NO_NETWORK)
#define __XAFILE_CHUNK_MAX		0x40000000u
static const char* __xafileErrHandle = "async file handle is invalid.";
static const char* __xafileErrRead = "async file read failed.";
static const char* __xafileErrWrite = "async file write failed.";
static const char* __xafileErrFlush = "async file flush failed.";
static const char* __xafileErrSize = "async file size query failed.";
static const char* __xafileErrSetSize = "async file resize failed.";
static const char* __xafileErrClosed = "async file is already closing.";
static const char* __xafileErrConfig = "async file config is invalid.";
static const char* __xafileErrMemory = "async file ran out of memory.";
struct xasyncfile_struct {
	xmutex pLock;
	uint32 iFlags;
	uint32 iShareFlags;
	volatile long iRefCount;
	bool bClosing;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hFile;
	#else
		int fd;
	#endif
};
typedef struct {
	xasyncfile* pFile;
	uint64 iOffset;
	size_t iSize;
} __xafile_read_task;
typedef struct {
	xasyncfile* pFile;
	uint64 iOffset;
	size_t iSize;
	ptr pData;
} __xafile_write_task;
typedef struct {
	xasyncfile* pFile;
	uint64 iSize;
} __xafile_size_task;
typedef enum {
	__XAFILE_PATH_APPEND = 1,
	__XAFILE_PATH_READ_ALL,
	__XAFILE_PATH_WRITE_ALL,
	__XAFILE_PATH_GET_ALL,
	__XAFILE_PATH_PUT_ALL,
	__XAFILE_PATH_COPY,
	__XAFILE_PATH_MOVE,
	__XAFILE_PATH_DELETE,
	__XAFILE_PATH_DIR_CREATE,
	__XAFILE_PATH_DIR_CREATE_ALL,
	__XAFILE_PATH_DIR_COPY,
	__XAFILE_PATH_DIR_MOVE,
	__XAFILE_PATH_DIR_DELETE
} __xafile_path_task_kind;
typedef struct {
	__xafile_path_task_kind iKind;
	str sPath;
	str sPath2;
	ptr pData;
	size_t iSize;
	int iCharset;
	bool bReWrite;
} __xafile_path_task;
// 内部函数：设置错误
static void __xafileSetError(const char* sError)
{
	xrtSetError((str)(sError ? sError : __xafileErrHandle), FALSE);
}
// 内部函数：标记失败任务
static int32 __xafileTaskFail(xfuture_result* pOut, const char* sError)
{
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pOut->iStatus = XRT_NET_ERROR;
	if ( sError && sError[0] ) {
		pOut->sError = xrtCopyStr((str)sError, 0);
		if ( pOut->sError && pOut->sError != xCore.sNull ) {
			pOut->iFlags |= XFUTURE_RESULT_F_OWN_ERROR;
		}
		else {
			pOut->sError = (str)sError;
		}
	}
	return pOut->iStatus;
}
// 内部函数：标记失败任务最后一次错误
static int32 __xafileTaskFailLastError(xfuture_result* pOut, const char* sFallback)
{
	str sError = xrtGetError();
	if ( sError == NULL || sError == xCore.sNull || sError[0] == '\0' ) {
		sError = (str)(sFallback ? sFallback : __xafileErrHandle);
	}
	return __xafileTaskFail(pOut, (const char*)sError);
}
// 内部函数：判断是否存在最后一次错误
static bool __xafileHasLastError(void)
{
	str sError = xrtGetError();
	return sError != NULL &&
		sError != xCore.sNull &&
		sError[0] != '\0';
}
// 内部函数：解析任务
static int32 __xafileTaskResolve(xfuture_result* pOut, ptr pValue)
{
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pValue;
	return pOut->iStatus;
}
// 内部函数：创建缓冲区
static xasyncfilebuf* __xafileBufCreate(void)
{
	xasyncfilebuf* pBuf = (xasyncfilebuf*)xrtMalloc(sizeof(xasyncfilebuf));
	if ( pBuf == NULL ) {
		return NULL;
	}
	memset(pBuf, 0, sizeof(*pBuf));
	return pBuf;
}
// 内部函数：__xafileIoCreate
static xasyncfileio* __xafileIoCreate(uint64 iValue, uint64 iOffset)
{
	xasyncfileio* pInfo = (xasyncfileio*)xrtMalloc(sizeof(xasyncfileio));
	if ( pInfo == NULL ) {
		return NULL;
	}
	pInfo->iValue = iValue;
	pInfo->iOffset = iOffset;
	return pInfo;
}
// 内部函数：__xafileBufDestroyInner
static void __xafileBufDestroyInner(xasyncfilebuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->pData != NULL && pBuf->pData != xCore.sNull ) {
		xrtFree(pBuf->pData);
	}
	xrtFree(pBuf);
}
// 内部函数：释放路径任务
static void __xafilePathTaskUnit(__xafile_path_task* pTask)
{
	if ( pTask == NULL ) {
		return;
	}
	if ( pTask->sPath && pTask->sPath != xCore.sNull ) {
		xrtFree(pTask->sPath);
	}
	if ( pTask->sPath2 && pTask->sPath2 != xCore.sNull ) {
		xrtFree(pTask->sPath2);
	}
	if ( pTask->pData && pTask->pData != xCore.sNull ) {
		xrtFree(pTask->pData);
	}
	xrtFree(pTask);
}
// 内部函数：__xafileAddRef
static bool __xafileAddRef(xasyncfile* pFile, bool bRejectClosing)
{
	bool bOk = false;
	if ( pFile == NULL || pFile->pLock == NULL ) {
		return false;
	}
	xrtMutexLock(pFile->pLock);
	if ( pFile->iRefCount > 0 ) {
		if ( !bRejectClosing || !pFile->bClosing ) {
			pFile->iRefCount++;
			bOk = true;
		}
	}
	xrtMutexUnlock(pFile->pLock);
	return bOk;
}
// 内部函数：释放
static void __xafileRelease(xasyncfile* pFile)
{
	xmutex pLock;
	bool bDestroy = false;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hFile = INVALID_HANDLE_VALUE;
	#else
		int fd = -1;
	#endif
	if ( pFile == NULL ) {
		return;
	}
	pLock = pFile->pLock;
	if ( pLock == NULL ) {
		xrtFree(pFile);
		return;
	}
	xrtMutexLock(pLock);
	if ( pFile->iRefCount > 0 ) {
		pFile->iRefCount--;
	}
	if ( pFile->iRefCount == 0 ) {
		bDestroy = true;
		#if defined(_WIN32) || defined(_WIN64)
			hFile = pFile->hFile;
			pFile->hFile = INVALID_HANDLE_VALUE;
		#else
			fd = pFile->fd;
			pFile->fd = -1;
		#endif
		pFile->pLock = NULL;
	}
	xrtMutexUnlock(pLock);
	if ( !bDestroy ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( hFile != INVALID_HANDLE_VALUE ) {
			CloseHandle(hFile);
		}
	#else
		if ( fd != -1 ) {
			close(fd);
		}
	#endif
	xrtMutexDestroy(pLock);
	xrtFree(pFile);
}
// 内部函数：__xafileSeekLocked
static bool __xafileSeekLocked(xasyncfile* pFile, uint64 iOffset)
{
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tOffset;
		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		tOffset.QuadPart = (LONGLONG)iOffset;
		return SetFilePointerEx(pFile->hFile, tOffset, NULL, FILE_BEGIN) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( iOffset > 0x7fffffffffffffffULL ) {
			errno = EINVAL;
			return false;
		}
		return lseek(pFile->fd, (off_t)iOffset, SEEK_SET) != (off_t)-1;
	#endif
}
// 内部函数：__xafileReadLocked
static bool __xafileReadLocked(xasyncfile* pFile, ptr pData, size_t iSize, size_t* piRead)
{
	size_t iTotal = 0u;
	if ( piRead ) {
		*piRead = 0u;
	}
	if ( iSize == 0u ) {
		return true;
	}
	if ( pFile == NULL || pData == NULL ) {
		return false;
	}
	while ( iTotal < iSize ) {
		size_t iChunkSize = iSize - iTotal;
		if ( iChunkSize > __XAFILE_CHUNK_MAX ) {
			iChunkSize = __XAFILE_CHUNK_MAX;
		}
		#if defined(_WIN32) || defined(_WIN64)
			DWORD iChunkRead = 0u;
			if ( !ReadFile(pFile->hFile, (uint8*)pData + iTotal, (DWORD)iChunkSize, &iChunkRead, NULL) ) {
				return false;
			}
			iTotal += (size_t)iChunkRead;
			if ( iChunkRead == 0u || iChunkRead < (DWORD)iChunkSize ) {
				break;
			}
		#else
			ssize_t iChunkRead = read(pFile->fd, (uint8*)pData + iTotal, iChunkSize);
			if ( iChunkRead < 0 ) {
				return false;
			}
			iTotal += (size_t)iChunkRead;
			if ( iChunkRead == 0 || (size_t)iChunkRead < iChunkSize ) {
				break;
			}
		#endif
	}
	if ( piRead ) {
		*piRead = iTotal;
	}
	return true;
}
// 内部函数：__xafileWriteLocked
static bool __xafileWriteLocked(xasyncfile* pFile, const void* pData, size_t iSize, size_t* piWrite)
{
	size_t iTotal = 0u;
	if ( piWrite ) {
		*piWrite = 0u;
	}
	if ( iSize == 0u ) {
		return true;
	}
	if ( pFile == NULL || pData == NULL ) {
		return false;
	}
	while ( iTotal < iSize ) {
		size_t iChunkSize = iSize - iTotal;
		if ( iChunkSize > __XAFILE_CHUNK_MAX ) {
			iChunkSize = __XAFILE_CHUNK_MAX;
		}
		#if defined(_WIN32) || defined(_WIN64)
			DWORD iChunkWrite = 0u;
			if ( !WriteFile(pFile->hFile, (const uint8*)pData + iTotal, (DWORD)iChunkSize, &iChunkWrite, NULL) ) {
				return false;
			}
			if ( iChunkWrite == 0u ) {
				return false;
			}
			iTotal += (size_t)iChunkWrite;
		#else
			ssize_t iChunkWrite = write(pFile->fd, (const uint8*)pData + iTotal, iChunkSize);
			if ( iChunkWrite <= 0 ) {
				return false;
			}
			iTotal += (size_t)iChunkWrite;
		#endif
	}
	if ( piWrite ) {
		*piWrite = iTotal;
	}
	return true;
}
// 内部函数：__xafileFlushLocked
static bool __xafileFlushLocked(xasyncfile* pFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		return FlushFileBuffers(pFile->hFile) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		return fsync(pFile->fd) == 0;
	#endif
}
// 内部函数：__xafileGetSizeLocked
static bool __xafileGetSizeLocked(xasyncfile* pFile, uint64* piSize)
{
	if ( piSize ) {
		*piSize = 0u;
	}
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tSize;
		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		if ( !GetFileSizeEx(pFile->hFile, &tSize) ) {
			return false;
		}
		if ( piSize ) {
			*piSize = (uint64)tSize.QuadPart;
		}
		return true;
	#else
		struct stat tStat;
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( fstat(pFile->fd, &tStat) != 0 ) {
			return false;
		}
		if ( piSize ) {
			*piSize = (uint64)tStat.st_size;
		}
		return true;
	#endif
}
// 内部函数：__xafileSetSizeLocked
static bool __xafileSetSizeLocked(xasyncfile* pFile, uint64 iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tSize;
		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		tSize.QuadPart = (LONGLONG)iSize;
		if ( !SetFilePointerEx(pFile->hFile, tSize, NULL, FILE_BEGIN) ) {
			return false;
		}
		return SetEndOfFile(pFile->hFile) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( iSize > 0x7fffffffffffffffULL ) {
			errno = EINVAL;
			return false;
		}
		return ftruncate(pFile->fd, (off_t)iSize) == 0;
	#endif
}
// 异步初始化文件配置
XXAPI void xrtAsyncFileConfigInit(xasyncfileconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iFlags = XAFILE_F_READ;
	pConfig->iShareFlags = XAFILE_SHARE_READ;
}
// 异步销毁文件缓冲区
XXAPI void xrtAsyncFileBufDestroy(xasyncfilebuf* pBuf)
{
	__xafileBufDestroyInner(pBuf);
}
// 异步销毁文件 io
XXAPI void xrtAsyncFileIoDestroy(xasyncfileio* pInfo)
{
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
}
#if defined(XRT_NO_THREAD)
static const char* __xafileErrThreadRequired = "async file requires thread support.";
// 异步打开文件
XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig)
{
	(void)pConfig;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步关闭文件
XXAPI void xrtAsyncFileClose(xasyncfile* pFile)
{
	(void)pFile;
}
// 异步读取文件
XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	(void)pFile;
	(void)iOffset;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步写入文件
XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	(void)pFile;
	(void)iOffset;
	(void)pData;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步刷新文件
XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile)
{
	(void)pFile;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步获取文件大小
XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile)
{
	(void)pFile;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步设置文件大小
XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize)
{
	(void)pFile;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步追加文件
XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	(void)sPath;
	(void)sText;
	(void)iSize;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步读取文件全部
XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset)
{
	(void)sPath;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步写入文件全部
XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	(void)sPath;
	(void)sText;
	(void)iSize;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步获取文件全部
XXAPI xfuture* xrtFileGetAllAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 文件 put 全部异步相关处理
XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize)
{
	(void)sPath;
	(void)pData;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步复制文件
XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// xrtFileMoveAsync 相关处理
XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步删除文件
XXAPI xfuture* xrtFileDeleteAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步创建目录
XXAPI xfuture* xrtDirCreateAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步创建目录全部
XXAPI xfuture* xrtDirCreateAllAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步复制目录
XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// xrtDirMoveAsync 相关处理
XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
// 异步删除目录
XXAPI xfuture* xrtDirDeleteAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}
#else
// 异步打开文件
XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig)
{
	xasyncfile* pFile = NULL;
	uint32 iFlags;
	uint32 iShareFlags;
	if ( pConfig == NULL || pConfig->sPath == NULL || pConfig->sPath[0] == '\0' ) {
		__xafileSetError(__xafileErrConfig);
		return NULL;
	}
	iFlags = pConfig->iFlags;
	if ( iFlags == 0u ) {
		iFlags = XAFILE_F_READ;
	}
	if ( (iFlags & (XAFILE_F_CREATE | XAFILE_F_TRUNCATE)) && !(iFlags & XAFILE_F_WRITE) ) {
		__xafileSetError(__xafileErrConfig);
		return NULL;
	}
	iShareFlags = pConfig->iShareFlags;
	if ( iShareFlags == 0u ) {
		iShareFlags = XAFILE_SHARE_READ;
	}
	pFile = (xasyncfile*)xrtMalloc(sizeof(*pFile));
	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	memset(pFile, 0, sizeof(*pFile));
	pFile->pLock = xrtMutexCreate();
	if ( pFile->pLock == NULL ) {
		xrtFree(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pFile->iFlags = iFlags;
	pFile->iShareFlags = iShareFlags;
	pFile->iRefCount = 1;
	#if defined(_WIN32) || defined(_WIN64)
		pFile->hFile = INVALID_HANDLE_VALUE;
	#else
		pFile->fd = -1;
	#endif
	#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iDesiredAccess = 0u;
		DWORD iShareMode = 0u;
		DWORD iCreation = OPEN_EXISTING;
		u16str sPathW;
		if ( iFlags & XAFILE_F_READ ) {
			iDesiredAccess |= GENERIC_READ;
		}
		if ( iFlags & XAFILE_F_WRITE ) {
			iDesiredAccess |= GENERIC_WRITE;
		}
		if ( iShareFlags & XAFILE_SHARE_READ ) {
			iShareMode |= FILE_SHARE_READ;
		}
		if ( iShareFlags & XAFILE_SHARE_WRITE ) {
			iShareMode |= FILE_SHARE_WRITE;
		}
		if ( iShareFlags & XAFILE_SHARE_DELETE ) {
			iShareMode |= FILE_SHARE_DELETE;
		}
		if ( (iFlags & XAFILE_F_CREATE) && (iFlags & XAFILE_F_TRUNCATE) ) {
			iCreation = CREATE_ALWAYS;
		}
		else if ( iFlags & XAFILE_F_CREATE ) {
			iCreation = OPEN_ALWAYS;
		}
		else if ( iFlags & XAFILE_F_TRUNCATE ) {
			iCreation = TRUNCATE_EXISTING;
		}
		sPathW = xrtUTF8to16(pConfig->sPath, 0, NULL);
		if ( sPathW == NULL || sPathW == (u16str)xCore.sNull ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
		pFile->hFile = CreateFileW(
			sPathW,
			iDesiredAccess,
			iShareMode,
			NULL,
			iCreation,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		xrtFree(sPathW);
		if ( pFile->hFile == INVALID_HANDLE_VALUE ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
	}
	#else
	{
		int iOpenFlags = 0;
		if ( (iFlags & XAFILE_F_READ) && (iFlags & XAFILE_F_WRITE) ) {
			iOpenFlags |= O_RDWR;
		}
		else if ( iFlags & XAFILE_F_WRITE ) {
			iOpenFlags |= O_WRONLY;
		}
		else {
			iOpenFlags |= O_RDONLY;
		}
		if ( iFlags & XAFILE_F_CREATE ) {
			iOpenFlags |= O_CREAT;
		}
		if ( iFlags & XAFILE_F_TRUNCATE ) {
			iOpenFlags |= O_TRUNC;
		}
		pFile->fd = open(pConfig->sPath, iOpenFlags, 0666);
		if ( pFile->fd == -1 ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
	}
	#endif
	return pFile;
}
// 异步关闭文件
XXAPI void xrtAsyncFileClose(xasyncfile* pFile)
{
	if ( pFile == NULL || pFile->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pFile->pLock);
	pFile->bClosing = true;
	xrtMutexUnlock(pFile->pLock);
	__xafileRelease(pFile);
}
// 内部函数：读取任务
static int32 __xafileReadTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_read_task* pTask = (__xafile_read_task*)pArg;
	xasyncfilebuf* pBuf = NULL;
	ptr pData = NULL;
	size_t iRead = 0u;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}
	pBuf = __xafileBufCreate();
	if ( pBuf == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	pBuf->iOffset = pTask->iOffset;
	if ( pTask->iSize > 0u ) {
		pData = xrtMalloc(pTask->iSize + 1u);
		if ( pData == NULL ) {
			iRet = __xafileTaskFail(pOut, __xafileErrMemory);
			goto Exit;
		}
	}
	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_READ) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrRead);
		goto Exit;
	}
	if ( !__xafileSeekLocked(pTask->pFile, pTask->iOffset) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrRead);
		goto Exit;
	}
	if ( !__xafileReadLocked(pTask->pFile, pData, pTask->iSize, &iRead) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrRead);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);
	if ( pData != NULL ) {
		((char*)pData)[iRead] = '\0';
	}
	if ( iRead == 0u && pData != NULL ) {
		xrtFree(pData);
		pData = NULL;
	}
	pBuf->pData = pData;
	pBuf->iSize = iRead;
	pBuf->bEOF = (pTask->iSize > 0u && iRead < pTask->iSize);
	pData = NULL;
	iRet = __xafileTaskResolve(pOut, pBuf);
	pBuf = NULL;
Exit:
	if ( pData != NULL && pData != xCore.sNull ) {
		xrtFree(pData);
	}
	if ( pBuf != NULL ) {
		__xafileBufDestroyInner(pBuf);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}
// 内部函数：写入任务
static int32 __xafileWriteTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_write_task* pTask = (__xafile_write_task*)pArg;
	xasyncfileio* pInfo = NULL;
	size_t iWrite = 0u;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}
	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_WRITE) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrWrite);
		goto Exit;
	}
	if ( !__xafileSeekLocked(pTask->pFile, pTask->iOffset) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
		goto Exit;
	}
	if ( !__xafileWriteLocked(pTask->pFile, pTask->pData, pTask->iSize, &iWrite) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);
	pInfo = __xafileIoCreate((uint64)iWrite, pTask->iOffset);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;
Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		if ( pTask->pData != NULL && pTask->pData != xCore.sNull ) {
			xrtFree(pTask->pData);
		}
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}
// 内部函数：刷新任务
static int32 __xafileFlushTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}
	xrtMutexLock(pTask->pFile->pLock);
	if ( pTask->pFile->iFlags & XAFILE_F_WRITE ) {
		if ( !__xafileFlushLocked(pTask->pFile) ) {
			xrtMutexUnlock(pTask->pFile->pLock);
			iRet = __xafileTaskFailLastError(pOut, __xafileErrFlush);
			goto Exit;
		}
	}
	xrtMutexUnlock(pTask->pFile->pLock);
	iRet = __xafileTaskResolve(pOut, NULL);
Exit:
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}
// 内部函数：获取大小任务
static int32 __xafileGetSizeTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	xasyncfileio* pInfo = NULL;
	uint64 iSize = 0u;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}
	xrtMutexLock(pTask->pFile->pLock);
	if ( !__xafileGetSizeLocked(pTask->pFile, &iSize) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrSize);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);
	pInfo = __xafileIoCreate(iSize, 0u);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;
Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}
// 内部函数：设置大小任务
static int32 __xafileSetSizeTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	xasyncfileio* pInfo = NULL;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}
	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_WRITE) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrSetSize);
		goto Exit;
	}
	if ( !__xafileSetSizeLocked(pTask->pFile, pTask->iSize) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrSetSize);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);
	pInfo = __xafileIoCreate(pTask->iSize, 0u);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;
Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}
// 内部函数：路径任务进程相关处理
static int32 __xafilePathTaskProc(ptr pArg, xfuture_result* pOut)
{
	__xafile_path_task* pTask = (__xafile_path_task*)pArg;
	xasyncfilebuf* pBuf = NULL;
	xasyncfileio* pInfo = NULL;
	ptr pData = NULL;
	size_t iSize = 0u;
	int iCount = 0;
	int32 iRet = XRT_NET_ERROR;
	xrtClearError();
	if ( pTask == NULL || pTask->sPath == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrConfig);
	}
	switch ( pTask->iKind ) {
		case __XAFILE_PATH_APPEND:
			iCount = xrtFileAppend(pTask->sPath, (str)pTask->pData, pTask->iSize, pTask->iCharset);
			if ( iCount == 0 && pTask->iSize > 0u ) {
				iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		case __XAFILE_PATH_READ_ALL:
			pData = xrtFileReadAll(pTask->sPath, pTask->iCharset, &iSize);
			if ( pData == NULL || (pData == xCore.sNull && __xafileHasLastError()) ) {
				iRet = __xafileTaskFailLastError(pOut, (const char*)sErrorFile_Read);
				goto Exit;
			}
			if ( pData == xCore.sNull ) {
				pData = NULL;
			}
			pBuf = __xafileBufCreate();
			if ( pBuf == NULL ) {
				iRet = __xafileTaskFail(pOut, __xafileErrMemory);
				goto Exit;
			}
			pBuf->pData = pData;
			pBuf->iSize = iSize;
			pData = NULL;
			iRet = __xafileTaskResolve(pOut, pBuf);
			pBuf = NULL;
			goto Exit;
		case __XAFILE_PATH_WRITE_ALL:
			iCount = xrtFileWriteAll(pTask->sPath, (str)pTask->pData, pTask->iSize, pTask->iCharset);
			if ( iCount == 0 && pTask->iSize > 0u ) {
				iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		case __XAFILE_PATH_GET_ALL:
			pData = xrtFileGetAll(pTask->sPath, &iSize);
			if ( pData == NULL || (pData == xCore.sNull && __xafileHasLastError()) ) {
				iRet = __xafileTaskFailLastError(pOut, (const char*)sErrorFile_Read);
				goto Exit;
			}
			if ( pData == xCore.sNull ) {
				pData = NULL;
			}
			pBuf = __xafileBufCreate();
			if ( pBuf == NULL ) {
				iRet = __xafileTaskFail(pOut, __xafileErrMemory);
				goto Exit;
			}
			pBuf->pData = pData;
			pBuf->iSize = iSize;
			pData = NULL;
			iRet = __xafileTaskResolve(pOut, pBuf);
			pBuf = NULL;
			goto Exit;
		case __XAFILE_PATH_PUT_ALL:
			iCount = xrtFilePutAll(pTask->sPath, pTask->pData, pTask->iSize);
			if ( iCount == 0 && pTask->iSize > 0u ) {
				iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		case __XAFILE_PATH_COPY:
			if ( !xrtFileCopy(pTask->sPath, pTask->sPath2, pTask->bReWrite) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file copy failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;
		case __XAFILE_PATH_MOVE:
			if ( !xrtFileMove(pTask->sPath, pTask->sPath2, pTask->bReWrite) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file move failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;
		case __XAFILE_PATH_DELETE:
			if ( !xrtFileDelete(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file delete failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;
		case __XAFILE_PATH_DIR_CREATE:
			if ( !xrtDirCreate(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir create failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;
		case __XAFILE_PATH_DIR_CREATE_ALL:
			if ( !xrtDirCreateAll(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir create-all failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;
		case __XAFILE_PATH_DIR_COPY:
			if ( !xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFail(pOut, "async dir copy failed.");
				goto Exit;
			}
			iCount = xrtDirCopy(pTask->sPath, pTask->sPath2, pTask->bReWrite);
			if ( __xafileHasLastError() || !xrtDirExists(pTask->sPath2) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir copy failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		case __XAFILE_PATH_DIR_MOVE:
			if ( !xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFail(pOut, "async dir move failed.");
				goto Exit;
			}
			iCount = xrtDirMove(pTask->sPath, pTask->sPath2, pTask->bReWrite);
			if ( __xafileHasLastError() || !xrtDirExists(pTask->sPath2) || xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir move failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		case __XAFILE_PATH_DIR_DELETE:
			if ( !xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFail(pOut, "async dir delete failed.");
				goto Exit;
			}
			iCount = xrtDirDelete(pTask->sPath);
			if ( __xafileHasLastError() || xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir delete failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;
		default:
			iRet = __xafileTaskFail(pOut, __xafileErrConfig);
			goto Exit;
	}
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;
Exit:
	if ( pData != NULL && pData != xCore.sNull ) {
		xrtFree(pData);
	}
	if ( pBuf != NULL ) {
		__xafileBufDestroyInner(pBuf);
	}
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	__xafilePathTaskUnit(pTask);
	return iRet;
}
// 内部函数：__xafileStartReadTask
static xfuture* __xafileStartReadTask(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	__xafile_read_task* pTask;
	xfuture* pFuture;
	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}
	pTask = (__xafile_read_task*)xrtMalloc(sizeof(__xafile_read_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->pFile = pFile;
	pTask->iOffset = iOffset;
	pTask->iSize = iSize;
	pFuture = xTaskRunThread(__xafileReadTask, pTask, 0);
	if ( pFuture == NULL ) {
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}
// 内部函数：__xafileStartWriteTask
static xfuture* __xafileStartWriteTask(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	__xafile_write_task* pTask;
	xfuture* pFuture;
	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}
	pTask = (__xafile_write_task*)xrtMalloc(sizeof(__xafile_write_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	memset(pTask, 0, sizeof(*pTask));
	pTask->pFile = pFile;
	pTask->iOffset = iOffset;
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)pData, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafileRelease(pFile);
			xrtFree(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	pFuture = xTaskRunThread(__xafileWriteTask, pTask, 0);
	if ( pFuture == NULL ) {
		if ( pTask->pData != NULL && pTask->pData != xCore.sNull ) {
			xrtFree(pTask->pData);
		}
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}
// 内部函数：启动大小任务
static xfuture* __xafileStartSizeTask(xasyncfile* pFile, xtask_thread_fn pfnTask, uint64 iSize)
{
	__xafile_size_task* pTask;
	xfuture* pFuture;
	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}
	pTask = (__xafile_size_task*)xrtMalloc(sizeof(__xafile_size_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->pFile = pFile;
	pTask->iSize = iSize;
	pFuture = xTaskRunThread(pfnTask, pTask, 0);
	if ( pFuture == NULL ) {
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}
// 内部函数：创建路径任务
static __xafile_path_task* __xafilePathTaskCreate(__xafile_path_task_kind iKind, str sPath, str sPath2)
{
	__xafile_path_task* pTask = (__xafile_path_task*)xrtMalloc(sizeof(__xafile_path_task));
	if ( pTask == NULL ) {
		return NULL;
	}
	memset(pTask, 0, sizeof(*pTask));
	pTask->iKind = iKind;
	pTask->sPath = xrtCopyStr(sPath, 0);
	if ( sPath != NULL && sPath[0] != '\0' && (pTask->sPath == NULL || pTask->sPath == xCore.sNull) ) {
		__xafilePathTaskUnit(pTask);
		return NULL;
	}
	if ( sPath2 != NULL ) {
		pTask->sPath2 = xrtCopyStr(sPath2, 0);
		if ( sPath2[0] != '\0' && (pTask->sPath2 == NULL || pTask->sPath2 == xCore.sNull) ) {
			__xafilePathTaskUnit(pTask);
			return NULL;
		}
	}
	return pTask;
}
// 内部函数：启动路径任务
static xfuture* __xafileStartPathTask(__xafile_path_task* pTask)
{
	xfuture* pFuture;
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pFuture = xTaskRunThread(__xafilePathTaskProc, pTask, 0);
	if ( pFuture == NULL ) {
		__xafilePathTaskUnit(pTask);
	}
	return pFuture;
}
// 异步读取文件
XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	return __xafileStartReadTask(pFile, iOffset, iSize);
}
// 异步写入文件
XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	return __xafileStartWriteTask(pFile, iOffset, pData, iSize);
}
// 异步刷新文件
XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile)
{
	return __xafileStartSizeTask(pFile, __xafileFlushTask, 0u);
}
// 异步获取文件大小
XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile)
{
	return __xafileStartSizeTask(pFile, __xafileGetSizeTask, 0u);
}
// 异步设置文件大小
XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize)
{
	return __xafileStartSizeTask(pFile, __xafileSetSizeTask, iSize);
}
// 异步追加文件
XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_APPEND, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	if ( iSize == 0u && sText != NULL ) {
		iSize = strlen((const char*)sText);
	}
	pTask->iCharset = iCharset;
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)sText, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}
// 异步读取文件全部
XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_READ_ALL, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->iCharset = iCharset;
	return __xafileStartPathTask(pTask);
}
// 异步写入文件全部
XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_WRITE_ALL, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	if ( iSize == 0u && sText != NULL ) {
		iSize = strlen((const char*)sText);
	}
	pTask->iCharset = iCharset;
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)sText, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}
// 异步获取文件全部
XXAPI xfuture* xrtFileGetAllAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_GET_ALL, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}
// 文件 put 全部异步相关处理
XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_PUT_ALL, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)pData, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}
// 异步复制文件
XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_COPY, sSrc, sDst);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}
// xrtFileMoveAsync 相关处理
XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_MOVE, sSrc, sDst);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}
// 异步删除文件
XXAPI xfuture* xrtFileDeleteAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DELETE, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}
// 异步创建目录
XXAPI xfuture* xrtDirCreateAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_CREATE, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}
// 异步创建目录全部
XXAPI xfuture* xrtDirCreateAllAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_CREATE_ALL, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}
// 异步复制目录
XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_COPY, sSrc, sDst);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}
// xrtDirMoveAsync 相关处理
XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_MOVE, sSrc, sDst);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}
// 异步删除目录
XXAPI xfuture* xrtDirDeleteAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_DELETE, sPath, NULL);
	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}
#endif
#endif
#endif
#ifndef XRT_NO_THREAD

// ========================================
// File: D:/git/xrt/lib/thread.h
// ========================================


/* ================================ 线程管理 ================================ */
#if !defined(_WIN32) && !defined(_WIN64)
	#if defined(CLOCK_MONOTONIC)
		#define __XRT_THREAD_WAIT_CLOCK CLOCK_MONOTONIC
	#else
		#define __XRT_THREAD_WAIT_CLOCK CLOCK_REALTIME
	#endif
	// 内部函数：构建线程绝对超时时间
	static bool __xrtThreadMakeAbsTimeout(struct timespec* pTs, uint32 iTimeoutMs)
	{
		uint64 iNs;
		if ( !pTs ) return false;
		if ( clock_gettime(__XRT_THREAD_WAIT_CLOCK, pTs) != 0 ) return false;
		iNs = (uint64)pTs->tv_nsec + ((uint64)iTimeoutMs * 1000000ULL);
		pTs->tv_sec += (time_t)(iNs / 1000000000ULL);
		pTs->tv_nsec = (long)(iNs % 1000000000ULL);
		return true;
	}
	// 内部函数：初始化单调时钟条件变量
	static bool __xrtThreadInitMonotonicCond(pthread_cond_t* pCond)
	{
		pthread_condattr_t tAttr;
		if ( !pCond ) return false;
		if ( pthread_condattr_init(&tAttr) != 0 ) return false;
		#if defined(CLOCK_MONOTONIC)
			(void)pthread_condattr_setclock(&tAttr, CLOCK_MONOTONIC);
		#endif
		if ( pthread_cond_init(pCond, &tAttr) != 0 ) {
			pthread_condattr_destroy(&tAttr);
			return false;
		}
		pthread_condattr_destroy(&tAttr);
		return true;
	}
#endif
// 内部函数：分配线程对象内存
static inline ptr __xrtThreadObjAlloc(size_t iSize)
{
	ptr (*procMalloc)(size_t) = xCore.malloc ? xCore.malloc : malloc;
	return procMalloc(iSize);
}
// 内部函数：释放线程对象内存
static inline void __xrtThreadObjFree(ptr pMem)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;
	if ( pMem ) {
		procFree(pMem);
	}
}
// 线程包装函数（统一完成 attach / detach / exit code 保存）
#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI xrtThreadWrapper(LPVOID lpParameter)
{
	xthread pThread = (xthread)lpParameter;
	uint32 iExitCode = 0;
	if ( pThread == NULL ) {
		return 0;
	}
	__xrtThreadAttachManaged(pThread);
	if ( pThread->Proc ) {
		iExitCode = pThread->Proc(pThread->Param);
	}
	__xrtThreadExitManaged(pThread, iExitCode);
	return iExitCode;
}
#else
// 线程包装函数（统一完成 attach / detach / exit code 保存）
static void* xrtThreadWrapper(void* pParameter)
{
	xthread pThread = (xthread)pParameter;
	uint32 iExitCode = 0;
	if ( pThread == NULL ) {
		return (void*)(uintptr_t)0;
	}
	__xrtThreadAttachManaged(pThread);
	if ( pThread->Proc ) {
		iExitCode = pThread->Proc(pThread->Param);
	}
	__xrtThreadExitManaged(pThread, iExitCode);
	return (void*)(uintptr_t)iExitCode;
}
#endif
// 创建线程
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize)
{
	xthread pThread = __xrtThreadObjAlloc(sizeof(xthread_struct));
	if ( !pThread ) return NULL;
	pThread->Proc = pProc;
	pThread->Param = pParam;
	pThread->StopFlag = 0;
	pThread->bFinished = FALSE;
	pThread->bJoined = FALSE;
	pThread->bAutoDestroy = FALSE;
	pThread->ExitCode = 0;
	pThread->TID = 0;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iThreadID;
		pThread->Handle = CreateThread(NULL, iStackSize, xrtThreadWrapper, pThread, 0, &iThreadID);
		pThread->TID = iThreadID;
		if ( !pThread->Handle ) {
			__xrtThreadObjFree(pThread);
			return NULL;
		}
	#else
		pthread_t tid;
		pthread_attr_t attr;
		if ( pthread_mutex_init(&pThread->FinishLock, NULL) != 0 ) {
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		if ( !__xrtThreadInitMonotonicCond(&pThread->FinishCond) ) {
			pthread_mutex_destroy(&pThread->FinishLock);
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		pthread_attr_init(&attr);
		if ( iStackSize > 0 ) {
			pthread_attr_setstacksize(&attr, iStackSize);
		}
		if ( pthread_create(&tid, &attr, xrtThreadWrapper, pThread) != 0 ) {
			pthread_attr_destroy(&attr);
			pthread_cond_destroy(&pThread->FinishCond);
			pthread_mutex_destroy(&pThread->FinishLock);
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		pthread_attr_destroy(&attr);
		pThread->Handle = (ptr)tid;
	#endif
	return pThread;
}
// 销毁线程对象（不终止线程，仅释放管理结构）
XXAPI void xrtThreadDestroy(xthread pThread)
{
	if ( pThread ) {
		if ( !pThread->bFinished && !pThread->bJoined ) {
			xrtSetError("thread is still running.", FALSE);
			return;
		}
		#if defined(_WIN32) || defined(_WIN64)
			if ( pThread->Handle ) {
				CloseHandle(pThread->Handle);
				pThread->Handle = NULL;
			}
		#else
			if ( pThread->Handle && !pThread->bJoined ) {
				void* pExit = NULL;
				pthread_join((pthread_t)pThread->Handle, &pExit);
				pThread->bJoined = TRUE;
			}
			pthread_cond_destroy(&pThread->FinishCond);
			pthread_mutex_destroy(&pThread->FinishLock);
			pThread->Handle = NULL;
		#endif
		__xrtThreadObjFree(pThread);
	}
}
// 等待线程结束
XXAPI void xrtThreadWait(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return;
	if ( pThread->bJoined ) return;
	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pThread->Handle, INFINITE);
		DWORD iExitCode = 0;
		if ( GetExitCodeThread(pThread->Handle, &iExitCode) ) {
			pThread->ExitCode = (uint32)iExitCode;
		}
		pThread->bFinished = TRUE;
		pThread->bJoined = TRUE;
	#else
		void* pExit = NULL;
		pthread_join((pthread_t)pThread->Handle, &pExit);
		pThread->bFinished = TRUE;
		pThread->bJoined = TRUE;
	#endif
}
// 等待线程结束（带超时，毫秒）
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout)
{
	if ( !pThread || !pThread->Handle ) return XRT_WAIT_ERROR;
	if ( pThread->bJoined ) return XRT_WAIT_OK;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pThread->Handle, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) {
			DWORD iExitCode = 0;
			if ( GetExitCodeThread(pThread->Handle, &iExitCode) ) {
				pThread->ExitCode = (uint32)iExitCode;
			}
			pThread->bFinished = TRUE;
			pThread->bJoined = TRUE;
			return XRT_WAIT_OK;
		}
		if ( ret == WAIT_TIMEOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#else
		int iRet;
		if ( pthread_mutex_lock(&pThread->FinishLock) != 0 ) {
			return XRT_WAIT_ERROR;
		}
		if ( iTimeout == 0 && !pThread->bFinished ) {
			pthread_mutex_unlock(&pThread->FinishLock);
			return XRT_WAIT_TIMEOUT;
		}
		if ( iTimeout == UINT32_MAX ) {
			while ( !pThread->bFinished ) {
				if ( pthread_cond_wait(&pThread->FinishCond, &pThread->FinishLock) != 0 ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_ERROR;
				}
			}
		} else if ( !pThread->bFinished ) {
			struct timespec ts;
			if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) {
				pthread_mutex_unlock(&pThread->FinishLock);
				return XRT_WAIT_ERROR;
			}
			while ( !pThread->bFinished ) {
				iRet = pthread_cond_timedwait(&pThread->FinishCond, &pThread->FinishLock, &ts);
				if ( iRet == ETIMEDOUT ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_TIMEOUT;
				}
				if ( iRet != 0 ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_ERROR;
				}
			}
		}
		pthread_mutex_unlock(&pThread->FinishLock);
		if ( !pThread->bJoined ) {
			void* pExit = NULL;
			if ( pthread_join((pthread_t)pThread->Handle, &pExit) != 0 ) {
				return XRT_WAIT_ERROR;
			}
			pThread->bJoined = TRUE;
		}
		return XRT_WAIT_OK;
	#endif
}
// 发送停止信号
XXAPI void xrtThreadStop(xthread pThread)
{
	if ( pThread ) {
		__xrtAtomicStoreU32((volatile uint32*)&pThread->StopFlag, 1u);
	}
}
// 检查是否应该停止
XXAPI bool xrtThreadShouldStop(xthread pThread)
{
	if ( pThread ) {
		return __xrtAtomicLoadU32((const volatile uint32*)&pThread->StopFlag) != 0;
	}
	return FALSE;
}
// 强制终止线程
// 挂起线程
// 恢复线程
// 获取线程状态
XXAPI int xrtThreadGetState(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) return XRT_THREAD_STOPPED;
	if ( pThread->bFinished ) return XRT_THREAD_STOPPED;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD exitCode;
		if ( GetExitCodeThread(pThread->Handle, &exitCode) ) {
			return (exitCode == STILL_ACTIVE) ? XRT_THREAD_RUNNING : XRT_THREAD_STOPPED;
		}
		return XRT_THREAD_STOPPED;
	#else
		return XRT_THREAD_RUNNING;
	#endif
}
// 获取线程退出码
XXAPI uint32 xrtThreadGetExitCode(xthread pThread)
{
	if ( !pThread ) return 0;
	return pThread->ExitCode;
}
// 获取当前线程ID
XXAPI uint64 xrtThreadGetCurrentId()
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetCurrentThreadId();
	#else
		return (uint64)(uintptr_t)pthread_self();
	#endif
}
// 让出当前线程的时间片
XXAPI void xrtThreadYield()
{
	#if defined(_WIN32) || defined(_WIN64)
		SwitchToThread();
	#else
		sched_yield();
	#endif
}
/* ================================ 互斥体 ================================ */
// 创建互斥体
XXAPI xmutex xrtMutexCreate()
{
	xmutex pMutex = __xrtThreadObjAlloc(sizeof(xmutex_struct));
	if ( !pMutex ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pMutex->objLock);
		pMutex->iOwnerThreadId = 0;
	#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		int ret = pthread_mutex_init(&pMutex->objLock, &attr);
		pthread_mutexattr_destroy(&attr);
		if ( ret != 0 ) {
			__xrtThreadObjFree(pMutex);
			pMutex = NULL;
			return NULL;
		}
	#endif
	
	return pMutex;
}
// 销毁互斥体
XXAPI void xrtMutexDestroy(xmutex pMutex)
{
	if ( pMutex ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( pMutex->iOwnerThreadId != 0 ) {
				xrtSetError("mutex is still locked.", FALSE);
				return;
			}
			xrtMutexUnit(pMutex);
		#else
			if ( pthread_mutex_trylock(&pMutex->objLock) != 0 ) {
				xrtSetError("mutex is still locked.", FALSE);
				return;
			}
			pthread_mutex_unlock(&pMutex->objLock);
			if ( pthread_mutex_destroy(&pMutex->objLock) != 0 ) {
				xrtSetError("mutex destroy failed.", FALSE);
				return;
			}
		#endif
		__xrtThreadObjFree(pMutex);
	}
}
// 初始化互斥体
XXAPI void xrtMutexInit(xmutex pMutex)
{
	if ( !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pMutex->objLock);
		pMutex->iOwnerThreadId = 0;
	#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&pMutex->objLock, &attr);
		pthread_mutexattr_destroy(&attr);
	#endif
}
// 释放互斥体
XXAPI void xrtMutexUnit(xmutex pMutex)
{
	if ( !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		if ( pMutex->iOwnerThreadId != 0 ) {
			xrtSetError("mutex is still locked.", FALSE);
			return;
		}
		// SRWLOCK 不需要显式销毁
		pMutex->iOwnerThreadId = 0;
	#else
		int iRet = pthread_mutex_trylock(&pMutex->objLock);
		if ( iRet != 0 ) {
			xrtSetError("mutex is still locked.", FALSE);
			return;
		}
		pthread_mutex_unlock(&pMutex->objLock);
		if ( pthread_mutex_destroy(&pMutex->objLock) != 0 ) {
			xrtSetError("mutex destroy failed.", FALSE);
		}
	#endif
}
// 锁定互斥体
XXAPI void xrtMutexLock(xmutex pMutex)
{
	if ( !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		AcquireSRWLockExclusive(&pMutex->objLock);
		pMutex->iOwnerThreadId = GetCurrentThreadId();
	#else
		pthread_mutex_lock(&pMutex->objLock);
	#endif
}
// 尝试锁定互斥体
XXAPI bool xrtMutexTryLock(xmutex pMutex)
{
	if ( !pMutex ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		if ( pMutex->iOwnerThreadId == GetCurrentThreadId() ) {
			return FALSE;
		}
		if ( TryAcquireSRWLockExclusive(&pMutex->objLock) == 0 ) {
			return FALSE;
		}
		pMutex->iOwnerThreadId = GetCurrentThreadId();
		return TRUE;
	#else
		return pthread_mutex_trylock(&pMutex->objLock) == 0;
	#endif
}
// 解锁互斥体
XXAPI void xrtMutexUnlock(xmutex pMutex)
{
	if ( !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		ReleaseSRWLockExclusive(&pMutex->objLock);
	#else
		pthread_mutex_unlock(&pMutex->objLock);
	#endif
}
/* ================================ 信号量 ================================ */
// 创建信号量
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue)
{
	xsem pSem = __xrtThreadObjAlloc(sizeof(xsem_struct));
	if ( !pSem ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->objSem = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
		if ( !pSem->objSem ) {
			__xrtThreadObjFree(pSem);
			return NULL;
		}
	#else
		if ( pthread_mutex_init(&pSem->objLock, NULL) != 0 ) {
			__xrtThreadObjFree(pSem);
			return NULL;
		}
		if ( !__xrtThreadInitMonotonicCond(&pSem->objCond) ) {
			pthread_mutex_destroy(&pSem->objLock);
			__xrtThreadObjFree(pSem);
			return NULL;
		}
		pSem->iValue = iInitValue;
		pSem->iMaxValue = iMaxValue;
	#endif
	
	return pSem;
}
// 销毁信号量
XXAPI void xrtSemDestroy(xsem pSem)
{
	if ( pSem ) {
		xrtSemUnit(pSem);
		__xrtThreadObjFree(pSem);
	}
}
// 初始化信号量
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue)
{
	if ( !pSem ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->objSem = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
	#else
		if ( pthread_mutex_init(&pSem->objLock, NULL) != 0 ) return;
		if ( !__xrtThreadInitMonotonicCond(&pSem->objCond) ) {
			pthread_mutex_destroy(&pSem->objLock);
			return;
		}
		pSem->iValue = iInitValue;
		pSem->iMaxValue = iMaxValue;
	#endif
}
// xrtSemUnit 相关处理
XXAPI void xrtSemUnit(xsem pSem)
{
	if ( !pSem ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		CloseHandle(pSem->objSem);
		pSem->objSem = NULL;
	#else
		pthread_cond_destroy(&pSem->objCond);
		pthread_mutex_destroy(&pSem->objLock);
		memset(&pSem->objCond, 0, sizeof(pSem->objCond));
		memset(&pSem->objLock, 0, sizeof(pSem->objLock));
		pSem->iValue = 0;
		pSem->iMaxValue = 0;
	#endif
}
// xrtSemWait 相关处理
XXAPI void xrtSemWait(xsem pSem)
{
	if ( !pSem ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pSem->objSem, INFINITE);
	#else
		pthread_mutex_lock(&pSem->objLock);
		while ( pSem->iValue == 0 ) {
			pthread_cond_wait(&pSem->objCond, &pSem->objLock);
		}
		pSem->iValue--;
		pthread_mutex_unlock(&pSem->objLock);
	#endif
}
// xrtSemTryWait 相关处理
XXAPI bool xrtSemTryWait(xsem pSem)
{
	if ( !pSem ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return WaitForSingleObject(pSem->objSem, 0) == WAIT_OBJECT_0;
	#else
		bool bOk = FALSE;
		pthread_mutex_lock(&pSem->objLock);
		if ( pSem->iValue > 0 ) {
			pSem->iValue--;
			bOk = TRUE;
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}
// xrtSemWaitTimeout 相关处理
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout)
{
	if ( !pSem ) return XRT_WAIT_ERROR;
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pSem->objSem, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) return XRT_WAIT_OK;
		if ( ret == WAIT_TIMEOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		int ret = XRT_WAIT_ERROR;
		if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) return XRT_WAIT_ERROR;
		pthread_mutex_lock(&pSem->objLock);
		while ( pSem->iValue == 0 ) {
			int iWaitRet = pthread_cond_timedwait(&pSem->objCond, &pSem->objLock, &ts);
			if ( iWaitRet == ETIMEDOUT ) {
				ret = XRT_WAIT_TIMEOUT;
				goto exit_wait;
			}
			if ( iWaitRet != 0 ) {
				ret = XRT_WAIT_ERROR;
				goto exit_wait;
			}
		}
		pSem->iValue--;
		ret = XRT_WAIT_OK;
	exit_wait:
		pthread_mutex_unlock(&pSem->objLock);
		return ret;
	#endif
}
// xrtSemPost 相关处理
XXAPI bool xrtSemPost(xsem pSem)
{
	if ( !pSem ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->objSem, 1, NULL) != 0;
	#else
		bool bOk = FALSE;
		pthread_mutex_lock(&pSem->objLock);
		if ( pSem->iValue < pSem->iMaxValue ) {
			pSem->iValue++;
			bOk = TRUE;
			pthread_cond_signal(&pSem->objCond);
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}
// xrtSemPostMultiple 相关处理
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount)
{
	if ( !pSem || iCount == 0 ) return FALSE;
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->objSem, iCount, NULL) != 0;
	#else
		bool bOk = FALSE;
		uint32 iPosted = 0;
		pthread_mutex_lock(&pSem->objLock);
		while ( iPosted < iCount && pSem->iValue < pSem->iMaxValue ) {
			pSem->iValue++;
			iPosted++;
		}
		if ( iPosted > 0 ) {
			pthread_cond_broadcast(&pSem->objCond);
			bOk = (iPosted == iCount);
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}
// xrtCondCreate 相关处理
XXAPI xcond xrtCondCreate()
{
	xcond pCond = __xrtThreadObjAlloc(sizeof(xcond_struct));
	if ( !pCond ) return NULL;
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeConditionVariable(&pCond->objCond);
	#else
		if ( !__xrtThreadInitMonotonicCond(&pCond->objCond) ) {
			__xrtThreadObjFree(pCond);
			return NULL;
		}
	#endif
	
	return pCond;
}
// 销毁条件变量
XXAPI void xrtCondDestroy(xcond pCond)
{
	if ( pCond ) {
		xrtCondUnit(pCond);
		__xrtThreadObjFree(pCond);
	}
}
// 初始化条件变量
XXAPI void xrtCondInit(xcond pCond)
{
	if ( !pCond ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeConditionVariable(&pCond->objCond);
	#else
		if ( !__xrtThreadInitMonotonicCond(&pCond->objCond) ) {
			memset(&pCond->objCond, 0, sizeof(pCond->objCond));
		}
	#endif
}
// 释放条件变量
XXAPI void xrtCondUnit(xcond pCond)
{
	if ( !pCond ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows CONDITION_VARIABLE 不需要显式销毁
		memset(&pCond->objCond, 0, sizeof(pCond->objCond));
	#else
		pthread_cond_destroy(&pCond->objCond);
		memset(&pCond->objCond, 0, sizeof(pCond->objCond));
	#endif
}
// 等待条件变量
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex)
{
	if ( !pCond || !pMutex ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		SleepConditionVariableSRW(&pCond->objCond, 
			&pMutex->objLock, INFINITE, 0);
		pMutex->iOwnerThreadId = GetCurrentThreadId();
	#else
		pthread_cond_wait(&pCond->objCond, 
			(pthread_mutex_t*)&pMutex->objLock);
	#endif
}
// 等待条件变量（带超时）
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout)
{
	if ( !pCond || !pMutex ) {
		return XRT_WAIT_ERROR;
	}
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		if ( SleepConditionVariableSRW(&pCond->objCond, 
				&pMutex->objLock, iTimeout, 0) ) {
			pMutex->iOwnerThreadId = GetCurrentThreadId();
			return XRT_WAIT_OK;
		}
		pMutex->iOwnerThreadId = GetCurrentThreadId();
		if ( GetLastError() == ERROR_TIMEOUT ) {
			return XRT_WAIT_TIMEOUT;
		}
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) {
			return XRT_WAIT_ERROR;
		}
		int ret = pthread_cond_timedwait(&pCond->objCond, 
			(pthread_mutex_t*)&pMutex->objLock, &ts);
		if ( ret == 0 ) return XRT_WAIT_OK;
		if ( ret == ETIMEDOUT ) return XRT_WAIT_TIMEOUT;
		return XRT_WAIT_ERROR;
	#endif
}
// 唤醒一个等待的线程
XXAPI void xrtCondSignal(xcond pCond)
{
	if ( !pCond ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pCond->objCond);
	#else
		pthread_cond_signal(&pCond->objCond);
	#endif
}
// 唤醒所有等待的线程
XXAPI void xrtCondBroadcast(xcond pCond)
{
	if ( !pCond ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pCond->objCond);
	#else
		pthread_cond_broadcast(&pCond->objCond);
	#endif
}
/* ================================ 读写锁实现 ================================ */
// 内部函数：__xrtRWLockStateLock
static inline void __xrtRWLockStateLock(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pRWLock->objStateLock);
	#else
		pthread_mutex_lock(&pRWLock->objStateLock);
	#endif
}
// 内部函数：__xrtRWLockStateUnlock
static inline void __xrtRWLockStateUnlock(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pRWLock->objStateLock);
	#else
		pthread_mutex_unlock(&pRWLock->objStateLock);
	#endif
}
// 内部函数：__xrtRWLockWaitReader
static inline void __xrtRWLockWaitReader(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		SleepConditionVariableCS(&pRWLock->objReadCond, &pRWLock->objStateLock, INFINITE);
	#else
		pthread_cond_wait(&pRWLock->objReadCond, &pRWLock->objStateLock);
	#endif
}
// 内部函数：__xrtRWLockWaitWriter
static inline void __xrtRWLockWaitWriter(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		SleepConditionVariableCS(&pRWLock->objWriteCond, &pRWLock->objStateLock, INFINITE);
	#else
		pthread_cond_wait(&pRWLock->objWriteCond, &pRWLock->objStateLock);
	#endif
}
// 内部函数：__xrtRWLockSignalWriter
static inline void __xrtRWLockSignalWriter(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pRWLock->objWriteCond);
	#else
		pthread_cond_signal(&pRWLock->objWriteCond);
	#endif
}
// 内部函数：__xrtRWLockBroadcastReaders
static inline void __xrtRWLockBroadcastReaders(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pRWLock->objReadCond);
	#else
		pthread_cond_broadcast(&pRWLock->objReadCond);
	#endif
}
// 创建读写锁
XXAPI xrwlock xrtRWLockCreate()
{
	xrwlock pRWLock = __xrtThreadObjAlloc(sizeof(xrwlock_struct));
	if ( !pRWLock ) return NULL;
	xrtRWLockInit(pRWLock);
	return pRWLock;
}
// 销毁读写锁
XXAPI void xrtRWLockDestroy(xrwlock pRWLock)
{
	if ( pRWLock ) {
		xrtRWLockUnit(pRWLock);
		__xrtThreadObjFree(pRWLock);
	}
}
// 初始化读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockInit(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	memset(pRWLock, 0, sizeof(*pRWLock));
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pRWLock->objStateLock);
		InitializeConditionVariable(&pRWLock->objReadCond);
		InitializeConditionVariable(&pRWLock->objWriteCond);
	#else
		pthread_mutex_init(&pRWLock->objStateLock, NULL);
		pthread_cond_init(&pRWLock->objReadCond, NULL);
		pthread_cond_init(&pRWLock->objWriteCond, NULL);
	#endif
}
// 释放读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockUnit(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection(&pRWLock->objStateLock);
		memset(pRWLock, 0, sizeof(*pRWLock));
	#else
		pthread_cond_destroy(&pRWLock->objWriteCond);
		pthread_cond_destroy(&pRWLock->objReadCond);
		pthread_mutex_destroy(&pRWLock->objStateLock);
		memset(pRWLock, 0, sizeof(*pRWLock));
	#endif
}
// 获取读锁（阻塞）
XXAPI void xrtRWLockReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	__xrtRWLockStateLock(pRWLock);
	pRWLock->iWaitingReaderCount++;
	while ( pRWLock->bWriterLocked || (pRWLock->iWaitingWriterCount > 0) ) {
		__xrtRWLockWaitReader(pRWLock);
	}
	pRWLock->iWaitingReaderCount--;
	pRWLock->iReaderCount++;
	__xrtRWLockStateUnlock(pRWLock);
}
// 尝试获取读锁（非阻塞）
XXAPI bool xrtRWLockTryReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	bool bResult;
	__xrtRWLockStateLock(pRWLock);
	bResult = !pRWLock->bWriterLocked && (pRWLock->iWaitingWriterCount == 0);
	if ( bResult ) {
		pRWLock->iReaderCount++;
	}
	__xrtRWLockStateUnlock(pRWLock);
	
	return bResult != FALSE;
}
// 释放读锁
XXAPI void xrtRWLockReadUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->iReaderCount > 0 ) {
		pRWLock->iReaderCount--;
		if ( pRWLock->iReaderCount == 0 ) {
			if ( pRWLock->iWaitingWriterCount > 0 ) {
				__xrtRWLockSignalWriter(pRWLock);
			} else if ( pRWLock->iWaitingReaderCount > 0 ) {
				__xrtRWLockBroadcastReaders(pRWLock);
			}
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}
// 获取写锁（阻塞）
XXAPI void xrtRWLockWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	__xrtRWLockStateLock(pRWLock);
	pRWLock->iWaitingWriterCount++;
	while ( pRWLock->bWriterLocked || (pRWLock->iReaderCount > 0) ) {
		__xrtRWLockWaitWriter(pRWLock);
	}
	pRWLock->iWaitingWriterCount--;
	pRWLock->bWriterLocked = TRUE;
	__xrtRWLockStateUnlock(pRWLock);
}
// 尝试获取写锁（非阻塞）
XXAPI bool xrtRWLockTryWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	bool bResult;
	__xrtRWLockStateLock(pRWLock);
	bResult = !pRWLock->bWriterLocked && (pRWLock->iReaderCount == 0);
	if ( bResult ) {
		pRWLock->bWriterLocked = TRUE;
	}
	__xrtRWLockStateUnlock(pRWLock);
	
	return bResult != FALSE;
}
// 释放写锁
XXAPI void xrtRWLockWriteUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->bWriterLocked ) {
		pRWLock->bWriterLocked = FALSE;
		if ( pRWLock->iWaitingWriterCount > 0 ) {
			__xrtRWLockSignalWriter(pRWLock);
		} else if ( pRWLock->iWaitingReaderCount > 0 ) {
			__xrtRWLockBroadcastReaders(pRWLock);
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}
// 写锁降级为读锁（原子保留锁状态）
XXAPI void xrtRWLockDowngrade(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->bWriterLocked ) {
		pRWLock->bWriterLocked = FALSE;
		pRWLock->iReaderCount++;
		if ( (pRWLock->iWaitingWriterCount == 0) && (pRWLock->iWaitingReaderCount > 0) ) {
			__xrtRWLockBroadcastReaders(pRWLock);
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}
// 读锁升级为写锁（原子转换，返回 FALSE 表示锁状态无效）
XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->iReaderCount == 0 ) {
		__xrtRWLockStateUnlock(pRWLock);
		return FALSE;
	}
	pRWLock->iWaitingWriterCount++;
	pRWLock->iReaderCount--;
	while ( pRWLock->bWriterLocked || (pRWLock->iReaderCount > 0) ) {
		__xrtRWLockWaitWriter(pRWLock);
	}
	pRWLock->iWaitingWriterCount--;
	pRWLock->bWriterLocked = TRUE;
	__xrtRWLockStateUnlock(pRWLock);
	return TRUE;
}
#endif
#ifndef XRT_NO_QUEUE

// ========================================
// File: D:/git/xrt/lib/queue.h
// ========================================

#ifndef __XRT_QUEUE_MAX_CAPACITY
	#define __XRT_QUEUE_MAX_CAPACITY (1u << 30)
#endif
// 内部函数：加载队列原子 32 位值
static inline uint32 __xrtQueueAtomicLoad32(const volatile uint32* pValue)
{
	return __xrtAtomicLoadU32(pValue);
}
// 内部函数：保存队列原子 32 位值
static inline void __xrtQueueAtomicStore32(volatile uint32* pValue, uint32 iValue)
{
	__xrtAtomicStoreU32(pValue, iValue);
}
// 内部函数：加载队列原子 64 位值
static inline uint64 __xrtQueueAtomicLoad64(const volatile uint64* pValue)
{
	return (uint64)__xrtAtomicLoad64((const volatile int64*)pValue);
}
// 内部函数：保存队列原子 64 位值
static inline void __xrtQueueAtomicStore64(volatile uint64* pValue, uint64 iValue)
{
	__xrtAtomicStore64((volatile int64*)pValue, (int64)iValue);
}
// 内部函数：比较并交换队列原子 64 位值
static inline bool __xrtQueueAtomicCAS64(volatile uint64* pValue, uint64 iExpected, uint64 iDesired)
{
	return (uint64)__xrtAtomicCompareExchange64((volatile int64*)pValue, (int64)iDesired, (int64)iExpected) == iExpected;
}
// 内部函数：将队列容量向上对齐到 2 次幂
static inline uint32 __xrtQueueRoundUpPow2(uint32 iCapacity)
{
	if ( iCapacity == 0 || iCapacity > __XRT_QUEUE_MAX_CAPACITY ) {
		return 0;
	}
	iCapacity--;
	iCapacity |= (iCapacity >> 1);
	iCapacity |= (iCapacity >> 2);
	iCapacity |= (iCapacity >> 4);
	iCapacity |= (iCapacity >> 8);
	iCapacity |= (iCapacity >> 16);
	iCapacity++;
	if ( iCapacity == 0 || iCapacity > __XRT_QUEUE_MAX_CAPACITY ) {
		return 0;
	}
	return iCapacity;
}
// 内部函数：解析队列容量
static inline bool __xrtQueueResolveCapacity(const xqueue_config* pCfg, uint32* pCapacity)
{
	uint32 iCapacity;
	if ( pCapacity == NULL || pCfg == NULL ) {
		return FALSE;
	}
	iCapacity = __xrtQueueRoundUpPow2(pCfg->iCapacity);
	if ( iCapacity == 0 ) {
		xrtSetError("invalid queue capacity.", FALSE);
		return FALSE;
	}
	*pCapacity = iCapacity;
	return TRUE;
}
// 内部函数：获取 SPSC 队列元素数量
static inline uint32 __xrtQueueSPSCCount(const xspscq pQueue)
{
	uint32 iHead;
	uint32 iTail;
	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return 0;
	}
	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	return (uint32)(iTail - iHead);
}
// 内部函数：获取 MPSC 队列元素数量
static inline uint32 __xrtQueueMPSCCount(const xmpscq pQueue)
{
	uint64 iHead;
	uint64 iTail;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return 0;
	}
	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
	if ( iTail <= iHead ) {
		return 0;
	}
	if ( (iTail - iHead) > 0xffffffffULL ) {
		return 0xffffffffu;
	}
	return (uint32)(iTail - iHead);
}
// 内部函数：获取 MPMC 队列元素数量
static inline uint32 __xrtQueueMPMCCount(const xmpmcq pQueue)
{
	uint64 iHead;
	uint64 iTail;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return 0;
	}
	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
	if ( iTail <= iHead ) {
		return 0;
	}
	if ( (iTail - iHead) > 0xffffffffULL ) {
		return 0xffffffffu;
	}
	return (uint32)(iTail - iHead);
}
#ifndef XRT_NO_QUEUE_WAIT
// 内部函数：获取等待队列当前毫秒时间
static inline uint64 __xrtQueueWaitNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
			return 0;
		}
		return ((uint64)tNow.tv_sec * 1000u) + ((uint64)tNow.tv_nsec / 1000000u);
	#endif
}
// 内部函数：加载等待队列长整型状态
static inline long __xrtQueueWaitLoadLong(const volatile long* pValue)
{
	return __xrtAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}
// 内部函数：累加等待队列长整型状态
static inline void __xrtQueueWaitAddLong(volatile long* pValue, long iDelta)
{
	(void)__xrtAtomicAddFetch32(pValue, iDelta);
}
// 内部函数：判断等待队列是否已关闭且排空
static inline bool __xrtMPSCQWaitIsClosedAndDrained(const xmpscqwait pQueue)
{
	return pQueue != NULL && xrtQueueIsClosed(&pQueue->tQueue.tBase) && xrtQueueIsDrained(&pQueue->tQueue.tBase);
}
// 内部函数：唤醒全部等待中的消费者
static inline void __xrtMPSCQWaitWakeAll(xmpscqwait pQueue)
{
	long iWaiters;
	uint32 iWakeCount;
	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return;
	}
	iWaiters = __xrtQueueWaitLoadLong(&pQueue->iWaiters);
	if ( iWaiters <= 0 ) {
		return;
	}
	iWakeCount = (iWaiters > (long)0xffffffffu) ? 0xffffffffu : (uint32)iWaiters;
	(void)xrtSemPostMultiple(pQueue->hItems, iWakeCount);
}
// 内部函数：__xrtMPSCQWaitPopWithToken
static xqueue_result __xrtMPSCQWaitPopWithToken(xmpscqwait pQueue, ptr* ppItem)
{
	xqueue_result iRet;
	bool bWake = FALSE;
	if ( pQueue == NULL || pQueue->hPopLock == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	xrtMutexLock(pQueue->hPopLock);
	iRet = xrtMPSCQTryPop(&pQueue->tQueue, ppItem);
	bWake = __xrtMPSCQWaitIsClosedAndDrained(pQueue);
	xrtMutexUnlock(pQueue->hPopLock);
	if ( bWake ) {
		__xrtMPSCQWaitWakeAll(pQueue);
	}
	if ( iRet == XQUEUE_OK || iRet == XQUEUE_CLOSED || iRet == XQUEUE_ERROR ) {
		return iRet;
	}
	return __xrtMPSCQWaitIsClosedAndDrained(pQueue) ? XQUEUE_CLOSED : XQUEUE_EMPTY;
}
// 内部函数：__xrtMPSCQWaitPopCore
static xqueue_result __xrtMPSCQWaitPopCore(xmpscqwait pQueue, ptr* ppItem, bool bInfinite, uint32 iTimeoutMs)
{
	uint64 iDeadlineMs = 0;
	if ( pQueue == NULL || pQueue->hItems == NULL || pQueue->hPopLock == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	*ppItem = NULL;
	if ( !bInfinite ) {
		iDeadlineMs = __xrtQueueWaitNowMs() + (uint64)iTimeoutMs;
	}
	for ( ;; ) {
		if ( xrtSemTryWait(pQueue->hItems) ) {
			return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
		}
		else {
			if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
				return XQUEUE_CLOSED;
			}
			__xrtQueueWaitAddLong(&pQueue->iWaiters, 1);
			if ( xrtSemTryWait(pQueue->hItems) ) {
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
			}
			else if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return XQUEUE_CLOSED;
			}
			else if ( bInfinite ) {
				xrtSemWait(pQueue->hItems);
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
			}
			else {
				uint64 iNowMs = __xrtQueueWaitNowMs();
				uint64 iRemainMs;
				int iWaitRet;
				if ( iNowMs >= iDeadlineMs ) {
					__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
					return XQUEUE_TIMEOUT;
				}
				iRemainMs = iDeadlineMs - iNowMs;
				if ( iRemainMs > 0xffffffffu ) {
					iRemainMs = 0xffffffffu;
				}
				iWaitRet = xrtSemWaitTimeout(pQueue->hItems, (uint32)iRemainMs);
				__xrtQueueWaitAddLong(&pQueue->iWaiters, -1);
				if ( iWaitRet == XRT_WAIT_OK ) {
					return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
				}
				else if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
					if ( __xrtMPSCQWaitIsClosedAndDrained(pQueue) ) {
						return XQUEUE_CLOSED;
					}
					return XQUEUE_TIMEOUT;
				}
				else {
					return XQUEUE_ERROR;
				}
			}
		}
	}
}
#endif
// xrtSPSCQInit 相关处理
XXAPI bool xrtSPSCQInit(xspscq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	ptr* arrItems;
	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xspscq_struct));
		return FALSE;
	}
	arrItems = (ptr*)xrtCalloc(iCapacity, sizeof(ptr));
	if ( arrItems == NULL ) {
		memset(pQueue, 0, sizeof(xspscq_struct));
		return FALSE;
	}
	memset(pQueue, 0, sizeof(xspscq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_SPSC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrItems = arrItems;
	return TRUE;
}
// xrtSPSCQUnit 相关处理
XXAPI void xrtSPSCQUnit(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrItems != NULL ) {
		xrtFree(pQueue->arrItems);
	}
	memset(pQueue, 0, sizeof(xspscq_struct));
}
// xrtSPSCQCreate 相关处理
XXAPI xspscq xrtSPSCQCreate(const xqueue_config* pCfg)
{
	xspscq pQueue = (xspscq)xrtCalloc(1, sizeof(xspscq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtSPSCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}
// xrtSPSCQDestroy 相关处理
XXAPI void xrtSPSCQDestroy(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtSPSCQUnit(pQueue);
	xrtFree(pQueue);
}
// xrtSPSCQTryPush 相关处理
XXAPI xqueue_result xrtSPSCQTryPush(xspscq pQueue, ptr pItem)
{
	uint32 iHead;
	uint32 iTail;
	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return XQUEUE_ERROR;
	}
	if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
		return XQUEUE_CLOSED;
	}
	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	if ( (uint32)(iTail - iHead) >= pQueue->iCapacity ) {
		return XQUEUE_FULL;
	}
	pQueue->arrItems[iTail & pQueue->iMask] = pItem;
	__xrtQueueAtomicStore32(&pQueue->iTail, iTail + 1u);
	return XQUEUE_OK;
}
// xrtSPSCQTryPop 相关处理
XXAPI xqueue_result xrtSPSCQTryPop(xspscq pQueue, ptr* ppItem)
{
	uint32 iHead;
	uint32 iTail;
	ptr pItem;
	if ( pQueue == NULL || pQueue->arrItems == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	iHead = __xrtQueueAtomicLoad32(&pQueue->iHead);
	iTail = __xrtQueueAtomicLoad32(&pQueue->iTail);
	if ( iHead == iTail ) {
		*ppItem = NULL;
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}
		return XQUEUE_EMPTY;
	}
	pItem = pQueue->arrItems[iHead & pQueue->iMask];
	pQueue->arrItems[iHead & pQueue->iMask] = NULL;
	__xrtQueueAtomicStore32(&pQueue->iHead, iHead + 1u);
	*ppItem = pItem;
	return XQUEUE_OK;
}
// xrtSPSCQApproxCount 相关处理
XXAPI uint32 xrtSPSCQApproxCount(xspscq pQueue)
{
	return __xrtQueueSPSCCount(pQueue);
}
// xrtSPSCQClose 相关处理
XXAPI void xrtSPSCQClose(xspscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}
// xrtSPSCQDrain 相关处理
XXAPI uint32 xrtSPSCQDrain(xspscq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;
	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}
	for ( ;; ) {
		iRet = xrtSPSCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}
	return iCount;
}
// xrtSPSCQReset 相关处理
XXAPI bool xrtSPSCQReset(xspscq pQueue)
{
	if ( pQueue == NULL || pQueue->arrItems == NULL ) {
		return FALSE;
	}
	if ( xrtSPSCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}
	memset(pQueue->arrItems, 0, sizeof(ptr) * pQueue->iCapacity);
	__xrtQueueAtomicStore32(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore32(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}
// xrtMPSCQInit 相关处理
XXAPI bool xrtMPSCQInit(xmpscq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	xmpscq_slot* arrSlots;
	uint32 i;
	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xmpscq_struct));
		return FALSE;
	}
	arrSlots = (xmpscq_slot*)xrtCalloc(iCapacity, sizeof(xmpscq_slot));
	if ( arrSlots == NULL ) {
		memset(pQueue, 0, sizeof(xmpscq_struct));
		return FALSE;
	}
	memset(pQueue, 0, sizeof(xmpscq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_MPSC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrSlots = arrSlots;
	for ( i = 0; i < iCapacity; ++i ) {
		pQueue->arrSlots[i].iSeq = (uint64)i;
		pQueue->arrSlots[i].pItem = NULL;
	}
	return TRUE;
}
// xrtMPSCQUnit 相关处理
XXAPI void xrtMPSCQUnit(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrSlots != NULL ) {
		xrtFree(pQueue->arrSlots);
	}
	memset(pQueue, 0, sizeof(xmpscq_struct));
}
// xrtMPSCQCreate 相关处理
XXAPI xmpscq xrtMPSCQCreate(const xqueue_config* pCfg)
{
	xmpscq pQueue = (xmpscq)xrtCalloc(1, sizeof(xmpscq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPSCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}
// xrtMPSCQDestroy 相关处理
XXAPI void xrtMPSCQDestroy(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPSCQUnit(pQueue);
	xrtFree(pQueue);
}
// xrtMPSCQTryPush 相关处理
XXAPI xqueue_result xrtMPSCQTryPush(xmpscq pQueue, ptr pItem)
{
	uint64 iTail;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return XQUEUE_ERROR;
	}
	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}
		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		pSlot = &pQueue->arrSlots[(uint32)(iTail & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)iTail;
		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + 1u) ) {
				pSlot->pItem = pItem;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iTail + 1u);
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}
		if ( iDiff < 0 ) {
			return XQUEUE_FULL;
		}
		xrtThreadYield();
	}
}
// xrtMPSCQTryPop 相关处理
XXAPI xqueue_result xrtMPSCQTryPop(xmpscq pQueue, ptr* ppItem)
{
	uint64 iHead;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	uint64 iTail;
	if ( pQueue == NULL || pQueue->arrSlots == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	pSlot = &pQueue->arrSlots[(uint32)(iHead & pQueue->iMask)];
	iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
	iDiff = (int64)iSeq - (int64)(iHead + 1u);
	if ( iDiff == 0 ) {
		*ppItem = pSlot->pItem;
		pSlot->pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->iHead, iHead + 1u);
		__xrtQueueAtomicStore64(&pSlot->iSeq, iHead + pQueue->iCapacity);
		return XQUEUE_OK;
	}
	*ppItem = NULL;
	if ( iDiff < 0 ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
			if ( iTail == iHead ) {
				return XQUEUE_CLOSED;
			}
		}
		return XQUEUE_EMPTY;
	}
	return XQUEUE_EMPTY;
}
// xrtMPSCQPushBatch 相关处理
XXAPI uint32 xrtMPSCQPushBatch(xmpscq pQueue, ptr* arrItems, uint32 iCount)
{
	uint64 iTail;
	uint32 iReady;
	uint32 i;
	uint64 iPos;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || arrItems == NULL || iCount == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}
	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return 0u;
		}
		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		iReady = 0u;
		iDiff = 0;
		while ( iReady < iCount ) {
			iPos = iTail + iReady;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)iPos;
			if ( iDiff == 0 ) {
				iReady++;
				continue;
			}
			break;
		}
		if ( iReady == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}
		if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + iReady) ) {
			for ( i = 0; i < iReady; ++i ) {
				iPos = iTail + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				pSlot->pItem = arrItems[i];
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + 1u);
			}
			return iReady;
		}
		xrtThreadYield();
	}
}
// xrtMPSCQPopBatch 相关处理
XXAPI uint32 xrtMPSCQPopBatch(xmpscq pQueue, ptr* arrItems, uint32 iCap)
{
	uint64 iHead;
	uint32 iPopped;
	uint32 i;
	uint64 iPos;
	xmpscq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || arrItems == NULL || iCap == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}
	iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
	iPopped = 0u;
	while ( iPopped < iCap ) {
		iPos = iHead + iPopped;
		pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)(iPos + 1u);
		if ( iDiff != 0 ) {
			break;
		}
		iPopped++;
	}
	for ( i = 0; i < iPopped; ++i ) {
		iPos = iHead + i;
		pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
		arrItems[i] = pSlot->pItem;
		pSlot->pItem = NULL;
		__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + pQueue->iCapacity);
	}
	if ( iPopped != 0u ) {
		__xrtQueueAtomicStore64(&pQueue->iHead, iHead + iPopped);
	}
	return iPopped;
}
// xrtMPSCQApproxCount 相关处理
XXAPI uint32 xrtMPSCQApproxCount(xmpscq pQueue)
{
	return __xrtQueueMPSCCount(pQueue);
}
// xrtMPSCQClose 相关处理
XXAPI void xrtMPSCQClose(xmpscq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}
// xrtMPSCQDrain 相关处理
XXAPI uint32 xrtMPSCQDrain(xmpscq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;
	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}
	for ( ;; ) {
		iRet = xrtMPSCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}
	return iCount;
}
// xrtMPSCQReset 相关处理
XXAPI bool xrtMPSCQReset(xmpscq pQueue)
{
	uint32 i;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return FALSE;
	}
	if ( xrtMPSCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}
	for ( i = 0; i < pQueue->iCapacity; ++i ) {
		pQueue->arrSlots[i].pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->arrSlots[i].iSeq, (uint64)i);
	}
	__xrtQueueAtomicStore64(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore64(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}
// xrtMPMCQInit 相关处理
XXAPI bool xrtMPMCQInit(xmpmcq pQueue, const xqueue_config* pCfg)
{
	uint32 iCapacity;
	xmpmcq_slot* arrSlots;
	uint32 i;
	if ( pQueue == NULL ) {
		return FALSE;
	}
	if ( !__xrtQueueResolveCapacity(pCfg, &iCapacity) ) {
		memset(pQueue, 0, sizeof(xmpmcq_struct));
		return FALSE;
	}
	arrSlots = (xmpmcq_slot*)xrtCalloc(iCapacity, sizeof(xmpmcq_slot));
	if ( arrSlots == NULL ) {
		memset(pQueue, 0, sizeof(xmpmcq_struct));
		return FALSE;
	}
	memset(pQueue, 0, sizeof(xmpmcq_struct));
	pQueue->tBase.iKind = XQUEUE_KIND_MPMC;
	pQueue->tBase.bClosed = 0;
	pQueue->iCapacity = iCapacity;
	pQueue->iMask = iCapacity - 1u;
	pQueue->arrSlots = arrSlots;
	for ( i = 0; i < iCapacity; ++i ) {
		pQueue->arrSlots[i].iSeq = (uint64)i;
		pQueue->arrSlots[i].pItem = NULL;
	}
	return TRUE;
}
// xrtMPMCQUnit 相关处理
XXAPI void xrtMPMCQUnit(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->arrSlots != NULL ) {
		xrtFree(pQueue->arrSlots);
	}
	memset(pQueue, 0, sizeof(xmpmcq_struct));
}
// xrtMPMCQCreate 相关处理
XXAPI xmpmcq xrtMPMCQCreate(const xqueue_config* pCfg)
{
	xmpmcq pQueue = (xmpmcq)xrtCalloc(1, sizeof(xmpmcq_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPMCQInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}
// xrtMPMCQDestroy 相关处理
XXAPI void xrtMPMCQDestroy(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPMCQUnit(pQueue);
	xrtFree(pQueue);
}
// xrtMPMCQTryPush 相关处理
XXAPI xqueue_result xrtMPMCQTryPush(xmpmcq pQueue, ptr pItem)
{
	uint64 iTail;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return XQUEUE_ERROR;
	}
	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return XQUEUE_CLOSED;
		}
		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		pSlot = &pQueue->arrSlots[(uint32)(iTail & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)iTail;
		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + 1u) ) {
				pSlot->pItem = pItem;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iTail + 1u);
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}
		if ( iDiff < 0 ) {
			return XQUEUE_FULL;
		}
		xrtThreadYield();
	}
}
// xrtMPMCQTryPop 相关处理
XXAPI xqueue_result xrtMPMCQTryPop(xmpmcq pQueue, ptr* ppItem)
{
	uint64 iHead;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	uint64 iTail;
	ptr pItem;
	if ( pQueue == NULL || pQueue->arrSlots == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	for ( ;; ) {
		iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
		pSlot = &pQueue->arrSlots[(uint32)(iHead & pQueue->iMask)];
		iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
		iDiff = (int64)iSeq - (int64)(iHead + 1u);
		if ( iDiff == 0 ) {
			if ( __xrtQueueAtomicCAS64(&pQueue->iHead, iHead, iHead + 1u) ) {
				pItem = pSlot->pItem;
				pSlot->pItem = NULL;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iHead + pQueue->iCapacity);
				*ppItem = pItem;
				return XQUEUE_OK;
			}
			xrtThreadYield();
			continue;
		}
		*ppItem = NULL;
		if ( iDiff < 0 ) {
			if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
				iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
				if ( iTail == iHead ) {
					return XQUEUE_CLOSED;
				}
			}
			return XQUEUE_EMPTY;
		}
		xrtThreadYield();
	}
}
// xrtMPMCQPushBatch 相关处理
XXAPI uint32 xrtMPMCQPushBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCount)
{
	uint64 iTail;
	uint32 iReady;
	uint32 i;
	uint64 iPos;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || arrItems == NULL || iCount == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}
	for ( ;; ) {
		if ( __xrtQueueAtomicLoad32(&pQueue->tBase.bClosed) != 0 ) {
			return 0u;
		}
		iTail = __xrtQueueAtomicLoad64(&pQueue->iTail);
		iReady = 0u;
		iDiff = 0;
		while ( iReady < iCount ) {
			iPos = iTail + iReady;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)iPos;
			if ( iDiff == 0 ) {
				iReady++;
				continue;
			}
			break;
		}
		if ( iReady == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}
		if ( __xrtQueueAtomicCAS64(&pQueue->iTail, iTail, iTail + iReady) ) {
			for ( i = 0; i < iReady; ++i ) {
				iPos = iTail + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				pSlot->pItem = arrItems[i];
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + 1u);
			}
			return iReady;
		}
		xrtThreadYield();
	}
}
// xrtMPMCQPopBatch 相关处理
XXAPI uint32 xrtMPMCQPopBatch(xmpmcq pQueue, ptr* arrItems, uint32 iCap)
{
	uint64 iHead;
	uint32 iPopped;
	uint32 i;
	uint64 iPos;
	xmpmcq_slot* pSlot;
	uint64 iSeq;
	int64 iDiff;
	if ( pQueue == NULL || arrItems == NULL || iCap == 0u ) {
		return 0u;
	}
	if ( pQueue->arrSlots == NULL ) {
		return 0u;
	}
	for ( ;; ) {
		iHead = __xrtQueueAtomicLoad64(&pQueue->iHead);
		iPopped = 0u;
		iDiff = 0;
		while ( iPopped < iCap ) {
			iPos = iHead + iPopped;
			pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
			iSeq = __xrtQueueAtomicLoad64(&pSlot->iSeq);
			iDiff = (int64)iSeq - (int64)(iPos + 1u);
			if ( iDiff == 0 ) {
				iPopped++;
				continue;
			}
			break;
		}
		if ( iPopped == 0u ) {
			if ( iDiff < 0 ) {
				return 0u;
			}
			xrtThreadYield();
			continue;
		}
		if ( __xrtQueueAtomicCAS64(&pQueue->iHead, iHead, iHead + iPopped) ) {
			for ( i = 0; i < iPopped; ++i ) {
				iPos = iHead + i;
				pSlot = &pQueue->arrSlots[(uint32)(iPos & pQueue->iMask)];
				arrItems[i] = pSlot->pItem;
				pSlot->pItem = NULL;
				__xrtQueueAtomicStore64(&pSlot->iSeq, iPos + pQueue->iCapacity);
			}
			return iPopped;
		}
		xrtThreadYield();
	}
}
// xrtMPMCQApproxCount 相关处理
XXAPI uint32 xrtMPMCQApproxCount(xmpmcq pQueue)
{
	return __xrtQueueMPMCCount(pQueue);
}
// xrtMPMCQClose 相关处理
XXAPI void xrtMPMCQClose(xmpmcq pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 1u);
}
// xrtMPMCQDrain 相关处理
XXAPI uint32 xrtMPMCQDrain(xmpmcq pQueue, xqueue_drain_fn procDrain, ptr pUserData)
{
	uint32 iCount = 0;
	ptr pItem = NULL;
	xqueue_result iRet;
	if ( pQueue == NULL || procDrain == NULL ) {
		return 0;
	}
	for ( ;; ) {
		iRet = xrtMPMCQTryPop(pQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		procDrain(pItem, pUserData);
		iCount++;
	}
	return iCount;
}
// xrtMPMCQReset 相关处理
XXAPI bool xrtMPMCQReset(xmpmcq pQueue)
{
	uint32 i;
	if ( pQueue == NULL || pQueue->arrSlots == NULL ) {
		return FALSE;
	}
	if ( xrtMPMCQApproxCount(pQueue) != 0 ) {
		return FALSE;
	}
	for ( i = 0; i < pQueue->iCapacity; ++i ) {
		pQueue->arrSlots[i].pItem = NULL;
		__xrtQueueAtomicStore64(&pQueue->arrSlots[i].iSeq, (uint64)i);
	}
	__xrtQueueAtomicStore64(&pQueue->iHead, 0u);
	__xrtQueueAtomicStore64(&pQueue->iTail, 0u);
	__xrtQueueAtomicStore32(&pQueue->tBase.bClosed, 0u);
	return TRUE;
}
// xrtQueueIsClosed 相关处理
XXAPI bool xrtQueueIsClosed(const xqueuebase* pQueue)
{
	if ( pQueue == NULL ) {
		return FALSE;
	}
	return __xrtQueueAtomicLoad32(&pQueue->bClosed) != 0;
}
// xrtQueueIsDrained 相关处理
XXAPI bool xrtQueueIsDrained(const xqueuebase* pQueue)
{
	if ( pQueue == NULL || !xrtQueueIsClosed(pQueue) ) {
		return FALSE;
	}
	switch ( pQueue->iKind ) {
		case XQUEUE_KIND_SPSC:
			return __xrtQueueSPSCCount((xspscq)pQueue) == 0;
		case XQUEUE_KIND_MPSC:
			return __xrtQueueMPSCCount((xmpscq)pQueue) == 0;
		case XQUEUE_KIND_MPMC:
			return __xrtQueueMPMCCount((xmpmcq)pQueue) == 0;
		default:
			break;
	}
	return FALSE;
}
#ifndef XRT_NO_QUEUE_WAIT
// xrtMPSCQWaitInit 相关处理
XXAPI bool xrtMPSCQWaitInit(xmpscqwait pQueue, const xqueue_config* pCfg)
{
	if ( pQueue == NULL ) {
		return FALSE;
	}
	memset(pQueue, 0, sizeof(xmpscqwait_struct));
	if ( !xrtMPSCQInit(&pQueue->tQueue, pCfg) ) {
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		return FALSE;
	}
	pQueue->hItems = xrtSemCreate(0u, 0x7fffffffu);
	if ( pQueue->hItems == NULL ) {
		xrtMPSCQUnit(&pQueue->tQueue);
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		xrtSetError("mpsc wait queue semaphore init failed.", FALSE);
		return FALSE;
	}
	pQueue->hPopLock = xrtMutexCreate();
	if ( pQueue->hPopLock == NULL ) {
		xrtSemDestroy(pQueue->hItems);
		pQueue->hItems = NULL;
		xrtMPSCQUnit(&pQueue->tQueue);
		memset(pQueue, 0, sizeof(xmpscqwait_struct));
		xrtSetError("mpsc wait queue mutex init failed.", FALSE);
		return FALSE;
	}
	pQueue->iWaiters = 0;
	return TRUE;
}
// xrtMPSCQWaitUnit 相关处理
XXAPI void xrtMPSCQWaitUnit(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	if ( pQueue->hPopLock != NULL ) {
		xrtMutexDestroy(pQueue->hPopLock);
		pQueue->hPopLock = NULL;
	}
	if ( pQueue->hItems != NULL ) {
		xrtSemDestroy(pQueue->hItems);
		pQueue->hItems = NULL;
	}
	xrtMPSCQUnit(&pQueue->tQueue);
	memset(pQueue, 0, sizeof(xmpscqwait_struct));
}
// xrtMPSCQWaitCreate 相关处理
XXAPI xmpscqwait xrtMPSCQWaitCreate(const xqueue_config* pCfg)
{
	xmpscqwait pQueue = (xmpscqwait)xrtCalloc(1, sizeof(xmpscqwait_struct));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtMPSCQWaitInit(pQueue, pCfg) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}
// xrtMPSCQWaitDestroy 相关处理
XXAPI void xrtMPSCQWaitDestroy(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPSCQWaitUnit(pQueue);
	xrtFree(pQueue);
}
// xrtMPSCQWaitTryPush 相关处理
XXAPI xqueue_result xrtMPSCQWaitTryPush(xmpscqwait pQueue, ptr pItem)
{
	xqueue_result iRet;
	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return XQUEUE_ERROR;
	}
	iRet = xrtMPSCQTryPush(&pQueue->tQueue, pItem);
	if ( iRet != XQUEUE_OK ) {
		return iRet;
	}
	if ( !xrtSemPost(pQueue->hItems) ) {
		xrtSetError("mpsc wait queue semaphore post failed.", FALSE);
		return XQUEUE_ERROR;
	}
	return XQUEUE_OK;
}
// xrtMPSCQWaitTryPop 相关处理
XXAPI xqueue_result xrtMPSCQWaitTryPop(xmpscqwait pQueue, ptr* ppItem)
{
	if ( pQueue == NULL || pQueue->hItems == NULL || ppItem == NULL ) {
		return XQUEUE_ERROR;
	}
	*ppItem = NULL;
	if ( !xrtSemTryWait(pQueue->hItems) ) {
		return __xrtMPSCQWaitIsClosedAndDrained(pQueue) ? XQUEUE_CLOSED : XQUEUE_EMPTY;
	}
	return __xrtMPSCQWaitPopWithToken(pQueue, ppItem);
}
// xrtMPSCQWaitPop 相关处理
XXAPI xqueue_result xrtMPSCQWaitPop(xmpscqwait pQueue, ptr* ppItem)
{
	return __xrtMPSCQWaitPopCore(pQueue, ppItem, TRUE, 0u);
}
// xrtMPSCQWaitPopTimeout 相关处理
XXAPI xqueue_result xrtMPSCQWaitPopTimeout(xmpscqwait pQueue, ptr* ppItem, uint32 iTimeoutMs)
{
	return __xrtMPSCQWaitPopCore(pQueue, ppItem, FALSE, iTimeoutMs);
}
// xrtMPSCQWaitApproxCount 相关处理
XXAPI uint32 xrtMPSCQWaitApproxCount(xmpscqwait pQueue)
{
	if ( pQueue == NULL ) {
		return 0u;
	}
	return xrtMPSCQApproxCount(&pQueue->tQueue);
}
// xrtMPSCQWaitClose 相关处理
XXAPI void xrtMPSCQWaitClose(xmpscqwait pQueue)
{
	if ( pQueue == NULL || pQueue->hItems == NULL ) {
		return;
	}
	if ( __xrtAtomicCompareExchangeU32(&pQueue->tQueue.tBase.bClosed, 1u, 0u) != 0u ) {
		return;
	}
	__xrtMPSCQWaitWakeAll(pQueue);
}
#endif
#endif
#ifndef XRT_NO_COROUTINE

// ========================================
// File: D:/git/xrt/lib/coroutine.h
// ========================================


/* ================================ 协程后端自动选择 ================================ */
/*
	backend 策略说明：
	1. 单头文件仍是 XRT 的第一优先级约束之一
	2. production backend 优先使用 header 内的 inline asm
	3. 主线只保留 production backend；不再在主线内编译 archive fallback
	4. 平台支持按 ABI + 编译器家族逐步扩展，不对未验证目标做超前承诺
*/
#ifndef XRT_CO_REQUIRE_PRODUCTION_BACKEND
	#define XRT_CO_REQUIRE_PRODUCTION_BACKEND	1
#endif
#define __XRT_CO_BACKEND_TIER_PRODUCTION	2
#define __XRT_CO_BACKEND_STYLE_INLINE_ASM	2
#define __XRT_CO_BACKEND_STYLE_FIBER		3
#if defined(_MSC_VER) && !defined(__clang__) && (defined(_WIN32) || defined(_WIN64))
	#define __XRT_CO_FIBER_WIN
	#if defined(_WIN64)
		#define __XRT_CO_BACKEND_NAME	"fiber-win64-msvc"
	#else
		#define __XRT_CO_BACKEND_NAME	"fiber-win32-msvc"
	#endif
	#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
	#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_FIBER
#elif defined(__TINYC__)
	#if (defined(_WIN64)) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64_WIN
		#define __XRT_CO_BACKEND_NAME	"asm-x64-win64-tcc"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64
		#define __XRT_CO_BACKEND_NAME	"asm-x64-sysv-tcc"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#endif
#elif defined(__GNUC__) || defined(__clang__)
	#if (defined(_WIN64)) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64_WIN
		#define __XRT_CO_BACKEND_NAME	"asm-x64-win64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64
		#define __XRT_CO_BACKEND_NAME	"asm-x64-sysv"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__aarch64__)
		#define __XRT_CO_ASM_ARM64
		#define __XRT_CO_BACKEND_NAME	"asm-arm64-aapcs64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__riscv) && (__riscv_xlen == 64)
		#define __XRT_CO_ASM_RV64
		#define __XRT_CO_BACKEND_NAME	"asm-rv64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__loongarch64)
		#define __XRT_CO_ASM_LA64
		#define __XRT_CO_BACKEND_NAME	"asm-la64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#endif
#endif
#ifndef __XRT_CO_BACKEND_TIER
	#error "XRT coroutine production backend is unavailable for this target."
#endif
#if XRT_CO_REQUIRE_PRODUCTION_BACKEND && (__XRT_CO_BACKEND_TIER != __XRT_CO_BACKEND_TIER_PRODUCTION)
	#error "XRT coroutine production backend is required, but current target is not using a production backend."
#endif
#ifdef __XRT_CO_FIBER_WIN
	#define __XRT_CO_BACKEND_NEEDS_STACK_ALLOC	0
#else
	#define __XRT_CO_BACKEND_NEEDS_STACK_ALLOC	1
#endif
/* ================================ 线程级协程运行时 ================================ */
static xrtCoroRuntimeState* __xrt_co_get_runtime_from_thread(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return NULL;
	}
	return &pThreadData->tCoro;
}
// 内部函数：协程 require 线程数据相关处理
static xrtThreadData* __xrt_co_require_thread_data(bool bSetError)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL && bSetError ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
	}
	return pThreadData;
}
// 内部函数：获取协程运行时
static xrtCoroRuntimeState* __xrt_co_get_runtime()
{
	return __xrt_co_get_runtime_from_thread(xrtThreadGetCurrent());
}
// 内部函数：__xrt_co_require_runtime
static xrtCoroRuntimeState* __xrt_co_require_runtime(bool bSetError)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(bSetError);
	return __xrt_co_get_runtime_from_thread(pThreadData);
}
// 内部函数：获取协程当前
static xcoro __xrt_co_get_current()
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();
	if ( pRuntime == NULL ) {
		return NULL;
	}
	return (xcoro)pRuntime->pCurrent;
}
// 内部函数：设置协程当前
static void __xrt_co_set_current(xcoro pCo)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();
	if ( pRuntime ) {
		pRuntime->pCurrent = pCo;
	}
}
// 内部函数：检查协程所有权 tid
static bool __xrt_co_check_owner_tid(uint64 iOwnerThreadId, const char* sError)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	if ( pThreadData == NULL ) {
		return FALSE;
	}
	if ( iOwnerThreadId != 0 && pThreadData->iThreadId != iOwnerThreadId ) {
		if ( sError ) {
			xrtSetError(sError, FALSE);
		}
		return FALSE;
	}
	return TRUE;
}
// 内部函数：检查协程所有权
static bool __xrt_co_check_owner(xcoro pCo, const char* sError)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	return __xrt_co_check_owner_tid(pCo->__iOwnerThreadId, sError);
}
// 内部函数：初始化 coro 运行时线程
static void __xrtCoroRuntimeInitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	if ( pRuntime == NULL ) {
		return;
	}
	memset(pRuntime, 0, sizeof(xrtCoroRuntimeState));
	pRuntime->iOwnerThreadId = pThreadData->iThreadId;
}
// 内部函数：释放 coro 运行时线程
static void __xrtCoroRuntimeUnitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	if ( pRuntime == NULL ) {
		return;
	}
	#ifdef __XRT_CO_FIBER_WIN
		if ( (pRuntime->iFlags & XRT_CO_RUNTIME_FIBER_CONVERTED) != 0 ) {
			(void)ConvertFiberToThread();
		}
	#elif defined(__XRT_CO_ASM_X64_WIN) || defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)
		xrtFree(pRuntime->pBackendMain);
	#endif
	memset(pRuntime, 0, sizeof(xrtCoroRuntimeState));
}
/* ================================ 协程生命周期辅助 ================================ */
#ifndef MAP_ANONYMOUS
	#ifdef MAP_ANON
		#define MAP_ANONYMOUS MAP_ANON
	#endif
#endif
#define __XRT_CO_FLAG_CANCEL_REQUESTED	0x00000001u
#define __XRT_CO_FLAG_CANCELLED			0x00000002u
#define __XRT_CO_FLAG_CLOSE_REQUESTED	0x00000004u
#define __XRT_CO_FLAG_REAP_PENDING		0x00000008u
#define __XRT_CO_FLAG_JOIN_PINNED		0x00000010u
#define __XRT_CO_FLAG_READY_QUEUED		0x00000020u
#define __XRT_CO_FLAG_TIMER_QUEUED		0x00000040u
#define __XRT_CO_FLAG_STARTED			0x00000080u
#define __XRT_CO_FLAG_IN_CLEANUP		0x00000100u
#define __XRT_CO_WAIT_NONE				0u
#define __XRT_CO_WAIT_TIMER				1u
#define __XRT_CO_WAIT_JOIN				2u
#define __XRT_CO_WAIT_EVENT				3u
#define __XRT_CO_WAIT_RESULT_NONE		0u
#define __XRT_CO_WAIT_RESULT_SIGNAL		1u
#define __XRT_CO_WAIT_RESULT_TIMEOUT	2u
// 内部函数：__xrt_co_is_terminal
static bool __xrt_co_is_terminal(xcoro pCo)
{
	return pCo && pCo->iState == XRT_CO_DEAD;
}
// 内部函数：__xrt_co_is_cancel_requested_flag
static bool __xrt_co_is_cancel_requested_flag(xcoro pCo)
{
	return pCo && ((pCo->__iFlags & __XRT_CO_FLAG_CANCEL_REQUESTED) != 0);
}
static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_release_dead(xcoro pCo);
static bool __xrt_co_sched_run_until(xcosched* pSched, xcoro pTarget);
// 内部函数：等待协程 clear 状态
static void __xrt_co_clear_wait_state(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}
	pCo->__pWaitObject = NULL;
	pCo->__iWaitKind = __XRT_CO_WAIT_NONE;
}
// 内部函数：__xrt_co_wait_link_clear
static void __xrt_co_wait_link_clear(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}
	pCo->__pWaitPrev = NULL;
	pCo->__pWaitNext = NULL;
}
// 内部函数：__xrt_co_wait_queue_push_tail
static void __xrt_co_wait_queue_push_tail(xcoro* ppHead, xcoro* ppTail, xcoro pCo)
{
	if ( ppHead == NULL || ppTail == NULL || pCo == NULL ) {
		return;
	}
	__xrt_co_wait_link_clear(pCo);
	if ( *ppTail != NULL ) {
		(*ppTail)->__pWaitNext = pCo;
		pCo->__pWaitPrev = *ppTail;
	}
	else {
		*ppHead = pCo;
	}
	*ppTail = pCo;
}
// 内部函数：删除协程 wait 队列
static void __xrt_co_wait_queue_remove(xcoro* ppHead, xcoro* ppTail, xcoro pCo)
{
	if ( ppHead == NULL || ppTail == NULL || pCo == NULL ) {
		return;
	}
	if ( pCo->__pWaitPrev != NULL ) {
		pCo->__pWaitPrev->__pWaitNext = pCo->__pWaitNext;
	}
	else if ( *ppHead == pCo ) {
		*ppHead = pCo->__pWaitNext;
	}
	if ( pCo->__pWaitNext != NULL ) {
		pCo->__pWaitNext->__pWaitPrev = pCo->__pWaitPrev;
	}
	else if ( *ppTail == pCo ) {
		*ppTail = pCo->__pWaitPrev;
	}
	__xrt_co_wait_link_clear(pCo);
}
// 内部函数：__xrt_co_wait_queue_pop_head
static xcoro __xrt_co_wait_queue_pop_head(xcoro* ppHead, xcoro* ppTail)
{
	xcoro pHead = NULL;
	if ( ppHead == NULL || ppTail == NULL ) {
		return NULL;
	}
	pHead = *ppHead;
	if ( pHead != NULL ) {
		__xrt_co_wait_queue_remove(ppHead, ppTail, pHead);
	}
	return pHead;
}
// 内部函数：__xrt_co_join_pin_acquire
static void __xrt_co_join_pin_acquire(xcoro pTarget)
{
	if ( pTarget == NULL ) {
		return;
	}
	pTarget->__iJoinRefCount++;
	pTarget->__iFlags |= __XRT_CO_FLAG_JOIN_PINNED;
}
// 内部函数：__xrt_co_join_pin_release
static void __xrt_co_join_pin_release(xcoro pTarget)
{
	if ( pTarget == NULL ) {
		return;
	}
	if ( pTarget->__iJoinRefCount > 0 ) {
		pTarget->__iJoinRefCount--;
	}
	if ( pTarget->__iJoinRefCount == 0 ) {
		pTarget->__iFlags &= ~__XRT_CO_FLAG_JOIN_PINNED;
	}
}
// 内部函数：__xrt_co_detach_join_waiter
static void __xrt_co_detach_join_waiter(xcoro pWaiter, bool bReleasePin)
{
	xcoro pTarget = NULL;
	if ( pWaiter == NULL ) {
		return;
	}
	pTarget = pWaiter->__pJoinTarget;
	if ( pTarget != NULL ) {
		__xrt_co_wait_queue_remove(&pTarget->__pJoinWaitHead, &pTarget->__pJoinWaitTail, pWaiter);
	}
	if ( pWaiter->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
		__xrt_co_clear_wait_state(pWaiter);
	}
	if ( bReleasePin && pTarget != NULL ) {
		pWaiter->__pJoinTarget = NULL;
		__xrt_co_join_pin_release(pTarget);
	}
}
// 内部函数：__xrt_co_detach_event_waiter_locked
static void __xrt_co_detach_event_waiter_locked(xcoevent pEvent, xcoro pCo)
{
	if ( pEvent == NULL || pCo == NULL ) {
		return;
	}
	__xrt_co_wait_queue_remove(&pEvent->pWaitHead, &pEvent->pWaitTail, pCo);
	if ( pCo->__pWaitObject == pEvent && pCo->__iWaitKind == __XRT_CO_WAIT_EVENT ) {
		__xrt_co_clear_wait_state(pCo);
	}
}
// 内部函数：__xrt_co_pop_event_waiter_locked
static xcoro __xrt_co_pop_event_waiter_locked(xcoevent pEvent)
{
	xcoro pCo = NULL;
	if ( pEvent == NULL ) {
		return NULL;
	}
	pCo = __xrt_co_wait_queue_pop_head(&pEvent->pWaitHead, &pEvent->pWaitTail);
	if ( pCo != NULL && pCo->__pWaitObject == pEvent && pCo->__iWaitKind == __XRT_CO_WAIT_EVENT ) {
		__xrt_co_clear_wait_state(pCo);
	}
	return pCo;
}
// 内部函数：__xrt_co_event_try_consume_locked
static bool __xrt_co_event_try_consume_locked(xcoevent pEvent)
{
	if ( pEvent == NULL || !pEvent->bSignaled ) {
		return FALSE;
	}
	if ( !pEvent->bManualReset ) {
		pEvent->bSignaled = FALSE;
	}
	return TRUE;
}
// 内部函数：__xrt_co_free_cleanup_nodes
static void __xrt_co_free_cleanup_nodes(xcoro pCo)
{
	while ( pCo && pCo->__pCleanupTop ) {
		xco_cleanup* pCleanup = pCo->__pCleanupTop;
		pCo->__pCleanupTop = pCleanup->pPrev;
		xrtFree(pCleanup);
	}
}
// 内部函数：__xrt_co_run_cleanup
static void __xrt_co_run_cleanup(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}
	pCo->__iFlags |= __XRT_CO_FLAG_IN_CLEANUP;
	while ( pCo->__pCleanupTop ) {
		xco_cleanup* pCleanup = pCo->__pCleanupTop;
		pCo->__pCleanupTop = pCleanup->pPrev;
		if ( pCleanup->Proc ) {
			pCleanup->Proc(pCleanup->Arg);
		}
		xrtFree(pCleanup);
	}
	pCo->__iFlags &= ~__XRT_CO_FLAG_IN_CLEANUP;
}
// 内部函数：获取协程栈页大小
static size_t __xrt_co_stack_page_size()
{
	#if defined(_WIN32) || defined(_WIN64)
		SYSTEM_INFO tInfo;
		GetSystemInfo(&tInfo);
		return (tInfo.dwPageSize != 0) ? (size_t)tInfo.dwPageSize : 4096u;
	#else
		long iPage = sysconf(_SC_PAGESIZE);
		return (iPage > 0) ? (size_t)iPage : 4096u;
	#endif
}
// 内部函数：__xrt_co_align_up
static size_t __xrt_co_align_up(size_t iValue, size_t iAlign)
{
	if ( iAlign == 0 ) {
		return iValue;
	}
	return (iValue + iAlign - 1) & ~(iAlign - 1);
}
// 内部函数：规范化协程栈大小
static size_t __xrt_co_normalize_stack_size(size_t iStackSize)
{
	size_t iPageSize = __xrt_co_stack_page_size();
	if ( iStackSize == 0 ) {
		iStackSize = XRT_CO_STACK_DEFAULT;
	}
	if ( iStackSize < XRT_CO_STACK_MIN ) {
		iStackSize = XRT_CO_STACK_MIN;
	}
	if ( iStackSize > XRT_CO_STACK_MAX ) {
		iStackSize = XRT_CO_STACK_MAX;
	}
	return __xrt_co_align_up(iStackSize, iPageSize);
}
// 内部函数：分配协程栈
static bool __xrt_co_stack_alloc(xcoro pCo)
{
	size_t iPageSize = 0;
	size_t iGuardSize = 0;
	size_t iStackSize = 0;
	size_t iAllocSize = 0;
	ptr pBase = NULL;
	if ( pCo == NULL ) {
		return FALSE;
	}
	iPageSize = __xrt_co_stack_page_size();
	iGuardSize = iPageSize;
	iStackSize = __xrt_co_normalize_stack_size(pCo->iStackSize);
	if ( iStackSize > (SIZE_MAX - iGuardSize) ) {
		xrtSetError("coroutine stack size overflow.", FALSE);
		return FALSE;
	}
	iAllocSize = iStackSize + iGuardSize;
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iOldProtect = 0;
			pBase = VirtualAlloc(NULL, iAllocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if ( pBase == NULL ) {
				xrtSetError("failed to reserve coroutine stack.", FALSE);
				return FALSE;
			}
			if ( !VirtualProtect(pBase, iGuardSize, PAGE_NOACCESS, &iOldProtect) ) {
				VirtualFree(pBase, 0, MEM_RELEASE);
				xrtSetError("failed to protect coroutine guard page.", FALSE);
				return FALSE;
			}
		}
	#else
		pBase = mmap(NULL, iAllocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if ( pBase == MAP_FAILED ) {
			xrtSetError("failed to reserve coroutine stack.", FALSE);
			return FALSE;
		}
		if ( mprotect(pBase, iGuardSize, PROT_NONE) != 0 ) {
			munmap(pBase, iAllocSize);
			xrtSetError("failed to protect coroutine guard page.", FALSE);
			return FALSE;
		}
	#endif
	pCo->__pStackMem = pBase;
	pCo->__iStackAllocSize = iAllocSize;
	pCo->__iStackGuardSize = iGuardSize;
	pCo->__pStack = (uint8*)pBase + iGuardSize;
	pCo->iStackSize = iStackSize;
	return TRUE;
}
// 内部函数：释放协程栈
static void __xrt_co_stack_free(xcoro pCo)
{
	if ( pCo == NULL || pCo->__pStackMem == NULL ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		VirtualFree(pCo->__pStackMem, 0, MEM_RELEASE);
	#else
		munmap(pCo->__pStackMem, pCo->__iStackAllocSize);
	#endif
	pCo->__pStack = NULL;
	pCo->__pStackMem = NULL;
	pCo->__iStackAllocSize = 0;
	pCo->__iStackGuardSize = 0;
}
// 内部函数：__xrt_co_wake_join_waiters
static void __xrt_co_wake_join_waiters(xcoro pTarget)
{
	xcoro pWaiter = NULL;
	if ( pTarget == NULL ) {
		return;
	}
	for ( ;; ) {
		pWaiter = __xrt_co_wait_queue_pop_head(&pTarget->__pJoinWaitHead, &pTarget->__pJoinWaitTail);
		if ( pWaiter == NULL ) {
			break;
		}
		if ( pWaiter->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
			__xrt_co_clear_wait_state(pWaiter);
		}
		pWaiter->__iWakeTime = 0;
		if ( pWaiter->__pSched ) {
			__xrt_co_sched_mark_ready((xcosched*)pWaiter->__pSched, pWaiter);
		}
	}
}
// 内部函数：完成协程
static void __xrt_co_finish(xcoro pCo, uint32 iTermReason, int64 iExitCode)
{
	if ( pCo == NULL ) {
		return;
	}
	pCo->iState = XRT_CO_DEAD;
	pCo->iExitCode = iExitCode;
	pCo->iTermReason = iTermReason;
	pCo->__iWakeTime = 0;
	__xrt_co_clear_wait_state(pCo);
	if ( iTermReason == XRT_CO_TERM_CANCELLED ) {
		pCo->__iFlags |= __XRT_CO_FLAG_CANCELLED;
	}
	__xrt_co_run_cleanup(pCo);
	__xrt_co_wake_join_waiters(pCo);
}
// 内部函数：__xrt_co_finalize_ready_cancel
static bool __xrt_co_finalize_ready_cancel(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}
	if ( pCo->iState != XRT_CO_READY ) {
		return FALSE;
	}
	__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
	return TRUE;
}
/* ================================ 时间辅助函数 ================================ */
static int64 __xrt_co_time_ms()
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64)GetTickCount64();
	#else
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
	#endif
}
// 内部函数：休眠协程毫秒
static void UNUSED_ATTR __xrt_co_sleep_ms(int iMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iMs);
	#else
		struct timespec ts;
		ts.tv_sec = iMs / 1000;
		ts.tv_nsec = (iMs % 1000) * 1000000L;
		nanosleep(&ts, NULL);
	#endif
}
/* ================================ 后端实现: x86_64 内联汇编 ================================ */
#define __XRT_CO_JMP_RDX	"jmp *0x00(%%rdx)\n\t"
#define __XRT_CO_JMP_RSI	"jmp *0x00(%%rsi)\n\t"
#if defined(__TINYC__) && defined(__XRT_CO_ASM_X64_WIN)
	/*
		TCC x64 Windows inline asm 只能解析部分 XMM 指令/寄存器名。
		这里让 xmm6/xmm7 继续走可读语法，其余高位寄存器使用 .byte 编码。
	*/
	#define __XRT_CO_WIN64_SAVE_XMM6	"movups %%xmm6,  0x50(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM7	"movups %%xmm7,  0x60(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM8	".byte 0x44,0x0f,0x11,0x41,0x70\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM9	".byte 0x44,0x0f,0x11,0x89,0x80,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM10	".byte 0x44,0x0f,0x11,0x91,0x90,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM11	".byte 0x44,0x0f,0x11,0x99,0xA0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM12	".byte 0x44,0x0f,0x11,0xA1,0xB0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM13	".byte 0x44,0x0f,0x11,0xA9,0xC0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM14	".byte 0x44,0x0f,0x11,0xB1,0xD0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM15	".byte 0x44,0x0f,0x11,0xB9,0xE0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM15	".byte 0x44,0x0f,0x10,0xBA,0xE0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM14	".byte 0x44,0x0f,0x10,0xB2,0xD0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM13	".byte 0x44,0x0f,0x10,0xAA,0xC0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM12	".byte 0x44,0x0f,0x10,0xA2,0xB0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM11	".byte 0x44,0x0f,0x10,0x9A,0xA0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM10	".byte 0x44,0x0f,0x10,0x92,0x90,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM9	".byte 0x44,0x0f,0x10,0x8A,0x80,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM8	".byte 0x44,0x0f,0x10,0x42,0x70\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM7	"movups 0x60(%%rdx), %%xmm7\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM6	"movups 0x50(%%rdx), %%xmm6\n\t"
#else
	#define __XRT_CO_WIN64_SAVE_XMM6	"movdqu %%xmm6,  0x50(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM7	"movdqu %%xmm7,  0x60(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM8	"movdqu %%xmm8,  0x70(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM9	"movdqu %%xmm9,  0x80(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM10	"movdqu %%xmm10, 0x90(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM11	"movdqu %%xmm11, 0xA0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM12	"movdqu %%xmm12, 0xB0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM13	"movdqu %%xmm13, 0xC0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM14	"movdqu %%xmm14, 0xD0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM15	"movdqu %%xmm15, 0xE0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM15	"movdqu 0xE0(%%rdx), %%xmm15\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM14	"movdqu 0xD0(%%rdx), %%xmm14\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM13	"movdqu 0xC0(%%rdx), %%xmm13\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM12	"movdqu 0xB0(%%rdx), %%xmm12\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM11	"movdqu 0xA0(%%rdx), %%xmm11\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM10	"movdqu 0x90(%%rdx), %%xmm10\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM9	"movdqu 0x80(%%rdx), %%xmm9\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM8	"movdqu 0x70(%%rdx), %%xmm8\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM7	"movdqu 0x60(%%rdx), %%xmm7\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM6	"movdqu 0x50(%%rdx), %%xmm6\n\t"
#endif
#ifdef __XRT_CO_ASM_X64_WIN
/*
	Windows x64 ABI callee-saved 寄存器:
	0x00 = rip
	0x08 = rsp
	0x10 = rbp
	0x18 = rbx
	0x20 = rdi
	0x28 = rsi
	0x30 = r12
	0x38 = r13
	0x40 = r14
	0x48 = r15
	0x50 = xmm6
	0x60 = xmm7
	0x70 = xmm8
	0x80 = xmm9
	0x90 = xmm10
	0xA0 = xmm11
	0xB0 = xmm12
	0xC0 = xmm13
	0xD0 = xmm14
	0xE0 = xmm15
*/
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"leaq 1f(%%rip), %%rax\n\t"
		"movq %%rax, 0x00(%%rcx)\n\t"
		"movq %%rsp, 0x08(%%rcx)\n\t"
		"movq %%rbp, 0x10(%%rcx)\n\t"
		"movq %%rbx, 0x18(%%rcx)\n\t"
		"movq %%rdi, 0x20(%%rcx)\n\t"
		"movq %%rsi, 0x28(%%rcx)\n\t"
		"movq %%r12, 0x30(%%rcx)\n\t"
		"movq %%r13, 0x38(%%rcx)\n\t"
		"movq %%r14, 0x40(%%rcx)\n\t"
		"movq %%r15, 0x48(%%rcx)\n\t"
		__XRT_CO_WIN64_SAVE_XMM6
		__XRT_CO_WIN64_SAVE_XMM7
		__XRT_CO_WIN64_SAVE_XMM8
		__XRT_CO_WIN64_SAVE_XMM9
		__XRT_CO_WIN64_SAVE_XMM10
		__XRT_CO_WIN64_SAVE_XMM11
		__XRT_CO_WIN64_SAVE_XMM12
		__XRT_CO_WIN64_SAVE_XMM13
		__XRT_CO_WIN64_SAVE_XMM14
		__XRT_CO_WIN64_SAVE_XMM15
		__XRT_CO_WIN64_LOAD_XMM15
		__XRT_CO_WIN64_LOAD_XMM14
		__XRT_CO_WIN64_LOAD_XMM13
		__XRT_CO_WIN64_LOAD_XMM12
		__XRT_CO_WIN64_LOAD_XMM11
		__XRT_CO_WIN64_LOAD_XMM10
		__XRT_CO_WIN64_LOAD_XMM9
		__XRT_CO_WIN64_LOAD_XMM8
		__XRT_CO_WIN64_LOAD_XMM7
		__XRT_CO_WIN64_LOAD_XMM6
		"movq 0x48(%%rdx), %%r15\n\t"
		"movq 0x40(%%rdx), %%r14\n\t"
		"movq 0x38(%%rdx), %%r13\n\t"
		"movq 0x30(%%rdx), %%r12\n\t"
		"movq 0x28(%%rdx), %%rsi\n\t"
		"movq 0x20(%%rdx), %%rdi\n\t"
		"movq 0x18(%%rdx), %%rbx\n\t"
		"movq 0x10(%%rdx), %%rbp\n\t"
		"movq 0x08(%%rdx), %%rsp\n\t"
		__XRT_CO_JMP_RDX
		"1:\n\t"
		:
		: "c"(pFrom), "d"(pTo)
		: "memory", "rax", "r8", "r9", "r10", "r11"
	);
}
#endif
#ifdef __XRT_CO_ASM_X64
/*
	x86_64 System V ABI callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (rip)
	arrReg[1] = rsp
	arrReg[2] = rbp
	arrReg[3] = rbx
	arrReg[4] = r12
	arrReg[5] = r13
	arrReg[6] = r14
	arrReg[7] = r15
*/
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"leaq 1f(%%rip), %%rax\n\t"
		"movq %%rax, 0x00(%%rdi)\n\t"
		"movq %%rsp, 0x08(%%rdi)\n\t"
		"movq %%rbp, 0x10(%%rdi)\n\t"
		"movq %%rbx, 0x18(%%rdi)\n\t"
		"movq %%r12, 0x20(%%rdi)\n\t"
		"movq %%r13, 0x28(%%rdi)\n\t"
		"movq %%r14, 0x30(%%rdi)\n\t"
		"movq %%r15, 0x38(%%rdi)\n\t"
		"movq 0x38(%%rsi), %%r15\n\t"
		"movq 0x30(%%rsi), %%r14\n\t"
		"movq 0x28(%%rsi), %%r13\n\t"
		"movq 0x20(%%rsi), %%r12\n\t"
		"movq 0x18(%%rsi), %%rbx\n\t"
		"movq 0x10(%%rsi), %%rbp\n\t"
		"movq 0x08(%%rsi), %%rsp\n\t"
		__XRT_CO_JMP_RSI
		"1:\n\t"
		: : "D"(pFrom), "S"(pTo)
		: "memory", "rax", "rcx", "rdx",
		  "r8", "r9", "r10", "r11"
	);
}
#endif
/* ================================ 后端实现: ARM64 内联汇编 ================================ */
#ifdef __XRT_CO_ASM_ARM64
/*
	ARM64 AAPCS64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址
	arrReg[1] = sp
	arrReg[2..3] = x19, x20
	arrReg[4..5] = x21, x22
	arrReg[6..7] = x23, x24
	arrReg[8..9] = x25, x26
	arrReg[10..11] = x27, x28
	arrReg[12..13] = x29 (fp), x30 (lr)
	arrReg[14..17] = q8, q9
	arrReg[18..21] = q10, q11
	arrReg[22..25] = q12, q13
	arrReg[26..29] = q14, q15
*/
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"adr x2, 1f\n\t"
		"mov x3, sp\n\t"
		"stp x2,  x3,  [%0, #0x00]\n\t"
		"stp x19, x20, [%0, #0x10]\n\t"
		"stp x21, x22, [%0, #0x20]\n\t"
		"stp x23, x24, [%0, #0x30]\n\t"
		"stp x25, x26, [%0, #0x40]\n\t"
		"stp x27, x28, [%0, #0x50]\n\t"
		"stp x29, x30, [%0, #0x60]\n\t"
		"stp q8,  q9,  [%0, #0x70]\n\t"
		"stp q10, q11, [%0, #0x90]\n\t"
		"stp q12, q13, [%0, #0xB0]\n\t"
		"stp q14, q15, [%0, #0xD0]\n\t"
		"ldp q14, q15, [%1, #0xD0]\n\t"
		"ldp q12, q13, [%1, #0xB0]\n\t"
		"ldp q10, q11, [%1, #0x90]\n\t"
		"ldp q8,  q9,  [%1, #0x70]\n\t"
		"ldp x29, x30, [%1, #0x60]\n\t"
		"ldp x27, x28, [%1, #0x50]\n\t"
		"ldp x25, x26, [%1, #0x40]\n\t"
		"ldp x23, x24, [%1, #0x30]\n\t"
		"ldp x21, x22, [%1, #0x20]\n\t"
		"ldp x19, x20, [%1, #0x10]\n\t"
		"ldp x2,  x3,  [%1, #0x00]\n\t"
		"mov sp, x3\n\t"
		"br x2\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "x2", "x3"
	);
}
#endif
/* ================================ 后端实现: RISC-V 64 内联汇编 ================================ */
#ifdef __XRT_CO_ASM_RV64
	#if (defined(__riscv_flen) && (__riscv_flen == 32)) || defined(__riscv_float_abi_single)
		#define __XRT_CO_RV64_FP32
		#define __XRT_CO_RV64_HAS_FP
	#elif (defined(__riscv_flen) && (__riscv_flen >= 64)) || defined(__riscv_float_abi_double) || defined(__riscv_float_abi_quad)
		#define __XRT_CO_RV64_FP64
		#define __XRT_CO_RV64_HAS_FP
	#endif
/*
	RISC-V LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra/pc)
	arrReg[1] = sp
	arrReg[2] = s0 (fp)
	arrReg[3..13] = s1 ~ s11
	若使用硬浮点 ABI:
	arrReg[14..25] = fs0 ~ fs11（统一按 8 字节槽位存放，单精度 ABI 仅使用每槽低 4 字节）
*/
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_RV64_FP64
		__asm__ volatile (
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"
			"sd sp,   0x08(%0)\n\t"
			"sd s0,   0x10(%0)\n\t"
			"sd s1,   0x18(%0)\n\t"
			"sd s2,   0x20(%0)\n\t"
			"sd s3,   0x28(%0)\n\t"
			"sd s4,   0x30(%0)\n\t"
			"sd s5,   0x38(%0)\n\t"
			"sd s6,   0x40(%0)\n\t"
			"sd s7,   0x48(%0)\n\t"
			"sd s8,   0x50(%0)\n\t"
			"sd s9,   0x58(%0)\n\t"
			"sd s10,  0x60(%0)\n\t"
			"sd s11,  0x68(%0)\n\t"
			"fsd fs0,  0x70(%0)\n\t"
			"fsd fs1,  0x78(%0)\n\t"
			"fsd fs2,  0x80(%0)\n\t"
			"fsd fs3,  0x88(%0)\n\t"
			"fsd fs4,  0x90(%0)\n\t"
			"fsd fs5,  0x98(%0)\n\t"
			"fsd fs6,  0xA0(%0)\n\t"
			"fsd fs7,  0xA8(%0)\n\t"
			"fsd fs8,  0xB0(%0)\n\t"
			"fsd fs9,  0xB8(%0)\n\t"
			"fsd fs10, 0xC0(%0)\n\t"
			"fsd fs11, 0xC8(%0)\n\t"
			"fld fs11, 0xC8(%1)\n\t"
			"fld fs10, 0xC0(%1)\n\t"
			"fld fs9,  0xB8(%1)\n\t"
			"fld fs8,  0xB0(%1)\n\t"
			"fld fs7,  0xA8(%1)\n\t"
			"fld fs6,  0xA0(%1)\n\t"
			"fld fs5,  0x98(%1)\n\t"
			"fld fs4,  0x90(%1)\n\t"
			"fld fs3,  0x88(%1)\n\t"
			"fld fs2,  0x80(%1)\n\t"
			"fld fs1,  0x78(%1)\n\t"
			"fld fs0,  0x70(%1)\n\t"
			"ld s11,   0x68(%1)\n\t"
			"ld s10,   0x60(%1)\n\t"
			"ld s9,    0x58(%1)\n\t"
			"ld s8,    0x50(%1)\n\t"
			"ld s7,    0x48(%1)\n\t"
			"ld s6,    0x40(%1)\n\t"
			"ld s5,    0x38(%1)\n\t"
			"ld s4,    0x30(%1)\n\t"
			"ld s3,    0x28(%1)\n\t"
			"ld s2,    0x20(%1)\n\t"
			"ld s1,    0x18(%1)\n\t"
			"ld s0,    0x10(%1)\n\t"
			"ld sp,    0x08(%1)\n\t"
			"ld t0,    0x00(%1)\n\t"
			"jr t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#elif defined(__XRT_CO_RV64_FP32)
		__asm__ volatile (
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"
			"sd sp,   0x08(%0)\n\t"
			"sd s0,   0x10(%0)\n\t"
			"sd s1,   0x18(%0)\n\t"
			"sd s2,   0x20(%0)\n\t"
			"sd s3,   0x28(%0)\n\t"
			"sd s4,   0x30(%0)\n\t"
			"sd s5,   0x38(%0)\n\t"
			"sd s6,   0x40(%0)\n\t"
			"sd s7,   0x48(%0)\n\t"
			"sd s8,   0x50(%0)\n\t"
			"sd s9,   0x58(%0)\n\t"
			"sd s10,  0x60(%0)\n\t"
			"sd s11,  0x68(%0)\n\t"
			"fsw fs0,  0x70(%0)\n\t"
			"fsw fs1,  0x78(%0)\n\t"
			"fsw fs2,  0x80(%0)\n\t"
			"fsw fs3,  0x88(%0)\n\t"
			"fsw fs4,  0x90(%0)\n\t"
			"fsw fs5,  0x98(%0)\n\t"
			"fsw fs6,  0xA0(%0)\n\t"
			"fsw fs7,  0xA8(%0)\n\t"
			"fsw fs8,  0xB0(%0)\n\t"
			"fsw fs9,  0xB8(%0)\n\t"
			"fsw fs10, 0xC0(%0)\n\t"
			"fsw fs11, 0xC8(%0)\n\t"
			"flw fs11, 0xC8(%1)\n\t"
			"flw fs10, 0xC0(%1)\n\t"
			"flw fs9,  0xB8(%1)\n\t"
			"flw fs8,  0xB0(%1)\n\t"
			"flw fs7,  0xA8(%1)\n\t"
			"flw fs6,  0xA0(%1)\n\t"
			"flw fs5,  0x98(%1)\n\t"
			"flw fs4,  0x90(%1)\n\t"
			"flw fs3,  0x88(%1)\n\t"
			"flw fs2,  0x80(%1)\n\t"
			"flw fs1,  0x78(%1)\n\t"
			"flw fs0,  0x70(%1)\n\t"
			"ld s11,   0x68(%1)\n\t"
			"ld s10,   0x60(%1)\n\t"
			"ld s9,    0x58(%1)\n\t"
			"ld s8,    0x50(%1)\n\t"
			"ld s7,    0x48(%1)\n\t"
			"ld s6,    0x40(%1)\n\t"
			"ld s5,    0x38(%1)\n\t"
			"ld s4,    0x30(%1)\n\t"
			"ld s3,    0x28(%1)\n\t"
			"ld s2,    0x20(%1)\n\t"
			"ld s1,    0x18(%1)\n\t"
			"ld s0,    0x10(%1)\n\t"
			"ld sp,    0x08(%1)\n\t"
			"ld t0,    0x00(%1)\n\t"
			"jr t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#else
	__asm__ volatile (
		"la t0, 1f\n\t"
		"sd t0,  0x00(%0)\n\t"
		"sd sp,  0x08(%0)\n\t"
		"sd s0,  0x10(%0)\n\t"
		"sd s1,  0x18(%0)\n\t"
		"sd s2,  0x20(%0)\n\t"
		"sd s3,  0x28(%0)\n\t"
		"sd s4,  0x30(%0)\n\t"
		"sd s5,  0x38(%0)\n\t"
		"sd s6,  0x40(%0)\n\t"
		"sd s7,  0x48(%0)\n\t"
		"sd s8,  0x50(%0)\n\t"
		"sd s9,  0x58(%0)\n\t"
		"sd s10, 0x60(%0)\n\t"
		"sd s11, 0x68(%0)\n\t"
		"ld s11, 0x68(%1)\n\t"
		"ld s10, 0x60(%1)\n\t"
		"ld s9,  0x58(%1)\n\t"
		"ld s8,  0x50(%1)\n\t"
		"ld s7,  0x48(%1)\n\t"
		"ld s6,  0x40(%1)\n\t"
		"ld s5,  0x38(%1)\n\t"
		"ld s4,  0x30(%1)\n\t"
		"ld s3,  0x28(%1)\n\t"
		"ld s2,  0x20(%1)\n\t"
		"ld s1,  0x18(%1)\n\t"
		"ld s0,  0x10(%1)\n\t"
		"ld sp,  0x08(%1)\n\t"
		"ld t0,  0x00(%1)\n\t"
		"jr t0\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
		  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
	);
	#endif
}
#endif
/* ================================ 后端实现: LoongArch64 内联汇编 ================================ */
#ifdef __XRT_CO_ASM_LA64
	#if defined(__loongarch_frlen) && (__loongarch_frlen == 32)
		#define __XRT_CO_LA64_FP32
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_frlen) && (__loongarch_frlen >= 64)
		#define __XRT_CO_LA64_FP64
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_single_float)
		#define __XRT_CO_LA64_FP32
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_double_float)
		#define __XRT_CO_LA64_FP64
		#define __XRT_CO_LA64_HAS_FP
	#endif
/*
	LoongArch64 LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra)
	arrReg[1] = sp ($r3)
	arrReg[2] = fp ($r22)
	arrReg[3..11] = s0~s8 ($r23~$r31)
	若使用硬浮点 ABI:
	arrReg[12..19] = fs0 ~ fs7（统一按 8 字节槽位存放，单精度 ABI 仅使用每槽低 4 字节）
*/
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_LA64_FP64
		__asm__ volatile (
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"
			"st.d   $sp,   %0, 0x08\n\t"
			"st.d   $fp,   %0, 0x10\n\t"
			"st.d   $s0,   %0, 0x18\n\t"
			"st.d   $s1,   %0, 0x20\n\t"
			"st.d   $s2,   %0, 0x28\n\t"
			"st.d   $s3,   %0, 0x30\n\t"
			"st.d   $s4,   %0, 0x38\n\t"
			"st.d   $s5,   %0, 0x40\n\t"
			"st.d   $s6,   %0, 0x48\n\t"
			"st.d   $s7,   %0, 0x50\n\t"
			"st.d   $s8,   %0, 0x58\n\t"
			"fst.d  $fs0,  %0, 0x60\n\t"
			"fst.d  $fs1,  %0, 0x68\n\t"
			"fst.d  $fs2,  %0, 0x70\n\t"
			"fst.d  $fs3,  %0, 0x78\n\t"
			"fst.d  $fs4,  %0, 0x80\n\t"
			"fst.d  $fs5,  %0, 0x88\n\t"
			"fst.d  $fs6,  %0, 0x90\n\t"
			"fst.d  $fs7,  %0, 0x98\n\t"
			"fld.d  $fs7,  %1, 0x98\n\t"
			"fld.d  $fs6,  %1, 0x90\n\t"
			"fld.d  $fs5,  %1, 0x88\n\t"
			"fld.d  $fs4,  %1, 0x80\n\t"
			"fld.d  $fs3,  %1, 0x78\n\t"
			"fld.d  $fs2,  %1, 0x70\n\t"
			"fld.d  $fs1,  %1, 0x68\n\t"
			"fld.d  $fs0,  %1, 0x60\n\t"
			"ld.d   $s8,   %1, 0x58\n\t"
			"ld.d   $s7,   %1, 0x50\n\t"
			"ld.d   $s6,   %1, 0x48\n\t"
			"ld.d   $s5,   %1, 0x40\n\t"
			"ld.d   $s4,   %1, 0x38\n\t"
			"ld.d   $s3,   %1, 0x30\n\t"
			"ld.d   $s2,   %1, 0x28\n\t"
			"ld.d   $s1,   %1, 0x20\n\t"
			"ld.d   $fp,   %1, 0x10\n\t"
			"ld.d   $s0,   %1, 0x18\n\t"
			"ld.d   $sp,   %1, 0x08\n\t"
			"ld.d   $t0,   %1, 0x00\n\t"
			"jr $t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#elif defined(__XRT_CO_LA64_FP32)
		__asm__ volatile (
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"
			"st.d   $sp,   %0, 0x08\n\t"
			"st.d   $fp,   %0, 0x10\n\t"
			"st.d   $s0,   %0, 0x18\n\t"
			"st.d   $s1,   %0, 0x20\n\t"
			"st.d   $s2,   %0, 0x28\n\t"
			"st.d   $s3,   %0, 0x30\n\t"
			"st.d   $s4,   %0, 0x38\n\t"
			"st.d   $s5,   %0, 0x40\n\t"
			"st.d   $s6,   %0, 0x48\n\t"
			"st.d   $s7,   %0, 0x50\n\t"
			"st.d   $s8,   %0, 0x58\n\t"
			"fst.s  $fs0,  %0, 0x60\n\t"
			"fst.s  $fs1,  %0, 0x68\n\t"
			"fst.s  $fs2,  %0, 0x70\n\t"
			"fst.s  $fs3,  %0, 0x78\n\t"
			"fst.s  $fs4,  %0, 0x80\n\t"
			"fst.s  $fs5,  %0, 0x88\n\t"
			"fst.s  $fs6,  %0, 0x90\n\t"
			"fst.s  $fs7,  %0, 0x98\n\t"
			"fld.s  $fs7,  %1, 0x98\n\t"
			"fld.s  $fs6,  %1, 0x90\n\t"
			"fld.s  $fs5,  %1, 0x88\n\t"
			"fld.s  $fs4,  %1, 0x80\n\t"
			"fld.s  $fs3,  %1, 0x78\n\t"
			"fld.s  $fs2,  %1, 0x70\n\t"
			"fld.s  $fs1,  %1, 0x68\n\t"
			"fld.s  $fs0,  %1, 0x60\n\t"
			"ld.d   $s8,   %1, 0x58\n\t"
			"ld.d   $s7,   %1, 0x50\n\t"
			"ld.d   $s6,   %1, 0x48\n\t"
			"ld.d   $s5,   %1, 0x40\n\t"
			"ld.d   $s4,   %1, 0x38\n\t"
			"ld.d   $s3,   %1, 0x30\n\t"
			"ld.d   $s2,   %1, 0x28\n\t"
			"ld.d   $s1,   %1, 0x20\n\t"
			"ld.d   $fp,   %1, 0x10\n\t"
			"ld.d   $s0,   %1, 0x18\n\t"
			"ld.d   $sp,   %1, 0x08\n\t"
			"ld.d   $t0,   %1, 0x00\n\t"
			"jr $t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#else
	__asm__ volatile (
		"la.local $t0, 1f\n\t"
		"st.d $t0,  %0, 0x00\n\t"
		"st.d $sp,  %0, 0x08\n\t"
		"st.d $fp,  %0, 0x10\n\t"
		"st.d $s0,  %0, 0x18\n\t"
		"st.d $s1,  %0, 0x20\n\t"
		"st.d $s2,  %0, 0x28\n\t"
		"st.d $s3,  %0, 0x30\n\t"
		"st.d $s4,  %0, 0x38\n\t"
		"st.d $s5,  %0, 0x40\n\t"
		"st.d $s6,  %0, 0x48\n\t"
		"st.d $s7,  %0, 0x50\n\t"
		"st.d $s8,  %0, 0x58\n\t"
		"ld.d $s8,  %1, 0x58\n\t"
		"ld.d $s7,  %1, 0x50\n\t"
		"ld.d $s6,  %1, 0x48\n\t"
		"ld.d $s5,  %1, 0x40\n\t"
		"ld.d $s4,  %1, 0x38\n\t"
		"ld.d $s3,  %1, 0x30\n\t"
		"ld.d $s2,  %1, 0x28\n\t"
		"ld.d $s1,  %1, 0x20\n\t"
		"ld.d $s0,  %1, 0x18\n\t"
		"ld.d $fp,  %1, 0x10\n\t"
		"ld.d $sp,  %1, 0x08\n\t"
		"ld.d $t0,  %1, 0x00\n\t"
		"jr $t0\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
	);
	#endif
}
#endif
/* ================================ 后端实现: Windows Fiber ================================ */
#ifdef __XRT_CO_FIBER_WIN
// 内部函数：__xrt_co_prepare_backend_main
static bool __xrt_co_prepare_backend_main(xrtCoroRuntimeState* pRuntime)
{
	if ( pRuntime == NULL ) {
		return FALSE;
	}
	if ( pRuntime->pBackendMain != NULL ) {
		return TRUE;
	}
	if ( IsThreadAFiber() ) {
		pRuntime->pBackendMain = GetCurrentFiber();
		pRuntime->iFlags |= XRT_CO_RUNTIME_FIBER_HOSTED;
		return TRUE;
	}
	pRuntime->pBackendMain = ConvertThreadToFiber(NULL);
	if ( pRuntime->pBackendMain == NULL ) {
		xrtSetError("failed to convert current thread to fiber.", FALSE);
		return FALSE;
	}
	pRuntime->iFlags |= XRT_CO_RUNTIME_FIBER_CONVERTED;
	return TRUE;
}
// 内部函数：__xrt_co_fiber_entry
static VOID CALLBACK __xrt_co_fiber_entry(LPVOID pParameter)
{
	xcoro pCo = (xcoro)pParameter;
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();
	if ( pCo == NULL || pRuntime == NULL || pRuntime->pBackendMain == NULL ) {
		return;
	}
	pCo->pfnEntry(pCo->pParam);
	__xrt_co_finish(pCo, __xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED, 0);
	SwitchToFiber(pRuntime->pBackendMain);
}
// 内部函数：__xrt_co_init_ctx
static bool __xrt_co_init_ctx(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}
	pCo->__hFiber = CreateFiber(pCo->iStackSize, __xrt_co_fiber_entry, pCo);
	if ( pCo->__hFiber == NULL ) {
		xrtSetError("failed to create coroutine fiber.", FALSE);
		return FALSE;
	}
	return TRUE;
}
// 内部函数：__xrt_co_swap_to_co
static void __xrt_co_swap_to_co(xrtCoroRuntimeState* pRuntime, xcoro pCo)
{
	if ( pRuntime && pRuntime->pBackendMain && pCo && pCo->__hFiber ) {
		SwitchToFiber(pCo->__hFiber);
	}
}
// 内部函数：__xrt_co_swap_to_main
static void __xrt_co_swap_to_main(xrtCoroRuntimeState* pRuntime)
{
	if ( pRuntime && pRuntime->pBackendMain ) {
		SwitchToFiber(pRuntime->pBackendMain);
	}
}
#endif
/* ================================ 汇编后端: 入口 + 初始化 + swap 包装 ================================ */
#if defined(__XRT_CO_ASM_X64_WIN) || defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)
// 内部函数：__xrt_co_prepare_backend_main
static bool __xrt_co_prepare_backend_main(xrtCoroRuntimeState* pRuntime)
{
	__xrt_co_ctx* pMainCtx = NULL;
	if ( pRuntime == NULL ) {
		return FALSE;
	}
	if ( pRuntime->pBackendMain == NULL ) {
		pMainCtx = (__xrt_co_ctx*)xrtCalloc(1, sizeof(__xrt_co_ctx));
		if ( pMainCtx == NULL ) {
			xrtSetError("failed to allocate coroutine main context.", FALSE);
			return FALSE;
		}
		pRuntime->pBackendMain = pMainCtx;
	}
	return TRUE;
}
// 协程入口包装（不接收参数，从线程状态读取当前协程）
static void __xrt_co_asm_entry(void)
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();
	if ( pCo == NULL || pRuntime == NULL || pRuntime->pBackendMain == NULL ) {
		return;
	}
	pCo->pfnEntry(pCo->pParam);
	__xrt_co_finish(pCo, __xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED, 0);
	__xrt_co_swap(&pCo->__tCtx, (__xrt_co_ctx*)pRuntime->pBackendMain);
}
// 内部函数：__xrt_co_init_ctx
static bool __xrt_co_init_ctx(xcoro pCo)
{
	uint8* pStackTop = (uint8*)pCo->__pStack + pCo->iStackSize;
	// 对齐到 16 字节边界
	pStackTop = (uint8*)((uintptr)pStackTop & ~(uintptr)0x0F);
	#ifdef __XRT_CO_ASM_X64
		// x86_64: 模拟 call 指令压入返回地址的效果 (rsp mod 16 == 8)
		pStackTop -= 8;
	#elif defined(__XRT_CO_ASM_X64_WIN)
		// Win64: 需要模拟返回地址 + 32 字节 shadow space
		pStackTop -= 40;
	#endif
	memset(&pCo->__tCtx, 0, sizeof(__xrt_co_ctx));
	// arrReg[0] = 入口点, arrReg[1] = 栈顶
	pCo->__tCtx.arrReg[0] = (ptr)__xrt_co_asm_entry;
	pCo->__tCtx.arrReg[1] = (ptr)pStackTop;
	return TRUE;
}
// 内部函数：__xrt_co_swap_to_co
static void __xrt_co_swap_to_co(xrtCoroRuntimeState* pRuntime, xcoro pCo)
{
	__xrt_co_swap((__xrt_co_ctx*)pRuntime->pBackendMain, &pCo->__tCtx);
}
// 内部函数：__xrt_co_swap_to_main
static void __xrt_co_swap_to_main(xrtCoroRuntimeState* pRuntime)
{
	xcoro pCurrent = __xrt_co_get_current();
	if ( pRuntime && pRuntime->pBackendMain && pCurrent ) {
		__xrt_co_swap(&pCurrent->__tCtx, (__xrt_co_ctx*)pRuntime->pBackendMain);
	}
}
#endif
/* ================================ 内部销毁辅助 ================================ */
static void __xrt_co_destroy_raw(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}
	#ifdef __XRT_CO_FIBER_WIN
		if ( pCo->__hFiber != NULL ) {
			DeleteFiber(pCo->__hFiber);
			pCo->__hFiber = NULL;
		}
	#endif
	__xrt_co_stack_free(pCo);
	__xrt_co_free_cleanup_nodes(pCo);
	xrtFree(pCo);
}
/* ================================ 基础协程 API ================================ */
XXAPI xcoro xrtCoCreateEx(xco_entry pfnEntry, ptr pParam, const xco_create_args* pArgs)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	xcoro pCo = NULL;
	size_t iStackSize = 0;
	ptr pUserData = NULL;
	uint32 iFlags = 0;
	if ( pThreadData == NULL || pRuntime == NULL ) {
		return NULL;
	}
	if ( !pfnEntry ) {
		xrtSetError("coroutine entry is null.", FALSE);
		return NULL;
	}
	if ( pArgs != NULL ) {
		iStackSize = pArgs->iStackSize;
		pUserData = pArgs->pUserData;
		iFlags = pArgs->iFlags;
	}
	if ( iFlags != XRT_CO_CREATE_NONE ) {
		xrtSetError("unsupported coroutine create flags.", FALSE);
		return NULL;
	}
	// 栈大小处理
	iStackSize = __xrt_co_normalize_stack_size(iStackSize);
	// 分配协程结构体
	pCo = (xcoro)xrtMalloc(sizeof(xcoro_struct));
	if ( !pCo ) {
		return NULL;
	}
	memset(pCo, 0, sizeof(xcoro_struct));
	pCo->iState = XRT_CO_READY;
	pCo->__iFlags = 0;
	pCo->__iOwnerThreadId = pThreadData->iThreadId;
	pCo->pfnEntry = pfnEntry;
	pCo->pParam = pParam;
	pCo->pUserData = pUserData;
	pCo->pResult = NULL;
	pCo->iExitCode = 0;
	pCo->iTermReason = XRT_CO_TERM_NONE;
	pCo->iStackSize = iStackSize;
	if ( !__xrt_co_prepare_backend_main(pRuntime) ) {
		xrtFree(pCo);
		return NULL;
	}
	#if __XRT_CO_BACKEND_NEEDS_STACK_ALLOC
		if ( !__xrt_co_stack_alloc(pCo) ) {
			xrtFree(pCo);
			return NULL;
		}
	#endif
	if ( !__xrt_co_init_ctx(pCo) ) {
		__xrt_co_destroy_raw(pCo);
		return NULL;
	}
	return pCo;
}
// 创建协程
XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	xco_create_args tArgs;
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.iStackSize = iStackSize;
	return xrtCoCreateEx(pfnEntry, pParam, &tArgs);
}
// 销毁协程
XXAPI void xrtCoDestroy(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return;
	}
	if ( pCo->__pSched != NULL ) {
		xrtSetError("cannot destroy coroutine while it is owned by a scheduler.", FALSE);
		return;
	}
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_DEAD) ) {
		xrtSetError("coroutine destroy requires READY or DEAD state.", FALSE);
		return;
	}
	__xrt_co_destroy_raw(pCo);
}
// 取消协程
XXAPI bool xrtCoCancel(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}
	pCo->__iFlags |= __XRT_CO_FLAG_CANCEL_REQUESTED;
	if ( pCo == __xrt_co_get_current() ) {
		return TRUE;
	}
	if ( pCo->iState == XRT_CO_READY && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
		__xrt_co_finalize_ready_cancel(pCo);
		return TRUE;
	}
	if ( pCo->__pJoinTarget != NULL ) {
		__xrt_co_detach_join_waiter(pCo, TRUE);
	}
	if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
		xcoevent pEvent = (xcoevent)pCo->__pWaitObject;
		if ( pEvent->pLock ) {
			xrtMutexLock(pEvent->pLock);
			__xrt_co_detach_event_waiter_locked(pEvent, pCo);
			xrtMutexUnlock(pEvent->pLock);
		}
		else {
			__xrt_co_clear_wait_state(pCo);
		}
	}
	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}
	if ( pCo->__pSched != NULL ) {
		__xrt_co_sched_detach_timer((xcosched*)pCo->__pSched, pCo);
		__xrt_co_sched_mark_ready((xcosched*)pCo->__pSched, pCo);
	}
	return TRUE;
}
// 关闭协程
XXAPI bool xrtCoClose(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	if ( pCo->__pSched != NULL ) {
		if ( pCo->iState == XRT_CO_DEAD ) {
			return __xrt_co_sched_release_dead(pCo);
		}
		pCo->__iFlags |= __XRT_CO_FLAG_CLOSE_REQUESTED;
		return xrtCoCancel(pCo);
	}
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_DEAD) ) {
		xrtSetError("close requires READY or DEAD coroutine when no scheduler owns it.", FALSE);
		return FALSE;
	}
	xrtCoDestroy(pCo);
	return TRUE;
}
// xrtCoResume 相关处理
XXAPI bool xrtCoResume(xcoro pCo)
{
	xrtCoroRuntimeState* pRuntime = NULL;
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	pRuntime = __xrt_co_require_runtime(TRUE);
	if ( pRuntime == NULL ) {
		return FALSE;
	}
	if ( !__xrt_co_prepare_backend_main(pRuntime) ) {
		return FALSE;
	}
	// 只能恢复 READY 或 SUSPENDED 状态的协程
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_SUSPENDED) ) {
		xrtSetError("coroutine is not in a resumable state.", FALSE);
		return FALSE;
	}
	// 不能在协程内部调用 Resume（不支持嵌套）
	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("nested coroutine resume is not supported.", FALSE);
		return FALSE;
	}
	if ( pCo->iState == XRT_CO_READY &&
		__xrt_co_is_cancel_requested_flag(pCo) &&
		(pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
		__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
		return TRUE;
	}
	pCo->__iWakeTime = 0;
	pCo->__iFlags |= __XRT_CO_FLAG_STARTED;
	__xrt_co_set_current(pCo);
	pCo->iState = XRT_CO_RUNNING;
	__xrt_co_swap_to_co(pRuntime, pCo);
	// 从这里恢复意味着协程 yield 了或者结束了
	__xrt_co_set_current(NULL);
	return TRUE;
}
// xrtCoYield 相关处理
XXAPI void xrtCoYield()
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_require_runtime(FALSE);
	if ( pCo == NULL || pRuntime == NULL ) {
		return;
	}
	if ( (pCo->__iFlags & __XRT_CO_FLAG_IN_CLEANUP) != 0 ) {
		xrtSetError("cannot yield while running coroutine cleanup.", FALSE);
		return;
	}
	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_swap_to_main(pRuntime);
}
// 获取协程状态
XXAPI int xrtCoGetState(xcoro pCo)
{
	if ( !pCo ) return XRT_CO_DEAD;
	return pCo->iState;
}
// 获取协程当前
XXAPI xcoro xrtCoGetCurrent()
{
	return __xrt_co_get_current();
}
// xrtCoIsCancelRequested 相关处理
XXAPI bool xrtCoIsCancelRequested()
{
	xcoro pCo = __xrt_co_get_current();
	return pCo ? __xrt_co_is_cancel_requested_flag(pCo) : FALSE;
}
// xrtCoWasCancelled 相关处理
XXAPI bool xrtCoWasCancelled(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	return (pCo->__iFlags & __XRT_CO_FLAG_CANCELLED) != 0;
}
// xrtCoGetExitCode 相关处理
XXAPI int64 xrtCoGetExitCode(xcoro pCo)
{
	if ( pCo == NULL ) {
		return 0;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return 0;
	}
	return pCo->iExitCode;
}
// 设置协程结果
XXAPI void xrtCoSetResult(ptr pResult)
{
	xcoro pCo = __xrt_co_get_current();
	if ( pCo == NULL ) {
		xrtSetError("xrtCoSetResult must be called from inside a coroutine.", FALSE);
		return;
	}
	pCo->pResult = pResult;
}
// 获取协程结果
XXAPI ptr xrtCoGetResult(xcoro pCo)
{
	if ( pCo == NULL ) {
		return NULL;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return NULL;
	}
	return pCo->pResult;
}
// 获取协程 backend 名称
XXAPI str xrtCoGetBackendName()
{
	return (str)__XRT_CO_BACKEND_NAME;
}
// xrtCoGetBackendTier 相关处理
XXAPI int xrtCoGetBackendTier()
{
	return __XRT_CO_BACKEND_TIER;
}
// xrtCoGetBackendStyle 相关处理
XXAPI int xrtCoGetBackendStyle()
{
	return __XRT_CO_BACKEND_STYLE;
}
/* ================================ 协程调度器 ================================ */
#define __XRT_CO_SCHED_INIT_CAP  16
#define __XRT_CO_TIMER_INIT_CAP  16
#define __XRT_CO_SCHED_WAIT_FOREVER 0xFFFFFFFFu
typedef struct __xrt_co_post_item {
	xcoro pCo;
	struct __xrt_co_post_item* pNext;
} __xrt_co_post_item;
struct xrt_co_scheduler {
	xrtThreadData* pThreadData;
	uint64 iOwnerThreadId;
	xcoro* arrCoros;
	int iCount;
	int iCapacity;
	int iAlive;
	xcoro pReadyHead;
	xcoro pReadyTail;
	xcoro* arrTimers;
	int iTimerCount;
	int iTimerCapacity;
	xmutex pPostMutex;
	xcond pPostCond;
	__xrt_co_post_item* pPostHead;
	__xrt_co_post_item* pPostTail;
	__xrt_co_post_item* pPostFree;
};
// 内部函数：__xrt_co_sched_find_index
static int __xrt_co_sched_find_index(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL ) {
		return -1;
	}
	for ( int i = 0; i < pSched->iCount; i++ ) {
		if ( pSched->arrCoros[i] == pCo ) {
			return i;
		}
	}
	return -1;
}
// 内部函数：__xrt_co_sched_ready_unlink
static bool __xrt_co_sched_ready_unlink(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL || (pCo->__iFlags & __XRT_CO_FLAG_READY_QUEUED) == 0 ) {
		return FALSE;
	}
	if ( pCo->__pReadyPrev ) {
		pCo->__pReadyPrev->__pReadyNext = pCo->__pReadyNext;
	}
	else {
		pSched->pReadyHead = pCo->__pReadyNext;
	}
	if ( pCo->__pReadyNext ) {
		pCo->__pReadyNext->__pReadyPrev = pCo->__pReadyPrev;
	}
	else {
		pSched->pReadyTail = pCo->__pReadyPrev;
	}
	pCo->__pReadyPrev = NULL;
	pCo->__pReadyNext = NULL;
	pCo->__iFlags &= ~__XRT_CO_FLAG_READY_QUEUED;
	return TRUE;
}
// 内部函数：确保协程 sched 定时器容量
static bool __xrt_co_sched_timer_ensure_capacity(xcosched* pSched)
{
	xcoro* arrNew = NULL;
	int iNewCap = 0;
	if ( pSched == NULL ) {
		return FALSE;
	}
	if ( pSched->iTimerCount < pSched->iTimerCapacity ) {
		return TRUE;
	}
	iNewCap = (pSched->iTimerCapacity <= 0) ? __XRT_CO_TIMER_INIT_CAP : (pSched->iTimerCapacity * 2);
	arrNew = (xcoro*)xrtRealloc(pSched->arrTimers, sizeof(xcoro) * iNewCap);
	if ( arrNew == NULL ) {
		return FALSE;
	}
	pSched->arrTimers = arrNew;
	pSched->iTimerCapacity = iNewCap;
	return TRUE;
}
// 内部函数：__xrt_co_sched_timer_swap
static void __xrt_co_sched_timer_swap(xcosched* pSched, int iA, int iB)
{
	xcoro pTmp = NULL;
	if ( pSched == NULL || iA == iB ) {
		return;
	}
	pTmp = pSched->arrTimers[iA];
	pSched->arrTimers[iA] = pSched->arrTimers[iB];
	pSched->arrTimers[iB] = pTmp;
	if ( pSched->arrTimers[iA] ) {
		pSched->arrTimers[iA]->__iTimerIndex = (uint32)(iA + 1);
	}
	if ( pSched->arrTimers[iB] ) {
		pSched->arrTimers[iB]->__iTimerIndex = (uint32)(iB + 1);
	}
}
// 内部函数：__xrt_co_sched_timer_sift_up
static void __xrt_co_sched_timer_sift_up(xcosched* pSched, int iIndex)
{
	while ( pSched && iIndex > 0 ) {
		int iParent = (iIndex - 1) / 2;
		xcoro pNode = pSched->arrTimers[iIndex];
		xcoro pParent = pSched->arrTimers[iParent];
		if ( pNode == NULL || pParent == NULL || pParent->__iWakeTime <= pNode->__iWakeTime ) {
			break;
		}
		__xrt_co_sched_timer_swap(pSched, iIndex, iParent);
		iIndex = iParent;
	}
}
// 内部函数：__xrt_co_sched_timer_sift_down
static void __xrt_co_sched_timer_sift_down(xcosched* pSched, int iIndex)
{
	while ( pSched ) {
		int iLeft = iIndex * 2 + 1;
		int iRight = iLeft + 1;
		int iSmallest = iIndex;
		if ( iLeft < pSched->iTimerCount &&
			pSched->arrTimers[iLeft] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iLeft]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iLeft;
		}
		if ( iRight < pSched->iTimerCount &&
			pSched->arrTimers[iRight] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iRight]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iRight;
		}
		if ( iSmallest == iIndex ) {
			break;
		}
		__xrt_co_sched_timer_swap(pSched, iIndex, iSmallest);
		iIndex = iSmallest;
	}
}
// 内部函数：__xrt_co_sched_detach_timer
static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;
	int iLast = 0;
	if ( pSched == NULL || pCo == NULL || (pCo->__iFlags & __XRT_CO_FLAG_TIMER_QUEUED) == 0 || pCo->__iTimerIndex == 0 ) {
		return FALSE;
	}
	iIndex = (int)pCo->__iTimerIndex - 1;
	iLast = pSched->iTimerCount - 1;
	if ( iIndex < 0 || iIndex >= pSched->iTimerCount ) {
		pCo->__iTimerIndex = 0;
		pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
		return FALSE;
	}
	if ( iIndex != iLast ) {
		__xrt_co_sched_timer_swap(pSched, iIndex, iLast);
	}
	pSched->arrTimers[iLast] = NULL;
	pSched->iTimerCount--;
	pCo->__iTimerIndex = 0;
	pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
	if ( iIndex < pSched->iTimerCount ) {
		__xrt_co_sched_timer_sift_down(pSched, iIndex);
		__xrt_co_sched_timer_sift_up(pSched, iIndex);
	}
	return TRUE;
}
// 内部函数：__xrt_co_sched_attach_timer
static bool __xrt_co_sched_attach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;
	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__iWakeTime <= 0 ) {
		return FALSE;
	}
	__xrt_co_sched_ready_unlink(pSched, pCo);
	if ( pCo->__iFlags & __XRT_CO_FLAG_TIMER_QUEUED ) {
		iIndex = (int)pCo->__iTimerIndex - 1;
		if ( iIndex >= 0 && iIndex < pSched->iTimerCount ) {
			__xrt_co_sched_timer_sift_down(pSched, iIndex);
			__xrt_co_sched_timer_sift_up(pSched, iIndex);
			return TRUE;
		}
		pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
		pCo->__iTimerIndex = 0;
	}
	if ( !__xrt_co_sched_timer_ensure_capacity(pSched) ) {
		return FALSE;
	}
	iIndex = pSched->iTimerCount++;
	pSched->arrTimers[iIndex] = pCo;
	pCo->__iFlags |= __XRT_CO_FLAG_TIMER_QUEUED;
	pCo->__iTimerIndex = (uint32)(iIndex + 1);
	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_sched_timer_sift_up(pSched, iIndex);
	return TRUE;
}
// 内部函数：__xrt_co_sched_mark_ready
static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__pSched != pSched ) {
		return FALSE;
	}
	if ( pCo->__pJoinTarget != NULL && !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
		return FALSE;
	}
	__xrt_co_sched_detach_timer(pSched, pCo);
	if ( pCo->__iFlags & __XRT_CO_FLAG_READY_QUEUED ) {
		pCo->iState = XRT_CO_READY;
		return TRUE;
	}
	pCo->__iWakeTime = 0;
	pCo->iState = XRT_CO_READY;
	pCo->__pReadyPrev = pSched->pReadyTail;
	pCo->__pReadyNext = NULL;
	if ( pSched->pReadyTail ) {
		pSched->pReadyTail->__pReadyNext = pCo;
	}
	else {
		pSched->pReadyHead = pCo;
	}
	pSched->pReadyTail = pCo;
	pCo->__iFlags |= __XRT_CO_FLAG_READY_QUEUED;
	return TRUE;
}
// 内部函数：__xrt_co_sched_collect_expired_timers
static void __xrt_co_sched_collect_expired_timers(xcosched* pSched, int64 iNow)
{
	while ( pSched && pSched->iTimerCount > 0 ) {
		xcoro pCo = pSched->arrTimers[0];
		if ( pCo == NULL ) {
			break;
		}
		if ( pCo->__iWakeTime > iNow ) {
			break;
		}
		__xrt_co_sched_detach_timer(pSched, pCo);
		if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
			xcoevent pEvent = (xcoevent)pCo->__pWaitObject;
			if ( pEvent && pEvent->pLock ) {
				xrtMutexLock(pEvent->pLock);
				pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_TIMEOUT;
				__xrt_co_detach_event_waiter_locked(pEvent, pCo);
				xrtMutexUnlock(pEvent->pLock);
			}
		}
		pCo->__iWakeTime = 0;
		__xrt_co_sched_mark_ready(pSched, pCo);
	}
}
// 内部函数：__xrt_co_sched_reap_dead
static bool __xrt_co_sched_reap_dead(xcosched* pSched, int iIndex)
{
	xcoro pCo = NULL;
	if ( pSched == NULL || iIndex < 0 || iIndex >= pSched->iCount ) {
		return FALSE;
	}
	pCo = pSched->arrCoros[iIndex];
	if ( pCo == NULL || pCo->iState != XRT_CO_DEAD ) {
		return FALSE;
	}
	__xrt_co_sched_ready_unlink(pSched, pCo);
	__xrt_co_sched_detach_timer(pSched, pCo);
	if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 ) {
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		if ( pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
	}
	if ( pCo->__iFlags & __XRT_CO_FLAG_JOIN_PINNED ) {
		return TRUE;
	}
	pCo->__pSched = NULL;
	__xrt_co_destroy_raw(pCo);
	pSched->arrCoros[iIndex] = NULL;
	return TRUE;
}
// 内部函数：__xrt_co_sched_release_dead
static bool __xrt_co_sched_release_dead(xcoro pCo)
{
	xcosched* pSched = NULL;
	if ( pCo == NULL || pCo->__pSched == NULL || pCo->iState != XRT_CO_DEAD ) {
		return FALSE;
	}
	pSched = (xcosched*)pCo->__pSched;
	int iIndex = __xrt_co_sched_find_index(pSched, pCo);
	if ( iIndex >= 0 ) {
		__xrt_co_sched_ready_unlink(pSched, pCo);
		__xrt_co_sched_detach_timer(pSched, pCo);
		if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 && pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
		pCo->__iFlags &= ~__XRT_CO_FLAG_JOIN_PINNED;
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		pCo->__pSched = NULL;
		__xrt_co_destroy_raw(pCo);
		pSched->arrCoros[iIndex] = NULL;
		return TRUE;
	}
	return FALSE;
}
// 内部函数：__xrt_co_sched_on_return
static void __xrt_co_sched_on_return(xcosched* pSched, xcoro pCo, int64 iNow)
{
	int iIndex = 0;
	if ( pSched == NULL || pCo == NULL ) {
		return;
	}
	if ( pCo->iState == XRT_CO_DEAD ) {
		iIndex = __xrt_co_sched_find_index(pSched, pCo);
		if ( iIndex >= 0 ) {
			__xrt_co_sched_reap_dead(pSched, iIndex);
		}
		return;
	}
	if ( pCo->__pSched != pSched ) {
		return;
	}
	if ( pCo->__pJoinTarget != NULL ) {
		if ( !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			return;
		}
	}
	if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
		if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			__xrt_co_sched_attach_timer(pSched, pCo);
			return;
		}
		else if ( pCo->__iWakeTime > 0 ) {
			xcoevent pEvent = (xcoevent)pCo->__pWaitObject;
			if ( pEvent && pEvent->pLock ) {
				xrtMutexLock(pEvent->pLock);
				pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_TIMEOUT;
				__xrt_co_detach_event_waiter_locked(pEvent, pCo);
				xrtMutexUnlock(pEvent->pLock);
			}
			pCo->__iWakeTime = 0;
			__xrt_co_sched_mark_ready(pSched, pCo);
			return;
		}
		pCo->iState = XRT_CO_SUSPENDED;
		return;
	}
	if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
		pCo->iState = XRT_CO_SUSPENDED;
		__xrt_co_sched_attach_timer(pSched, pCo);
		return;
	}
	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}
	__xrt_co_sched_mark_ready(pSched, pCo);
}
// 内部函数：__xrt_co_sched_next_wake_time
static int64 __xrt_co_sched_next_wake_time(xcosched* pSched, int64 iNow)
{
	if ( pSched == NULL ) {
		return 0;
	}
	if ( pSched->pReadyHead != NULL ) {
		return iNow;
	}
	if ( pSched->iTimerCount > 0 && pSched->arrTimers[0] ) {
		return pSched->arrTimers[0]->__iWakeTime;
	}
	return 0;
}
// 内部函数：__xrt_co_sched_has_pending_posts
static bool __xrt_co_sched_has_pending_posts(xcosched* pSched)
{
	bool bPending = FALSE;
	if ( pSched == NULL || pSched->pPostMutex == NULL ) {
		return FALSE;
	}
	xrtMutexLock(pSched->pPostMutex);
	bPending = (pSched->pPostHead != NULL);
	xrtMutexUnlock(pSched->pPostMutex);
	return bPending;
}
// 内部函数：__xrt_co_sched_post_item_alloc
static __xrt_co_post_item* __xrt_co_sched_post_item_alloc(xcosched* pSched)
{
	__xrt_co_post_item* pItem = NULL;
	if ( pSched == NULL ) {
		return NULL;
	}
	xrtMutexLock(pSched->pPostMutex);
	pItem = pSched->pPostFree;
	if ( pItem != NULL ) {
		pSched->pPostFree = pItem->pNext;
	}
	xrtMutexUnlock(pSched->pPostMutex);
	if ( pItem == NULL ) {
		pItem = (__xrt_co_post_item*)xrtMalloc(sizeof(__xrt_co_post_item));
	}
	if ( pItem != NULL ) {
		pItem->pCo = NULL;
		pItem->pNext = NULL;
	}
	return pItem;
}
// 内部函数：__xrt_co_sched_post_item_free
static void __xrt_co_sched_post_item_free(xcosched* pSched, __xrt_co_post_item* pItem)
{
	if ( pSched == NULL || pItem == NULL || pSched->pPostMutex == NULL ) {
		if ( pItem != NULL ) {
			xrtFree(pItem);
		}
		return;
	}
	xrtMutexLock(pSched->pPostMutex);
	pItem->pCo = NULL;
	pItem->pNext = pSched->pPostFree;
	pSched->pPostFree = pItem;
	xrtMutexUnlock(pSched->pPostMutex);
}
// 内部函数：__xrt_co_sched_drain_posts
static void __xrt_co_sched_drain_posts(xcosched* pSched)
{
	__xrt_co_post_item* pHead = NULL;
	if ( pSched == NULL || pSched->pPostMutex == NULL ) {
		return;
	}
	xrtMutexLock(pSched->pPostMutex);
	pHead = pSched->pPostHead;
	pSched->pPostHead = NULL;
	pSched->pPostTail = NULL;
	xrtMutexUnlock(pSched->pPostMutex);
	while ( pHead ) {
		__xrt_co_post_item* pNext = pHead->pNext;
		xcoro pCo = pHead->pCo;
		if ( pCo && pCo->__pSched == pSched && pCo->iState != XRT_CO_DEAD ) {
			if ( pCo->__pJoinTarget == NULL || __xrt_co_is_terminal(pCo->__pJoinTarget) ) {
				pCo->__iWakeTime = 0;
				__xrt_co_sched_detach_timer(pSched, pCo);
				__xrt_co_sched_mark_ready(pSched, pCo);
			}
		}
		__xrt_co_sched_post_item_free(pSched, pHead);
		pHead = pNext;
	}
}
// 内部函数：__xrt_co_sched_wait_for_post
static bool __xrt_co_sched_wait_for_post(xcosched* pSched, uint32 iTimeout)
{
	bool bReady = FALSE;
	if ( pSched == NULL || pSched->pPostMutex == NULL || pSched->pPostCond == NULL || iTimeout == 0 ) {
		return FALSE;
	}
	xrtMutexLock(pSched->pPostMutex);
	if ( pSched->pPostHead == NULL ) {
		if ( iTimeout == __XRT_CO_SCHED_WAIT_FOREVER ) {
			xrtCondWait(pSched->pPostCond, pSched->pPostMutex);
		}
		else {
			(void)xrtCondWaitTimeout(pSched->pPostCond, pSched->pPostMutex, iTimeout);
		}
	}
	bReady = (pSched->pPostHead != NULL);
	xrtMutexUnlock(pSched->pPostMutex);
	return bReady;
}
// 内部函数：__xrt_co_sched_compute_wait_timeout
static uint32 __xrt_co_sched_compute_wait_timeout(xcosched* pSched, uint32 iTimeout)
{
	int64 iNow = 0;
	int64 iNextWake = 0;
	bool bInfinite = (iTimeout == __XRT_CO_SCHED_WAIT_FOREVER);
	if ( pSched == NULL ) {
		return 0;
	}
	if ( pSched->pReadyHead != NULL || __xrt_co_sched_has_pending_posts(pSched) ) {
		return 0;
	}
	iNow = __xrt_co_time_ms();
	iNextWake = __xrt_co_sched_next_wake_time(pSched, iNow);
	if ( iNextWake > iNow ) {
		int64 iDelta = iNextWake - iNow;
		if ( !bInfinite && iDelta > iTimeout ) {
			iDelta = iTimeout;
		}
		if ( iDelta < 0 ) {
			iDelta = 0;
		}
		if ( iDelta > 0xFFFFFFFELL ) {
			iDelta = 0xFFFFFFFELL;
		}
		return (uint32)iDelta;
	}
	if ( iNextWake > 0 ) {
		return 0;
	}
	return bInfinite ? __XRT_CO_SCHED_WAIT_FOREVER : iTimeout;
}
// 内部函数：__xrt_co_sched_run_until
static bool __xrt_co_sched_run_until(xcosched* pSched, xcoro pTarget)
{
	if ( pSched == NULL || pTarget == NULL ) {
		return FALSE;
	}
	while ( !__xrt_co_is_terminal(pTarget) && pSched->iAlive > 0 ) {
		if ( !xrtCoSchedPollOnce(pSched, __XRT_CO_SCHED_WAIT_FOREVER) && !__xrt_co_is_terminal(pTarget) ) {
			break;
		}
		if ( __xrt_co_is_terminal(pTarget) ) {
			break;
		}
	}
	return __xrt_co_is_terminal(pTarget);
}
// 加入协程
XXAPI bool xrtCoJoin(xcoro pCo)
{
	xcoro pCurrent = NULL;
	xrtCoroRuntimeState* pRuntime = NULL;
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}
	pRuntime = __xrt_co_require_runtime(TRUE);
	if ( pRuntime == NULL ) {
		return FALSE;
	}
	pCurrent = __xrt_co_get_current();
	if ( pCurrent != NULL ) {
		if ( pCurrent == pCo ) {
			xrtSetError("coroutine cannot join itself.", FALSE);
			return FALSE;
		}
		if ( pCurrent->__pSched == NULL || pCo->__pSched == NULL || pCurrent->__pSched != pCo->__pSched ) {
			xrtSetError("coroutine join inside coroutine requires the same scheduler.", FALSE);
			return FALSE;
		}
		if ( pCurrent->__pJoinTarget != NULL && pCurrent->__pJoinTarget != pCo ) {
			xrtSetError("current coroutine is already waiting for another join target.", FALSE);
			return FALSE;
		}
		if ( !__xrt_co_is_terminal(pCo) ) {
			__xrt_co_join_pin_acquire(pCo);
			pCurrent->__pJoinTarget = pCo;
			pCurrent->__iWaitKind = __XRT_CO_WAIT_JOIN;
			__xrt_co_wait_queue_push_tail(&pCo->__pJoinWaitHead, &pCo->__pJoinWaitTail, pCurrent);
			pCurrent->iState = XRT_CO_SUSPENDED;
			__xrt_co_swap_to_main(pRuntime);
		}
		if ( pCurrent->__pJoinTarget == pCo && __xrt_co_is_terminal(pCo) ) {
			pCurrent->__pJoinTarget = NULL;
			if ( pCurrent->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
				pCurrent->__iWaitKind = __XRT_CO_WAIT_NONE;
			}
			__xrt_co_join_pin_release(pCo);
		}
		return __xrt_co_is_terminal(pCo);
	}
	if ( pCo->__pSched != NULL ) {
		bool bJoined = FALSE;
		__xrt_co_join_pin_acquire(pCo);
		bJoined = __xrt_co_sched_run_until((xcosched*)pCo->__pSched, pCo);
		__xrt_co_join_pin_release(pCo);
		return bJoined;
	}
	while ( !__xrt_co_is_terminal(pCo) ) {
		if ( !xrtCoResume(pCo) ) {
			return __xrt_co_is_terminal(pCo);
		}
	}
	return TRUE;
}
// xrtCoExit 相关处理
XXAPI void xrtCoExit(int64 iExitCode)
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_require_runtime(FALSE);
	if ( pCo == NULL || pRuntime == NULL ) {
		xrtSetError("xrtCoExit must be called from inside a coroutine.", FALSE);
		return;
	}
	__xrt_co_finish(
		pCo,
		__xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED,
		iExitCode
	);
	__xrt_co_swap_to_main(pRuntime);
}
// 设置协程 user 数据
XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData)
{
	if ( pCo == NULL ) {
		return;
	}
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return;
	}
	pCo->pUserData = pData;
}
// 获取协程 user 数据
XXAPI ptr xrtCoGetUserData(xcoro pCo)
{
	if ( !pCo ) return NULL;
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return NULL;
	}
	return pCo->pUserData;
}
// xrtCoPushCleanup 相关处理
XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;
	if ( pCo == NULL ) {
		xrtSetError("xrtCoPushCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	if ( proc == NULL ) {
		xrtSetError("coroutine cleanup proc is null.", FALSE);
		return FALSE;
	}
	pCleanup = (xco_cleanup*)xrtMalloc(sizeof(xco_cleanup));
	if ( pCleanup == NULL ) {
		return FALSE;
	}
	pCleanup->Proc = proc;
	pCleanup->Arg = pArg;
	pCleanup->pPrev = pCo->__pCleanupTop;
	pCo->__pCleanupTop = pCleanup;
	return TRUE;
}
// xrtCoPopCleanup 相关处理
XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;
	if ( pCo == NULL ) {
		xrtSetError("xrtCoPopCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	if ( pCo->__pCleanupTop == NULL ) {
		xrtSetError("coroutine cleanup stack is empty.", FALSE);
		return FALSE;
	}
	pCleanup = pCo->__pCleanupTop;
	if ( pCleanup->Proc != proc || pCleanup->Arg != pArg ) {
		xrtSetError("coroutine cleanup pop requires the current top cleanup.", FALSE);
		return FALSE;
	}
	pCo->__pCleanupTop = pCleanup->pPrev;
	if ( bExecute && pCleanup->Proc ) {
		pCo->__iFlags |= __XRT_CO_FLAG_IN_CLEANUP;
		pCleanup->Proc(pCleanup->Arg);
		pCo->__iFlags &= ~__XRT_CO_FLAG_IN_CLEANUP;
	}
	xrtFree(pCleanup);
	return TRUE;
}
// 内部函数：检查协程 sched 所有权
static bool __xrt_co_check_sched_owner(xcosched* pSched, const char* sError)
{
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}
	return __xrt_co_check_owner_tid(pSched->iOwnerThreadId, sError);
}
// xrtCoSchedCreate 相关处理
XXAPI xcosched* xrtCoSchedCreate()
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	xcosched* pSched = NULL;
	if ( pThreadData == NULL || pRuntime == NULL ) {
		return NULL;
	}
	pSched = (xcosched*)xrtMalloc(sizeof(xcosched));
	if ( !pSched ) return NULL;
	memset(pSched, 0, sizeof(xcosched));
	pSched->arrCoros = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	if ( !pSched->arrCoros ) {
		xrtFree(pSched);
		return NULL;
	}
	memset(pSched->arrCoros, 0, sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	pSched->arrTimers = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);
	if ( !pSched->arrTimers ) {
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	memset(pSched->arrTimers, 0, sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);
	pSched->pPostMutex = xrtMutexCreate();
	if ( pSched->pPostMutex == NULL ) {
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	pSched->pPostCond = xrtCondCreate();
	if ( pSched->pPostCond == NULL ) {
		xrtMutexDestroy(pSched->pPostMutex);
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	pSched->pThreadData = pThreadData;
	pSched->iOwnerThreadId = pThreadData->iThreadId;
	pSched->iCount = 0;
	pSched->iCapacity = __XRT_CO_SCHED_INIT_CAP;
	pSched->iAlive = 0;
	pSched->pReadyHead = NULL;
	pSched->pReadyTail = NULL;
	pSched->iTimerCount = 0;
	pSched->iTimerCapacity = __XRT_CO_TIMER_INIT_CAP;
	if ( pRuntime->pDefaultSched == NULL ) {
		pRuntime->pDefaultSched = pSched;
	}
	return pSched;
}
// xrtCoSchedCurrent 相关处理
XXAPI xcosched* xrtCoSchedCurrent()
{
	xcoro pCurrent = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();
	if ( pCurrent && pCurrent->__pSched ) {
		return (xcosched*)pCurrent->__pSched;
	}
	return pRuntime ? (xcosched*)pRuntime->pDefaultSched : NULL;
}
// xrtCoSchedDestroy 相关处理
XXAPI void xrtCoSchedDestroy(xcosched* pSched)
{
	xrtCoroRuntimeState* pRuntime = NULL;
	if ( pSched == NULL ) {
		return;
	}
	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return;
	}
	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("cannot destroy scheduler while a coroutine is running.", FALSE);
		return;
	}
	pRuntime = __xrt_co_get_runtime();
	__xrt_co_sched_drain_posts(pSched);
	while ( pSched->pPostFree != NULL ) {
		__xrt_co_post_item* pNext = pSched->pPostFree->pNext;
		xrtFree(pSched->pPostFree);
		pSched->pPostFree = pNext;
	}
	// 销毁所有协程
	for ( int i = 0; i < pSched->iCount; i++ ) {
		if ( pSched->arrCoros[i] ) {
			__xrt_co_sched_ready_unlink(pSched, pSched->arrCoros[i]);
			__xrt_co_sched_detach_timer(pSched, pSched->arrCoros[i]);
			pSched->arrCoros[i]->__pSched = NULL;
			__xrt_co_destroy_raw(pSched->arrCoros[i]);
			pSched->arrCoros[i] = NULL;
		}
	}
	if ( pRuntime && pRuntime->pDefaultSched == pSched ) {
		pRuntime->pDefaultSched = NULL;
	}
	if ( pSched->pPostCond ) {
		xrtCondDestroy(pSched->pPostCond);
	}
	if ( pSched->pPostMutex ) {
		xrtMutexDestroy(pSched->pPostMutex);
	}
	xrtFree(pSched->arrCoros);
	xrtFree(pSched->arrTimers);
	xrtFree(pSched);
}
// xrtCoSchedSpawn 相关处理
XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	xcoro pCo = NULL;
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return NULL;
	}
	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return NULL;
	}
	// 扩容检查
	if ( pSched->iCount >= pSched->iCapacity ) {
		int iNewCap = pSched->iCapacity * 2;
		xcoro* pNewArr = (xcoro*)xrtRealloc(pSched->arrCoros, sizeof(xcoro) * iNewCap);
		if ( !pNewArr ) return NULL;
		pSched->arrCoros = pNewArr;
		pSched->iCapacity = iNewCap;
	}
	pCo = xrtCoCreate(pfnEntry, pParam, iStackSize);
	if ( !pCo ) return NULL;
	pCo->__pSched = pSched;
	pSched->arrCoros[pSched->iCount] = pCo;
	pSched->iCount++;
	pSched->iAlive++;
	__xrt_co_sched_mark_ready(pSched, pCo);
	return pCo;
}
// xrtCoSchedPost 相关处理
XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo)
{
	__xrt_co_post_item* pItem = NULL;
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}
	if ( pCo && pCo->__pSched != pSched ) {
		xrtSetError("coroutine does not belong to target scheduler.", FALSE);
		return FALSE;
	}
	if ( pCo && pCo->iState == XRT_CO_DEAD ) {
		return TRUE;
	}
	pItem = __xrt_co_sched_post_item_alloc(pSched);
	if ( pItem == NULL ) {
		return FALSE;
	}
	pItem->pCo = pCo;
	pItem->pNext = NULL;
	xrtMutexLock(pSched->pPostMutex);
	if ( pSched->pPostTail ) {
		pSched->pPostTail->pNext = pItem;
	}
	else {
		pSched->pPostHead = pItem;
	}
	pSched->pPostTail = pItem;
	xrtCondSignal(pSched->pPostCond);
	xrtMutexUnlock(pSched->pPostMutex);
	return TRUE;
}
// xrtCoSchedPollOnce 相关处理
XXAPI bool xrtCoSchedPollOnce(xcosched* pSched, uint32 iTimeout)
{
	int64 iNow = 0;
	uint32 iWaitTimeout = 0;
	xcoro pCo = NULL;
	int iIndex = 0;
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}
	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return FALSE;
	}
	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("cannot poll scheduler from inside a running coroutine.", FALSE);
		return FALSE;
	}
	if ( pSched->iAlive <= 0 ) return FALSE;
	iNow = __xrt_co_time_ms();
	__xrt_co_sched_drain_posts(pSched);
	__xrt_co_sched_collect_expired_timers(pSched, iNow);
	if ( pSched->pReadyHead == NULL ) {
		iWaitTimeout = __xrt_co_sched_compute_wait_timeout(pSched, iTimeout);
		if ( iWaitTimeout > 0 ) {
			(void)__xrt_co_sched_wait_for_post(pSched, iWaitTimeout);
			iNow = __xrt_co_time_ms();
			__xrt_co_sched_drain_posts(pSched);
			__xrt_co_sched_collect_expired_timers(pSched, iNow);
		}
	}
	while ( pSched->pReadyHead != NULL ) {
		pCo = pSched->pReadyHead;
		__xrt_co_sched_ready_unlink(pSched, pCo);
		if ( pCo == NULL ) {
			break;
		}
		if ( pCo->iState == XRT_CO_DEAD ) {
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}
		if ( __xrt_co_is_cancel_requested_flag(pCo) && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
			__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}
		xrtCoResume(pCo);
		__xrt_co_sched_on_return(pSched, pCo, __xrt_co_time_ms());
		break;
	}
	return pSched->iAlive > 0;
}
// xrtCoSchedStep 相关处理
XXAPI bool xrtCoSchedStep(xcosched* pSched)
{
	return xrtCoSchedPollOnce(pSched, 0);
}
// xrtCoSchedRun 相关处理
XXAPI void xrtCoSchedRun(xcosched* pSched)
{
	if ( pSched == NULL ) {
		return;
	}
	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return;
	}
	while ( pSched->iAlive > 0 ) {
		if ( !xrtCoSchedPollOnce(pSched, __XRT_CO_SCHED_WAIT_FOREVER) ) {
			if ( pSched->iAlive <= 0 ) {
				break;
			}
		}
	}
}
// xrtCoSchedGetAlive 相关处理
XXAPI int xrtCoSchedGetAlive(xcosched* pSched)
{
	if ( !pSched ) return 0;
	return pSched->iAlive;
}
// 休眠协程直到指定时刻
XXAPI void xrtCoSleepUntil(int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();
	if ( !pCo ) {
		xrtSetError("xrtCoSleepUntil must be called from inside a coroutine.", FALSE);
		return;
	}
	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoSleepUntil requires a scheduler-managed coroutine.", FALSE);
		return;
	}
	pCo->__iWakeTime = iDeadlineMs;
	pCo->__iWaitKind = __XRT_CO_WAIT_TIMER;
	xrtCoYield();
}
// xrtCoWaitDeadline 相关处理
XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();
	if ( !pCo ) {
		xrtSetError("xrtCoWaitDeadline must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitDeadline requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}
	if ( iDeadlineMs > __xrt_co_time_ms() ) {
		pCo->__iWakeTime = iDeadlineMs;
		pCo->__iWaitKind = __XRT_CO_WAIT_TIMER;
		xrtCoYield();
	}
	return !__xrt_co_is_cancel_requested_flag(pCo);
}
// 创建协程事件
XXAPI xcoevent xrtCoEventCreate(bool bManualReset, bool bInitialState)
{
	xcoevent pEvent = (xcoevent)xrtCalloc(1, sizeof(xcoevent_struct));
	if ( pEvent == NULL ) {
		return NULL;
	}
	pEvent->pLock = xrtMutexCreate();
	if ( pEvent->pLock == NULL ) {
		xrtFree(pEvent);
		return NULL;
	}
	pEvent->bManualReset = bManualReset;
	pEvent->bSignaled = bInitialState;
	pEvent->pWaitHead = NULL;
	pEvent->pWaitTail = NULL;
	return pEvent;
}
// 销毁协程事件
XXAPI void xrtCoEventDestroy(xcoevent pEvent)
{
	xmutex pLock = NULL;
	if ( pEvent == NULL ) {
		return;
	}
	if ( pEvent->pLock ) {
		xrtMutexLock(pEvent->pLock);
		if ( pEvent->pWaitHead != NULL ) {
			xrtMutexUnlock(pEvent->pLock);
			xrtSetError("cannot destroy coroutine event while a waiter is attached.", FALSE);
			return;
		}
		pLock = pEvent->pLock;
		pEvent->pLock = NULL;
		xrtMutexUnlock(pLock);
		xrtMutexDestroy(pLock);
	}
	xrtFree(pEvent);
}
// 设置协程事件
XXAPI void xrtCoEventSet(xcoevent pEvent)
{
	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return;
	}
	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return;
	}
	for ( ;; ) {
		xcoro pWaiter = NULL;
		xcosched* pSched = NULL;
		bool bWakeAll = FALSE;
		xrtMutexLock(pEvent->pLock);
		bWakeAll = pEvent->bManualReset;
		if ( pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}
		pWaiter = __xrt_co_pop_event_waiter_locked(pEvent);
		if ( pWaiter == NULL && !pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}
		if ( pWaiter != NULL ) {
			pWaiter->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
			pSched = (xcosched*)pWaiter->__pSched;
		}
		xrtMutexUnlock(pEvent->pLock);
		if ( pWaiter != NULL && pSched != NULL ) {
			(void)xrtCoSchedPost(pSched, pWaiter);
		}
		if ( !bWakeAll || pWaiter == NULL ) {
			break;
		}
	}
}
// 重置协程事件
XXAPI void xrtCoEventReset(xcoevent pEvent)
{
	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return;
	}
	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return;
	}
	xrtMutexLock(pEvent->pLock);
	pEvent->bSignaled = FALSE;
	xrtMutexUnlock(pEvent->pLock);
}
// 内部函数：等待协程事件 core
static bool __xrt_co_wait_event_core(xcoevent pEvent, bool bInfinite, int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();
	int64 iNowMs = 0;
	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return FALSE;
	}
	if ( pCo == NULL ) {
		xrtSetError("xrtCoWaitEvent must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitEvent requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}
	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return FALSE;
	}
	pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_NONE;
	xrtMutexLock(pEvent->pLock);
	if ( __xrt_co_event_try_consume_locked(pEvent) ) {
		pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
		xrtMutexUnlock(pEvent->pLock);
		return TRUE;
	}
	iNowMs = __xrt_co_time_ms();
	if ( !bInfinite && iDeadlineMs <= iNowMs ) {
		xrtMutexUnlock(pEvent->pLock);
		return FALSE;
	}
	__xrt_co_wait_queue_push_tail(&pEvent->pWaitHead, &pEvent->pWaitTail, pCo);
	pCo->__pWaitObject = pEvent;
	pCo->__iWaitKind = __XRT_CO_WAIT_EVENT;
	if ( bInfinite ) {
		pCo->__iWakeTime = 0;
	}
	else {
		pCo->__iWakeTime = iDeadlineMs;
	}
	xrtMutexUnlock(pEvent->pLock);
	xrtCoYield();
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}
	return pCo->__iWaitResult == __XRT_CO_WAIT_RESULT_SIGNAL;
}
// 等待协程事件
XXAPI bool xrtCoWaitEvent(xcoevent pEvent)
{
	return __xrt_co_wait_event_core(pEvent, TRUE, 0);
}
// 等待协程事件直到指定时刻
XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs)
{
	return __xrt_co_wait_event_core(pEvent, FALSE, iDeadlineMs);
}
// 等待协程事件超时
XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs)
{
	if ( iTimeoutMs == XRT_CO_WAIT_INFINITE ) {
		return __xrt_co_wait_event_core(pEvent, TRUE, 0);
	}
	return __xrt_co_wait_event_core(pEvent, FALSE, __xrt_co_time_ms() + (int64)iTimeoutMs);
}
// 休眠协程
XXAPI void xrtCoSleep(uint32 iMs)
{
	xrtCoSleepUntil(__xrt_co_time_ms() + (int64)iMs);
}
#endif
#ifndef XRT_NO_NETWORK
#ifndef XRT_NO_XURL

// ========================================
// File: D:/git/xrt/lib/xurl.h
// ========================================

#ifndef XRT_XURL_H
#define XRT_XURL_H
/*
    XRT URL/query implementation layer.
    Public declarations live in xrt.h.
    This file is intended to be expanded only from xrt.c / single-head generation.
*/
// 构造字符串视图
XXAPI xrtstrview xrtStrView(const char* sPtr, size_t iLen)
{
	xrtstrview tView;
	tView.sPtr = sPtr;
	tView.iLen = iLen;
	return tView;
}
// 判断字符串视图是否为空
XXAPI bool xrtStrViewIsEmpty(xrtstrview tView)
{
	return tView.sPtr == NULL || tView.iLen == 0u;
}
// 复制字符串视图
XXAPI bool xrtStrViewCopyTo(xrtstrview tView, char* sOut, size_t iOutCap)
{
	if ( sOut == NULL || iOutCap == 0u ) return false;
	if ( xrtStrViewIsEmpty(tView) ) {
		sOut[0] = '\0';
		return true;
	}
	if ( tView.iLen >= iOutCap ) return false;
	memcpy(sOut, tView.sPtr, tView.iLen);
	sOut[tView.iLen] = '\0';
	return true;
}
// 内部函数：判断两个 ASCII 字符是否忽略大小写相等
static bool __xrtUrlAsciiEqNoCase(char chA, char chB)
{
	if ( chA >= 'A' && chA <= 'Z' ) chA = (char)(chA + 32);
	if ( chB >= 'A' && chB <= 'Z' ) chB = (char)(chB + 32);
	return chA == chB;
}
// 内部函数：判断 URL 视图与文本是否忽略大小写相等
static bool __xrtUrlViewEqNoCase(xrtstrview tView, const char* sText)
{
	size_t i = 0u;
	if ( sText == NULL ) return false;
	while ( sText[i] != '\0' ) {
		if ( i >= tView.iLen ) return false;
		if ( !__xrtUrlAsciiEqNoCase(tView.sPtr[i], sText[i]) ) return false;
		i++;
	}
	return i == tView.iLen;
}
// 内部函数：判断字符是否为控制字符或空白
static bool __xrtUrlIsCtlOrSpace(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU <= 0x20u || chU == 0x7Fu;
}
// 内部函数：校验文本中是否包含控制字符或空白
static bool __xrtUrlValidateNoCtlOrSpace(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(sText[i]) ) return false;
	}
	return true;
}
// 内部函数：判断是否为 URL 协议字符
static bool __xrtUrlIsSchemeChar(char ch, bool bFirst)
{
	if ( (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) return true;
	if ( bFirst ) return false;
	if ( ch >= '0' && ch <= '9' ) return true;
	return ch == '+' || ch == '-' || ch == '.';
}
// 内部函数：解析 URL 端口
static bool __xrtUrlParsePort(const char* sText, size_t iLen, uint16* pPort)
{
	uint32 iValue = 0u;
	size_t i;
	if ( sText == NULL || pPort == NULL || iLen == 0u || iLen > 5u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) return false;
		iValue = (iValue * 10u) + (uint32)(ch - '0');
		if ( iValue > 65535u ) return false;
	}
	if ( iValue == 0u ) return false;
	*pPort = (uint16)iValue;
	return true;
}
// 获取 URL 默认端口
XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme)
{
	if ( __xrtUrlViewEqNoCase(tScheme, "http") ) return 80u;
	if ( __xrtUrlViewEqNoCase(tScheme, "ws") ) return 80u;
	if ( __xrtUrlViewEqNoCase(tScheme, "https") ) return 443u;
	if ( __xrtUrlViewEqNoCase(tScheme, "wss") ) return 443u;
	return 0u;
}
// 判断是否为 URL 安全协议
XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme)
{
	return __xrtUrlViewEqNoCase(tScheme, "https") || __xrtUrlViewEqNoCase(tScheme, "wss");
}
// 内部函数：__xrtUrlHostNeedsBrackets
static bool __xrtUrlHostNeedsBrackets(xrtstrview tHost)
{
	size_t i;
	for ( i = 0u; i < tHost.iLen; ++i ) {
		if ( tHost.sPtr[i] == ':' ) return true;
	}
	return false;
}
// 内部函数：__xrtUrlAppendBytes
static bool __xrtUrlAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t iCur;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) return false;
	iCur = *pOffset;
	if ( iCur + iLen >= iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut + iCur, sText, iLen);
	iCur += iLen;
	sOut[iCur] = '\0';
	*pOffset = iCur;
	return true;
}
// 内部函数：追加 URL 端口
static bool __xrtUrlAppendPort(char* sOut, size_t iOutCap, size_t* pOffset, uint16 iPort)
{
	char sBuf[8];
	int iLen = snprintf(sBuf, sizeof(sBuf), "%u", (unsigned)iPort);
	if ( iLen <= 0 || (size_t)iLen >= sizeof(sBuf) ) return false;
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sBuf, (size_t)iLen);
}
// 判断是否为 URL 默认端口
XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL)
{
	uint16 iDefaultPort;
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tScheme) || pURL->iPort == 0u ) return false;
	iDefaultPort = xrtUrlDefaultPort(pURL->tScheme);
	return iDefaultPort != 0u && iDefaultPort == pURL->iPort;
}
// 判断是否为 URL 视图协议
XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme)
{
	return pURL != NULL && sScheme != NULL && !xrtStrViewIsEmpty(pURL->tScheme) && __xrtUrlViewEqNoCase(pURL->tScheme, sScheme);
}
// 判断 URL 视图是否匹配两个协议之一
XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB)
{
	return xrtUrlViewIsScheme(pURL, sSchemeA) || xrtUrlViewIsScheme(pURL, sSchemeB);
}
// 复制 URL 视图协议
XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tScheme, sOut, iOutCap);
}
// 复制 URL 视图授权段
XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tAuthority, sOut, iOutCap);
}
// 复制 URL 视图路径
XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tPath, sOut, iOutCap);
}
// 复制 URL 视图查询
XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tQuery, sOut, iOutCap);
}
// 复制 URL 视图片段
XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tFragment, sOut, iOutCap);
}
// 解析 URL 授权段
XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iAt = (size_t)-1;
	size_t iHostStart = 0u;
	size_t iColon = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL || iLen == 0u ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '@' ) iAt = i;
	}
	if ( iAt != (size_t)-1 ) {
		if ( iAt == 0u || iAt + 1u >= iLen ) return false;
		pOut->tUserInfo = xrtStrView(sText, iAt);
		pOut->iFlags |= XRT_URL_F_HAS_USERINFO;
		iHostStart = iAt + 1u;
	}
	if ( sText[iHostStart] == '[' ) {
		size_t iClose = (size_t)-1;
		for ( i = iHostStart + 1u; i < iLen; ++i ) {
			if ( sText[i] == ']' ) {
				iClose = i;
				break;
			}
		}
		if ( iClose == (size_t)-1 || iClose == iHostStart + 1u ) return false;
		pOut->tHost = xrtStrView(sText + iHostStart + 1u, iClose - iHostStart - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
		if ( iClose + 1u < iLen ) {
			if ( sText[iClose + 1u] != ':' ) return false;
			if ( !__xrtUrlParsePort(sText + iClose + 2u, iLen - iClose - 2u, &pOut->iPort) ) return false;
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		}
	} else {
		size_t iFirstColon = (size_t)-1;
		size_t iLastColon = (size_t)-1;
		for ( i = iHostStart; i < iLen; ++i ) {
			if ( sText[i] == ':' ) {
				if ( iFirstColon == (size_t)-1 ) iFirstColon = i;
				iLastColon = i;
			}
		}
		if ( iFirstColon != (size_t)-1 && iFirstColon != iLastColon ) return false;
		if ( iLastColon != (size_t)-1 ) iColon = iLastColon;
		if ( iColon != (size_t)-1 ) {
			if ( iColon == iHostStart || iColon + 1u >= iLen ) return false;
			pOut->tHost = xrtStrView(sText + iHostStart, iColon - iHostStart);
			if ( !__xrtUrlParsePort(sText + iColon + 1u, iLen - iColon - 1u, &pOut->iPort) ) return false;
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		} else {
			pOut->tHost = xrtStrView(sText + iHostStart, iLen - iHostStart);
		}
		if ( xrtStrViewIsEmpty(pOut->tHost) ) return false;
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
	}
	pOut->tAuthority = xrtStrView(sText, iLen);
	pOut->iFlags |= XRT_URL_F_HAS_AUTHORITY;
	return true;
}
// 解析 URL 授权段
XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseAuthorityN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// 解析 URL Target 部分
XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iQuery = (size_t)-1;
	size_t iFrag = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) iQuery = i;
	}
	pOut->iFlags = XRT_URL_F_TARGET_ONLY;
	if ( iQuery != (size_t)-1 ) pOut->iFlags |= XRT_URL_F_HAS_QUERY;
	if ( iFrag != (size_t)-1 ) pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT;
	if ( iQuery != (size_t)-1 ) {
		pOut->tPath = xrtStrView(sText, iQuery);
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, ((iFrag != (size_t)-1) ? iFrag : iLen) - iQuery - 1u);
	} else {
		pOut->tPath = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	}
	if ( pOut->tPath.iLen > 0u ) pOut->iFlags |= XRT_URL_F_HAS_PATH;
	if ( iFrag != (size_t)-1 && iFrag + 1u <= iLen ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
	}
	pOut->tTarget = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	return true;
}
// 解析 URL Target 部分
XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseTargetN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// 解析 URL 视图
XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iSchemeEnd = (size_t)-1;
	size_t iPos;
	size_t iPathStart;
	size_t iPathEnd;
	size_t iQuery = (size_t)-1;
	size_t iFrag = (size_t)-1;
	xrturlview tAuthority;
	size_t i;
	if ( pOut == NULL || sText == NULL ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == ':' ) {
			iSchemeEnd = i;
			break;
		}
		if ( ch == '/' || ch == '?' || ch == '#' ) break;
		if ( !__xrtUrlIsSchemeChar(ch, i == 0u) ) {
			iSchemeEnd = (size_t)-1;
			break;
		}
	}
	if ( iSchemeEnd == (size_t)-1 || iSchemeEnd + 2u >= iLen || sText[iSchemeEnd + 1u] != '/' || sText[iSchemeEnd + 2u] != '/' ) {
		return xrtUrlParseTargetN(sText, iLen, pOut);
	}
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	pOut->tScheme = xrtStrView(sText, iSchemeEnd);
	pOut->iFlags |= XRT_URL_F_ABSOLUTE;
	if ( xrtUrlIsSecureScheme(pOut->tScheme) ) pOut->iFlags |= XRT_URL_F_SECURE;
	iPos = iSchemeEnd + 3u;
	iPathStart = iPos;
	while ( iPathStart < iLen && sText[iPathStart] != '/' && sText[iPathStart] != '?' && sText[iPathStart] != '#' ) iPathStart++;
	if ( iPathStart == iPos ) return false;
	if ( !xrtUrlParseAuthorityN(sText + iPos, iPathStart - iPos, &tAuthority) ) return false;
	pOut->tAuthority = tAuthority.tAuthority;
	pOut->tUserInfo = tAuthority.tUserInfo;
	pOut->tHost = tAuthority.tHost;
	pOut->iPort = tAuthority.iPort;
	pOut->iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_HOST | XRT_URL_F_HAS_PORT);
	if ( (pOut->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
		pOut->iPort = xrtUrlDefaultPort(pOut->tScheme);
	}
	for ( i = iPathStart; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) iQuery = i;
	}
	iPathEnd = (iQuery != (size_t)-1) ? iQuery : ((iFrag != (size_t)-1) ? iFrag : iLen);
	pOut->tPath = xrtStrView(sText + iPathStart, iPathEnd - iPathStart);
	if ( pOut->tPath.iLen > 0u ) pOut->iFlags |= XRT_URL_F_HAS_PATH;
	if ( iQuery != (size_t)-1 ) {
		size_t iQueryEnd = (iFrag != (size_t)-1) ? iFrag : iLen;
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, iQueryEnd - iQuery - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_QUERY;
	}
	if ( iFrag != (size_t)-1 ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	pOut->tTarget = xrtStrView(sText + iPathStart, ((iFrag != (size_t)-1) ? iFrag : iLen) - iPathStart);
	return true;
}
// 解析 URL 视图
XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseViewN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// 复制 URL 视图主机
XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	return xrtStrViewCopyTo(pURL->tHost, sOut, iOutCap);
}
// 复制 URL 视图 Target 部分
XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( xrtStrViewIsEmpty(pURL->tTarget) ) {
		if ( iOutCap < 2u ) return false;
		sOut[0] = '/';
		sOut[1] = '\0';
		return true;
	}
	if ( pURL->tTarget.sPtr[0] == '?' ) {
		if ( pURL->tTarget.iLen + 2u > iOutCap ) return false;
		sOut[0] = '/';
		memcpy(sOut + 1u, pURL->tTarget.sPtr, pURL->tTarget.iLen);
		sOut[pURL->tTarget.iLen + 1u] = '\0';
		return true;
	}
	return xrtStrViewCopyTo(pURL->tTarget, sOut, iOutCap);
}
// 构建 URL 主机头部
XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	bool bDefaultPort;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	sOut[0] = '\0';
	bDefaultPort = pURL->iPort != 0u && pURL->iPort == xrtUrlDefaultPort(pURL->tScheme);
	if ( __xrtUrlHostNeedsBrackets(pURL->tHost) ) {
		if ( bDefaultPort || (pURL->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
			return snprintf(sOut, iOutCap, "[%.*s]", (int)pURL->tHost.iLen, pURL->tHost.sPtr) > 0 &&
				strlen(sOut) < iOutCap;
		}
		return snprintf(sOut, iOutCap, "[%.*s]:%u", (int)pURL->tHost.iLen, pURL->tHost.sPtr, (unsigned)pURL->iPort) > 0 &&
			strlen(sOut) < iOutCap;
	}
	if ( bDefaultPort || (pURL->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
		return snprintf(sOut, iOutCap, "%.*s", (int)pURL->tHost.iLen, pURL->tHost.sPtr) > 0 &&
			strlen(sOut) < iOutCap;
	}
	return snprintf(sOut, iOutCap, "%.*s:%u", (int)pURL->tHost.iLen, pURL->tHost.sPtr, (unsigned)pURL->iPort) > 0 &&
		strlen(sOut) < iOutCap;
}
// 构建 URL 主机头部固定长度
XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap)
{
	xrturlview tURL;
	if ( sScheme == NULL || sHost == NULL || sOut == NULL || iOutCap == 0u ) return false;
	memset(&tURL, 0, sizeof(tURL));
	tURL.tScheme = xrtStrView(sScheme, strlen(sScheme));
	tURL.tHost = xrtStrView(sHost, strlen(sHost));
	tURL.iPort = iPort;
	tURL.iFlags = XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_HOST;
	if ( iPort != 0u ) tURL.iFlags |= XRT_URL_F_HAS_PORT;
	if ( xrtUrlIsSecureScheme(tURL.tScheme) ) tURL.iFlags |= XRT_URL_F_SECURE;
	return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
}
// 内部函数：获取 URL 最后一次 segment
static bool __xrtUrlGetLastSegment(const char* sPath, size_t iLen, size_t* pStart, size_t* pLen)
{
	size_t i;
	if ( pStart ) *pStart = 0u;
	if ( pLen ) *pLen = 0u;
	if ( sPath == NULL || iLen == 0u ) return false;
	i = iLen;
	while ( i > 0u && sPath[i - 1u] == '/' ) i--;
	if ( i == 0u ) return false;
	{
		size_t iEnd = i;
		while ( i > 0u && sPath[i - 1u] != '/' ) i--;
		if ( pStart ) *pStart = i;
		if ( pLen ) *pLen = iEnd - i;
		return iEnd > i;
	}
}
// 内部函数：__xrtUrlAppendSegment
static bool __xrtUrlAppendSegment(char* sOut, size_t iOutCap, size_t* pOffset, const char* sSeg, size_t iSegLen)
{
	if ( sOut == NULL || pOffset == NULL || (sSeg == NULL && iSegLen != 0u) ) return false;
	if ( *pOffset > 0u && sOut[*pOffset - 1u] != '/' ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, pOffset, "/", 1u) ) return false;
	}
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sSeg, iSegLen);
}
// 内部函数：__xrtUrlAppendDotDotSegment
static bool __xrtUrlAppendDotDotSegment(char* sOut, size_t iOutCap, size_t* pOffset)
{
	return __xrtUrlAppendSegment(sOut, iOutCap, pOffset, "..", 2u);
}
// 内部函数：弹出 URL 最后一次 segment
static bool __xrtUrlPopLastSegment(char* sOut, size_t iOutCap, size_t* pOffset, bool bAbsolute)
{
	size_t iStart = 0u;
	size_t iLen = 0u;
	(void)iOutCap;
	if ( sOut == NULL || pOffset == NULL ) return false;
	if ( *pOffset == 0u ) return false;
	if ( bAbsolute && *pOffset == 1u && sOut[0] == '/' ) return false;
	if ( !__xrtUrlGetLastSegment(sOut, *pOffset, &iStart, &iLen) ) return false;
	if ( !bAbsolute && iLen == 2u && memcmp(sOut + iStart, "..", 2u) == 0 ) return false;
	*pOffset = iStart;
	if ( *pOffset > 0u && sOut[*pOffset - 1u] == '/' && !(bAbsolute && *pOffset == 1u) ) (*pOffset)--;
	sOut[*pOffset] = '\0';
	if ( bAbsolute && *pOffset == 0u ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		*pOffset = 1u;
	}
	return true;
}
// 规范化 URL 路径
XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	bool bAbsolute;
	bool bTrailingSlash;
	size_t i = 0u;
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sPath == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(sPath, iLen) ) return false;
	sOut[0] = '\0';
	bAbsolute = iLen > 0u && sPath[0] == '/';
	bTrailingSlash = iLen > 0u && sPath[iLen - 1u] == '/';
	if ( bAbsolute ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		iOff = 1u;
		while ( i < iLen && sPath[i] == '/' ) i++;
	}
	while ( i < iLen ) {
		size_t iSegStart = i;
		size_t iSegLen;
		while ( i < iLen && sPath[i] != '/' ) i++;
		iSegLen = i - iSegStart;
		if ( iSegLen == 0u || (iSegLen == 1u && sPath[iSegStart] == '.') ) {
		} else if ( iSegLen == 2u && sPath[iSegStart] == '.' && sPath[iSegStart + 1u] == '.' ) {
			if ( !__xrtUrlPopLastSegment(sOut, iOutCap, &iOff, bAbsolute) ) {
				if ( !bAbsolute ) {
					if ( !__xrtUrlAppendDotDotSegment(sOut, iOutCap, &iOff) ) return false;
				}
			}
		} else {
			if ( !__xrtUrlAppendSegment(sOut, iOutCap, &iOff, sPath + iSegStart, iSegLen) ) return false;
		}
		while ( i < iLen && sPath[i] == '/' ) i++;
	}
	if ( bTrailingSlash ) {
		if ( iOff == 0u ) {
			if ( bAbsolute ) {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
			} else {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "./", 2u) ) return false;
			}
		} else if ( sOut[iOff - 1u] != '/' ) {
			if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
		}
	}
	if ( bAbsolute && iOff == 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 URL Target 部分
XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) return false;
	sOut[0] = '\0';
	if ( !xrtStrViewIsEmpty(pURL->tPath) ) {
		if ( pURL->tPath.sPtr[0] != '/' ) return false;
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tPath.sPtr, pURL->tPath.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tPath.sPtr, pURL->tPath.iLen) ) return false;
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	}
	if ( !xrtStrViewIsEmpty(pURL->tQuery) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tQuery.sPtr, pURL->tQuery.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "?", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tQuery.sPtr, pURL->tQuery.iLen) ) return false;
	}
	if ( !xrtStrViewIsEmpty(pURL->tFragment) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tFragment.sPtr, pURL->tFragment.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "#", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tFragment.sPtr, pURL->tFragment.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 URL 授权段
XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	bool bNeedPort;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	sOut[0] = '\0';
	if ( !xrtStrViewIsEmpty(pURL->tUserInfo) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "@", 1u) ) return false;
	}
	if ( __xrtUrlHostNeedsBrackets(pURL->tHost) ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "[", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "]", 1u) ) return false;
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) return false;
	}
	bNeedPort = (pURL->iFlags & XRT_URL_F_HAS_PORT) != 0u || (pURL->iPort != 0u && !xrtUrlIsDefaultPort(pURL));
	if ( bNeedPort && pURL->iPort != 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, ":", 1u) ) return false;
		if ( !__xrtUrlAppendPort(sOut, iOutCap, &iOff, pURL->iPort) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 URL
XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	char sTarget[4096];
	size_t iTargetLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) return false;
	sOut[0] = '\0';
	if ( (pURL->iFlags & XRT_URL_F_ABSOLUTE) == 0u ) {
		return xrtUrlBuildTarget(pURL, sOut, iOutCap, pOutLen);
	}
	if ( xrtStrViewIsEmpty(pURL->tScheme) || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tScheme.sPtr, pURL->tScheme.iLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tScheme.sPtr, pURL->tScheme.iLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "://", 3u) ) return false;
	if ( !xrtUrlBuildAuthority(pURL, sOut + iOff, iOutCap - iOff, &iTargetLen) ) return false;
	iOff += iTargetLen;
	if ( !xrtUrlBuildTarget(pURL, sTarget, sizeof(sTarget), &iTargetLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, sTarget, iTargetLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 解析 URL
XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrturlview tRef;
	xrturlview tOut;
	char sMergedPath[4096];
	char sNormalizedPath[4096];
	size_t iNormLen = 0u;
	size_t iMergeLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pBase == NULL || sOut == NULL || iOutCap == 0u || sRef == NULL ) return false;
	if ( (pBase->iFlags & XRT_URL_F_ABSOLUTE) == 0u || xrtStrViewIsEmpty(pBase->tScheme) || xrtStrViewIsEmpty(pBase->tHost) ) return false;
	if ( iRefLen == 0u ) return xrtUrlBuild(pBase, sOut, iOutCap, pOutLen);
	if ( iRefLen >= 2u && sRef[0] == '/' && sRef[1] == '/' ) {
		size_t iAuthEnd = 2u;
		xrturlview tAuthority;
		memset(&tOut, 0, sizeof(tOut));
		while ( iAuthEnd < iRefLen && sRef[iAuthEnd] != '/' && sRef[iAuthEnd] != '?' && sRef[iAuthEnd] != '#' ) iAuthEnd++;
		if ( !xrtUrlParseAuthorityN(sRef + 2u, iAuthEnd - 2u, &tAuthority) ) return false;
		tOut = *pBase;
		tOut.tAuthority = tAuthority.tAuthority;
		tOut.tUserInfo = tAuthority.tUserInfo;
		tOut.tHost = tAuthority.tHost;
		tOut.iPort = (tAuthority.iFlags & XRT_URL_F_HAS_PORT) ? tAuthority.iPort : xrtUrlDefaultPort(pBase->tScheme);
		tOut.iFlags &= ~(XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT | XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		tOut.iFlags |= XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_HOST | (pBase->iFlags & XRT_URL_F_SECURE);
		tOut.iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT);
		if ( iAuthEnd < iRefLen ) {
			if ( !xrtUrlParseTargetN(sRef + iAuthEnd, iRefLen - iAuthEnd, &tRef) ) return false;
			tOut.tPath = tRef.tPath;
			tOut.tQuery = tRef.tQuery;
			tOut.tFragment = tRef.tFragment;
			tOut.tTarget = tRef.tTarget;
			tOut.iFlags |= tRef.iFlags & (XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		}
		return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
	}
	if ( !xrtUrlParseViewN(sRef, iRefLen, &tRef) ) return false;
	if ( (tRef.iFlags & XRT_URL_F_ABSOLUTE) != 0u ) return xrtUrlBuild(&tRef, sOut, iOutCap, pOutLen);
	tOut = *pBase;
	tOut.iFlags &= ~(XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
	tOut.tQuery = xrtStrView(NULL, 0u);
	tOut.tFragment = xrtStrView(NULL, 0u);
	if ( (tRef.iFlags & XRT_URL_F_HAS_PATH) != 0u && !xrtStrViewIsEmpty(tRef.tPath) ) {
		if ( tRef.tPath.sPtr[0] == '/' ) {
			if ( !xrtUrlNormalizePathTo(tRef.tPath.sPtr, tRef.tPath.iLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) return false;
		} else {
			size_t iBaseDirLen = 0u;
			if ( !xrtStrViewIsEmpty(pBase->tPath) ) {
				size_t i;
				for ( i = pBase->tPath.iLen; i > 0u; --i ) {
					if ( pBase->tPath.sPtr[i - 1u] == '/' ) {
						iBaseDirLen = i;
						break;
					}
				}
				if ( iBaseDirLen == 0u ) iBaseDirLen = 1u;
			} else {
				iBaseDirLen = 1u;
			}
			if ( iBaseDirLen == 1u && (xrtStrViewIsEmpty(pBase->tPath) || pBase->tPath.sPtr[0] != '/') ) {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, "/", 1u) ) return false;
			} else {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, pBase->tPath.sPtr, iBaseDirLen) ) return false;
			}
			if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, tRef.tPath.sPtr, tRef.tPath.iLen) ) return false;
			if ( !xrtUrlNormalizePathTo(sMergedPath, iMergeLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) return false;
		}
		tOut.tPath = xrtStrView(sNormalizedPath, iNormLen);
		tOut.iFlags |= XRT_URL_F_HAS_PATH;
		if ( (tRef.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
			tOut.tQuery = tRef.tQuery;
			tOut.iFlags |= XRT_URL_F_HAS_QUERY;
		}
		if ( (tRef.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) {
			tOut.tFragment = tRef.tFragment;
			tOut.iFlags |= XRT_URL_F_HAS_FRAGMENT;
		}
		return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
	}
	tOut.tPath = pBase->tPath;
	if ( !xrtStrViewIsEmpty(pBase->tPath) ) tOut.iFlags |= XRT_URL_F_HAS_PATH;
	if ( (tRef.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = tRef.tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	} else if ( (pBase->iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = pBase->tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	}
	if ( (tRef.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) {
		tOut.tFragment = tRef.tFragment;
		tOut.iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
}
// 解析 URL
XXAPI bool xrtUrlResolve(const xrturlview* pBase, const char* sRef, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sRef == NULL ) return false;
	return xrtUrlResolveTo(pBase, sRef, strlen(sRef), sOut, iOutCap, pOutLen);
}
// 获取下一个查询
XXAPI bool xrtQueryNextN(const char* sQuery, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	size_t iCur;
	size_t iAmp;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sQuery == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	if ( iCur == 0u && iLen > 0u && sQuery[0] == '?' ) iCur = 1u;
	while ( iCur < iLen && sQuery[iCur] == '&' ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iAmp = iCur;
	while ( iAmp < iLen && sQuery[iAmp] != '&' ) {
		if ( sQuery[iAmp] == '=' && iEq == (size_t)-1 ) iEq = iAmp;
		iAmp++;
	}
	memset(pOut, 0, sizeof(xrtquerypair));
	if ( iEq != (size_t)-1 ) {
		pOut->tKey = xrtStrView(sQuery + iCur, iEq - iCur);
		pOut->tValue = xrtStrView(sQuery + iEq + 1u, iAmp - iEq - 1u);
		pOut->iFlags |= XRT_QUERY_F_HAS_VALUE;
	} else {
		pOut->tKey = xrtStrView(sQuery + iCur, iAmp - iCur);
	}
	for ( i = 0u; i < pOut->tKey.iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(pOut->tKey.sPtr[i]) ) return false;
	}
	*pOffset = (iAmp < iLen) ? (iAmp + 1u) : iAmp;
	return true;
}
// 获取下一个查询
XXAPI bool xrtQueryNext(const char* sQuery, size_t* pOffset, xrtquerypair* pOut)
{
	if ( sQuery == NULL ) return false;
	return xrtQueryNextN(sQuery, strlen(sQuery), pOffset, pOut);
}
// 统计查询
XXAPI size_t xrtQueryCountN(const char* sQuery, size_t iLen)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL ) return 0u;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) iCount++;
	return iCount;
}
// 统计查询
XXAPI size_t xrtQueryCount(const char* sQuery)
{
	if ( sQuery == NULL ) return 0u;
	return xrtQueryCountN(sQuery, strlen(sQuery));
}
// 查找查询
XXAPI bool xrtQueryFindN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, xrtquerypair* pOut)
{
	size_t iOffset = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL || sKey == NULL ) return false;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == iKeyLen && memcmp(tPair.tKey.sPtr, sKey, iKeyLen) == 0 ) {
			if ( pOut ) *pOut = tPair;
			return true;
		}
	}
	return false;
}
// 查找查询
XXAPI bool xrtQueryFind(const char* sQuery, const char* sKey, xrtquerypair* pOut)
{
	if ( sQuery == NULL || sKey == NULL ) return false;
	return xrtQueryFindN(sQuery, strlen(sQuery), sKey, strlen(sKey), pOut);
}
// 解析查询
XXAPI bool xrtQueryParseToN(const char* sQuery, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( pCount ) *pCount = 0u;
	if ( sQuery == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tPair;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}
// 解析查询
XXAPI bool xrtQueryParseTo(const char* sQuery, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sQuery == NULL ) return false;
	return xrtQueryParseToN(sQuery, strlen(sQuery), pOut, iCap, pCount);
}
// 内部函数：获取 URL hex 值
static int __xrtUrlHexValue(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return (int)(ch - '0');
	if ( ch >= 'a' && ch <= 'f' ) return 10 + (int)(ch - 'a');
	if ( ch >= 'A' && ch <= 'F' ) return 10 + (int)(ch - 'A');
	return -1;
}
// xrtPercentDecodeTo 相关处理
XXAPI bool xrtPercentDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bPlusAsSpace)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == '%' ) {
			int iHi;
			int iLo;
			if ( i + 2u >= iLen ) return false;
			iHi = __xrtUrlHexValue(sText[i + 1u]);
			iLo = __xrtUrlHexValue(sText[i + 2u]);
			if ( iHi < 0 || iLo < 0 ) return false;
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = (char)((iHi << 4) | iLo);
			i += 2u;
			continue;
		}
		if ( ch == '+' && bPlusAsSpace ) ch = ' ';
		if ( iOut + 1u >= iOutCap ) return false;
		sOut[iOut++] = ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}
// 解析 URL 固定长度
XXAPI bool xrtUrlParseFixedTo(const char* sURL, const char* sSchemeA, const char* sSchemeB, bool* pSchemeB, char* sHost, size_t iHostCap, uint16* pPort, char* sTarget, size_t iTargetCap)
{
	xrturlview tURL;
	if ( sURL == NULL || sSchemeA == NULL || sHost == NULL || iHostCap == 0u || sTarget == NULL || iTargetCap == 0u ) return false;
	if ( !xrtUrlParseView(sURL, &tURL) ) return false;
	if ( (tURL.iFlags & XRT_URL_F_ABSOLUTE) == 0u ) return false;
	if ( sSchemeB != NULL ) {
		if ( !xrtUrlViewMatchesScheme2(&tURL, sSchemeA, sSchemeB) ) return false;
	} else if ( !xrtUrlViewIsScheme(&tURL, sSchemeA) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tURL, sHost, iHostCap) ) return false;
	if ( !xrtUrlViewCopyTargetTo(&tURL, sTarget, iTargetCap) ) return false;
	if ( pPort ) *pPort = tURL.iPort;
	if ( pSchemeB ) *pSchemeB = (sSchemeB != NULL) ? xrtUrlViewIsScheme(&tURL, sSchemeB) : false;
	return true;
}
// 解析 URL
XXAPI bool xrtUrlParse(const char* sURL, xurl pOut)
{
	if ( pOut == NULL || sURL == NULL ) return false;
	memset(pOut, 0, sizeof(xurl_struct));
	if ( !xrtUrlParseFixedTo(sURL, "http", "https", &pOut->bHttps, pOut->sHost, sizeof(pOut->sHost), &pOut->iPort, pOut->sPath, sizeof(pOut->sPath)) ) return false;
	return true;
}
#endif
#endif
#ifndef XRT_NO_HTTP_UTIL

// ========================================
// File: D:/git/xrt/lib/xhttp_util.h
// ========================================

#ifndef XRT_XHTTP_UTIL_H
#define XRT_XHTTP_UTIL_H
#ifndef XHTTP_UTIL_REALLOC
	#define XHTTP_UTIL_REALLOC realloc
#endif
#ifndef XHTTP_UTIL_FREE
	#define XHTTP_UTIL_FREE free
#endif
#define XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY 1u
#define XRT_MULTIPART_STREAM_STATE_HEADERS       2u
#define XRT_MULTIPART_STREAM_STATE_BODY          3u
#define XRT_MULTIPART_STREAM_STATE_PART_END      4u
#define XRT_MULTIPART_STREAM_STATE_DONE          5u
#define XRT_MULTIPART_STREAM_STATE_ERROR         6u
/*
    XRT HTTP utility implementation layer.
    Public declarations live in xrt.h.
    This file is intended to be expanded only from xrt.c / single-head generation.
*/
static char __xrtHttpUtilToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}
// 内部函数：判断两段文本是否忽略大小写相等
static bool __xrtHttpUtilEqNoCaseN(const char* sA, size_t iLenA, const char* sB, size_t iLenB)
{
	size_t i;
	if ( sA == NULL || sB == NULL || iLenA != iLenB ) return false;
	for ( i = 0u; i < iLenA; ++i ) {
		if ( __xrtHttpUtilToLower(sA[i]) != __xrtHttpUtilToLower(sB[i]) ) return false;
	}
	return true;
}
// 内部函数：判断是否为 HTTP Token 字符
static bool __xrtHttpUtilIsTokenChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return true;
	if ( ch >= 'A' && ch <= 'Z' ) return true;
	if ( ch >= 'a' && ch <= 'z' ) return true;
	switch ( ch ) {
		case '!': case '#': case '$': case '%': case '&': case '\'':
		case '*': case '+': case '-': case '.': case '^': case '_':
		case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}
// 内部函数：判断是否为 Cookie Octet 字符
static bool __xrtHttpUtilIsCookieOctet(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU >= 0x21u && chU <= 0x7Eu && ch != '"' && ch != ',' && ch != ';' && ch != '\\';
}
// 内部函数：判断文本中是否包含控制字符
static bool __xrtHttpUtilContainsCtl(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) return true;
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return true;
	}
	return false;
}
// 内部函数：裁剪 HTTP 视图两端空白
static void __xrtHttpUtilTrimView(xrtstrview* pView)
{
	if ( pView == NULL || pView->sPtr == NULL ) return;
	while ( pView->iLen > 0u && (pView->sPtr[0] == ' ' || pView->sPtr[0] == '\t') ) {
		pView->sPtr++;
		pView->iLen--;
	}
	while ( pView->iLen > 0u && (pView->sPtr[pView->iLen - 1u] == ' ' || pView->sPtr[pView->iLen - 1u] == '\t') ) {
		pView->iLen--;
	}
}
// 内部函数：解析 32 位整数
static bool __xrtHttpUtilParseInt32(const char* sText, size_t iLen, int32_t* pOut)
{
	bool bNeg = false;
	size_t i = 0u;
	int64_t iValue = 0;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( sText[0] == '-' || sText[0] == '+' ) {
		bNeg = sText[0] == '-';
		i = 1u;
		if ( i >= iLen ) return false;
	}
	for ( ; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) return false;
		iValue = (iValue * 10) + (int64_t)(ch - '0');
		if ( !bNeg && iValue > 2147483647LL ) return false;
		if ( bNeg && iValue > 2147483648LL ) return false;
	}
	*pOut = bNeg ? (int32_t)(-iValue) : (int32_t)iValue;
	return true;
}
// 内部函数：向输出缓冲区追加字节
static bool __xrtHttpUtilAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t iCur;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) return false;
	iCur = *pOffset;
	if ( iCur + iLen >= iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut + iCur, sText, iLen);
	iCur += iLen;
	sOut[iCur] = '\0';
	*pOffset = iCur;
	return true;
}
// 内部函数：校验 Multipart 边界
static bool __xrtHttpUtilValidateBoundaryN(const char* sBoundary, size_t iBoundaryLen)
{
	size_t i;
	if ( sBoundary == NULL || iBoundaryLen == 0u || iBoundaryLen > 70u ) return false;
	for ( i = 0u; i < iBoundaryLen; ++i ) {
		unsigned char ch = (unsigned char)sBoundary[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return false;
	}
	return true;
}
// 初始化 HTTP 工具限制配置
XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits)
{
	if ( !pLimits ) return;
	pLimits->iMaxNameBytes = 256u;
	pLimits->iMaxValueBytes = 4096u;
	pLimits->iMaxPairs = 128u;
	pLimits->iMaxHeaderLineBytes = 8192u;
	pLimits->iMaxHeaderBytes = 64u * 1024u;
	pLimits->iMaxHeaderCount = 64u;
	pLimits->iMaxTokenBytes = 256u;
	pLimits->iMaxBoundaryBytes = 70u;
	pLimits->iMaxMultipartHeaders = 64u;
	pLimits->iMaxMultipartParts = 64u;
	pLimits->iMaxMultipartBytes = 256u * 1024u;
}
// 内部函数：解析 HTTP 工具限制配置
static const xrthttputillimits* __xrtHttpUtilResolveLimits(const xrthttputillimits* pIn, xrthttputillimits* pLocal)
{
	if ( pIn ) return pIn;
	xrtHttpUtilLimitsInit(pLocal);
	return pLocal;
}
// 应用 Multipart 流限制配置
XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	if ( !pConfig ) return;
	pConfig->iMaxBufferedBytes = pResolved->iMaxMultipartBytes;
	pConfig->iMaxHeaderBytes = pResolved->iMaxHeaderBytes;
	pConfig->iMaxPartHeaders = pResolved->iMaxMultipartHeaders;
	if ( pResolved->iMaxBoundaryBytes > 0u ) {
		pConfig->iTailReserve = pResolved->iMaxBoundaryBytes + 8u;
	}
}
// 校验 HTTP Token
XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( sText == NULL ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == 0u || tToken.iLen > pResolved->iMaxTokenBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}
// 校验 HTTP Token
XXAPI bool xrtHttpTokenValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验 HTTP 参数
XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrthttpparam tParam;
	if ( sText == NULL ) return false;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( tParam.tName.iLen == 0u || tParam.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u && tParam.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}
// 校验 HTTP 参数
XXAPI bool xrtHttpParamValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtHttpParamValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验查询
XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sText == NULL ) return false;
	while ( xrtQueryNextN(sText, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == 0u || tPair.tKey.iLen > pResolved->iMaxNameBytes ) return false;
		if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u && tPair.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}
// 校验查询
XXAPI bool xrtQueryValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtQueryValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验 Cookie
XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}
// 校验 Cookie
XXAPI bool xrtCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtCookieValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验表单 URL 编码
XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	return xrtQueryValidateN(sText, iLen, pLimits);
}
// 校验表单 URL 编码
XXAPI bool xrtFormUrlEncodedValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtFormUrlEncodedValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验 HTTP 头部块
XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtheaderpair tHeader;
	if ( sBlock == NULL ) return false;
	if ( iLen > pResolved->iMaxHeaderBytes ) return false;
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
		if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) return false;
		if ( ++iCount > pResolved->iMaxHeaderCount ) return false;
	}
	return iOffset == iLen;
}
// 校验 HTTP 头部块
XXAPI bool xrtHttpHeaderBlockValidate(const char* sBlock, const xrthttputillimits* pLimits)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderBlockValidateN(sBlock, strlen(sBlock), pLimits);
}
// xrtSetCookieValidateN 相关处理
XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	xrtsetcookieview tCookie;
	if ( !xrtSetCookieParseN(sText, iLen, &tCookie) ) return false;
	if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) return false;
	if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u && tCookie.tDomain.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u && tCookie.tPath.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u && tCookie.tExpires.iLen > pResolved->iMaxValueBytes ) return false;
	return true;
}
// xrtSetCookieValidate 相关处理
XXAPI bool xrtSetCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtSetCookieValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}
// 校验 Multipart
XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iPartCount = 0u;
	xrtmultipartpartview tPart;
	size_t iHeaderOff;
	size_t iHeaderCount;
	xrtheaderpair tHeader;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	if ( iBoundaryLen == 0u || iBoundaryLen > pResolved->iMaxBoundaryBytes ) return false;
	if ( iLen > pResolved->iMaxMultipartBytes ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		if ( ++iPartCount > pResolved->iMaxMultipartParts ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u && (tPart.tName.iLen == 0u || tPart.tName.iLen > pResolved->iMaxNameBytes) ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u && tPart.tFileName.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u && tPart.tFileNameExt.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u && tPart.tContentType.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_TRANSFER_ENCODING) != 0u && tPart.tTransferEncoding.iLen > pResolved->iMaxValueBytes ) return false;
		iHeaderOff = 0u;
		iHeaderCount = 0u;
		while ( xrtHttpHeaderNextLineN(tPart.tHeaders.sPtr, tPart.tHeaders.iLen, &iHeaderOff, &tHeader) ) {
			size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
			if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) return false;
			if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) return false;
			if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) return false;
			if ( ++iHeaderCount > pResolved->iMaxMultipartHeaders ) return false;
		}
		if ( iHeaderOff != tPart.tHeaders.iLen ) return false;
	}
	return true;
}
// 校验 Multipart
XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartValidateN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pLimits);
}
// 内部函数：__xrtHttpUtilIsAttrChar
static bool UNUSED_ATTR __xrtHttpUtilIsAttrChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return true;
	if ( ch >= 'A' && ch <= 'Z' ) return true;
	if ( ch >= 'a' && ch <= 'z' ) return true;
	switch ( ch ) {
		case '!': case '#': case '$': case '&': case '+': case '-':
		case '.': case '^': case '_': case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}
// 内部函数：__xrtHttpUtilHexDigit
static char __xrtHttpUtilHexDigit(uint8 iValue)
{
	if ( iValue < 10u ) {
		return (char)('0' + (char)iValue);
	}
	return (char)('A' + (char)(iValue - 10u));
}
// 内部函数：追加 HTTP util 引用字符串
static bool __xrtHttpUtilAppendQuotedString(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		if ( ch == '"' || ch == '\\' ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\\", 1u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, &ch, 1u) ) return false;
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u);
}
// 判断是否为 HTTP Token
XXAPI bool xrtHttpIsTokenN(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL || iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) return false;
	}
	return true;
}
// 判断是否为 HTTP Token
XXAPI bool xrtHttpIsToken(const char* sText)
{
	if ( sText == NULL ) return false;
	return xrtHttpIsTokenN(sText, strlen(__xrt_cstr(sText)));
}
// 解码 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringDecodeToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iIn;
	size_t iOut = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( iLen < 2u || sText[0] != '"' || sText[iLen - 1u] != '"' ) return false;
	sOut[0] = '\0';
	for ( iIn = 1u; iIn + 1u < iLen; ++iIn ) {
		unsigned char ch = (unsigned char)sText[iIn];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		if ( ch == '\\' ) {
			if ( iIn + 2u > iLen - 1u ) return false;
			ch = (unsigned char)sText[++iIn];
			if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		} else if ( ch == '"' ) {
			return false;
		}
		if ( iOut + 1u >= iOutCap ) return false;
		sOut[iOut++] = (char)ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}
// 解码 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringDecodeTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpQuotedStringDecodeToN(sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}
// 构建 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringBuildToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (sText == NULL && iLen != 0u) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, &iOff, sText, iLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringBuildTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpQuotedStringBuildToN(sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}
// xrtPercentEncodeTo 相关处理
XXAPI bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		bool bKeep = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
			ch == '-' || ch == '_' || ch == '.' || ch == '~';
		if ( ch == ' ' && bSpaceAsPlus ) {
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = '+';
			continue;
		}
		if ( bKeep ) {
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = (char)ch;
			continue;
		}
		if ( iOut + 3u >= iOutCap ) return false;
		sOut[iOut++] = '%';
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)((ch >> 4) & 0x0Fu));
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)(ch & 0x0Fu));
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}
// 内部函数：解析 HTTP util 扩展值视图
static bool __xrtHttpUtilParseExtValueView(xrtstrview tRaw, xrtstrview* pCharset, xrtstrview* pLanguage, xrtstrview* pEncoded)
{
	size_t iFirstTick = (size_t)-1;
	size_t iSecondTick = (size_t)-1;
	size_t i;
	if ( pCharset ) *pCharset = xrtStrView(NULL, 0u);
	if ( pLanguage ) *pLanguage = xrtStrView(NULL, 0u);
	if ( pEncoded ) *pEncoded = xrtStrView(NULL, 0u);
	if ( xrtStrViewIsEmpty(tRaw) ) return false;
	for ( i = 0u; i < tRaw.iLen; ++i ) {
		if ( tRaw.sPtr[i] == '\'' ) {
			if ( iFirstTick == (size_t)-1 ) iFirstTick = i;
			else {
				iSecondTick = i;
				break;
			}
		}
	}
	if ( iFirstTick == (size_t)-1 || iSecondTick == (size_t)-1 || iFirstTick == 0u ) return false;
	for ( i = 0u; i < iFirstTick; ++i ) {
		unsigned char ch = (unsigned char)tRaw.sPtr[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(tRaw.sPtr[i]) ) return false;
	}
	if ( pCharset ) *pCharset = xrtStrView(tRaw.sPtr, iFirstTick);
	if ( pLanguage ) *pLanguage = xrtStrView(tRaw.sPtr + iFirstTick + 1u, iSecondTick - iFirstTick - 1u);
	if ( pEncoded ) *pEncoded = xrtStrView(tRaw.sPtr + iSecondTick + 1u, tRaw.iLen - iSecondTick - 1u);
	return true;
}
// 解码 HTTP 扩展值
XXAPI bool xrtHttpDecodeExtValueTo(const char* sText, size_t iLen, xrtstrview* pCharset, xrtstrview* pLanguage, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrtstrview tCharset;
	xrtstrview tLanguage;
	xrtstrview tEncoded;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtHttpUtilParseExtValueView(xrtStrView(sText, iLen), &tCharset, &tLanguage, &tEncoded) ) return false;
	if ( !xrtPercentDecodeTo(tEncoded.sPtr, tEncoded.iLen, sOut, iOutCap, pOutLen, false) ) return false;
	if ( pCharset ) *pCharset = tCharset;
	if ( pLanguage ) *pLanguage = tLanguage;
	return true;
}
// 解码 HTTP 扩展值
XXAPI bool xrtHttpDecodeExtValue(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpDecodeExtValueTo(sText, strlen(__xrt_cstr(sText)), NULL, NULL, sOut, iOutCap, pOutLen);
}
// 构建 HTTP 扩展值
XXAPI bool xrtHttpBuildExtValueTo(const char* sCharset, const char* sLanguage, const char* sText, size_t iTextLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iEncLen = 0u;
	size_t iCharsetLen;
	size_t iLanguageLen;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( sCharset == NULL || sCharset[0] == '\0' ) sCharset = "UTF-8";
	if ( sLanguage == NULL ) sLanguage = "";
	iCharsetLen = strlen(sCharset);
	iLanguageLen = strlen(sLanguage);
	for ( i = 0u; i < iCharsetLen; ++i ) {
		unsigned char ch = (unsigned char)sCharset[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(sCharset[i]) ) return false;
	}
	for ( i = 0u; i < iLanguageLen; ++i ) {
		unsigned char ch = (unsigned char)sLanguage[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || sLanguage[i] == '\'' ) return false;
	}
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sCharset, iCharsetLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sLanguage, iLanguageLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) return false;
	if ( !xrtPercentEncodeTo(sText, iTextLen, sOut + iOff, iOutCap - iOff, &iEncLen, false) ) return false;
	iOff += iEncLen;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 HTTP 扩展值
XXAPI bool xrtHttpBuildExtValue(const char* sCharset, const char* sLanguage, const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpBuildExtValueTo(sCharset, sLanguage, sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}
// HTTP 头部 split 行相关处理
XXAPI bool xrtHttpHeaderSplitLineN(const char* sLine, size_t iLen, xrtheaderpair* pOut)
{
	size_t iColon = (size_t)-1;
	size_t i;
	if ( sLine == NULL || pOut == NULL || iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sLine[i] == ':' ) {
			iColon = i;
			break;
		}
		if ( sLine[i] == '\r' || sLine[i] == '\n' ) return false;
	}
	if ( iColon == (size_t)-1 || iColon == 0u ) return false;
	for ( i = 0u; i < iColon; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sLine[i]) ) return false;
	}
	pOut->tName = xrtStrView(sLine, iColon);
	pOut->tValue = xrtStrView(sLine + iColon + 1u, iLen - iColon - 1u);
	__xrtHttpUtilTrimView(&pOut->tValue);
	return true;
}
// HTTP 头部 split 行相关处理
XXAPI bool xrtHttpHeaderSplitLine(const char* sLine, xrtheaderpair* pOut)
{
	if ( sLine == NULL ) return false;
	return xrtHttpHeaderSplitLineN(sLine, strlen(sLine), pOut);
}
// 构建 HTTP 头部行
XXAPI bool xrtHttpHeaderBuildLineTo(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 HTTP 头部行
XXAPI bool xrtHttpHeaderBuildLine(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderBuildLineTo(sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}
// 构建 HTTP 头部规范化行
XXAPI bool xrtHttpHeaderBuildCanonicalLineToN(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	bool bUpper = true;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen == 0u ) return false;
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
		if ( ch >= 'A' && ch <= 'Z' ) ch = (char)(ch + 32);
		if ( bUpper && ch >= 'a' && ch <= 'z' ) ch = (char)(ch - 32);
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, &ch, 1u) ) return false;
		bUpper = (ch == '-');
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 HTTP 头部规范化行
XXAPI bool xrtHttpHeaderBuildCanonicalLineTo(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderBuildCanonicalLineToN(sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}
// 构建 HTTP 头部块
XXAPI bool xrtHttpHeaderBuildBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtHttpHeaderBuildLineTo(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + iOff,
			iOutCap - iOff,
			&iLineLen) ) return false;
		iOff += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 HTTP 头部规范化块
XXAPI bool xrtHttpHeaderBuildCanonicalBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtHttpHeaderBuildCanonicalLineToN(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + iOff,
			iOutCap - iOff,
			&iLineLen) ) return false;
		iOff += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 获取下一个 HTTP Token
XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t i;
	if ( pOut ) memset(pOut, 0, sizeof(xrtstrview));
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ',' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ',' ) iEnd++;
	while ( iEnd > iCur && (sText[iEnd - 1u] == ' ' || sText[iEnd - 1u] == '\t') ) iEnd--;
	if ( iEnd <= iCur ) return false;
	for ( i = iCur; i < iEnd; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) return false;
	}
	pOut->sPtr = sText + iCur;
	pOut->iLen = iEnd - iCur;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}
// 获取下一个 HTTP Token
XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}
// 统计 HTTP Token
XXAPI size_t xrtHttpTokenCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrtstrview tToken;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) ++iCount;
	return iCount;
}
// 统计 HTTP Token
XXAPI size_t xrtHttpTokenCount(const char* sText)
{
	if ( sText == NULL ) return 0u;
	return xrtHttpTokenCountN(sText, strlen(__xrt_cstr(sText)));
}
// 查找 HTTP Token
XXAPI bool xrtHttpTokenFindN(const char* sText, size_t iLen, const char* sToken, size_t iTokenLen, xrtstrview* pOut)
{
	size_t iOffset = 0u;
	xrtstrview tToken;
	if ( pOut ) memset(pOut, 0, sizeof(xrtstrview));
	if ( sText == NULL || sToken == NULL || iTokenLen == 0u ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == iTokenLen && __xrtHttpUtilEqNoCaseN(tToken.sPtr, tToken.iLen, sToken, iTokenLen) ) {
			if ( pOut ) *pOut = tToken;
			return true;
		}
	}
	return false;
}
// 查找 HTTP Token
XXAPI bool xrtHttpTokenFind(const char* sText, const char* sToken, xrtstrview* pOut)
{
	if ( sText == NULL || sToken == NULL ) return false;
	return xrtHttpTokenFindN(sText, strlen(__xrt_cstr(sText)), sToken, strlen(__xrt_cstr(sToken)), pOut);
}
// 解析 HTTP Token
XXAPI bool xrtHttpTokenParseToN(const char* sText, size_t iLen, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( pCount ) *pCount = 0u;
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tToken;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}
// 解析 HTTP Token
XXAPI bool xrtHttpTokenParseTo(const char* sText, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}
// 追加 HTTP Token
XXAPI bool xrtHttpTokenAppendTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken, size_t iTokenLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sToken == NULL || iTokenLen == 0u ) return false;
	for ( i = 0u; i < iTokenLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sToken[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, ", ", 2u) ) return false;
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sToken, iTokenLen);
}
// 追加 HTTP Token
XXAPI bool xrtHttpTokenAppend(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken)
{
	if ( sToken == NULL ) return false;
	return xrtHttpTokenAppendTo(sOut, iOutCap, pOffset, sToken, strlen(__xrt_cstr(sToken)));
}
// 判断是否包含 HTTP 头部 Token
XXAPI bool xrtHttpHeaderContainsTokenN(const char* sValue, size_t iValueLen, const char* sToken)
{
	if ( sValue == NULL || sToken == NULL || sToken[0] == '\0' ) return false;
	return xrtHttpTokenFindN(sValue, iValueLen, sToken, strlen(__xrt_cstr(sToken)), NULL);
}
// 判断是否包含 HTTP 头部 Token
XXAPI bool xrtHttpHeaderContainsToken(const char* sValue, const char* sToken)
{
	if ( sValue == NULL ) return false;
	return xrtHttpHeaderContainsTokenN(sValue, strlen(sValue), sToken);
}
// 查找 HTTP 头部
XXAPI bool xrtHttpHeaderFindN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	size_t iNameLen;
	size_t i;
	if ( pHeaders == NULL || sName == NULL ) return false;
	iNameLen = strlen(__xrt_cstr(sName));
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = pHeaders[i].tValue;
			return true;
		}
	}
	return false;
}
// 查找 HTTP 头部
XXAPI bool xrtHttpHeaderFind(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	return xrtHttpHeaderFindN(pHeaders, iCount, sName, pOut);
}
// 统计 HTTP 头部
XXAPI size_t xrtHttpHeaderCountN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) ++iHits;
	}
	return iHits;
}
// 统计 HTTP 头部
XXAPI size_t xrtHttpHeaderCount(const xrtheaderpair* pHeaders, size_t iCount, const char* sName)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderCountN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)));
}
// 查找 HTTP 头部 nth
XXAPI bool xrtHttpHeaderFindNthN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pOut)
{
	size_t i;
	size_t iSeen = 0u;
	if ( pOut ) *pOut = xrtStrView(NULL, 0u);
	if ( pHeaders == NULL || sName == NULL || pOut == NULL || iNameLen == 0u ) return false;
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( iSeen == iNth ) {
			*pOut = pHeaders[i].tValue;
			return true;
		}
		++iSeen;
	}
	return false;
}
// 查找 HTTP 头部 nth
XXAPI bool xrtHttpHeaderFindNth(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNth, xrtstrview* pOut)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderFindNthN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), iNth, pOut);
}
// 查找 HTTP 头部全部
XXAPI size_t xrtHttpHeaderFindAllToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, xrtstrview* pOut, size_t iOutCap)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( pOut != NULL && iHits < iOutCap ) pOut[iHits] = pHeaders[i].tValue;
		++iHits;
	}
	return iHits;
}
// 查找 HTTP 头部全部
XXAPI size_t xrtHttpHeaderFindAllTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut, size_t iOutCap)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderFindAllToN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), pOut, iOutCap);
}
// HTTP 头部 canonicalize 名称相关处理
XXAPI bool xrtHttpHeaderCanonicalizeNameToN(const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	bool bUpper = true;
	if ( sName == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen + 1u > iOutCap ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
		if ( ch >= 'A' && ch <= 'Z' ) ch = (char)(ch + 32);
		if ( bUpper && ch >= 'a' && ch <= 'z' ) ch = (char)(ch - 32);
		sOut[i] = ch;
		bUpper = (ch == '-');
	}
	sOut[iNameLen] = '\0';
	if ( pOutLen ) *pOutLen = iNameLen;
	return true;
}
// HTTP 头部 canonicalize 名称相关处理
XXAPI bool xrtHttpHeaderCanonicalizeNameTo(const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderCanonicalizeNameToN(sName, strlen(__xrt_cstr(sName)), sOut, iOutCap, pOutLen);
}
// 加入 HTTP 头部 values
XXAPI bool xrtHttpHeaderJoinValuesTo(const xrtstrview* pValues, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pValues == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( i != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pValues[i].sPtr, pValues[i].iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtHttpHeaderCollectAndJoinToN 相关处理
XXAPI bool xrtHttpHeaderCollectAndJoinToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	size_t iHits = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pHeaders == NULL || sName == NULL || sOut == NULL || iOutCap == 0u || iNameLen == 0u ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( iHits != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pHeaders[i].tValue.sPtr, pHeaders[i].tValue.iLen) ) return false;
		++iHits;
	}
	if ( iHits == 0u ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtHttpHeaderCollectAndJoinTo 相关处理
XXAPI bool xrtHttpHeaderCollectAndJoinTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderCollectAndJoinToN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), sOut, iOutCap, pOutLen);
}
// 获取下一个 HTTP 头部行
XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut)
{
	size_t iCur;
	size_t iEnd;
	if ( sBlock == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	if ( iCur >= iLen ) return false;
	if ( iCur + 1u < iLen && sBlock[iCur] == '\r' && sBlock[iCur + 1u] == '\n' ) {
		*pOffset = iCur + 2u;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen ) {
		if ( sBlock[iEnd] == '\n' ) {
			if ( iEnd == iCur || sBlock[iEnd - 1u] != '\r' ) return false;
			break;
		}
		iEnd++;
	}
	if ( iEnd < iLen ) {
		if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iEnd - iCur - 1u, pOut) ) return false;
		*pOffset = iEnd + 1u;
		return true;
	}
	if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iLen - iCur, pOut) ) return false;
	*pOffset = iLen;
	return true;
}
// 获取下一个 HTTP 头部行
XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderNextLineN(sBlock, strlen(sBlock), pOffset, pOut);
}
// 查找 HTTP 头部行
XXAPI bool xrtHttpHeaderFindLineN(const char* sBlock, size_t iLen, const char* sName, xrtheaderpair* pOut)
{
	size_t iOffset = 0u;
	xrtheaderpair tHeader;
	size_t iNameLen;
	if ( sBlock == NULL || sName == NULL ) return false;
	iNameLen = strlen(__xrt_cstr(sName));
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = tHeader;
			return true;
		}
	}
	return false;
}
// 查找 HTTP 头部行
XXAPI bool xrtHttpHeaderFindLine(const char* sBlock, const char* sName, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderFindLineN(sBlock, strlen(sBlock), sName, pOut);
}
// 解析 HTTP 头部块
XXAPI bool xrtHttpHeaderParseBlockToN(const char* sBlock, size_t iLen, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCountOut = 0u;
	xrtheaderpair tHeader;
	if ( pCount ) *pCount = 0u;
	if ( sBlock == NULL || (pHeaders == NULL && iCap != 0u) ) return false;
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( iCountOut >= iCap ) return false;
		pHeaders[iCountOut++] = tHeader;
	}
	if ( iOffset < iLen ) {
		if ( !(iOffset + 1u == iLen && sBlock[iOffset] == '\n') &&
		     !(iOffset + 2u == iLen && sBlock[iOffset] == '\r' && sBlock[iOffset + 1u] == '\n') ) {
			if ( iOffset + 1u < iLen || !(sBlock[iOffset] == '\r' || sBlock[iOffset] == '\n') ) return false;
		}
	}
	if ( pCount ) *pCount = iCountOut;
	return true;
}
// 解析 HTTP 头部块
XXAPI bool xrtHttpHeaderParseBlockTo(const char* sBlock, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderParseBlockToN(sBlock, strlen(sBlock), pHeaders, iCap, pCount);
}
// 追加 HTTP 头部键值对
XXAPI bool xrtHttpHeaderAppendPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( *pCount >= iCap || iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	pHeaders[*pCount].tName = xrtStrView(sName, iNameLen);
	pHeaders[*pCount].tValue = xrtStrView(sValue, iValueLen);
	(*pCount)++;
	return true;
}
// 追加 HTTP 头部键值对
XXAPI bool xrtHttpHeaderAppendPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}
// 设置 HTTP 头部键值对
XXAPI bool xrtHttpHeaderSetPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	for ( i = 0u; i < *pCount; ++i ) {
		if ( pHeaders[i].tName.iLen == iNameLen && __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
			pHeaders[i].tValue = xrtStrView(sValue, iValueLen);
			return true;
		}
	}
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, iNameLen, sValue, iValueLen);
}
// 设置 HTTP 头部键值对
XXAPI bool xrtHttpHeaderSetPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderSetPairN(pHeaders, iCap, pCount, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}
// 删除 HTTP 头部
XXAPI size_t xrtHttpHeaderRemoveN(xrtheaderpair* pHeaders, size_t* pCount, const char* sName, size_t iNameLen)
{
	size_t iRead;
	size_t iWrite = 0u;
	size_t iRemoved = 0u;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( iRead = 0u; iRead < *pCount; ++iRead ) {
		if ( pHeaders[iRead].tName.iLen == iNameLen &&
		     __xrtHttpUtilEqNoCaseN(pHeaders[iRead].tName.sPtr, pHeaders[iRead].tName.iLen, sName, iNameLen) ) {
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) pHeaders[iWrite] = pHeaders[iRead];
		iWrite++;
	}
	*pCount = iWrite;
	return iRemoved;
}
// 删除 HTTP 头部
XXAPI size_t xrtHttpHeaderRemove(xrtheaderpair* pHeaders, size_t* pCount, const char* sName)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderRemoveN(pHeaders, pCount, sName, strlen(__xrt_cstr(sName)));
}
// 获取下一个 Cookie
XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 || iEq == iCur ) return false;
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) return false;
	for ( i = 0u; i < pOut->tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[i]) ) return false;
	}
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}
// 获取下一个 Cookie
XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut)
{
	if ( sText == NULL ) return false;
	return xrtCookieNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}
// 查找 Cookie
XXAPI bool xrtCookieFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrtcookiepair* pOut)
{
	size_t iOffset = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == iNameLen && memcmp(tCookie.tName.sPtr, sName, iNameLen) == 0 ) {
			if ( pOut ) *pOut = tCookie;
			return true;
		}
	}
	return false;
}
// 查找 Cookie
XXAPI bool xrtCookieFind(const char* sText, const char* sName, xrtcookiepair* pOut)
{
	if ( sText == NULL || sName == NULL ) return false;
	return xrtCookieFindN(sText, strlen(__xrt_cstr(sText)), sName, strlen(__xrt_cstr(sName)), pOut);
}
// 解析 Cookie
XXAPI bool xrtCookieParseToN(const char* sText, size_t iLen, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( pCount ) *pCount = 0u;
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tCookie;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}
// 解析 Cookie
XXAPI bool xrtCookieParseTo(const char* sText, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtCookieParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}
// xrtSetCookieParseN 相关处理
XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( __xrtHttpUtilContainsCtl(sText, iLen) ) return false;
	memset(pOut, 0, sizeof(xrtsetcookieview));
	iCur = 0u;
	while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) iCur++;
	if ( iCur >= iLen ) return false;
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 || iEq == iCur ) return false;
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) return false;
	for ( iCur = 0u; iCur < pOut->tName.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[iCur]) ) return false;
	}
	for ( iCur = 0u; iCur < pOut->tValue.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsCookieOctet(pOut->tValue.sPtr[iCur]) ) return false;
	}
	pOut->iFlags |= XRT_SET_COOKIE_F_HAS_VALUE;
	iCur = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	while ( iCur < iLen ) {
		xrtstrview tAttrName;
		xrtstrview tAttrValue;
		size_t iAttrEnd = iCur;
		size_t iAttrEq = (size_t)-1;
		while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) iCur++;
		if ( iCur >= iLen ) break;
		iAttrEnd = iCur;
		while ( iAttrEnd < iLen && sText[iAttrEnd] != ';' ) {
			if ( sText[iAttrEnd] == '=' && iAttrEq == (size_t)-1 ) iAttrEq = iAttrEnd;
			iAttrEnd++;
		}
		if ( iAttrEq == (size_t)-1 ) {
			tAttrName = xrtStrView(sText + iCur, iAttrEnd - iCur);
			__xrtHttpUtilTrimView(&tAttrName);
			if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Secure", 6u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_SECURE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "HttpOnly", 8u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_HTTP_ONLY;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Partitioned", 11u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_PARTITIONED;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "SameParty", 9u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_SAME_PARTY;
			}
		} else {
			int32_t iMaxAge = 0;
			tAttrName = xrtStrView(sText + iCur, iAttrEq - iCur);
			tAttrValue = xrtStrView(sText + iAttrEq + 1u, iAttrEnd - iAttrEq - 1u);
			__xrtHttpUtilTrimView(&tAttrName);
			__xrtHttpUtilTrimView(&tAttrValue);
			if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Domain", 6u) ) {
				pOut->tDomain = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_DOMAIN;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Path", 4u) ) {
				pOut->tPath = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_PATH;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Expires", 7u) ) {
				pOut->tExpires = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_EXPIRES;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Max-Age", 7u) ) {
				if ( !__xrtHttpUtilParseInt32(tAttrValue.sPtr, tAttrValue.iLen, &iMaxAge) ) return false;
				pOut->iMaxAge = iMaxAge;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_MAX_AGE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "SameSite", 8u) ) {
				if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Lax", 3u) ) {
					pOut->iSameSite = XRT_SAME_SITE_LAX;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Strict", 6u) ) {
					pOut->iSameSite = XRT_SAME_SITE_STRICT;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "None", 4u) ) {
					pOut->iSameSite = XRT_SAME_SITE_NONE;
				} else {
					return false;
				}
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_SAME_SITE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Priority", 8u) ) {
				if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Low", 3u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_LOW;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Medium", 6u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_MEDIUM;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "High", 4u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_HIGH;
				} else {
					return false;
				}
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_PRIORITY;
			}
		}
		iCur = (iAttrEnd < iLen) ? (iAttrEnd + 1u) : iAttrEnd;
	}
	return true;
}
// xrtSetCookieParse 相关处理
XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtSetCookieParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// 解析 set Cookie 行
XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut)
{
	xrtheaderpair tHeader;
	if ( !xrtHttpHeaderSplitLineN(sLine, iLen, &tHeader) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Set-Cookie", 10u) ) return false;
	return xrtSetCookieParseN(tHeader.tValue.sPtr, tHeader.tValue.iLen, pOut);
}
// 解析 set Cookie 行
XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut)
{
	if ( sLine == NULL ) return false;
	return xrtSetCookieParseLineN(sLine, strlen(sLine), pOut);
}
static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut);
// 获取下一个 HTTP 参数
XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	xrtstrview tName;
	xrtstrview tValue;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iLen;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 ) {
		tName = xrtStrView(sText + iCur, iEnd - iCur);
		tValue = xrtStrView(NULL, 0u);
	} else {
		tName = xrtStrView(sText + iCur, iEq - iCur);
		tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	}
	__xrtHttpUtilTrimView(&tName);
	if ( xrtStrViewIsEmpty(tName) ) return false;
	for ( i = 0u; i < tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tName.sPtr[i]) ) return false;
	}
	if ( iEq != (size_t)-1 ) {
		if ( !__xrtHttpUtilParseParamValue(tValue, &tValue) ) return false;
	}
	pOut->iFlags = (iEq != (size_t)-1) ? XRT_HTTP_PARAM_F_HAS_VALUE : XRT_HTTP_PARAM_F_NONE;
	pOut->tName = tName;
	pOut->tValue = tValue;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}
// 获取下一个 HTTP 参数
XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpParamNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}
// 统计 HTTP 参数
XXAPI size_t xrtHttpParamCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrthttpparam tParam;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) ++iCount;
	return iCount;
}
// 统计 HTTP 参数
XXAPI size_t xrtHttpParamCount(const char* sText)
{
	if ( sText == NULL ) return 0u;
	return xrtHttpParamCountN(sText, strlen(__xrt_cstr(sText)));
}
// 查找 HTTP 参数
XXAPI bool xrtHttpParamFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	size_t iOffset = 0u;
	xrthttpparam tParam;
	if ( pOut ) memset(pOut, 0, sizeof(xrthttpparam));
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) return false;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = tParam;
			return true;
		}
	}
	return false;
}
// 查找 HTTP 参数
XXAPI bool xrtHttpParamFind(const char* sText, const char* sName, xrthttpparam* pOut)
{
	if ( sText == NULL || sName == NULL ) return false;
	return xrtHttpParamFindN(sText, strlen(__xrt_cstr(sText)), sName, strlen(__xrt_cstr(sName)), pOut);
}
// 追加 HTTP 参数键值对
XXAPI bool xrtHttpParamAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bQuoteValue)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !bHasValue ) return true;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
	if ( bQuoteValue ) return __xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sValue, iValueLen);
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}
// 追加 HTTP 参数键值对
XXAPI bool xrtHttpParamAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue, bool bHasValue, bool bQuoteValue)
{
	if ( sName == NULL ) return false;
	return xrtHttpParamAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, sValue ? strlen(sValue) : 0u, bHasValue, bQuoteValue);
}
// xrtHttpMediaTypeParseN 相关处理
XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut)
{
	size_t iSemi = 0u;
	size_t iSlash = (size_t)-1;
	size_t iPlus = (size_t)-1;
	size_t i;
	xrtstrview tToken;
	xrtstrview tParams;
	if ( pOut == NULL ) return false;
	memset(pOut, 0, sizeof(xrtmediatypeview));
	if ( sText == NULL || iLen == 0u ) return false;
	while ( iSemi < iLen && sText[iSemi] != ';' ) iSemi++;
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) return false;
	for ( i = 0u; i < tToken.iLen; ++i ) {
		char ch = tToken.sPtr[i];
		if ( ch == '/' && iSlash == (size_t)-1 ) {
			iSlash = i;
			continue;
		}
		if ( ch == '+' && iPlus == (size_t)-1 ) {
			iPlus = i;
			continue;
		}
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
	}
	if ( iSlash == (size_t)-1 || iSlash == 0u || iSlash + 1u >= tToken.iLen ) return false;
	if ( iPlus != (size_t)-1 && iPlus <= iSlash + 1u ) return false;
	pOut->tType = xrtStrView(tToken.sPtr, iSlash);
	if ( iPlus == (size_t)-1 ) {
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, tToken.iLen - iSlash - 1u);
	} else {
		if ( iPlus + 1u >= tToken.iLen ) return false;
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, iPlus - iSlash - 1u);
		pOut->tSuffix = xrtStrView(tToken.sPtr + iPlus + 1u, tToken.iLen - iPlus - 1u);
		pOut->iFlags |= XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX;
	}
	if ( xrtStrViewIsEmpty(pOut->tSubType) ) return false;
	if ( iSemi < iLen ) {
		tParams = xrtStrView(sText + iSemi + 1u, iLen - iSemi - 1u);
		__xrtHttpUtilTrimView(&tParams);
		if ( !xrtStrViewIsEmpty(tParams) ) {
			pOut->tParams = tParams;
			pOut->iFlags |= XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS;
		}
	}
	return true;
}
// xrtHttpMediaTypeParse 相关处理
XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpMediaTypeParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// xrtHttpMediaTypeBuildTo 相关处理
XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pType == NULL || sOut == NULL || iOutCap == 0u ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pType->tType) || xrtStrViewIsEmpty(pType->tSubType) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tType.sPtr, pType->tType.iLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSubType.sPtr, pType->tSubType.iLen) ) return false;
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX) != 0u ) {
		if ( xrtStrViewIsEmpty(pType->tSuffix) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "+", 1u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSuffix.sPtr, pType->tSuffix.iLen) ) return false;
	}
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) != 0u ) {
		if ( __xrtHttpUtilContainsCtl(pType->tParams.sPtr, pType->tParams.iLen) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; ", 2u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tParams.sPtr, pType->tParams.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtHttpMediaTypeBuild 相关处理
XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpMediaTypeBuildTo(pType, sOut, iOutCap, pOutLen);
}
// xrtHttpMediaTypeFindParamN 相关处理
XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	if ( pOut ) memset(pOut, 0, sizeof(xrthttpparam));
	if ( pType == NULL || sName == NULL || iNameLen == 0u ) return false;
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u || xrtStrViewIsEmpty(pType->tParams) ) return false;
	return xrtHttpParamFindN(pType->tParams.sPtr, pType->tParams.iLen, sName, iNameLen, pOut);
}
// xrtHttpMediaTypeFindParam 相关处理
XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut)
{
	if ( sName == NULL ) return false;
	return xrtHttpMediaTypeFindParamN(pType, sName, strlen(__xrt_cstr(sName)), pOut);
}
// xrtHttpContentDispositionParseN 相关处理
XXAPI bool xrtHttpContentDispositionParseN(const char* sText, size_t iLen, xrtcontentdispositionview* pOut)
{
	size_t iSemi = 0u;
	size_t i;
	xrthttpparam tParam;
	xrtstrview tToken;
	xrtstrview tParams;
	xrtstrview tExtValue;
	xrtstrview tCharset;
	xrtstrview tLanguage;
	xrtstrview tEncoded;
	size_t iParamOff = 0u;
	if ( pOut == NULL ) return false;
	memset(pOut, 0, sizeof(xrtcontentdispositionview));
	if ( sText == NULL || iLen == 0u ) return false;
	while ( iSemi < iLen && sText[iSemi] != ';' ) iSemi++;
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) return false;
	for ( i = 0u; i < tToken.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tToken.sPtr[i]) ) return false;
	}
	pOut->tType = tToken;
	if ( iSemi < iLen ) {
		tParams = xrtStrView(sText + iSemi + 1u, iLen - iSemi - 1u);
		__xrtHttpUtilTrimView(&tParams);
		if ( !xrtStrViewIsEmpty(tParams) ) {
			pOut->tParams = tParams;
			pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_PARAMS;
			while ( xrtHttpParamNextN(tParams.sPtr, tParams.iLen, &iParamOff, &tParam) ) {
				if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "name", 4u) &&
				     (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME;
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename", 8u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tFileName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME;
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename*", 9u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					tExtValue = tParam.tValue;
					if ( !__xrtHttpUtilParseExtValueView(tExtValue, &tCharset, &tLanguage, &tEncoded) ) return false;
					pOut->tFileNameExt = tExtValue;
					pOut->tFileNameCharset = tCharset;
					pOut->tFileNameLanguage = tLanguage;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT;
				}
			}
		}
	}
	return true;
}
// xrtHttpContentDispositionParse 相关处理
XXAPI bool xrtHttpContentDispositionParse(const char* sText, xrtcontentdispositionview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpContentDispositionParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}
// 解码 HTTP content disposition 文件名称
XXAPI bool xrtHttpContentDispositionDecodeFileNameTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		return xrtHttpDecodeExtValueTo(
			pDisp->tFileNameExt.sPtr,
			pDisp->tFileNameExt.iLen,
			NULL,
			NULL,
			sOut,
			iOutCap,
			pOutLen);
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) == 0u ) return false;
	iLen = pDisp->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut, pDisp->tFileName.sPtr, iLen);
	sOut[iLen] = '\0';
	if ( pOutLen ) *pOutLen = iLen;
	return true;
}
// 解码 HTTP content disposition 文件名称
XXAPI bool xrtHttpContentDispositionDecodeFileName(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionDecodeFileNameTo(pDisp, sOut, iOutCap, pOutLen);
}
// xrtHttpContentDispositionBuildTo 相关处理
XXAPI bool xrtHttpContentDispositionBuildTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pDisp->tType) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tType.sPtr, pDisp->tType.iLen) ) return false;
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "name", 4u, pDisp->tName.sPtr, pDisp->tName.iLen, true, true) ) return false;
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "filename", 8u, pDisp->tFileName.sPtr, pDisp->tFileName.iLen, true, true) ) return false;
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; filename*=", 12u) ) return false;
		/* tFileNameExt stores the raw RFC 5987 ext-value; preserve it verbatim when rebuilding. */
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tFileNameExt.sPtr, pDisp->tFileNameExt.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtHttpContentDispositionBuild 相关处理
XXAPI bool xrtHttpContentDispositionBuild(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionBuildTo(pDisp, sOut, iOutCap, pOutLen);
}
// 追加 Cookie 键值对
XXAPI bool xrtCookieAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	for ( i = 0u; i < iValueLen; ++i ) {
		if ( !__xrtHttpUtilIsCookieOctet(sValue[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}
// 追加 Cookie 键值对
XXAPI bool xrtCookieAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtCookieAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}
// 构建 Cookie
XXAPI bool xrtCookieBuildTo(const xrtcookiepair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pPairs[i].tName.sPtr, pPairs[i].tName.iLen, pPairs[i].tValue.sPtr, pPairs[i].tValue.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 追加查询键值对
XXAPI bool xrtQueryAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, size_t iKeyLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bPlusAsSpace)
{
	size_t iWritten = 0u;
	if ( sOut == NULL || pOffset == NULL || sKey == NULL ) return false;
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "&", 1u) ) return false;
	}
	if ( !xrtPercentEncodeTo(sKey, iKeyLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) return false;
	*pOffset += iWritten;
	if ( bHasValue ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
		if ( !xrtPercentEncodeTo(sValue, iValueLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) return false;
		*pOffset += iWritten;
	}
	return true;
}
// 追加查询键值对
XXAPI bool xrtQueryAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, const char* sValue)
{
	if ( sKey == NULL ) return false;
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sKey, strlen(sKey), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL, false);
}
// 构建查询
XXAPI bool xrtQueryBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtQueryAppendPairTo(sOut, iOutCap, &iOff,
			pPairs[i].tKey.sPtr,
			pPairs[i].tKey.iLen,
			pPairs[i].tValue.sPtr,
			pPairs[i].tValue.iLen,
			(pPairs[i].iFlags & XRT_QUERY_F_HAS_VALUE) != 0u,
			false) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 获取下一个表单 URL 编码
XXAPI bool xrtFormUrlEncodedNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNextN(sText, iLen, pOffset, pOut);
}
// 获取下一个表单 URL 编码
XXAPI bool xrtFormUrlEncodedNext(const char* sText, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNext(sText, pOffset, pOut);
}
// 解析表单 URL 编码
XXAPI bool xrtFormUrlEncodedParseToN(const char* sText, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	return xrtQueryParseToN(sText, iLen, pOut, iCap, pCount);
}
// 解析表单 URL 编码
XXAPI bool xrtFormUrlEncodedParseTo(const char* sText, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtFormUrlEncodedParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}
// 解码表单 URL 编码
XXAPI bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtPercentDecodeTo(sText, iLen, sOut, iOutCap, pOutLen, true);
}
// 追加表单 URL 编码 field
XXAPI bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sName, iNameLen, sValue, iValueLen, bHasValue, true);
}
// 追加表单 URL 编码 field
XXAPI bool xrtFormUrlEncodedAppendField(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL ) return false;
	return xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL);
}
// 构建表单 URL 编码
XXAPI bool xrtFormUrlEncodedBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, &iOff,
			pPairs[i].tKey.sPtr,
			pPairs[i].tKey.iLen,
			pPairs[i].tValue.sPtr,
			pPairs[i].tValue.iLen,
			(pPairs[i].iFlags & XRT_QUERY_F_HAS_VALUE) != 0u) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtSetCookieBuildTo 相关处理
XXAPI bool xrtSetCookieBuildTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pCookie->tName) ) return false;
	if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pCookie->tName.sPtr, pCookie->tName.iLen, pCookie->tValue.sPtr, pCookie->tValue.iLen) ) return false;
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Domain=", 9u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tDomain.sPtr, pCookie->tDomain.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Path=", 7u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tPath.sPtr, pCookie->tPath.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Expires=", 10u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tExpires.sPtr, pCookie->tExpires.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_MAX_AGE) != 0u ) {
		char sNum[32];
		int iLen = snprintf(sNum, sizeof(sNum), "%d", (int)pCookie->iMaxAge);
		if ( iLen <= 0 || (size_t)iLen >= sizeof(sNum) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Max-Age=", 11u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sNum, (size_t)iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_SAME_SITE) != 0u ) {
		const char* sSameSite = NULL;
		if ( pCookie->iSameSite == XRT_SAME_SITE_LAX ) sSameSite = "Lax";
		else if ( pCookie->iSameSite == XRT_SAME_SITE_STRICT ) sSameSite = "Strict";
		else if ( pCookie->iSameSite == XRT_SAME_SITE_NONE ) sSameSite = "None";
		else return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameSite=", 12u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sSameSite, strlen(sSameSite)) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SECURE) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Secure", 8u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HTTP_ONLY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; HttpOnly", 10u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_PARTITIONED) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Partitioned", 13u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SAME_PARTY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameParty", 11u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PRIORITY) != 0u ) {
		const char* sPriority = NULL;
		if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_LOW ) sPriority = "Low";
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_MEDIUM ) sPriority = "Medium";
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_HIGH ) sPriority = "High";
		else return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Priority=", 11u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sPriority, strlen(sPriority)) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 set Cookie 行
XXAPI bool xrtSetCookieBuildLineTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "Set-Cookie: ", 12u) ) return false;
	if ( !xrtSetCookieBuildTo(pCookie, sOut + iOff, iOutCap - iOff, &iLineLen) ) return false;
	iOff += iLineLen;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// 构建 set Cookie 行
XXAPI bool xrtSetCookieBuildLine(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtSetCookieBuildLineTo(pCookie, sOut, iOutCap, pOutLen);
}
// 内部函数：解析 HTTP util 参数值
static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut)
{
	if ( pOut == NULL ) return false;
	*pOut = tRaw;
	__xrtHttpUtilTrimView(pOut);
	if ( pOut->iLen >= 2u && pOut->sPtr[0] == '"' && pOut->sPtr[pOut->iLen - 1u] == '"' ) {
		pOut->sPtr++;
		pOut->iLen -= 2u;
	}
	return !xrtStrViewIsEmpty(*pOut);
}
// 内部函数：查找 HTTP util 参数
static bool __xrtHttpUtilFindParamN(xrtstrview tValue, const char* sName, xrtstrview* pOut)
{
	xrthttpparam tParam;
	if ( pOut ) *pOut = xrtStrView(NULL, 0u);
	if ( sName == NULL ) return false;
	if ( !xrtHttpParamFindN(tValue.sPtr, tValue.iLen, sName, strlen(__xrt_cstr(sName)), &tParam) ) return false;
	if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) == 0u ) return false;
	if ( pOut ) *pOut = tParam.tValue;
	return true;
}
// xrtMultipartBoundaryFromContentTypeN 相关处理
XXAPI bool xrtMultipartBoundaryFromContentTypeN(const char* sValue, size_t iLen, xrtstrview* pOut)
{
	xrtmediatypeview tMediaType;
	xrtstrview tBoundary;
	size_t i;
	if ( sValue == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( !xrtHttpMediaTypeParseN(sValue, iLen, &tMediaType) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tType.sPtr, tMediaType.tType.iLen, "multipart", 9u) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "form-data", 9u) &&
	     !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "mixed", 5u) ) return false;
	if ( (tMediaType.iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u ) return false;
	if ( !__xrtHttpUtilFindParamN(tMediaType.tParams, "boundary", &tBoundary) ) return false;
	if ( xrtStrViewIsEmpty(tBoundary) || tBoundary.iLen > 70u ) return false;
	for ( i = 0u; i < tBoundary.iLen; ++i ) {
		unsigned char ch = (unsigned char)tBoundary.sPtr[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return false;
	}
	*pOut = tBoundary;
	return true;
}
// xrtMultipartBoundaryFromContentType 相关处理
XXAPI bool xrtMultipartBoundaryFromContentType(const char* sValue, xrtstrview* pOut)
{
	if ( sValue == NULL ) return false;
	return xrtMultipartBoundaryFromContentTypeN(sValue, strlen(sValue), pOut);
}
// xrtMultipartBuildContentTypeTo 相关处理
XXAPI bool xrtMultipartBuildContentTypeTo(const char* sBoundary, size_t iBoundaryLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "multipart/form-data; boundary=", 30u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sBoundary, iBoundaryLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}
// xrtMultipartBuildContentType 相关处理
XXAPI bool xrtMultipartBuildContentType(const char* sBoundary, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartBuildContentTypeTo(sBoundary, strlen(sBoundary), sOut, iOutCap, pOutLen);
}
// 内部函数：匹配 HTTP util 边界
static bool __xrtHttpUtilMatchBoundaryAt(const char* sBody, size_t iLen, size_t iPos, const char* sBoundary, size_t iBoundaryLen, bool* pFinal, size_t* pAfter)
{
	if ( pFinal ) *pFinal = false;
	if ( pAfter ) *pAfter = 0u;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	if ( iPos + 2u + iBoundaryLen > iLen ) return false;
	if ( sBody[iPos] != '-' || sBody[iPos + 1u] != '-' ) return false;
	if ( memcmp(sBody + iPos + 2u, sBoundary, iBoundaryLen) != 0 ) return false;
	iPos += 2u + iBoundaryLen;
	if ( iPos + 1u < iLen && sBody[iPos] == '-' && sBody[iPos + 1u] == '-' ) {
		if ( pFinal ) *pFinal = true;
		if ( pAfter ) *pAfter = iPos + 2u;
		return true;
	}
	if ( iPos + 1u < iLen && sBody[iPos] == '\r' && sBody[iPos + 1u] == '\n' ) {
		if ( pAfter ) *pAfter = iPos + 2u;
		return true;
	}
	return false;
}
// 内部函数：查找 HTTP util 边界行
static bool __xrtHttpUtilFindBoundaryLine(const char* sBody, size_t iLen, size_t iStart, const char* sBoundary, size_t iBoundaryLen, size_t* pPos, bool* pFinal, size_t* pAfter)
{
	size_t i;
	if ( pPos ) *pPos = 0u;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	for ( i = iStart; i < iLen; ++i ) {
		bool bFinal = false;
		size_t iAfter = 0u;
		bool bLineStart = (i == 0u) || (i >= 2u && sBody[i - 2u] == '\r' && sBody[i - 1u] == '\n');
		if ( !bLineStart ) continue;
		if ( !__xrtHttpUtilMatchBoundaryAt(sBody, iLen, i, sBoundary, iBoundaryLen, &bFinal, &iAfter) ) continue;
		if ( pPos ) *pPos = i;
		if ( pFinal ) *pFinal = bFinal;
		if ( pAfter ) *pAfter = iAfter;
		return true;
	}
	return false;
}
// 内部函数：__xrtHttpUtilMultipartParseContentDisposition
static bool __xrtHttpUtilMultipartParseContentDisposition(xrtstrview tValue, xrtmultipartpartview* pOut)
{
	xrtcontentdispositionview tDisp;
	if ( pOut == NULL ) return false;
	if ( !xrtHttpContentDispositionParseN(tValue.sPtr, tValue.iLen, &tDisp) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tDisp.tType.sPtr, tDisp.tType.iLen, "form-data", 9u) ) return false;
	pOut->tContentDisposition = tValue;
	pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_DISP;
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u ) {
		pOut->tName = tDisp.tName;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_NAME;
	}
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u ) {
		pOut->tFileName = tDisp.tFileName;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_FILENAME;
	}
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		pOut->tFileNameExt = tDisp.tFileNameExt;
		pOut->tFileNameCharset = tDisp.tFileNameCharset;
		pOut->tFileNameLanguage = tDisp.tFileNameLanguage;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_FILENAME_EXT;
	}
	return true;
}
// 获取下一个 Multipart
XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut)
{
	size_t iPos;
	size_t iAfterBoundary;
	bool bFinal = false;
	size_t iHeaderEnd;
	size_t iBodyEnd;
	size_t iNextPos;
	bool bNextFinal = false;
	size_t iUnusedAfter = 0u;
	xrtheaderpair tHeader;
	size_t iHeaderOff = 0u;
	if ( sBody == NULL || sBoundary == NULL || pOffset == NULL || pOut == NULL || iBoundaryLen == 0u ) return false;
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, *pOffset, sBoundary, iBoundaryLen, &iPos, &bFinal, &iAfterBoundary) ) {
		*pOffset = iLen;
		return false;
	}
	if ( bFinal ) {
		*pOffset = iLen;
		return false;
	}
	if ( iAfterBoundary > iLen ) return false;
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	iHeaderEnd = iAfterBoundary;
	while ( iHeaderEnd + 3u < iLen ) {
		if ( sBody[iHeaderEnd] == '\r' && sBody[iHeaderEnd + 1u] == '\n' && sBody[iHeaderEnd + 2u] == '\r' && sBody[iHeaderEnd + 3u] == '\n' ) break;
		iHeaderEnd++;
	}
	if ( iHeaderEnd + 3u >= iLen ) return false;
	pOut->tHeaders = xrtStrView(sBody + iAfterBoundary, iHeaderEnd - iAfterBoundary);
	iBodyEnd = iHeaderEnd + 4u;
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, iBodyEnd, sBoundary, iBoundaryLen, &iNextPos, &bNextFinal, &iUnusedAfter) ) return false;
	if ( iNextPos < 2u || sBody[iNextPos - 2u] != '\r' || sBody[iNextPos - 1u] != '\n' ) return false;
	pOut->tBody = xrtStrView(sBody + iBodyEnd, iNextPos - iBodyEnd - 2u);
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Disposition", 19u) ) {
			if ( !__xrtHttpUtilMultipartParseContentDisposition(tHeader.tValue, pOut) ) return false;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Type", 12u) ) {
			pOut->tContentType = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_TYPE;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Transfer-Encoding", 25u) ) {
			pOut->tTransferEncoding = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_TRANSFER_ENCODING;
		}
	}
	*pOffset = iNextPos;
	(void)bNextFinal;
	return true;
}
// 获取下一个 Multipart
XXAPI bool xrtMultipartNext(const char* sBody, const char* sBoundary, size_t* pOffset, xrtmultipartpartview* pOut)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartNextN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOffset, pOut);
}
// 解析 Multipart
XXAPI bool xrtMultipartParseToN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtmultipartpartview tPart;
	if ( pCount ) *pCount = 0u;
	if ( sBody == NULL || sBoundary == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tPart;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}
// 解析 Multipart
XXAPI bool xrtMultipartParseTo(const char* sBody, const char* sBoundary, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartParseToN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOut, iCap, pCount);
}
// 解码 Multipart 文件名称
XXAPI bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pPart == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( (pPart->iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u ) {
		return xrtHttpDecodeExtValueTo(
			pPart->tFileNameExt.sPtr,
			pPart->tFileNameExt.iLen,
			NULL,
			NULL,
			sOut,
			iOutCap,
			pOutLen);
	}
	if ( (pPart->iFlags & XRT_MULTIPART_F_HAS_FILENAME) == 0u ) return false;
	iLen = pPart->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut, pPart->tFileName.sPtr, iLen);
	sOut[iLen] = '\0';
	if ( pOutLen ) *pOutLen = iLen;
	return true;
}
// 解码 Multipart 文件名称
XXAPI bool xrtMultipartDecodeFileName(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtMultipartDecodeFileNameTo(pPart, sOut, iOutCap, pOutLen);
}
// 初始化 Multipart 流配置
XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig)
{
	if ( !pConfig ) return;
	pConfig->iMaxBufferedBytes = 256u * 1024u;
	pConfig->iMaxHeaderBytes = 64u * 1024u;
	pConfig->iMaxPartHeaders = 64u;
	pConfig->iTailReserve = 0u;
}
// 内部函数：__xrtHttpUtilMultipartStreamZeroEvent
static void __xrtHttpUtilMultipartStreamZeroEvent(xrtmultipartstreamevent* pEvent)
{
	if ( !pEvent ) return;
	memset(pEvent, 0, sizeof(xrtmultipartstreamevent));
	pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
}
// 内部函数：__xrtHttpUtilMultipartStreamCompact
static void __xrtHttpUtilMultipartStreamCompact(xrtmultipartstream* pStream)
{
	if ( !pStream || pStream->iCursor == 0u ) return;
	if ( pStream->iCursor >= pStream->iBufferLen ) {
		pStream->iCursor = 0u;
		pStream->iBufferLen = 0u;
		return;
	}
	memmove(pStream->pBuffer, pStream->pBuffer + pStream->iCursor, pStream->iBufferLen - pStream->iCursor);
	pStream->iBufferLen -= pStream->iCursor;
	pStream->iCursor = 0u;
}
// 内部函数：确保 HTTP util Multipart 流 cap
static bool __xrtHttpUtilMultipartStreamEnsureCap(xrtmultipartstream* pStream, size_t iNeed)
{
	size_t iNewCap;
	char* pNewBuf;
	if ( !pStream ) return false;
	if ( iNeed <= pStream->iBufferCap ) return true;
	if ( iNeed > pStream->iMaxBufferedBytes ) return false;
	iNewCap = pStream->iBufferCap ? pStream->iBufferCap : 1024u;
	while ( iNewCap < iNeed ) {
		size_t iGrow = iNewCap < 65536u ? iNewCap : 65536u;
		if ( iNewCap > ((size_t)-1) - iGrow ) return false;
		iNewCap += iGrow;
	}
	if ( iNewCap > pStream->iMaxBufferedBytes ) iNewCap = pStream->iMaxBufferedBytes;
	if ( iNewCap < iNeed ) return false;
	pNewBuf = (char*)XHTTP_UTIL_REALLOC(pStream->pBuffer, iNewCap);
	if ( !pNewBuf ) return false;
	pStream->pBuffer = pNewBuf;
	pStream->iBufferCap = iNewCap;
	return true;
}
// 初始化 Multipart 流
XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig)
{
	xrtmultipartstreamconfig tConfig;
	if ( !pStream ) return false;
	memset(pStream, 0, sizeof(xrtmultipartstream));
	xrtMultipartStreamConfigInit(&tConfig);
	if ( pConfig ) tConfig = *pConfig;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_BOUNDARY;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	if ( tConfig.iMaxBufferedBytes == 0u ) tConfig.iMaxBufferedBytes = 256u * 1024u;
	if ( tConfig.iMaxHeaderBytes == 0u ) tConfig.iMaxHeaderBytes = 64u * 1024u;
	if ( tConfig.iMaxPartHeaders == 0u ) tConfig.iMaxPartHeaders = 64u;
	if ( tConfig.iTailReserve == 0u ) tConfig.iTailReserve = iBoundaryLen + 8u;
	if ( tConfig.iTailReserve < iBoundaryLen + 8u ) tConfig.iTailReserve = iBoundaryLen + 8u;
	if ( tConfig.iMaxBufferedBytes < tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u ) {
		tConfig.iMaxBufferedBytes = tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u;
	}
	memcpy(pStream->aBoundary, sBoundary, iBoundaryLen);
	pStream->aBoundary[iBoundaryLen] = '\0';
	pStream->iBoundaryLen = iBoundaryLen;
	pStream->iMaxBufferedBytes = tConfig.iMaxBufferedBytes;
	pStream->iMaxHeaderBytes = tConfig.iMaxHeaderBytes;
	pStream->iMaxPartHeaders = tConfig.iMaxPartHeaders;
	pStream->iTailReserve = tConfig.iTailReserve;
	pStream->iState = XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY;
	return true;
}
// 释放 Multipart 流
XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	if ( pStream->pBuffer ) {
		XHTTP_UTIL_FREE(pStream->pBuffer);
		pStream->pBuffer = NULL;
	}
	memset(pStream, 0, sizeof(xrtmultipartstream));
}
// 重置 Multipart 流
XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	pStream->iBufferLen = 0u;
	pStream->iCursor = 0u;
	pStream->iBoundaryPos = 0u;
	pStream->iAfterBoundary = 0u;
	pStream->iError = XRT_MULTIPART_STREAM_ERR_NONE;
	pStream->iState = XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY;
	pStream->bFinalBoundary = false;
	pStream->bFinishedInput = false;
	memset(&pStream->tCurrentPart, 0, sizeof(xrtmultipartpartview));
}
// xrtMultipartStreamFeed 相关处理
XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen)
{
	if ( !pStream || pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR || pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) return false;
	if ( pStream->bFinishedInput ) return false;
	if ( iLen == 0u ) return true;
	if ( pData == NULL ) return false;
	__xrtHttpUtilMultipartStreamCompact(pStream);
	if ( pStream->iBufferLen > pStream->iMaxBufferedBytes || iLen > pStream->iMaxBufferedBytes - pStream->iBufferLen ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	if ( !__xrtHttpUtilMultipartStreamEnsureCap(pStream, pStream->iBufferLen + iLen + 1u) ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	memcpy(pStream->pBuffer + pStream->iBufferLen, pData, iLen);
	pStream->iBufferLen += iLen;
	pStream->pBuffer[pStream->iBufferLen] = '\0';
	return true;
}
// 完成 Multipart 流
XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	pStream->bFinishedInput = true;
}
// 获取 Multipart 流错误
XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream)
{
	return pStream ? pStream->iError : XRT_MULTIPART_STREAM_ERR_NONE;
}
// 内部函数：解析 HTTP util Multipart 流 headers
static bool __xrtHttpUtilMultipartStreamParseHeaders(xrtmultipartstream* pStream, xrtmultipartpartview* pOut)
{
	size_t iHeaderEnd;
	size_t iHeaderOff = 0u;
	size_t iHeaderCount = 0u;
	xrtheaderpair tHeader;
	if ( !pStream || !pOut ) return false;
	iHeaderEnd = pStream->iCursor;
	while ( iHeaderEnd + 3u < pStream->iBufferLen ) {
		if ( pStream->pBuffer[iHeaderEnd] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 1u] == '\n' &&
		     pStream->pBuffer[iHeaderEnd + 2u] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 3u] == '\n' ) {
			break;
		}
		iHeaderEnd++;
		if ( iHeaderEnd - pStream->iCursor > pStream->iMaxHeaderBytes ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
	}
	if ( iHeaderEnd + 3u >= pStream->iBufferLen ) {
		if ( pStream->bFinishedInput ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		return false;
	}
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	pOut->tHeaders = xrtStrView(pStream->pBuffer + pStream->iCursor, iHeaderEnd - pStream->iCursor);
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		++iHeaderCount;
		if ( iHeaderCount > pStream->iMaxPartHeaders ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Disposition", 19u) ) {
			if ( !__xrtHttpUtilMultipartParseContentDisposition(tHeader.tValue, pOut) ) {
				pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_HEADER;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
				return false;
			}
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Type", 12u) ) {
			pOut->tContentType = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_TYPE;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Transfer-Encoding", 25u) ) {
			pOut->tTransferEncoding = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_TRANSFER_ENCODING;
		}
	}
	pStream->iCursor = iHeaderEnd + 4u;
	return true;
}
// 获取下一个 Multipart 流
XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent)
{
	if ( pEvent ) __xrtHttpUtilMultipartStreamZeroEvent(pEvent);
	if ( pStream == NULL || pEvent == NULL ) return XRT_MULTIPART_STREAM_RESULT_ERROR;
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
		return pEvent->iResult;
	}
	__xrtHttpUtilMultipartStreamCompact(pStream);
	for (;;) {
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY ) {
			size_t iPos = 0u;
			bool bFinal = false;
			size_t iAfter = 0u;
			if ( !__xrtHttpUtilFindBoundaryLine(pStream->pBuffer, pStream->iBufferLen, pStream->iCursor, pStream->aBoundary, pStream->iBoundaryLen, &iPos, &bFinal, &iAfter) ) {
				if ( pStream->bFinishedInput ) {
					pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
					pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
					return pEvent->iResult;
				}
				return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
			}
			pStream->iCursor = iAfter;
			if ( bFinal ) {
				pStream->iState = XRT_MULTIPART_STREAM_STATE_DONE;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
				return pEvent->iResult;
			}
			pStream->iState = XRT_MULTIPART_STREAM_STATE_HEADERS;
			continue;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_HEADERS ) {
			xrtmultipartpartview tPart;
			if ( !__xrtHttpUtilMultipartStreamParseHeaders(pStream, &tPart) ) {
				if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
					return pEvent->iResult;
				}
				return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
			}
			pStream->tCurrentPart = tPart;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_BODY;
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_BEGIN;
			pEvent->tPart = tPart;
			return pEvent->iResult;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_BODY ) {
			size_t iPos = 0u;
			bool bFinal = false;
			size_t iAfter = 0u;
			if ( __xrtHttpUtilFindBoundaryLine(pStream->pBuffer, pStream->iBufferLen, pStream->iCursor, pStream->aBoundary, pStream->iBoundaryLen, &iPos, &bFinal, &iAfter) ) {
				size_t iDataLen = (iPos >= pStream->iCursor + 2u) ? (iPos - pStream->iCursor - 2u) : 0u;
				if ( iDataLen > 0u ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_DATA;
					pEvent->tPart = pStream->tCurrentPart;
					pEvent->tData = xrtStrView(pStream->pBuffer + pStream->iCursor, iDataLen);
					pStream->iCursor += iDataLen;
					return pEvent->iResult;
				}
				pStream->iBoundaryPos = iPos;
				pStream->iAfterBoundary = iAfter;
				pStream->bFinalBoundary = bFinal;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_PART_END;
				continue;
			}
			if ( pStream->bFinishedInput ) {
				pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
				return pEvent->iResult;
			}
			if ( pStream->iBufferLen > pStream->iCursor + pStream->iTailReserve ) {
				size_t iDataLen = pStream->iBufferLen - pStream->iCursor - pStream->iTailReserve;
				if ( iDataLen > 0u ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_DATA;
					pEvent->tPart = pStream->tCurrentPart;
					pEvent->tData = xrtStrView(pStream->pBuffer + pStream->iCursor, iDataLen);
					pStream->iCursor += iDataLen;
					return pEvent->iResult;
				}
			}
			return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_PART_END ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_END;
			pEvent->tPart = pStream->tCurrentPart;
			pStream->iCursor = pStream->iAfterBoundary;
			memset(&pStream->tCurrentPart, 0, sizeof(xrtmultipartpartview));
			pStream->iState = pStream->bFinalBoundary ? XRT_MULTIPART_STREAM_STATE_DONE : XRT_MULTIPART_STREAM_STATE_HEADERS;
			return pEvent->iResult;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
			return pEvent->iResult;
		}
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_HEADER;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
}
// xrtMultipartAppendFieldPartTo 相关处理
XXAPI bool xrtMultipartAppendFieldPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}
// xrtMultipartAppendFieldPart 相关处理
XXAPI bool xrtMultipartAppendFieldPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sValue)
{
	if ( sBoundary == NULL || sName == NULL || sValue == NULL ) return false;
	return xrtMultipartAppendFieldPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}
// 追加 Multipart 原始数据 part
XXAPI bool xrtMultipartAppendRawPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (pHeaders == NULL && iHeaderCount != 0u) || (pBody == NULL && iBodyLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) return false;
	for ( i = 0u; i < iHeaderCount; ++i ) {
		size_t iLineLen = 0u;
		if ( !xrtHttpHeaderBuildLineTo(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + *pOffset,
			iOutCap - *pOffset,
			&iLineLen) ) return false;
		*pOffset += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}
// 追加 Multipart 原始数据 part
XXAPI bool xrtMultipartAppendRawPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartAppendRawPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), pHeaders, iHeaderCount, pBody, iBodyLen);
}
// 追加 Multipart 文件 part 扩展
XXAPI bool xrtMultipartAppendFilePartExtTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sFileNameExt, size_t iFileNameExtLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || sFileName == NULL || (sContentType == NULL && iContentTypeLen != 0u) || (pBody == NULL && iBodyLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( iContentTypeLen != 0u && __xrtHttpUtilContainsCtl(sContentType, iContentTypeLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename=", 11u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sFileName, iFileNameLen) ) return false;
	if ( sFileNameExt != NULL && iFileNameExtLen != 0u ) {
		size_t iExtLen = 0u;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename*=", 12u) ) return false;
		if ( !xrtHttpBuildExtValueTo("UTF-8", "", sFileNameExt, iFileNameExtLen, sOut + *pOffset, iOutCap - *pOffset, &iExtLen) ) return false;
		*pOffset += iExtLen;
	}
	if ( iContentTypeLen != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Type: ", 16u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sContentType, iContentTypeLen) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}
// 追加 Multipart 文件 part 扩展
XXAPI bool xrtMultipartAppendFilePartExt(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sFileNameExt, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL || sName == NULL || sFileName == NULL || pBody == NULL ) return false;
	return xrtMultipartAppendFilePartExtTo(
		sOut,
		iOutCap,
		pOffset,
		sBoundary,
		strlen(sBoundary),
		sName,
		strlen(__xrt_cstr(sName)),
		sFileName,
		strlen(sFileName),
		sFileNameExt,
		sFileNameExt ? strlen(sFileNameExt) : 0u,
		sContentType,
		sContentType ? strlen(sContentType) : 0u,
		pBody,
		iBodyLen);
}
// 追加 Multipart 文件 part
XXAPI bool xrtMultipartAppendFilePartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen)
{
	return xrtMultipartAppendFilePartExtTo(
		sOut,
		iOutCap,
		pOffset,
		sBoundary,
		iBoundaryLen,
		sName,
		iNameLen,
		sFileName,
		iFileNameLen,
		NULL,
		0u,
		sContentType,
		iContentTypeLen,
		pBody,
		iBodyLen);
}
// 追加 Multipart 文件 part
XXAPI bool xrtMultipartAppendFilePart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	return xrtMultipartAppendFilePartExt(sOut, iOutCap, pOffset, sBoundary, sName, sFileName, NULL, sContentType, pBody, iBodyLen);
}
// xrtMultipartAppendFinishTo 相关处理
XXAPI bool xrtMultipartAppendFinishTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen)
{
	if ( sOut == NULL || pOffset == NULL ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--\r\n", 4u);
}
// xrtMultipartAppendFinish 相关处理
XXAPI bool xrtMultipartAppendFinish(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartAppendFinishTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary));
}
#endif
#endif

// ========================================
// File: D:/git/xrt/lib/xnet_base.h
// ========================================

#ifndef XRT_XNET_BASE_H
#define XRT_XNET_BASE_H
/*
    XRT mainline network base model.
    This header defines the shared public types used by the modern xnet stack.
    It is designed to work both inside xrt.h and as a focused standalone header
    for transport development and testing.
    Public responsibilities:
      - IPv4/IPv6 address and endpoint representation
      - shared result codes, socket handles, and config structures
      - forward declarations for engine, stream, datagram, listener, and future
        objects
*/
#if defined(XXRTL_CORE)
	#define __XNET_IN_XRT_CORE 1
#else
	#define __XNET_IN_XRT_CORE 0
#endif
/* ============================== Basic local types ============================== */
#if !__XNET_IN_XRT_CORE
	typedef void* ptr;
	typedef uint8_t uint8;
	typedef uint16_t uint16;
	typedef uint32_t uint32;
	typedef uint64_t uint64;
#endif
#if defined(_WIN32) || defined(_WIN64)
	#if !defined(XRT_XSOCKET_DEFINED)
		typedef SOCKET xsocket;
		#define XRT_XSOCKET_DEFINED 1
	#endif
	#ifndef XSOCKET_INVALID
		#define XSOCKET_INVALID INVALID_SOCKET
	#endif
#else
	#if !defined(XRT_XSOCKET_DEFINED)
		typedef int xsocket;
		#define XRT_XSOCKET_DEFINED 1
	#endif
	#ifndef XSOCKET_INVALID
		#define XSOCKET_INVALID (-1)
	#endif
#endif
#ifndef XNET_SOCKET_INVALID
	#define XNET_SOCKET_INVALID XSOCKET_INVALID
#endif
#if !__XNET_IN_XRT_CORE
	typedef enum {
		XRT_NET_OK      =  0,
		XRT_NET_ERROR   = -1,
		XRT_NET_AGAIN   = -2,
		XRT_NET_TIMEOUT = -3,
		XRT_NET_CLOSED  = -4,
		XRT_NET_CANCELLED = -5
	} xnet_result;
#endif
#if !__XNET_IN_XRT_CORE
	typedef struct xrt_tls_config xtlsconfig;
#endif
/* ============================== Opaque handles ============================== */
#if !defined(XRT_BUILD_CORE)
typedef struct xrt_net_engine   xnetengine;
typedef struct xrt_net_mem_ctx  xnetmemctx;
typedef struct xrt_net_worker   xnetworker;
typedef struct xrt_net_listener xnetlistener;
typedef struct xrt_net_stream   xnetstream;
typedef struct xrt_net_dgram    xdgramsock;
typedef struct xrt_net_chain    xnetchain;
typedef struct xrt_net_future   xnetfuture;
typedef struct xrt_net_proxy    xnetproxy;
typedef struct xrt_tls_session  xtlssession;
typedef void (*xnet_task_fn)(xnetworker* pWorker, ptr pArg);
/* ============================== Address model ============================== */
typedef struct {
	uint16 iFamily;
	uint16 iPort;
	uint32 iScopeId;
	uint8 aAddr[16];
} xnetaddr;
/* ============================== Small public helpers ============================== */
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
/* ============================== Flags ============================== */
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
/* ============================== Config structs ============================== */
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
#endif /* !XRT_BUILD_CORE */
/* ============================== Internal address helpers ============================== */
#define __XNET_ADDR_STR_CAP 80
#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	#define __XNET_THREAD_LOCAL __declspec(thread)
#elif defined(_MSC_VER)
	#define __XNET_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
	#define __XNET_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
	#define __XNET_THREAD_LOCAL _Thread_local
#else
	#define __XNET_THREAD_LOCAL
#endif
// 内部函数：__xnetAtomicCompareExchange32
static long __xnetAtomicCompareExchange32(volatile long* pValue, long iExchange, long iComparand)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicCompareExchange32(pValue, iExchange, iComparand);
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iExchange, (LONG)iComparand);
	#else
		return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
	#endif
}
// 内部函数：__xnetAtomicExchange32
static long __xnetAtomicExchange32(volatile long* pValue, long iValue)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicExchange32(pValue, iValue);
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)InterlockedExchange((volatile LONG*)pValue, (LONG)iValue);
	#else
		return __sync_lock_test_and_set(pValue, iValue);
	#endif
}
// 内部函数：__xnetAtomicAddFetch32
static long __xnetAtomicAddFetch32(volatile long* pValue, long iDelta)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicAddFetch32(pValue, iDelta);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		long iPrev;
		long iNext;
		do {
			iPrev = (long)InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
			iNext = iPrev + iDelta;
		} while ( (long)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iNext, (LONG)iPrev) != iPrev );
		return iNext;
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)(InterlockedExchangeAdd((volatile LONG*)pValue, (LONG)iDelta) + (LONG)iDelta);
	#else
		return __sync_add_and_fetch(pValue, iDelta);
	#endif
}
// 内部函数：__xnetAtomicAddFetch64
static int64 __xnetAtomicAddFetch64(volatile int64* pValue, int64 iDelta)
{
	return __xrtAtomicAddFetch64(pValue, iDelta);
}
// 内部函数：__xnetAtomicLoad64
static int64 __xnetAtomicLoad64(const volatile int64* pValue)
{
	return __xrtAtomicLoad64(pValue);
}
// 内部函数：__xnetAtomicLoad32
static long __xnetAtomicLoad32(const volatile long* pValue)
{
	return __xnetAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}
// 内部函数：__xnetAddrFromSockAddr
static bool __xnetAddrFromSockAddr(xnetaddr* pAddr, const struct sockaddr* pSA)
{
	if ( !pAddr || !pSA ) return false;
	memset(pAddr, 0, sizeof(xnetaddr));
	if ( pSA->sa_family == AF_INET ) {
		const struct sockaddr_in* pSA4 = (const struct sockaddr_in*)pSA;
		pAddr->iFamily = AF_INET;
		pAddr->iPort = ntohs(pSA4->sin_port);
		memcpy(pAddr->aAddr, &pSA4->sin_addr, 4);
		return true;
	}
	if ( pSA->sa_family == AF_INET6 ) {
		const struct sockaddr_in6* pSA6 = (const struct sockaddr_in6*)pSA;
		pAddr->iFamily = AF_INET6;
		pAddr->iPort = ntohs(pSA6->sin6_port);
		pAddr->iScopeId = pSA6->sin6_scope_id;
		memcpy(pAddr->aAddr, &pSA6->sin6_addr, 16);
		return true;
	}
	return false;
}
// 内部函数：复制固定长度字符串
static void __xnetCopyFixedString(char* sDst, size_t iDstCap, const char* sSrc)
{
	size_t iLen;
	if ( !sDst || iDstCap == 0 ) return;
	sDst[0] = '\0';
	if ( !sSrc || !sSrc[0] ) return;
	iLen = strlen(__xrt_cstr(sSrc));
	if ( iLen >= iDstCap ) iLen = iDstCap - 1u;
	memcpy(sDst, sSrc, iLen);
	sDst[iLen] = '\0';
}
// 内部函数：判断代理配置是否有效
static bool __xnetProxyConfigIsValid(const xnetproxyconfig* pCfg)
{
	if ( !pCfg ) return false;
	if ( pCfg->iType != XNET_PROXY_SOCKS5 && pCfg->iType != XNET_PROXY_HTTP_CONNECT ) return false;
	if ( !pCfg->sHost[0] || pCfg->iPort == 0 ) return false;
	return true;
}
// 内部函数：__xnetAddrToSockAddr
static bool __xnetAddrToSockAddr(const xnetaddr* pAddr, struct sockaddr_storage* pStorage, socklen_t* pLen)
{
	if ( !pAddr || !pStorage || !pLen ) return false;
	memset(pStorage, 0, sizeof(struct sockaddr_storage));
	if ( pAddr->iFamily == AF_INET ) {
		struct sockaddr_in* pSA4 = (struct sockaddr_in*)pStorage;
		pSA4->sin_family = AF_INET;
		pSA4->sin_port = htons(pAddr->iPort);
		memcpy(&pSA4->sin_addr, pAddr->aAddr, 4);
		*pLen = sizeof(struct sockaddr_in);
		return true;
	}
	if ( pAddr->iFamily == AF_INET6 ) {
		struct sockaddr_in6* pSA6 = (struct sockaddr_in6*)pStorage;
		pSA6->sin6_family = AF_INET6;
		pSA6->sin6_port = htons(pAddr->iPort);
		pSA6->sin6_scope_id = pAddr->iScopeId;
		memcpy(&pSA6->sin6_addr, pAddr->aAddr, 16);
		*pLen = sizeof(struct sockaddr_in6);
		return true;
	}
	return false;
}
// 内部函数：__xnetAddrTempBuf
static char* __xnetAddrTempBuf(void)
{
	static __XNET_THREAD_LOCAL char aRing[4][__XNET_ADDR_STR_CAP];
	static __XNET_THREAD_LOCAL uint32 iIndex = 0;
	char* pBuf = aRing[iIndex & 3u];
	iIndex++;
	pBuf[0] = '\0';
	return pBuf;
}
/* ============================== Address public helpers ============================== */
XXAPI void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort)
{
	if ( !pAddr ) return;
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iFamily = (uint16)((iFamily == AF_INET6) ? AF_INET6 : AF_INET);
	pAddr->iPort = iPort;
}
// xrtNetAddrParse 相关处理
XXAPI xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort)
{
	#if defined(_WIN32) || defined(_WIN64)
		struct sockaddr_storage tStorage;
		int iStorageLen;
		char sAddrBuf[64];
	#endif
	if ( !pAddr || !sIP || !sIP[0] ) return XRT_NET_ERROR;
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iPort = iPort;
	#if defined(_WIN32) || defined(_WIN64)
		memset(&tStorage, 0, sizeof(tStorage));
		iStorageLen = (int)sizeof(tStorage);
		snprintf(sAddrBuf, sizeof(sAddrBuf), "%s", sIP);
		if ( WSAStringToAddressA(sAddrBuf, AF_INET, NULL, (struct sockaddr*)&tStorage, &iStorageLen) == 0 ) {
			pAddr->iFamily = AF_INET;
			memcpy(pAddr->aAddr, &((struct sockaddr_in*)&tStorage)->sin_addr, 4u);
			return XRT_NET_OK;
		}
		memset(&tStorage, 0, sizeof(tStorage));
		iStorageLen = (int)sizeof(tStorage);
		snprintf(sAddrBuf, sizeof(sAddrBuf), "%s",