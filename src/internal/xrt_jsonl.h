#ifndef XRT_INTERNAL_JSONL_H
#define XRT_INTERNAL_JSONL_H

#include "xrt_text_lines.h"
#include "xrt_json.h"

#if defined(XRT_FEATURE_JSONL_CORE)
extern const xtextlinesformat __xrtJsonlFormat;
#endif

#if defined(XRT_FEATURE_JSONL_READ)
bool __xrtJsonlReadConfigValid(const xjsonlreadconfig* pConfig);
#endif

#if defined(XRT_FEATURE_JSONL_WRITE)
str __xrtJsonlStringify(const xvalue* pArray, const xjsonlwriteconfig* pConfig, size_t* pSize);
#endif

#endif

