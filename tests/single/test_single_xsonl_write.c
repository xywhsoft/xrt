#define XRT_MODULE_XSONL_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_XSONL_READ) || defined(XRT_FEATURE_XSONL_FILE)
#error "write-only selection pulled in read/file"
#endif

#include "../xsonl/test_xsonl_write.c"

