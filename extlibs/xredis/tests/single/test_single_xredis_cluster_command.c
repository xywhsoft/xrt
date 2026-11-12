#define XREDIS_CLUSTER_COMMAND_WHITEBOX
#define XREDIS_MODULE_XREDIS_CLUSTER_COMMAND
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static _Thread_local bool qpClockArmed;
static _Thread_local unsigned qpClockCalls;
static _Thread_local double qpClockBase;
static double qpClock(void)
{if(qpClockArmed)return qpClockBase+(qpClockCalls++?.001:0);return xrtTimer();}
#define xrtTimer qpClock
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#undef xrtTimer
#include "../test_cluster_command.c"
