#ifndef XRT_INTERNAL_VFS_H
#define XRT_INTERNAL_VFS_H

#include "xrt_dir.h"
#include "xrt_sync.h"

#include <xrt/vfs.h>



#if defined(XRT_FEATURE_VFS)

/* 验证规范虚拟绝对路径；成功后返回不含零结尾的借用视图。 */
bool __xrtVfsPath(cstr sPath, xstrview* pPath);

/* 验证 provider 使用的规范相对路径。 */
bool __xrtVfsRelativePath(xstrview Path, bool bAllowEmpty);



/* 设置 xrt.vfs 域的稳定错误。 */
void __xrtVfsError(xerrkind Kind, xvfserror Code,
	cstr sOperation, cstr sMessage);

#endif

#endif
