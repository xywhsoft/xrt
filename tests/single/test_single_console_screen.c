#define XRT_MODULE_CONSOLE_SCREEN
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#if defined(XRT_FEATURE_IO) || defined(XRT_FEATURE_THREAD) || defined(XRT_FEATURE_BUFFER) || defined(XRT_FEATURE_CONSOLE_TERMINAL)
#error "screen queries must not pull stream or session dependencies"
#endif
#include "../console/test_console_screen.c"
