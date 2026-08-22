#ifndef XRT_INTERNAL_TEMP_H
#define XRT_INTERNAL_TEMP_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_TEMP_MEMORY)
/* 为协程或任务切换当前绑定的临时 arena。 */
xtemparena* __xrtTempContextSwap(xtemparena* pArena);
#endif

#endif
