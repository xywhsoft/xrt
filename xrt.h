


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>



#ifndef XXRTL_CORE
	#define XXRTL_CORE
	
	
	
	// charset define
	#define XCORE_CHARSET_ANSI		0
	#define XCORE_CHARSET_UNICODE	1
	#define XCORE_CHARSET_UTF8		2
	
	
	
	// basic type define
	typedef char* ustr;
	typedef wchar_t* wstr;
	
	// windows utf16 | linux utf8
	#if defined(_WIN32) || defined(_WIN64)
		typedef wstr str;
	#else
		typedef ustr str;
	#endif
	
	typedef char int8;
	typedef unsigned char uint8;
	typedef short int16;
	typedef unsigned short uint16;
	typedef unsigned int uint;
	typedef int int32;
	typedef unsigned int uint32;
	typedef long long int64;
	typedef unsigned long long uint64;
	// long = auto 32 / 64 bit integer
	typedef unsigned long ulong;
	
	typedef void* ptr;
	typedef intptr_t intptr;
	typedef uintptr_t uintptr;
	
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
	
	
	
	// 全局
	typedef struct {
		
		// 是否已经初始化过
		int bInit;
		
		// 全局数据 (不可改变)
		str sNull;
		
		// 临时性全局数据 (可以改变，用于多个返回值的情况做临时存储)
		str sRet;
		int64 iRet;
		double nRet;
		
		// 错误信息
		str LastError;
		int __pri_FreeError;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
		// 内存函数
		ptr (*malloc)(size_t iSize);
		ptr (*calloc)(size_t iNum, size_t iSize);
		ptr (*realloc)(ptr pMem, size_t iSize);
		void (*free)(ptr pMem);
		
	} xrtGlobalData;
	
	
	
	// 初始化 xCore
	XXAPI xrtGlobalData* xrtInit();
	
	// 释放 xCore
	XXAPI void xrtUnit();
	
	
	
	/* ------------------------------------ Base 函数库 ------------------------------------ */
	
	// 申请内存
	XXAPI ptr xrtMalloc(size_t iSize);
	
	// 申请类内存
	XXAPI ptr xrtCalloc(size_t iNum, size_t iSize);
	
	// 重新申请内存
	XXAPI ptr xrtRealloc(ptr pMem, size_t iSize);
	
	// 释放内存（ 会先判断是否为 null ）
	XXAPI void xrtFree(ptr pmem);
	
	// 设置错误
	XXAPI void xrtSetError(str sError, int bFree);
	
	// 清除错误
	XXAPI void xrtClearError();
	
	
	
	/* ------------------------------------ Charset 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Math 函数库 ------------------------------------ */
	
	// 随机数
	XXAPI int xrtRand(int min, int max);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（需使用 xrtFree 释放）
	XXAPI ustr xrtCopyString(ustr sText, int iSize);
	XXAPI wstr xrtCopyStringW(wstr sText, int iSize);
	
	// 字符串转为小写（bSrcRevise 为 false 时，需使用 xCore.free 释放内存）
	XXAPI ustr xrtLCase(ustr sText, int iSize, int bSrcRevise);
	XXAPI wstr xrtLCaseW(wstr sText, int iSize, int bSrcRevise);
	
	// 字符串转为大写（bSrcRevise 为 FALSE 时，需使用 xCore.free 释放内存）
	XXAPI ustr xrtUCase(ustr sText, int iSize, int bSrcRevise);
	XXAPI wstr xrtUCaseW(wstr sText, int iSize, int bSrcRevise);
	
	// 搜索字符串（没找到字符串的情况下会返回 NULL）
	XXAPI ustr xrtFindStr(ustr sText, ustr sSubText, int bCase);
	XXAPI uint xrtInStr(ustr sText, ustr sSubText, int bCase);
	XXAPI wstr xrtFindStrW(wstr sText, wstr sSubText, int bCase);
	XXAPI uint xrtInStrW(wstr sText, wstr sSubText, int bCase);
	
	// 裁剪字符串（bSrcRevise 为 FALSE 时，需使用 xCore.free 释放内存）
	XXAPI wstr xrtLTrimW(wstr sText, wstr sSub, int bSrcRevise);
	XXAPI wstr xrtRTrimW(wstr sText, wstr sSub, int bSrcRevise);
	XXAPI wstr xrtTrimW(wstr sText, wstr sSub, int bSrcRevise);
	
	// 字符串格式化（需使用 xrtFree 释放）
	XXAPI ustr xrtFormat(ustr sFormat, ...);
	XXAPI wstr xrtFormatW(wstr sFormat, ...);
	
	// 字符串替换（需使用 xCore.free 释放）
	XXAPI ustr xrtReplace(ustr original, ustr pattern, ustr replacement);
	XXAPI wstr xrtReplaceW(wstr original, wstr pattern, wstr replacement);
	
	// 字符串分割（需使用 xCore.free 释放）
	XXAPI ustr* xrtSplit(ustr sText, ustr sSep, int bSrcRevise);
	XXAPI wstr* xrtSplitW(wstr sText, wstr sSep, int bSrcRevise);
	
	
	
	/* ------------------------------------ Time 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Path 函数库 ------------------------------------ */
	
	
	
#endif


