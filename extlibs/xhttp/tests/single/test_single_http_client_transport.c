#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



#if !defined(TEST_SINGLE_HTTP_CLIENT_BACKEND)
	#define TEST_SINGLE_HTTP_CLIENT_BACKEND XNET_PORT_SELECT
	#define TEST_SINGLE_HTTP_CLIENT_BACKEND_NAME "single select"
#endif

#define TEST_HTTP_CLIENT_BACKEND \
	TEST_SINGLE_HTTP_CLIENT_BACKEND
#define TEST_HTTP_CLIENT_BACKEND_NAME \
	TEST_SINGLE_HTTP_CLIENT_BACKEND_NAME
#include "../http/test_http_client.c"
