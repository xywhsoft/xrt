#ifndef XRT_INTERNAL_FILE_H
#define XRT_INTERNAL_FILE_H

#include "xrt_path.h"
#include "xrt_time.h"

#if defined(XRT_FEATURE_NET_FILE)
	#include "xrt_atomic.h"
#endif



#if defined(XRT_FEATURE_FILE)

/* 检查文件打开选项，并把空选项展开为稳定默认值。 */
bool __xrtFileOptions(const xfileoptions* pInput, xfileoptions* pOptions);



/* 接管原生句柄并创建文件对象；无论成功失败，调用后句柄都归本函数处理。 */
xfile __xrtFileTakeNative(intptr_t iHandle, uint32 iFlags);



/* 接管数据句柄和可选控制句柄；两个句柄都在失败或关闭时释放。 */
xfile __xrtFileTakeNativePair(intptr_t iHandle,
	intptr_t iControl, uint32 iFlags);



/* 返回文件控制操作使用的原生句柄，普通文件与数据句柄相同。 */
intptr_t __xrtFileControlNative(xfile File);



#if defined(XRT_FEATURE_NET_FILE)
/* 把异步文件永久绑定到首个完成端口，并返回共享关联状态。 */
bool __xrtFileAsyncBind(
	xfile File,
	uint64 iOwner,
	bool** ppAssociated
);
#endif



/* 设置带系统错误码的文件错误。 */
void __xrtFileSetError(xfileerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode);



/* 设置带显式错误类别和系统错误码的文件错误。 */
void __xrtFileSetKindError(xerrkind Kind, xfileerror Code,
	cstr sOperation, cstr sMessage, int iSystemCode);



/* 设置不带系统错误码的文件错误。 */
void __xrtFileError(xerrkind Kind, xfileerror Code,
	cstr sOperation, cstr sMessage);



/* 查询可选路径；不存在是成功状态并通过 pExists 返回。 */
bool __xrtFilePathInfo(cstr sPath, bool bFollowLink,
	bool* pExists, xfileinfo* pInfo);



/* 判断两份元数据是否明确指向同一个文件系统对象。 */
bool __xrtFileInfoSame(const xfileinfo* pLeft, const xfileinfo* pRight);



#if defined(XRT_FEATURE_FILE_WHOLE)

/* 流式复制普通文件；bFollowSource 仅供已明确授权跟随链接的组合功能使用。 */
bool __xrtFileCopy(cstr sSource, cstr sTarget,
	bool bReplace, bool bFollowSource);



/* 判断当前文件错误是否明确表示跨卷改名。 */
bool __xrtFileCrossDevice(void);

#endif



#if defined(_WIN32) || defined(_WIN64)

/* 把公共标志转换为 Windows 文件访问模式。 */
DWORD __xrtFileWindowsAccess(uint32 iFlags);



/* 把公共标志转换为 Windows 创建方式。 */
DWORD __xrtFileWindowsDisposition(uint32 iFlags);



/* 为追加文件创建受限数据句柄，并保留原句柄执行控制操作。 */
bool __xrtFileWindowsAppendHandles(HANDLE* pHandle,
	HANDLE* pControl, uint32 iFlags);



/* 查询 Windows 原生句柄并转换为稳定元数据。 */
bool __xrtFileWindowsStat(HANDLE hFile, xfileinfo* pInfo, bool bReport);



/* 把 Windows FILETIME 转换为 Unix Epoch 微秒。 */
xtime __xrtFileWindowsTime(FILETIME Time);



/* 把 Windows 枚举条目已有信息转换为稳定元数据。 */
void __xrtFileWindowsFindInfo(const WIN32_FIND_DATAW* pNative,
	xfileinfo* pInfo);

#else

struct stat;



/* 把公共标志转换为 POSIX open 标志。 */
int __xrtFilePosixFlags(uint32 iFlags);



/* 相对目录描述符打开默认不可继承的 POSIX 描述符。 */
int __xrtFilePosixOpenAt(int hDirectory, cstr sPath,
	int iFlags, uint32 iMode);



/* 把 POSIX stat 转换为稳定元数据。 */
bool __xrtFilePosixInfo(const struct stat* pNative, xfileinfo* pInfo);

#endif

#endif

#endif
