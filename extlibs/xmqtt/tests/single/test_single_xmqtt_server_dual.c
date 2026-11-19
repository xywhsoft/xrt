#define XMQTT_MODULE_XMQTT_SERVER
#define XMQTT_MODULE_XMQTT_CODEC311
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "adding compatibility must leave the client unselected"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_server.c"
