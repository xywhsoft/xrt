/* 强制 TLS 与 TCP 两级有界队列短写，验证 Writable 和 Drain 恢复。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#define TEST_TLS_STREAM_BACKEND_NAME "select backpressure"
#define TEST_TLS_STREAM_BACKPRESSURE
#include "test_tls_stream.c"
