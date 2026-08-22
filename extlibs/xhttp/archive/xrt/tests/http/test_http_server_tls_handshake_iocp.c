/* 使用同一组 HTTPS 握手失败契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "IOCP"
#include "test_http_server_tls_handshake.c"
