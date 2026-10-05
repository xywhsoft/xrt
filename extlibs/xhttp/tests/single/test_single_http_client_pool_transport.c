#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_POOL
	#define XHTTP_MODULE_HTTP_CLIENT_POOL
#endif
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



#if !defined(TEST_SINGLE_HTTP_POOL_BACKEND)
	#define TEST_SINGLE_HTTP_POOL_BACKEND XNET_PORT_SELECT
	#define TEST_SINGLE_HTTP_POOL_BACKEND_NAME "single select"
#endif

#define TEST_HTTP_POOL_BACKEND \
	TEST_SINGLE_HTTP_POOL_BACKEND
#define TEST_HTTP_POOL_BACKEND_NAME \
	TEST_SINGLE_HTTP_POOL_BACKEND_NAME
#include "../http/test_http_client_pool.c"

