#ifndef XRUDP_EXPORT_H
#define XRUDP_EXPORT_H

#include <xrt/core.h>

#if defined(_WIN32) || defined(_WIN64)
#if defined(XRUDP_BUILD_SHARED)
#define XRUDP_API __declspec(dllexport)
#elif defined(XRUDP_USE_SHARED)
#define XRUDP_API __declspec(dllimport)
#else
#define XRUDP_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(XRUDP_BUILD_SHARED)
#define XRUDP_API __attribute__((visibility("default")))
#else
#define XRUDP_API
#endif
#else
#define XRUDP_API
#endif

#endif
