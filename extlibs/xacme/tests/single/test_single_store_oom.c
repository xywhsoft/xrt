#ifdef XACME_MODULE_XACME
#undef XACME_MODULE_XACME
#endif
#define XACME_MODULE_ACME_STORE
#define XRT_MODULE_MEMORY_DEBUG
#define XACME_IMPLEMENTATION
#include "../../include/xacme/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xacme.h"
#include "../acme/test_store_oom.c"
