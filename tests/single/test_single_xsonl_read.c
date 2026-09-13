#define XRT_MODULE_XSONL_READ
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_XSONL_WRITE) || defined(XRT_FEATURE_XSONL_FILE)
#error "read-only selection pulled in write/file"
#endif

#include "../xsonl/test_xsonl_read.c"

