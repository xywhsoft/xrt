#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_TLS_DIAL_FUTURE_BACKEND)
	#define TEST_SINGLE_TLS_DIAL_FUTURE_BACKEND XNET_PORT_SELECT
#endif

#define TEST_TLS_DIAL_FUTURE_BACKEND TEST_SINGLE_TLS_DIAL_FUTURE_BACKEND
#include "../tls/test_tls_stream_dial_future_edges.c"
