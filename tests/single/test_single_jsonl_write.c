#define XRT_MODULE_JSONL_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_JSONL_READ) || defined(XRT_FEATURE_JSONL_FILE)
#error "write-only selection pulled in read/file"
#endif

#include "../jsonl/test_jsonl_write.c"

