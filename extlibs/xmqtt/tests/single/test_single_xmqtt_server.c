#define XMQTT_MODULE_XMQTT_SERVER
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_CODEC311) || defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "the standalone broker must only select its own closure"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_server.c"
