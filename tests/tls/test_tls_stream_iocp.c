/* 使用同一组组合契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP"
#include "test_tls_stream.c"
