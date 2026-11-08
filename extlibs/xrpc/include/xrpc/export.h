#ifndef XRPC_EXPORT_H
#define XRPC_EXPORT_H
#include <xrt/core.h>
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XRPC_BUILD_SHARED)
  #define XRPC_API __declspec(dllexport)
 #elif defined(XRPC_USE_SHARED)
  #define XRPC_API __declspec(dllimport)
 #else
  #define XRPC_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XRPC_BUILD_SHARED)
  #define XRPC_API __attribute__((visibility("default")))
 #else
  #define XRPC_API
 #endif
#else
 #define XRPC_API
#endif
#endif
