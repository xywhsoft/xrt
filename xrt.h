


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
	
	
	
	/* ------------------------------------ OS 函数库 ------------------------------------ */
	
	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
	
	
	
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
	
	
	
	/* ------------------------------------ Array 函数库 ------------------------------------ */
	
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
	
	// 直接插入指针数据
	XXAPI uint32 xrtArrayInsertPtr(xarray pArr, uint32 iPos, ptr pData);
	
	// 直接末尾添加指针数据
	XXAPI uint32 xrtArrayAppendPtr(xarray pArr, ptr pData);
	
	// 直接修改对应的指针数据
	XXAPI int xrtArraySetPtr(xarray pArr, uint32 iPos, ptr pData);
	
	// 直接获取对应的指针数据
	XXAPI ptr xrtArrayGetPtr(xarray pArr, uint32 iPos);
	XXAPI ptr xrtArrayGetPtr_Unsafe(xarray pArr, uint32 iPos);
	static inline ptr xrtArrayGetPtr_Inline(xarray pArr, uint32 iPos)
	{
		ptr* pMEM = (ptr*)&(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
		return pMEM[0];
	}
	
	// 成员排序
	XXAPI int xrtArraySort(xarray pArr, ptr procCompar);
	
	
	
	/* ------------------------------------ AVLTree Base 函数库 ------------------------------------ */
	
	// AVL树最大高度
	#define AVLTree_MAX_HEIGHT  48
	
	// AVL树节点基础定义
	typedef struct AVLTree_NodeBase {
		struct AVLTree_NodeBase* left;
		struct AVLTree_NodeBase* right;
		int height;
	} AVLTree_NodeBase;
	
	// AVL树对象数据结构
	typedef struct {
		AVLTree_NodeBase* RootNode;
		unsigned int Count;
	} AVLTree_BaseStruct, *AVLTree_BaseObject;
	
	// 比较回调函数
	typedef int (*AVLTree_CompProc)(void* pNode, void* pKey);
	
	// 遍历回调函数
	typedef int (*AVLTree_EachProc)(void* pNode, void* pArg);
	
	// 获取 AVLTree_NodeBase 结构体指针
	#define xrtAVLTree_GetNodeBase(p) ((AVLTree_NodeBase*)((void*)p - sizeof(AVLTree_NodeBase)))
	
	// 获取 AVLTree_NodeBase 结构体指针对应的数据段
	#define xrtAVLTree_GetNodeData(p) ((void*)(&p[1]))
	
	// 获取根节点数据段
	#define xrtAVLTree_GetRootData(obj) xrtAVLTree_GetNodeData(obj->RootNode)
	
	// 初始化 AVLTree
	#define xrtAVLTB_Init(o) (o)->RootNode = NULL; (o)->Count = 0
	
	// 释放 AVLTree
	#define xrtAVLTB_Unit AVLTB_Init
	
	// 向 AVLTree 中插入节点
	XXAPI AVLTree_NodeBase* xrtAVLTB_Insert(AVLTree_BaseObject objAVLT, AVLTree_CompProc procComp, void* pKey, AVLTree_NodeBase* pNewNode);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI AVLTree_NodeBase* xrtAVLTB_Remove(AVLTree_BaseObject objAVLT, AVLTree_CompProc procComp, void* pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI AVLTree_NodeBase* xrtAVLTB_Search(AVLTree_BaseObject objAVLT, AVLTree_CompProc procComp, void* pKey);
	
	// 删除所有成员
	#define xrtAVLTB_RemoveAll xrtAVLTB_Unit
	
	// 清空管理器
	#define xrtAVLTB_Clear xrtAVLTB_Unit
	
	// 遍历 AVLTree 所有节点
	XXAPI int xrtAVLTB_WalkRecuProc(AVLTree_NodeBase* root, AVLTree_EachProc procEach, void* pArg);
	XXAPI int xrtAVLTB_WalkExRecuProc(AVLTree_NodeBase* root, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, void* pArg);
	#define xrtAVLTB_Walk(obj, p, a) AVLTB_WalkRecuProc(obj->RootNode, (void*)p, (void*)a)
	#define xrtAVLTB_WalkEx(obj, p1, p2, p3, a) AVLTB_WalkExRecuProc(obj->RootNode, (void*)p1, (void*)p2, (void*)p3, (void*)a)
	
	
	
	/* ------------------------------------ Stack 函数库 ------------------------------------ */
	
	// 结构体静态栈数据结构
	typedef struct {
		char* Memory;						// 栈数据内存指针
		unsigned int ItemLength;			// 栈成员占用内存长度
		unsigned int MaxCount;				// 栈最大可以容纳多少成员（栈深度）
		unsigned int Count;					// 栈中存在多少成员（栈顶位置）
	} SSSTK_Struct, *SSSTK_Object;
	
	// 创建结构体静态栈
	XXAPI SSSTK_Object SSSTK_Create(unsigned int iMaxCount, unsigned int iItemLength);
	
	// 销毁结构体静态栈
	#define SSSTK_Destroy mmu_free
	
	// 压栈
	XXAPI void* SSSTK_Push(SSSTK_Object objSTK);
	XXAPI unsigned int SSSTK_PushData(SSSTK_Object objSTK, void* pData);
	
	// 出栈
	XXAPI void* SSSTK_Pop(SSSTK_Object objSTK);
	
	// 获取栈顶对象
	XXAPI void* SSSTK_Top(SSSTK_Object objSTK);
	
	// 获取任意位置对象
	XXAPI void* SSSTK_GetPos(SSSTK_Object objSTK, unsigned int iPos);
	XXAPI void* SSSTK_GetPos_Unsafe(SSSTK_Object objSTK, unsigned int iPos);
	
	
	
	/* ------------------------------------ LList 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Memory Pool 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ AVLTree 函数库 ------------------------------------ */
	/*
	// 值释放回调函数
	typedef void (*AVLTree_FreeProc)(void* objTree, void* pNode);
	
	// AVL树对象数据结构
	typedef struct {
		AVLTree_NodeBase* RootNode;
		unsigned int Count;
		AVLTree_CompProc CompProc;
		AVLTree_FreeProc FreeProc;
		MM256_Struct objMM;
		void* ExtData;
		AVLTree_NodeBase* NodeCache;
	} AVLTree_Struct, *AVLTree_Object;
	
	// 创建 AVLTree
	XXAPI AVLTree_Object AVLTree_Create(unsigned int iItemLength, AVLTree_CompProc procComp);
	
	// 销毁 AVLTree
	XXAPI void AVLTree_Destroy(AVLTree_Object objAVLT);
	
	// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
	XXAPI void AVLTree_Init(AVLTree_Object objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp);
	
	// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
	XXAPI void AVLTree_Unit(AVLTree_Object objAVLT);
	
	// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
	XXAPI void* AVLTree_Insert(AVLTree_Object objAVLT, void* pKey, int* bNew);
	
	// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
	XXAPI int AVLTree_Remove(AVLTree_Object objAVLT, void* pKey);
	
	// 在 AVLTree 中查找节点
	XXAPI void* AVLTree_Search(AVLTree_Object objAVLT, void* pKey);
	
	// 删除所有成员
	#define AVLTree_RemoveAll AVLTree_Unit
	
	// 清空管理器
	#define AVLTree_Clear AVLTree_Unit
	
	// 遍历 AVLTree 所有节点
	#define AVLTree_Walk AVLTB_Walk
	#define AVLTree_WalkEx AVLTB_WalkEx
	*/
	
	
	
	/* ------------------------------------ Collect 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ List 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Table 函数库 ------------------------------------ */
	
	
	
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
	
	
	
	/* ------------------------------------ JSON 函数库 ------------------------------------ */
	
	
	
	/* ------------------------------------ Template 函数库 ------------------------------------ */
	
	
	
#endif


