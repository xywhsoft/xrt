#define XREDIS_REFRESH_WHITEBOX
#define XREDIS_MODULE_XREDIS_CLUSTER_REFRESH
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
/* Only this whitebox binary injects a caller-thread clock jump after parsing.
 * Native workers and all unarmed calls continue to use the real clock. */
static _Thread_local unsigned frClockTicks;
static _Thread_local double frClockAfter;
static double frClock(void)
{double Now=xrtTimer();if(frClockTicks && --frClockTicks==0)return frClockAfter;return Now;}
#define xrtTimer frClock
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#undef xrtTimer
#include "../test_cluster_refresh.c"
