#ifndef XWS_GROUP_EXPORT_H
#define XWS_GROUP_EXPORT_H

#include <xrt/core.h>

/* 独立动态库只导出本库接口，依赖由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XWS_GROUP_BUILD_SHARED)
  #define XWS_GROUP_API __declspec(dllexport)
 #elif defined(XWS_GROUP_USE_SHARED)
  #define XWS_GROUP_API __declspec(dllimport)
 #else
  #define XWS_GROUP_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XWS_GROUP_BUILD_SHARED)
  #define XWS_GROUP_API __attribute__((visibility("default")))
 #else
  #define XWS_GROUP_API
 #endif
#else
 #define XWS_GROUP_API
#endif

#endif
