/* 使用同一组截断关闭契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP truncation"
#define TEST_TLS_STREAM_TRUNCATED
#include "test_tls_stream.c"
