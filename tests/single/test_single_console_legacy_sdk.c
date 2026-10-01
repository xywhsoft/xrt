/* Emulate older Win32 SDKs even when the compiler ships modern headers. */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#undef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#undef ENABLE_QUICK_EDIT_MODE
#undef ENABLE_EXTENDED_FLAGS
#endif

#define XRT_MODULE_CONSOLE_TERMINAL
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#if defined(_WIN32) || defined(_WIN64)
_Static_assert(ENABLE_VIRTUAL_TERMINAL_PROCESSING == 0x0004, "Win32 VT flag");
_Static_assert(ENABLE_QUICK_EDIT_MODE == 0x0040, "Win32 quick-edit flag");
_Static_assert(ENABLE_EXTENDED_FLAGS == 0x0080, "Win32 extended flag");
#endif

/* Query, styled output and failed terminal admission all use the real code. */
#include "../console/test_console_terminal.c"
