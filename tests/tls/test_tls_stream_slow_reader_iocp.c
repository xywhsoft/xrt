/* 使用同一组慢读方契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP slow reader"
#define TEST_TLS_STREAM_SLOW_READER
#include "test_tls_stream.c"
