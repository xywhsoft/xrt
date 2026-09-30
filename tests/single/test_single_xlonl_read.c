#define XRT_MODULE_XLONL_READ
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_XLONL_WRITE) || defined(XRT_FEATURE_XLONL_FILE)
#error "read-only selection pulled in write/file"
#endif

#include "../xlonl/test_xlonl_read.c"
