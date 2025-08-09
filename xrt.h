


#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>
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
		
		// 调试模式
		int DebugMode;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
		// 内存函数
		ptr (*malloc)(size_t iSize);
		ptr (*calloc)(size_t iNum, size_t iSize);
		ptr (*realloc)(ptr pMem, size_t iSize);
		void (*free)(ptr pMem);
		
		// 内置错误描述
		struct {
			str MALLOC;
		} ERROR_DESC;
		
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
	XXAPI ustr xrtCopyStr(ustr sText, size_t iSize);
	XXAPI wstr xrtCopyStrW(wstr sText, size_t iSize);
	
	// 字符串转为小写（bSrcRevise 为 false 时，需使用 xrtFree 释放内存）
	XXAPI ustr xrtLCase(ustr sText, size_t iSize, int bSrcRevise);
	XXAPI wstr xrtLCaseW(wstr sText, size_t iSize, int bSrcRevise);
	
	// 字符串转为大写（bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存）
	XXAPI ustr xrtUCase(ustr sText, size_t iSize, int bSrcRevise);
	XXAPI wstr xrtUCaseW(wstr sText, size_t iSize, int bSrcRevise);
	
	// 搜索字符串（没找到字符串的情况下会返回 NULL）
	XXAPI ustr xrtFindStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bCase);
	XXAPI uint xrtInStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bCase);
	XXAPI wstr xrtFindStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase);
	XXAPI uint xrtInStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI ustr xrtCheckStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize);
	XXAPI wstr xrtCheckStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtLTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI ustr xrtRTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI ustr xrtTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtLTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtRTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtFilterStr(ustr sText, size_t iSize, ustr sFilter, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtFilterStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	
	// 字符串格式化（需使用 xrtFree 释放）
	XXAPI ustr xrtFormat(ustr sFormat, ...);
	XXAPI wstr xrtFormatW(wstr sFormat, ...);
	
	// 字符串替换（需使用 xrtFree 释放）
	XXAPI ustr xrtReplace(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, ustr sRepText, size_t iRepSize);
	XXAPI wstr xrtReplaceW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, wstr sRepText, size_t iRepSize);
	
	// 字符串分割（任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据）
	XXAPI ustr* xrtSplit(ustr sText, size_t iSize, ustr sSepText, size_t iSepSize, int bSrcRevise);
	XXAPI wstr* xrtSplitW(wstr sText, size_t iSize, wstr sSepText, size_t iSepSize, int bSrcRevise);
	
	// HEX 编码（需使用 xrtFree 释放）
	XXAPI ustr xrtHexEncode(ptr pMem, size_t iSize);
	XXAPI wstr xrtHexEncodeW(ptr pMem, size_t iSize);
	
	// HEX 解码（需使用 xrtFree 释放）
	XXAPI ustr xrtHexDecode(ptr pMem, size_t iSize);
	XXAPI wstr xrtHexDecodeW(ptr pMem, size_t iSize);
	
	// 生成随机字符串（需使用 xrtFree 释放）
	XXAPI ustr xrtRandStr(ustr sTemplate, size_t iSize, size_t iLen);
	XXAPI wstr xrtRandStrW(wstr sTemplate, size_t iSize, size_t iLen);
	
	
	
	/* ------------------------------------ Time 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Path 函数库 ------------------------------------ */
	
	// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtPathGetNameExt(ustr sPath, size_t iSize);
	XXAPI wstr xrtPathGetNameExtW(wstr sPath, size_t iSize);
	
	// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtPathGetName(ustr sPath, size_t iSize);
	XXAPI wstr xrtPathGetNameW(wstr sPath, size_t iSize);
	
	// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtPathGetExt(ustr sPath, size_t iSize);
	XXAPI wstr xrtPathGetExtW(wstr sPath, size_t iSize);
	
	// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtPathGetDir(ustr sPath, size_t iSize);
	XXAPI wstr xrtPathGetDirW(wstr sPath, size_t iSize);
	
	// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
	XXAPI int xrtPathIsAbs(ustr sPath, size_t iSize);
	XXAPI int xrtPathIsAbsW(wstr sPath, size_t iSize);
	
	// 判断是否为相对路径
	XXAPI int xrtPathIsRel(ustr sPath, size_t iSize);
	XXAPI int xrtPathIsRelW(wstr sPath, size_t iSize);
	
	// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
	XXAPI ustr xrtPathRandom(ustr sHead, size_t iHeadSize, ustr sFoot, size_t iFootSize, int iLen);
	XXAPI wstr xrtPathRandomW(wstr sHead, size_t iHeadSize, wstr sFoot, size_t iFootSize, int iLen);
	
	
	
#endif


