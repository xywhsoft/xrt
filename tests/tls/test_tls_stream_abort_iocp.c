/* 使用同一组失败后 Abort 契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP failed-state abort"
#define TEST_TLS_STREAM_ABORT_FAILED
#include "test_tls_stream.c"
