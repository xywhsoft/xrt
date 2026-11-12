#define XREDIS_CLUSTER_SYNC_WHITEBOX
#define XREDIS_MODULE_XREDIS_CLUSTER_COMMAND_SYNC
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static _Thread_local bool swClockArmed,swFailWait;
static _Thread_local unsigned swClockCalls;
static _Thread_local double swClockBase;
static double swClock(void)
{if(swClockArmed)return swClockBase+(swClockCalls++?.001:0);return xrtTimer();}
static xwaitresult swWait(xfuture* F)
{if(swFailWait)return XWAIT_ERROR;return xrtFutureWait(F);}
#define xrtTimer swClock
#define xrtFutureWait swWait
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#undef xrtFutureWait
#undef xrtTimer
#include "../test_cluster_sync.c"
