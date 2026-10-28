#define XMQTT_MODULE_XMQTT_CODEC311
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES) || defined(XMQTT_FEATURE_XMQTT_CODEC5)
#error "3.1.1 standalone must not select properties or codec5"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test_codec311.c"
