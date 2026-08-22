#ifndef XRT_STRING_H
#define XRT_STRING_H

#include <stdarg.h>

#include <xrt/core.h>



#if (defined(XRT_FEATURE_STRING_SPLIT) || defined(XRT_FEATURE_STRING_FORMAT) || \
	defined(XRT_FEATURE_STRING_GLOB)) && !defined(XRT_FEATURE_STRING)
	#error "XRT string extension features require XRT_FEATURE_STRING"
#endif

#if defined(XRT_FEATURE_STRING_GLOB) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT string glob requires XRT_FEATURE_UNICODE"
#endif



#if defined(XRT_FEATURE_STRING)
/* 字符串体系的稳定错误代码。 */
typedef enum xstrerror {
	XSTR_ERROR_FORMAT = 1,
	XSTR_ERROR_PATTERN
} xstrerror;



/* 字符串构建器始终在 Data[Size] 保留一个零结尾。 */
typedef struct xstrbuf {
	str Data;
	size_t Size;
	size_t Capacity;
} xstrbuf;



XRT_EXTERN_C_BEGIN



/* 从零结尾字符串创建借用视图，空指针视为空字符串。 */
XRT_API xstrview xrtStrView(cstr sText);



/* 从明确长度创建借用视图。 */
XRT_API xstrview xrtStrViewN(cstr sText, size_t iSize);



/* 判断字符串视图是否为空。 */
XRT_API bool xrtStrEmpty(xstrview Text);



/* 按无符号字节进行词典序比较。 */
XRT_API int xrtStrCompare(xstrview Left, xstrview Right);



/* 按 ASCII 大小写不敏感规则进行词典序比较。 */
XRT_API int xrtStrCaseCompare(xstrview Left, xstrview Right);



/* 判断两个字符串视图是否完全相等。 */
XRT_API bool xrtStrEqual(xstrview Left, xstrview Right);



/* 按 ASCII 大小写不敏感规则判断相等。 */
XRT_API bool xrtStrCaseEqual(xstrview Left, xstrview Right);



/* 从指定字节位置查找子串，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtStrFind(xstrview Text, xstrview Part, size_t iStart);



/* 按 ASCII 大小写不敏感规则查找子串。 */
XRT_API size_t xrtStrCaseFind(xstrview Text, xstrview Part, size_t iStart);



/* 从指定字节位置查找单个字节。 */
XRT_API size_t xrtStrFindByte(xstrview Text, unsigned char iByte, size_t iStart);



/* 从指定字节位置查找属于集合的首个字节。 */
XRT_API size_t xrtStrFindAny(xstrview Text, xstrview Set, size_t iStart);



/* 从右侧查找最后一个子串，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtStrRFind(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则从右侧查找最后一个子串。 */
XRT_API size_t xrtStrCaseRFind(xstrview Text, xstrview Part);



