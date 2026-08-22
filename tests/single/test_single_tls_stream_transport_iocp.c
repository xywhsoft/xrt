/* 复用完整单头 TLS-over-TCP 契约验证 Windows IOCP。 */
#define TEST_SINGLE_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_TLS_STREAM_BACKEND_NAME "single IOCP"
#include "test_single_tls_stream_transport.c"
