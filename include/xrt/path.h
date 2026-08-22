#ifndef XRT_PATH_H
#define XRT_PATH_H

#include <xrt/string.h>



#if defined(XRT_FEATURE_PATH) && !defined(XRT_FEATURE_STRING)
	#error "XRT path support requires XRT_FEATURE_STRING"
#endif

#if (defined(XRT_FEATURE_PATH_SYSTEM) || defined(XRT_FEATURE_PATH_SAFE)) && \
	!defined(XRT_FEATURE_PATH)
	#error "XRT path system and safe features require XRT_FEATURE_PATH"
#endif

#if (defined(XRT_FEATURE_PATH_SYSTEM) || defined(XRT_FEATURE_PATH_SAFE)) && \
	!defined(XRT_FEATURE_UNICODE)
	#error "XRT path system and safe features require XRT_FEATURE_UNICODE"
#endif



/* 路径风格决定根、分隔符和绝对路径语义，不读取目标文件系统。 */
typedef enum xpathstyle {
	XPATH_NATIVE = 0,
	XPATH_POSIX,
	XPATH_WINDOWS
} xpathstyle;



/* 根类型明确区分 Windows 驱动器相对路径、根相对路径和完整绝对路径。 */
typedef enum xpathroot {
	XPATH_ROOT_NONE = 0,
	XPATH_ROOT_POSIX,
	XPATH_ROOT_WINDOWS,
	XPATH_ROOT_DRIVE_RELATIVE,
	XPATH_ROOT_DRIVE,
	XPATH_ROOT_UNC,
	XPATH_ROOT_DEVICE
} xpathroot;



/* 路径分解标志。 */
typedef enum xpathflag {
	XPATH_FLAG_ROOTED = 0x01,
	XPATH_FLAG_ABSOLUTE = 0x02,
	XPATH_FLAG_TRAILING_SEPARATOR = 0x04
} xpathflag;



/* 全部字段都借用输入路径；Ext 包含前导点，隐藏文件名本身不算扩展名。 */
typedef struct xpathparts {
	xstrview Root;
	xstrview Parent;
	xstrview Name;
	xstrview Stem;
	xstrview Ext;
	xpathroot RootKind;
	uint32 Flags;
} xpathparts;



/* 路径组件类型；根、点、双点和普通名称保持明确语义。 */
typedef enum xpathcomponentkind {
	XPATH_COMPONENT_ROOT = 1,
	XPATH_COMPONENT_CURRENT,
	XPATH_COMPONENT_PARENT,
	XPATH_COMPONENT_NORMAL
} xpathcomponentkind;



/* 路径组件借用输入文本。 */
typedef struct xpathcomponent {
	xstrview Text;
	xpathcomponentkind Kind;
} xpathcomponent;



/* 零分配路径组件迭代器；字段仅由路径 API 维护。 */
typedef struct xpathiter {
	xstrview Path;
	size_t Position;
	size_t RootSize;
	xpathstyle Style;
	uint32 State;
} xpathiter;



/* 路径模块稳定错误代码。 */
typedef enum xpatherror {
	XPATH_ERROR_FORMAT = 1,
	XPATH_ERROR_OVERFLOW,
	XPATH_ERROR_ROOT,
	XPATH_ERROR_SYSTEM
} xpatherror;



#if defined(XRT_FEATURE_PATH_SAFE)

/* 流式可移植路径段检查器使用固定存储，不分配内存。 */
#define XPATH_SAFE_SEGMENT_STORAGE_SIZE 40u



/* 固定存储只允许通过 Path Safe Segment API 访问。 */
typedef union xpathsafesegment {
	uint64 Alignment;
	uint8 Storage[XPATH_SAFE_SEGMENT_STORAGE_SIZE];
} xpathsafesegment;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_PATH)

/* 按指定风格零分配地分解路径，成功后所有视图都借用输入。 */
XRT_API bool xrtPathParse(xstrview Path, xpathstyle Style, xpathparts* pParts);



/* 初始化零分配路径组件迭代器，成功后迭代器借用输入。 */
XRT_API bool xrtPathIterInit(xpathiter* pIterator,
	xstrview Path, xpathstyle Style);



/* 返回下一个借用组件，遍历结束时返回 false。 */
XRT_API bool xrtPathNext(xpathiter* pIterator, xpathcomponent* pComponent);



/* 返回本机路径分隔符。 */
XRT_API char xrtPathSep(void);



