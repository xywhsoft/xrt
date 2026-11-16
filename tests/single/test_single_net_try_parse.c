#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#define XRT_MODULE_NET
#define XRT_MODULE_MEMORY_STATS
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include "../network/test_net_try_parse.c"
