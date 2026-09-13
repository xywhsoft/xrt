#ifndef XRT_INTERNAL_XSONL_H
#define XRT_INTERNAL_XSONL_H

#include "xrt_text_lines.h"
#include "xrt_xson.h"

#if defined(XRT_FEATURE_XSONL_CORE)
extern const xtextlinesformat __xrtXsonlFormat;
#endif

#if defined(XRT_FEATURE_XSONL_READ)
bool __xrtXsonlReadConfigValid(const xxsonlreadconfig* pConfig);
#endif

#if defined(XRT_FEATURE_XSONL_WRITE)
str __xrtXsonlStringify(const xvalue* pArray, const xxsonlwriteconfig* pConfig, size_t* pSize);
#endif

#endif