/* 返回本机路径列表分隔符，Windows 为分号，POSIX 为冒号。 */
XRT_API char xrtPathListSep(void);



/* 复制本机路径的末级名称，包含扩展名。 */
XRT_API str xrtPathName(cstr sPath);



/* 复制本机路径的末级名称，不包含最后一个扩展名。 */
XRT_API str xrtPathStem(cstr sPath);



/* 复制本机路径的最后一个扩展名，结果包含前导点。 */
XRT_API str xrtPathExt(cstr sPath);



/* 复制本机路径的父路径；没有父路径时返回已分配的空字符串。 */
XRT_API str xrtPathParent(cstr sPath);



/* 判断本机路径是否完整绝对；Windows 驱动器相对和根相对路径返回 false。 */
XRT_API bool xrtPathIsAbs(cstr sPath);



/* 判断本机路径词法上是否恰好为一个完整文件系统根。 */
XRT_API bool xrtPathIsRoot(cstr sPath);



/* 判断本机路径是否带根；Windows 的 C:foo 和 \foo 也属于带根路径。 */
XRT_API bool xrtPathIsRooted(cstr sPath);



/* 判断路径是否能被安全拼入任意基目录；只做词法检查，不解析符号链接。 */
XRT_API bool xrtPathIsLocal(xstrview Path, xpathstyle Style);



/* 按本机风格拼接两个路径；Windows 根相对右项保留已有卷前缀。 */
XRT_API str xrtPathJoin(cstr sLeft, cstr sRight);



/* 按指定风格拼接并清理；Windows 根相对项保留已有卷前缀。 */
XRT_API str xrtPathBuild(const xstrview* arrParts, size_t iCount, xpathstyle Style);



/* 纯词法清理分隔符、点和双点段，不访问文件系统或解析符号链接。 */
XRT_API str xrtPathClean(xstrview Path, xpathstyle Style);



/* 纯词法计算从目录 Base 到 Target 的相对路径；根不同时报错。 */
XRT_API str xrtPathRelative(xstrview Base, xstrview Target, xpathstyle Style);



/* 替换本机路径的末级名称；新名称可以包含相对路径段。 */
XRT_API str xrtPathWithName(cstr sPath, cstr sName);



/* 替换最后一个扩展名；空扩展名删除扩展名，非空值可省略前导点。 */
XRT_API str xrtPathWithExt(cstr sPath, cstr sExtension);

#endif



#if defined(XRT_FEATURE_PATH_SYSTEM)

/* 返回当前工作目录的绝对 UTF-8 路径。 */
XRT_API str xrtPathCwd(void);



/* 修改进程当前工作目录；该操作影响进程内其他线程。 */
XRT_API bool xrtPathSetCwd(cstr sPath);



/* 使用操作系统规则返回绝对路径；空路径表示当前工作目录。 */
XRT_API str xrtPathAbs(cstr sPath);



/* 返回已存在路径跟随符号链接后的物理绝对路径。 */
XRT_API str xrtPathReal(cstr sPath);



/* 把两个路径转为绝对路径后计算从 Base 到 Target 的相对路径。 */
XRT_API str xrtPathRel(cstr sBase, cstr sTarget);



/* 返回当前用户主目录。 */
XRT_API str xrtPathHome(void);



/* 返回操作系统临时目录。 */
XRT_API str xrtPathTemp(void);



/* 返回当前可执行文件的绝对 UTF-8 路径。 */
XRT_API str xrtPathExecutable(void);



/* 返回当前可执行文件所在目录。 */
XRT_API str xrtPathAppDir(void);

#endif



#if defined(XRT_FEATURE_PATH_SAFE)

/* 初始化一个可跨任意输入分块复用的可移植路径段检查器。 */
XRT_API void xrtPathSafeSegmentInit(xpathsafesegment* pState);



/* 加入一个已解码字节；一旦确定非法便返回 false。 */
XRT_API bool xrtPathSafeSegmentFeed(
	xpathsafesegment* pState,
	uint8 iValue
);



/* 完成空段、点段、尾部规则和 Windows 设备保留名检查。 */
XRT_API bool xrtPathSafeSegmentFinish(
	const xpathsafesegment* pState
);



/* 检查归档条目是否为跨 Windows/POSIX 可移植的 UTF-8 相对路径。 */
XRT_API bool xrtPathIsSafeEntry(xstrview Path, bool bDirectory);

#endif



XRT_EXTERN_C_END

#endif
