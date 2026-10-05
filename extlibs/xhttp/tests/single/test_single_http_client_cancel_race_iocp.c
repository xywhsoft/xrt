#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



#define TEST_HTTP_CLIENT_RACE_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_RACE_BACKEND_NAME "single IOCP"
#include "../http/test_http_client_cancel_race.c"
