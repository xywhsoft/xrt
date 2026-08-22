/* 复用完整单头 HTTPS Server 契约验证 Windows IOCP。 */
#define TEST_SINGLE_HTTP_SERVER_TLS_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_HTTP_SERVER_TLS_BACKEND_NAME "single IOCP"
#include "test_single_http_server_tls_transport.c"
