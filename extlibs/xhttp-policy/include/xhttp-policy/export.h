#ifndef XHTTP_POLICY_EXPORT_H
#define XHTTP_POLICY_EXPORT_H

#include <xrt/core.h>

/* 独立动态库仅导出本库实现，依赖库由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XHTTP_POLICY_BUILD_SHARED)
  #define XHTTP_POLICY_API __declspec(dllexport)
 #elif defined(XHTTP_POLICY_USE_SHARED)
  #define XHTTP_POLICY_API __declspec(dllimport)
 #else
  #define XHTTP_POLICY_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XHTTP_POLICY_BUILD_SHARED)
  #define XHTTP_POLICY_API __attribute__((visibility("default")))
 #else
  #define XHTTP_POLICY_API
 #endif
#else
 #define XHTTP_POLICY_API
#endif

#endif
