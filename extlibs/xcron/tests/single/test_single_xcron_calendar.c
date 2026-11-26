#define XCRON_MODULE_XCRON_CALENDAR
#define XRT_MODULE_MEMORY_STATS
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#if defined(XCRON_FEATURE_XCRON_SCHEDULER) || defined(XRT_FEATURE_EVENT_WAITER)
#error "xcron calendar must build without the scheduler and XRT event waiter"
#endif
#include "../test_calendar.c"
