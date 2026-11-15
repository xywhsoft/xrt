#define XMQTT_WS_SINGLE_TEST
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#ifndef XMQTT_MODULE_XMQTT_CLIENT_WS
#define XMQTT_MODULE_XMQTT_CLIENT_WS
#endif
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_client_ws.c"
