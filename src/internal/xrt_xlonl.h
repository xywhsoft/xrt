#ifndef XRT_INTERNAL_XLONL_H
#define XRT_INTERNAL_XLONL_H

#include "xrt_text_lines.h"
#include "xrt_xlon.h"

#if defined(XRT_FEATURE_XLONL_CORE)
extern const xtextlinesformat __xrtXlonlFormat;
#endif

#if defined(XRT_FEATURE_XLONL_READ)
bool __xrtXlonlReadConfigValid(const xxlonlreadconfig* pConfig);
#endif

#if defined(XRT_FEATURE_XLONL_WRITE)
str __xrtXlonlStringify(const xvalue* pArray, const xxlonlwriteconfig* pConfig, size_t* pSize);
#endif

#endif
