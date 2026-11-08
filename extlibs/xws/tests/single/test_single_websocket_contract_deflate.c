#include "../../../xws/include/xws/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XWS_IMPLEMENTATION
#include "../../../../single/extlibs/xws.h"

/* Exercise the unaligned negotiated-Deflate output on a live connection. */
#include "../websocket/test_connection_future_deflate.c"
