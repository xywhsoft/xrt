#ifndef XCRON_H
#define XCRON_H

#include <xcron/features.h>

#if defined(XCRON_FEATURE_XCRON) && \
    (!defined(XCRON_FEATURE_XCRON_EXPRESSION) || !defined(XCRON_FEATURE_XCRON_CALENDAR))
#error "xcron requires expression and calendar"
#endif

#include <xcron/expression.h>
#include <xcron/calendar.h>
#include <xcron/scheduler.h>

#endif
