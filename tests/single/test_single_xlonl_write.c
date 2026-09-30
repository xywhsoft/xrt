#define XRT_MODULE_XLONL_WRITE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(XRT_FEATURE_XLONL_READ) || defined(XRT_FEATURE_XLONL_FILE)
#error "write-only selection pulled in read/file"
#endif

#include "../xlonl/test_xlonl_write.c"
