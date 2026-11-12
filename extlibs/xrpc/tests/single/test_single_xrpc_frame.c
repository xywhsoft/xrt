#define XRPC_MODULE_XRPC_FRAME
#define XRT_MODULE_MEMORY_STATS
#include <xrpc/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XRPC_IMPLEMENTATION
#include "../../../../single/extlibs/xrpc.h"
#if defined(XRT_FEATURE_JSON_READ) || defined(XRT_FEATURE_NET_ENGINE) || defined(XRT_FEATURE_FUTURE)
#error "Framing alone must not pull JSON, networking or Futures"
#endif
#include "../test_frame.c"
