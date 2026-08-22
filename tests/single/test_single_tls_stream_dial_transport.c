#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_TLS_DIAL_BACKEND)
	#define TEST_SINGLE_TLS_DIAL_BACKEND XNET_PORT_SELECT
#endif

#define TEST_TLS_DIAL_BACKEND TEST_SINGLE_TLS_DIAL_BACKEND
#include "../tls/test_tls_stream_dial.c"
