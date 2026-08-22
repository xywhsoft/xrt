#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#define TEST_HTTP_CLIENT_RACE_BACKEND XNET_PORT_SELECT
#define TEST_HTTP_CLIENT_RACE_BACKEND_NAME "single select"
#include "../http/test_http_client_cancel_race.c"
