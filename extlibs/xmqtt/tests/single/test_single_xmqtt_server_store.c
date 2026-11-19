#define XMQTT_MODULE_XMQTT_SERVER_STORE
#include <xmqtt/features.h>
#if defined(XRT_FEATURE_FILE) || defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "the broker store must select only its required protocol closure"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_server_store.c"
