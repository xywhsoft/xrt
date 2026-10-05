#define XRUNTIME_MODULE_TYPED_ARRAY
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"
#include "../runtime/test_typed_array_clear.c"
