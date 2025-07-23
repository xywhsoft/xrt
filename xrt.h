


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
	typedef char* astr;
	typedef wchar_t* wstr;
	typedef char* ustr;
	#ifdef SETUP_CHARSET_USEANSI
		#define SETUP_CHARSET_DEFINE
		typedef astr str;
	#endif
	#ifdef SETUP_CHARSET_USEUNICODE
		#define SETUP_CHARSET_DEFINE
		typedef wstr str;
	#endif
	#ifdef SETUP_CHARSET_USEUTF8
		#define SETUP_CHARSET_DEFINE
		typedef ustr str;
	#endif
	#ifndef SETUP_CHARSET_DEFINE
		typedef astr str;
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
	
	
	
	// 初始化 xCore
	XXAPI void xCoreInit(void);
	
	// 释放 xCore
	XXAPI void xCoreUnit(void);
	
	
	
	// 全局
	typedef struct {
		
		// 全局数据 (不可改变)
		str nullstring;
		
		// 临时性全局数据 (可以改变，用于多个返回值的情况做临时存储)
		str sRet;
		int64 iRet;
		double dRet;
		int LastErrorID;
		str LastError;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
	} xCoreStruct, *xCoreObject;
	
	
	
	#ifdef XXRTL_CORE_IMPORTDLL
		__declspec(dllimport) extern xCoreStruct xCore;
	#else
		XXAPI extern xCoreStruct xCore;
	#endif
	
	
	
#endif


