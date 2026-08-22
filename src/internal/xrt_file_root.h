#ifndef XRT_INTERNAL_FILE_ROOT_H
#define XRT_INTERNAL_FILE_ROOT_H

#include "xrt_file_link.h"



#if defined(XRT_FEATURE_FILE_ROOT)

#if defined(_WIN32) || defined(_WIN64)
	typedef HANDLE xrootnative;
	#define XRT_ROOT_NATIVE_INVALID INVALID_HANDLE_VALUE
#else
	typedef int xrootnative;
	#define XRT_ROOT_NATIVE_INVALID (-1)
#endif



/* 平台操作可成功、发现需要解析的链接或失败。 */
typedef enum xrootstep {
	XROOT_STEP_ERROR = -1,
	XROOT_STEP_DONE = 0,
	XROOT_STEP_LINK = 1
} xrootstep;



/* 根对象只保存诊断路径和锚定目录的原生句柄。 */
struct xroot_impl {
	str Path;
	xrootnative Handle;
};



/* 设置带系统代码的目录根错误。 */
void __xrtRootSetError(xrooterror Code, cstr sOperation,
	cstr sMessage, int iSystemCode);



/* 设置不带系统代码的目录根错误。 */
void __xrtRootError(xerrkind Kind, xrooterror Code,
	cstr sOperation, cstr sMessage);



/* 把当前错误保留为原因并改写为目录根错误。 */
void __xrtRootWrapError(xrooterror Code,
	cstr sOperation, cstr sMessage);



/* 打开并验证初始根目录。 */
bool __xrtRootNativeOpen(cstr sPath, xrootnative* pHandle);



/* 关闭根目录或遍历过程中临时持有的目录句柄。 */
bool __xrtRootNativeClose(xrootnative Handle, bool bReport);



/* 不跟随当前分量打开目录；链接目标通过拥有字符串返回。 */
xrootstep __xrtRootNativeOpenDir(xrootnative Parent, cstr sName,
	xrootnative* pHandle, str* pLink);



/* 不跟随当前分量打开普通文件，并按选项决定是否继续解析链接。 */
xrootstep __xrtRootNativeOpenFile(xrootnative Parent, cstr sName,
	const xfileoptions* pOptions, xfile* pFile, str* pLink);



/* 查询当前分量元数据，并按参数决定是否继续解析链接。 */
xrootstep __xrtRootNativeStat(xrootnative Parent, cstr sName,
	bool bFollowLink, xfileinfo* pInfo, str* pLink);



/* 相对父目录创建一个新目录。 */
bool __xrtRootNativeCreateDir(xrootnative Parent,
	cstr sName, uint32 iMode);



/* 相对父目录删除非目录对象或空目录，并可要求末级必须是目录。 */
bool __xrtRootNativeRemove(xrootnative Parent,
	cstr sName, bool bDirectoryOnly);



/* 相对父目录读取末级链接目标。 */
str __xrtRootNativeReadLink(xrootnative Parent, cstr sName);



/* 相对父目录创建符号链接。 */
bool __xrtRootNativeLinkCreate(xrootnative Parent,
	cstr sName, cstr sTarget, bool bDirectory);



/* 在两个已锚定父目录之间创建硬链接。 */
bool __xrtRootNativeLinkHard(xrootnative SourceParent, cstr sSource,
	xrootnative TargetParent, cstr sTarget);



/* 相对父目录创建 FIFO。 */
bool __xrtRootNativeFifoCreate(xrootnative Parent,
	cstr sName, uint32 iMode);



/* 相对父目录设置权限，并按策略返回需要继续解析的链接。 */
xrootstep __xrtRootNativeSetMode(xrootnative Parent,
	cstr sName, bool bFollowLink, uint32 iMode, str* pLink);

#endif

#endif
