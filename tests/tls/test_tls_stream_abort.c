/* 验证失败终态尚未关闭传输时，Abort 立即收敛并保留首个根因。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#define TEST_TLS_STREAM_BACKEND_NAME "select failed-state abort"
#define TEST_TLS_STREAM_ABORT_FAILED
#include "test_tls_stream.c"
