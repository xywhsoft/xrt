


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
	typedef u8str str;
	typedef wchar_t* wstr;			// windows 系统为 u16str，linux 系统为 u32str
	
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
		
		// 调试模式
		int DebugMode;
		
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
		
		// 内置通用错误描述
		struct {
			str MALLOC;
		} ERROR_DESC;
		
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
	
	// 获取范围随机数
	XXAPI int xrtRandRange(int min, int max);
	
	
	
	/* ------------------------------------ String 函数库 ------------------------------------ */
	
	// 创建字符串副本（ 需使用 xrtFree 释放 ）
	XXAPI str xrtCopyStr(str sText, size_t iSize);
	XXAPI wstr xrtCopyStrW(wstr sText, size_t iSize);
	XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
	XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
	
	// 字符串转为小写（ bSrcRevise 为 false 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLCase(str sText, size_t iSize, int bSrcRevise);
	XXAPI wstr xrtLCaseW(wstr sText, size_t iSize, int bSrcRevise);
	
	// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtUCase(str sText, size_t iSize, int bSrcRevise);
	XXAPI wstr xrtUCaseW(wstr sText, size_t iSize, int bSrcRevise);
	
	// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）
	XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, int bCase);
	XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, int bCase);
	XXAPI wstr xrtFindStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase);
	XXAPI uint xrtInStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase);
	
	// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
	XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
	XXAPI wstr xrtCheckStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize);
	
	// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtLTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtRTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	
	// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
	XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, int bSrcRevise);
	XXAPI wstr xrtFilterStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise);
	
	// 字符串格式化（ 需使用 xrtFree 释放 ）
	XXAPI str xrtFormat(str sFormat, ...);
	XXAPI wstr xrtFormatW(wstr sFormat, ...);
	
	// 字符串替换（ 需使用 xrtFree 释放 ）
	XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
	XXAPI wstr xrtReplaceW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, wstr sRepText, size_t iRepSize);
	
	// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
	XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, int bSrcRevise);
	XXAPI wstr* xrtSplitW(wstr sText, size_t iSize, wstr sSepText, size_t iSepSize, int bSrcRevise);
	
	// 生成随机字符串（ 需使用 xrtFree 释放 ）
	XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
	XXAPI wstr xrtRandStrW(wstr sTemplate, size_t iSize, size_t iLen);
	
	// HEX 编码（ 需使用 xrtFree 释放 ）
	XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
	XXAPI wstr xrtHexEncodeW(ptr pMem, size_t iSize);
	
	// HEX 解码（ 需使用 xrtFree 释放 ）
	XXAPI ptr xrtHexDecode(str pText, size_t iSize);
	XXAPI ptr xrtHexDecodeW(wstr pText, size_t iSize);
	
	// Base64 编码（ 需使用 xrtFree 释放 ）
	str xrtBase64Encode(ptr pMem, size_t iSize);
	wstr xrtBase64EncodeW(ptr pMem, size_t iSize);
	
	// Base64 解码（ 需使用 xrtFree 释放 ）
	ptr xrtBase64Decode(str sText, size_t iSize);
	ptr xrtBase64DecodeW(wstr sText, size_t iSize);
	
	
	
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
	XXAPI wstr xrtNowStrW();
	
	// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtDateStr();
	XXAPI wstr xrtDateStrW();
	
	// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeStr();
	XXAPI wstr xrtTimeStrW();
	
	// 转换日期 + 时间为字符串（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
	XXAPI wstr xrtTimeToStrW(xtime iTime, int iFormat);
	
	// 时间单位累加
	XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
	
	// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
	XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
	
	
	
	/* ------------------------------------ Path 函数库 ------------------------------------ */
	
	// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetNameExt(str sPath, size_t iSize);
	XXAPI wstr xrtPathGetNameExtW(wstr sPath, size_t iSize);
	
	// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetName(str sPath, size_t iSize);
	XXAPI wstr xrtPathGetNameW(wstr sPath, size_t iSize);
	
	// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetExt(str sPath, size_t iSize);
	XXAPI wstr xrtPathGetExtW(wstr sPath, size_t iSize);
	
	// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathGetDir(str sPath, size_t iSize);
	XXAPI wstr xrtPathGetDirW(wstr sPath, size_t iSize);
	
	// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
	XXAPI int xrtPathIsAbs(str sPath, size_t iSize);
	XXAPI int xrtPathIsAbsW(wstr sPath, size_t iSize);
	
	// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
	XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen);
	XXAPI wstr xrtPathRandomW(wstr sHead, size_t iHeadSize, wstr sFoot, size_t iFootSize, size_t iLen);
	
	// 拼接路径（ 需要使用 xrtFree 释放内存 ）
	XXAPI str xrtPathJoin(uint iCount, ...);
	XXAPI wstr xrtPathJoinW(uint iCount, ...);
	
	
	
	/* ------------------------------------ OS 函数库 ------------------------------------ */
	
	// 运行程序
	XXAPI ptr xrtRun(str sPath, size_t iSize);
	XXAPI ptr xrtRunW(wstr sPath, size_t iSize);
	
	// 打开文件（ 仅支持 windows 系统 ）
	XXAPI ptr xrtStart(str sPath, size_t iSize);
	XXAPI ptr xrtStartW(wstr sPath, size_t iSize);
	
	// 运行程序并等待程序运行结束
	XXAPI int xrtChain(str sPath, size_t iSize);
	XXAPI int xrtChainW(wstr sPath, size_t iSize);
	
	
	
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
	XXAPI xfile xrtOpenW(wstr sPath, int bReadOnly, int iCharset);
	
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
	XXAPI wstr xrtReadW(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入数据 ( iSize 为要写入的字节数 )
	XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize);
	XXAPI size_t xrtWriteW(xfile objFile, wstr sText, size_t iSize);
	
	// 从已打开的文件读取二进制数据 ( 需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtGet(xfile objFile, size_t iSize);
	
	// 向已打开的文件写入二进制数据
	XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize);
	
	// 向文件追加写入数据
	XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset);
	XXAPI int xrtFileAppendW(wstr sPath, wstr sText, size_t iSize, int iCharset);
	
	// 写入并覆盖文件内容
	XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
	XXAPI int xrtFileWriteAllW(wstr sPath, wstr sText, size_t iSize, int iCharset);
	
	// 读取文件的全部内容 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtFileReadAll(str sPath, int iCharset);
	XXAPI wstr xrtFileReadAllW(wstr sPath, int iCharset);
	
	// 写入并覆盖文件内容 ( 二进制 )
	XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize);
	XXAPI int xrtFilePutAllW(wstr sPath, ptr pBuff, size_t iSize);
	
	// 读取文件的全部内容 ( 二进制，需要使用 xrtFree 释放内存 )
	XXAPI ptr xrtFileGetAll(str sPath);
	XXAPI ptr xrtFileGetAllW(wstr sPath);
	
	// 判断路径是否存在
	XXAPI int xrtPathExists(str sPath);
	XXAPI int xrtPathExistsW(wstr sPath);
	
	// 判断文件是否存在
	XXAPI int xrtFileExists(str sPath);
	XXAPI int xrtFileExistsW(wstr sPath);
	
	// 判断目录是否存在
	XXAPI int xrtDirExists(str sPath);
	XXAPI int xrtDirExistsW(wstr sPath);
	
	// 获取文件长度
	XXAPI size_t xrtFileGetSize(str sPath);
	XXAPI size_t xrtFileGetSizeW(wstr sPath);
	
	// 设置文件长度
	XXAPI int xrtFileSetSize(str sPath, size_t iSize);
	XXAPI int xrtFileSetSizeW(wstr sPath, size_t iSize);
	
	// 获取文件属性
	XXAPI int xrtFileGetAttr(str sPath);
	XXAPI int xrtFileGetAttrW(wstr sPath);
	
	// 设置文件属性
	XXAPI int xrtFileSetAttr(str sPath, int iAttr);
	XXAPI int xrtFileSetAttrW(wstr sPath, int iAttr);
	
	// 获取文件最后一次访问时间
	XXAPI int64 xrtFileGetAccessTime(str sPath);
	XXAPI int64 xrtFileGetAccessTimeW(wstr sPath);
	
	// 获取文件修改时间
	XXAPI int64 xrtFileGetChangeTime(str sPath);
	XXAPI int64 xrtFileGetChangeTimeW(wstr sPath);
	
	// 复制文件
	XXAPI int xrtFileCopy(str sSrc, str sDst, int bReWrite);
	XXAPI int xrtFileCopyW(wstr sSrc, wstr sDst, int bReWrite);
	
	// 移动文件（重命名）
	XXAPI int xrtFileMove(str sSrc, str sDst, int bReWrite);
	XXAPI int xrtFileMoveW(wstr sSrc, wstr sDst, int bReWrite);
	
	// 删除文件
	XXAPI int xrtFileDelete(str sPath);
	XXAPI int xrtFileDeleteW(wstr sPath);
	
	// 扫描文件夹
	XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
	XXAPI int xrtDirScanW(wstr sPath, int bRecu, ptr pProc, ptr Param);
	
	// 创建文件夹
	XXAPI int xrtDirCreate(str sPath);
	XXAPI int xrtDirCreateW(wstr sPath);
	
	// 创建多级文件夹
	XXAPI int xrtDirCreateAll(str sPath);
	XXAPI int xrtDirCreateAllW(wstr sPath);
	
	// 复制文件夹
	XXAPI int xrtDirCopy(str sSrc, str sDst, int bReWrite);
	XXAPI int xrtDirCopyW(wstr sSrc, wstr sDst, int bReWrite);
	
	// 移动文件夹
	XXAPI int xrtDirMove(str sSrc, str sDst, int bReWrite);
	XXAPI int xrtDirMoveW(wstr sSrc, wstr sDst, int bReWrite);
	
	// 删除文件夹
	XXAPI int xrtDirDelete(str sPath);
	XXAPI int xrtDirDeleteW(wstr sPath);
	
	
	
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
	
	
	
	/* ------------------------------------ XID 函数库 ------------------------------------ */
	
	// XID 数据结构 ( 192 bit )
	typedef struct {
		int32 Data;				// 自定义数据
		int32 Tick;				// CPU 时钟 ( 低 32 位 )
		xtime Time;				// 当前时间戳
		int32 Addr;				// 本机 IP 地址
		int32 Rand;				// 随机数
	} xid_struct, *xid;
	
	// XID 转 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtXIDtoStr(xid pXID);
	
	// 获取 XID ( 需要使用 xrtFree 释放内存 )
	XXAPI xid xrtMakeXID(int32 iData);
	
	// 获取 XID 字符串 ( 需要使用 xrtFree 释放内存 )
	XXAPI str xrtMakeXIDS(int32 iData);
	
	// 比较两个 XID 是否相同
	XXAPI int xrtCompXID(xid pXID1, xid pXID2);
	
	
	
#endif


