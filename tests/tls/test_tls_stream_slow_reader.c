/* 延迟消费明文，验证组合层暂停 TCP 并在消费后恢复驱动。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#define TEST_TLS_STREAM_BACKEND_NAME "select slow reader"
#define TEST_TLS_STREAM_SLOW_READER
#include "test_tls_stream.c"
