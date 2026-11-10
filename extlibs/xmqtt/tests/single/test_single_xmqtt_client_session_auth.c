#define XMQTT_MODULE_XMQTT_CLIENT_SESSION
#define XMQTT_MODULE_XMQTT_CLIENT_AUTH
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) || defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS) || defined(XRT_FEATURE_NET_RESOLVER)
#error "Numeric Session Auth must not acquire Dial, Resolver or TLS"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_client_session_dial.c"
