/* Real file/Future lifecycle with an explicitly selected native file backend. */
#define XRT_MODULE_NET_FILE_FUTURE
#define XRT_MODULE_MEMORY_DEBUG
#if defined(_WIN32)
#define XRT_MODULE_NET_PORT_IOCP
#else
#define XRT_MODULE_NET_PORT_URING
#endif
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include "../network/test_net_file_future.c"
