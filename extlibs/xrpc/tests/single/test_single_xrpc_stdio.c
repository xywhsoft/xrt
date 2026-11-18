#define XRPC_MODULE_XRPC_STDIO
#include <xrpc/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XRPC_IMPLEMENTATION
#include "../../../../single/extlibs/xrpc.h"
#if defined(XRT_FEATURE_NET_ENGINE) || defined(XRT_FEATURE_PROCESS)
#error "Host-driven stdio must not pull a network engine or process launcher"
#endif
#include "../test_stdio.c"
