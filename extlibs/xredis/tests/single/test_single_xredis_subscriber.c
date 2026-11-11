#define XREDIS_FEATURE_XREDIS_SUBSCRIBER
#define XRT_FEATURE_MEMORY_STATS
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#include "../test_subscriber.c"
