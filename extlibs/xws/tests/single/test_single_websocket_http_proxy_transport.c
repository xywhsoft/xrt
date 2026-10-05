#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



#if !defined(TEST_SINGLE_WS_HTTP_PROXY_BACKEND)
	#define TEST_SINGLE_WS_HTTP_PROXY_BACKEND XNET_PORT_SELECT
	#define TEST_SINGLE_WS_HTTP_PROXY_BACKEND_NAME "select"
#endif

#define TEST_WS_HTTP_PROXY_BACKEND TEST_SINGLE_WS_HTTP_PROXY_BACKEND
#define TEST_WS_HTTP_PROXY_BACKEND_NAME TEST_SINGLE_WS_HTTP_PROXY_BACKEND_NAME
#include "../websocket/test_http_proxy.c"
