/* 使用同一组 TLS Stream Future 契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_FUTURE_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_FUTURE_BACKEND_NAME "IOCP"
#include "test_tls_stream_future.c"
