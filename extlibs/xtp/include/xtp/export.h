#ifndef XTP_EXPORT_H
#define XTP_EXPORT_H
#include <xrt/core.h>
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XTP_BUILD_SHARED)
  #define XTP_API __declspec(dllexport)
 #elif defined(XTP_USE_SHARED)
  #define XTP_API __declspec(dllimport)
 #else
  #define XTP_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XTP_BUILD_SHARED)
  #define XTP_API __attribute__((visibility("default")))
 #else
  #define XTP_API
 #endif
#else
 #define XTP_API
#endif
#endif
