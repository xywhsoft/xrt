/* 使用同一组双层背压契约验证 Windows IOCP 后端。 */
#define TEST_TLS_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_TLS_STREAM_BACKEND_NAME "IOCP backpressure"
#define TEST_TLS_STREAM_BACKPRESSURE
#include "test_tls_stream.c"
