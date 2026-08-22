#ifndef XRT_INTERNAL_FILE_LINK_H
#define XRT_INTERNAL_FILE_LINK_H

#include "xrt_file.h"



#if defined(XRT_FEATURE_FILE_LINK)

#if defined(_WIN32) || defined(_WIN64)

/* 从重解析点句柄读取目标；解析模式使用真实 substitute 名称。 */
str __xrtLinkWindowsReadHandle(HANDLE hLink, bool bResolve);

#else

/* 相对目录描述符读取符号链接目标。 */
str __xrtLinkReadAt(int hDirectory, cstr sLink);

#endif

#endif

#endif
