#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




#if !defined(TEST_SINGLE_HTTP_CLIENT_BACKEND)
	#define TEST_SINGLE_HTTP_CLIENT_BACKEND XNET_PORT_SELECT
	#define TEST_SINGLE_HTTP_CLIENT_BACKEND_NAME "single select"
#endif

#define TEST_HTTP_CLIENT_BACKEND \
	TEST_SINGLE_HTTP_CLIENT_BACKEND
#define TEST_HTTP_CLIENT_BACKEND_NAME \
	TEST_SINGLE_HTTP_CLIENT_BACKEND_NAME
#include "../http/test_http_client.c"
