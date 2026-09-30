#ifndef XRT_INTERNAL_DIR_H
#define XRT_INTERNAL_DIR_H

#include "xrt_file.h"



#if defined(XRT_FEATURE_DIR)

#define XRT_DIR_BACKEND_VERSION 1u

/* Next 只在 ITEM 时修改 pEntry；Close 消费 State 且恰好调用一次。 */
typedef struct xrt_dir_backend_ops {
	uint32 Size;
	uint32 Version;
	bool VirtualPath;
	uint8 Reserved[3];
	xdirnext (*Next)(ptr pState, cstr sPath,
		uint32 iFlags, xdirentry* pEntry);
	bool (*Close)(ptr pState);
} xrt_dir_backend_ops;



/* 接管后端 State。失败时也调用一次 Ops.Close。 */
xdir __xrtDirTakeBackend(const xrt_dir_backend_ops* pOps,
	ptr pState, cstr sPath, uint32 iFlags);



/* 验证目录枚举标志，供原生目录和 VFS 共用。 */
bool __xrtDirFlagsValid(uint32 iFlags);

/* 设置带系统代码的目录错误。 */
void __xrtDirSetError(xdirerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode);



/* 设置不带系统代码的目录错误。 */
void __xrtDirError(xerrkind Kind, xdirerror Code,
	cstr sOperation, cstr sMessage);

#endif

#endif
