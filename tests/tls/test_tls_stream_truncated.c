/* 强制底层 TCP 绕过 close_notify，验证截断根因不会被关闭覆盖。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#define TEST_TLS_STREAM_BACKEND_NAME "select truncation"
#define TEST_TLS_STREAM_TRUNCATED
#include "test_tls_stream.c"
