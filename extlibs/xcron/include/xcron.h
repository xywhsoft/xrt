#ifndef XCRON_H
#define XCRON_H

#include <xcron/features.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_EXPRESSION)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_EXPRESSION"
#endif

#include <xcron/expression.h>
#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_CALENDAR)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_CALENDAR"
#endif
#include <xcron/calendar.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_TIMEZONE)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_TIMEZONE"
#endif
#include <xcron/timezone.h>

#if defined(XCRON_FEATURE_XCRON) && (!defined(XCRON_FEATURE_XCRON_TZDB) || !defined(XCRON_FEATURE_XCRON_TZDB_FILE))
#error "XCRON_FEATURE_XCRON requires tzdb and tzdb_file"
#endif
#include <xcron/tzdb.h>
#include <xcron/tzdb_file.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_ZONED"
#endif
#include <xcron/zoned.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_DELIVERY)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_DELIVERY"
#endif
#include <xcron/delivery.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_SCHEDULER)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_SCHEDULER"
#endif
#include <xcron/scheduler.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_EXECUTOR)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_EXECUTOR"
#endif
#include <xcron/executor.h>

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_SERVICE)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_SERVICE"
#endif
#include <xcron/service.h>

#endif