/* 统计不重叠子串数量，空子串返回零。 */
XRT_API size_t xrtStrCount(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则统计不重叠子串数量。 */
XRT_API size_t xrtStrCaseCount(xstrview Text, xstrview Part);



/* 判断字符串是否包含指定子串。 */
XRT_API bool xrtStrContains(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则判断是否包含子串。 */
XRT_API bool xrtStrCaseContains(xstrview Text, xstrview Part);



/* 判断字符串是否包含集合中的任意字节。 */
XRT_API bool xrtStrContainsAny(xstrview Text, xstrview Set);



/* 判断字符串是否以指定子串开始。 */
XRT_API bool xrtStrStarts(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则判断是否以子串开始。 */
XRT_API bool xrtStrCaseStarts(xstrview Text, xstrview Part);



/* 判断字符串是否以指定子串结束。 */
XRT_API bool xrtStrEnds(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则判断是否以子串结束。 */
XRT_API bool xrtStrCaseEnds(xstrview Text, xstrview Part);



/* 围绕首个分隔符切分借用视图，未找到时 Before 返回完整输入。 */
XRT_API bool xrtStrCut(xstrview Text, xstrview Separator,
	xstrview* pBefore, xstrview* pAfter);



/* 围绕最后一个分隔符切分借用视图，未找到时 Before 返回完整输入。 */
XRT_API bool xrtStrRCut(xstrview Text, xstrview Separator,
	xstrview* pBefore, xstrview* pAfter);



/* 删除匹配的前缀并返回剩余借用视图。 */
XRT_API bool xrtStrCutPrefix(xstrview Text, xstrview Prefix, xstrview* pRest);



/* 删除匹配的后缀并返回剩余借用视图。 */
XRT_API bool xrtStrCutSuffix(xstrview Text, xstrview Suffix, xstrview* pRest);



/* 按字节截取借用视图，范围会钳制到源字符串。 */
XRT_API xstrview xrtStrSlice(xstrview Text, size_t iStart, size_t iCount);



/* 删除左侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrimLeft(xstrview Text);



/* 删除右侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrimRight(xstrview Text);



/* 删除两侧 ASCII 空白并返回借用视图。 */
XRT_API xstrview xrtStrTrim(xstrview Text);



/* 删除左侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimLeftSet(xstrview Text, xstrview Set);



/* 删除右侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimRightSet(xstrview Text, xstrview Set);



/* 删除两侧属于指定字节集合的内容。 */
XRT_API xstrview xrtStrTrimSet(xstrview Text, xstrview Set);



/* 判断字符串是否只包含 ASCII 空白。 */
XRT_API bool xrtStrBlank(xstrview Text);



/* 复制零结尾字符串，返回值始终由 xrtFree 释放。 */
XRT_API str xrtStrDup(cstr sText);



/* 复制明确长度字符串并追加零结尾。 */
XRT_API str xrtStrDupN(cstr sText, size_t iSize);



/* 复制字符串视图并追加零结尾。 */
XRT_API str xrtStrDupView(xstrview Text);



/* 连接两个字符串视图。 */
XRT_API str xrtStrConcat(xstrview Left, xstrview Right);



/* 使用分隔符连接一组字符串视图。 */
XRT_API str xrtStrJoin(xstrview Separator, const xstrview* arrText, size_t iCount);



/* 重复字符串指定次数。 */
XRT_API str xrtStrRepeat(xstrview Text, size_t iCount);



/* 替换所有不重叠子串。 */
XRT_API str xrtStrReplace(xstrview Text, xstrview Part, xstrview Replacement);



/* 按字节位置插入子串。 */
XRT_API str xrtStrInsert(xstrview Text, size_t iPosition, xstrview Part);



/* 按字节范围删除内容。 */
XRT_API str xrtStrRemove(xstrview Text, size_t iPosition, size_t iCount);



/* 按字节反转到调用方缓冲区并补零；允许输入和输出起点相同。 */
XRT_API bool xrtStrReverseBytesTo(xstrview Text, char* sOutput, size_t iCapacity);



/* 按字节反转字符串。 */
XRT_API str xrtStrReverseBytes(xstrview Text);



/* 把 ASCII 字母转换为小写并写入调用方缓冲区；允许原地转换。 */
XRT_API bool xrtStrLowerTo(xstrview Text, char* sOutput, size_t iCapacity);



/* 复制字符串并把 ASCII 字母转换为小写。 */
XRT_API str xrtStrLower(xstrview Text);



/* 把 ASCII 字母转换为大写并写入调用方缓冲区；允许原地转换。 */
XRT_API bool xrtStrUpperTo(xstrview Text, char* sOutput, size_t iCapacity);



/* 复制字符串并把 ASCII 字母转换为大写。 */
XRT_API str xrtStrUpper(xstrview Text);



/*
	删除集合中的全部字节并写入调用方缓冲区。
	输出为空且容量为零时只查询所需长度；允许输入和输出起点相同。
*/
XRT_API bool xrtStrFilterTo(xstrview Text, xstrview Set,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);



/* 删除集合中的全部字节并创建独立字符串。 */
XRT_API str xrtStrFilter(xstrview Text, xstrview Set);



/* 按字节宽度在左侧重复填充字符串。 */
XRT_API str xrtStrPadLeft(xstrview Text, size_t iWidth, xstrview Fill);



/* 按字节宽度在右侧重复填充字符串。 */
XRT_API str xrtStrPadRight(xstrview Text, size_t iWidth, xstrview Fill);



/* 按字节宽度在两侧重复填充字符串。 */
XRT_API str xrtStrPadCenter(xstrview Text, size_t iWidth, xstrview Fill);



/* 初始化空字符串构建器。 */
XRT_API void xrtStrBufInit(xstrbuf* pBuffer);



/* 释放字符串构建器持有的内存。 */
XRT_API void xrtStrBufFree(xstrbuf* pBuffer);



/* 清空字符串构建器但保留容量。 */
XRT_API void xrtStrBufClear(xstrbuf* pBuffer);



/* 返回字符串构建器当前内容的借用视图。 */
XRT_API xstrview xrtStrBufView(const xstrbuf* pBuffer);



/* 检查字符串构建器的公开状态是否自洽。 */
XRT_API bool xrtStrBufValid(const xstrbuf* pBuffer);



/* 检查视图是否借用构建器当前内容，并返回原始字节偏移。 */
XRT_API bool xrtStrBufAlias(
	const xstrbuf* pBuffer,
	xstrview Text,
	bool* pAlias,
	size_t* pOffset
);



/* 保证字符串构建器至少具有指定数据容量。 */
XRT_API bool xrtStrBufReserve(xstrbuf* pBuffer, size_t iCapacity);



/* 调整字符串构建器长度，扩展区域填零。 */
XRT_API bool xrtStrBufResize(xstrbuf* pBuffer, size_t iSize);



/* 追加字符串视图，允许追加自身的有效子视图。 */
XRT_API bool xrtStrBufAppend(xstrbuf* pBuffer, xstrview Text);



/* 追加一个字节。 */
XRT_API bool xrtStrBufAppendByte(xstrbuf* pBuffer, char iByte);



/* 重复追加字符串视图。 */
XRT_API bool xrtStrBufAppendRepeat(xstrbuf* pBuffer, xstrview Text, size_t iCount);



/* 取走构建器内存并把构建器重置为空。 */
XRT_API str xrtStrBufTake(xstrbuf* pBuffer);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_STRING_SPLIT)

/* 通用字符串拆分迭代器不分配内存。 */
typedef struct xstrsplit {
	xstrview Text;
	xstrview Separator;
	size_t Position;
	uint32 State;
	bool Done;
} xstrsplit;



/* 行迭代器同时识别 LF、CRLF 和 CR。 */
typedef struct xstrlines {
	xstrview Text;
	size_t Position;
	uint32 State;
	bool Done;
} xstrlines;



/* 字段迭代器跳过连续 ASCII 空白且不返回空字段。 */
typedef struct xstrfields {
	xstrview Text;
	size_t Position;
	uint32 State;
	bool Done;
} xstrfields;



/* 便捷拆分结果在一个分配块内保存视图和零结尾副本。 */
typedef struct xstrlist {
	size_t Count;
	xstrview* Items;
	size_t DataSize;
} xstrlist;



XRT_EXTERN_C_BEGIN



/* 初始化不分配内存的字符串拆分迭代器。 */
XRT_API bool xrtStrSplitInit(xstrsplit* pSplit, xstrview Text, xstrview Separator);



/* 返回下一个借用片段，结束时返回 false。 */
XRT_API bool xrtStrSplitNext(xstrsplit* pSplit, xstrview* pItem);



/* 初始化不分配内存的行迭代器。 */
XRT_API bool xrtStrLinesInit(xstrlines* pLines, xstrview Text);



/* 返回下一行借用视图，结束时返回 false。 */
XRT_API bool xrtStrLinesNext(xstrlines* pLines, xstrview* pLine);



/* 初始化按连续 ASCII 空白拆分的零分配字段迭代器。 */
XRT_API bool xrtStrFieldsInit(xstrfields* pFields, xstrview Text);



/* 返回下一个非空借用字段，结束时返回 false。 */
XRT_API bool xrtStrFieldsNext(xstrfields* pFields, xstrview* pField);



/* 为指定片段数量和零结尾数据容量分配单块字符串列表。 */
XRT_API xstrlist* xrtStrListAlloc(size_t iCount, size_t iDataSize);



/* 将片段复制到列表数据区并推进字节偏移。 */
XRT_API bool xrtStrListWrite(
	xstrlist* pList,
	size_t iIndex,
	xstrview Item,
	size_t* pOffset
);



/* 一次性拆分字符串并返回独立的零结尾片段。 */
XRT_API xstrlist* xrtStrSplit(xstrview Text, xstrview Separator);



/* 一次性按行拆分字符串。 */
XRT_API xstrlist* xrtStrSplitLines(xstrview Text);



/* 一次性按连续 ASCII 空白拆分字符串。 */
XRT_API xstrlist* xrtStrFields(xstrview Text);



/* 释放便捷拆分结果。 */
XRT_API void xrtStrListFree(xstrlist* pList);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_STRING_GLOB)

/* 通配匹配可以选择只对 ASCII 字母忽略大小写。 */
typedef enum xstrglobflag {
	XSTR_GLOB_CASE_ASCII = 0x01
} xstrglobflag;



XRT_EXTERN_C_BEGIN



/* 使用严格 UTF-8 通配模式匹配完整字符串。 */
XRT_API bool xrtStrGlob(xstrview Text, xstrview Pattern, uint32 iFlags);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_STRING_FORMAT)

XRT_EXTERN_C_BEGIN



/* 使用 printf 规则创建字符串。 */
XRT_API str xrtFormat(cstr sFormat, ...);



/* 使用 printf 规则和已有参数列表创建字符串。 */
XRT_API str xrtFormatV(cstr sFormat, va_list Args);



/* 使用 printf 规则直接追加到构建器。 */
XRT_API bool xrtStrBufAppendFormat(xstrbuf* pBuffer, cstr sFormat, ...);



/* 使用 printf 规则和已有参数列表直接追加到构建器。 */
XRT_API bool xrtStrBufAppendFormatV(xstrbuf* pBuffer, cstr sFormat, va_list Args);



XRT_EXTERN_C_END

#endif

#endif
