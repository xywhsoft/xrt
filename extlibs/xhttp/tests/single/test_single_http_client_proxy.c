#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_PROXY
	#define XHTTP_MODULE_HTTP_CLIENT_PROXY
#endif
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../http/test_http_client_proxy_config.c"


