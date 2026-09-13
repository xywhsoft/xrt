#define XRT_MODULE_JSONL_READ
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_JSONL_WRITE) || defined(XRT_FEATURE_JSONL_FILE)
#error "read-only selection pulled in write/file"
#endif

#include "../jsonl/test_jsonl_read.c"

