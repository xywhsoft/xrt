#ifndef XRT_INTERNAL_DIR_H
#define XRT_INTERNAL_DIR_H

#include "xrt_file.h"



#if defined(XRT_FEATURE_DIR)

/* 设置带系统代码的目录错误。 */
void __xrtDirSetError(xdirerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode);



/* 设置不带系统代码的目录错误。 */
void __xrtDirError(xerrkind Kind, xdirerror Code,
	cstr sOperation, cstr sMessage);

#endif

#endif
