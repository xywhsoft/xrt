#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_SELECT
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "single select"
#include "../http/test_http_client_stream_cancel_race.c"
