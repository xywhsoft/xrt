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

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_ZONED"
#endif
#include <xcron/zoned.h>

#endif
