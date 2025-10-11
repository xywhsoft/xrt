


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
#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>



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
		
		// 高精度时钟频率单位
		#if defined(_WIN32) || defined(_WIN64)
			uint64 Frequency;
		#endif
		
		// 调试模式
		int DebugMode;
		
		// 本机 IP 地址 ( 用于生成 XID )
		uint LocalAddr;
		
		// 应用信息
		str AppFile;
		str AppPath;
		
		// 环形临时内存（固定 32 个临时内存循环使用和释放）
		void* TempMem[32];
		uint32 TempMemIdx;
		
		// 内存函数
		ptr (*malloc)(size_t iSize);
		ptr (*calloc)(size_t iNum, size_t iSize);
		ptr (*realloc)(ptr pMem, size_t iSize);
		void (*free)(ptr pMem);
		
	} xrtGlobalData;
	
	// 全局数据
	XXAPI extern xrtGlobalData xCore;
	
	
	
	// 初始化 xCore
	XXAPI xrtGlobalData* xrtInit();
	
	// 释放 xCore
	XXAPI void xrtUnit();
	
	
	
	/* ------------------------------------ 基础功能补充 ------------------------------------ */
	
	// 内存查找
	XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize);
	
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
	
	// 设置错误
	XXAPI void xrtSetError(str sError, int bFree);
	XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, int bFree);
	XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, int bFree);
	
	// 清除错误
	XXAPI void xrtClearError();
	
	
	
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
	XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize);
	
	// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize);
	
	// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize);
	
	// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
	XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize);
	
	// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
	XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize);
	
	// utf-32 转 utf-16（ 需使用 xrtFree 释放 ）
	XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize);
	
	// utf-16 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, int bSrcRevise);
	
	// utf-32 大端序和小端序转换 ( 需使用 xrtFree 释放 )
	XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, int bSrcRevise);
	
	// 任意编码转换 ( 需使用 xrtFree 释放 )
	XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
	
	// 是否为 utf-8 字符串
	XXAPI int xrtIsUTF8(str sText, size_t iSize);
	
	// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
	XXAPI int xrtDetectCharset(ptr sText, size_t iSize, int bBOM);
	
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
	
	// 获取 32 位随机数
	XXAPI uint32 xrtRand32();
	
	// 设置 32 位随机数种子
	XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq);
	
	// 获取 64 位随机数
	XXAPI uint64 xrtRand64();
	
	// 设置 64 位随机数种子
	XXAPI void xrtSetRandSeed64(uint64 lowseed, uint64 lowseq, uint64 highseed, uint64 highseq);
	
	// 获取 32 位范围随机数
	XXAPI int xrtRandRange(int min, int max);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（ 需使用 xrtFree 释放 ）
	XXAPI str xrtCopyStr(str sText, size_t iSize);
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, int bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, int bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, int bCase);
	XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, int bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, int bSrcRevise);
	
	// 字符串格式化（ 需使用 xrtFree 释放 ）
	XXAPI str xrtFormat(str sFormat, ...);
	
	// 字符串替换（ 需使用 xrtFree 释放 ）
	XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
	
	// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
	XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, int bSrcRevise);
	
	// 生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
	
	// HEX 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
	
	// HEX 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtHexDecode(str pText, size_t iSize);
	
	// Base64 编码（ 需使用 xrtFree 释放 ）
	str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
	
	// Base64 解码（ 需使用 xrtFree 释放 ）
	ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
	
	
	
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
	XXAPI int xrtPathIsAbs(str sPath, size_t iSize);
	
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
	XXAPI int xrtIsLeapYear(int iYear);
	
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
	
	// 时间单位累加
	XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
	
	// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
	XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
	
	
	
	/* ------------------------------------ File 函数库 ------------------------------------ */
	
	// 文件对象
	typedef struct {
		union {
			ptr obj;			// windows 文件对象
			int idx;			// linux 文件句柄
		};
		int Charset;			// 文件字符集
		uint BOM;				// BOM大小
	} xfile_struct, *xfile;
	
	// 游标控制
	#define XRT_SEEK_SET		0
	#define XRT_SEEK_CUR		1
	#define XRT_SEEK_END		2
	
	// 打开文件 ( 需要使用 xrtClose 关闭文件 )
	XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
	
	// 关闭文件
	XXAPI int xrtClose(xfile objFile);
	
	// 设置游标位置
	XXAPI size_t xrtSeek(xfile objFile, long iOffset, int iMoveMethod);
	
	// 获取游标位置
	XXAPI size_t xrtTell(xfile objFile);
	
	// 获取文件末尾位置 ( 获取一打开文件的动态大小 )
	XXAPI size_t xrtGetEOF(xfile objFile);
	
	// 是否已经读取到文件末尾
	XXAPI int xrtEOF(xfile objFile);
	
	// 设置文件末尾
	XXAPI int xrtSetEOF(xfile objFile);
	
	// 从已打开的文件读取数据 ( iSize 为要读取的字节数，需要使用 xrtFree 释放内存 )
	XXAPI str xrtRead(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入数据 ( iSize 为要写入的字节数 )
	XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize);
	
	// 从已打开的文件读取二进制数据 ( 需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtGet(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入二进制数据
	XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize);
	
	// 向文件追加写入数据
	XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset);
	
	// 写入并覆盖文件内容
	XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
	
	// 读取文件的全部内容 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtFileReadAll(str sPath, int iCharset);
	
	// 写入并覆盖文件内容 ( 二进制 )
	XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize);
	
	// 读取文件的全部内容 ( 二进制，需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtFileGetAll(str sPath);
	
	// 判断路径是否存在
	XXAPI int xrtPathExists(str sPath);
	
	// 判断文件是否存在
	XXAPI int xrtFileExists(str sPath);
	
	// 判断目录是否存在
	XXAPI int xrtDirExists(str sPath);
	
	// 获取文件长度
	XXAPI size_t xrtFileGetSize(str sPath);
	
	// 设置文件长度
	XXAPI int xrtFileSetSize(str sPath, size_t iSize);
	
	// 获取文件属性
	XXAPI int xrtFileGetAttr(str sPath);
	
	// 设置文件属性
	XXAPI int xrtFileSetAttr(str sPath, int iAttr);
	
	// 获取文件最后一次访问时间
	XXAPI int64 xrtFileGetAccessTime(str sPath);
	
	// 获取文件修改时间
	XXAPI int64 xrtFileGetChangeTime(str sPath);
	
	// 复制文件
	XXAPI int xrtFileCopy(str sSrc, str sDst, int bReWrite);
	
	// 移动文件（重命名）
	XXAPI int xrtFileMove(str sSrc, str sDst, int bReWrite);
	
	// 删除文件
	XXAPI int xrtFileDelete(str sPath);
	
	// 扫描文件夹
	XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
	
	// 创建文件夹
	XXAPI int xrtDirCreate(str sPath);
	
	// 创建多级文件夹
	XXAPI int xrtDirCreateAll(str sPath);
	
	// 复制文件夹
	XXAPI int xrtDirCopy(str sSrc, str sDst, int bReWrite);
	
	// 移动文件夹
	XXAPI int xrtDirMove(str sSrc, str sDst, int bReWrite);
	
	// 删除文件夹
	XXAPI int xrtDirDelete(str sPath);
	
	
	
	/* ------------------------------------ Thread 函数库 ------------------------------------ */
	
	// 线程数据结构
	typedef struct {
		ptr Handle;
		uint32 TID;
		uint32 (*Proc)(ptr param);
		ptr Param;
	} xthread_struct, *xthread;
	
	// 创建线程
	XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize);
	
	
	
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
	XXAPI uint32 xrtHash32_WithSeed(void* key, size_t len, unsigned int seed);
	XXAPI uint32 xrtHash32(void* key, size_t len);
	
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
	XXAPI uint64 xrtHash64_WithSeed(void* key, size_t len, unsigned long long seed);
	XXAPI uint64 xrtHash64(void* key, size_t len);
	XXAPI uint64 xrtHash64_Micro_WithSeed(void* key, size_t len, unsigned long long seed);
	XXAPI uint64 xrtHash64_Micro(void* key, size_t len);
	XXAPI uint64 xrtHash64_Nano_WithSeed(void* key, size_t len, unsigned long long seed);
	XXAPI uint64 xrtHash64_Nano(void* key, size_t len);
	
	
	
	/* ------------------------------------ Network 函数库 ------------------------------------ */
	
	// 获取本机 IP ( 需使用 xrtFree 释放 )
	str xrtGetLocalIP();
	
	// 获取本机 IP ( 返回 uint32 )
	uint32 xrtGetLocalRawIP();
	
	// 获取本机 MAC 地址 ( 需使用 xrtFree 释放 )
	str xrtGetLocalMAC();
	
	// 获取本机名称 ( 需使用 xrtFree 释放 )
	str xrtGetLocalName();
	
	
	
	/* ------------------------------------ XID 函数库 ------------------------------------ */
	
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
	XXAPI int xrtCompXID(xid pXID1, xid pXID2);
	
	
	
	/* ------------------------------------ Buffer 函数库 ------------------------------------ */
	
	// 内容类型
	#define XBUFFER_BINARY 0					// 二进制
	#define XBUFFER_ANSI 1						// ANSI 字符串
	#define XBUFFER_UTF8 1						// UTF8 字符串
	#define XBUFFER_UTF16 2						// UTF16 字符串
	#define XBUFFER_UTF32 4						// UTF32 字符串
	
	// 默认增量长度
	#define XBUFFER_ALLOC_STEP 0x10000
	
	// 内存缓冲区管理单元数据结构
	typedef struct {
		char* Buffer;							// 内存缓冲区
		unsigned int Length;					// 内存长度
		unsigned int AllocLength;				// 已申请内存长度
		unsigned int AllocStep;					// 预分配内存步长
	} xbuffer_struct, *xbuffer;
	
	// 创建内存缓冲区管理器
	XXAPI xbuffer xrtBufferCreate(unsigned int iAllocLength, unsigned int iStep);
	
	// 销毁内存缓冲区管理器
	XXAPI void xrtBufferDestroy(xbuffer pBuf);
	
	// 初始化缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferInit(xbuffer pBuf, unsigned int iAllocLength, unsigned int iStep);
	
	// 释放缓冲区管理器（对自维护结构体指针使用）
	XXAPI void xrtBufferUnit(xbuffer pBuf);
	
	// 分配内存
	XXAPI int xrtBufferMalloc(xbuffer pBuf, unsigned int iCount);
	
	// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
	XXAPI int xrtBufferInsert(xbuffer pBuf, unsigned int iPos, void* pData, unsigned int iSize, unsigned int bStrMode);
	
	// 末尾添加数据
	XXAPI int xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode);
	
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
		uint Count;								// 管理器中存在多少成员
		uint AllocCount;						// 已经申请的结构数量
		uint AllocStep;							// 预分配内存步长
	} xparray_struct, *xparray;
	
	// 创建指针内存管理器
	XXAPI xparray xrtPtrArrayCreate();
	
	// 销毁指针内存管理器
	XXAPI void xrtPtrArrayDestroy(xparray pObject);
	
	// 初始化内存管理器（对自维护结构体指针使用，和 PAMM_Create 功能类似）
	XXAPI void xrtPtrArrayInit(xparray pObject);
	
	// 释放内存管理器（对自维护结构体指针使用，和 PAMM_Destroy 功能类似）
	XXAPI void xrtPtrArrayUnit(xparray pObject);
	
	// 分配内存
	XXAPI int xrtPtrArrayMalloc(xparray pObject, unsigned int iCount);
	
	// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
	XXAPI unsigned int xrtPtrArrayInsert(xparray pObject, unsigned int iPos, void* pVal);
	
	// 末尾添加成员
	XXAPI unsigned int xrtPtrArrayAppend(xparray pObject, void* pVal);
	
	// 添加成员，自动查找空隙（替换为 NULL 的值）
	XXAPI unsigned int xrtPtrArrayAddAlt(xparray pObject, void* pVal);
	
	// 交换成员
	XXAPI int xrtPtrArraySwap(xparray pObject, unsigned int iPosA, unsigned int iPosB);
	
	// 删除成员
	XXAPI int xrtPtrArrayRemove(xparray pObject, unsigned int iPos, unsigned int iCount);
	
	// 删除所有成员
	#define xrtPtrArrayRemoveAll xrtPtrArrayUnit
	
	// 清空管理器
	#define xrtPtrArrayClear xrtPtrArrayUnit
	
	// 获取成员指针
	XXAPI void* xrtPtrArrayGet(xparray pObject, unsigned int iPos);
	XXAPI void* xrtPtrArrayGet_Unsafe(xparray pObject, unsigned int iPos);
	static inline void* xrtPtrArrayGet_Inline(xparray pObject, unsigned int iPos)
	{
		return pObject->Memory[iPos - 1];
	}
	
	// 设置成员指针
	XXAPI int xrtPtrArraySet(xparray pObject, unsigned int iPos, void* pVal);
	XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, unsigned int iPos, void* pVal);
	static inline void xrtPtrArraySet_Inline(xparray pObject, unsigned int iPos, void* pVal)
	{
		pObject->Memory[iPos - 1] = pVal;
	}
	
	// 成员排序
	XXAPI int xrtPtrArraySort(xparray pObject, void* procCompar);
	
	
	
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
	} xarray_struct, *xarray;
	
	// 创建数组
	XXAPI xarray xrtArrayCreate(uint32 iItemLength);
	
	// 销毁数组
	XXAPI void xrtArrayDestroy(xarray pArr);
	
	// 初始化数组的数据结构 ( 用于内嵌数组的对象使用 )
	XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength);
	
	// 释放数组的数据结构 ( 但不会释放数组结构体本身的内存，用于内嵌数组的对象使用 )
	XXAPI void xrtArrayUnit(xarray pArr);
	
	// 分配内存
	XXAPI int xrtArrayAlloc(xarray pArr, uint32 iCount);
	
	// 中间插入成员
	XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
	
	// 末尾添加成员
	XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
	
	// 交换成员
	XXAPI int xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB);
	
	// 删除成员
	XXAPI int xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
	
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
	XXAPI int xrtArraySort(xarray pArr, ptr procCompar);
	
	
	
	/* ------------------------------------ BSMM 函数库 ------------------------------------ */
	
	/*
		Blocks Struct Memory Management [块结构内存管理器]
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 内存指针单向链表数据结构
	typedef struct MemPtr_LLNode {
		void* Ptr;
		struct MemPtr_LLNode* Next;
	} MemPtr_LLNode;
	
	// 数据块结构内存管理器数据结构
	typedef struct {
		unsigned int ItemLength;			// 成员占用内存长度
		unsigned int Count;					// 管理器中存在多少成员
		xparray_struct PageMMU;				// 内存页管理器
		MemPtr_LLNode* LL_Free;				// 已释放的内存块链表
	} xbsmm_struct, *xbsmm;
	
	// 创建数据块结构内存管理器
	XXAPI xbsmm xrtBsmmCreate(unsigned int iItemLength);
	
	// 销毁数据块结构内存管理器
	XXAPI void xrtBsmmDestroy(xbsmm objBSMM);
	
	// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
	XXAPI void xrtBsmmInit(xbsmm objBSMM, unsigned int iItemLength);
	
	// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
	XXAPI void xrtBsmmUnit(xbsmm objBSMM);
	
	// 申请结构体内存
	XXAPI void* xrtBsmmAlloc(xbsmm objBSMM);
	
	// 释放结构体内存
	XXAPI void xrtBsmmFree(xbsmm objBSMM, void* Ptr);
	
	// 获取成员指针（非特殊需求不建议使用）
	static inline void* xrtBsmmGetPtr_Inline(xbsmm objBSMM, unsigned int iIdx)
	{
		unsigned int iBlock = iIdx >> 8;
		unsigned int iPos = iIdx & 0xFF;
		char* pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
		if ( pBlock ) {
			return &pBlock[iPos * objBSMM->ItemLength];
		} else {
			return NULL;
		}
	}
	
	
	
	/* ------------------------------------ Memory Unit 函数库 ------------------------------------ */
	
	// 内存固定的前置数据（用于识别内存是哪个管理器分配的）
	typedef struct {
		unsigned int ItemFlag;
	} MMU_Value, *MMU_ValuePtr;
	
	// MMU有效ID区间掩码
	#define MMU_FLAG_MASK				0x3FFFFFFF
	
	// 结构体使用状态标记
	#define MMU_FLAG_USE				0x80000000
	
	// GC回收标记位
	#define MMU_FLAG_GC					0x40000000
	
	// 非内存管理器管理的内存
	#define MMU_FLAG_EXT				0xBFFFFFFF
	
	// MM256 or MM64K GC标记
	#define xrtMemUnitGC_Mark(p) ((MMU_ValuePtr)((void*)p - sizeof(MMU_Value)))->ItemFlag |= MMU_FLAG_GC
	
	// 数据管理单元数据结构
	typedef struct {
		unsigned char FreeList[256];				// 已释放成员列表
		unsigned int ItemLength;					// 成员占用内存长度
		unsigned short Count;						// 成员数量
		unsigned char FreeCount;					// 已释放成员数量
		unsigned char FreeOffset;					// 首个已释放成员在列表中的偏移位置
		unsigned int Flag;							// 值的 Flag 前缀（由上级管理器下发，0-255 区间会被 idx 覆盖）
		char Memory[];								// 数据
	} xmemunit_struct, *xmemunit;
	
	// 创建内存管理单元（iItemLength会自动增加4个字节用于确定内存位置和所属的管理器单元编号）
	XXAPI xmemunit xrtMemUnitCreate(unsigned int iItemLength);
	
	// 销毁内存管理单元
	#define xrtMemUnitDestroy xrtFree
	
	// 从内存管理单元中申请一个元素
	static inline void* xrtMemUnitAlloc_Inline(xmemunit objUnit)
	{
		unsigned char idx = objUnit->Count;
		// 优先复用已释放的数据
		if ( objUnit->FreeCount > 0 ) {
			idx = objUnit->FreeList[objUnit->FreeOffset];
			objUnit->FreeOffset++;
			objUnit->FreeCount--;
		}
		objUnit->Count++;
		MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
		v->ItemFlag = objUnit->Flag | idx;
		return (void*)&v[1];
	}
	XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
	
	// 释放内存管理单元中某个元素（FreeIdx不会清空 ItemFlag，建议由调用方负责清空）
	static inline void xrtMemUnitFreeIdx_Inline(xmemunit objUnit, unsigned char idx)
	{
		objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
		objUnit->Count--;
		if ( objUnit->Count ) {
			objUnit->FreeCount++;
		} else {
			objUnit->FreeCount = 0;
			objUnit->FreeOffset = 0;
		}
	}
	XXAPI int xrtMemUnitFreeIdx(xmemunit objUnit, unsigned char idx);
	static inline void xrtMemUnitFree_Inline(xmemunit objUnit, void* obj)
	{
		MMU_ValuePtr v = obj - 4;
		unsigned char idx = v->ItemFlag & 0xFF;
		v->ItemFlag = 0;
		xrtMemUnitFreeIdx_Inline(objUnit, idx);
	}
	XXAPI int xrtMemUnitFree(xmemunit objUnit, void* obj);
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI int xrtMemUnitGC(xmemunit objUnit, int bFreeMark);
	
	
	
	/* ------------------------------------ Fixed-Size Memory Pool 函数库 ------------------------------------ */
	
	// 内存管理器链表结构
	typedef struct MMU_LLNode {
		unsigned int Flag;
		xmemunit objMMU;
		struct MMU_LLNode* Prev;
		struct MMU_LLNode* Next;
	} MMU_LLNode;
	
	// 256步进内存管理器数据结构
	typedef struct {
		unsigned int ItemLength;					// 成员占用内存长度
		xbsmm_struct arrMMU;						// MMU 阵列
		MMU_LLNode* LL_Idle;						// 有空闲的内存管理单元链表首元素 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的内存管理单元链表首元素 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 缓存的全空内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的内存管理单元 Flag 链表首元素 (申请新单元优先从这里找)
	} xfsmempool_struct, *xfsmempool;
	
	// 创建内存管理器
	XXAPI xfsmempool xrtFSMemPoolCreate(unsigned int iItemLength);
	
	// 销毁内存管理器
	XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM);
	
	// 初始化内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolInit(xfsmempool objMM, unsigned int iItemLength);
	
	// 释放内存管理器（对自维护结构体指针使用）
	XXAPI void xrtFSMemPoolUnit(xfsmempool objMM);
	
	// 从内存管理器中申请一块内存
	XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
	
	// 将内存管理器申请的内存释放掉
	XXAPI void xrtFSMemPoolFree(xfsmempool objMM, void* ptr);
	
	// 将一块内存标记为使用中
	#define xrtFSMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtFSMemPoolGC(xfsmempool objMM, int bFreeMark);
	
	
	
	/* ------------------------------------ Stack 函数库 ------------------------------------ */
	
	// 结构体静态栈数据结构
	typedef struct {
		union {
			char* Memory;					// 栈数据内存 - 结构体
			ptr* PtrMem;					// 栈数据内存 - 指针
		};
		unsigned int ItemLength;			// 栈成员占用内存长度
		unsigned int MaxCount;				// 栈最大可以容纳多少成员（栈深度）
		unsigned int Count;					// 栈中存在多少成员（栈顶位置）
	} xstack_struct, *xstack;
	
	// 创建结构体静态栈
	XXAPI xstack xrtStackCreate(unsigned int iMaxCount, unsigned int iItemLength);
	
	// 销毁结构体静态栈
	#define xrtStackDestroy xrtFree
	
	// 压栈
	XXAPI ptr xrtStackPush(xstack objSTK);
	XXAPI uint xrtStackPushData(xstack objSTK, ptr pData);
	XXAPI uint xrtStackPushPtr(xstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtStackPop(xstack objSTK);
	XXAPI ptr xrtStackPopPtr(xstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtStackTop(xstack objSTK);
	XXAPI ptr xrtStackTopPtr(xstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtStackGetPos(xstack objSTK, uint iPos);
	XXAPI ptr xrtStackGetPos_Unsafe(xstack objSTK, uint iPos);
	XXAPI ptr xrtStackGetPosPtr(xstack objSTK, uint iPos);
	XXAPI ptr xrtStackGetPosPtr_Unsafe(xstack objSTK, uint iPos);
	
	
	
	/* ------------------------------------ Dynamic Stack 函数库 ------------------------------------ */
	
	/*
		结构体动态栈，结构体内存256个递增，栈最大深度不固定
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
	*/
	
	// 结构体动态栈数据结构
	typedef struct {
		unsigned int ItemLength;					// 栈成员占用内存长度
		unsigned int Count;							// 栈中存在多少成员（栈顶位置）
		xparray_struct MMU;							// MMU 管理器
	} xdynstack_struct, *xdynstack;
	
	// 创建结构体动态栈
	XXAPI xdynstack xrtDynStackCreate(unsigned int iItemLength);
	
	// 销毁结构体动态栈
	XXAPI void xrtDynStackDestroy(xdynstack objSTK);
	
	// 初始化结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackInit(xdynstack objSTK, unsigned int iItemLength);
	
	// 释放结构体动态栈（对自维护结构体指针使用）
	XXAPI void xrtDynStackUnit(xdynstack objSTK);
	
	// 压栈
	XXAPI ptr xrtDynStackPush(xdynstack objSTK);
	XXAPI uint xrtDynStackPushData(xdynstack objSTK, ptr pData);
	XXAPI uint xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
	
	// 出栈
	XXAPI ptr xrtDynStackPop(xdynstack objSTK);
	XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
	
	// 获取栈顶对象
	XXAPI ptr xrtDynStackTop(xdynstack objSTK);
	XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
	
	// 获取任意位置对象
	XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, unsigned int iPos);
	XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, unsigned int iPos);
	XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, unsigned int iPos);
	XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, unsigned int iPos);
	
	
	
	/* ------------------------------------ Linked List Base 函数库 ------------------------------------ */
	
	// 双向链表节点基础定义
	typedef struct LList_NodeBase {
		struct LList_NodeBase* Prev;
		struct LList_NodeBase* Next;
	} xllistnode_struct, *xllistnode;
	
	// 链表对象数据结构
	typedef struct {
		xllistnode FirstNode;
		xllistnode LastNode;
		unsigned int Count;
	} xllistbase_struct, *xllistbase;
	
	// 初始化链表
	#define xrtLLB_Init(o) (o)->FirstNode = NULL; (o)->LastNode = NULL; (o)->Count = 0
	
	// 释放链表
	#define xrtLLB_Unit xrtLLB_Init
	
	// 节点前插入 (objNode为空则插入到FirstNode之前)
	XXAPI void xrtLLB_InsertPrev(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
	
	// 节点后插入 (objNode为空则插入到LastNode之后)
	XXAPI void xrtLLB_InsertNext(xllistbase objLLB, xllistnode objNode, xllistnode objNewNode);
	
	// 删除节点
	XXAPI void xrtLLB_Remove(xllistbase objLLB, xllistnode objNode);
	
	// 删除所有成员
	#define xrtLLB_RemoveAll LLB_Unit
	
	// 清空管理器
	#define xrtLLB_Clear LLB_Unit
	
	
	
	/* ------------------------------------ Linked List 函数库 ------------------------------------ */
	
	// 链表对象数据结构
	typedef struct {
		xllistnode FirstNode;
		xllistnode LastNode;
		unsigned int Count;
		xfsmempool_struct objMM;
	} xllist_struct, *xllist;
	
	// 创建链表
	XXAPI xllist xrtLListCreate(unsigned int iItemLength);
	
	// 销毁链表
	XXAPI void xrtLListDestroy(xllist objLL);
	
	// 初始化链表（对自维护结构体指针使用）
	XXAPI void xrtLListInit(xllist objLL, unsigned int iItemLength);
	
	// 释放链表（对自维护结构体指针使用）
	XXAPI void xrtLListUnit(xllist objLL);
	
	// 节点前插入 (objNode为空则插入到FirstNode之前)
	XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode);
	
	// 节点后插入 (objNode为空则插入到LastNode之后)
	XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode);
	
	// 删除节点
	XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode);
	
	// 删除所有成员
	#define xrtLListRemoveAll xrtLListUnit
	
	// 清空管理器
	#define xrtLListClear xrtLListUnit
	
	
	
	/* ------------------------------------ AVLTree Base 函数库 ------------------------------------ */
	
	// AVL树最大高度
	#define AVLTree_MAX_HEIGHT  48
	
	// AVL树节点基础定义
	typedef struct xavltnode_struct {
		struct xavltnode_struct* left;
		struct xavltnode_struct* right;
		int height;
	} xavltnode_struct, *xavltnode;
	
	// AVL树对象数据结构
	typedef struct {
		xavltnode RootNode;
		unsigned int Count;
	} xavltbase_struct, *xavltbase;
	
	// 比较回调函数
	typedef int (*AVLTree_CompProc)(void* pNode, void* pKey);
	
	// 遍历回调函数
	typedef int (*AVLTree_EachProc)(void* pNode, void* pArg);
	
	// 获取 xavltnode 对象
	#define xrtAVLTreeGetNodeBase(p) ((xavltnode)((void*)p - sizeof(xavltnode_struct)))
	
	// 获取 xavltnode 对应的数据段
	#define xrtAVLTreeGetNodeData(p) ((void*)(&p[1]))
	
	// 获取根节点数据段
	#define xrtAVLTreeGetRootData(obj) xrtAVLTreeGetNodeData(obj->RootNode)
	
	// 初始化 AVLTree
	#define xrtAVLTB_Init(o) (o)->RootNode = NULL; (o)->Count = 0
	
	// 释放 AVLTree
	#define xrtAVLTB_Unit xrtAVLTB_Init
	
	// 向 AVLTree 中插入节点
	XXAPI xavltnode xrtAVLTB_Insert(xavltbase objAVLT, AVLTree_CompProc procComp, void* pKey, xavltnode pNewNode);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI xavltnode xrtAVLTB_Remove(xavltbase objAVLT, AVLTree_CompProc procComp, void* pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI xavltnode xrtAVLTB_Search(xavltbase objAVLT, AVLTree_CompProc procComp, void* pKey);
	
	// 删除所有成员
	#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
	
	// 清空管理器
	#define xrtAVLTB_Clear xrtAVLTB_Unit
	
	// 遍历 AVLTree 所有节点
	XXAPI int xrtAVLTB_WalkRecuProc(xavltnode root, AVLTree_EachProc procEach, void* pArg);
	XXAPI int xrtAVLTB_WalkExRecuProc(xavltnode root, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, void* pArg);
	#define xrtAVLTB_Walk(obj, p, a) AVLTB_WalkRecuProc(obj->RootNode, (void*)p, (void*)a)
	#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) AVLTB_WalkExRecuProc(obj->RootNode, (void*)p1, (void*)p2, (void*)p3, (void*)a)
	
	
	
	/* ------------------------------------ AVLTree 函数库 ------------------------------------ */
	
	// 值释放回调函数
	typedef void (*AVLTree_FreeProc)(void* objTree, void* pNode);
	
	// AVL树对象数据结构
	typedef struct {
		xavltnode RootNode;
		unsigned int Count;
		AVLTree_CompProc CompProc;
		AVLTree_FreeProc FreeProc;
		xfsmempool_struct objMM;
		void* ExtData;
		xavltnode NodeCache;
	} xavltree_struct, *xavltree;
	
	// 创建 AVLTree
	XXAPI xavltree xrtAVLTreeCreate(unsigned int iItemLength, AVLTree_CompProc procComp);
	
	// 销毁 AVLTree
	XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
	
	// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
	XXAPI void xrtAVLTreeInit(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp);
	
	// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
	XXAPI void xrtAVLTreeUnit(xavltree objAVLT);
	
	// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
	XXAPI void* xrtAVLTreeInsert(xavltree objAVLT, void* pKey, int* bNew);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI int xrtAVLTreeRemove(xavltree objAVLT, void* pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI void* xrtAVLTreeSearch(xavltree objAVLT, void* pKey);
	
	// 删除所有成员
	#define xrtAVLTreeRemoveAll xrtAVLTreeUnit
	
	// 清空管理器
	#define xrtAVLTreeClear xrtAVLTreeUnit
	
	// 遍历 AVLTree 所有节点
	#define xrtAVLTreeWalk xrtAVLTB_Walk
	#define xrtAVLTreeWalkEx xrtAVLTB_WalkEx
	
	
	
	/* ------------------------------------ Memory Pool 函数库 ------------------------------------ */
	
	// MP256 or MP64K 大内存结构体前置结构
	typedef struct {
		unsigned int Index;							// BigMM 的块索引
		unsigned int Flag;							// 符合 MM256 标准的 Flag
	} MP_MemHead;
	
	// MP256 or MP64K 大内存信息链表结构体（实际返回的内存地址相当于 Ptr + sizeof(MP_MemHead)）
	typedef struct MP_BigInfoLL {
		unsigned int Size;							// 申请内存的大小，可有可无（可开发辅助功能，如泄漏检测）
		void* Ptr;									// 指针地址，使用 mmu_malloc 返回的地址
		struct MP_BigInfoLL* Next;					// 下一个链表节点（用于释放链表）
	} MP_BigInfoLL;
	
	// 单个长度区间的内存管理器结构
	typedef struct FSB_Item {
		unsigned int MinLength;						// 支持最小的内存长度
		unsigned int MaxLength;						// 支持最大的内存长度
		MMU_LLNode* LL_Idle;						// 空闲的 MMU 内存管理单元链表 (优先分配内存的单元)
		MMU_LLNode* LL_Full;						// 满载的 MMU 内存管理单元链表 (不会从这些单元中分配内存)
		MMU_LLNode* LL_Null;						// 全空的 MMU 内存管理单元 (备用单元，最多只留一个)
		MMU_LLNode* LL_Free;						// 已释放的 MMU 内存管理单元链表 (申请新单元优先从这里找)
		struct FSB_Item* left;						// 左子树
		struct FSB_Item* right;						// 右子树
	} FSB_Item;
	
	// 256步进内存池数据结构
	typedef struct {
		FSB_Item* FSB_Memory;						// fixed-size-blocks 内存（MP256_Create参数为 1 或 2 时自动创建，否则需要手动创建，不为空会调用 mmu_free 释放）
		FSB_Item* FSB_RootNode;						// fixed-size-blocks 二叉树（固定大小区块内存管理器阵列）
		xbsmm_struct arrMMU;						// MMU 阵列
		xbsmm_struct BigMM;							// 大内存数组
		MP_BigInfoLL* LL_BigFree;					// 大内存已释放的内存块链表
	} xmempool_struct, *xmempool;
	
	// 创建内存池
	XXAPI xmempool xrtMemPoolCreate(int bCustom);
	
	// 销毁内存池
	XXAPI void xrtMemPoolDestroy(xmempool objMP);
	
	// 初始化内存池（对自维护结构体指针使用，和 MP256_Create 功能类似）
	XXAPI void xrtMemPoolInit(xmempool objMP, int bCustom);
	
	// 释放内存池（对自维护结构体指针使用，和 MP256_Destroy 功能类似）
	XXAPI void xrtMemPoolUnit(xmempool objMP);
	
	// 从内存池中申请一块内存
	XXAPI void* xrtMemPoolAlloc(xmempool objMP, unsigned int iSize);
	
	// 将内存池申请的内存释放掉
	XXAPI void xrtMemPoolFree(xmempool objMP, void* ptr);
	
	// 将一块内存标记为使用中
	#define xrtMemPoolGC_Mark	xrtMemUnitGC_Mark
	
	// 进行一轮GC，将 标记 或 未标记 的内存全部回收
	XXAPI void xrtMemPoolGC(xmempool objMP, int bFreeMark);
	
	
	
	/* ------------------------------------ Dict 函数库 ------------------------------------ */
	
	// 字典 Key 数据结构
	#if defined(__x86_64__) || defined(_M_X64)
		// 64 bit [ 只取 32 位 ]
		typedef struct {
			void* Key;
			uint32 Hash;
			uint32 KeyLen;
		} Dict_Key;
	#elif defined(__i386__) || defined(_M_IX86)
		// 32 bit
		typedef struct {
			void* Key;
			uint32 Hash;
			uint32 KeyLen;
		} Dict_Key;
	#endif
	
	// 字典对象数据结构
	typedef struct {
		xavltree_struct AVLT;
		xmempool MP;
	} xdict_struct, *xdict;
	
	// 字典遍历回调函数
	typedef int (*Dict_EachProc)(Dict_Key* pKey, void* pVal, void* pArg);
	
	// 创建哈希表
	XXAPI xdict xrtDictCreate(unsigned int iItemLength);
	
	// 销毁哈希表
	XXAPI void xrtDictDestroy(xdict objHT);
	
	// 初始化哈希表（对自维护结构体指针使用，和 AVLHT32_Create 功能类似）
	XXAPI void xrtDictInit(xdict objHT, unsigned int iItemLength);
	
	// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
	XXAPI void xrtDictUnit(xdict objHT);
	
	// 设置值
	XXAPI void* xrtDictSet(xdict objHT, void* sKey, unsigned int iKeyLen, int* bNewRet);
	
	// 设置值 - 当值为 void* 时直接修改指针内容
	XXAPI int xrtDictSetPtr(xdict objHT, void* sKey, unsigned int iKeyLen, void* pVal, void** ppOldVal);
	
	// 获取值
	XXAPI void* xrtDictGet(xdict objHT, void* sKey, unsigned int iKeyLen);
	
	// 获取值 - 当值为 void* 时直接获取指针内容
	XXAPI void* xrtDictGetPtr(xdict objHT, void* sKey, unsigned int iKeyLen);
	
	// 删除值
	XXAPI int xrtDictRemove(xdict objHT, void* sKey, unsigned int iKeyLen);
	
	// 判断值是否存在
	XXAPI int xrtDictExists(xdict objHT, void* sKey, unsigned int iKeyLen);
	
	// 删除所有成员
	#define xrtDictRemoveAll xrtDictUnit
	
	// 清空管理器
	#define xrtDictClear xrtDictUnit
	
	// 获取表内元素数量
	XXAPI unsigned int xrtDictCount(xdict objHT);
	
	// 遍历表元素
	XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, void* pArg);
	
	
	
	/* ------------------------------------ List 函数库 ------------------------------------ */
	
	// 列表对象数据结构
	typedef struct {
		xavltree_struct AVLT;
	} xlist_struct, *xlist;
	
	// 列表遍历回调函数
	typedef int (*List_EachProc)(int64 pKey, void* pVal, void* pArg);
	
	// 创建列表
	XXAPI xlist xrtListCreate(unsigned int iItemLength);
	
	// 销毁列表
	XXAPI void xrtListDestroy(xlist objList);
	
	// 初始化列表（对自维护结构体指针使用）
	XXAPI void xrtListInit(xlist objList, unsigned int iItemLength);
	
	// 释放列表（对自维护结构体指针使用）
	XXAPI void xrtListUnit(xlist objList);
	
	// 设置值
	XXAPI ptr xrtListSet(xlist objList, int64 iKey, int* bNewRet);
	
	// 设置值 - 当值为 void* 时直接修改指针内容
	XXAPI int xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
	
	// 获取值
	XXAPI ptr xrtListGet(xlist objList, int64 iKey);
	
	// 获取值 - 当值为 void* 时直接获取指针内容
	XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
	
	// 删除值
	XXAPI int xrtListRemove(xlist objList, int64 iKey);
	
	// 判断值是否存在
	XXAPI int xrtListExists(xlist objList, int64 iKey);
	
	// 获取表内元素数量
	XXAPI unsigned int xrtListCount(xlist objList);
	
	// 遍历表元素
	XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg);
	
	
	
	/* ------------------------------------ Value 函数库 ------------------------------------ */
	/*
	// 数据类型 - 主类型
	#define XRT_DT_EMPTY			0				// 不存在的数据
	#define XRT_DT_NULL				1				// null
	#define XRT_DT_BOOL				2				// bool : true | false
	#define XRT_DT_INT				3				// 整数（int64）
	#define XRT_DT_FLOAT			4				// 浮点数（double）
	#define XRT_DT_TEXT				5				// 字符串
	#define XRT_DT_TIME				6				// 时间
	#define XRT_DT_POINT			7				// 指针
	#define XRT_DT_STRUCT			8				// 结构体
	#define XRT_DT_FUNCTION			9				// 函数
	#define XRT_DT_COLLECT			10				// 集合
	#define XRT_DT_ARRAY			11				// 数组
	#define XRT_DT_LIST				12				// 列表
	#define XRT_DT_TABLE			13				// 表
	#define XRT_DT_OBJECT			14				// 对象
	#define XRT_DT_SHEET			15				// 数据表
	#define XRT_DT_CUSTOM			16				// 自定义
	
	// 数据类型 - 子类型 [ 逻辑 ]
	#define XRT_SDT_BOOL_FALSE		0				// false
	#define XRT_SDT_BOOL_TRUE		1				// true
	
	// 数据类型 - 子类型 [ 数字 ]
	#define XRT_SDT_NUM_INT			0				// 整数 ( int64 )
	#define XRT_SDT_NUM_FLOAT		1				// 浮点数 ( double )
	
	// 数据类型 - 子类型 [ 字符串 ]
	#define XRT_SDT_STR_U8			0				// utf-8 字符串
	#define XRT_SDT_STR_U16			1				// utf-16 字符串
	#define XRT_SDT_STR_U32			2				// utf-32 字符串
	#define XRT_SDT_STR_BIN			3				// 二进制数据
	
	// 数据类型 - 子类型 [ 函数 ]
	#define XRT_SDT_FUNC_XL			0				// xlang 函数
	#define XRT_SDT_FUNC_CL			1				// clang 函数
	
	// 函数类型回调
	typedef struct xvalue_struct xvalue_struct, *xvalue;
	typedef struct xcustom_struct xcustom_struct, *xcustom;
	typedef xvalue (*xfunction)(xvalue varENV, xvalue varParam);
	
	// Value 基类 [ 4 byte ]
	struct {
		uint8 Type;					// 值的主类型
		uint8 SubType;				// 值的子类型
		uint isStatic : 1;			// 是否为静态资源 ( 不会自动释放 )
	} xvalue_base_struct, *xvalue_base;
	
	// Value 标准数据类 [ XRT_DT_NUM、XRT_DT_TEXT、XRT_DT_TIME、XRT_DT_POINT ]
	struct {
		uint8 Type;
		uint8 SubType;
		uint16 Flag;
		union {
			uint32 Size;			// XRT_DT_TEXT、XRT_DT_STRUCT 使用
			uint32 RetCount;		// XRT_DT_COLLECT、XRT_DT_ARRAY、XRT_DT_TABLE、
		};
		union {
			int64 vInt;
			double vFloat;
			str vText;
			ptr vTemplate;
			xtime vTime;
			ptr vPoint;
			ptr vStruct;
			ptr vCollect;
			ptr vArray;
			ptr vTable;
			ptr vObject;
			ptr vSheet;
			xfunction vFunc;
			ptr vFuncC;
		};
	} xvalue_struct, *xvalue;
	
	// Custom 类 [ 16 bytes ]
	struct xcustom_struct {
		void (*construct)(xcustom var);
		void (*destruct)(xcustom var);
		int (*set)(xcustom var, str key, xcustom val);
		xvalue (*get)(xcustom var, str key);
		xvalue (*call)(xcustom var, str key, xvalue param);
		ptr value;
	};
	*/
	
	
	
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
	
	// 解析字符串
	static inline int jnum_parse(const char *str, jnum_type_t *type, jnum_value_t *value)
	{
		const char *s = str;
		while (1) {
			switch (*s) {
			case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': case ' ': ++s; break;
			default: goto next;
			}
		}
	next:
		return (int)(xrtParseNum(s, type, value) + (s - str));
	}
	
	// 字符串转数字
	XXAPI int32_t xrtStrToI32(const char* pStr);
	XXAPI int64_t xrtStrToI64(const char* pStr);
	XXAPI uint32_t xrtStrToU32(const char* pStr);
	XXAPI uint64_t xrtStrToU64(const char* pStr);
	XXAPI double xrtStrToNum(const char* pStr);
	
	
	
	/* ------------------------------------ JSON 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Template 函数库 ------------------------------------ */
	
	
	
#endif


