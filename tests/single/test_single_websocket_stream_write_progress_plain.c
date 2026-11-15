#define XRT_MODULE_WEBSOCKET_STREAM_WRITE_PROGRESS
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#if defined(XRT_FEATURE_TLS_STREAM) || defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
#error "plain prefix observation must not force TLS"
#endif
#include "../../examples/websocket/write_progress/main.c"
