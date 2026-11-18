#define XRPC_MODULE_XRPC_STDIO_PROCESS
#include <xrpc/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XRPC_IMPLEMENTATION
#include "../../../../single/extlibs/xrpc.h"
#if !defined(XRT_FEATURE_PROCESS_PIPE) || defined(XRT_FEATURE_NET_ENGINE) || defined(XRT_FEATURE_TLS)
#error "stdio process requires native pipes and must stay independent of net and TLS"
#endif
#include "../test_stdio_process.c"
